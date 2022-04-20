
#include <os_thread.h>
#include <os_malloc.h>

// 互斥锁

// 　　使用场景：

// 　　　　读写互斥、读互斥，io密集性不那么高的场景

// 　　获取互斥锁会阻塞，线程会休眠，可以获取锁时，系统会唤醒线程。存在线程的挂起和唤醒，所以效率上会有影响。

// 　　

// 　　c++11之前:

// 　　int pthread_mutex_init(pthread_mutex_t *restrict mutex,const pthread_mutexattr_t *restrict attr);

// 　　int pthread_mutex_destroy(pthread_mutex_t *mutex);

// 　　int pthread_mutex_lock(pthread_mutex_t *mutex);

// 　　int pthread_mutex_trylock(pthread_mutex_t *mutex);　　

// 　　int pthread_mutex_unlock(pthread_mutex_t *mutex);

 

// 　　c++11 提供了mutex的封装类，封装了之前互斥锁相关操作的api，使用更加方便：

// 　　

 

// 自旋锁

// 　　使用场景：

// 　　读写互斥，读互斥，io读写密集型高。

// 　　自旋锁不能获取到锁资源时不会休眠，会自旋转。耗cpu，但是不会有线程挂起和唤醒的过程。

// 　　

// 　　相关api:

// 　　int pthread_spin_init(pthread_spinlock_t *lock, int pshared);

// 　　int pthread_spin_destroy(pthread_spinlock_t *lock);

 

// 　　int pthread_spin_lock(pthread_spinlock_t *lock);

// 　　int pthread_spin_trylock(pthread_spinlock_t *lock);

// 　　int pthread_spin_unlock(pthread_spinlock_t *lock);

// 　　int pthread_spin_destroy(pthread_spinlock_t *lock);

// 　　

// 读写锁

// 　　使用场景：

// 　　读写互斥，读不互斥。

// 　　读写锁像自旋锁一样，在不能获取锁的时候 ，线程不会挂起，会自旋。

 

// 　　相关api:

// 　　int pthread_rwlock_init(pthread_rwlock_t *restrict rwlock,const pthread_rwlockattr_t *restrict attr);

// 　　int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);

 

// 　　int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);　　

// 　　int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);

// 　　int pthread_rwlock_timedrdlock(pthread_rwlock_t *restrict rwlock,const struct timespec *restrict abs_timeout);

 

// 　　int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);

// 　　int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);

// 　　int pthread_rwlock_timedwrlock(pthread_rwlock_t *restrict rwlock,const struct timespec *restrict abs_timeout);

// 　　

// 　　int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);

 

// 条件变量

// 　　互斥锁有一个明显的缺点：只有两种状态，锁定和非锁定

// 　　而条件变量则通过允许线程阻塞并等待另一线程发送唤醒信号的方法弥补互斥锁的不足，它常和互斥锁一起使用。　　

// 　　

// 　　相关api:

// 　　int pthread_cond_init(pthread_cond_t *restrict cond,const pthread_condattr_t *restrict attr);

// 　　int pthread_cond_destroy(pthread_cond_t *cond);

 

// 　　int pthread_cond_wait(pthread_cond_t *restrict cond,pthread_mutex_t *restrict mutex);

// 　　int pthread_cond_timedwait(pthread_cond_t *restrict cond,pthread_mutex_t *restrict mutex,const struct timespec *restrict abstime);

// 　　

// 　　int pthread_cond_signal(pthread_cond_t *cond);

// 　　int pthread_cond_broadcast(pthread_cond_t *cond);

// 　　

// 信号量

// 　　　　主要用途是线程调度、通过调度顺序来保证共享资源。互斥锁主要用途是锁共享资源/

// 　　　　int sem_init(sem_t *sem, int pshared, unsigned int value);

// 　　　　int sem_wait(sem_t *sem);

// 　　　　int sem_trywait(sem_t *sem);

// 　　　　int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout);

// 　　　　int sem_post(sem_t *sem);

// 　　　　int sem_destroy(sem_t *sem);　

// #include <unistd.h>
// #include <sched.h>

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
 
bool sis_thread_create(cb_thread_working func_, void* val_, s_sis_thread *thread_)
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
#ifdef __APPLE__
unsigned int sis_thread_handle(s_sis_thread_id_t id_) 
{
	return id_->__sig;
}
#else
unsigned int sis_thread_handle(s_sis_thread_id_t id_) 
{
	return (unsigned int)id_;
}
#endif
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

int  sis_rwlock_init(s_sis_rwlock_t *rwlock_)
{
	// pthread_rwlockattr_t attr;
	// pthread_rwlockattr_init(&attr);
	// pthread_rwlockattr_settype(&attr, SIS_PTHREAD_MUTEX_RECURSIVE);
	return pthread_rwlock_init(rwlock_, NULL);

}
#ifdef __APPLE__
int sis_mutex_wait_lock_r(s_sis_rwlock_t *rwlock_, int msec_)
{
	return 0;
}
int sis_mutex_wait_lock_w(s_sis_rwlock_t *rwlock_, int msec_)
{
	return 0;
}
#else
int sis_mutex_wait_lock_r(s_sis_rwlock_t *rwlock_, int msec_)
{
	struct timeval tv;
	struct timespec ts;
	sis_time_get_day(&tv, NULL);
	ts.tv_sec = tv.tv_sec + msec_;
	ts.tv_nsec = tv.tv_usec * 1000;
	return pthread_rwlock_timedrdlock(rwlock_, &ts);
}
int sis_mutex_wait_lock_w(s_sis_rwlock_t *rwlock_, int msec_)
{
	struct timeval tv;
	struct timespec ts;
	sis_time_get_day(&tv, NULL);
	ts.tv_sec = tv.tv_sec + msec_;
	ts.tv_nsec = tv.tv_usec * 1000;
	return pthread_rwlock_timedwrlock(rwlock_, &ts);
}
#endif
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
	// pthread_mutex_lock(&wait_->mutex);
	if (!pthread_mutex_trylock(&wait_->mutex)) // 保证已经处于等待状态 否则死锁
	{
		// pthread_cond_signal(&wait_->cond);
		pthread_cond_broadcast(&wait_->cond);
		pthread_mutex_unlock(&wait_->mutex);
	}
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
