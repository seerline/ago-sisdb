
#include <sis_log.h>
#include <os_str.h>
#include <sis_time.h>
/********************************/
// 一定要用static定义，不然内存混乱
static s_sis_log _sis_log = {
	.outscreen = true,
	.init_mutex = false,
	.level = 10,
	.maxsize = 5,
	.logfp = NULL
};

/********************************/
void sis_log_start()
{
	sis_mutex_init(&_sis_log.mutex, NULL);
	_sis_log.init_mutex = true;
}
void sis_log_stop()
{
	sis_log_close();
	sis_mutex_destroy(&_sis_log.mutex);
	_sis_log.init_mutex = false;
}

void sis_log_close()
{
	if (_sis_log.logfp)
	{
		sis_file_close(_sis_log.logfp);
		_sis_log.logfp = NULL;
	}
}

void sis_log_open(const char *log_, int level_, int limit_)
{
	// 如果指定了 log_ 名就输出到文件，
	// 如果等于 console 就打印到屏幕上
	// 配置中应该有一个log文件名和最高级别
	// 建立一个全局变量，专门用于输出log
	// error直接输出到屏幕
	sis_log_close();

	if (!log_ || !sis_strcasecmp(log_, "console"))
	{
		_sis_log.outscreen = true;
	}
	else
	{
		sis_strncpy(_sis_log.filename, 255, log_, strlen(log_));
		_sis_log.outscreen = false;
	}

	_sis_log.level = level_;
	_sis_log.maxsize = limit_;
	if (_sis_log.maxsize <= 0)
	{
		_sis_log.maxsize = 1;
	}

	if (!_sis_log.outscreen)
	{
		_sis_log.logfp = sis_file_open(_sis_log.filename, 
			SIS_FILE_IO_READ | SIS_FILE_IO_WRITE | SIS_FILE_IO_CREATE | SIS_FILE_IO_APPEND, 0);
		if (!_sis_log.logfp)
		{
			printf("open log file fail.[%s]\n", _sis_log.filename);
		}
	}
}
bool sis_log_input(int level_, s_sis_thread_id_t thread_, const char *func_)
{
	if (level_ > _sis_log.level)
	{
		return false;
	}
	_sis_log.warning = level_;
	_sis_log.id = thread_;
	sis_strncpy(_sis_log.funcname, 255, func_, strlen(func_));
	return true;
}

void sis_log_check()
{
	if (_sis_log.outscreen||!_sis_log.logfp)
	{
		return ;
	}
	sis_time_format_now(_sis_log.showtime, 127);

	struct stat log_stat;
	if (fstat(fileno(_sis_log.logfp), &log_stat) < 0)
	{
		return;
	}
	int filesize = (int)log_stat.st_size;
	if (filesize > (_sis_log.maxsize * 1024 * 1024))
	{
		sis_log_close();
		char newfile[255], oldname[255];

		memset(oldname, 0, 255);
		for (int i = (int)strlen(_sis_log.filename) - 1; i > 0; i--)
		{
			if (_sis_log.filename[i] == '.')
			{
				sis_strncpy(oldname, 255, _sis_log.filename, i);
				break;
			}
		}
		memset(newfile, 0, 255);
		sis_sprintf(newfile, 255, "%s.%s.log", oldname, _sis_log.showtime);
		remove(newfile);
		rename(_sis_log.filename, newfile);
		_sis_log.logfp = sis_file_open(_sis_log.filename, 
			// SIS_FILE_IO_READ | SIS_FILE_IO_WRITE | SIS_FILE_IO_TRUNC, 0);
			SIS_FILE_IO_READ | SIS_FILE_IO_WRITE | SIS_FILE_IO_CREATE, 0);
	}

}

