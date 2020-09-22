#ifndef _SISDB_SERVER_H
#define _SISDB_SERVER_H

#include "sis_method.h"
#include "sis_queue.h"
#include "sis_net.h"
#include "sis_net.io.h"
#include "sis_message.h"
#include "worker.h"
#include "sis_dynamic.h"

#define SISDB_STATUS_NONE     0
#define SISDB_STATUS_INITING  1
#define SISDB_STATUS_WORKING  2

// 四个时间尺度的数据表 同名字段会传递只能由大到小
// 'T'  TICK数据 4-8 不覆盖 有就追加数据
// 'M'  分钟数据 4 同值最新覆盖
// 'D'  日数据 4 同值最新覆盖

// 根据SDB的时间定义来确定是什么周期
// # 一个是KEY 一个是属性FIELD
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

// 保留内存数据的限制
typedef struct s_sisdb_catch
{
	int  last_day;  // > 0 表示加载最后几年的数据 0 表示全部加载
	int  last_min;  // > 0 表示加载最后几天的分钟线 0 表示不加载历史数据
	int  last_sec;  // > 0 表示加载最后几天的秒线 0 表示不加载历史数据
	int  last_sno;  // > 0 表示加载最后几天数据 0 表示只保留当日 不从历史数据加载 只从log加载 
} s_sisdb_catch;

typedef struct s_sisdb_userauth
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

	s_sis_share_list   *recv_list;   // 所有收到的数据放队列中 供多线程分享任务

	s_sis_share_reader *reader_recv; // 读取发送队列

	// s_sis_share_reader *reader_wlog; // 读取发送队列 - 只有写动做会记录 wlog 
	s_sis_method       *wlog_method; // 默认传入数据的方法
	s_sis_worker       *wlog_save;   // 实时缓存存储类 对每条指令都记录在盘 save 无误后清除 
	s_sis_mutex_t       wlog_lock;   // wlog & save & pack 互斥  
	// save 时 不再接收数据写入 但仍然可以读取数据

	// 重启加载时从fast_save加载限定数据，然后从wlog中获取最新数据 作为内存数据 

	// 查询和订阅请求 都发送到 fast_save 由这个方法来处理
	s_sis_worker       *fast_save;   // 快速存储类
	// 快速写盘不做 reader 写盘时从内存中获取数据直接写盘 

	s_sis_share_reader *reader_convert; // 读取发送队列 处理完后 把转换后的数据回写到 recv_list 中, 
	// style 设置为 0 表示不被 wlog 处理
	s_sis_map_pointer  *converts;       // 需要转换的表 (dataset+table) s_sis_pointer_list * --> s_sisdb_convert

	s_sis_map_pointer  *user_auth;    // 用户账号密码 s_sisdb_userinfo 索引为 username
	s_sis_map_int      *user_access;  // 用户权限信息 access 索引为 cid

	s_sis_message      *message;     // 消息传递
	
	bool                switch_wget;  // 设置后所有 get 会同时存盘

	s_sisdb_catch       catch_cfg;    // 数据加载配置 
	s_sis_map_list     *datasets;     // 数据集合 s_sis_worker s_sis_db 分为不同目录存储 

	s_sis_net_class    *socket;      // 服务监听器 s_sis_net_server

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
void  sisdb_server_work_init(void *);
void  sisdb_server_working(void *);
void  sisdb_server_work_uninit(void *);
void  sisdb_server_uninit(void *);
void  sisdb_server_method_init(void *);
void  sisdb_server_method_uninit(void *);

int _sisdb_server_load(s_sisdb_server_cxt *context);
int _sisdb_server_save(s_sisdb_server_cxt *context, int date);
int _sisdb_server_pack(s_sisdb_server_cxt *context);

int cmd_sisdb_server_auth(void *worker_, void *argv_);
int cmd_sisdb_server_show(void *worker_, void *argv_);
int cmd_sisdb_server_save(void *worker_, void *argv_);
int cmd_sisdb_server_pack(void *worker_, void *argv_);
int cmd_sisdb_server_call(void *worker_, void *argv_);
int cmd_sisdb_server_wget(void *worker_, void *argv_);

// ***  这里的数据不存盘 ***//
// 为应对数据分发的功能，在server层支持订阅数据 发布数据 的功能 数据内容为二进制流具体解析sisdb不管
// sisdb仅仅作为一个中间通讯组件使用
// snew 注册一个无锁队列
// snew shl2  -1   最小限制 默认值 满足最慢的订阅者
// snew shl2   0   不设限制  
// snew shl2  xxx  指定最大数据容量
int cmd_sisdb_server_snew(void *worker_, void *argv_);
// kpub shl2  数据流写入无锁队列中 
int cmd_sisdb_server_spub(void *worker_, void *argv_);
// 对指定的无锁队列增加订阅者
// ksub shl2  默认从最新的数据开始订阅
// ksub shl2  0 从最开始订阅
// ksub shl2  xxx 从某个时间开始订阅 *不支持*
int cmd_sisdb_server_ssub(void *worker_, void *argv_);
// kdel shl2  0 数据清理  客户端订阅清理 all 全部清理
int cmd_sisdb_server_sdel(void *worker_, void *argv_);
// 需要保留的数据key
int cmd_sisdb_server_sget(void *worker_, void *argv_);
// 
int cmd_sisdb_server_sset(void *worker_, void *argv_);


#endif