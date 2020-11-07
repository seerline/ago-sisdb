
#include "os_thread.h"
#include <os_time.h>
#include <assert.h>

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
void sis_thread_kill(s_sis_thread_id_t thread)
{
	LPDWORD code = NULL;
	if(GetExitCodeThread(thread, code))
	{
		ExitThread(*code);
	}
}
/////////////////////////////////////
//
//////////////////////////////////////////
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
int  sis_mutex_trylock(s_sis_mutex_t *m, void *n)
{
	if(TryEnterCriticalSection(m))
	{
		return 0;
	}
	return 1;
}

/////////////////////////////////////
//  s_sis_wait
//////////////////////////////////////////

void sis_thread_wait_create(s_sis_wait *wait_)
{
	wait_->semaphore = CreateSemaphoreA (NULL, 0, 0xFFFFFFFF, NULL);
	sis_mutex_create(&wait_->mutex);
}

void sis_thread_wait_destroy(s_sis_wait *wait_)
{
	CloseHandle(wait_->semaphore);
	sis_mutex_destroy(&wait_->mutex);
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
        ReleaseSemaphore (wait_->semaphore, wait_->count, NULL);
	}
}
void sis_thread_wait_notice(s_sis_wait *wait_)
{
	if (wait_->count > 0)
	{
        ReleaseSemaphore (wait_->semaphore, 1, NULL);
	}
}
int sis_thread_wait_sleep(s_sis_wait *wait_, int delay_) // 秒
{
	wait_->count++;
	int o = SignalObjectAndWait(&wait_->mutex, wait_->semaphore, delay_ * 1000, FALSE);
	sis_mutex_lock(&wait_->mutex);
	wait_->count--;
	return o == WAIT_OBJECT_0 ? 0 : SIS_ETIMEDOUT;
	// 返回 SIS_ETIMEDOUT 就正常处理
}
int sis_thread_wait_sleep_msec(s_sis_wait *wait_, int msec_) // 毫秒
{
	wait_->count++;
	int o = SignalObjectAndWait(&wait_->mutex, wait_->semaphore, msec_, FALSE);
	sis_mutex_lock(&wait_->mutex);
	wait_->count--;
	return o == WAIT_OBJECT_0 ? 0 : SIS_ETIMEDOUT;
	// 返回 SIS_ETIMEDOUT 就正常处理
}
void sis_thread_wait_start(s_sis_wait *wait_)
{
	sis_mutex_lock(&wait_->mutex);
}
void sis_thread_wait_stop(s_sis_wait *wait_)
{
	sis_mutex_unlock(&wait_->mutex);
}
