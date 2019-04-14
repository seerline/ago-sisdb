
#include "sis_time.h"

int sis_time_get_iyear(time_t ttime) //2015
{
	struct tm ptm = {0};
	sis_time_check(ttime, &ptm);
	return (ptm.tm_year + 1900);
}

int sis_time_get_imonth(time_t ttime) //201510
{
	struct tm ptm = {0};
	sis_time_check(ttime, &ptm);
	return (ptm.tm_year + 1900) * 100 + (ptm.tm_mon + 1);
}

int sis_time_get_idate(time_t ttime) //20151012
{
	struct tm ptm = {0};
	sis_time_check(ttime, &ptm);
	return (ptm.tm_year + 1900) * 10000 + (ptm.tm_mon + 1) * 100 + ptm.tm_mday;
}
int sis_time_get_id(int id) // 233035-000
{
	struct tm ptm = {0};
	sis_time_check(0, &ptm);
	return (ptm.tm_hour * 10000 + ptm.tm_min * 100 + ptm.tm_sec) * 1000 + id;
}
int sis_time_get_itime(time_t ttime) //103020
{
	struct tm ptm = {0};
	sis_time_check(ttime, &ptm);
	return ptm.tm_hour * 10000 + ptm.tm_min * 100 + ptm.tm_sec;
}
int sis_time_get_iminute(time_t ttime) //1030
{
	struct tm ptm = {0};
	sis_time_check(ttime, &ptm);
	return ptm.tm_hour * 100 + ptm.tm_min;
}
int sis_time_get_isec(time_t ttime)
{
	struct tm ptm = {0};
	sis_time_check(ttime, &ptm);
	return ptm.tm_sec;
}
int sis_time_get_showtime(time_t ttime)
{
	struct tm ptm = {0};
	sis_time_check(ttime, &ptm);
	return (ptm.tm_mon + 1) * 10000 * 10000 + ptm.tm_mday * 100 * 10000 + ptm.tm_hour * 10000 + ptm.tm_min * 100 + ptm.tm_sec;
}
short sis_time_get_offset_iminute(short nMin, int offsec)
{
	time_t tt = sis_time_make_time(0, nMin * 100);
	tt += offsec;
	return (short)sis_time_get_iminute(tt);
}
time_t sis_time_get_offset_time(time_t curTime, int offsec)
{
	//time_t tt = sis_time_make_time(sis_time_get_idate(&curTime), sis_time_get_iminute(&curTime) * 100 + offsec);
	time_t tt = curTime + offsec;
	return tt;
}
int sis_time_get_iminute_offset_i(int begin, int end) //930-1130
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
int sis_time_get_iminute_offset_time(time_t tstart, time_t tend)
{
	return (int)((tend - tstart) / 60);
}
int sis_time_get_iminute_minnum(int source, int minnum) //minnumÎª·ÖÖÓÊý
{
	int mincount = (int)(source / 100) * 60 + (int)(source % 100) + minnum;
	return (int)(mincount / 60) * 100 + (int)(mincount % 60);
}

time_t sis_time_make_time(int tdate, int ttime)
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

int sis_time_get_week_ofday(int today)
{
	time_t tnow;
	struct tm ptm = {0};
	if (today == 0)
	{
		time(&tnow);
		sis_localtime(&ptm, &tnow);
	}
	else
	{
		tnow = sis_time_make_time(today, 0);
		sis_localtime(&ptm, &tnow);
	}
	return ptm.tm_wday;
}
int sis_time_get_month_ofday(int today)
{
	time_t tnow;
	struct tm ptm = {0};
	if (today == 0)
	{
		time(&tnow);
		sis_localtime(&ptm, &tnow);
	}
	else
	{
		tnow = sis_time_make_time(today, 0);
		sis_localtime(&ptm, &tnow);
	}
	return ptm.tm_mon + 1;
}
int sis_time_get_dayoffset_ofday(int tstart, int tend)
{
	time_t ttstart = sis_time_make_time(tstart, 0);
	time_t ttend = sis_time_make_time(tend, 0);

	return (int)((ttend - ttstart) / (24 * 3600));
}
int sis_time_get_offset_day(int today_, int offset_) // ??????20100101??offset??
{
	time_t ttstart = sis_time_make_time(today_, 0);
	ttstart += (offset_ * 24 * 3600);
	return sis_time_get_idate(ttstart);
}
int sis_time_next_work_day(int today_, int offset_)
{
	time_t tt = sis_time_make_time(today_, 0);
	tt += offset_ * (24 * 3600);
	int today = sis_time_get_idate(tt);
	int week = sis_time_get_week_ofday(today);
	if (week == 0 || week == 6)
	{
		sis_time_next_work_day(today, 1);
	}
	return today;
}

