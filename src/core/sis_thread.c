
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

void sis_wait_start(s_sis_wait_handle *handle_)
{
	*handle_ = sis_wait_malloc();
	s_sis_wait *wait = sis_wait_get(*handle_);
	sis_thread_wait_start(wait);
    while (!wait->status)
    {
		// SIGNAL_EXIT_FAST		
        if (sis_thread_wait_sleep_msec(wait, 5000) != SIS_ETIMEDOUT)
        {
			printf("exit %d\n",wait->status);
			break;
        }
		else
		{
			printf("wait %d %d\n",wait->status, sis_get_signal());
		}
    }	
	sis_thread_wait_stop(wait);
	sis_wait_free(*handle_);
	*handle_ = -1;
}
void sis_wait_stop(s_sis_wait_handle handle_)
{
	s_sis_wait *wait = sis_wait_get(handle_);
	if(wait)
	{
		wait->status = 1;
		sis_thread_wait_notice(wait);
	}
}
// void sis_wait_stop(s_sis_wait_handle *handle_)
// {
// 	s_sis_wait *wait = NULL;
// 	while(!wait)
// 	{
// 		wait = sis_wait_get(*handle_);
// 		wait->status = 1;
// 		sis_thread_wait_notice(wait);
// 	}
// }

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
// 线程任务定义
/////////////////////////
bool sis_service_thread_working(s_sis_service_thread *task_)
{
	return task_->working;
}

