
#include "lw_thread.h"
#include <assert.h>

bool thread_create(THREAD_START_ROUTINE func, void* var, unsigned long *thread)
{
#ifdef _MSC_VER
	uintptr_t result = 0;
	unsigned int id = 0;
	result = _beginthreadex(NULL, 0, &func, var, 0, &id);
	if (result == 0)
	{
		return false;
	}
	else
	{
		CloseHandle((HANDLE)result);
	}
#else
	pthread_t result = 0;
	pthread_attr_t attr;
	int irc;
	irc = pthread_attr_init(&attr); 
	if(irc) { return false; }
	irc = pthread_attr_setstacksize(&attr, 1024 * 64);//测试2008-07-15
	if(irc) { return false; }
	irc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if(irc) { return false; }
	irc = pthread_create(&result, &attr, func, var);
	if(irc) { return false; }
	pthread_attr_destroy(&attr);
#endif
	*thread = (unsigned long)result;
	return true;
}

void thread_join(unsigned thread)
{

#ifdef _MSC_VER
	WaitForSingleObject((void*)thread, INFINITE);
	CloseHandle((HANDLE)thread);
#else
	pthread_join((pthread_t)thread, 0);
#endif
}

void thread_clear(unsigned thread)
{
#ifndef _MSC_VER
	pthread_t t = (pthread_t)thread;
	pthread_detach(t);
#endif

}

unsigned thread_self()
{
#ifdef _MSC_VER
	return (unsigned)GetCurrentThreadId();
#else
	return (unsigned)pthread_self();
#endif
}

/////////////////////////////////////
//
//////////////////////////////////////////
int mutex_init(pthread_mutex_t *m)
{
#ifdef _MSC_VER
	InitializeCriticalSection(m);
	return 0;
#else
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	// 设置允许递归加锁(默认不支持)
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
	return pthread_mutex_init(m, &attr);
#endif
}
void mutex_destroy(pthread_mutex_t *m)
{
	assert(m);
#ifdef _MSC_VER
	DeleteCriticalSection(m);
#else
	pthread_mutex_destroy(m);
#endif
}
void mutex_lock(pthread_mutex_t *m)
{
	assert(m);
	pthread_mutex_lock(m);
}
void mutex_unlock(pthread_mutex_t *m)
{
	assert(m);
	pthread_mutex_unlock(m);
}
////////////////////////
// 多读一写锁定义
////////////////////////
int mutex_rw_create(pthread_mutex_rw *m)
{
	int rtn = mutex_create(&m->mutex_s);
	m->try_write_b = false;
	m->reads_i = 0;
	m->writes_i = 0;
	return rtn;
}
void mutex_rw_destroy(pthread_mutex_rw *m)
{
	assert(m);
	mutex_destroy(&m->mutex_s);
}
void mutex_rw_lock_r(pthread_mutex_rw *m)
{
	assert(m);
	for (;;)
	{
		mutex_lock(&m->mutex_s);
		if (m->try_write_b || m->writes_i > 0)
		{
			mutex_unlock(&m->mutex_s);
#ifdef _MSC_VER
			Sleep(50);
#else
			usleep(50 * 1000);
#endif
			continue;
		}

		assert(m->reads_i >= 0);
		++m->reads_i;
		mutex_unlock(&m->mutex_s);
		break;
	}
}
void mutex_rw_unlock_r(pthread_mutex_rw *m)
{
	assert(m);
	mutex_lock(&m->mutex_s);
	--m->reads_i;
	assert(m->reads_i >= 0);
	mutex_unlock(&m->mutex_s);
}
void mutex_rw_lock_w(pthread_mutex_rw *m)
{
	for (;;)
	{
		mutex_lock(&m->mutex_s);
		m->try_write_b = true;
		if (m->reads_i > 0 || m->writes_i > 0)
		{
			mutex_unlock(&m->mutex_s);
#ifdef _MSC_VER
			Sleep(50);
#else
			usleep(50 * 1000);
#endif
			continue;
		}
		m->try_write_b = false;
		assert(m->writes_i >= 0);
		++m->writes_i;
		mutex_unlock(&m->mutex_s);
		break;
	}
}
void mutex_rw_unlock_w(pthread_mutex_rw *m)
{
	assert(m);
	mutex_lock(&m->mutex_s);
	--m->writes_i;
	assert(m->writes_i >= 0);
	mutex_unlock(&m->mutex_s);
}
