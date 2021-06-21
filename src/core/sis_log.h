
#ifndef _SIS_LOG_H
#define _SIS_LOG_H

#include <sis_os.h>
#include <os_thread.h>
#include <os_file.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// char *__ascii_logo =
// "                _._                                                  \n"
// "           _.-``__ ''-._                                             \n"
// "      _.-``    `.  `_.  ''-._           sisdb %s (%s/%d) %s bit\n"
// "  .-`` .-```.  ```\\/    _.,_ ''-._                                  \n"
// " (    '|||   ,       .-`  | `,    )     Running in %s mode\n"
// " |`-._`|||..-` __...-.``-._|'` _.-'|     Port: %d\n"
// " |    `|||_   `._    /     _.-'    |     PID: %ld\n"
// "  `-._ ||| `-._  `-./  _.-'    _.-'                                   \n"
// " |`-._`|||_    `-.__.-'    _.-'_.-'|                                  \n"
// " |    `|||_`-._        _.-'_.-'    |           www.sisdb.com          \n"
// "  `-._ ||| `-._`-.__.-'_.-'    _.-'                                   \n"
// " |`-._`|||_    `-.__.-'    _.-'_.-'|                                  \n"
// " |    `|||_`-._        _.-'_.-'    |                                  \n"
// "  `-._ ||| `-._`-.__.-'_.-'    _.-'                                   \n"
// "      `|||_    `-.__.-'    _.-'                                       \n"
// "  +-------------------     ||   `-. _.-'                                           \n"
// "  +--------------------                                               \n\n";


// LOG(0) -- 输出内存无法申请的错误，写log后直接退出程序
// LOG(1) -- 程序初始化发生的错误，写log后正常退出程序
// LOG(2..5) -- 程序运行正常应该记录的log信息
// LOG(6..9) -- 其他不太重要的信息

// DEBUG("xxx",char *,size_t) 调试程序的时候用于输出，xxx为文件名，发布版本时DEBUG被屏蔽

// printf( "This is line %d.\n", __LINE__ );
// printf( "This function is %s.\n", __func__ );


typedef struct s_sis_log {
	int  level;   //  其他小于该值的就输出
    bool outscreen; 
    // int  line;    // 当前打印的函数行
    char funcname[255];   // 当时函数名
    s_sis_thread_id_t id; // 当时的线程号

    bool          init_mutex; 
    s_sis_mutex_t mutex;  // 多线程写入时需要加锁，不然报错

    s_sis_file_handle logfp; // 文件句柄
    char   filename[255];    // 文件名为“control”是输出到屏幕
    size_t maxsize;        // 以M为单位

    int    warning;        // 警报级别 0 -红色 1 为黄色
    char   showtime[128];
    char   buffer[1024];     // 打印缓存， 不超过1024个字符，过多字符通过其他方式输出检查
}s_sis_log;

#ifdef __cplusplus
extern "C" {
#endif
void sis_log_start();   // 传递 线程号 函数名 和行号
void sis_log_stop();

void sis_log_open(const char *log_, int level_, int limit_);
void sis_log_close();
bool sis_log_input(int, s_sis_thread_id_t, const char *);  // 传递 线程号 函数名

void sis_log(const char *fmt_, ...);

#ifdef __cplusplus
}
#endif

// #define LOG(a) if(a > 1) printf

#define LOG(level) 	if (sis_log_input(level, sis_thread_self(),__func__)) sis_log

//////////////////////////////////////////
//
//////////////////////////////////////////

#define LOGSCR1(color, format)  color format RESET
#define LOGSCR(color, format, args)  printf( color format RESET, args) //打印到屏幕上

#define MOVERIGHT(y) printf("\033[%dC", (y))
#define MOVELEFT(y) printf("\033[%dD", (y)) 
#define CLRTOTAIL "\033[K"
//////////////////////////////////////////

void sis_out_percent_start(const char *info_);
void sis_out_percent(size_t step_, size_t count_);
void sis_out_percent_stop();

#ifdef __cplusplus
extern "C" {
#endif
size_t sis_writefile(const char *name, void *value, size_t len);
size_t sis_readfile(const char *name, void *value, size_t len);

void sis_out_binary(const char *key_, const char *val_, size_t len_);
#ifdef __cplusplus
}
#endif

#endif //_SIS_LOG_H