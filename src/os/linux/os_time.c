
#include "os_time.h"


void sis_localtime(struct tm *m, const time_t* t)
{
	localtime_r(t, m);
}

time_t sis_time_get_now()
{
	time_t tt;
	time(&tt);
	return tt;
}

unsigned long long sis_time_get_now_msec()
{
	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv, &tz);
	return ((unsigned long long)tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

unsigned long long sis_time_get_now_usec()
{
	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv, &tz);
	return ((unsigned long long)tv.tv_sec) * 1000000 + tv.tv_usec;
}
void sis_time_check(time_t tt, struct tm *ptm)
{
	if (tt == 0)
	{
		time(&tt);
		sis_localtime(ptm, &tt);
	}
	else
	{
		sis_localtime(ptm, &tt);
	}
}

void sis_sleep(int msec)
{
	if (!msec)
	{
		return ;
	}
	usleep(msec * 1000);
}

void sis_time_format_now(char *out_, size_t olen)
{
	time_t now = sis_time_get_now(); 
	struct tm ptm = {0};
	sis_localtime(&ptm, &now);
	// strftime(out_, olen, "[%y-%m-%d %H:%M:%S]", &ptm);
	strftime(out_, olen, "%y%m%d-%H%M%S", &ptm);
}
