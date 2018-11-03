
#include <os_thread.h>

bool sis_thread_create(SIS_THREAD_START_ROUTINE func_, void* val_, s_sis_thread_id_t *thread_)
{
	s_sis_thread_id_t result = 0;
	pthread_attr_t attr;
	int irc;
	irc = pthread_attr_init(&attr); 
	if(irc) { return false; }
	irc = pthread_attr_setstacksize(&attr, 1024 * 64);//测试2008-07-15
	if(irc) { return false; }
	irc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if(irc) { return false; }
	irc = pthread_create(&result, &attr, func_, val_);
	if(irc) { return false; }
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
			sis_sleep(50);
			continue;
		}
		assert(mutex_->reads_i >= 0);
		++mutex_->reads_i;
		sis_mutex_unlock(&mutex_->mutex_s);
		break;
	}
}
void sis_mutex_rw_unlock_r(s_sis_mutex_rw *mutex_)
{
	assert(mutex_);
	sis_mutex_lock(&mutex_->mutex_s);
	--mutex_->reads_i;
	assert(mutex_->reads_i >= 0);
	sis_mutex_unlock(&mutex_->mutex_s);
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
			sis_sleep(50);
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

  
void  sis_thread_wait_start(s_sis_wait *wait_) 
{
	sis_mutex_lock(&wait_->mutex); 	
}
void  sis_thread_wait_stop(s_sis_wait *wait_) 
{
	sis_mutex_unlock(&wait_->mutex); 	
}
int  sis_thread_wait_sleep(s_sis_wait *wait_, int delay_) // 秒
{
	struct timeval tv;
	struct timespec ts; 
	set_time_get_day(&tv, NULL);
	ts.tv_sec = tv.tv_sec + delay_;  
    ts.tv_nsec = tv.tv_usec * 1000;  

	return pthread_cond_timedwait(&wait_->cond, &wait_->mutex, &ts);  
	// 返回 ETIMEDOUT 就正常处理
}

void  sis_thread_wait_create(s_sis_wait *wait_)
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

int __kill = 0;
s_sis_wait __thread_wait; //线程内部延时处理

void *__task_ta(void *argv_)
{

    sis_thread_wait_start(&__thread_wait);
    while (!__kill)
    {
        if(sis_thread_wait_sleep(&__thread_wait, 1) == SIS_ETIMEDOUT)
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
			sis_sleep(3000);
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
}

int main()
{
	// s_sis_mutex_rw save_mutex;
	sis_thread_wait_create(&__thread_wait);

	s_sis_thread_id_t ta ;
	sis_thread_create(__task_ta, NULL, &ta);
	printf("thread a ok!\n");

	s_sis_thread_id_t tb ;
	sis_thread_create(__task_tb, NULL, &tb);
	printf("thread b ok!\n");

    // sis_mutex_rw_create(&save_mutex);
	signal(SIGINT,exithandle);

	while(!__kill)
	{
		sis_sleep(300);
	}
//    sis_thread_wait_kill(&__thread_wait);
    //???这里要好好测试一下，看看两个能不能一起退出来
    // sis_thread_join(server.db->init_pid);
    
    // sis_thread_join(ta);
	// printf("thread a end!\n");

    // sis_thread_join(tb);
	// printf("thread b end!\n");

    // sis_mutex_rw_destroy(&save_mutex);
	// printf("ok . \n");
    // sis_thread_wait_destroy(&__thread_wait);
	printf("end . \n");
	return 1;
}
#endif
