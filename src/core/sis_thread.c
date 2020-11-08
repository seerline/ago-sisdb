
#include "sis_thread.h"

#define SIS_MAX_WAIT  0xFFFF
static bool __sys_wait_init = false; 
static s_sis_wait __sys_wait_pool[SIS_MAX_WAIT];
// 定义一组静态信号
s_sis_wait_handle sis_wait_malloc()
{
	if (!__sys_wait_init)
	{
		for(int i = 0; i < SIS_MAX_WAIT; i++)
		{
			__sys_wait_pool[i].used = false;
			sis_thread_wait_create(&__sys_wait_pool[i]);
		}		
		__sys_wait_init = true;
	}
	for(int i = 0; i < SIS_MAX_WAIT; i++)
	{
		if (!__sys_wait_pool[i].used)
		{
			sis_thread_wait_init(&__sys_wait_pool[i]);
			return i;
		}
	}	
	return -1;
}
s_sis_wait *sis_wait_get(s_sis_wait_handle id_)
{
	if (id_ >= 0 && id_ < SIS_MAX_WAIT)
	{
		return &__sys_wait_pool[id_];
	}
	return NULL;
}
void sis_wait_free(s_sis_wait_handle id_)
{
	if (id_ >= 0 && id_ < SIS_MAX_WAIT)
	{
		__sys_wait_pool[id_].used = false;
	}
}

////////////////////////
// 多读一写锁定义
////////////////////////
int sis_mutex_rw_create(s_sis_mutex_rw *mutex_)
{
	int o = sis_mutex_create(&mutex_->mutex_s);
	mutex_->try_write_b = false;
	mutex_->reads_i = 0;
	mutex_->writes_i = 0;
	return o;
}
void sis_mutex_rw_destroy(s_sis_mutex_rw *mutex_)
{
	assert(mutex_);
	sis_mutex_destroy(&mutex_->mutex_s);
}
void sis_mutex_rw_lock_r(s_sis_mutex_rw *mutex_)
{
	assert(mutex_);
	for (;;)
	{
		sis_mutex_lock(&mutex_->mutex_s);
		if (mutex_->try_write_b || mutex_->writes_i > 0)
		{
			sis_mutex_unlock(&mutex_->mutex_s);
			sis_sleep(10);
			continue;
		}
		assert(mutex_->reads_i >= 0);
		++mutex_->reads_i;
		sis_mutex_unlock(&mutex_->mutex_s);
		break;
	}
}
// 返回 0 表示加锁成功
int sis_mutex_rw_try_lock_r(s_sis_mutex_rw *mutex_)
{
	assert(mutex_);
	sis_mutex_lock(&mutex_->mutex_s);
	if (mutex_->try_write_b || mutex_->writes_i > 0)
	{
		sis_mutex_unlock(&mutex_->mutex_s);
		return 1;
	}
	assert(mutex_->reads_i >= 0);
	++mutex_->reads_i;
	sis_mutex_unlock(&mutex_->mutex_s);
	return 0;
}

void sis_mutex_rw_unlock_r(s_sis_mutex_rw *mutex_)
{
	assert(mutex_);
	sis_mutex_lock(&mutex_->mutex_s);
	--mutex_->reads_i;
	assert(mutex_->reads_i >= 0);
	sis_mutex_unlock(&mutex_->mutex_s);
}
int sis_mutex_rw_try_lock_w(s_sis_mutex_rw *mutex_)
{
	sis_mutex_lock(&mutex_->mutex_s);
	mutex_->try_write_b = true;
	if (mutex_->reads_i > 0 || mutex_->writes_i > 0)
	{
		sis_mutex_unlock(&mutex_->mutex_s);
		return 1;
	}
	mutex_->try_write_b = false;
	assert(mutex_->writes_i >= 0);
	++mutex_->writes_i;
	sis_mutex_unlock(&mutex_->mutex_s);
	return 0;
}
void sis_mutex_rw_lock_w(s_sis_mutex_rw *mutex_)
{
	for (;;)
	{
		sis_mutex_lock(&mutex_->mutex_s);
		mutex_->try_write_b = true;
		if (mutex_->reads_i > 0 || mutex_->writes_i > 0)
		{
			sis_mutex_unlock(&mutex_->mutex_s);
			sis_sleep(10);
			continue;
		}
		mutex_->try_write_b = false;
		assert(mutex_->writes_i >= 0);
		++mutex_->writes_i;
		sis_mutex_unlock(&mutex_->mutex_s);
		break;
	}
}

void sis_mutex_rw_unlock_w(s_sis_mutex_rw *mutex_)
{
	assert(mutex_);
	sis_mutex_lock(&mutex_->mutex_s);
	--mutex_->writes_i;
	assert(mutex_->writes_i >= 0);
	sis_mutex_unlock(&mutex_->mutex_s);
}

