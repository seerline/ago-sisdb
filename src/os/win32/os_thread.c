
#include "os_thread.h"
#include <os_time.h>
#include <assert.h>
#include <synchapi.h>

unsigned _thread_process(void* arg)
{
	s_sis_thread *thread = (s_sis_thread *)arg;
	thread->worker(thread->argv);
	return 0;
}
bool sis_thread_create(cb_thread_working func, void* var, s_sis_thread *thread)
{
	uintptr_t result = 0;
	unsigned int id = 0;
	result = _beginthreadex(NULL, 0, &_thread_process, thread, 0, &id);
	if (result == 0)
	{
		return false;
	}
	thread->argv = var;
	thread->worker = func;
	thread->thread_id = (s_sis_thread_id_t)result;
	thread->working = true;
	return true;
}
void sis_thread_finish(s_sis_thread *thread_)
{
	if (thread_)
	{
		thread_->working = false;
	}
}
void sis_thread_join(s_sis_thread *thread_)
{
	int msec = thread_->timeout < 500 ? 500 : thread_->timeout;
	while(thread_->working)
	{
		sis_sleep(10);
		msec-=10;
		if (msec < 0)
		{
			// 防止用户使用不当时延时退出
			break;
		}
	}
	WaitForSingleObject((void*)thread_->thread_id, INFINITE);
	CloseHandle((s_sis_thread_id_t)thread_->thread_id);
}

void sis_thread_clear(s_sis_thread *thread_)
{
}

s_sis_thread_id_t sis_thread_self()
{
	return (s_sis_thread_id_t)GetCurrentThreadId();
}
unsigned int sis_thread_handle(s_sis_thread_id_t id_) 
{
	return (unsigned int)id_;
}
void sis_thread_kill(s_sis_thread_id_t thread)
{
	LPDWORD code = NULL;
	if(GetExitCodeThread(thread, code))
	{
		ExitThread(*code);
	}
}
/////////////////////////////////////
// s_sis_mutex_t
/////////////////////////////////////
// #define sis_thread_mutex_destroy(m) DeleteCriticalSection(m)
// #define sis_thread_mutex_init(m,v) InitializeCriticalSection(m)
// #define sis_thread_mutex_lock(m) EnterCriticalSection(m)
// #define sis_thread_mutex_unlock(m) LeaveCriticalSection(m)
 
int sis_mutex_create(s_sis_mutex_t *m)
{
	// assert(m);
	InitializeCriticalSection(m);
	return 0;
}
void sis_mutex_destroy(s_sis_mutex_t *m)
{
	// assert(m);
	DeleteCriticalSection(m);
}

void sis_mutex_lock(s_sis_mutex_t *m)
{
	// assert(m);
	EnterCriticalSection(m);
}
void sis_mutex_unlock(s_sis_mutex_t *m)
{
	// assert(m);
	LeaveCriticalSection(m);
}
int sis_mutex_init(s_sis_mutex_t *m, void *n)
{
	InitializeCriticalSection(m);
	return 0;
}
int  sis_mutex_trylock(s_sis_mutex_t *m)
{
	if(TryEnterCriticalSection(m))
	{
		return 0;
	}
	return 1;
}
/////////////////////////////////////
// s_sis_rwlock_t
/////////////////////////////////////

int  sis_rwlock_init(s_sis_rwlock_t *rwlock_)
{
    rwlock_->m_mutex = CreateMutex(NULL, FALSE, NULL);  
    if (rwlock_->m_mutex == NULL)  
	{
		return -1;
	}
  
    rwlock_->m_readEvent = CreateEvent(NULL, TRUE, TRUE, NULL);  
    if (rwlock_->m_readEvent == NULL)  
	{
		CloseHandle(rwlock_->m_mutex);  
		return -2;
	}
  
    rwlock_->m_writeEvent = CreateEvent(NULL, TRUE, TRUE, NULL);  
    if (rwlock_->m_writeEvent == NULL)
	{
		CloseHandle(rwlock_->m_mutex);  
		CloseHandle(rwlock_->m_readEvent);  
		return -2;
	}  
    return 0;

}
void sis_rwlock_destroy(s_sis_rwlock_t *rwlock_)
{
	CloseHandle(rwlock_->m_mutex);  
	CloseHandle(rwlock_->m_readEvent); 
	CloseHandle(rwlock_->m_writeEvent); 
}

