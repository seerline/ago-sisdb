
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _DIGGER_IO_H
#define _DIGGER_IO_H

#include "sts_core.h"
#include "sts_conf.h"
#include "sts_str.h"
#include "sts_file.h"
#include "sts_map.h"

#define STS_DIGGER_MAXLEN   32
#define STS_DIGGER_FILE_VERSION  1

#define STS_SERVER_OK        0
#define STS_SERVER_ERROR     1

#define STS_SERVER_STATUS_NOINIT    0
#define STS_SERVER_STATUS_INITED    1
#define STS_SERVER_STATUS_LOADED    2
#define STS_SERVER_STATUS_CLOSE     3

#pragma pack(push,1)

typedef struct s_digger_server
{
	int id;
	
	int status; //是否已经初始化 0 没有初始化

	char conf_name[STS_PATH_LEN];  //配置文件路径

	char dbpath[STS_PATH_LEN];    //数据库路径

	int    loglevel;
	size_t logsize;
	char   logpath[STS_PATH_LEN];   //log路径

	s_sts_conf_handle *config;  // 配置文件句柄

	void *context; // 数据库来源的句柄 s_sts_module_context
				  // 获取数据时，直接原样返回即可
	s_sts_map_pointer *command; // 命令集合

	char service_name[STS_DIGGER_MAXLEN];  //服务名
	char source_name[STS_DIGGER_MAXLEN];  //服务名

	s_sts_sds catch_code;   // 码表的热备份

}s_digger_server;

#pragma pack(pop)

char * digger_open(const char *conf_);
void digger_close();

s_sts_sds digger_start_sds(const char *name_, const char *com_);
int digger_stop(const char *key_);

s_sts_sds digger_get_sds(const char *db_,const char *key_, const char *com_);
int digger_set(const char *db_, const char *key_, const char *com_);

// s_sts_sds digger_sub_sds(const char *db_, const char *key_);
// int digger_pub(const char *db_, const char *key_, const char *com_);

s_sts_sds digger_call_sds(void *context_, const char *func_, const char *argv_);

s_sts_sds get_stsdb_info_sds(void *ctx_, const char *code_, const char *db_, const char *com_);

#endif  /* _DIGGER_IO_H */
