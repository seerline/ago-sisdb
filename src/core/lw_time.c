
#include "lw_time.h"
#include <stdio.h>

#ifdef _MSC_VER
int gettimeofday(struct timeval *tp, void *tzp)
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
#endif

void dp_gmtime(struct tm *m, const time_t* t)
{
#ifdef _MSC_VER
	gmtime_s(m, t);
#else
	gmtime_r(t, m);
#endif
}

void dp_localtime(struct tm *m, const time_t* t)
{
#ifdef _MSC_VER
	localtime_s(m, t);
#else
	localtime_r(t, m);
#endif
}

time_t dp_nowtime()
{
	time_t tt;
	time(&tt);
	return tt;
}

unsigned long long dp_nowtime_msec()
{
#ifndef _MSC_VER
	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv, &tz);
	return ((unsigned long long)tv.tv_sec) * 1000 + tv.tv_usec / 1000;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (((unsigned long long)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
#endif
}

void check_time_t(time_t tt, struct tm *ptm) //2015
{
	if (tt == 0)
	{
		time(&tt);
		dp_localtime(ptm, &tt);
	}
	else
	{
		dp_localtime(ptm, &tt);
	}
}
int dp_get_iyear(time_t ttime) //2015
{
	struct tm ptm = { 0 };
	check_time_t(ttime, &ptm);
	return (ptm.tm_year + 1900);
}
int dp_get_imonth(time_t ttime) //201510
{
	struct tm ptm = { 0 };
	check_time_t(ttime, &ptm);
	return (ptm.tm_year + 1900) * 100 + (ptm.tm_mon + 1);
}

int dp_get_idate(time_t ttime) //20151012
{
	struct tm ptm = { 0 };
	check_time_t(ttime, &ptm);
	return (ptm.tm_year + 1900) * 10000 + (ptm.tm_mon + 1) * 100 + ptm.tm_mday;
}
int dp_get_itime(time_t ttime) //103020
{
	struct tm ptm = { 0 };
	check_time_t(ttime, &ptm);
	return ptm.tm_hour * 10000 + ptm.tm_min * 100 + ptm.tm_sec;
}
int dp_get_iminute(time_t ttime) //1030
{
	struct tm ptm = { 0 };
	check_time_t(ttime, &ptm);
	return ptm.tm_hour * 100 + ptm.tm_min;
}
int dp_get_isec(time_t ttime)
{
	struct tm ptm = { 0 };
	check_time_t(ttime, &ptm);
	return ptm.tm_sec;
}
int dp_get_showtime(time_t ttime)
{
	struct tm ptm = { 0 };
	check_time_t(ttime, &ptm);
	return (ptm.tm_mon + 1) * 10000 * 10000 + ptm.tm_mday * 100 * 10000 + ptm.tm_hour * 10000 + ptm.tm_min * 100 + ptm.tm_sec;

}
short dp_get_offset_iminute(short nMin, int offsec)
{
	time_t tt = dp_make_time(0, nMin * 100);
	tt += offsec;
	return (short)dp_get_iminute(tt);
}
time_t dp_get_offset_time(time_t curTime, int offsec)
{
	//time_t tt = dp_make_time(dp_get_idate(&curTime), dp_get_iminute(&curTime) * 100 + offsec);
	time_t tt = curTime + offsec;
	return tt;
}
int dp_get_iminute_offset_i(int begin, int end) //930-1130 
{
	int bh, eh, bm, em;
	int count = 0;
	bh = (int)(begin / 100);
	bm = (int)(begin % 100);
	eh = (int)(end / 100);
	em = (int)(end % 100);
	count = (eh * 60 + em) - (bh * 60 + bm);
	return count;
}
int dp_get_iminute_offset_time(time_t tstart, time_t tend)
{
	return (int)((tend - tstart) / 60);
}
int dp_get_iminute_minnum(int source, int minnum)//minnum为分钟数
{
	int mincount = (int)(source / 100) * 60 + (int)(source % 100) + minnum;
	return (int)(mincount / 60) * 100 + (int)(mincount % 60);
}

time_t dp_make_time(int tdate, int ttime)
{
	struct tm stime;
	stime.tm_year = tdate / 10000 - 1900;
	stime.tm_mon = (tdate % 10000) / 100 - 1;
	stime.tm_mday = tdate % 100;
	stime.tm_hour = ttime / 10000;
	stime.tm_min = (ttime % 10000) / 100;
	stime.tm_sec = ttime % 100;
	stime.tm_isdst = 0;
	stime.tm_wday = 0;
	stime.tm_yday = 0;
	return mktime(&stime);
}

int dp_get_week_ofday(int today)
{
	time_t tnow;
	struct tm ptm = { 0 };
	if (today == 0)
	{
		time(&tnow);
		dp_localtime(&ptm, &tnow);
	}
	else
	{
		tnow = dp_make_time(today, 0);
		dp_localtime(&ptm, &tnow);
	}
	return ptm.tm_wday;
}
int dp_get_month_ofday(int today)
{
	time_t tnow;
	struct tm ptm = { 0 };
	if (today == 0)
	{
		time(&tnow);
		dp_localtime(&ptm, &tnow);
	}
	else
	{
		tnow = dp_make_time(today, 0);
		dp_localtime(&ptm, &tnow);
	}
	return ptm.tm_mon + 1;
}
int dp_get_dayoffset_ofday(int tstart, int tend)
{
	time_t ttstart = dp_make_time(tstart, 0);
	time_t ttend = dp_make_time(tend, 0);

	return (int)((ttend - ttstart) / (24 * 3600));
}
int dp_next_work_day(int today_, int offset_ )
{
	time_t tt = dp_make_time(today_, 0);
	tt += offset_ * (24 * 3600);
	int today = dp_get_idate(tt);
	int week = dp_get_week_ofday(today);
	if (week == 0 || week == 6){
		dp_next_work_day(today, 1);
	}
	return today;
}

bool dp_str_is_date(char* date)
{
	int len = (int)strlen(date);
	if (len < 8) return false;
	for (int i = 0; i < len; i++)
	{
		if (date[i] >= '0' && date[i] <= '9')
			continue;
		else
		{
			return false;
		}
	}
	return true;
}

void format_minute(char * curTime, time_t ttime) //"930"
{
	if (!curTime) return;
	struct tm ptm = { 0 };
	check_time_t(ttime, &ptm);

	snprintf(curTime, sizeof(curTime), "%02d%02d", ptm.tm_hour, ptm.tm_min);
}

void format_date(char * curTime, time_t ttime) //"20150912"
{
	if (!curTime) return;
	struct tm ptm = { 0 };
	check_time_t(ttime, &ptm);

	snprintf(curTime, sizeof(curTime), "%d", (ptm.tm_year + 1900) * 10000 + (ptm.tm_mon + 1) * 100 + ptm.tm_mday);

}
void format_datetime(char * curTime, time_t ttime) //"20150912103000"
{
	if (!curTime) return;
	struct tm ptm = { 0 };
	check_time_t(ttime, &ptm);

	snprintf(curTime, sizeof(curTime), "%d%02d%02d%02d", (ptm.tm_year + 1900) * 10000 + (ptm.tm_mon + 1) * 100 + ptm.tm_mday,
		ptm.tm_hour  , ptm.tm_min  , ptm.tm_sec);
}

int get_minute_from_shortstr(char* time)  //"12:30" => 1230
{
	int nResult = 0;
	if (strlen(time) < 5) return nResult;

	int i = 0, c = 0;
	for (c = 0; c<2; ++c)
	if (!isdigit(time[i++])) return nResult;
	if (time[i++] != ':') return nResult;
	for (c = 0; c < 2; ++c)
	if (!isdigit(time[i++])) return nResult;
	int hour, min;
	i = 0;

	hour = time[i++] - '0';
	hour = 10 * hour + time[i++] - '0';
	if (23 < hour) return nResult;
	++i; // skip ':'

	min = time[i++] - '0';
	min = 10 * min + time[i++] - '0';
	if (59 < min) return nResult;

	nResult = hour * 100 + min;

	return nResult;
}

int get_itime_from_str(char* time) //"12:30:38" => 123038
{
	int nResult = 0;
	if (strlen(time)<8) return nResult;

	int i = 0, c = 0;

	for (c = 0; c < 2; ++c)
	if (!isdigit(time[i++])) return nResult;
	if (time[i++] != ':') return nResult;

	for (c = 0; c < 2; ++c)
	if (!isdigit(time[i++])) return nResult;
	if (time[i++] != ':') return nResult;

	for (c = 0; c < 2; ++c)
	if (!isdigit(time[i++])) return nResult;

	int hour, min, sec;// , millis;

	i = 0;

	hour = time[i++] - '0';
	hour = 10 * hour + time[i++] - '0';
	if (23 < hour) return nResult;
	++i; // skip ':'

	min = time[i++] - '0';
	min = 10 * min + time[i++] - '0';
	if (59 < min) return nResult;
	++i; // skip ':'

	sec = time[i++] - '0';
	sec = 10 * sec + time[i++] - '0';
	// No check for >= 0 as no '-' are converted here
	if (60 < sec) return nResult;

	nResult = hour * 10000 + min * 100 + sec;

	return nResult;
}

int get_idate_from_str(const char* time) //"20150212" = > 20150212
{
	int nResult = atoi(time);
	return nResult;
}

int get_time_from_longstr(char* datetime, int* nDate, int* nTime) //"2015-10-20 12:30:38" => 20151020,123038
{
	int nResult = 0;
	if (strlen(datetime)<19) return nResult;

	int i = 0, c = 0;

	for (c = 0; c < 4; ++c)
	if (!isdigit(datetime[i++])) return nResult;
	if (datetime[i++] != '-') return nResult;

	for (c = 0; c < 2; ++c)
	if (!isdigit(datetime[i++])) return nResult;
	if (datetime[i++] != '-') return nResult;

	for (c = 0; c < 2; ++c)
	if (!isdigit(datetime[i++])) return nResult;

	int year, mon, mday, hour, min, sec;// , millis;

	i = 0;
	//2015-12-20 09:12:00

	year = datetime[i++] - '0';
	year = 10 * year + datetime[i++] - '0';
	year = 10 * year + datetime[i++] - '0';
	year = 10 * year + datetime[i++] - '0';
	++i; // skip '-'

	mon = datetime[i++] - '0';
	mon = 10 * mon + datetime[i++] - '0';
	if (mon < 1 || 12 < mon) return nResult;
	++i; // skip '-'

	mday = datetime[i++] - '0';
	mday = 10 * mday + datetime[i++] - '0';
	if (mday < 1 || 31 < mday) return nResult;

	++i; // skip ' '

	hour = datetime[i++] - '0';
	hour = 10 * hour + datetime[i++] - '0';
	// No check for >= 0 as no '-' are converted here
	if (23 < hour) return nResult;

	++i; // skip ':'

	min = datetime[i++] - '0';
	min = 10 * min + datetime[i++] - '0';
	// No check for >= 0 as no '-' are converted here
	if (59 < min) return nResult;

	++i; // skip ':'

	sec = datetime[i++] - '0';
	sec = 10 * sec + datetime[i++] - '0';

	// No check for >= 0 as no '-' are converted here
	if (60 < sec) return nResult;

	*nDate = year * 10000 + mon * 100 + mday;
	*nTime = hour * 10000 + min * 100 + sec;

	return 1;
}

//单位毫秒
void dp_sleep(int msec)
{
#ifdef _MSC_VER
	Sleep(msec);
#else 
	usleep(msec * 1000);
#endif
}

s_delay *delay_create(unsigned int msec)
{
	s_delay *m = malloc(sizeof(*m));
	m->is_busy = false;
	m->m_msec_i = msec;
	if (m->m_msec_i < 5) m->m_msec_i = 5;
	if (m->m_msec_i > 5000) m->m_msec_i = 5000;
	m->start_msec = dp_nowtime_msec();
	return m;
}
void delay_busy(s_delay *m)
{
	m->is_busy = true;
}
void delay_destroy(s_delay *m)
{
	if (!m->is_busy) {
		m->usesd_msec = (unsigned int)(dp_nowtime_msec() - m->start_msec);
		if (m->m_msec_i > m->usesd_msec)
		{
			dp_sleep(m->m_msec_i - m->usesd_msec);
		}
	}
	free(m);
}