void sis_log(const char *fmt_, ...)
{
	if (!_sis_log.init_mutex)
	{
		sis_log_start();
	}
	sis_mutex_lock(&_sis_log.mutex);
	
	sis_log_check();

	va_list args;

	if (_sis_log.outscreen)
	{
		switch (_sis_log.warning)
		{
			case 0:
				fprintf(stderr, CLR_LHOT);
				break;
			case 1:
				fprintf(stderr, CLR_LBLUE);
				break;
			case 2:
				fprintf(stderr, CLR_YELLOW);
				break;
			default:
				break;
		}
		sis_time_format_now(_sis_log.showtime, 127);
		va_start(args, fmt_);
		sis_sprintf(_sis_log.buffer, 1023, "%s[%x]%s.%s", 
			_sis_log.showtime, sis_thread_handle(_sis_log.id), _sis_log.funcname, fmt_);
		vfprintf(stderr, _sis_log.buffer, args);
		// vfprintf(stderr, fmt_, args);
		va_end(args);
		fprintf(stderr, RESET);
	}
	else
	{	
		va_start(args, fmt_);
		sis_sprintf(_sis_log.buffer, 1023, "%s[%x]%s.%s", 
			_sis_log.showtime, sis_thread_handle(_sis_log.id), _sis_log.funcname, fmt_);
		vfprintf(_sis_log.logfp, _sis_log.buffer, args);
		fflush(_sis_log.logfp);
		va_end(args);
	}
	sis_mutex_unlock(&_sis_log.mutex);
}

//////////////////////////////////////////
//
//////////////////////////////////////////


void sis_out_percent_start(const char *info_)
{
	if (!_sis_log.outscreen)
	{
		return ;
	}
	// printf("%s     ", info_);
}

void sis_out_percent(size_t step_, size_t count_)
{
	if (!_sis_log.outscreen)
	{
		return ;
	}
	printf("\b\b\b\b%3.0f%%", 100 * (float)step_ / (float)count_);
	fflush(stdout);
}
void sis_out_percent_stop()
{
	if (!_sis_log.outscreen)
	{
		return ;
	}
	printf("\b\b\b\b100%%\n");
}

size_t sis_writefile(const char *name, void *value, size_t len)
{
	s_sis_file_handle fp = sis_file_open(name, SIS_FILE_IO_READ | SIS_FILE_IO_WRITE | SIS_FILE_IO_CREATE | SIS_FILE_IO_APPEND, 0);
	if (fp)
	{
		size_t size = sis_file_write(fp, (char *)value, len);
		sis_file_close(fp);
		return size;
	}
	return 0;
}
size_t sis_readfile(const char *name, void *value, size_t len)
{
	s_sis_file_handle fp = sis_file_open(name, SIS_FILE_IO_READ, 0);
	if (fp)
	{
		size_t size = sis_file_read(fp, (char *)value, len);
		sis_file_close(fp);
		return size;
	}
	return 0;
}
// void sis_out_binary(const char *key_, const char *val_, size_t len_)
// {
// 	printf("%s : %d ==> \n", key_, (int)len_);
// 	size_t i = 0;
// 	size_t j;
// 	while (i < len_)
// 	{
// 		uint8 val = val_[i] & 0xff;
// 		if (i % 8 == 0 && i > 0)
// 		{
// 			printf(" ");
// 		}
// 		if (i % 16 == 0)
// 		{
// 			if (i > 0)
// 			{
// 				printf(" ");
// 				for (j = i - 16; j < i; j++)
// 				{
// 					int vv = val_[j];
// 					if (vv > 0x20 && vv < 0x7e)
// 					{
// 						printf("%c", vv);
// 					}
// 					else
// 					{
// 						printf(".");
// 					}
// 				}
// 				printf("\n");
// 			}
// 			printf("%.8lu: ", i);
// 		}
// 		printf("%.2x ", val);
// 		i++;
// 	}
// 	if (len_ % 16 != 0)
// 	{
// 		for (j = 0; j < (16 - (len_ % 16)); j++)
// 		{
// 			printf("   ");
// 		}
// 		printf("  ");
// 		if (len_ % 16 <= 8)
// 			printf(" ");
// 		for (j = i - (len_ % 16); j < i; j++)
// 		{
// 			int vv = val_[j];
// 			if (vv > 0x20 && vv < 0x7e)
// 			{
// 				printf("%c", vv);
// 			}
// 			else
// 			{
// 				printf(".");
// 			}
// 		}
// 	}
// 	printf("\n");
// }
void sis_out_binary(const char *key_, const char *val_, size_t len_)
{
	printf("%s : %d ==> \n", key_, (int)len_);
	size_t i = 0;
	size_t k = 0;
	while (i < len_)
	{
		uint8 val = val_[i] & 0xff;
		if (i % 8 == 0 && i > 0)
		{
			printf(" ");
		}
		if (i % 16 == 0)
		{
			if (i > 0)
			{
				printf(" ");
				for (int j = i - 16; j < i; j++)
				{
					int vv = val_[j];
					if (vv > 0x20 && vv < 0x7e)
					{
						printf("%c", vv);
					}
					else
					{
						printf(".");
					}
				}
				k += 16;
				printf("\n");
			}
			printf("%.8lu: ", i);
		}
		printf("%.2x ", val);
		i++;
	}
	if (k < len_)
	{
		for (int j = 0; j < (16 - len_ + k) % 16 ; j++)
		{
			printf("   ");
		}
		printf("  ");
		for (int j = k; j < len_; j++)
		{
			int vv = val_[j];
			if (vv > 0x20 && vv < 0x7e)
			{
				printf("%c", vv);
			}
			else
			{
				printf(".");
			}
		}
	}
	printf("\n");
}
#if 0

