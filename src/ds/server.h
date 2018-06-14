#ifndef _STOCKS_H
#define _STOCKS_H

#include <sts_core.h>
#include <sts_conf.h>
#include <sts_list.h>
#include <sts_time.h>

#include <os_fork.h>
#include <os_thread.h>

#include <worker.h>

#define STOCK_WORK_MODE_NORMAL  0
#define STOCK_WORK_MODE_DEBUG   1
#define STOCK_WORK_MODE_SINGLE  2

#pragma pack(push,1)

typedef struct s_stock_server
{
	volatile int status; //是否已经初始化
	
	char conf_name[STS_PATH_LEN];  //配置文件路径

	int    loglevel;
	size_t logsize;
	char   logpath[STS_PATH_LEN];   //log路径
	char   logname[STS_PATH_LEN];   //log路径

	s_sts_conf_handle *config;  // 配置文件句柄

	int  work_mode;                 // 以什么模式来运行程序
	char single_name[STS_NAME_LEN]; // 单任务运行时的名字

	s_sts_struct_list *workers;  // 总共启动的工作线程列表

}s_stock_server;

#pragma pack(pop)


#endif