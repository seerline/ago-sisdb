#ifndef _sis_mul_worker_h
#define _sis_mul_worker_h

#include "sis_method.h"
#include "sis_thread.h"
#include "sis_list.h"

typedef struct s_sis_mul_worker_task
{
    void               *context;    // 上下文
    void               *argv;       // 参数
    void              (*vfree)(void *);
    sis_method_define  *method;     // 调用方法
} s_sis_mul_worker_task;

typedef struct s_sis_mul_worker_thread
{
    s_sis_wait_thread  *work_thread;
    s_sis_mutex_t       wlock; 
    s_sis_pointer_list *tasks;      // 事件列表 处理完一条删除一条
} s_sis_mul_worker_thread;

typedef struct s_sis_mul_worker_cxt
{
    int                       wnums;      // 线程数 最小 1 最大64 
    s_sis_mul_worker_thread **wthreads;   // 线程池处理 s_sis_mul_worker_thread
} s_sis_mul_worker_cxt;

s_sis_mul_worker_thread *sis_mul_worker_thread_create();
void sis_mul_worker_thread_destroy(s_sis_mul_worker_thread *);
void sis_mul_worker_thread_add_task(s_sis_mul_worker_thread *, sis_method_define *method, void *context,void *argv, void *vfree);

s_sis_mul_worker_cxt *sis_mul_worker_cxt_create(int nums);
void sis_mul_worker_cxt_destroy(s_sis_mul_worker_cxt *);
void sis_mul_worker_cxt_add_task(s_sis_mul_worker_cxt *, const char *hashstr,
    sis_method_define *method, void *context,void *argv, void *vfree);
void sis_mul_worker_cxt_add_task_i(s_sis_mul_worker_cxt *, int tidx,
    sis_method_define *method, void *context,void *argv, void *vfree);

// 等待所有任务执行完毕 才返回
void sis_mul_worker_cxt_wait(s_sis_mul_worker_cxt *);

#endif