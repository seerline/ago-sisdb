
//******************************************************
// Copyright (C) 2018, Coollyer <48707400@qq.com>
//*******************************************************

#ifndef _ZIPDB_H
#define _ZIPDB_H

#include "sis_core.h"
#include "sis_math.h"
#include "sis_malloc.h"

#include "sis_list.lock.h"
#include "sis_json.h"
#include "sis_dynamic.h"
#include "sis_bits.h"
#include "sis_obj.h"
#include "sis_net.io.h"

#include "worker.h"

#define ZIPMEM_MAXSIZE (16*1024)

#define ZIPDB_CLEAR_MODE_ALL    0
#define ZIPDB_CLEAR_MODE_DATA   1
#define ZIPDB_CLEAR_MODE_READER 2

#pragma pack(push,1)

// 单个数据结构
typedef struct s_zipdb_chars
{
	char    keyn[32];
	char    sdbn[64];
	uint16  size;
	void   *data;     
} s_zipdb_chars;

typedef struct s_zipdb_bytes
{
	uint32  kidx;
	uint16  sidx;
	uint16  size;
	void   *data;    
} s_zipdb_bytes;

// 压缩数据结构
typedef struct s_zipdb_bits
{
	uint8   init;
	uint32  size;
	uint8   data[0];    
} s_zipdb_bits;

// 仅仅解压 有数据就回调
// 对待sdb多记录压缩 需要一次性把单类数据压缩进去
// 对待sno多记录压缩 需要修改sdbs的结构传入worker中
typedef struct s_zipdb_worker
{
	void                 *cb_source;   // 返回对象 unzipdb专用
	cb_sis_struct_decode *cb_read;     // 按结构返回数据 unzipdb专用

	s_sis_object         *zip_obj;     // zipdb 专用
	int      	          initsize;    // 超过多大数据重新初始化 字节
	int      	          zip_size;    // 单个数据块的大小
	int      	          cur_size;    // 缓存数据当前的尺寸

	s_sis_bits_stream    *cur_sbits;   // 当前指向缓存的位操作类
	s_sis_map_list       *keys;        // key 的结构字典表 s_sis_sds
	s_sis_map_list       *sdbs;        // sdb 的结构字典表 s_sis_dynamic_db 包括
} s_zipdb_worker;

typedef struct s_zipdb_reader
{
	int        cid;
	s_sis_sds  serial;
	// 以下两个回调互斥
	void      *zipdb_worker;          // 来源对象

	bool       ishead;                // 是否从头发送
	bool       isinit;                // 0 刚刚订阅 1 正常订阅 
 
	bool                sub_whole;     // 是否返回所有的数据
	///////以下为定制数据处理///////
	// 这里先处理实时的 历史的以后再说
	bool                sub_disk;     // 是否从磁盘中获取数据
	int                 sub_date;     // 订阅日期 如果为历史就启动一个线程专门处理 历史默认为从头发送
	s_sis_sds           sub_keys;     // 订阅股票
	s_sis_sds           sub_sdbs;     // 订阅数据
	// 当读者需要特定的数据 先对收到的数据进行解压 再根据指定的代码和表压缩 然后再传递出去
	// 为简便处理 即便传入的数据为原始数据 也从压缩数据解压后获取
	//  传入的数据如果是压缩的就解压，然后过滤后压缩 如果是原始数据就直接过滤后压缩
	s_zipdb_worker     *sub_ziper;     // 用于压缩的操作类
	s_zipdb_worker     *sub_unziper;   // 用于解压的操作类
	///////以上为定制数据处理///////

	s_sis_lock_reader  *reader;       // 每个读者一个订阅者
	// 返回压缩的数据 
	sis_method_define  *cb_zipbits;       // s_zipdb_bits

    sis_method_define  *cb_sub_start;    // char *
    sis_method_define  *cb_sub_realtime; // char *
    sis_method_define  *cb_sub_stop;     // char *

	sis_method_define  *cb_dict_keys;   // json char *
	sis_method_define  *cb_dict_sdbs;   // char *

} s_zipdb_reader;

