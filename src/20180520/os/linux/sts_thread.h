#ifndef _STS_THREAD_H
#define _STS_THREAD_H

#ifdef _MSC_VER
#include <winsock2.h>
#include <process.h>
#include <stdbool.h>
#else
#include <pthread.h>
#include <stdbool.h>
#endif

#ifdef _MSC_VER
typedef unsigned int(_stdcall THREAD_START_ROUTINE)(void *);
#define  THREAD_PROC unsigned int _stdcall
#else
// extern "C" { 
	typedef void * (THREAD_START_ROUTINE)(void *); 
//}
#define THREAD_PROC void *
#endif

#ifdef _MSC_VER
typedef CRITICAL_SECTION s_sts_thread_mutex_t;
#define sts_thread_mutex_destroy(m) DeleteCriticalSection(m)
#define sts_thread_mutex_init(m,v) InitializeCriticalSection(m)
#define sts_thread_mutex_lock(m) EnterCriticalSection(m)
#define sts_thread_mutex_unlock(m) LeaveCriticalSection(m)
#define STS_THREAD_MUTEX_INITIALIZER {(void*)-1,-1,0,0,0,0}
#else
typedef pthread_mutex_t s_sts_thread_mutex_t;
#define sts_thread_mutex_destroy pthread_mutex_destroy
#define sts_thread_mutex_init pthread_mutex_init
#define sts_thread_mutex_lock pthread_mutex_lock
#define sts_thread_mutex_unlock pthread_mutex_unlock
#define STS_THREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#endif

//unsigned long thread = 0;
bool sts_thread_create(THREAD_START_ROUTINE func, void* var, unsigned long *thread);
void sts_thread_join(unsigned thread); //等待线程结束
void sts_thread_clear(unsigned thread); //仅仅对linux，释放线程资源
unsigned sts_thread_self(); //获取线程ID

// 互斥锁定义
int  sts_mutex_create(s_sts_thread_mutex_t *m);
void sts_mutex_destroy(s_sts_thread_mutex_t *m);
void sts_mutex_lock(s_sts_thread_mutex_t *m);
void sts_mutex_unlock(s_sts_thread_mutex_t *m);

// 多读一写锁定义
typedef struct s_sts_thread_mutex_rw {
	s_sts_thread_mutex_t mutex_s;
	volatile bool try_write_b;
	volatile int reads_i;
	volatile int writes_i;
} s_sts_thread_mutex_rw;

int  sts_mutex_rw_create(s_sts_thread_mutex_rw *m);
void sts_mutex_rw_destroy(s_sts_thread_mutex_rw *m);
void sts_mutex_rw_lock_r(s_sts_thread_mutex_rw *m);
void sts_mutex_rw_unlock_r(s_sts_thread_mutex_rw *m);
void sts_mutex_rw_lock_w(s_sts_thread_mutex_rw *m);
void sts_mutex_rw_unlock_w(s_sts_thread_mutex_rw *m);

#endif /* _STS_THREAD_H */
