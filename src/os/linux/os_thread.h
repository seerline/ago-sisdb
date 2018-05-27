#ifndef _OS_THREAD_H
#define _OS_THREAD_H

#include <pthread.h>
#include <stdbool.h>
#include <assert.h>

#include <os_time.h>

#include <errno.h>

#define STS_ETIMEDOUT ETIMEDOUT
// 线程常量定义
#define STS_THREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define STS_THREAD_PROC void *

// 线程类型定义
typedef void * (STS_THREAD_START_ROUTINE)(void *); 
typedef pthread_mutex_t s_sts_mutex_t;
typedef pthread_cond_t s_sts_cond_t;
typedef pthread_t s_sts_thread_id_t;

// 线程函数定义

bool sts_thread_create(STS_THREAD_START_ROUTINE func_, void* val_, s_sts_thread_id_t *thread_);
// 等待线程结束
void sts_thread_join(s_sts_thread_id_t thread); 
// 仅仅对linux，释放线程资源
void sts_thread_clear(s_sts_thread_id_t thread);
//获取线程ID
s_sts_thread_id_t sts_thread_self(); 
// 杀死
#define sts_thread_kill kill

// 互斥锁定义
int  sts_mutex_create(s_sts_mutex_t *mutex_);
#define sts_mutex_destroy pthread_mutex_destroy
#define sts_mutex_lock    pthread_mutex_lock
#define sts_mutex_unlock  pthread_mutex_unlock
#define sts_mutex_init    pthread_mutex_init

// 多读一写锁定义
typedef struct s_sts_mutex_rw {
	s_sts_mutex_t mutex_s;
	volatile bool try_write_b;
	volatile int reads_i;
	volatile int writes_i;
} s_sts_mutex_rw;

int  sts_mutex_rw_create(s_sts_mutex_rw *mutex_);
void sts_mutex_rw_destroy(s_sts_mutex_rw *mutex_);
void sts_mutex_rw_lock_r(s_sts_mutex_rw *mutex_);
void sts_mutex_rw_unlock_r(s_sts_mutex_rw *mutex_);
void sts_mutex_rw_lock_w(s_sts_mutex_rw *mutex_);
void sts_mutex_rw_unlock_w(s_sts_mutex_rw *mutex_);

typedef struct s_sts_wait {
	s_sts_cond_t cond;  
	s_sts_mutex_t mutex;
} s_sts_wait;

void sts_thread_wait_create(s_sts_wait *wait_);
void sts_thread_wait_destroy(s_sts_wait *wait_);
void sts_thread_wait_kill(s_sts_wait *wait_);

// 采用这种延时方式一般延时都在1秒以上，否则没有必要这么复杂，所以delay单位为秒
int   sts_thread_wait_sleep(s_sts_wait *wait_, int delay_);
void  sts_thread_wait_start(s_sts_wait *wait_);
void  sts_thread_wait_stop(s_sts_wait *wait_);


#endif /* _STS_THREAD_H */
