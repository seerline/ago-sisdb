#ifndef _OS_THREAD_H
#define _OS_THREAD_H

// #include <sis_os.h>
#include <os_time.h>
#include <minwinbase.h>

// 超过时间才返回该值，如果强制退出不返回该值
#define SIS_ETIMEDOUT ETIMEDOUT  // 60
// 线程常量定义
typedef void *(_stdcall SIS_THREAD_START_ROUTINE)(void *);
#define  SIS_THREAD_PROC unsigned int _stdcall

// 线程类型定义
typedef CRITICAL_SECTION s_sis_mutex_t;
//typedef pthread_cond_t s_sis_cond_t;
typedef HANDLE s_sis_thread_id_t;

#define sis_thread_mutex_destroy(m) DeleteCriticalSection(m)
#define sis_thread_mutex_init(m,v) InitializeCriticalSection(m)
#define sis_thread_mutex_lock(m) EnterCriticalSection(m)
#define sis_thread_mutex_unlock(m) LeaveCriticalSection(m)
#define SIS_THREAD_MUTEX_INITIALIZER {(void*)-1,-1,0,0,0,0}

typedef struct s_sis_thread {	
	int                       timeout; 
	bool                      working;  // 1 工作中 0 结束
	s_sis_thread_id_t         thread_id;  
	SIS_THREAD_START_ROUTINE *worker;
	void 					 *argv;
} s_sis_thread;

typedef struct s_sis_wait {
	unsigned char status; // 0 正常 1 退出
	bool          used;
	HANDLE        semaphore;
	int           count;
	s_sis_mutex_t mutex;
} s_sis_wait;

#ifdef __cplusplus
extern "C" {
#endif
// 线程函数定义
bool sis_thread_create(SIS_THREAD_START_ROUTINE func_, void* val_, s_sis_thread *thread_);
// 等待线程结束
void sis_thread_finish(s_sis_thread *thread_);
void sis_thread_join(s_sis_thread *thread_); 
// 仅仅对linux，释放线程资源
void sis_thread_clear(s_sis_thread *thread);
//获取线程ID
s_sis_thread_id_t sis_thread_self(); 
// 杀死
void sis_thread_kill(s_sis_thread_id_t thread);

/////////////////////////////

int  sis_mutex_create(s_sis_mutex_t *m);
void sis_mutex_destroy(s_sis_mutex_t *m);
void sis_mutex_lock(s_sis_mutex_t *m);
void sis_mutex_unlock(s_sis_mutex_t *m);
int  sis_mutex_init(s_sis_mutex_t *m, void *);
int  sis_mutex_trylock(s_sis_mutex_t *m, void *);

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


#ifdef __cplusplus
}
#endif

#include <wdm.h>
typedef int s_sis_unlock_mutex;

#ifdef __cplusplus
extern "C" {
#endif

// https://docs.microsoft.com/zh-cn/windows/win32/sync/synchronization-functions?redirectedfrom=MSDN#interlocked_functions
// #define BCAS InterlockedCompareExchangePointer
// #define VCAS(a,b,c) InterlockedCompareExchange(a,c,b)
// #define ADDF InterlockedExchangeAdd
// #define SUBF InterlockedExchangeSub
// #define ANDF InterlockedExchangeAnd

#ifdef __cplusplus
}
#endif

#endif /* _SIS_THREAD_H */
