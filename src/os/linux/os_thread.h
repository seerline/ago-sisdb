#ifndef _OS_THREAD_H
#define _OS_THREAD_H

#include <pthread.h>
#include <stdbool.h>
#include <assert.h>

#include <os_time.h>
#include <stdio.h>
#include <errno.h>

// 超过时间才返回该值，如果强制退出不返回该值
#define SIS_ETIMEDOUT ETIMEDOUT
// 线程常量定义
#define SIS_THREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define SIS_THREAD_PROC void *

// 线程类型定义
typedef void * (SIS_THREAD_START_ROUTINE)(void *); 
typedef pthread_mutex_t s_sis_mutex_t;
typedef pthread_cond_t s_sis_cond_t;
typedef pthread_t s_sis_thread_id_t;

typedef struct s_sis_thread {	
	int                timeout; 
	bool               working;  // 1 工作中 0 结束
	s_sis_thread_id_t  thread_id;  
} s_sis_thread;

// 线程函数定义
#ifdef __cplusplus
extern "C" {
#endif
bool sis_thread_create(SIS_THREAD_START_ROUTINE func_, void* val_, s_sis_thread *thread_);
// 等待线程结束
void sis_thread_finish(s_sis_thread *thread_);
void sis_thread_join(s_sis_thread *thread_); 
// 仅仅对linux，释放线程资源
void sis_thread_clear(s_sis_thread *thread);
//获取线程ID
s_sis_thread_id_t sis_thread_self(); 
// 杀死
#ifdef __cplusplus
}
#endif
#define sis_thread_kill kill

// 互斥锁定义
// windows支持的锁
// PTHREAD_MUTEX_RECURSIVE_NP  即嵌套锁
// linux支持的锁
#ifndef  __APPLE__
#define  SIS_PTHREAD_MUTEX_NORMAL      PTHREAD_MUTEX_FAST_NP // 普通锁
#define  SIS_PTHREAD_MUTEX_RECURSIVE   PTHREAD_MUTEX_RECURSIVE_NP // 嵌套锁
#define  SIS_PTHREAD_MUTEX_ERRORCHECK  PTHREAD_MUTEX_ERRORCHECK_NP // 纠错锁
#define  SIS_PTHREAD_MUTEX_TIMED_NP    PTHREAD_MUTEX_TIMED_NP
#define  SIS_PTHREAD_MUTEX_ADAPTIVE_NP PTHREAD_MUTEX_ADAPTIVE_NP  // 适应锁
#else
#define  SIS_PTHREAD_MUTEX_NORMAL    PTHREAD_MUTEX_NORMAL // 普通锁
#define  SIS_PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE // 嵌套锁
#define  SIS_PTHREAD_MUTEX_ERRORCHECK PTHREAD_MUTEX_ERRORCHECK // 纠错锁
#endif



int  sis_mutex_create(s_sis_mutex_t *mutex_);
#define sis_mutex_destroy 	pthread_mutex_destroy
#define sis_mutex_lock    	pthread_mutex_lock
#define sis_mutex_unlock  	pthread_mutex_unlock
#define sis_mutex_init    	pthread_mutex_init
#define sis_mutex_trylock   pthread_mutex_trylock

#define sis_pthread_cond_init	pthread_cond_init
#define sis_pthread_mutex_init  pthread_mutex_init

typedef struct s_sis_wait {
	bool          used;  // 是否正在使用
	s_sis_cond_t  cond;  
	s_sis_mutex_t mutex;
} s_sis_wait;
// 百思不得解 不能动态申请

#ifdef __cplusplus
extern "C" {
#endif
void sis_thread_wait_create(s_sis_wait *wait_);
void sis_thread_wait_destroy(s_sis_wait *wait_);
void sis_thread_wait_kill(s_sis_wait *wait_);
void sis_thread_wait_notice(s_sis_wait *wait_);

// 采用这种延时方式一般延时都在1秒以上，否则没有必要这么复杂，所以delay单位为秒
int   sis_thread_wait_sleep(s_sis_wait *wait_, int delay_);
int   sis_thread_wait_sleep_msec(s_sis_wait *wait_, int msec_);
void  sis_thread_wait_start(s_sis_wait *wait_);
void  sis_thread_wait_stop(s_sis_wait *wait_);
#ifdef __cplusplus
}
#endif

#endif /* _SIS_THREAD_H */