bool sis_time_str_is_date(char *date)
{
	int len = (int)strlen(date);
	if (len < 8)
	{
		return false;
	}
	for (int i = 0; i < len; i++)
	{
		if (date[i] >= '0' && date[i] <= '9')
		{
			continue;
		}
		else
		{
			return false;
		}
	}
	return true;
}

void sis_time_format_minute(char *out_, size_t olen_, time_t tt_) //"930"
{
	if (!out_)
	{
		return;
	}
	struct tm ptm = {0};
	sis_time_check(tt_, &ptm);

	sis_sprintf(out_, olen_, "%02d%02d", ptm.tm_hour, ptm.tm_min);
}

void sis_time_format_date(char *out_, size_t olen_, time_t tt_) //"20150912"
{
	if (!out_)
	{
		return;
	}
	struct tm ptm = {0};
	sis_time_check(tt_, &ptm);

	sis_sprintf(out_, olen_, "%d", (ptm.tm_year + 1900) * 10000 + (ptm.tm_mon + 1) * 100 + ptm.tm_mday);
}
void sis_time_format_datetime(char *out_, size_t olen_, time_t tt_) //"20150912103000"
{
	if (!out_)
	{
		return;
	}
	struct tm ptm = {0};
	sis_time_check(tt_, &ptm);

	sis_sprintf(out_, olen_, "%d%02d%02d%02d", (ptm.tm_year + 1900) * 10000 + (ptm.tm_mon + 1) * 100 + ptm.tm_mday,
				ptm.tm_hour, ptm.tm_min, ptm.tm_sec);
}
void sis_time_format_datetime_longstr(char *out_, size_t olen_, int idate_, int itime_)
{
	if (!out_)
	{
		return;
	}
	time_t tt = sis_time_make_time(idate_, itime_);
	struct tm ptm = {0};
	sis_time_check(tt, &ptm);

	sis_sprintf(out_, olen_, "%04d-%02d-%02d %02d:%02d:%02d",
				ptm.tm_year + 1900, ptm.tm_mon + 1, ptm.tm_mday,
				ptm.tm_hour, ptm.tm_min, ptm.tm_sec);
}
int sis_time_get_minute_from_shortstr(char *time) //"12:30" => 1230
{
	int out = 0;
	if (strlen(time) < 5)
	{
		return out;
	}

	int i = 0, c = 0;
	for (c = 0; c < 2; ++c)
	{
		if (!isdigit(time[i++]))
		{
			return out;
		}
	}
	if (time[i++] != ':')
	{
		return out;
	}
	for (c = 0; c < 2; ++c)
	{
		if (!isdigit(time[i++]))
		{
			return out;
		}
	}
	int hour, min;
	i = 0;

	hour = time[i++] - '0';
	hour = 10 * hour + time[i++] - '0';
	if (23 < hour)
	{
		return out;
	}
	++i; // skip ':'

	min = time[i++] - '0';
	min = 10 * min + time[i++] - '0';
	if (59 < min)
	{
		return out;
	}

	out = hour * 100 + min;

	return out;
}

