#ifndef _SISDB_H
#define _SISDB_H

#include "sis_method.h"
#include "sis_net.node.h"
#include "sis_queue.h"
#include "sis_dynamic.h"

// 四个时间尺度的数据表 同名字段会传递只能由大到小
// 'T'  TICK数据 8 有就追加数据
// 'S'  秒数据   4-8
// 'M'  分钟数据 4 
// 'D'  日  数据 4 

// 根据SDB的时间定义来确定是什么周期
// # 一个是KEY 一个是属性 FIELD
// # KEY 有自己的属性 保留小数点 dot 默认为 2 除非特别设置
// # FIELD 为一个map表 name 数据类型（I:整数,F:浮点数,P:转换价格,J:JSON,C:定长字符串,S:不定长字符串,M:结构体)
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

// 每条命令的三种权限 具备写权限的才保留
#define SISDB_ACCESS_READ       0
#define SISDB_ACCESS_WRITE      1
#define SISDB_ACCESS_PUBLISH    2   // 发布信息权限
#define SISDB_ACCESS_ADMIN      7   // 超级用户 全部权限

#define SISDB_ACCESS_SREAD       "read"
#define SISDB_ACCESS_SWRITE      "write"
#define SISDB_ACCESS_SADMIN      "admin"

/////////////////////////////////////////////////////////
//  数据格是定义 
/////////////////////////////////////////////////////////
// 下面以字节流方式传递
// #define SISDB_FORMAT_SDB     0x0001   // 后面开始为数据 keylen+key + sdblen+sdb + ssize + 二进制数据
// #define SISDB_FORMAT_SDBS    0x0002   // 后面开始为数据 count + keylen+key + sdblen+sdb + ssize + 二进制数据
// #define SISDB_FORMAT_KEY     0x0004   // 后面开始为数据 keylen+key + ssize + 二进制数据
// #define SISDB_FORMAT_KEYS    0x0008   // 后面开始为数据 count + keylen+key + ssize + 二进制数据
#define SISDB_FORMAT_STRUCT  0x0001   // 结构数据  

#define SISDB_FORMAT_BYTES   0x00FF   // 二进制数据流  
// 下面必须都是可见字符
#define SISDB_FORMAT_STRING  0x0100   // 直接传数据 json文档 {k1:{},k2:{}}
#define SISDB_FORMAT_JSON    0x0200   // 直接传数据 json文档 {k1:{},k2:{}}
#define SISDB_FORMAT_ARRAY   0x0400   // 数组格式 {k1:[]}
#define SISDB_FORMAT_CSV     0x0800   // csv格式，需要处理字符串的信息
#define SISDB_FORMAT_CHARS   (SISDB_FORMAT_STRING|SISDB_FORMAT_JSON|SISDB_FORMAT_ARRAY|SISDB_FORMAT_CSV) 

#define SISDB_MAX_GET_SIZE   0xFFFF  // 最大数据块月64K 超过就启动线程 异步去发送

// // 记录我订阅了哪些数据 收到信息后一一对应后调用回调
// s_sis_map_pointer    *map_subs;  // 以 key 为索引的 s_sis_net_sub 结构的 s_sis_pointer_list *
// 								 // 保存订阅列表 方便断线后从新发送订阅信息
// // 多个client订阅的列表 需要一一对应发送
// s_sis_map_pointer    *map_pubs;  // 以key为索引的 s_sis_net_pub 结构的 s_sis_pointer_list *
// 								 // 同一key可能有多个用户订阅

typedef struct s_sisdb_kv
{
	uint16             format;    // bytes chars
	s_sis_sds          value;     // 需要保存的信息
} s_sisdb_kv;	

// 数据量过大需开启线程处理
typedef struct s_sisdb_sub_info
{
	s_sis_struct_list     cids;      // 多个客户ID
	s_sis_object         *obj;       // 准备发送的数据 s_sis_net_message 由多个客户共享
} s_sisdb_sub_info;	

#define s_sisdb_field s_sis_dynamic_field

typedef struct s_sisdb_table
{
	s_sis_dynamic_db     *db;       // 对应的数据表
	s_sis_string_list    *fields;   // 默认的字段序列
} s_sisdb_table;	

typedef struct s_sisdb_cxt
{
	int                 status;      // 工作状态

	s_sis_sds           name;        // 数据库名字 sisdb

	s_sis_map_pointer  *kvs;         // 单键值的映射表 的字典表 s_sisdb_kv 记录 单键的信息
	// 下面数据永不清理
	s_sis_map_list     *keys;        // key 的结构字典表 s_sis_sds
	s_sis_map_list     *sdbs;        // sdb 的结构字典表 s_sisdb_table 
	// 下面数据可能定时清理
	s_sis_map_pointer  *collects;    // 数据集合的字典表 s_sisdb_collect 这里实际存放数据，数量为股票个数x数据表数
									 // SH600600.DAY 
	// 用户订阅后登记到 sub_dict 中, 先把内存中数据发布到 pub_list 由其他线程 pub_reader 处理数据发送给客户
	// 当key数据更新后 如果key被订阅 就写入 pub_list 中,也由 pub_reader 处理数据
	s_sis_share_list   *pub_list;    // 发布的信息汇总到这个列表 回调后再 根据 s_sisdb_sub_info 一一发送信息
	s_sis_share_reader *pub_reader;  // 发布信息的读者
	// 多个 client 订阅的列表 需要一一对应发送
	s_sis_mutex_t       sub_lock;    // 更新订阅字典 sub_dict 的互斥
	s_sis_map_pointer  *sub_dict;    // 以 key 为索引的 s_sisdb_sub_info 同一key可能有多个用户订阅


}s_sisdb_cxt;

s_sisdb_kv *sisdb_kv_create(uint16 format_, s_sis_sds in_);
void sisdb_kv_destroy(void *info_);

bool  sisdb_init(void *, void *);
void  sisdb_uninit(void *);
void  sisdb_method_init(void *);
void  sisdb_method_uninit(void *);

int cmd_sisdb_get(void *worker_, void *argv_);
int cmd_sisdb_set(void *worker_, void *argv_);
int cmd_sisdb_del(void *worker_, void *argv_);
int cmd_sisdb_drop(void *worker_, void *argv_);

int cmd_sisdb_gets(void *worker_, void *argv_);
int cmd_sisdb_bset(void *worker_, void *argv_);
int cmd_sisdb_dels(void *worker_, void *argv_);

int cmd_sisdb_sub(void *worker_, void *argv_);
int cmd_sisdb_unsub(void *worker_, void *argv_);
int cmd_sisdb_subs(void *worker_, void *argv_);

#endif