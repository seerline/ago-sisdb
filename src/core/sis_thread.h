#ifndef _SIS_THREAD_H
#define _SIS_THREAD_H

#include <sis_core.h>
#include <sis_time.h>
#include <sis_list.h>

// 任务运行模式 间隔秒数运行，按分钟时间点运行
#define SIS_WORK_MODE_NONE     0
#define SIS_WORK_MODE_GAPS     1  // 间隔秒数运行，需要配合开始和结束时间
#define SIS_WORK_MODE_PLANS    2  // 按时间列表运行，时间精确到分钟
#define SIS_WORK_MODE_ONCE     3  // 只运行一次

typedef struct s_sis_plan_task {
	int  		 		work_mode; 
	bool         		working;          // 退出时设置为false 
	s_sis_struct_list  *work_plans;       // plans-work 定时任务 uint16 的数组
	s_sis_time_gap      work_gap; 		  // always-work 循环运行的配置

	s_sis_mutex_t 		mutex;  // 锁
	s_sis_thread_id_t   work_pid;

	s_sis_wait 			wait;   //   线程内部延时处理
	void(*call)(void *);        // ==NULL 不释放对应内存
} s_sis_plan_task;

s_sis_plan_task *sis_plan_task_create();

void sis_plan_task_destroy(s_sis_plan_task *task_);

bool sis_plan_task_start(s_sis_plan_task *task_,SIS_THREAD_START_ROUTINE func_, void* val_);

bool sis_plan_task_working(s_sis_plan_task *task_);
bool sis_plan_task_execute(s_sis_plan_task *task_);  // 检查时间是否到了，可以执行就返回真


#endif //_SIS_TIME_H