void _rwlock_add_writer(s_sis_rwlock_t *rwlock_)
{
    switch (WaitForSingleObject(rwlock_->m_mutex, INFINITE))  
    {  
    case WAIT_OBJECT_0:  
        if (++rwlock_->m_writersWaiting == 1)  
		{
			ResetEvent(rwlock_->m_readEvent);  
		}             
        ReleaseMutex(rwlock_->m_mutex);  
        break;  
    default:  
        break;  
    }  	
}
void _rwlock_del_writer(s_sis_rwlock_t *rwlock_)
{
    switch (WaitForSingleObject(rwlock_->m_mutex, INFINITE))  
    {  
    case WAIT_OBJECT_0:  
        if (--rwlock_->m_writersWaiting == 0 && rwlock_->m_writers == 0)   
        {
			SetEvent(rwlock_->m_readEvent);  
		}    
        ReleaseMutex(rwlock_->m_mutex);  
        break;  
    default:  
        break;   
    }  	
}

void sis_rwlock_lock_r(s_sis_rwlock_t *rwlock_)
{
    HANDLE h[2];  
    h[0] = rwlock_->m_mutex;  
    h[1] = rwlock_->m_readEvent;  
    switch (WaitForMultipleObjects(2, h, TRUE, INFINITE))  
    {  
    case WAIT_OBJECT_0:  
    case WAIT_OBJECT_0 + 1:  
        ++rwlock_->m_readers;  
        ResetEvent(rwlock_->m_writeEvent);  
        ReleaseMutex(rwlock_->m_mutex);  
        assert(rwlock_->m_writers == 0);  
        break;  
    default:  
        // printf("cannot lock reader/writer lock.\n"); 
		break; 
    }  	
}
void sis_rwlock_lock_w(s_sis_rwlock_t *rwlock_)
{
    _rwlock_add_writer(rwlock_);  
    HANDLE h[2];  
    h[0] = rwlock_->m_mutex;  
    h[1] = rwlock_->m_writeEvent;  
    switch (WaitForMultipleObjects(2, h, TRUE, INFINITE))  
    {  
    case WAIT_OBJECT_0:  
    case WAIT_OBJECT_0 + 1:  
        --rwlock_->m_writersWaiting;  
        ++rwlock_->m_readers;  
        ++rwlock_->m_writers;  
        ResetEvent(rwlock_->m_readEvent);  
        ResetEvent(rwlock_->m_writeEvent);  
        ReleaseMutex(rwlock_->m_mutex);  
        assert(rwlock_->m_writers == 1);  
        break;  
    default:  
        _rwlock_del_writer(rwlock_);  
        // cout<<"cannot lock reader/writer lock"<<endl;  
		break; 
    }  	
}
void sis_rwlock_unlock(s_sis_rwlock_t *rwlock_)
{
    switch (WaitForSingleObject(rwlock_->m_mutex, INFINITE))  
    {  
    case WAIT_OBJECT_0:  
        rwlock_->m_writers = 0;  
        if (rwlock_->m_writersWaiting == 0) SetEvent(rwlock_->m_readEvent);  
        if (--rwlock_->m_readers == 0) SetEvent(rwlock_->m_writeEvent);  
        ReleaseMutex(rwlock_->m_mutex);  
        break;  
    default:  
        // cout<<"cannot unlock reader/writer lock"<<endl;  
		break; 
    }  	
}
DWORD _rwlock_trylock_r_once(s_sis_rwlock_t *rwlock_)
{  
    HANDLE h[2];  
    h[0] = rwlock_->m_mutex;  
    h[1] = rwlock_->m_readEvent;  
    DWORD result = WaitForMultipleObjects(2, h, TRUE, 1);   
    switch (result)  
    {  
    case WAIT_OBJECT_0:  
    case WAIT_OBJECT_0 + 1:  
        ++rwlock_->m_readers;  
        ResetEvent(rwlock_->m_writeEvent);  
        ReleaseMutex(rwlock_->m_mutex);  
        assert(rwlock_->m_writers == 0);  
        return result;  
    case WAIT_TIMEOUT:  
    default:  
        break;  
    }  
    return result;  
}  
// 返回 0 表示锁成功
int  sis_rwlock_trylock_r(s_sis_rwlock_t *rwlock_)
{
    for (;;)  
    {  
        if (rwlock_->m_writers != 0 || rwlock_->m_writersWaiting != 0)  
            return 1;  
  
        DWORD result = _rwlock_trylock_r_once(rwlock_);  
        switch (result)  
        {  
        case WAIT_OBJECT_0:  
        case WAIT_OBJECT_0 + 1:  
            return 0;  
        case WAIT_TIMEOUT:  
            continue;  
        default:  
            break;  
        }  
    }  	
}
int  sis_rwlock_trylock_w(s_sis_rwlock_t *rwlock_)
{
    _rwlock_add_writer(rwlock_);  
    HANDLE h[2];  
    h[0] = rwlock_->m_mutex;  
    h[1] = rwlock_->m_writeEvent;  
    switch (WaitForMultipleObjects(2, h, TRUE, 1))  
    {  
    case WAIT_OBJECT_0:  
    case WAIT_OBJECT_0 + 1:  
        --rwlock_->m_writersWaiting;  
        ++rwlock_->m_readers;  
        ++rwlock_->m_writers;  
        ResetEvent(rwlock_->m_readEvent);  
        ResetEvent(rwlock_->m_writeEvent);  
        ReleaseMutex(rwlock_->m_mutex);  
        assert(rwlock_->m_writers == 1);  
        return true;  
    case WAIT_TIMEOUT:  
        _rwlock_del_writer(rwlock_);  
    default:  
        _rwlock_del_writer(rwlock_);  
        // cout<<"cannot lock reader/writer lock"<<endl;  
		break; 
    }  
    return false;  	
}
/////////////////////////////////////
//  s_sis_wait
/////////////////////////////////////
void sis_thread_wait_create(s_sis_wait *wait_)
{
	// wait_->event = CreateSemaphoreA (NULL, 0, 0xFFFFFFFF, NULL);
    // 0xFFFFFFFF 似乎被认为是-1
    wait_->event = CreateSemaphoreA (NULL, 0, 0xFFFFFF, NULL);
	wait_->mutex = CreateMutexA (NULL, FALSE, NULL);
}

