#ifndef _SIS_TIME_H
#define _SIS_TIME_H

#include <sis_core.h>
#include <sis_str.h>
#include <sis_os.h>
#include <os_time.h>

#ifdef __cplusplus
extern "C" {
#endif

int sis_time_get_iyear(time_t ttime); //2015
int sis_time_get_imonth(time_t ttime); //201510
int sis_time_get_idate(time_t ttime); //20151012
int sis_time_get_id(int id); // 233035-000

int sis_time_get_itime(time_t ttime); //103020
int sis_msec_get_itime(msec_t msec); //103020
int sis_msec_get_idate(msec_t msec); //20151012
long sis_msec_get_mtime(msec_t msec);//103020000 时分秒毫秒
int sis_sec_get_itime(time_t ttime); //103020
int sis_time_get_iminute(time_t ttime); //1030
int sis_msec_get_iminute(msec_t msec); //1030
int sis_time_get_isec(time_t ttime); // 20
int sis_time_get_showtime(time_t ttime); //0212103020 月日时间
msec_t sis_msec_get_showtime(msec_t ttime); //20211030103050123 月日时间

short sis_time_get_offset_iminute(short nMin, int offsec);
time_t sis_time_get_offset_time(time_t curTime, int offsec);  //增加或减少秒，返回time_t

int sis_time_get_isec_offset_i(int begin, int end); //93010-113010 
int sis_time_get_iminute_offset_i(int begin, int end); //930-1130 
int sis_time_get_iminute_offset_time(time_t tstart, time_t tend);// 判断中间间隔几分钟
int sis_time_get_iminute_minnum(int source, int minnum);  //增加或减少分钟  900,-5 --> 855

time_t sis_time_make_time(int tdate, int ttime);
msec_t sis_time_make_msec(int tdate, int ttime, int msec);
int sis_time_get_week_ofday(int today);// 判断是周几 [0,6]
int sis_time_get_month_ofday(int today);// 判断是几月[0,11]
int sis_time_get_dayoffset_ofday(int tstart, int tend);// 判断中间间隔几天
int sis_time_next_work_day(int today_, int offset_);//跳过周末

int sis_time_get_offset_day(int today_, int offset_); // 根据当前日期20100101偏移offset天，
// < 0 往前数 > 0 往后数
bool sis_time_str_is_date(char* date); //判断字符串是不是日期20150212

void sis_time_format_minute(char * out_, size_t olen_, time_t tt_); //"930"
void sis_time_format_date(char * out_, size_t olen_, int date_); //"2015-09-12"
void sis_time_format_csec(char * out_, size_t olen_, msec_t tt_); //"09:30:00"
void sis_time_format_msec(char * out_, size_t olen_, msec_t tt_); //"09:30:00.123"
void sis_time_format_datetime(char * out_, size_t olen_, time_t tt_); //"20150912103000"
void sis_msec_format_datetime(char * out_, size_t olen_, msec_t tt_); //"20150912103059000"
void sis_time_format_datetime_longstr(char * out_, size_t olen_, int idate_, int itime_); // "2008-12-13 09:30:00"

void sis_time_format_msec_longstr(char * out_, size_t olen_, msec_t msec_); // "2008-12-13 09:30:00.000"

int sis_time_get_minute_from_shortstr(char* in_);//"12:30" => 1230
int sis_time_get_itime_from_str(char* in_);//"12:30:38" => 123038
int sis_time_get_iyear_from_str(const char* in_, char bc_);//"xxx-20150212.xxx" => 2015
int sis_time_get_idate_from_str(const char* in_, char bc_);//"xxx-20150212.xxx" => 20150212
int sis_time_get_idate_from_shstr(const char* in_);//"2015-02-12" => 20150212
int sis_time_get_time_from_shstr(const char* , int* , int* ); //"20151020-12:30:38.110" => 20151020,123038

msec_t sis_time_get_msec_from_shortstr(const char *in_, int idate); //"12:30:38"  
msec_t sis_time_get_msec_from_longstr(const char* ); //"2015-10-20 12:30:38" | //"2015-10-20 12:30:38.100" 
msec_t sis_time_get_msec_from_str(const char *sdate,const char *stime); // "2015-10-20" "12:30:38.110"
msec_t sis_time_get_msec_from_int(int64 ); // "20151020123038110"

#ifdef __cplusplus
}
#endif

// 长期等待 不超时退出
#define SIS_WAIT_LONG(_a_) do { while(!(_a_)) { sis_sleep(100); }} while(0)
// 等待条件产生 或超时
#define SIS_WAIT_TIME(_a_,_t_) do { int _wt_ = _t_; while(!(_a_)) { _wt_-=3; sis_sleep(3); if(_wt_ < 0) break; }} while(0);

typedef struct s_sis_time_delay {
	bool is_busy;
	unsigned int m_msec_i;
	unsigned int usesd_msec;
	unsigned long long start_msec;
} s_sis_time_delay;

s_sis_time_delay *sis_delay_create(unsigned int msec);
void sis_delay_busy(s_sis_time_delay *m);
void sis_delay_destroy(s_sis_time_delay *m);

typedef struct s_sis_msec_pair{
	msec_t	start;  
	msec_t	stop; 
}s_sis_msec_pair;

static inline bool sis_msec_pair_whole(s_sis_msec_pair *mp_)
{
	return (mp_->stop < mp_->start || (mp_->stop == 0 && mp_->start == 0));
}

typedef struct s_sis_minute_pair{
	uint16	first;  // 单位分钟
	uint16	second; // 单位分钟
}s_sis_minute_pair;


typedef struct s_sis_time_pair{
	uint32	first;  // 单位分钟
	uint32	second; // 单位分钟
}s_sis_time_pair;

typedef struct s_sis_date_pair{
	uint32	start;  // 开始日期
	uint32	stop;   // 结束日期
	uint32	move;   // 当前日期
}s_sis_date_pair;

typedef struct s_sis_time_gap{
	uint16	start; // 单位分钟 如果为0就不判断
	uint16	stop;  // 单位分钟 如果为0就不判断
	uint32	delay; // 间隔秒数
}s_sis_time_gap;

#endif //_SIS_TIME_H