
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _STS_DB_IO_H
#define _STS_DB_IO_H

#include "sts_core.h"
#include "sts_comm.h"
#include "sts_conf.h"
#include "sts_str.h"

#include "sts_table.h"


#define STS_SERVER_STATUS_NOINIT    0
#define STS_SERVER_STATUS_INITED    1
#define STS_SERVER_STATUS_LOADED    2

#pragma pack(push,1)

typedef struct s_stsdb_server
{
	int status; //是否已经初始化 0 没有初始化

	char conf_name[STS_FILE_PATH_LEN];  //配置文件路径
	char conf_path[STS_FILE_PATH_LEN];  //配置文件路径
	s_sts_conf_handle *config;  // 配置文件句柄

}s_stsdb_server;

#pragma pack(pop)

int stsdb_init(const char *conf_);

int stsdb_start(s_sts_module_context *ctx_);

int stsdb_get(s_sts_module_context *ctx_,const char *db_, const char *key_, const char *com_);

int stsdb_set_json(s_sts_module_context *ctx_,const char *db_, const char *key_, const char *val_);
int stsdb_set_struct(s_sts_module_context *ctx_,const char *db_, const char *key_, const char *val_);




#endif  /* _STS_DB_IO_H */
