#ifndef _SISDB_SERVER_H
#define _SISDB_SERVER_H

#include "sis_method.h"
#include "sis_list.lock.h"
#include "sis_net.h"
#include "sis_net.msg.h"
#include "sis_message.h"
#include "worker.h"
#include "sis_dynamic.h"
#include "sisdb_sys.h"

#define SISDB_STATUS_NONE  0
#define SISDB_STATUS_INIT  1
#define SISDB_STATUS_WORK  2
#define SISDB_STATUS_EXIT  3

// 打开网络 加载 system 模块 然后加载 init 化脚本
// 根据脚本中参数决定需要启动哪些模块 
// 所有可以被启动的模块 以 OPEN 开始； 以 STOP 结束
// 需要存盘和数据加载的动作 由模块自行管理 server 只解决通讯 和 模块方法调用的 工作

// QQQ 既然每个worker都保留了一个上下文环境，那为什么上下文环境中还要存一个worker列表呢？不应该是最多只存储一个worker么？
/**
 * @brief 服务器上下文结构体，存储包含了如下信息：
 * 用户列表
 * 工作者列表
 * 网络连接
 * 发送队列
 * 接收队列
 */
typedef struct s_sisdb_server_cxt
{
	int                 status;       // 工作状态 0 表示没有初始化 1 表示已经初始化
	int                 level;        // 等级 默认 0 为最高级 只能 0 --> 1,2,3... 发数据 ||  0 <--> 0 数据是互相备份

	s_sis_sds           work_path;     // 系统相关信息 必须的模块
	s_sis_sds           init_name;     // 初始化文件名

	s_sis_worker       *work_system;  // 系统相关信息 必须的模块
	s_sis_map_kint     *user_access;  // 用户权限信息 s_sisdb_userinfo 索引为 cid


	s_sis_map_list     *users;        // 用户列表  s_sisdb_userinfo
	s_sis_map_list     *works;        // 工作列表  s_sisdb_workinfo

	s_sis_net_class    *socket;       // 服务监听器 s_sis_net_server
	s_sis_lock_list    *socket_recv;  // 所有收到的数据放队列中 供多线程分享任务
	s_sis_lock_reader  *reader_recv;  // 读取发送队列

}s_sisdb_server_cxt;


//////////////////////////////////
// s_sisdb_index
// 索引块 用于当日订阅时顺序输出数据
///////////////////////////////////

//////////////////////////////////
// s_sisdb_server
///////////////////////////////////

bool  sisdb_server_init(void *, void *);
void  sisdb_server_uninit(void *);
void  sisdb_server_working(void *);
void  sisdb_server_work_init(void *);
void  sisdb_server_work_uninit(void *);

int _sisdb_server_stop(s_sisdb_server_cxt *context, int cid);
int _sisdb_server_load(s_sisdb_server_cxt *context);

int cmd_sisdb_server_auth(void *worker_, void *argv_);
int cmd_sisdb_server_show(void *worker_, void *argv_);
int cmd_sisdb_server_setuser(void *worker_, void *argv_);

int cmd_sisdb_server_sub(void *worker_, void *argv_);
int cmd_sisdb_server_unsub(void *worker_, void *argv_);

int cmd_sisdb_server_open(void *worker_, void *argv_);
int cmd_sisdb_server_close(void *worker_, void *argv_);
int cmd_sisdb_server_save(void *worker_, void *argv_);
int cmd_sisdb_server_pack(void *worker_, void *argv_);
int cmd_sisdb_server_call(void *worker_, void *argv_);

void sisdb_server_sysinfo_save(s_sisdb_server_cxt *context);
int  sisdb_server_sysinfo_load(s_sisdb_server_cxt *context);

int sisdb_server_open_works(s_sis_worker *worker_);
int sisdb_server_open(s_sis_worker *worker_, const char *workname_, const char *config_);


#endif