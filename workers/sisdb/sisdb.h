#ifndef _SISDB_H
#define _SISDB_H

#include "sis_method.h"
#include "sis_net.msg.h"
#include "sis_list.lock.h"
#include "sis_dynamic.h"
#include "sis_utils.h"
#include "sis_db.h"
#include "sis_obj.h"
#include "worker.h"
#include "sisdb_sub.h"
#include "sisdb_fmap.h"

// 所有查询全部先返回二进制数据 最后再转换格式返回
// 启动时只加载LOG 通过LOG的写入指令 加载磁盘的相应数据 
// 所有 key 值都会描述其数据类型 对应数据区 以及提取时间 有效时间 方便超过时间后自动清理
// 无论任何时候只要从磁盘中获取了数据 除非碰到 save 指令才会写盘 

// 放内存的疑问	???
// 删除指令似乎只能直接删除索引 和对数据块做删除标记 无法在内存中标注
// 假设写盘时只要数据时间重叠就覆盖老的索引 那么删除时可以考虑带空的数据区 写盘时如果发现是空的数据区就代表删除
// 数据区必须和磁盘一一对应对应 每个时间段一个数据块 数据块可以为空 为空代表删除 调用删除函数处理磁盘数据 磁盘如果没有数据表示已经删除 直接返回
// 获取数据时 不支持偏移 必须定位时间 根据对应时间获取数据块 放入内存后修改 
// 再次获取数据 只加载多出的时间数据 无论磁盘是否有数据 加载过一次的数据都需要保留一个块 块的数据可以为空 表示已经加载 但没有数据
// ---- 如果分块 检索逻辑复杂 
// ---- 不如增加一个删除列表 记录需要删除什么文件的什么数据 只有当该文件的数据都清理了才会增加一条记录 
// ---- 解决方案 需要建立一个内存和磁盘的映射表 应该和文件格式无关 
// sisdb 搭建一个 disk_catch 就可以把数据在内存中进行交换 每天save成功后清理一次内存 读取频次最低的全部从内存中清理掉
//          不实时清理内存 是因为如果内存数据清理后 磁盘和LOG数据并未同步而引起的数据不一致
// save 时直接再创建一个 disk_catch 和 当前时刻的 log 合并后得到新的数据 
//      再把新数据落盘 最后再加锁 然后交换新旧文件
//      save的同时 系统可以写新的 log 文件


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
// #define SIS_ACCESS_PUBLISH    2   // 发布信息权限
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

typedef struct s_sisdb_cxt
{
	int                 status;        // 工作状态
  
	s_sis_sds_save     *work_path;     // 数据库路径 sisdb
	s_sis_sds_save     *work_name;     // 数据库名字 sisdb
	s_sis_sds           safe_path;     // 安全路径 sisdb
  
	int                 work_date;     // 当前工作日期
	int                 save_time;     // 存盘时间

    s_sis_mutex_t		wlog_lock;     // 写log要加锁
	int                 wlog_open;     // wlog是否可写
	s_sis_method       *wlog_write;    // log的写入方法
	s_sis_worker       *wlog_worker;   // 当前使用的flog类
	
	s_sisdb_fmap_cxt   *work_fmap_cxt; // 管理所有的数据

	// 多个 client 订阅的列表 需要一一对应发送
	s_sisdb_sub_cxt    *work_sub_cxt;  // 实时信息发布管理

	// s_sis_map_list     *work_psubs;    // s_sisdb_sub_unit 订阅磁盘历史数据
	int                 save_status;   // 0 可以存盘 1 正在存盘 2 存盘成功 -1 存盘失败
	// 直接回调组装好的 s_sis_net_message open 时设置
	void               *cb_source;       // 
	sis_method_define  *cb_net_message;  // s_sis_net_message 

}s_sisdb_cxt;

bool  sisdb_init(void *, void *);
void  sisdb_uninit(void *);
void  sisdb_method_init(void *);
void  sisdb_method_uninit(void *);
void  sisdb_working(void *);

int cmd_sisdb_show(void *worker_, void *argv_);
// create 一个表
int cmd_sisdb_create(void *worker_, void *argv_);
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
// 订阅表的最新数据 历史数据用 psub 获取
int cmd_sisdb_hsub(void *worker_, void *argv_);
// 取消新数据订阅
int cmd_sisdb_unsub(void *worker_, void *argv_);
// 订阅历史数据
// 回放数据 需指定日期 支持模糊匹配 所有数据全部拿到按时间排序后一条一条返回 
// 会先发送start 然后是各种数据 s_sis_db_chars * 最后是stop 每个请求获取数据样本后 开一个线程处理并返回数据
// 同一个连接只能同时有一个回放请求 启动一个线程 新建一个订阅类
// 这里暂时不考虑内存和磁盘数据的拼接问题 
// ** 如果考虑也可以 先从内存看数据是否已经加载 如果加载，直接放数据到订阅类 **
// 用该功能 读取的数据并不常驻内存 数据用完就释放
int cmd_sisdb_psub(void *worker_, void *argv_);
// 取消回放
int cmd_sisdb_unpsub(void *worker_, void *argv_);
// 直接从磁盘获取数据
int cmd_sisdb_read(void *worker_, void *argv_);// 从磁盘加载数据
// 存盘后会清理所有日下的内存数据 日上的数据仅仅保留最近1年 以保证内存空间可用
int cmd_sisdb_save (void *worker_, void *argv_);// 存盘
int cmd_sisdb_pack (void *worker_, void *argv_);// 合并整理数据
int cmd_sisdb_init(void *worker_, void *argv_);
int cmd_sisdb_open(void *worker_, void *argv_);
int cmd_sisdb_close(void *worker_, void *argv_);
int cmd_sisdb_rlog (void *worker_, void *argv_);// 加载没有写盘的log信息
int cmd_sisdb_wlog (void *worker_, void *argv_);// 写入没有写盘的log信息
int cmd_sisdb_clear(void *worker_, void *argv_);// 停止某个客户的所有查询

//////////////////////////////////////////////////
//
/////////////////////////////////////////////////
int sisdb_disk_pack(s_sisdb_cxt *context);

s_sis_object *sisdb_disk_read(s_sisdb_cxt *context, s_sis_net_message *netmsg);

// 从磁盘中加载log
int sisdb_rlog_read(s_sis_worker *worker);

void sisdb_wlog_open(s_sisdb_cxt *context);
void sisdb_wlog_remove(s_sisdb_cxt *context);
void sisdb_wlog_close(s_sisdb_cxt *context);

// 开始存盘操作 锁定内存数据 移动相关文件 改名log 主程序可以继续接受新的写入信息 但不直接修改内存
void sisdb_disk_save_start(s_sisdb_cxt *context);
// 开始执行存盘操作 如果失败就恢复原状 利用老的文件和log生成新的文件到safe目录
int sisdb_disk_save(s_sisdb_cxt *context);
// 成功后 交换safe文件 删除处理完的log文件 重置内存write标记=0 然后加载新的log信息
void sisdb_disk_save_stop(s_sisdb_cxt *context);

#endif