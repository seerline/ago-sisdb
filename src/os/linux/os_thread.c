
#include <os_thread.h>
#include <os_malloc.h>

// int _now_cpu_index = 0;

// int sis_thread_auto_cpu(s_sis_thread *thread_)  
// {  
// 	int cpus = sysconf(_SC_NPROCESSORS_CONF);
//     cpu_set_t mask;  
//     CPU_ZERO(&mask);  
//     CPU_SET(_now_cpu_index, &mask);  
// 	_now_cpu_index = (_now_cpu_index + 1) % cpus;
  
//     // printf("thread %u, i = %d\n", pthread_self(), i);  
//     if(-1 == pthread_setaffinity_np(thread_->thread_id ,sizeof(cpu_set_t), &mask))  
//     {  
//         printf("pthread_setaffinity_np fail.\n");  
//         return -1;  
//     }  
//     return 0;  
// }  
 
bool sis_thread_create(SIS_THREAD_START_ROUTINE func_, void* val_, s_sis_thread *thread_)
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
	// 如果不用此方式，会造成条件变量广播丢失问题
	// irc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE); // join 阻塞等
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
	thread_->argv = val_;
	thread_->worker = func_;
	thread_->thread_id = result;
	thread_->working = true;
	// sis_thread_auto_cpu(thread_);
	return true;	
}
void sis_thread_finish(s_sis_thread *thread_)
{
	if (thread_)
	{
		thread_->working = false;
	}
}

// 等待线程结束
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
	pthread_join(thread_->thread_id, 0);
}

void sis_thread_clear(s_sis_thread *thread_)
{
	pthread_detach(thread_->thread_id);
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
	
	// pthread_mutexattr_settype(&attr, SIS_PTHREAD_MUTEX_ERRORCHECK);
	// 设置允许递归加锁(默认不支持)
	// 下面的锁有时候会返回忙状态
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
#ifndef __XRELEASE__
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
int sis_thread_wait_sleep_msec(s_sis_wait *wait_, int msec_) // 毫秒
{
	struct timeval tv;   // 微秒
	struct timespec ts;  // 纳秒
	sis_time_get_day(&tv, NULL);
	ts.tv_sec = tv.tv_sec + (int)(msec_ / 1000);
	int msec = tv.tv_usec + (msec_ % 1000) * 1000;
	if (msec >= 1000000)
	{
		ts.tv_sec ++ ;
		ts.tv_nsec = (msec - 1000000) * 1000;
	}
	else
	{
		ts.tv_nsec = msec * 1000;
	}
	// printf("offset %ld=%ld %ld=%ld\n",ts.tv_sec,tv.tv_sec, ts.tv_nsec, tv.tv_usec);
	return pthread_cond_timedwait(&wait_->cond, &wait_->mutex, &ts);
	// 返回 SIS_ETIMEDOUT 就正常处理
}
#else

int sis_thread_wait_sleep(s_sis_wait *wait_, int delay_) // 秒
{
	delay_ = 3;
	while (delay_)
	{
		// printf("%d\n",delay_);
		sis_sleep(1000);
		delay_--;
	}
	return SIS_ETIMEDOUT;
}
int sis_thread_wait_sleep_msec(s_sis_wait *wait_, int msec_) // 毫秒
{
	msec_ = 10;
	while (msec_)
	{
		// printf("%d\n",msec_);
		sis_sleep(1);
		msec_--;
	}
	return SIS_ETIMEDOUT;
}
#endif

void sis_thread_wait_create(s_sis_wait *wait_)
{
	pthread_cond_init(&wait_->cond, NULL);
	pthread_mutex_init(&wait_->mutex, NULL);
	// pthread_mutexattr_t attr;
	// pthread_mutexattr_init(&attr);
	// pthread_mutexattr_settype(&attr, SIS_PTHREAD_MUTEX_TIMED_NP);
	// pthread_mutex_init(&wait_->mutex, &attr);
}
void sis_thread_wait_destroy(s_sis_wait *wait_)
{
	pthread_cond_destroy(&wait_->cond);
	pthread_mutex_destroy(&wait_->mutex);
}
void sis_thread_wait_init(s_sis_wait *wait_)
{
	wait_->used = true;
	wait_->status = 0;
}
void sis_thread_wait_notice(s_sis_wait *wait_)
{
	pthread_mutex_lock(&wait_->mutex);
	// pthread_cond_signal(&wait_->cond);
	pthread_cond_broadcast(&wait_->cond);
	pthread_mutex_unlock(&wait_->mutex);
}
void sis_thread_wait_kill(s_sis_wait *wait_)
{
	pthread_mutex_lock(&wait_->mutex);
	// pthread_cond_signal(&wait_.cond);
	pthread_cond_broadcast(&wait_->cond);
	pthread_mutex_unlock(&wait_->mutex);
}

#if 0

#include <signal.h>
#include <stdio.h>

int __exit = 0;
int __kill = 0;
s_sis_wait __thread_wait; //线程内部延时处理
s_sis_wait __thread_wait_b; //线程内部延时处理

s_sis_thread ta ;
s_sis_thread tb ;
void *__task_ta(void *argv_)
{
    sis_thread_wait_start(&__thread_wait);
    while (!__kill)
    {
		printf(" test ..a.. \n");
        if(sis_thread_wait_sleep(&__thread_wait, 3) == SIS_ETIMEDOUT)
        {
            printf("timeout ..a.. %d \n",__kill);
        } else 
		{
            printf("no timeout ..a.. %d \n",__kill);
		}
    }
	sis_thread_wait_stop(&__thread_wait);
	printf("a ok . \n");
	sis_sleep(5*1000);
	printf("a ok ...\n");
    ta.working = false;
	// pthread_detach(pthread_self());

    return NULL;
}
void *__task_tb(void *argv_)
{

    sis_thread_wait_start(&__thread_wait);
    while (!__kill)
    {
		printf(" test ..b.. \n");
        if(sis_thread_wait_sleep(&__thread_wait, 6) == SIS_ETIMEDOUT)
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
	printf("b ok . \n");
	sis_sleep(8*1000);
	printf("b ok ... \n");
	tb.working = false;
    return NULL;
}
void exithandle(int sig)
{
	__kill = 1;
    printf("sighup received kill=%d \n",__kill);

	printf("kill . \n");
    sis_thread_wait_kill(&__thread_wait);
	
	sis_thread_join(&ta);
	printf("a ok .... \n");
	sis_thread_join(&tb);
	printf("b ok .... \n");

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
	printf("end . \n");
	return 1;
}
#endif