bool sis_service_thread_execute(s_sis_service_thread *task_)
{
	s_sis_wait *wait = sis_wait_get(task_->wait_delay);
	if (task_->work_mode == SIS_WORK_MODE_NONE)
	{
		return false;
	}
	if (task_->work_mode == SIS_WORK_MODE_ONCE)
	{
		task_->working = false;
		return true;
	}
	else if (task_->work_mode == SIS_WORK_MODE_NOTICE)
	{
		int o = sis_thread_wait_sleep(wait, 45);
		// printf("notice start... %d %d\n", SIS_ETIMEDOUT, o);
		if (o != SIS_ETIMEDOUT)
		{
			// only notice return true.
			if (task_->working)
			{
				// printf("notice work...\n");
				return true;
			}
		}
	}
	else if (task_->work_mode == SIS_WORK_MODE_GAPS)
	{
		if (!task_->isfirst)
		{
			task_->isfirst = true;
			return true;
		}	
		// printf("gap = [%d, %d , %d]\n", task_->work_gap.start ,task_->work_gap.stop, task_->work_gap.delay);
		if (sis_thread_wait_sleep_msec(wait, task_->work_gap.delay) == SIS_ETIMEDOUT)
		{
			// printf("delay 1 = %d\n", task_->work_gap.delay);
			if (task_->work_gap.start == task_->work_gap.stop)
			{
				return true;
			}
			else
			{
				int min = sis_time_get_iminute(0);
				if ((task_->work_gap.start < task_->work_gap.stop && min > task_->work_gap.start && min < task_->work_gap.stop) ||
					(task_->work_gap.start > task_->work_gap.stop && (min > task_->work_gap.start || min < task_->work_gap.stop)))
				{
					return true;
				}
			}
			// printf("delay = %d\n", task_->work_gap.delay);
		}
	}
	else // SIS_WORK_MODE_PLANS
	{
		if (!task_->isfirst)
		{
			task_->isfirst = true;
			return true;
		}		
		else if (sis_thread_wait_sleep(wait, 45) == SIS_ETIMEDOUT) 
		{
			int min = sis_time_get_iminute(0);
			// printf("save plan ... -- -- -- %d \n", min);
			for (int k = 0; k < task_->work_plans->count; k++)
			{
				uint16 *lm = sis_struct_list_get(task_->work_plans, k);
				if (min == *lm)
				{
					return true;
				}
			}
		}
	}
	return false;
}
s_sis_service_thread *sis_service_thread_create()
{
	s_sis_service_thread *task = sis_malloc(sizeof(s_sis_service_thread));
	memset(task, 0, sizeof(s_sis_service_thread));

	task->work_plans = sis_struct_list_create(sizeof(uint16));

	task->wait_delay = sis_wait_malloc();

	sis_mutex_create(&task->mutex);

	return task;
}
void sis_service_thread_wait_start(s_sis_service_thread *task_)
{
	s_sis_wait *wait = sis_wait_get(task_->wait_delay);
	sis_thread_wait_start(wait);
}
void sis_service_thread_wait_notice(s_sis_service_thread *task_)
{
	s_sis_wait *wait = sis_wait_get(task_->wait_delay);
	sis_thread_wait_notice(wait);
}
void sis_service_thread_wait_stop(s_sis_service_thread *task_)
{
	s_sis_wait *wait = sis_wait_get(task_->wait_delay);
	sis_thread_wait_stop(wait);
	sis_thread_finish(&task_->work_thread);
}
bool sis_service_thread_start(s_sis_service_thread *task_, SIS_THREAD_START_ROUTINE func_, void *val_)
{
	if (!sis_thread_create(func_, val_, &task_->work_thread))
	{
		// LOG(1)("can't link task thread.\n");
		return false;
	}
	task_->working = true;
	return true;
}
void sis_service_thread_destroy(s_sis_service_thread *task_)
{
	if(task_->working)
	{
		task_->working = false;
		
		s_sis_wait *wait = sis_wait_get(task_->wait_delay);

		sis_thread_wait_kill(wait);
		if (task_->work_thread.thread_id)
		{
			sis_thread_join(&task_->work_thread);
		}
	}
	sis_wait_free(task_->wait_delay);
	sis_mutex_destroy(&task_->mutex);

	sis_struct_list_destroy(task_->work_plans);

	sis_free(task_);
	LOG(5)("plan_task end.\n");
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
		sis_wait_stop(__wait);
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
#if 0
#include <signal.h>
#include <stdio.h>

s_sis_wait __thread_wait__; 
s_sis_wait_handle __wait;
int __count = 0;

int sis_thread_wait_sleep1(s_sis_wait *wait_, int delay_) // 秒
{
	struct timeval tv;
	struct timespec ts;
	sis_time_get_day(&tv, NULL);
	ts.tv_sec = tv.tv_sec + delay_;
	ts.tv_nsec = tv.tv_usec * 1000;
	return pthread_cond_timedwait(&wait_->cond, &wait_->mutex, &ts);
	// 返回 SIS_ETIMEDOUT 就正常处理
}

void *_plan_task_example(void *argv_)
{
	s_sis_service_thread *task = (s_sis_service_thread *)argv_;

	// sis_service_thread_wait_start(task);
	// sis_thread_wait_create(&task->wait);
	// sis_thread_wait_start(&task->wait);
	sis_thread_wait_create(&__thread_wait__);
	sis_thread_wait_start(&__thread_wait__);
	// sis_out_binary("alone: ",(const char *)&__thread_wait__,sizeof(s_sis_wait));

	// sis_thread_wait_create(&test_wait->wait);
	// sis_thread_wait_start(&test_wait->wait);
	// sis_out_binary("new: ",&test_wait->wait,sizeof(s_sis_wait));

	s_sis_wait *wait = sis_wait_get(__wait);
	sis_mutex_lock(&wait->mutex);

	while (sis_service_thread_working(task))
	{
		printf(" test ..1.. %p \n", task);
		sis_sleep(1000);
		// if(sis_thread_wait_sleep(&task->wait, 3) == SIS_ETIMEDOUT)
		// if(sis_thread_wait_sleep(&task->wait, 3) == SIS_ETIMEDOUT)
		// if(sis_thread_wait_sleep_msec(&task->wait, 1000) == SIS_ETIMEDOUT)
		// if(sis_thread_wait_sleep_msec(&__thread_wait__, 1300) == SIS_ETIMEDOUT)
		if(sis_thread_wait_sleep1(wait, 1) == SIS_ETIMEDOUT)
		{
			__count++;
			printf(" test ... [%d]\n", __count);
		}
		else
		{
			printf(" no timeout ... \n");
		}
		
		// if (sis_service_thread_execute(task))
		{
			// printf(" run ... \n");
		}
	}
	printf("end . \n");
	// sis_service_thread_wait_stop(task);
	// sis_thread_wait_stop(&task->wait);
	// sis_thread_wait_stop(&__thread_wait__);
	// sis_thread_wait_stop(&test_wait->wait);
	sis_mutex_unlock(&wait->mutex);
	return NULL;
}

int __exit = 0;
s_sis_service_thread *__plan_task;

void exithandle(int sig)
{
	printf("exit .1. \n");
	sis_service_thread_destroy(__plan_task);
	printf("exit .ok . \n");
	__exit = 1;
}

int main()
{
	__wait = sis_wait_malloc();

	__plan_task = sis_service_thread_create();
	__plan_task->work_mode = SIS_WORK_MODE_GAPS;
	__plan_task->work_gap.start = 0;
	__plan_task->work_gap.stop = 2359;
	__plan_task->work_gap.delay = 5;

	printf(" test ..0.. %p wait = %lld\n", __plan_task, sizeof(__plan_task->wait));
	sis_service_thread_start(__plan_task, _plan_task_example, __plan_task);

	signal(SIGINT, exithandle);

	printf("working ... \n");
	while (!__exit)
	{
		sis_sleep(300);
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



