#ifndef _SERVER_H
#define _SERVER_H

#include <sts_core.h>
#include <sts_conf.h>

#pragma pack(push,1)

typedef struct s_digger_server
{
	bool inited; //是否已经初始化

	char conf_name[STS_FILE_PATH_LEN];  //配置文件路径
	char conf_path[STS_FILE_PATH_LEN];  //配置文件路径
	s_sts_conf_handle *config;  // 配置文件句柄

	int id; 
}s_digger_server;

#pragma pack(pop)

#endif