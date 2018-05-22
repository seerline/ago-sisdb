
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _STS_DB_IO_H
#define _STS_DB_IO_H

#include "sts_core.h"
#include "sts_conf.h"
#include "sts_str.h"

#include "sts_db.h"
#include "sts_table.h"

#define STS_SERVER_OK        0
#define STS_SERVER_ERROR    1

#define STS_SERVER_STATUS_NOINIT    0
#define STS_SERVER_STATUS_INITED    1
#define STS_SERVER_STATUS_LOADED    2

#pragma pack(push,1)

typedef struct s_stsdb_server
{
	int status; //是否已经初始化 0 没有初始化

	char service_name[STS_STR_LEN];  //服务名
	char conf_name[STS_FILE_PATH_LEN];  //配置文件路径
	char conf_path[STS_FILE_PATH_LEN];  //配置文件路径
	s_sts_conf_handle *config;  // 配置文件句柄
	s_sts_db *db;  // 数据库句柄

}s_stsdb_server;

#pragma pack(pop)

char * stsdb_init(const char *conf_);

sds stsdb_list();

sds stsdb_get(const char *db_, const char *key_, const char *com_);

int stsdb_set(const char *dt_, const char *db_, const char *key_, const char *val_, size_t len_);


#endif  /* _STS_DB_IO_H */
