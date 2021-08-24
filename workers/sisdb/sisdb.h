#ifndef _SISDB_H
#define _SISDB_H

#include "sis_method.h"
#include "sis_net.msg.h"
#include "sis_list.lock.h"
#include "sis_dynamic.h"
#include "sis_db.h"
#include "worker.h"
#include "sisdb_sub.h"

// 所有查询全部先返回二进制数据 最后再转换格式返回
// 启动时只加载LOG 通过LOG的写入指令 加载磁盘的相应数据 
// 所有 key 值都会描述其数据类型 对应数据区 以及提取时间 有效时间 方便超过时间后自动清理
// 
// 四个时间尺度的数据表 同名字段会传递只能由大到小
// 'N'  纳秒数据 8 不覆盖 有就追加数据
// 'W'  微秒数据 8 不覆盖 有就追加数据
// 'T'  毫秒数据 8 不覆盖 有就追加数据
// 'S'  秒数据 4 同值最新覆盖
// 'M'  分钟数据 4 同值最新覆盖
// 'D'  日数据 4 同值最新覆盖

// set SH600600 {"name":"aaaa","dot":2,"market":"SH"} 数据直接写入SH600600键值中
// set SH600600.stk_snapshot {"open":1200,"newp":1300,"volume":10000} 写入
// getkey SH* 获取所有SH开头的 code
// sub SH600600,SH600601.stk_snapshot,stk_trancat 订阅两只股票的两类数据
// sub *.* 订阅所有股票的所有数据
// 只有sub和getkey有模糊匹配功能
// update SH600600.money {search:{sno :3, name:sss}, val:{...}} 大括号表示并且的关系
// update SH600600.money {search:[{sno :3}, {name:sss}], val:{...}} 方括号表示或的关系
// del SH600600.money {search:{sno :[1,3]}} 大括号表示并且的关系 取sno为1..3的数据 (1,3) 不包含 1,3
// del SH600600.money {search:[{sno :[1,3]}, {sno:[5,8]}]} 方括号表示或的关系 取1..3 和 5..8的数据
// 取边界外的数据不支持
// 最多的方法是取一段时间的数据

// 四个时间尺度的数据表 同名字段会传递只能由大到小
// 'T'  TICK数据 8 有就追加数据
// 'S'  秒数据   4-8
// 'M'  分钟数据 4 
// 'D'  日  数据 4 

// 每条命令的三种权限 具备写权限的才保留
// #define SIS_ACCESS_READ       0
// #define SIS_ACCESS_WRITE      1
#define SIS_ACCESS_PUBLISH    2   // 发布信息权限
// #define SIS_ACCESS_ADMIN      7   // 超级用户 全部权限

// #define SIS_ACCESS_SREAD       "read"   // read
// #define SIS_ACCESS_SWRITE      "write"  // set sets
// #define SIS_ACCESS_SADMIN      "admin"  // del pack save 

#define SISDB_MAX_GET_SIZE   0xFFFF  // 最大数据块月64K 超过就启动线程 异步去发送

// // 记录我订阅了哪些数据 收到信息后一一对应后调用回调
// s_sis_map_pointer    *map_subs;  // 以 key 为索引的 s_sis_net_sub 结构的 s_sis_pointer_list *
// 								 // 保存订阅列表 方便断线后从新发送订阅信息
// // 多个client订阅的列表 需要一一对应发送
// s_sis_map_pointer    *map_pubs;  // 以key为索引的 s_sis_net_pub 结构的 s_sis_pointer_list *
// 								 // 同一key可能有多个用户订阅

#define SISDB_SUB_ONE_MUL       0   // 订阅指定的多个单键值
#define SISDB_SUB_ONE_ALL       1   // 订阅所有的单键

// #define SISDB_SUB_TABLE_ONE       4   // 订阅指定的一个sdb键值 订阅sno时有用
#define SISDB_SUB_TABLE_MUL       5   // 订阅指定的多个sdb键值
#define SISDB_SUB_TABLE_KEY       6   // 订阅匹配的key的所有结构数据
#define SISDB_SUB_TABLE_SDB       7   // 订阅匹配的sdb的所有结构数据
#define SISDB_SUB_TABLE_ALL       8   // 订阅所有的结构数据

typedef struct s_sisdb_catch
{
	int  last_day;  // > 0 表示加载最后几年的数据 0 表示全部加载
	int  last_min;  // > 0 表示加载最后几天的分钟线 0 表示不加载历史数据
	int  last_sec;  // > 0 表示加载最后几天的秒线 0 表示不加载历史数据
	int  last_sno;  // > 0 表示加载最后几天数据 0 表示只保留当日 不从历史数据加载 只从log加载 
} s_sisdb_catch;

typedef struct s_sisdb_reader
{
	// int                   cid;
	// s_sis_sds             serial;
	// void                 *sisdb_worker;          // 来源对象

	uint8                 sub_fmt;    // 订阅数据格式
	uint8                 sub_type;   // 订阅类型
	s_sis_sds             sub_keys;   // SISDB_SUB_ONE_MUL SISDB_SUB_TABLE_MUL SISDB_SUB_TABLE_KEY 时有值
	s_sis_sds             sub_sdbs;   // SISDB_SUB_TABLE_SDB 时有值
	s_sis_pointer_list   *netmsgs;    // s_sis_net_message
} s_sisdb_reader;	
	
