
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SIS_DB_IO_H
#define _SIS_DB_IO_H

#include "sis_core.h"
#include "sis_conf.h"
#include "sis_str.h"
#include "sis_file.h"

#include "sis_db.h"
#include "sis_table.h"
#include "sis_db_file.h"

#pragma pack(push,1)

typedef struct s_sisdb_server
{
	int status; //是否已经初始化 0 没有初始化

	char conf_name[SIS_PATH_LEN];  //配置文件路径

	char dbpath[SIS_PATH_LEN];    //数据库路径

	int    loglevel;
	size_t logsize;
	char   logpath[SIS_PATH_LEN];   //log路径

	s_sis_conf_handle *config;  // 配置文件句柄

	s_sis_db *db;  // 数据库句柄

	char service_name[SIS_TABLE_MAXLEN];  //服务名

}s_sisdb_server;

#pragma pack(pop)

char * sisdb_open(const char *conf_);

int sisdb_init(const char *market_);

s_sis_sds sisdb_list_sds();

void sisdb_close();
bool sisdb_save();
bool sisdb_saveto(const char * dt_, const char *db_);

s_sis_sds sisdb_get_sds(const char *db_, const char *key_, const char *com_);

int sisdb_set(const char *dt_, const char *db_, const char *key_, const char *val_, size_t len_);
int sisdb_set_format(int format_, const char *db_, const char *key_, const char *val_, size_t len_);


#endif  /* _SIS_DB_IO_H */
