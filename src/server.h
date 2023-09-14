#ifndef _SERVER_H
#define _SERVER_H

#include <sis_core.h>
#include <sis_conf.h>
#include <sis_list.h>
#include <sis_time.h>
#include <sis_map.h>
#include <os_fork.h>
#include <sis_thread.h>
#include <worker.h>

#include <sis_modules.h>

#define SERVER_WORK_MODE_NORMAL  0
#define SERVER_WORK_MODE_DEBUG   1
#define SERVER_WORK_MODE_FACE    2
#define SERVER_WORK_MODE_DATE    4

#define SERVER_STATUS_NONE  0
#define SERVER_STATUS_WORK  1
#define SERVER_STATUS_EXIT  2

#pragma pack(push,1)

typedef struct s_sis_server
{
	int status; //是否已经初始化
	
	int  fmt_trans;    // 格式转换
	int  load_mode;    // 0 conf 1 json
	char conf_name[SIS_PATH_LEN];  // 配置文件路径
	char json_name[SIS_PATH_LEN];  // 配置文件路径

	size_t log_size;
	int    log_level;
	char   log_path[SIS_PATH_LEN];   // log路径
	char   log_name[SIS_PATH_LEN];    // log文件名，通常以执行文件名为名

	s_sis_conf_handle *conf_h;       // 配置文件句柄
	s_sis_json_handle *json_h;    

	s_sis_json_node *cfgnode;
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

int sis_server_init();

void sis_server_uninit();

#ifdef __cplusplus
}
#endif

#define SIS_SERVER_EXIT (sis_get_server()->status == SERVER_STATUS_EXIT)
// 等待条件产生 或者程序退出 
#define SIS_WAIT_OR_EXIT(_a_) do { while(!(_a_)) { sis_sleep(30); if(sis_get_server()->status == SERVER_STATUS_EXIT) break; }} while(0);

#endif