int sis_time_get_itime_from_str(char *time) //"12:30:38" => 123038
{
	int out = 0;
	if (strlen(time) < 8)
	{
		return out;
	}

	int i = 0, c = 0;

	for (c = 0; c < 2; ++c)
	{
		if (!isdigit(time[i++]))
		{
			return out;
		}
	}
	if (time[i++] != ':')
	{
		return out;
	}

	for (c = 0; c < 2; ++c)
	{
		if (!isdigit(time[i++]))
		{
			return out;
		}
	}
	if (time[i++] != ':')
	{
		return out;
	}

	for (c = 0; c < 2; ++c)
	{
		if (!isdigit(time[i++]))
		{
			return out;
		}
	}

	int hour, min, sec; // , millis;

	i = 0;

	hour = time[i++] - '0';
	hour = 10 * hour + time[i++] - '0';
	if (23 < hour)
	{
		return out;
	}
	++i; // skip ':'

	min = time[i++] - '0';
	min = 10 * min + time[i++] - '0';
	if (59 < min)
	{
		return out;
	}
	++i; // skip ':'

	sec = time[i++] - '0';
	sec = 10 * sec + time[i++] - '0';
	// No check for >= 0 as no '-' are converted here
	if (60 < sec)
	{
		return out;
	}

	out = hour * 10000 + min * 100 + sec;

	return out;
}

//"xxx-20150212.xxx" => 20150212
int sis_time_get_idate_from_str(const char *in_, char bc_)
{
	int len = strlen(in_);

	int start = 0;
	
	while(bc_ && start < len && in_[start] != bc_)
	{
		start++;
	}
	if(start + 8 > len)
	{
		return 0;
	}

	for (int i = start; i < 8; i++)
	{
		if (!isdigit(in_[i]))
		{
			return 0;
		}
	}

	int year, mon, mday; 

	int i = start;

	year = in_[i++] - '0';
	year = 10 * year + in_[i++] - '0';
	year = 10 * year + in_[i++] - '0';
	year = 10 * year + in_[i++] - '0';

	mon = in_[i++] - '0';
	mon = 10 * mon + in_[i++] - '0';
	if (mon < 1 || 12 < mon)
	{
		return 0;
	}
	mday = in_[i++] - '0';
	mday = 10 * mday + in_[i++] - '0';
	if (mday < 1 || 31 < mday)
	{
		return 0;
	}
	return year * 10000 + mon * 100 + mday;
}
int sis_time_get_idate_from_shstr(const char *in_) //"2015-10-20" => 20151020
{
	int out = 0;
	if (strlen(in_) < 8)
	{
		return out;
	}
	int i = 0, c = 0;

	for (c = 0; c < 4; ++c)
	{
		if (!isdigit(in_[i++]))
		{
			return out;
		}
	}
	if (in_[i++] != '-')
	{
		return out;
	}

	for (c = 0; c < 2; ++c)
	{
		if (!isdigit(in_[i++]))
		{
			return out;
		}
	}
	if (in_[i++] != '-')
	{
		return out;
	}

	for (c = 0; c < 2; ++c)
	{
		if (!isdigit(in_[i++]))
		{
			return out;
		}
	}

	int year, mon, mday; // , millis;

	i = 0;
	//2015-12-20

	year = in_[i++] - '0';
	year = 10 * year + in_[i++] - '0';
	year = 10 * year + in_[i++] - '0';
	year = 10 * year + in_[i++] - '0';
	++i; // skip '-'

	mon = in_[i++] - '0';
	mon = 10 * mon + in_[i++] - '0';
	if (mon < 1 || 12 < mon)
	{
		return out;
	}
	++i; // skip '-'

	mday = in_[i++] - '0';
	mday = 10 * mday + in_[i++] - '0';
	if (mday < 1 || 31 < mday)
	{
		return out;
	}

	return year * 10000 + mon * 100 + mday;
}
int sis_time_get_time_from_longstr(const char *in_, int *date_, int *time_) //"2015-10-20 12:30:38" => 20151020,123038
{
	int out = 0;
	if (strlen(in_) < 19)
	{
		return out;
	}

	int i = 0, c = 0;

	for (c = 0; c < 4; ++c)
	{
		if (!isdigit(in_[i++]))
		{
			return out;
		}
	}
	if (in_[i++] != '-')
	{
		return out;
	}

	for (c = 0; c < 2; ++c)
	{
		if (!isdigit(in_[i++]))
		{
			return out;
		}
	}
	if (in_[i++] != '-')
	{
		return out;
	}

	for (c = 0; c < 2; ++c)
	{
		if (!isdigit(in_[i++]))
		{
			return out;
		}
	}

	int year, mon, mday, hour, min, sec; // , millis;

	i = 0;
	//2015-12-20 09:12:00

	year = in_[i++] - '0';
	year = 10 * year + in_[i++] - '0';
	year = 10 * year + in_[i++] - '0';
	year = 10 * year + in_[i++] - '0';
	++i; // skip '-'

	mon = in_[i++] - '0';
	mon = 10 * mon + in_[i++] - '0';
	if (mon < 1 || 12 < mon)
	{
		return out;
	}
	++i; // skip '-'

	mday = in_[i++] - '0';
	mday = 10 * mday + in_[i++] - '0';
	if (mday < 1 || 31 < mday)
	{
		return out;
	}

	++i; // skip ' '

	hour = in_[i++] - '0';
	hour = 10 * hour + in_[i++] - '0';
	// No check for >= 0 as no '-' are converted here
	if (23 < hour)
	{
		return out;
	}

	++i; // skip ':'

	min = in_[i++] - '0';
	min = 10 * min + in_[i++] - '0';
	// No check for >= 0 as no '-' are converted here
	if (59 < min)
	{
		return out;
	}

	++i; // skip ':'

	sec = in_[i++] - '0';
	sec = 10 * sec + in_[i++] - '0';

	// No check for >= 0 as no '-' are converted here
	if (60 < sec)
	{
		return out;
	}

	*date_ = year * 10000 + mon * 100 + mday;
	*time_ = hour * 10000 + min * 100 + sec;

	return 1;
}

