#ifndef _WORKER_H
#define _WORKER_H

#include <sis_core.h>
#include <sis_json.h>
#include <sis_list.h>
#include <sis_sds.h>

#include <sis_thread.h>
#include <sis_map.h>
#include <sis_method.h>

#include <sis_modules.h>
#include <sis_message.h>

/////////////////////////////////////////////////
//  worker thread
/////////////////////////////////////////////////

// 任务运行模式 间隔秒数运行，按分钟时间点运行
#define SIS_WORK_MODE_NONE     0  // 不运行
#define SIS_WORK_MODE_GAPS     1  // 在指定时间内 每隔 delay 毫秒运行一次working
#define SIS_WORK_MODE_PLANS    2  // 计划任务 按数组中指定时间触发working 单位为分钟
#define SIS_WORK_MODE_ONCE     3  // 默认值，每次启动该类只运行一次working 执行完就销毁
#define SIS_WORK_MODE_NOTICE   4  // 等候通知去执行working, 执行期间的通知忽略, 执行后再等下一个通知

typedef struct s_sis_work_thread {
	int  		 		work_mode;     // 任务模式，NONE，GAPS，PLANS，ONCE，NOTICE
	bool                isfirst;       // 对于之要求执行一次的任务，用于判断是否是第一次执行，初始为false，表示已完成第0次执行，改为true时表示已经完成第一次执行
	s_sis_struct_list  *work_plans;    // plans-work 定时任务 uint16 的数组
	s_sis_time_gap      work_gap; 	   // always-work 循环运行的配置
    s_sis_wait_thread  *work_thread;   //
} s_sis_work_thread;

/////////////////////////////////////////////////
//  worker
/////////////////////////////////////////////////
#define SIS_WORK_INIT_NONE       0  // 
#define SIS_WORK_INIT_METHOD     1  // 在指定时间内 每隔 delay 毫秒运行一次working
#define SIS_WORK_INIT_WORKER     2  // 计划任务 按数组中指定时间触发working 单位为分钟

typedef struct s_sis_worker {
	int                    status;        // 当前的工作状态
	int                    style;         // 工作者的类型 0 - normal 1 - python
    s_sis_sds              classname;     // 归属类名
    s_sis_sds              workername;
    void                  *context;        // 保存上下文数据指针
    void                  *father;         // 父指针 为 NULL 表示依赖于 server 存在
    s_sis_map_pointer     *workers;        // 该工作者 次一级 的工作线程
    int                    method_status;  // 方法状态 0 - 未初始化 1 - 初始化完成
	s_sis_methods         *methods;        // 该工作者 提供的方法
    s_sis_modules         *slots;          // 该工作者 标准接插函数指针
    s_sis_work_thread     *service_thread; // 该工作者 对应的服务线程
} s_sis_worker;

#ifdef __cplusplus
extern "C" {
#endif

s_sis_work_thread *sis_work_thread_create();

void sis_work_thread_destroy(s_sis_work_thread *task_);

bool sis_work_thread_open(s_sis_work_thread *task_,cb_thread_working func_, void* val_);

bool sis_work_thread_working(s_sis_work_thread *task_);  // 检查时间是否到了，可以执行就返回真

/////////////////////////////////////////////////
//  worker
/////////////////////////////////////////////////

// 在 notice 工作模式下 手动通知执行 working 方法
void sis_work_thread_notice(s_sis_worker *worker_);

// 可能为server
s_sis_worker *sis_worker_create_of_name(s_sis_worker *worker_, const char *name_, s_sis_json_node *);
s_sis_worker *sis_worker_create_of_conf(s_sis_worker *worker_, const char *name_, const char *config_);
s_sis_worker *sis_worker_create(s_sis_worker *worker_, s_sis_json_node *);
void sis_worker_destroy(void *);

s_sis_worker *sis_worker_load(s_sis_worker *worker_, s_sis_json_node *);

// 在 worker_ 下根据 node 配置生成多个子 worker, worker_ 为空表示依赖于 server
int sis_worker_init_multiple(s_sis_worker *worker_, s_sis_json_node *);

// 调用指定 worker 的方法
int sis_worker_command(s_sis_worker *worker_, const char *cmd_, void *msg_);

// 返回指定 worker 的方法
s_sis_method *sis_worker_get_method(s_sis_worker *worker_, const char *cmd_);

// 得到 worker 子工作者下属指定名字 worker
s_sis_worker *sis_worker_get(s_sis_worker *worker_, const char *);

// 从根部找起 workname_ = aaa.bbb.ccc
s_sis_worker *sis_worker_search(const char *workname_);

#ifdef __cplusplus
}
#endif


#endif