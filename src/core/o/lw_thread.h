#ifndef __LW_THREAD_H
#define __LW_THREAD_H

#ifdef _MSC_VER
#include <winsock2.h>
#include <process.h>
#include <stdbool.h>
#else
#include <pthread.h>
#endif

#ifdef _MSC_VER
typedef unsigned int(_stdcall THREAD_START_ROUTINE)(void *);
#define  THREAD_PROC unsigned int _stdcall
#else
extern "C" { typedef void * (THREAD_START_ROUTINE)(void *); }
#define THREAD_PROC void *
#endif

#ifdef _MSC_VER
typedef CRITICAL_SECTION pthread_mutex_t;
#define pthread_mutex_destroy(m) DeleteCriticalSection(m)
#define pthread_mutex_init(m,v) InitializeCriticalSection(m)
#define pthread_mutex_lock(m) EnterCriticalSection(m)
#define pthread_mutex_unlock(m) LeaveCriticalSection(m)
#define PTHREAD_MUTEX_INITIALIZER {(void*)-1,-1,0,0,0,0}
#endif

//unsigned long thread = 0;
bool thread_create(THREAD_START_ROUTINE func, void* var, unsigned long *thread);
void thread_join(unsigned thread); //等待线程结束
void thread_clear(unsigned thread); //仅仅对linux，释放线程资源
unsigned thread_self(); //获取线程ID

// 互斥锁定义
int  mutex_create(pthread_mutex_t *m);
void mutex_destroy(pthread_mutex_t *m);
void mutex_lock(pthread_mutex_t *m);
void mutex_unlock(pthread_mutex_t *m);

// 多读一写锁定义
typedef struct pthread_mutex_rw {
	pthread_mutex_t mutex_s;
	volatile bool try_write_b;
	volatile int reads_i;
	volatile int writes_i;
} pthread_mutex_rw;

int  mutex_rw_create(pthread_mutex_rw *m);
void mutex_rw_destroy(pthread_mutex_rw *m);
void mutex_rw_lock_r(pthread_mutex_rw *m);
void mutex_rw_unlock_r(pthread_mutex_rw *m);
void mutex_rw_lock_w(pthread_mutex_rw *m);
void mutex_rw_unlock_w(pthread_mutex_rw *m);

#endif /* __LW_THREAD_H */
