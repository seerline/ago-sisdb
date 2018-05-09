
#ifndef _STS_LOG_H
#define _STS_LOG_H

#include <stdio.h>
#include <stdlib.h>
#include <sts_types.h>

#define sts_out_error(a) printf
// sts_out_error 需要判断如果有回车就自动中断 使用("%.*s", 10, "xxxx") 方式

void sts_log_close();
bool sts_log_open(const char *log_);
char *sts_log(int level_, const char *fmt_, ...);

#endif //_STS_LOG_H