// 来源数据只管写盘 只有key, sdb有的才会保存数据
// 读取数据的人会接收到压缩的 out_bitzips 如果接收者订阅了全部就直接发送数据出去
//          否则就把数据解压，然后写入自己的 s_zipdb_cxt 再通过读者回调发送数据

#define MAP_ZIPDB_BITS(v) ((s_zipdb_bits *)sis_memory(v->ptr))

typedef struct s_zipdb_cxt
{
	int      work_status;  // 当前工作状态

	int      wait_msec;   // 超过多长时间生成新的块 毫秒
	int      initsize;    // 超过多大数据重新初始化 字节
	int      zip_size;    // 单个数据块的大小
	int      cur_size;    // 当前累计字节数 字节

	uint64   catch_size;  // 缓存数据保留最大的尺寸

	bool     inited;    // 是否已经初始化
	bool     stoped;    // 是否已经结束
	int      work_date; // 工作日期

	int                 wlog_load;  // 是否正在加载 wlog       
	int                 wlog_date;  // 是否正在加载 wlog    
	int                 wlog_init;  // 是否发送了 keys 和 sdbs  
	s_sis_sds           wlog_keys; 
	s_sis_sds           wlog_sdbs; 
	s_sis_worker       *wlog_worker;        // 当前使用的写log类

	s_sis_method       *wlog_method;
	s_zipdb_reader     *wlog_reader;         // 写wlog的读者

	int                 wfile_save;
	sis_method_define  *wfile_cb_sub_start;
	sis_method_define  *wfile_cb_sub_stop ;
	sis_method_define  *wfile_cb_dict_keys;
	sis_method_define  *wfile_cb_dict_sdbs;
	sis_method_define  *wfile_cb_zip_bytes;
	s_sis_worker       *wfile_worker; // 当前使用的写文件类

	s_sis_worker       *rfile_worker;       // 当前使用的读文件类

	s_sis_sds           work_keys; // 工作 keys
	s_sis_sds           work_sdbs; // 工作 sdbs
	s_sis_map_list     *keys;     // key 的结构字典表 s_sis_sds
	s_sis_map_list     *sdbs;     // sdb 的结构字典表 s_sis_dynamic_db 包括

	s_sis_fast_queue   *inputs;    // 传入的数据链 s_zipdb_bits

	s_sis_bits_stream  *cur_sbits;   // 当前指向缓存的位操作类

	s_sis_object       *cur_object;   // s_zipdb_bits -> 映射为memory 当前用于写数据的缓存 
	s_sis_object       *last_object;  // 最近一个其实数据包的指针
	// 这个outputs需要设置为无限容量
	int                 zipnums;    // 统计数量
	s_sis_lock_list    *outputs;    // 输出的数据链 s_zipdb_bits -> 映射为 memory 每10分钟一个新的压缩数据块
	s_sis_pointer_list *readeres;   // 读者列表 s_zipdb_reader

} s_zipdb_cxt;

#pragma pack(pop)


bool  zipdb_init(void *, void *);
void  zipdb_uninit(void *);
void  zipdb_method_init(void *);
void  zipdb_method_uninit(void *);