#define s_sisdb_table s_sis_dynamic_db
#define s_sisdb_field s_sis_dynamic_field
#define sisdb_table_create sis_dynamic_db_create
#define sisdb_table_destroy sis_dynamic_db_destroy


typedef struct s_sisdb_cxt
{
	int                 status;        // 工作状态
  
	s_sis_sds           work_path;        // 数据库名字 sisdb
	s_sis_sds           work_name;        // 数据库名字 sisdb
  
	int                 work_date;     // 当前工作日期
	int                 save_time;     // 存盘时间

	int                 wlog_load;     // 是否正在加载 wlog       
	int                 wlog_open;     // wlog是否可写
    s_sis_mutex_t		wlog_lock;     // 写log要加锁
	s_sis_worker       *wlog_worker;   // 当前使用的写log类

	s_sis_method       *wlog_method;

	int                 wfile_save;
	sis_method_define  *wfile_cb_sisdb_bytes;
	s_sis_worker       *wfile_worker; // 当前使用的写文件类

	s_sis_worker       *rfile_worker; // 当前使用的读文件类
	
	// 下面数据永不清理
	s_sis_map_list     *work_sdbs;    // sdb 的结构字典表 s_sis_dynamic_db
	// 下面数据可能定时清理
	s_sis_map_pointer  *work_keys;    // 数据集合的字典表 s_sisdb_collect 这里实际存放数据，数量为股票个数x数据表数
									  // SH600600.DAY 

	// 多个 client 订阅的列表 需要一一对应发送
	s_sisdb_sub_cxt    *work_sub_cxt;  // 信息发布管理

	// 直接回调组装好的 s_sis_net_message
	void               *cb_source;       // 
	sis_method_define  *cb_net_message;  // s_sis_net_message 

}s_sisdb_cxt;

s_sisdb_reader *sisdb_reader_create(s_sis_net_message *netmsg_);
void sisdb_reader_destroy(void *);

bool  sisdb_init(void *, void *);
void  sisdb_uninit(void *);
void  sisdb_method_init(void *);
void  sisdb_method_uninit(void *);
void  sisdb_working(void *);

int cmd_sisdb_show(void *worker_, void *argv_);
int cmd_sisdb_open(void *worker_, void *argv_);
// 主要的获取数据接口
// 不带参数表示从内存中快速获取完整数据
// 带 snodate 参数 表示内存找不到就去磁盘找对应数据
// 带 search 参数 表示获取指定时间段的数据
// 带 fields 参数 表示返回数据按指定字段排列
int cmd_sisdb_get(void *worker_, void *argv_);
// 以json数据写入数据
int cmd_sisdb_set(void *worker_, void *argv_);

// 删除某个key的数据
int cmd_sisdb_del(void *worker_, void *argv_);
// 如果数据为空就清除key
int cmd_sisdb_drop(void *worker_, void *argv_);

// 得到多股票和多表的最后一条数据 仅仅读取内存中的数据
int cmd_sisdb_gets(void *worker_, void *argv_);
// 得到所有键值
int cmd_sisdb_keys(void *worker_, void *argv_);
// 二进制数据写入
int cmd_sisdb_bset(void *worker_, void *argv_);
// 删除多个数据
int cmd_sisdb_dels(void *worker_, void *argv_);
// 订阅表的最新数据 历史数据用 psub 获取
int cmd_sisdb_sub(void *worker_, void *argv_);
// 取消新数据订阅
int cmd_sisdb_unsub(void *worker_, void *argv_);
// 订阅历史数据
// 回放数据 需指定日期 支持模糊匹配 所有数据全部拿到按时间排序后一条一条返回 
// 会先发送start 然后是各种数据 s_sis_db_chars * 最后是stop 每个请求获取数据样本后 开一个线程处理并返回数据
// 同一个连接只能同时有一个回放请求
// 这里暂时不考虑内存和磁盘数据的拼接问题
int cmd_sisdb_psub(void *worker_, void *argv_);
// 取消回放
int cmd_sisdb_unpsub(void *worker_, void *argv_);

int cmd_sisdb_disk_save (void *worker_, void *argv_);// 存盘
int cmd_sisdb_disk_pack (void *worker_, void *argv_);// 合并整理数据
int cmd_sisdb_rdisk(void *worker_, void *argv_);// 从磁盘加载数据
int cmd_sisdb_rlog (void *worker_, void *argv_);// 加载没有写盘的log信息
int cmd_sisdb_wlog (void *worker_, void *argv_);// 写入没有写盘的log信息
int cmd_sisdb_clear(void *worker_, void *argv_);// 停止某个客户的所有查询

int cmd_sisdb_getdb(void *worker_, void *argv_);

int sisdb_disk_save(s_sisdb_cxt *context);
int sisdb_disk_pack(s_sisdb_cxt *context);

#endif