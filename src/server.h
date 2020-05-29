#ifndef _SERVER_H
#define _SERVER_H

#include <sis_core.h>
#include <sis_conf.h>
#include <sis_list.h>
#include <sis_time.h>
#include <sis_map.h>
#include <os_fork.h>
#include <sis_thread.h>

#include <sis_modules.h>

#define SERVER_WORK_MODE_NORMAL  0
#define SERVER_WORK_MODE_DEBUG   1

#pragma pack(push,1)

typedef struct s_sis_server
{
	volatile int status; //是否已经初始化
	
	char conf_name[SIS_PATH_LEN];  //配置文件路径

	size_t log_size;
	int    log_level;
	bool   log_screen;
	char   log_path[SIS_PATH_LEN];   // log路径
	char   log_name[SIS_PATH_LEN];    // log文件名，通常以执行文件名为名

	s_sis_conf_handle *config;       // 配置文件句柄

	// int  work_series;                // 序列号
	int  work_mode;                  // 以什么模式来运行程序

	s_sis_map_pointer  *workers;     // 总共启动的工作线程列表
	s_sis_map_pointer  *modules;     // 总共加载的插件，按名字来检索

}s_sis_server;

#pragma pack(pop)
#ifdef __cplusplus
extern "C" {
#endif

bool sis_is_server(void *);

s_sis_server *sis_get_server();

s_sis_modules *sis_get_worker_slot(const char *);

#ifdef __cplusplus
}
#endif

#define SERVER_FAST_EXIT { if(sis_get_server()->status == SIS_SERVER_STATUS_CLOSE) break; }
// 发生一个异步请求时，需要堵塞的位置，需要判断程序是否结束以便快速退出
#define SIS_LONG_WAIT(_a_) ({ while(!(_a_)) { sis_sleep(30); SERVER_FAST_EXIT }})

#endif