////////////////////////
// 等待式线程定义
/////////////////////////
s_sis_wait_thread *sis_wait_thread_create(int waitmsec_)
{
	s_sis_wait_thread *o = SIS_MALLOC(s_sis_wait_thread, o);
	o->wait_notice = sis_wait_malloc();    
	o->work_wait = sis_wait_get(o->wait_notice);
    o->work_status = SIS_WAIT_STATUS_NONE;
	o->wait_msec = waitmsec_ < 10 ? 10 : waitmsec_;
	return o;
}
void sis_wait_thread_destroy(s_sis_wait_thread *swt_)
{
	if (swt_->work_status == SIS_WAIT_STATUS_WORK)
	{
		swt_->work_status = SIS_WAIT_STATUS_EXIT;
		while (swt_->work_status != SIS_WAIT_STATUS_NONE)
		{
			// 循环通知退出
			sis_thread_wait_notice(swt_->work_wait);
			sis_sleep(10);
		}
	}
    sis_wait_free(swt_->wait_notice);
	sis_free(swt_);
}
// 通知执行
void sis_wait_thread_notice(s_sis_wait_thread *swt_)
{
    sis_thread_wait_notice(swt_->work_wait);
}
bool sis_wait_thread_open(s_sis_wait_thread *swt_, cb_thread_working func_, void *source_)
{
    if (!sis_thread_create(func_, source_, &swt_->work_thread))
    {
        return false;
    }
	return true;
}
// 退出线程标志 但不会马上退出
void sis_wait_thread_close(s_sis_wait_thread *swt_)
{
    swt_->work_status = SIS_WAIT_STATUS_EXIT;
}

//////// 以下在线程中执行 ////// 
void sis_wait_thread_start(s_sis_wait_thread *swt_)
{
    swt_->work_status = SIS_WAIT_STATUS_WORK;
    sis_thread_wait_start(swt_->work_wait);
}
bool sis_wait_thread_noexit(s_sis_wait_thread *swt_)
{
	return (swt_->work_status != SIS_WAIT_STATUS_EXIT);
}
int sis_wait_thread_wait(s_sis_wait_thread *swt_, int waitmsec_)
{
	int waitmsec = waitmsec_ > 3 ? waitmsec_ : swt_->wait_msec;
	if (sis_thread_wait_sleep_msec(swt_->work_wait, waitmsec) == SIS_ETIMEDOUT)
	{
		return SIS_WAIT_TIMEOUT;
	}
	return SIS_WAIT_NOTICE;
}
void sis_wait_thread_stop(s_sis_wait_thread *swt_)
{
	sis_thread_wait_stop(swt_->work_wait);
	sis_thread_finish(&swt_->work_thread);
    swt_->work_status = SIS_WAIT_STATUS_NONE;
}

#if 0
#include <signal.h>
#include <stdio.h>

s_sis_wait_handle __wait = -1;

int __exit = 0;

void *_plan_task_example(void *argv_)
{
	while(!__exit)
	{
		sis_sleep(15000);
	}
	return NULL;
}

void exithandle(int sig)
{
	printf("exit .ok . \n");
	__exit = 1;
}

s_sis_thread ta ;

int main()
{
	sis_thread_create(_plan_task_example, NULL, &ta);

	signal(SIGINT, exithandle);

	int i = 0;
	while(!__exit)
	{
		i++;
		printf("---- start %d\n", i);
		sis_wait_start(&__wait);
		printf("----- stop %d\n", i);
	}
}
#endif

// test mutex
#if 0

#include <signal.h>
#include <stdio.h>

int __exit = 0;
int __kill = 0;

s_sis_thread ta ;
s_sis_thread tb ;

s_sis_mutex_rw mutex;

void *__task_ta(void *argv_)
{
    while (!__kill)
    {
		sis_mutex_rw_lock_r(&mutex);
		printf(" in ..a.. \n");
		sis_sleep(1*1000);
		printf(" out ..a.. \n");
		sis_mutex_rw_unlock_r(&mutex);
		sis_sleep(100);
    }
	printf("a ok . \n");
    return NULL;
}
void *__task_tb(void *argv_)
{
    while (!__kill)
    {
		sis_mutex_rw_lock_w(&mutex);
		printf(" in ..b.. \n");
		sis_sleep(5*1000);
		printf(" out ..b.. \n");
		sis_mutex_rw_unlock_w(&mutex);
		sis_sleep(100);
    }
	printf("b ok . \n");
    return NULL;
}
void exithandle(int sig)
{
	__kill = 1;
    printf("sighup received kill=%d \n",__kill);

	sis_thread_join(&ta);
	printf("a ok .... \n");
	sis_thread_join(&tb);
	printf("b ok .... \n");

	printf("ok . \n");


	// printf("kill b . \n");
    // sis_thread_wait_kill(&__thread_wait_b);
	// printf("free b . \n");
    // sis_thread_wait_destroy(&__thread_wait_b);
	// printf("ok b . \n");


	__exit = 1;
}

int main()
{

	sis_mutex_rw_create(&mutex);

	sis_thread_create(__task_ta, NULL, &ta);
	printf("thread a ok!\n");
	sis_thread_create(__task_ta, NULL, &ta);
	printf("thread a ok!\n");

	// s_sis_thread_id_t tb ;
	sis_thread_create(__task_tb, NULL, &tb);
	printf("thread b ok!\n");

	signal(SIGINT,exithandle);

	while(!__exit)
	{
		sis_sleep(300);
	}

	sis_mutex_rw_destroy(&mutex);

	printf("end . \n");
	return 1;
}
#endif



