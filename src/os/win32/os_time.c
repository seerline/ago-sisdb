
#include "os_time.h"
#include "os_str.h"

int sis_time_get_day(struct timeval *tp, void *tzp)
{
	time_t clock;
	struct tm tm;
	SYSTEMTIME wtm;
	GetLocalTime(&wtm);
	tm.tm_year = wtm.wYear - 1900;
	tm.tm_mon = wtm.wMonth - 1;
	tm.tm_mday = wtm.wDay;
	tm.tm_hour = wtm.wHour;
	tm.tm_min = wtm.wMinute;
	tm.tm_sec = wtm.wSecond;
	tm.tm_isdst = -1;
	clock = mktime(&tm);
	tp->tv_sec = (long)clock;
	tp->tv_usec = wtm.wMilliseconds * 1000;
	return (0);
}

void sis_localtime(struct tm *m, const time_t* t)
{
	localtime_s(m, t);
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
	sis_time_get_day(&tv, NULL);
	return (((unsigned long long)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}

unsigned long long sis_time_get_now_usec()
{
	struct timeval tv;
	sis_time_get_day(&tv, NULL);
	return (((unsigned long long)tv.tv_sec) * 1000000 + tv.tv_usec);
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
	Sleep(msec);
}

void sis_time_format_now(char *out_, size_t olen)
{
	time_t now = sis_time_get_now(); 
	struct tm ptm = {0};
	sis_localtime(&ptm, &now);
	snprintf(out_, olen, "%04d%02d%02d-%02d%02d%02d", (ptm.tm_year + 1900) % 100, (ptm.tm_mon + 1), ptm.tm_mday, ptm.tm_hour, ptm.tm_min, ptm.tm_sec);
}

