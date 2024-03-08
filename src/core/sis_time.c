
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
int sis_msec_get_itime(msec_t msec) //103020
{
	if (msec == 0)
	{
		return 0;
	}
	struct tm ptm = {0};
	sis_time_check((time_t)(msec/1000), &ptm);
	return ptm.tm_hour * 10000 + ptm.tm_min * 100 + ptm.tm_sec;
}
int sis_msec_get_idate(msec_t msec) //20151012
{
	if (msec == 0)
	{
		return 0;
	}
	struct tm ptm = {0};
	sis_time_check((time_t)(msec/1000), &ptm);
	return (ptm.tm_year + 1900) * 10000 + (ptm.tm_mon + 1) * 100 + ptm.tm_mday;
}
long sis_msec_get_mtime(msec_t msec) //103020 000
{
	if (msec == 0)
	{
		return 0;
	}
	struct tm ptm = {0};
	sis_time_check((time_t)(msec/1000), &ptm);
	return (ptm.tm_hour * 10000 + ptm.tm_min * 100 + ptm.tm_sec) * 1000l + msec % 1000;
}

int sis_sec_get_itime(time_t ttime) //103020
{
	return ttime ? sis_time_get_itime(ttime) : 0;
}
int sis_time_get_iminute(time_t ttime) //1030
{
	struct tm ptm = {0};
	sis_time_check(ttime, &ptm);
	return ptm.tm_hour * 100 + ptm.tm_min;
}
int sis_msec_get_iminute(msec_t msec) //1030
{
	if (msec == 0)
	{
		return 0;
	}
	struct tm ptm = {0};
	sis_time_check((time_t)(msec/1000), &ptm);
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
msec_t sis_msec_get_showtime(msec_t ttime) //20211030 103050123 月日时间
{
	int msec = ttime % 1000;
	struct tm ptm = {0};
	sis_time_check(ttime / 1000, &ptm);
	msec_t o = ((ptm.tm_year + 1900) * 10000 + (ptm.tm_mon + 1) * 100 + ptm.tm_mday);
	return o * 1000000000 + ptm.tm_hour * 10000000 + ptm.tm_min * 100000 + ptm.tm_sec * 1000 + msec;
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
int sis_time_get_isec_offset_i(int begin, int end) //93010-113010
{
	int bh, eh, bm, em, bs, es;
	int count = 0;
	bh = (int)(begin / 10000);
	bm = (int)((begin % 10000) / 100);
	bs = (int)(begin % 100);
	eh = (int)(end / 10000);
	em = (int)((end % 10000) / 100);
	es = (int)(end % 100);
	count = (eh * 3600 + em * 60 + es) - (bh * 3600 + bm * 60 + bs);
	return count;
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
msec_t sis_time_make_msec(int tdate, int ttime, int msec)
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
	msec_t ms = mktime(&stime);
	return ms * 1000 + msec % 1000;	
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
int sis_time_get_offset_day(int today_, int offset_) 
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

void sis_time_format_date(char *out_, size_t olen_, int date_) //"2015-09-12"
{
	if (!out_)
	{
		return;
	}
	int date = date_ % 10000;
	sis_sprintf(out_, olen_, "%04d-%02d-%02d", date_ / 10000, date / 100, date % 100);
}

void sis_time_format_msec(char * out_, size_t olen_, msec_t tt_) //"09:30:00.123"
{
	if (!out_)
	{
		return;
	}
	if (tt_ == 0)
	{
		sis_strcpy(out_, olen_, "00:00:00.000");
		return;
	}
	int msec = tt_ % 1000;
	time_t tt = tt_ / 1000;
	struct tm ptm = {0};
	sis_time_check(tt, &ptm);

	sis_sprintf(out_, olen_, "%02d:%02d:%02d.%03d",
				ptm.tm_hour, ptm.tm_min, ptm.tm_sec, msec);

}
void sis_time_format_csec(char * out_, size_t olen_, msec_t tt_) //"09:30:00"
{
	if (!out_)
	{
		return;
	}
	if (tt_ == 0)
	{
		sis_strcpy(out_, olen_, "00:00:00");
		return;
	}
	time_t tt = tt_ / 1000;
	struct tm ptm = {0};
	sis_time_check(tt, &ptm);

	sis_sprintf(out_, olen_, "%02d:%02d:%02d",
				ptm.tm_hour, ptm.tm_min, ptm.tm_sec);

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
void sis_msec_format_datetime(char * out_, size_t olen_, msec_t msec_) //"20150912103059000"
{
	if (!out_)
	{
		return;
	}
	int msec = msec_ % 1000;
	time_t tt = msec_ / 1000;
	struct tm ptm = {0};
	sis_time_check(tt, &ptm);
	sis_sprintf(out_, olen_, "%d%02d%02d%02d%03d", (ptm.tm_year + 1900) * 10000 + (ptm.tm_mon + 1) * 100 + ptm.tm_mday,
				ptm.tm_hour, ptm.tm_min, ptm.tm_sec, msec);

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
void sis_time_format_msec_longstr(char * out_, size_t olen_, msec_t msec_) // "2008-12-13 09:30:00.000"
{
	if (!out_)
	{
		return;
	}	
	int msec = msec_ % 1000;
	time_t tt = msec_ / 1000;
	struct tm ptm = {0};
	sis_time_check(tt, &ptm);

	sis_sprintf(out_, olen_, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
				ptm.tm_year + 1900, ptm.tm_mon + 1, ptm.tm_mday,
				ptm.tm_hour, ptm.tm_min, ptm.tm_sec, msec);
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
int sis_time_get_iyear_from_str(const char *in_, char bc_)
{
	int len = strlen(in_);

	int start = 0;
	
	while(bc_ && start < len && in_[start] != bc_)
	{
		start++;
	}
	if(start + 4 > len)
	{
		return 0;
	}
	for (int i = start; i < 4; i++)
	{
		if (!isdigit(in_[i]))
		{
			return 0;
		}
	}
	int year; 

	int i = start;

	year = in_[i++] - '0';
	year = 10 * year + in_[i++] - '0';
	year = 10 * year + in_[i++] - '0';
	year = 10 * year + in_[i++] - '0';

	return year;
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
msec_t sis_time_get_msec_from_shortstr(const char *in_, int idate) //"12:30:38"  
{
	msec_t out = 0;
	if (strlen(in_) < 7)
	{
		return out;
	}

	int i = 0;

	int hour, min, sec, msec = 0;

	i = 0;

	hour = in_[i++] - '0';
	if (in_[i] != ':')
	{
		hour = 10 * hour + in_[i++] - '0';
	}
	if (23 < hour)
	{
		return out;
	}

	i++; // skip ':'

	min = in_[i++] - '0';
	if (in_[i] != ':')
	{
		min = 10 * min + in_[i++] - '0';
	}
	if (59 < min)
	{
		return out;
	}

	i++; // skip ':'

	sec = in_[i++] - '0';
	if (in_[i] != ':')
	{
		sec = 10 * sec + in_[i++] - '0';
	}
	if (60 < sec)
	{
		return out;
	}

	if (in_[i] && in_[i] == '.')
	{
		++i;
		msec = in_[i++] - '0';
		msec = 10 * msec + in_[i++] - '0';
		msec = 10 * msec + in_[i++] - '0';
	}
	out = sis_time_make_time(idate, hour * 10000 + min * 100 + sec);
    return  out * 1000 + msec;
}

msec_t sis_time_get_msec_from_longstr(const char *in_) //"2015-10-20 12:30:38"
{
	msec_t out = 0;
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

	int year, mon, mday, hour, min, sec, msec = 0;

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

	if (in_[i] && in_[i] == '.')
	{
		++i;
		msec = in_[i++] - '0';
		msec = 10 * msec + in_[i++] - '0';
		msec = 10 * msec + in_[i++] - '0';
	}
	out = sis_time_make_time(year * 10000 + mon * 100 + mday, hour * 10000 + min * 100 + sec);
    return  out * 1000 + msec;
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
msec_t sis_time_get_msec_from_str(const char *sdate,const char *stime)
{
	msec_t curmsec = sis_time_get_now_msec();
	if (strlen(sdate) < 10||strlen(stime) < 12)
	{
		return curmsec;
	}
	int i = 0, c = 0;
	int year = 0, mon = 0, mday = 0, hour = 0, min = 0, sec = 0, msec = 0;
	// 检查日期格式
	for (c = 0; c < 4; ++c, i++)
	{
		if (!isdigit(sdate[i]))
		{
			return curmsec;
		}
		year = c == 0 ? sdate[i] - '0' : 10 * year + sdate[i] - '0';
	}
	if (sdate[i++] != '-')
	{
		return curmsec;
	}
	for (c = 0; c < 2; ++c, i++)
	{
		if (!isdigit(sdate[i]))
		{
			return curmsec;
		}
		mon = c == 0 ? sdate[i] - '0' : 10 * mon + sdate[i] - '0';
	}
	if (mon < 1 || 12 < mon)
	{
		return curmsec;
	}
	if (sdate[i++] != '-')
	{
		return curmsec;
	}
	for (c = 0; c < 2; ++c, i++)
	{
		if (!isdigit(sdate[i]))
		{
			return curmsec;
		}
		mday = c == 0 ? sdate[i] - '0' : 10 * mday + sdate[i] - '0';
	}
	if (mday < 1 || 31 < mday)
	{
		return curmsec;
	}
	i = 0;
	// 检查时间格式
	for (c = 0; c < 2; ++c, i++)
	{
		if (!isdigit(stime[i]))
		{
			return curmsec;
		}
		hour = c == 0 ? stime[i] - '0' : 10 * hour + stime[i] - '0';
	}
	if (hour < 0 || hour > 23)
	{
		return curmsec;
	}
	if (stime[i++] != ':')
	{
		return curmsec;
	}
	for (c = 0; c < 2; ++c, i++)
	{
		if (!isdigit(stime[i]))
		{
			return curmsec;
		}
		min = c == 0 ? stime[i] - '0' : 10 * min + stime[i] - '0';
	}
	if (min < 0 || min > 59)
	{
		return curmsec;
	}
	if (stime[i++] != ':')
	{
		return curmsec;
	}
	for (c = 0; c < 2; ++c, i++)
	{
		if (!isdigit(stime[i]))
		{
			return curmsec;
		}
		sec = c == 0 ? stime[i] - '0' : 10 * sec + stime[i] - '0';
	}
	if (sec < 0 || sec > 59)
	{
		return curmsec;
	}
	if (stime[i++] != '.')
	{
		return curmsec;
	}
	for (c = 0; c < 3; ++c, i++)
	{
		if (!isdigit(stime[i]))
		{
			return curmsec;
		}
		msec = c == 0 ? stime[i] - '0' : 10 * msec + stime[i] - '0';
	}
	curmsec = sis_time_make_time(year * 10000 + mon * 100 + mday, hour * 10000 + min * 100 + sec);
	return curmsec * 1000 + msec;

}
msec_t sis_time_get_msec_from_int(int64 inmsec) // "20151020123038110"
{
	int msec = 0;
	// if (inmsec > 1020123038110)
	if (inmsec > 9999999999999)
	{
		msec = inmsec % 1000;
	}
	msec_t curmsec = inmsec / 1000;
	int idate = curmsec / 1000000;
	int itime = curmsec % 1000000;
	curmsec = sis_time_make_time(idate, itime);	
	return curmsec * 1000 + msec;
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
