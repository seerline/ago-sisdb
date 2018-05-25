#ifndef _OS_THREAD_H
#define _OS_THREAD_H

#include <pthread.h>
#include <stdbool.h>
#include <assert.h>

#include <os_time.h>

// 线程常量定义
#define STS_THREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define THREAD_PROC void *

// 线程类型定义
typedef void * (THREAD_START_ROUTINE)(void *); 
typedef pthread_mutex_t s_sts_thread_mutex_t;
typedef pthread_t s_sts_thread_id_t;

// 线程函数定义
#define sts_thread_mutex_destroy pthread_mutex_destroy
#define sts_thread_mutex_init pthread_mutex_init
#define sts_thread_mutex_lock pthread_mutex_lock
#define sts_thread_mutex_unlock pthread_mutex_unlock

bool sts_thread_create(THREAD_START_ROUTINE func, void* var, s_sts_thread_id_t *thread);
// 等待线程结束
void sts_thread_join(s_sts_thread_id_t thread); 
// 仅仅对linux，释放线程资源
void sts_thread_clear(s_sts_thread_id_t thread);
//获取线程ID
s_sts_thread_id_t sts_thread_self(); 

// 互斥锁定义
int  sts_mutex_create(s_sts_thread_mutex_t *mutex_);
#define sts_mutex_destroy pthread_mutex_destroy
#define sts_mutex_lock pthread_mutex_lock
#define sts_mutex_unlock pthread_mutex_unlock

// 多读一写锁定义
typedef struct s_sts_thread_mutex_rw {
	s_sts_thread_mutex_t mutex_s;
	volatile bool try_write_b;
	volatile int reads_i;
	volatile int writes_i;
} s_sts_thread_mutex_rw;

int  sts_mutex_rw_create(s_sts_thread_mutex_rw *mutex_);
void sts_mutex_rw_destroy(s_sts_thread_mutex_rw *mutex_);
void sts_mutex_rw_lock_r(s_sts_thread_mutex_rw *mutex_);
void sts_mutex_rw_unlock_r(s_sts_thread_mutex_rw *mutex_);
void sts_mutex_rw_lock_w(s_sts_thread_mutex_rw *mutex_);
void sts_mutex_rw_unlock_w(s_sts_thread_mutex_rw *mutex_);

#endif /* _STS_THREAD_H */
