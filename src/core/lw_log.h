#ifndef _LW_LOG_H
#define _LW_LOG_H

#include <stdio.h>
#include "lw_thread.h"
#include "lw_public.h"

#define LOG_DEBUG 1001

///////////////////////
//LOG_DEBUG用于新程序调试使用,默认大于1000,同一调试使用一个编号，正式发布不再定义，就可以不显示该LOG
//1极少的可能主程序进入和退出或出错崩溃的提示信息，***不得在循环体调用***
//2主函数和主要线程的可能出错或不运行的提示
//3次一级出错信息
//4再次一级出错信息
//5正常的流程和数据信息
//6-9根据重要性打印的数据信息
///////////////////////

typedef struct s_log {
	int logLevel;
	int threadid;
	char funcname[255];

	pthread_mutex_t muLog;
	FILE *logFile;
	char filename[255];
	char buffer[2048];
	int  maxsize;
} s_log;

extern struct s_log __log;

void log_open(const char *file, int level, int ms);
void log_close();
void log_print(const char *fmt, ...);

int writefile(char *name, void *value, size_t len);

#ifdef LOG_DEBUG
#define LOG(level) 	snprintf(__log.funcname,sizeof(__log.funcname), "%s",__FUNCTION__); if ((level == LOG_DEBUG)||( level <= __log.logLevel && level < 10 )) log_print
#else
#define LOG(level) 	snprintf(__log.funcname,sizeof(__log.funcname),"%s",__FUNCTION__); if ( level <= __log.logLevel && level < 10 ) log_print
#endif

#endif //_LW_LOG_H


