#ifndef _SISDB_SERVER_H
#define _SISDB_SERVER_H

#include "sis_method.h"
#include "sis_queue.h"
#include "sis_net.h"
#include "sis_net.io.h"
#include "sis_message.h"

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


typedef struct s_sisdb_userinfo
{
	char username[32];
	char password[32];
	int  access;       // 权限 
} s_sisdb_userinfo;

typedef struct s_sisdb_server_cxt
{
	int    status;

	int    level;    // 等级 默认0为最高级 只能 0 --> 1,2,3... 发数据 ||  0 <--> 0 数据是互相备份
	// 不能反向 用于以后的数据同步
	s_sis_share_reader *reader_recv; // 读取发送队列

	s_sis_share_list   *recv_list;   // 所有收到的数据放队列中 供多线程分享任务

	s_sis_share_reader *reader_wlog; // 读取发送队列 - 只有写动做会记录 wlog 
	s_sis_method       *wlog_method; // 默认传入数据的方法
	s_sis_worker       *wlog_save;   // 实时缓存存储类 对每条指令都记录在盘 save 无误后清除 
	// 下次加载从fast_save加载限定数据 

	s_sis_share_reader *reader_convert; // 读取发送队列
	s_sis_method       *convert_method;   // 默认传入数据的方法
	s_sis_worker       *convert_worker;   // 数据自动切片或转移

	// 节省内存 所有订阅请求都发送到fast_save 由这个方法来处理
	s_sis_method       *fast_method; // 默认传入数据的方法
	s_sis_worker       *fast_save;   // 快速存储类

	// s_sis_worker       *slow_save;   // 慢速存储类
	s_sis_mutex_t       wlog_lock;  // 写时 save 和 write 时加锁 
	s_sis_mutex_t       fast_lock;   // 写时 save 和 write 时加锁 
	s_sis_mutex_t       save_lock;   // 写时 save 和 pack 互排斥

	s_sis_map_pointer  *user_auth;   // 用户账号密码 s_sisdb_userinfo

	s_sis_message      *message;     // 消息传递
	
	bool                logined;     // 是否已经登录     

	bool                switch_wget;  // 设置后所有 get 会同时存盘

	s_sis_map_list     *datasets;    // 数据集合 s_sis_worker s_sis_db 分为不同目录存储 

	s_sis_net_class    *server;      // 服务监听器 s_sis_net_server

}s_sisdb_server_cxt;

bool  sisdb_server_init(void *, void *);
void  sisdb_server_work_init(void *);
void  sisdb_server_working(void *);
void  sisdb_server_work_uninit(void *);
void  sisdb_server_uninit(void *);
void  sisdb_server_method_init(void *);
void  sisdb_server_method_uninit(void *);

int cmd_sisdb_server_auth(void *worker_, void *argv_);
int cmd_sisdb_server_show(void *worker_, void *argv_);
int cmd_sisdb_server_save(void *worker_, void *argv_);
int cmd_sisdb_server_pack(void *worker_, void *argv_);
int cmd_sisdb_server_call(void *worker_, void *argv_);
int cmd_sisdb_server_wlog(void *worker_, void *argv_);

#endif