// inline void why_me()
// {
//     printf( "This function is %s\n", __func__ );
//     printf( "The file is %s.\n", __FILE__ );
//     printf( "This is line %d.\n", __LINE__ );

// }
#define why_me()                                   \
	do                                             \
	{                                              \
		printf("This function is %s\n", __func__); \
		printf("The file is %s.\n", __FILE__);     \
		printf("This is line %d.\n", __LINE__);    \
	} while (0)
int main()
{
	printf("The file is %s.\n", __FILE__);
	printf("The date is %s.\n", __DATE__);
	printf("The time is %s.\n", __TIME__);
	printf("This is line %d.\n", __LINE__);
	printf("This function is %s.\n", __func__);

	why_me();

	// LOGSCR(CLR_RED, "%s\n", "my is CLR_RED.");
	// LOGSCR(CLR_GREEN, "%s\n", "my is CLR_GREEN.");
	// LOGSCR(CLR_BLUE, "%s\n", "my is CLR_BLUE.");
	// LOGSCR(CLR_GOLD, "%s\n", "my is CLR_GOLD.");

	// LOGSCR(CLR_LRED, "%s\n", "my is CLR_LRED.");
	// LOGSCR(CLR_LGREEN, "%s\n", "my is CLR_LGREEN.");
	// LOGSCR(CLR_LBLUE, "%s\n", "my is CLR_LBLUE.");
	// LOGSCR(CLR_LHOT, "%s\n", "my is CLR_LHOT.");
	// LOGSCR(CLR_YELLOW, "%s\n", "my is CLR_YELLOW.");

	sis_log_start();
	sis_log_open("./test.log", 5, 1);
	LOG(0)("%s\n", "my is error.");
	LOG(1)("%s\n", "my is warning.");
	LOG(2)("%s\n", "my is normal.");
	sis_log_stop();

	sis_out_percent_start("install ...");
	int count = 100; //1000*1000*100;
	int step = count / 100;
	for (size_t i = 0; i < count; i++)
	{
		if (i % step == 0)
		{
			sis_out_percent(i, count);
		}
		sis_sleep(30);
		// char *str = sis_malloc(1000);
		// sis_free(str);
	}
	sis_out_percent_stop();

	return 0;
}

#endif
