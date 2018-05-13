
#include "lw_log.h"
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>

struct s_log __log;

void log_open(const char *file, int level, int ms)
{
	if (__log.logFile){
		log_close();
	}
	__log.logLevel = level;
	__log.maxsize = ms;
	if (__log.maxsize <= 0) __log.maxsize = 1;
	pthread_mutex_init(&__log.muLog, NULL);

	snprintf(__log.filename, sizeof(__log.filename), "%s", file);
	bool b = (file && *file);
	if (b)
	{
		snprintf(__log.filename, sizeof(__log.filename), "%s", file);
		__log.logFile = fopen(__log.filename, "a+");
	}
	if (!__log.logFile)
	{
		__log.logFile = stderr;
		if (b)
		{
			LOG(1)("open log file fail.[%s]\n", __log.filename);
		}
	}
	__log.threadid = 0;
	__log.funcname[0] = 0;	
}
void log_close()
{
	if (__log.logFile && __log.logFile != stderr)
	{
		fclose(__log.logFile);
	}
	__log.logFile = NULL;
	pthread_mutex_destroy(&__log.muLog);
}
int log_check()
{
	if (!__log.logFile || __log.logFile == stderr) return -1;

	struct stat logFileStat;
	if (fstat(fileno(__log.logFile), &logFileStat) < 0)
	{
		return -2;
	}
	int filesize = (int)logFileStat.st_size;
	if (filesize > (__log.maxsize * 1024 * 1024))
	{
		log_close();
		char bakName[255], name[255];

		memset(name, 0, 255);
		int len = min((int)strlen(__log.filename), 255);
		for (int i = len - 1; i > 0; i--)
		{
			if (__log.filename[i] == '.')
			{
				strncpy(name, __log.filename, i);
				name[i] = 0;
				break;
			}
		}
		struct tm ptm = { 0 };
		time_t tt;
		time(&tt);
#ifndef _MSC_VER
		localtime_r(&tt, &ptm);
#else
		localtime_s(&ptm, &tt);
#endif
		snprintf(bakName, sizeof(bakName), "%s.%d-%d.log", name, (ptm.tm_year + 1900) * 10000 + (ptm.tm_mon + 1) * 100 + ptm.tm_mday,
			ptm.tm_hour * 10000 + ptm.tm_min * 100 + ptm.tm_sec);
		remove(bakName);
		rename(__log.filename, bakName);
		__log.logFile = fopen(__log.filename, "w+");
	}
	return 0;
}
void log_print(const char *fmt, ...)
{
	pthread_mutex_lock(&__log.muLog);
	if (log_check()) goto fail;

	struct tm ptm = {0};
	time_t tt;
	time(&tt);
#ifndef _MSC_VER
	localtime_r(&tt, &ptm);
#else
	localtime_s(&ptm, &tt);
#endif
	va_list args;
	va_start(args, fmt);
	__log.threadid = thread_self();
	snprintf(__log.buffer, sizeof(__log.buffer), "[%02d-%02d-%02d %02d:%02d:%02d][%x]%s.%s", (ptm.tm_year + 1900) % 100, (ptm.tm_mon + 1), ptm.tm_mday, ptm.tm_hour, ptm.tm_min, ptm.tm_sec,
		__log.threadid, __log.funcname, fmt);
	vfprintf(__log.logFile, __log.buffer, args);
	fflush(__log.logFile);
	va_end(args);
fail:
	pthread_mutex_unlock(&__log.muLog);
}

int writefile(char *name, void *value, size_t len)
{
	FILE *fp;
	fp = fopen(name, "ab+");
	if (fp)
	{
		int size = (int)fwrite((char *)value, 1, len, fp);
		fclose(fp);
		return size;
	}
	return 0;
}
