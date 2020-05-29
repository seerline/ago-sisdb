#ifndef _OS_THREAD_H
#define _OS_THREAD_H

#include <sis_os.h>
#include <os_time.h>
#include <minwinbase.h>
// 超过时间才返回该值，如果强制退出不返回该值
#define SIS_ETIMEDOUT ETIMEDOUT  // 60
// 线程常量定义
typedef void *(_stdcall SIS_THREAD_START_ROUTINE)(void *);
#define  SIS_THREAD_PROC unsigned int _stdcall

// 线程类型定义
//ssstypedef void * (SIS_THREAD_START_ROUTINE)(void *); 
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
#ifdef __cplusplus
extern "C" {
#endif
// 线程函数定义
bool sis_thread_create(SIS_THREAD_START_ROUTINE func, void* var,  s_sis_thread *thread);
// 等待线程结束
void sis_thread_join(s_sis_thread_id_t thread); // 等待线程结束
void sis_thread_clear(s_sis_thread_id_t thread); // 仅仅对linux，释放线程资源
s_sis_thread_id_t sis_thread_self(); //获取线程ID
// 获取线程ID
s_sis_thread_id_t sis_thread_self(); 
// 杀死
void sis_thread_kill(s_sis_thread_id_t thread);

// 互斥锁定义
// windows支持的锁
// PTHREAD_MUTEX_RECURSIVE_NP  即嵌套锁
int  sis_mutex_create(s_sis_mutex_t *m);
void sis_mutex_destroy(s_sis_mutex_t *m);
void sis_mutex_lock(s_sis_mutex_t *m);
void sis_mutex_unlock(s_sis_mutex_t *m);
//#define sis_mutex_init    	pthread_mutex_init
//#define sis_mutex_trylock   pthread_mutex_trylock

/*#ifdef __cplusplus
}
#endif

// 线程同步条件定义

#ifdef __cplusplus
extern "C" {
#endif*/

typedef struct s_sis_wait {
	bool          end;
	//s_sis_cond_t  cond;
	s_sis_mutex_t mutex;
} s_sis_wait;

void sis_thread_wait_create(s_sis_wait *wait_);
void sis_thread_wait_destroy(s_sis_wait *wait_);
void sis_thread_wait_kill(s_sis_wait *wait_);

// 采用这种延时方式一般延时都在1秒以上，否则没有必要这么复杂，所以delay单位为秒
int   sis_thread_wait_sleep(s_sis_wait *wait_, int delay_);
void  sis_thread_wait_start(s_sis_wait *wait_);
void  sis_thread_wait_stop(s_sis_wait *wait_);

#ifdef __cplusplus
}
#endif

#endif /* _SIS_THREAD_H */