// ***  数据流操作 ***//
// 数据采用结构化数据压缩方式，实时压缩，放入无锁发送队列中 
// 每10ms打一个包或者数据量超过16K(减少拆包和拼包的动作)一个包,
// 为相关压缩必须保证包的顺序 当时间为整10分钟时重新压缩 保证最差能取到最近 10 分钟数据
// zipdb 仅仅作为一个数据组件使用 和网络无关
// *****************//
// 初始化 需要传入 workdate,keys,sdbs 
int cmd_zipdb_init(void *worker_, void *argv_);
// 数据流开始写入 
int cmd_zipdb_start(void *worker_, void *argv_);
// 数据流写入完成 设置标记 方便发送订阅完成信号
int cmd_zipdb_stop(void *worker_, void *argv_);
// 需要传入 kidx sidx val 收到数据后压缩存储
int cmd_zipdb_ipub(void *worker_, void *argv_);  // s_zipdb_bytes
// 需要传入 key sdb val 收到数据后压缩存储
int cmd_zipdb_spub(void *worker_, void *argv_);  // s_zipdb_chars
// 需要传入 zipval 直接是压缩数据块 直接放入队列中
int cmd_zipdb_zpub(void *worker_, void *argv_);  // s_zipdb_bits
// 对指定的无锁队列增加订阅者
// 默认从最新的具备完备数据的数据包开始订阅 seat : 0
// seat : 1 从最开始订阅 
// 订阅后首先收到 订阅start 然后收到 key sdb 然后开始持续收到压缩的数据 最后收到 stop 
int cmd_zipdb_sub(void *worker_, void *argv_);
// 取消订阅
int cmd_zipdb_unsub(void *worker_, void *argv_);
// 默认仅仅做数据清理  data 清理数据 client 客户端订阅清理 all 全部清理(默认)
int cmd_zipdb_clear(void *worker_, void *argv_);

int cmd_zipdb_wlog(void *worker_, void *argv_);
int cmd_zipdb_rlog(void *worker_, void *argv_);
int cmd_zipdb_wsno(void *worker_, void *argv_);
int cmd_zipdb_rsno(void *worker_, void *argv_);

//////////////////////////////////////////////////////////////////
//------------------------s_zipdb_worker -----------------------//
//////////////////////////////////////////////////////////////////
s_zipdb_worker *zipdb_worker_create();
void zipdb_worker_destroy(s_zipdb_worker *);

void zipdb_worker_clear(s_zipdb_worker *);

void zipdb_worker_set_keys(s_zipdb_worker *, s_sis_sds );
void zipdb_worker_set_sdbs(s_zipdb_worker *, s_sis_sds );
// 写入需要解压的数据
void zipdb_worker_unzip_init(s_zipdb_worker *, void *cb_source, cb_sis_struct_decode *cb_read_);
void zipdb_worker_unzip_set(s_zipdb_worker *, s_zipdb_bits *);
// 写入需要压缩的数据
void zipdb_worker_zip_init(s_zipdb_worker *, int, int);
void zipdb_worker_zip_flush(s_zipdb_worker *, int);
void zipdb_worker_zip_set(s_zipdb_worker *, int ,int, char *, size_t);

//////////////////////////////////////////////////////////////////
//------------------------s_zipdb_reader -----------------------//
//////////////////////////////////////////////////////////////////
s_zipdb_reader *zipdb_reader_create();
void zipdb_reader_destroy(void *);

int zipdb_reader_realtime_start(s_zipdb_reader *reader);
int zipdb_reader_realtime_stop(s_zipdb_reader *reader);

int zipdb_reader_new_realtime(s_zipdb_reader *reader_);
// 清理指定的 reader
int zipdb_move_reader(s_zipdb_cxt *, int);

//////////////////////////////////////////////////////////////////
//------------------------zipdb function -----------------------//
//////////////////////////////////////////////////////////////////
// 从磁盘中获取数据 0 没有wlog文件 ** 文件有错误直接删除 并返回0
//               1 有文件 正在读 需要设置状态 并等待数据读完
#define ZIPDB_FILE_SIGN_ZPUB  0  // zpub ...
#define ZIPDB_FILE_SIGN_KEYS  1  // zset _sdbs_ ...
#define ZIPDB_FILE_SIGN_SDBS  2  // zset _keys_ ...

int zipdb_wlog_load(s_zipdb_cxt *);
// 把数据写入到wlog中
int zipdb_wlog_save(s_zipdb_cxt *, int , s_zipdb_bits *);
// 清除wlog文件
int zipdb_wlog_move(s_zipdb_cxt *);
// 把wlog转为snos格式 
int zipdb_wlog_save_snos(s_zipdb_cxt *);
// 读取 snos 文件 snos 为zipdb压缩的分块式顺序格式
int zipdb_snos_read(s_zipdb_cxt *);

#endif /* _SISDB_SNO_H */
