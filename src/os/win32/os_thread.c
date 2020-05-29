
#include "os_thread.h"
#include <os_time.h>
#include <assert.h>

unsigned _thread_process(void* arg)
{
	s_sis_thread *thread = (s_sis_thread *)arg;
	// printf("thread start...\n");
	thread->worker(thread->argv);
	return 0;
}
bool sis_thread_create(SIS_THREAD_START_ROUTINE func, void* var, s_sis_thread *thread)
{
	uintptr_t result = 0;
	unsigned int id = 0;
	thread->worker = func;
	thread->argv = var;
	result = _beginthreadex(NULL, 0, &_thread_process, thread, 0, &id);
	if (result == 0)
	{
		return false;
	}
	thread->thread_id = (s_sis_thread_id_t)result;
	thread->working = true;
	return true;
}

void sis_thread_join(s_sis_thread_id_t thread)
{
	WaitForSingleObject((void*)thread, INFINITE);
	CloseHandle((s_sis_thread_id_t)thread);
}

void sis_thread_clear(s_sis_thread_id_t thread)
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
	InitializeCriticalSection(m);
	return 0;
}
void sis_mutex_destroy(s_sis_mutex_t *m)
{
	assert(m);
	DeleteCriticalSection(m);
}

void sis_mutex_lock(s_sis_mutex_t *m)
{
	assert(m);
	EnterCriticalSection(m);
}
void sis_mutex_unlock(s_sis_mutex_t *m)
{
	assert(m);
	LeaveCriticalSection(m);
}

/////////////////////////////////////
//
//////////////////////////////////////////
void  sis_thread_wait_start(s_sis_wait *wait_) 
{
	// sis_mutex_lock(&wait_->mutex); 	
}
void  sis_thread_wait_stop(s_sis_wait *wait_) 
{
	// sis_mutex_unlock(&wait_->mutex); 	
}

int  sis_thread_wait_sleep(s_sis_wait *wait_, int delay_) // Ãë
{
	while (delay_&&!wait_->end)
	{
		// printf("%d\n",delay_);
		sis_sleep(1000);
		delay_--;
	}
	return SIS_ETIMEDOUT;
}

void  sis_thread_wait_create(s_sis_wait *wait_)
{
	wait_->end = false;
    // pthread_cond_init(&wait_->cond, NULL);  
    // sis_mutex_init(&wait_->mutex, NULL);  
	
}
void sis_thread_wait_kill(s_sis_wait *wait_)
{
	wait_->end = true;
    // sis_mutex_lock(&wait_->mutex);  
	// pthread_cond_broadcast(&wait_->cond); 
    // sis_mutex_unlock(&wait_->mutex);  
}
void sis_thread_wait_destroy(s_sis_wait *wait_)
{
    // pthread_cond_destroy(&wait_->cond);  
    // sis_mutex_destroy(&wait_->mutex);  
}