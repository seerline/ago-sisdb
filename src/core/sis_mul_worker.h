#ifndef _sis_mul_worker_h
#define _sis_mul_worker_h

#include "sis_method.h"
#include "sis_thread.h"
#include "sis_list.h"
#include "sis_time.h"

typedef struct s_sis_event_info
{
	int    style;     // 事件类型
	int    nums;      // 数量
    msec_t usec_set;  // 开始
	msec_t usec_sum;  // 总花费时间
	msec_t usec_max;  // 最大
	msec_t usec_min;  // 最小
} s_sis_event_info;

#define SIS_EVENT_OPEN(T,_i_,_e_) if (T) { _i_[_e_].usec_set = sis_time_get_now_usec(); }
#define SIS_EVENT_STOP(T,_i_,_e_) if (T) { \
            _i_[_e_].style = _e_;\
            _i_[_e_].nums++;\
            msec_t costusec = sis_time_get_now_usec() - _i_[_e_].usec_set;\
            _i_[_e_].usec_sum += costusec;\
            _i_[_e_].usec_max = SIS_MAXI(_i_[_e_].usec_max, costusec);\
            _i_[_e_].usec_min = SIS_MINI(_i_[_e_].usec_min, costusec);\
        }
typedef struct s_sis_worker_task
{
    void                         *context;    // 上下文
    void                         *argv;       // 参数
    void                        (*vfree)(void *);
    sis_method_define            *method;     // 调用方法
    struct s_sis_worker_task     *next;
} s_sis_worker_task;

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

/////////////////////////////////////////////////
//  s_sis_worker_tasks
/////////////////////////////////////////////////
typedef struct s_sis_worker_tasks {

    int                   isstop;
    s_sis_wait_thread    *work_thread;

	s_sis_mutex_t         lock;  

    volatile int          wnums;
    s_sis_worker_task    *whead;
    s_sis_worker_task    *wtail;

    volatile int          rnums;
    s_sis_worker_task    *rhead;  // 已经被pop出去 保存在这里 除非下一次pop或free
	s_sis_worker_task    *rtail;
} s_sis_worker_tasks;


s_sis_worker_tasks *sis_worker_tasks_create();
void sis_worker_tasks_destroy(s_sis_worker_tasks *);

int  sis_worker_tasks_add_task(s_sis_worker_tasks *, sis_method_define *method, void *context,void *argv, void *vfree);
// 队列是否为空
int  sis_worker_tasks_count(s_sis_worker_tasks *);

// 等待所有任务执行完毕 才返回
void sis_worker_tasks_wait(s_sis_worker_tasks *);


#endif