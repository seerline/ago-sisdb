#ifndef _OS_TIME_H
#define _OS_TIME_H

#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#define sis_time_get_day gettimeofday

void sis_localtime(struct tm *m, const time_t* t);

#ifdef __cplusplus
extern "C" {
#endif

//获取当前秒数
time_t sis_time_get_now(); 
//获得当前毫秒数
unsigned long long sis_time_get_now_msec(); 
//获得当前微秒数
unsigned long long sis_time_get_now_usec(); 
// 检查tt用当前时间替换
void sis_time_check(time_t tt_, struct tm *ptm_);
//睡眠单位毫秒
void sis_sleep(int msec);

void sis_time_format_now(char *out_, size_t olen);

#ifdef __cplusplus
}
#endif
#endif //_OS_TIME_H