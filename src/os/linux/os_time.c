
#include "os_time.h"

void sis_time_getgm(struct tm *m, const time_t* t)
{
	gmtime_r(t, m);
}

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
	set_time_get_day(&tv, &tz);
	return ((unsigned long long)tv.tv_sec) * 1000 + tv.tv_usec / 1000;
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
	usleep(msec * 1000);
}
