
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISDB_FILE_H
#define _SISDB_FILE_H

#include "sis_core.h"
#include "sis_conf.h"
#include "sis_str.h"

#include "os_file.h"

#include "sisdb.h"
#include "sisdb_table.h"
#include "sisdb_collect.h"
#include "sisdb_io.h"
#include "sis_memory.h"

#define SIS_DB_FILE_VERSION   1    // 数据结构版本

#define SIS_DB_FILE_CONF "%s/%s.json"    // 数据结构定义
#define SIS_DB_FILE_MAIN "%s/%s.sdb"   // 主数据库
#define SIS_DB_FILE_ZERO "%s/%s.sdb.0" // 不完备的数据块
#define SIS_DB_FILE_AOF  "%s/%s.aof"     // 顺序写入

#define SIS_DB_FILE_OUT_CSV  	"%s/%s/%s/%s.csv"    // 主数据库 db/sisdb/sh600600/day.csv
#define SIS_DB_FILE_OUT_JSON  	"%s/%s/%s/%s.json"   // 主数据库 db/sisdb/sh600600/day.json
#define SIS_DB_FILE_OUT_ARRAY   "%s/%s/%s/%s.arr"   // 主数据库 db/sisdb/sh600600/day.json
#define SIS_DB_FILE_OUT_STRUCT  "%s/%s/%s/%s.bin"    // 主数据库 db/sisdb/sh600600/day.bin
#define SIS_DB_FILE_OUT_ZIP  	"%s/%s/%s/%s.zip"    // 主数据库 db/sisdb/sh600600/day.zip

#pragma pack(push,1)
typedef struct s_sis_sdb_head{
	uint32	size;    // 数据大小
	char    key[SIS_MAXLEN_KEY];
	uint8	format;  // 数据格式 json array struct zip 
                     // 还有一个数据结构是 refer 存储 catch 和 zip 三条记录
}s_sis_sdb_head;

typedef struct s_sis_aof_head{
	uint32	size;    // 数据大小
	char    key[SIS_MAXLEN_KEY];
	uint8	format;  // 数据格式 json array struct zip 
	                 // 还有一个数据结构是 refer 存储 catch 和 zip 三条记录
}s_sis_aof_head;

#pragma pack(pop)

//------v1.0采用全部存盘的策略 ---------//
// 到时间保存
bool sisdb_file_save(s_sisdb_server *server_);

bool sisdb_file_out(s_sisdb_server *server_, const char * key_,const char *com_);

bool sisdb_file_save_aof(s_sisdb_server *server_, 
            int fmt_, const char *key_, 
            const char *val_, size_t len_);

bool sisdb_file_save_conf(s_sisdb_server *server_);
// 开机时加载数据
bool sisdb_file_load(s_sisdb_server *server_);

// 检查本地配置和数据库存盘的结构是否一致，如果不一致返回错误
// bool sisdb_file_check(const char *dbpath_, s_sis_db *db_);

// bool sisdb_file_save_json(const char *service_, const char *db_);
// bool sisdb_file_load_json(const char *service_, const char *db_);

#endif  /* _SISDB_FILE_H */
