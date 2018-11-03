#ifndef _OS_THREAD_H
#define _OS_THREAD_H

#ifdef _MSC_VER
#include <winsock2.h>
#include <process.h>
#include <stdbool.h>
#else
#include <pthread.h>
#include <stdbool.h>
#endif

#ifdef _MSC_VER
typedef unsigned int(_stdcall SIS_THREAD_START_ROUTINE)(void *);
#define  SIS_THREAD_PROC unsigned int _stdcall
#else
// extern "C" { 
	typedef void * (SIS_THREAD_START_ROUTINE)(void *); 
//}
#define SIS_THREAD_PROC void *
#endif

#ifdef _MSC_VER
typedef CRITICAL_SECTION s_sis_mutex_t;
#define sis_thread_mutex_destroy(m) DeleteCriticalSection(m)
#define sis_thread_mutex_init(m,v) InitializeCriticalSection(m)
#define sis_thread_mutex_lock(m) EnterCriticalSection(m)
#define sis_thread_mutex_unlock(m) LeaveCriticalSection(m)
#define SIS_THREAD_MUTEX_INITIALIZER {(void*)-1,-1,0,0,0,0}
#else
typedef pthread_mutex_t s_sis_mutex_t;
#define sis_thread_mutex_destroy pthread_mutex_destroy
#define sis_thread_mutex_init pthread_mutex_init
#define sis_thread_mutex_lock pthread_mutex_lock
#define sis_thread_mutex_unlock pthread_mutex_unlock
#define SIS_THREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#endif

//unsigned long thread = 0;
bool sis_thread_create(SIS_THREAD_START_ROUTINE func, void* var, unsigned long *thread);
void sis_thread_join(unsigned thread); //等待线程结束
void sis_thread_clear(unsigned thread); //仅仅对linux，释放线程资源
unsigned sis_thread_self(); //获取线程ID

// 互斥锁定义
int  sis_mutex_create(s_sis_mutex_t *m);
void sis_mutex_destroy(s_sis_mutex_t *m);
void sis_mutex_lock(s_sis_mutex_t *m);
void sis_mutex_unlock(s_sis_mutex_t *m);

// 多读一写锁定义
typedef struct s_sis_mutex_rw {
	s_sis_mutex_t mutex_s;
	volatile bool try_write_b;
	volatile int reads_i;
	volatile int writes_i;
} s_sis_mutex_rw;

int  sis_mutex_rw_create(s_sis_mutex_rw *m);
void sis_mutex_rw_destroy(s_sis_mutex_rw *m);
void sis_mutex_rw_lock_r(s_sis_mutex_rw *m);
void sis_mutex_rw_unlock_r(s_sis_mutex_rw *m);
void sis_mutex_rw_lock_w(s_sis_mutex_rw *m);
void sis_mutex_rw_unlock_w(s_sis_mutex_rw *m);

#endif /* _SIS_THREAD_H */
