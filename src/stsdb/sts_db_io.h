
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _STS_DB_IO_H
#define _STS_DB_IO_H

#include "sts_core.h"
#include "sts_conf.h"
#include "sts_str.h"
#include "sts_file.h"

#include "sts_db.h"
#include "sts_table.h"
#include "sts_db_file.h"

#define STS_SERVER_OK        0
#define STS_SERVER_ERROR     1

#define STS_SERVER_STATUS_NOINIT    0
#define STS_SERVER_STATUS_INITED    1
#define STS_SERVER_STATUS_LOADED    2
#define STS_SERVER_STATUS_CLOSE     3

#define STS_SERVER_SAVE_NONE     0
#define STS_SERVER_SAVE_GAPS     1
#define STS_SERVER_SAVE_PLANS    2

#pragma pack(push,1)

typedef struct s_stsdb_server
{
	int status; //是否已经初始化 0 没有初始化

	char conf_name[STS_PATH_LEN];  //配置文件路径

	char dbpath[STS_PATH_LEN];    //数据库路径

	int    loglevel;
	size_t logsize;
	char   logpath[STS_PATH_LEN];   //log路径

	s_sts_conf_handle *config;  // 配置文件句柄

	s_sts_db *db;  // 数据库句柄
	char service_name[STS_TABLE_MAXLEN];  //服务名

}s_stsdb_server;

#pragma pack(pop)

char * stsdb_open(const char *conf_);

int stsdb_init(const char *market_);

s_sts_sds stsdb_list_sds();

void stsdb_close();
bool stsdb_save();

s_sts_sds stsdb_get_sds(const char *db_, const char *key_, const char *com_);

int stsdb_set(const char *dt_, const char *db_, const char *key_, const char *val_, size_t len_);
int stsdb_set_format(int format_, const char *db_, const char *key_, const char *val_, size_t len_);


#endif  /* _STS_DB_IO_H */
