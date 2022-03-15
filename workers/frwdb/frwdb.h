
//******************************************************
// Copyright (C) 2018, Coollyer <48707400@qq.com>
//*******************************************************

// struct diffrent compress
#ifndef _FRWDB_H
#define _FRWDB_H

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

#include "sis_net.node.h"

#include "worker.h"

// *** 快速读写sisdb的插件 ***
// 有且只能有1个人写入数据 读数据的人可以多个 

#define FRWDB_STATUS_NONE    0
#define FRWDB_STATUS_WRING   1   // 当前写入进行中 单日1天数据写入
#define FRWDB_STATUS_STOPW   2   // 当前写入停止 单日数据写入完毕

#pragma pack(push,1)

typedef struct s_frwdb_reader
{
	int                 cid;
	s_sis_sds           cname;
	// 以下两个回调互斥
	void               *father;       // 来源对象 s_frwdb_cxt
        
	int                 rfmt;         // 返回数据的格式
	int                 status;       // 0 未初始化 1 初始化 2 正常工作 3 停止工作

	bool                iszip;        // 是否返回压缩格式
	bool                ishead;       // 是否从头发送
	bool                isinit;       // 0 刚刚订阅 1 正常订阅 
	bool                isstop;       // 用户中断 什么也不能干
 
	bool                sub_whole;    // 是否返回所有的数据
	///////以下为定制数据处理///////
	// 这里先处理实时的 历史的以后再说
	bool                sub_disk;     // 是否从磁盘中获取数据
	s_sis_worker       *sub_disker;   // 从磁盘读取数据的对象  sisdb_rsno

	int                 sub_date;     // 订阅日期 如果为历史就启动一个线程专门处理 历史默认为从头发送
	int                 start_date;   // 开始订阅日期
	int                 stop_date;    // 结束订阅日期
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

} s_frwdb_reader;

typedef struct s_frwdb_unit {
	int      kidx;    // 
	int      sidx;    // 
	int      size;    // 数据尺寸
	char     data[0]; // 数据区
} s_frwdb_unit;

// 来源数据只管写盘 只有key, sdb有的才会保存数据
// 读取数据的人会接收到压缩的 out_bitzips 如果接收者订阅了全部就直接发送数据出去
//          否则就把数据解压，然后写入自己的 s_frwdb_cxt 再通过读者回调发送数据

typedef struct s_frwdb_cxt
{
	int                 stoping;     // 是否正在结束
	int                 status;      // 0 1 2 3
	s_sis_sds_save     *work_path;   // 数据库路径 sisdb
	s_sis_sds_save     *work_name;   // 数据库名字 sisdb

	int                 save_time;   // 自动存盘时间 0 不存盘 40000 早上4点存盘
	int                 work_date;   // 工作日期
	int                 work_year;   // 当前处理的年 = 0 不处理合并年上 > 0 年切换时先执行上一年数据合并 再继续

    s_sis_net_mems     *work_nodes;   // 数据节点
    s_sis_wait_thread  *read_thread;  // 读数据的线程

	s_sis_map_list     *map_keys;    // key 的结构字典表 s_sis_sds
	s_sis_map_list     *map_sdbs;    // sdb 的结构字典表 s_sis_dynamic_db 包括
	s_sis_map_pointer  *map_data;    // 实际数据列表
						
	// s_sisdb_incr       *work_ziper;

	int                 wlog_mark;    // 按 start 时日期
	int                 wlog_opened;  // 是否打开了文件 
	s_sis_worker       *wlog_worker;  // 当前使用的写log类
	s_sis_method       *wlog_method;

	s_sis_json_node    *wfile_config; // 写文件配置
	s_sis_json_node    *rfile_config; // 读文件配置

	// s_sis_fast_queue   *inputs;    // 传入的数据链 

	int                 curr_readers;     // 当日数据的工作人数
	s_sis_map_kint     *map_reader_disk;  // 读者列表 s_frwdb_reader 读取磁盘数据
	s_sis_map_kint     *map_reader_curr;  // 读者列表 s_frwdb_reader 读取内存数据 当日且工作进行中可用

	// 直接回调组装好的 s_sis_net_message
	void               *cb_source;       // 
	sis_method_define  *cb_net_message;  // s_sis_net_message 

} s_frwdb_cxt;


#pragma pack(pop)


bool  frwdb_init(void *, void *);
void  frwdb_uninit(void *);
void  frwdb_working(void *);

// ***  数据流操作 ***//
// 数据采用结构化数据压缩方式，实时压缩，放入无锁发送队列中 
// 每10ms打一个包或者数据量超过16K(减少拆包和拼包的动作)一个包,
// 为相关压缩必须保证包的顺序 当时间为整10分钟时重新压缩 保证最差能取到最近 10 分钟数据
// snodb 仅仅作为一个数据组件使用 和网络无关
// *****************//
// 初始化 需要传入 workdate,keys,sdbs 
int cmd_frwdb_init(void *worker_, void *argv_);

int cmd_frwdb_create(void *worker_, void *argv_);

