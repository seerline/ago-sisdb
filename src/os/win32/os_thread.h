#ifndef _OS_THREAD_H
#define _OS_THREAD_H

// #include <sis_os.h>
// #ifdef WINDOWS
#include <os_time.h>
#include <minwinbase.h>


// 超过时间才返回该值，如果强制退出不返回该值
#define SIS_ETIMEDOUT ETIMEDOUT  // 60
// 线程常量定义
#define SIS_THREAD_MUTEX_INITIALIZER {(void*)-1,-1,0,0,0,0}
#define  SIS_THREAD_PROC unsigned int _stdcall

// 线程类型定义
typedef void *(_stdcall cb_thread_working)(void *);
typedef CRITICAL_SECTION s_sis_mutex_t;

typedef struct s_sis_rwlock_t
{
    HANDLE   m_mutex;  
    HANDLE   m_readEvent;  
    HANDLE   m_writeEvent;  
    unsigned m_readers;  
    unsigned m_writersWaiting;  
    unsigned m_writers;  
} s_sis_rwlock_t;
//typedef pthread_cond_t s_sis_cond_t;
typedef HANDLE s_sis_thread_id_t;

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
unsigned int sis_thread_handle(s_sis_thread_id_t id_); 
// 杀死
void sis_thread_kill(s_sis_thread_id_t thread);
#ifdef __cplusplus
}
#endif

/////////////////////////////
typedef struct s_sis_wait {
	unsigned char status; // 0 正常 1 退出
	bool          used;
	int           count;
	HANDLE        event;
	HANDLE        mutex;
} s_sis_wait;
#ifdef __cplusplus
extern "C" {
#endif

int  sis_mutex_create(s_sis_mutex_t *mutex_);
void sis_mutex_destroy(s_sis_mutex_t *m);
void sis_mutex_lock(s_sis_mutex_t *m);
void sis_mutex_unlock(s_sis_mutex_t *m);
int  sis_mutex_init(s_sis_mutex_t *m, void *);
int  sis_mutex_trylock(s_sis_mutex_t *m);

int  sis_rwlock_init(s_sis_rwlock_t *rwlock_);
void sis_rwlock_destroy(s_sis_rwlock_t *);
void sis_rwlock_lock_r(s_sis_rwlock_t *);
void sis_rwlock_lock_w(s_sis_rwlock_t *);
void sis_rwlock_unlock(s_sis_rwlock_t *);
// 返回 0 表示锁成功
int  sis_rwlock_trylock_r(s_sis_rwlock_t *);
int  sis_rwlock_trylock_w(s_sis_rwlock_t *);

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

// #include <wdm.h>
typedef int s_sis_unlock_mutex;

#ifdef __cplusplus
extern "C" {
#endif

static inline bool atomic_compare_exchange_uint(long *ptr, long agov, long 	newv)
{
	long curv = *ptr;
	long o = _InterlockedCompareExchange(ptr, newv, agov);
	if (o != curv) 
	{
		*ptr = o;
		return false;
	}
	return true;
}		
static inline void atomic_fetch_add_uint(long *ptr, long addv)
{
	_InterlockedExchangeAdd(ptr, addv);
}

static inline void atomic_fetch_sub_uint(long *ptr, long addv)
{
	__pragma(warning(push));
	__pragma(warning(disable: 4146))
	_InterlockedExchangeAdd(ptr, -addv);
	__pragma(warning(pop))
}
/*									\
ATOMIC_INLINE type							\
atomic_fetch_sub_##short_type(atomic_##short_type##_t *a,		\
    type val, atomic_memory_order_t mo) {				\						\
	__pragma(warning(push))						\
	__pragma(warning(disable: 4146))				\
	return atomic_fetch_add_##short_type(a, -val, mo);		\
	__pragma(warning(pop))						\
}	
*/	

// https://docs.microsoft.com/zh-cn/windows/win32/sync/synchronization-functions?redirectedfrom=MSDN#interlocked_functions
#define BCAS(a,b,c) atomic_compare_exchange_uint(a,b,c)
// #define VCAS(a,b,c) InterlockedCompareExchange(a,c,b)
#define ADDF(a,b)   atomic_fetch_add_uint(a,b)
#define SUBF(a,b)   atomic_fetch_sub_uint(a,b)
// #define SUBF InterlockedExchangeSub
// #define ANDF InterlockedExchangeAnd

#ifdef __cplusplus
}
#endif
// #endif // WINDOWS
#endif /* _SIS_THREAD_H */
