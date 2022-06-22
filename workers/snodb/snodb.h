
//******************************************************
// Copyright (C) 2018, Coollyer <48707400@qq.com>
//*******************************************************

// struct diffrent compress
#ifndef _SNODB_H
#define _SNODB_H

#include "sis_core.h"
#include "sis_math.h"
#include "sis_malloc.h"

#include "sis_list.lock.h"
#include "sis_json.h"
#include "sis_dynamic.h"
#include "sis_obj.h"
#include "sis_disk.h"
#include "sis_net.msg.h"
#include "sis_db.h"
#include "sisdb_sub.h"
#include "sisdb_incr.h"

#include "worker.h"

// *** 到这里不适合做顶级 顶级只传输最新的数据 ***
// 如果订阅是当天日期 如果是全量就直接传递最新数据 从指定位置 启动一个读者线程
//                 如果不是全量就 启动一个读者线程 不断解压再压缩 然后传输
// 不是当天日期 或当天已经处理了收盘 那么就直接从磁盘读取数据 启动一个线程 调用rsno收到回调后传输数据  
// 收到最新数据 实时写入 net 文件中,等待收盘后转为 sno 文件

#define SNODB_STATUS_NONE   0
#define SNODB_STATUS_INIT   1
#define SNODB_STATUS_WORK   2
#define SNODB_STATUS_EXIT   3

#pragma pack(push,1)

typedef struct s_snodb_reader
{
	int                 cid;
	s_sis_sds           serial;
	// 以下两个回调互斥
	void               *father;       // 来源对象 s_snodb_cxt
        
	int                 rfmt;         // 返回数据的格式
	int                 status;       // 0 未初始化 1 初始化 2 正常工作 3 停止工作

	bool                iszip;        // 是否返回压缩格式
	bool                ishead;       // 是否从头发送
	bool                isfields;     // 是否返回字段名 仅仅在JSON格式返回
	bool                isinit;       // 0 刚刚订阅 1 正常订阅 
	bool                isstop;       // 用户中断 什么也不能干
 
	bool                sub_whole;    // 是否返回所有的数据
	///////以下为定制数据处理///////
	// 这里先处理实时的 历史的以后再说
	bool                sub_disk;     // 是否从磁盘中获取数据
	s_sis_worker       *sub_disker;   // 从从盘读取数据的对象  sisdb_rsno

	int                 sub_date;     // 订阅日期 如果为历史就启动一个线程专门处理 历史默认为从头发送
	s_sis_sds           sub_keys;     // 订阅股票 可能为 * 
	s_sis_sds           sub_sdbs;     // 订阅数据 可能为 *
	// 当读者需要特定的数据 先对收到的数据进行解压 再根据指定的代码和表压缩 然后再传递出去
	// 为简便处理 即便传入的数据为原始数据 也从压缩数据解压后获取
	// 传入的数据如果是压缩的就解压，然后过滤后压缩 如果是原始数据就直接过滤后压缩
	s_sisdb_incr        *sub_ziper;   // 用于压缩的操作类
	s_sisdb_incr        *sub_unziper; // 用于解压的操作类
	///////以上为定制数据处理///////

	s_sis_lock_reader  *reader;        // 每个读者一个订阅者
	// 返回数据 s_sis_db_chars
	sis_method_define  *cb_sub_chars;
	// 返回压缩的数据 
	sis_method_define  *cb_sub_incrzip;  // s_sis_db_incrzip

    sis_method_define  *cb_sub_start;    // char *
    sis_method_define  *cb_sub_realtime; // char *
    sis_method_define  *cb_sub_stop;     // char *

	sis_method_define  *cb_dict_keys;    // char *
	sis_method_define  *cb_dict_sdbs;    // char *

} s_snodb_reader;

// 来源数据只管写盘 只有key, sdb有的才会保存数据
// 读取数据的人会接收到压缩的 out_bitzips 如果接收者订阅了全部就直接发送数据出去
//          否则就把数据解压，然后写入自己的 s_snodb_cxt 再通过读者回调发送数据

typedef struct s_snodb_cxt
{
	int                 stoping;     // 当前工作状态
   
	int                 status;      // 0 1 2 3
	int                 work_date;   // 工作日期
	s_sis_sds			work_path;
	s_sis_sds			work_name;

	s_sisdb_incr       *work_ziper;

	s_sis_sds           work_keys;   // 工作 keys
	s_sis_sds           work_sdbs;   // 工作 sdbs
	s_sis_map_list     *map_keys;    // key 的结构字典表 s_sis_sds
	s_sis_map_list     *map_sdbs;    // sdb 的结构字典表 s_sis_dynamic_db 包括

						
	int                 wait_msec;   // 超过多长时间生成新的块 毫秒           
	uint64              catch_size;  // 缓存数据保留最大的尺寸
           

	int                 wlog_date;    // wlog    
	int                 wlog_init;    // 是否发送了 keys 和 sdbs  
	s_sis_worker       *wlog_worker;  // 当前使用的写log类
	s_sis_method       *wlog_method;
	s_snodb_reader     *wlog_reader;  // 写wlog的读者

	int                 save_time;    // 自动存盘时间
	int                 wfile_save;
	sis_method_define  *wfile_cb_sub_start;
	sis_method_define  *wfile_cb_sub_stop ;
	sis_method_define  *wfile_cb_dict_keys;
	sis_method_define  *wfile_cb_dict_sdbs;
	sis_method_define  *wfile_cb_sub_incrzip;
	s_sis_worker       *wfile_worker; // 当前使用的写文件类

	s_sis_json_node    *rfile_config;

	s_sis_fast_queue   *inputs;    // 传入的数据链 


	s_sis_object       *near_object;     // 最近一个其实数据包的指针
	// 这个outputs需要设置为无限容量
	s_sis_lock_list    *outputs;          // 输出的数据链 memory 每10分钟一个新的压缩数据块
	s_sis_map_kint     *ago_reader_map;  // 读者列表 s_snodb_reader

	int                 cur_readers;     // 现有工作人数
	s_sis_map_kint     *cur_reader_map;  // 读者列表 s_snodb_reader

	// 直接回调组装好的 s_sis_net_message
	void               *cb_source;       // 
	sis_method_define  *cb_net_message;  // s_sis_net_message 

} s_snodb_cxt;


