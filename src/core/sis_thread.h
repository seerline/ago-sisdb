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

// 返回 0 表示加锁成功
int sis_mutex_rw_try_lock_w(s_sis_mutex_rw *mutex_);
int sis_mutex_rw_try_lock_r(s_sis_mutex_rw *mutex_);

////////////////////////
// 等待式线程定义
/////////////////////////
#define SIS_WAIT_STATUS_NONE   0  
#define SIS_WAIT_STATUS_WORK   1  
#define SIS_WAIT_STATUS_EXIT   2  

#define SIS_WAIT_TIMEOUT   0  
#define SIS_WAIT_NOTICE    1  

typedef struct s_sis_wait_thread {
	int                 wait_msec;//等待时间，单位：毫秒
	int         		work_status;   
	s_sis_thread        work_thread;  
	s_sis_wait         *work_wait; 
	s_sis_wait_handle 	wait_notice;
} s_sis_wait_thread;

s_sis_wait_thread *sis_wait_thread_create(int waitmsec_);

void sis_wait_thread_destroy(s_sis_wait_thread *swt_);
// 通知执行
void sis_wait_thread_notice(s_sis_wait_thread *swt_);

bool sis_wait_thread_open(s_sis_wait_thread *swt_, cb_thread_working func_, void *source_);
// 退出线程标志 但不会马上退出
void sis_wait_thread_close(s_sis_wait_thread *swt_);
// 是否为正在工作
static inline bool sis_wait_thread_iswork(s_sis_wait_thread *swt_)
{
	return (swt_->work_status == SIS_WAIT_STATUS_WORK);
}
static inline bool sis_wait_thread_isexit(s_sis_wait_thread *swt_)
{
	return (swt_->work_status == SIS_WAIT_STATUS_EXIT);
}

void sis_wait_thread_start(s_sis_wait_thread *swt_);
bool sis_wait_thread_noexit(s_sis_wait_thread *swt_);
int sis_wait_thread_wait(s_sis_wait_thread *swt_, int waitmsec_);
void sis_wait_thread_stop(s_sis_wait_thread *swt_);

#endif //_SIS_TIME_H