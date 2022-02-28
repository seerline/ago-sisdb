
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

/**
 * @brief 调用操作系统API创建线程
 * @param func_ 新线程运行时执行的函数
 * @param worker_ 新线程运行时传递给func_的参数
 * @param thread_ 用以保存结果的sis线程对象
 * @return true 成功
 * @return false 失败
 */
bool sis_thread_create(cb_thread_working func_, void* worker_, s_sis_thread *thread_)
{
	s_sis_thread_id_t thread_id = 0;//创建线程后操作系统返回的线程ID
	pthread_attr_t attr;//操作系统定义的线程相关属性
	
	// 初始化线程属性
	if(pthread_attr_init(&attr))
		return false;

	// 设置线程堆栈大小为64M	
	if(pthread_attr_setstacksize(&attr, 1024 * 64)) //测试2008-07-15
		return false;

	/** 设置线程状态为可分离
	 * 在任何一个时间点上，线程是可结合的（joinable），或者是分离的（detached）。一个可结合的线程能够被其他线程收回其资源和杀死；
	 * 在被其他线程回收之前，它的存储器资源（如栈）是不释放的。相反，一个分离的线程是不能被其他线程回收或杀死的，它的存储器资源在它
	 * 终止时由系统自动释放。
	 * 线程的分离状态决定一个线程以什么样的方式来终止自己。在默认情况下线程是非分离状态的，这种情况下，原有的线程等待创建的线程结束。
	 * 只有当pthread_join（）函数返回时，创建的线程才算终止，才能释放自己占用的系统资源。而分离线程不是这样子的，它没有被其他的线
	 * 程所等待，自己运行结束了，线程也就终止了，马上释放系统资源。
	 * 如果我们在创建线程时就知道不需要了解线程的终止状态，则可以pthread_attr_t结构中的detachstate线程属性，让线程以分离状态启动。
	*/	
	//QQQ  如果不用此方式，会造成条件变量广播丢失问题
	if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
		return false;
	// 如果不用此方式，会造成条件变量广播丢失问题
	// irc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE); // join 阻塞等
	if(pthread_create(&thread_id, &attr, func_, worker_))
		return false;

	//变量attr在其使用完毕后将其资源回收	
	pthread_attr_destroy(&attr);
	thread_->argv = worker_;
	thread_->worker = func_;
	thread_->thread_id = thread_id;
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
/**
 * @brief 获取互斥锁，如果锁被占用就挂起线程，直到锁被释放为止
 * @param wait_ 被操作的线程对象
 */
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
/** 满足以下条件之一，即可返回：
 * （1）等待超时delay_秒
 * （2）收到互斥体wait_->mutex的解锁信号
 * （3）wait_->cond条件满足
 */
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
/** 
 */

/**
 * @brief 满足以下条件之一，即可返回：
 * （1）等待超时msec_毫秒
 * （2）wait_->cond收到信号
 * @param wait_ 等待式线程对象
 * @param msec_ 超时时间
 * @return int 
 */
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
	//QQQ 这里为什么要先lock，是为了保证不丢信号么？
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
