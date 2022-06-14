#ifndef _OS_THREAD_H
#define _OS_THREAD_H

// 以下几个是测试cpu绑定 实际测试发现指定cpu反而效率低
// #ifndef _GNU_SOURCE
// #define _GNU_SOURCE
// #endif
// #include <sched.h>
// #include <sys/sysinfo.h>

#include <pthread.h>
#include <stdbool.h>
#include <assert.h>

#include <os_time.h>
#include <stdio.h>
#include <errno.h>

#include <os_fork.h>

// 超过时间才返回该值，如果强制退出不返回该值
#define SIS_ETIMEDOUT ETIMEDOUT
// 线程常量定义
#define SIS_THREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define SIS_THREAD_PROC void *

// 线程类型定义
typedef void * (cb_thread_working)(void *); 
typedef pthread_mutex_t s_sis_mutex_t;
typedef pthread_rwlock_t s_sis_rwlock_t;

typedef pthread_cond_t s_sis_cond_t;
typedef pthread_t s_sis_thread_id_t;

typedef struct s_sis_thread {	
	int                timeout; 
	bool               working;  // 1 工作中 0 结束
	s_sis_thread_id_t  thread_id;  
	cb_thread_working *worker;
	void 			  *argv;
} s_sis_thread;

#ifdef __cplusplus
extern "C" {
#endif

bool sis_thread_create(cb_thread_working func_, void* val_, s_sis_thread *thread_);
// 等待线程结束
void sis_thread_finish(s_sis_thread *thread_);
void sis_thread_join(s_sis_thread *thread_); 
// 仅仅对linux，释放线程资源
void sis_thread_clear(s_sis_thread *thread);
//获取线程ID
s_sis_thread_id_t sis_thread_self(); 
// 
unsigned int sis_thread_handle(s_sis_thread_id_t); 
// 杀死
#define sis_thread_kill kill
#ifdef __cplusplus
}
#endif

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

typedef struct s_sis_wait {
	unsigned char status; // 0 正常 1 退出
	bool          used;   // 是否正在使用
	s_sis_cond_t  cond;  
	s_sis_mutex_t mutex;
} s_sis_wait;
// 百思不得解 不能动态申请

