
#include <os_thread.h>
#include <os_malloc.h>

bool sis_thread_create(SIS_THREAD_START_ROUTINE func_, void *val_, s_sis_thread_id_t *thread_)
{
	s_sis_thread_id_t result = 0;
	pthread_attr_t attr;
	int irc;
	irc = pthread_attr_init(&attr);
	if (irc)
	{
		return false;
	}
	irc = pthread_attr_setstacksize(&attr, 1024 * 64); //测试2008-07-15
	if (irc)
	{
		return false;
	}
	irc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (irc)
	{
		return false;
	}
	irc = pthread_create(&result, &attr, func_, val_);
	if (irc)
	{
		return false;
	}
	pthread_attr_destroy(&attr);
	*thread_ = result;
	return true;
}

void sis_thread_join(s_sis_thread_id_t thread_)
{
	pthread_join((s_sis_thread_id_t)thread_, 0);
}

void sis_thread_clear(s_sis_thread_id_t thread_)
{
	s_sis_thread_id_t t = (s_sis_thread_id_t)thread_;
	pthread_detach(t);
}

s_sis_thread_id_t sis_thread_self()
{
	return (s_sis_thread_id_t)pthread_self();
}

/////////////////////////////////////
//
//////////////////////////////////////////
int sis_mutex_create(s_sis_mutex_t *mutex_)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	// 设置允许递归加锁(默认不支持)
	pthread_mutexattr_settype(&attr, SIS_PTHREAD_MUTEX_RECURSIVE);
	return pthread_mutex_init(mutex_, &attr);
}
/////////////////////////////////////
//
//////////////////////////////////////////
void sis_thread_wait_start(s_sis_wait *wait_)
{
	sis_mutex_lock(&wait_->mutex);
}
void sis_thread_wait_stop(s_sis_wait *wait_)
{
	sis_mutex_unlock(&wait_->mutex);
}
#ifdef __RELEASE__
// 要测试，暂时先这样，后期要检查问题
int sis_thread_wait_sleep(s_sis_wait *wait_, int delay_) // 秒
{
	struct timeval tv;
	struct timespec ts;
	sis_time_get_day(&tv, NULL);
	ts.tv_sec = tv.tv_sec + delay_;
	ts.tv_nsec = tv.tv_usec * 1000;

	return pthread_cond_timedwait(&wait_->cond, &wait_->mutex, &ts);
	// 返回 SIS_ETIMEDOUT 就正常处理
}
#else
// #include <stdio.h>
int sis_thread_wait_sleep(s_sis_wait *wait_, int delay_) // 秒
{
	while (delay_)
	{
		// printf("%d\n",delay_);
		sis_sleep(1000);
		delay_--;
	}
	return SIS_ETIMEDOUT;
}
#endif

void sis_thread_wait_create(s_sis_wait *wait_)
{
	pthread_cond_init(&wait_->cond, NULL);
	pthread_mutex_init(&wait_->mutex, NULL);
}
void sis_thread_wait_kill(s_sis_wait *wait_)
{
	pthread_mutex_lock(&wait_->mutex);
	// pthread_cond_signal(&wait_.cond);
	pthread_cond_broadcast(&wait_->cond);
	pthread_mutex_unlock(&wait_->mutex);
}
void sis_thread_wait_destroy(s_sis_wait *wait_)
{
	pthread_cond_destroy(&wait_->cond);
	pthread_mutex_destroy(&wait_->mutex);
}

#if 0

#include <signal.h>
#include <stdio.h>

int __exit = 0;
int __kill = 0;
s_sis_wait __thread_wait; //线程内部延时处理
s_sis_wait __thread_wait_b; //线程内部延时处理

void *__task_ta(void *argv_)
{

    sis_thread_wait_start(&__thread_wait);
    while (!__kill)
    {
        if(sis_thread_wait_sleep(&__thread_wait, 3) == SIS_ETIMEDOUT)
        {
            printf("timeout ..a.. %d \n",__kill);
        } else 
		{
            printf("no timeout ..a.. %d \n",__kill);
		}
    }
    sis_thread_wait_stop(&__thread_wait);
	// pthread_detach(pthread_self());
    return NULL;
}
void *__task_tb(void *argv_)
{

    sis_thread_wait_start(&__thread_wait);
    while (!__kill)
    {
        if(sis_thread_wait_sleep(&__thread_wait, 1) == SIS_ETIMEDOUT)
        {
			// sis_sleep(3000);
            printf("timeout ..b.. %d \n",__kill);
        } else 
		{
            printf("no timeout ..b.. %d \n",__kill);
		}
    }
    sis_thread_wait_stop(&__thread_wait);
	// pthread_detach(pthread_self());
    return NULL;
}
void exithandle(int sig)
{
	__kill = 1;
    printf("sighup received kill=%d \n",__kill);

	printf("kill . \n");
    sis_thread_wait_kill(&__thread_wait);
	printf("free . \n");
    sis_thread_wait_destroy(&__thread_wait);
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
	sis_thread_wait_create(&__thread_wait);
	sis_thread_wait_create(&__thread_wait_b);

	s_sis_thread_id_t ta ;
	sis_thread_create(__task_ta, NULL, &ta);
	printf("thread a ok!\n");

	s_sis_thread_id_t tb ;
	sis_thread_create(__task_tb, NULL, &tb);
	printf("thread b ok!\n");

	signal(SIGINT,exithandle);

	while(!__exit)
	{
		sis_sleep(300);
	}
	printf("end . \n");
	return 1;
}
#endif