int sis_time_get_time_from_shstr(const char *in_, int *date_, int *time_) //"20151020-12:30:38.110" => 20151020,123038
{
	int out = 0;
	if (strlen(in_) < 19)
	{
		return out;
	}

	int i = 0, c = 0;

	for (c = 0; c < 8; ++c)
	{
		if (!isdigit(in_[i++]))
		{
			return out;
		}
	}
	if (in_[i++] != '-')
	{
		return out;
	}

	int year, mon, mday, hour, min, sec; // , millis;

	i = 0;
	//20151020-12:30:38.110

	year = in_[i++] - '0';
	year = 10 * year + in_[i++] - '0';
	year = 10 * year + in_[i++] - '0';
	year = 10 * year + in_[i++] - '0';

	mon = in_[i++] - '0';
	mon = 10 * mon + in_[i++] - '0';
	if (mon < 1 || 12 < mon)
	{
		return out;
	}

	mday = in_[i++] - '0';
	mday = 10 * mday + in_[i++] - '0';
	if (mday < 1 || 31 < mday)
	{
		return out;
	}

	++i; // skip ' '

	hour = in_[i++] - '0';
	hour = 10 * hour + in_[i++] - '0';
	// No check for >= 0 as no '-' are converted here
	if (23 < hour)
	{
		return out;
	}

	++i; // skip ':'

	min = in_[i++] - '0';
	min = 10 * min + in_[i++] - '0';
	// No check for >= 0 as no '-' are converted here
	if (59 < min)
	{
		return out;
	}

	++i; // skip ':'

	sec = in_[i++] - '0';
	sec = 10 * sec + in_[i++] - '0';

	// No check for >= 0 as no '-' are converted here
	if (60 < sec)
	{
		return out;
	}

	*date_ = year * 10000 + mon * 100 + mday;
	*time_ = hour * 10000 + min * 100 + sec;

	return 1;
}

s_sis_time_delay *sis_delay_create(unsigned int msec)
{
	s_sis_time_delay *m = sis_malloc(sizeof(*m));
	m->is_busy = false;
	m->m_msec_i = msec;
	if (m->m_msec_i < 5)
	{
		m->m_msec_i = 5;
	}
	if (m->m_msec_i > 5000)
	{
		m->m_msec_i = 5000;
	}
	m->start_msec = sis_time_get_now_msec();
	return m;
}
void sis_delay_busy(s_sis_time_delay *m)
{
	m->is_busy = true;
}
void sis_delay_destroy(s_sis_time_delay *m)
{
	if (!m->is_busy)
	{
		m->usesd_msec = (unsigned int)(sis_time_get_now_msec() - m->start_msec);
		if (m->m_msec_i > m->usesd_msec)
		{
			sis_sleep(m->m_msec_i - m->usesd_msec);
		}
	}
	sis_free(m);
}