#pragma pack(pop)


bool  snodb_init(void *, void *);
void  snodb_uninit(void *);
void  snodb_working(void *);

// ***  数据流操作 ***//
// 数据采用结构化数据压缩方式，实时压缩，放入无锁发送队列中 
// 每10ms打一个包或者数据量超过16K(减少拆包和拼包的动作)一个包,
// 为相关压缩必须保证包的顺序 当时间为整10分钟时重新压缩 保证最差能取到最近 10 分钟数据
// snodb 仅仅作为一个数据组件使用 和网络无关
// *****************//
// 初始化 需要传入 workdate,keys,sdbs 
int cmd_snodb_init(void *worker_, void *argv_);
int cmd_snodb_set(void *worker_, void *argv_);
// 数据流开始写入 
int cmd_snodb_start(void *worker_, void *argv_);
// 数据流写入完成 设置标记 方便发送订阅完成信号
int cmd_snodb_stop(void *worker_, void *argv_);
// 需要传入 kidx sidx val 收到数据后压缩存储
int cmd_snodb_ipub(void *worker_, void *argv_);  // s_snodb_bytes
// 需要传入 key sdb val 收到数据后压缩存储
int cmd_snodb_pub(void *worker_, void *argv_);  // s_sis_db_chars
// 需要传入 zipval 直接是压缩数据块 直接放入队列中
int cmd_snodb_zpub(void *worker_, void *argv_);  // s_sis_db_incrzip
// 对指定的无锁队列增加订阅者
// 默认从最新的具备完备数据的数据包开始订阅 seat : 0
// seat : 1 从最开始订阅 
// 订阅后首先收到 订阅start 然后收到 key sdb 然后开始持续收到压缩的数据 最后收到 stop 
// int cmd_snodb_zsub(void *worker_, void *argv_);
// 订阅其他格式数据
int cmd_snodb_sub(void *worker_, void *argv_);
// 取消订阅
int cmd_snodb_unsub(void *worker_, void *argv_);
// 取消订阅
int cmd_snodb_get(void *worker_, void *argv_);

// 默认仅仅做数据清理  data 清理数据 client 客户端订阅清理 all 全部清理(默认)
int cmd_snodb_clear(void *worker_, void *argv_);

int cmd_snodb_wlog(void *worker_, void *argv_);
int cmd_snodb_rlog(void *worker_, void *argv_);

//////////////////////////////////////////////////////////////////
//------------------------snodb function -----------------------//
//////////////////////////////////////////////////////////////////
// 初始化一个读者 并启动
int snodb_register_reader(s_snodb_cxt *context_, s_sis_net_message *netmsg);
// 清理指定的 reader
int snodb_remove_reader(s_snodb_cxt *context_, int cid_);
// 直接读取单键值数据
int snodb_read(s_snodb_cxt *context_, s_sis_net_message *netmsg);


//////////////////////////////////////////////////////////////////
//------------------------s_snodb_reader -----------------------//
//////////////////////////////////////////////////////////////////

s_snodb_reader *snodb_reader_create();
void snodb_reader_destroy(void *);

int snodb_reader_new_realtime(s_snodb_reader *reader_);
int snodb_reader_realtime_start(s_snodb_reader *reader);
int snodb_reader_realtime_stop(s_snodb_reader *reader);

int snodb_reader_new_history(s_snodb_reader *reader_);
int snodb_reader_history_start(s_snodb_reader *reader);
int snodb_reader_history_stop(s_snodb_reader *reader);


//////////////////////////////////////////////////////////////////
//------------------------snodb disk function -----------------------//
//////////////////////////////////////////////////////////////////
// 从磁盘中获取数据 0 没有wlog文件 ** 文件有错误直接删除 并返回0
//               1 有文件 正在读 需要设置状态 并等待数据读完
#define SICDB_FILE_SIGN_ZPUB  0  // zpub ...
#define SICDB_FILE_SIGN_KEYS  1  // zset _sdbs_ ...
#define SICDB_FILE_SIGN_SDBS  2  // zset _keys_ ...

int snodb_wlog_load(s_snodb_cxt *);
// 停止wlog文件
int snodb_wlog_start(s_snodb_cxt *);
// 把数据写入到wlog中
int snodb_wlog_save(s_snodb_cxt *, int , char *imem_, size_t isize_);
// 停止wlog文件
int snodb_wlog_stop(s_snodb_cxt *);
// 清除wlog文件
int snodb_wlog_remove(s_snodb_cxt *);
// 把wlog转为snos格式 
int snodb_wlog_to_snos(s_snodb_cxt *);

#endif /* _SISDB_SNO_H */