int cmd_frwdb_setdb(void *worker_, void *argv_);
// 数据流开始写入 
int cmd_frwdb_start(void *worker_, void *argv_);
// 数据流写入完成 设置标记 方便发送订阅完成信号
int cmd_frwdb_stop(void *worker_, void *argv_);

// 把缓存的数据写入磁盘 
int cmd_frwdb_save(void *worker_, void *argv_);

int cmd_frwdb_merge(void *worker_, void *argv_);

int cmd_frwdb_set(void *worker_, void *argv_);
// 如果设置了 start 状态 表示按天写入数据 保存队列 直到 stop 再写盘 
// 如果是直接写 表示按键值写入数据 直到 save 再一起写盘
int cmd_frwdb_bset(void *worker_, void *argv_);
// 需要传入 kidx sidx val 收到数据后压缩存储
int cmd_frwdb_ipub(void *worker_, void *argv_);  // s_frwdb_bytes
// 需要传入 key sdb val 收到数据后压缩存储
int cmd_frwdb_pub(void *worker_, void *argv_);  // s_sis_db_chars
// 需要传入 zipval 直接是压缩数据块 直接放入队列中
int cmd_frwdb_zpub(void *worker_, void *argv_);  // s_sis_db_incrzip
// 对指定的无锁队列增加订阅者
// 默认从最新的具备完备数据的数据包开始订阅 seat : 0
// seat : 1 从最开始订阅 
// 订阅后首先收到 订阅start 然后收到 key sdb 然后开始持续收到压缩的数据 最后收到 stop 
// int cmd_frwdb_zsub(void *worker_, void *argv_);
// 订阅其他格式数据
int cmd_frwdb_sub(void *worker_, void *argv_);
// 取消订阅
int cmd_frwdb_unsub(void *worker_, void *argv_);
// 获取数据
int cmd_frwdb_get(void *worker_, void *argv_);
// 删除数据
int cmd_frwdb_del(void *worker_, void *argv_);
// 
int cmd_frwdb_wlog(void *worker_, void *argv_);
int cmd_frwdb_rlog(void *worker_, void *argv_);

//////////////////////////////////////////////////////////////////
//------------------------snodb function -----------------------//
//////////////////////////////////////////////////////////////////
// 初始化一个读者 并启动
int frwdb_register_reader(s_frwdb_cxt *context_, s_sis_net_message *netmsg);
// 清理指定的 reader
int frwdb_remove_reader(s_frwdb_cxt *context_, int cid_);
// 直接读取单键值数据
int frwdb_read(s_frwdb_cxt *context_, s_sis_net_message *netmsg);

//////////////////////////////////////////////////////////////////
//------------------------s_frwdb_reader -----------------------//
//////////////////////////////////////////////////////////////////

s_frwdb_reader *frwdb_reader_create();
void frwdb_reader_destroy(void *);

int frwdb_reader_new_realtime(s_frwdb_reader *reader_);
int frwdb_reader_realtime_start(s_frwdb_reader *reader);
int frwdb_reader_realtime_stop(s_frwdb_reader *reader);

void frwdb_reader_curr_init(s_frwdb_cxt *context);
void frwdb_reader_curr_start(s_frwdb_cxt *context);
void frwdb_reader_curr_write(s_frwdb_cxt *context, s_sis_db_chars *chars);
void frwdb_reader_curr_stop(s_frwdb_cxt *context);


int frwdb_reader_new_history(s_frwdb_reader *reader_);
int frwdb_reader_history_start(s_frwdb_reader *reader);
int frwdb_reader_history_stop(s_frwdb_reader *reader);


//////////////////////////////////////////////////////////////////
//------------------------snodb disk function -----------------------//
//////////////////////////////////////////////////////////////////
// 从磁盘中获取数据 0 没有wlog文件 ** 文件有错误直接删除 并返回0
//               1 有文件 正在读 需要设置状态 并等待数据读完
#define SICDB_FILE_SIGN_ZPUB  0  // zpub ...
#define SICDB_FILE_SIGN_KEYS  1  // zset _sdbs_ ...
#define SICDB_FILE_SIGN_SDBS  2  // zset _keys_ ...

int frwdb_wlog_load(s_sis_worker *);
// 停止wlog文件
int frwdb_wlog_start(s_frwdb_cxt *);
// 把数据写入到wlog中
int frwdb_wlog_save(s_frwdb_cxt *, int , char *imem_, size_t isize_);
// 停止wlog文件
int frwdb_wlog_stop(s_frwdb_cxt *);
// 清除wlog文件
int frwdb_wlog_remove(s_frwdb_cxt *);
// 把wlog转为snos格式 
int frwdb_wlog_to_snos(s_frwdb_cxt *);


int frwdb_wfile_nodes_to_memory(s_sis_node_list *nodes, s_sis_memory *imem);

void frwdb_write_start(s_frwdb_cxt *context, int workdate);

void frwdb_write_stop(s_frwdb_cxt *context);

#endif /* _SISDB_SNO_H */
