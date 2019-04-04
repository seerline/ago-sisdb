#ifndef _SIS_THREAD_H
#define _SIS_THREAD_H

#include <sis_os.h>
#include <sis_core.h>
#include <sis_time.h>
#include <sis_list.h>
#include <os_thread.h>

typedef int s_sis_wait_handle;

s_sis_wait_handle sis_wait_malloc();
s_sis_wait *sis_wait_get(s_sis_wait_handle id_);
void sis_wait_free(s_sis_wait_handle id_);

// 多读一写锁定义
typedef struct s_sis_mutex_rw {
	s_sis_mutex_t mutex_s;
	volatile bool try_write_b;
	volatile int  reads_i;
	volatile int  writes_i;
} s_sis_mutex_rw;

int  sis_mutex_rw_create(s_sis_mutex_rw *mutex_);
void sis_mutex_rw_destroy(s_sis_mutex_rw *mutex_);
void sis_mutex_rw_lock_r(s_sis_mutex_rw *mutex_);
void sis_mutex_rw_unlock_r(s_sis_mutex_rw *mutex_);
void sis_mutex_rw_lock_w(s_sis_mutex_rw *mutex_);
void sis_mutex_rw_unlock_w(s_sis_mutex_rw *mutex_);


////////////////////////
// 线程任务定义
/////////////////////////

// 任务运行模式 间隔秒数运行，按分钟时间点运行
#define SIS_WORK_MODE_NONE     0
#define SIS_WORK_MODE_GAPS     1  // 间隔秒数运行，需要配合开始和结束时间
#define SIS_WORK_MODE_PLANS    2  // 按时间列表运行，时间精确到分钟
#define SIS_WORK_MODE_ONCE     3  // 只运行一次

typedef struct s_sis_plan_task {
	int  		 		work_mode; 
	bool                isfirst;
	bool         		working;          // 退出时设置为false 
	s_sis_struct_list  *work_plans;       // plans-work 定时任务 uint16 的数组
	s_sis_time_gap      work_gap; 		  // always-work 循环运行的配置

	s_sis_mutex_t 		mutex;  // 锁
	s_sis_thread        work_thread;   // 其中working 是线程完全执行完毕

	s_sis_wait_handle 	wait_handle;   //   线程内部延时处理
	void(*call)(void *);        // ==NULL 不释放对应内存
} s_sis_plan_task;

s_sis_plan_task *sis_plan_task_create();

void sis_plan_task_destroy(s_sis_plan_task *task_);

bool sis_plan_task_start(s_sis_plan_task *task_,SIS_THREAD_START_ROUTINE func_, void* val_);

void sis_plan_task_wait_start(s_sis_plan_task *task_);
void sis_plan_task_wait_stop(s_sis_plan_task *task_);

bool sis_plan_task_working(s_sis_plan_task *task_);
bool sis_plan_task_execute(s_sis_plan_task *task_);  // 检查时间是否到了，可以执行就返回真


#endif //_SIS_TIME_H