#ifdef __cplusplus
extern "C" {
#endif

int  sis_mutex_create(s_sis_mutex_t *mutex_);
#define sis_mutex_destroy 	pthread_mutex_destroy
#define sis_mutex_lock    	pthread_mutex_lock
#define sis_mutex_unlock  	pthread_mutex_unlock
#define sis_mutex_init    	pthread_mutex_init
// 返回 0 表示锁成功
#define sis_mutex_trylock   pthread_mutex_trylock

int  sis_rwlock_init(s_sis_rwlock_t *rwlock_);
#define sis_rwlock_destroy 	  pthread_rwlock_destroy
#define sis_rwlock_lock_r     pthread_rwlock_rdlock
#define sis_rwlock_lock_w     pthread_rwlock_wrlock
#define sis_rwlock_unlock  	  pthread_rwlock_unlock
// 返回 0 表示锁成功
#define sis_rwlock_trylock_r   pthread_rwlock_tryrdlock
#define sis_rwlock_trylock_w   pthread_rwlock_trywrlock

int sis_mutex_wait_lock_r(s_sis_rwlock_t *rwlock_, int msec_);
int sis_mutex_wait_lock_w(s_sis_rwlock_t *rwlock_, int msec_);

void sis_thread_wait_create(s_sis_wait *wait_);
void sis_thread_wait_destroy(s_sis_wait *wait_);
void sis_thread_wait_init(s_sis_wait *wait_);
void sis_thread_wait_kill(s_sis_wait *wait_);
void sis_thread_wait_notice(s_sis_wait *wait_);

// 采用这种延时方式一般延时都在1秒以上，否则没有必要这么复杂，所以delay单位为秒
int   sis_thread_wait_sleep(s_sis_wait *wait_, int delay_);
int   sis_thread_wait_sleep_msec(s_sis_wait *wait_, int msec_);
void  sis_thread_wait_start(s_sis_wait *wait_);
void  sis_thread_wait_stop(s_sis_wait *wait_);

//////////////////////////////////////
//  unlock
//////////////////////////////////////
// **** 频繁发送条件变量激活 耗时增加3倍 增加更高效的锁机制 ***** // 

// type可以是1,2,4或8字节长度的int类型，即：
// int8_t / uint8_t
// int16_t / uint16_t
// int32_t / uint32_t
// int64_t / uint64_t

// 返回更新前的值（先fetch再计算）
// type __sync_fetch_and_add (type *ptr, type value, ...)
// type __sync_fetch_and_sub (type *ptr, type value, ...)
// type __sync_fetch_and_or (type *ptr, type value, ...)
// type __sync_fetch_and_and (type *ptr, type value, ...)
// type __sync_fetch_and_xor (type *ptr, type value, ...)
// type __sync_fetch_and_nand (type *ptr, type value, ...)
// 返回更新后的值（先计算再fetch）
// type __sync_add_and_fetch (type *ptr, type value, ...)
// type __sync_sub_and_fetch (type *ptr, type value, ...)
// type __sync_or_and_fetch (type *ptr, type value, ...)
// type __sync_and_and_fetch (type *ptr, type value, ...)
// type __sync_xor_and_fetch (type *ptr, type value, ...)
// type __sync_nand_and_fetch (type *ptr, type value, ...)
// 后面的可扩展参数(...)用来指出哪些变量需要memory barrier,因为目前gcc实现的是full barrier（
// 类似于linux kernel 中的mb(),表示这个操作之前的所有内存操作不会被重排序到这个操作之后）,所以可以略掉这个参数。


// 两个函数提供原子的比较和交换，如果*ptr == oldval,就将newval写入*ptr,
// 第一个函数在相等并写入的情况下返回true.
// 第二个函数在返回操作之前的值。
// bool __sync_bool_compare_and_swap (type *ptr, type oldval type newval, ...)
// type __sync_val_compare_and_swap (type *ptr, type oldval type newval, ...)

// 将*ptr设为value并返回*ptr操作之前的值。
// type __sync_lock_test_and_set (type *ptr, type value, ...)
// 将*ptr置0
// void __sync_lock_release (type *ptr, ...)
#ifdef __cplusplus
}
#endif

typedef int s_sis_unlock_mutex;



#ifdef __cplusplus
extern "C" {
#endif

#define BCAS __sync_bool_compare_and_swap
#define VCAS __sync_val_compare_and_swap
#define ADDF __sync_add_and_fetch
#define SUBF __sync_sub_and_fetch
#define ANDF __sync_and_and_fetch

// static inline void sis_unlock_mutex_init(s_sis_unlock_mutex *um_, int init_)
// {
//     ANDF(&(*um_), 0);
// }
// static inline void sis_unlock_mutex_lock(s_sis_unlock_mutex *um_)
// {
//     while(!BCAS(&(*um_), 0, 1))
//     {
//         sis_sleep(1);
//     }
// }

// static inline void sis_unlock_mutex_unlock(s_sis_unlock_mutex *um_)
// {
//     while(!BCAS(&(*um_), 1, 0))
//     {
//         sis_sleep(1);
//     }
// }
// // 0 锁住
// static inline int sis_unlock_mutex_try_lock(s_sis_unlock_mutex *um_)
// {
//     if (BCAS(&(*um_), 0, 1))
//     {
//         return 0;
//     }
//     return 1;
// }
// static inline int sis_unlock_mutex_try_unlock(s_sis_unlock_mutex *um_)
// {
//     if (BCAS(&(*um_), 1, 0))
//     {
//         return 0;
//     }
//     return 1;
// }
#ifdef __cplusplus
}
#endif

#endif /* _SIS_THREAD_H */
