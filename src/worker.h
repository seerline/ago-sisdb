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
//  worker
/////////////////////////////////////////////////

typedef struct s_sis_worker {
	int                     status;        // 当前的工作状态

    s_sis_sds               classname;     // 归属类名

    s_sis_sds               workername;
                    
    void                   *context;       // 保存上下文数据指针

    void                   *father;        // 父指针 为 NULL 表示依赖于 server 存在

    s_sis_map_pointer      *workers;       // 该工作者 次一级 的工作线程

    int                     method_status; // 方法状态 0 - 未初始化 1 - 初始化完成

	s_sis_methods          *methods;       // 该工作者 提供的方法
         
    s_sis_modules          *slots;         // 该工作者 标准接插函数指针
  
    s_sis_service_thread   *service_thread;       // 该工作者 对应的服务线程
	
} s_sis_worker;

#ifdef __cplusplus
extern "C" {
#endif
// 可能为server
s_sis_worker *sis_worker_create(s_sis_worker *worker_, s_sis_json_node *);
void sis_worker_destroy(void *);

s_sis_worker *sis_worker_load(s_sis_worker *worker_, s_sis_json_node *);

// 在 worker_ 下根据 node 配置生成多个子 worker, worker_ 为空表示依赖于 server
int sis_worker_init_multiple(s_sis_worker *worker_, s_sis_json_node *);

// 调用指定 worker 的方法
int sis_worker_command(s_sis_worker *worker_, const char *cmd_, void *msg_);

// 得到 worker 子工作者下属指定名字 worker
s_sis_worker *sis_worker_get(s_sis_worker *worker_, const char *);

// 从根部找起 workname_ = aaa.bbb.ccc
s_sis_worker *sis_worker_search(const char *workname_);

// 在 notice 工作模式下 手动通知执行 working 方法
void sis_worker_notice(s_sis_worker *worker_);

#ifdef __cplusplus
}
#endif


#endif