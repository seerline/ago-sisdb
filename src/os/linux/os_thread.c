
#include <os_thread.h>

bool sts_thread_create(STS_THREAD_START_ROUTINE func_, void* val_, s_sts_thread_id_t *thread_)
{
	s_sts_thread_id_t result = 0;
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

void sts_thread_join(s_sts_thread_id_t thread_)
{
	pthread_join((s_sts_thread_id_t)thread_, 0);
}

void sts_thread_clear(s_sts_thread_id_t thread_)
{
	s_sts_thread_id_t t = (s_sts_thread_id_t)thread_;
	pthread_detach(t);
}

s_sts_thread_id_t sts_thread_self()
{
	return (unsigned)pthread_self();
}

/////////////////////////////////////
//
//////////////////////////////////////////
int sts_mutex_create(s_sts_mutex_t *mutex_)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	// 设置允许递归加锁(默认不支持)
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
	return pthread_mutex_init(mutex_, &attr);
}

////////////////////////
// 多读一写锁定义
////////////////////////
int sts_mutex_rw_create(s_sts_mutex_rw *mutex_)
{
	int o = sts_mutex_create(&mutex_->mutex_s);
	mutex_->try_write_b = false;
	mutex_->reads_i = 0;
	mutex_->writes_i = 0;
	return o;
}
void sts_mutex_rw_destroy(s_sts_mutex_rw *mutex_)
{
	assert(mutex_);
	sts_mutex_destroy(&mutex_->mutex_s);
}
void sts_mutex_rw_lock_r(s_sts_mutex_rw *mutex_)
{
	assert(mutex_);
	for (;;)
	{
		sts_mutex_lock(&mutex_->mutex_s);
		if (mutex_->try_write_b || mutex_->writes_i > 0)
		{
			sts_mutex_unlock(&mutex_->mutex_s);
			sts_sleep(50);
			continue;
		}
		assert(mutex_->reads_i >= 0);
		++mutex_->reads_i;
		sts_mutex_unlock(&mutex_->mutex_s);
		break;
	}
}
void sts_mutex_rw_unlock_r(s_sts_mutex_rw *mutex_)
{
	assert(mutex_);
	sts_mutex_lock(&mutex_->mutex_s);
	--mutex_->reads_i;
	assert(mutex_->reads_i >= 0);
	sts_mutex_unlock(&mutex_->mutex_s);
}
void sts_mutex_rw_lock_w(s_sts_mutex_rw *mutex_)
{
	for (;;)
	{
		sts_mutex_lock(&mutex_->mutex_s);
		mutex_->try_write_b = true;
		if (mutex_->reads_i > 0 || mutex_->writes_i > 0)
		{
			sts_mutex_unlock(&mutex_->mutex_s);
			sts_sleep(50);
			continue;
		}
		mutex_->try_write_b = false;
		assert(mutex_->writes_i >= 0);
		++mutex_->writes_i;
		sts_mutex_unlock(&mutex_->mutex_s);
		break;
	}
}
void sts_mutex_rw_unlock_w(s_sts_mutex_rw *mutex_)
{
	assert(mutex_);
	sts_mutex_lock(&mutex_->mutex_s);
	--mutex_->writes_i;
	assert(mutex_->writes_i >= 0);
	sts_mutex_unlock(&mutex_->mutex_s);
}

  
void  sts_thread_wait_start(s_sts_wait *wait_) 
{
	sts_mutex_lock(&wait_->mutex); 	
}
void  sts_thread_wait_stop(s_sts_wait *wait_) 
{
	sts_mutex_unlock(&wait_->mutex); 	
}
int  sts_thread_wait_sleep(s_sts_wait *wait_, int delay_) // 秒
{
	struct timeval tv;
	struct timespec ts; 
	set_time_get_day(&tv, NULL);
	ts.tv_sec = tv.tv_sec + delay_;  
    ts.tv_nsec = tv.tv_usec * 1000;  

	return pthread_cond_timedwait(&wait_->cond, &wait_->mutex, &ts);  
	// 返回 ETIMEDOUT 就正常处理
}

void  sts_thread_wait_create(s_sts_wait *wait_)
{
    pthread_cond_init(&wait_->cond, NULL);  
    pthread_mutex_init(&wait_->mutex, NULL);  
	
}
void sts_thread_wait_kill(s_sts_wait *wait_)
{
    pthread_mutex_lock(&wait_->mutex);  
    // pthread_cond_signal(&wait_.cond);  
	pthread_cond_broadcast(&wait_->cond); 
    pthread_mutex_unlock(&wait_->mutex);  
}
void sts_thread_wait_destroy(s_sts_wait *wait_)
{
    pthread_cond_destroy(&wait_->cond);  
    pthread_mutex_destroy(&wait_->mutex);  
}