void sis_thread_wait_destroy(s_sis_wait *wait_)
{
	CloseHandle(wait_->event);
    CloseHandle(wait_->mutex);
}
void sis_thread_wait_init(s_sis_wait *wait_)
{
	wait_->used = true;
	wait_->status = 0;
	wait_->count = 0;
}
void sis_thread_wait_kill(s_sis_wait *wait_)
{
	if (wait_->count > 0)
	{
        ReleaseSemaphore (wait_->event, wait_->count, NULL);
	}
}
void sis_thread_wait_notice(s_sis_wait *wait_)
{
	if (wait_->count > 0)
	{
        ReleaseSemaphore (wait_->event, 1, NULL);
	}
}
int sis_thread_wait_sleep(s_sis_wait *wait_, int delay_) // 秒
{
	wait_->count++;
	int o = SignalObjectAndWait(wait_->mutex, wait_->event, delay_ * 1000, FALSE);
	WaitForSingleObject (wait_->mutex, INFINITE);
	wait_->count--;
	return o == WAIT_OBJECT_0 ? 0 : SIS_ETIMEDOUT;
	// 返回 SIS_ETIMEDOUT 就正常处理
}
int sis_thread_wait_sleep_msec(s_sis_wait *wait_, int msec_) // 毫秒
{
	wait_->count++;
	int o = SignalObjectAndWait(wait_->mutex, wait_->event, msec_, FALSE);
	WaitForSingleObject(wait_->mutex, INFINITE);
	wait_->count--;
	return o == WAIT_OBJECT_0 ? 0 : SIS_ETIMEDOUT;
	// 返回 SIS_ETIMEDOUT 就正常处理
}
void sis_thread_wait_start(s_sis_wait *wait_)
{
	WaitForSingleObject (wait_->mutex, INFINITE);
}
void sis_thread_wait_stop(s_sis_wait *wait_)
{
    ReleaseMutex (wait_->mutex);
}
