#ifndef _SISDB_SERVER_H
#define _SISDB_SERVER_H

#include "sis_method.h"
#include "sis_list.lock.h"
#include "sis_net.h"
#include "sis_net.msg.h"
#include "sis_message.h"
#include "worker.h"
#include "sis_dynamic.h"

#define SISDB_STATUS_NONE  0
#define SISDB_STATUS_INIT  1
#define SISDB_STATUS_WORK  2
#define SISDB_STATUS_EXIT  3

// 启动时只加载LOG 通过LOG的写入指令 加载磁盘的相应数据 
// 所有key值都会描述其数据类型 对应数据区 以及提取时间 有效时间 方便超过时间后自动清理
// 
// snodb 根据缓存大小保存压缩数据在内存 收到stop后把数据保存到磁盘中 历史数据从磁盘获取 


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

// 保留内存数据的限制
typedef struct s_sisdb_catch
{
	int  last_day;  // > 0 表示加载最后几年的数据 0 表示全部加载
	int  last_min;  // > 0 表示加载最后几天的分钟线 0 表示不加载历史数据
	int  last_sec;  // > 0 表示加载最后几天的秒线 0 表示不加载历史数据
	int  last_sno;  // > 0 表示加载最后几天数据 0 表示只保留当日 不从历史数据加载 只从log加载 
} s_sisdb_catch;

typedef struct s_sisdb_userinfo
{
	char username[32];
	char password[32];
	int8 access;       // 权限 
} s_sisdb_userinfo;

typedef struct s_sisdb_convert
{
	char                   dateset[128];
	s_sis_dynamic_convert *convert;
} s_sisdb_convert;

typedef struct s_sisdb_server_cxt
{
	int    status;

	int    level;    // 等级 默认0为最高级 只能 0 --> 1,2,3... 发数据 ||  0 <--> 0 数据是互相备份

	uint32              work_date;    // 序列存储的起始日

	s_sis_lock_list    *recv_list;   // 所有收到的数据放队列中 供多线程分享任务
	s_sis_lock_reader  *reader_recv; // 读取发送队列

	// save 时 不再接收数据写入 但仍然可以读取数据

	// 重启加载时从fast_save加载限定数据，然后从wlog中获取最新数据 作为内存数据 

	// 查询和订阅请求 都发送到 fast_save 由这个方法来处理
	// 快速写盘不做 reader 写盘时从内存中获取数据直接写盘 

	s_sis_lock_reader  *reader_convert; // 读取发送队列 处理完后 把转换后的数据回写到 recv_list 中, 
	// style 设置为 0 表示不被 wlog 处理
	s_sis_map_pointer  *converts;       // 需要转换的表 (dataset+table) s_sis_pointer_list * --> s_sisdb_convert

	s_sis_map_pointer  *user_auth;    // 用户账号密码 s_sisdb_userinfo 索引为 username
	s_sis_map_kint  *user_access;  // 用户权限信息 s_sisdb_userinfo 索引为 cid

	bool                switch_wget;  // 设置后所有 get 会同时存盘 方便检查导出数据到文件

	s_sisdb_catch       catch_cfg;    // 数据加载配置 
	s_sis_map_list     *datasets;     // 数据集合 s_sis_worker 分为不同目录存储 

	s_sis_net_class    *socket;          // 服务监听器 s_sis_net_server

}s_sisdb_server_cxt;

//////////////////////////////////
// s_sisdb_convert
///////////////////////////////////
s_sisdb_convert *sisdb_convert_create(const char *dataset_, s_sis_dynamic_convert *convert_);
void sisdb_convert_destroy(void *);
int sisdb_convert_init(s_sisdb_server_cxt *server_, s_sis_json_node *node_);
int sisdb_convert_working(s_sisdb_server_cxt *server_, s_sis_net_message *netmsg_);

//////////////////////////////////
// s_sisdb_index
// 索引块 用于当日订阅时顺序输出数据
///////////////////////////////////

//////////////////////////////////
// s_sisdb_server
///////////////////////////////////

bool  sisdb_server_init(void *, void *);
void  sisdb_server_uninit(void *);
void  sisdb_server_method_init(void *);
void  sisdb_server_method_uninit(void *);

int _sisdb_server_stop(s_sisdb_server_cxt *context, int cid);
int _sisdb_server_load(s_sisdb_server_cxt *context);
int _sisdb_server_save(s_sisdb_server_cxt *context);
int _sisdb_server_pack(s_sisdb_server_cxt *context);

int cmd_sisdb_server_auth(void *worker_, void *argv_);
int cmd_sisdb_server_show(void *worker_, void *argv_);
int cmd_sisdb_server_save(void *worker_, void *argv_);
int cmd_sisdb_server_pack(void *worker_, void *argv_);
int cmd_sisdb_server_call(void *worker_, void *argv_);
int cmd_sisdb_server_wget(void *worker_, void *argv_);

bool sisdb_server_check_auth(s_sisdb_server_cxt *context, s_sis_net_message *netmsg);
void sisdb_server_decr_auth(s_sisdb_server_cxt *context, int cid_);
void sisdb_server_incr_auth(s_sisdb_server_cxt *context, int cid, s_sisdb_userinfo *info);


#endif