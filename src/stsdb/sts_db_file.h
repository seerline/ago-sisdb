
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _STS_DB_FILE_H
#define _STS_DB_FILE_H

#include "sts_core.h"
#include "sts_conf.h"
#include "sts_str.h"
#include "sts_file.h"

#include "sts_db.h"
#include "sts_table.h"

#define STS_DB_FILE_CONF "%s.json"    // 数据结构定义
#define STS_DB_FILE_MAIN "%s.sdb"     // 主数据库
#define STS_DB_FILE_TEMP "%s.sdb.0"  // 已经完备的数据快 -- 暂时用不到
#define STS_DB_FILE_AOF  "%s.aof"     // 顺序写入

#pragma pack(push,1)

#pragma pack(pop)

//------v1.0采用全部存盘的策略 ---------//
// 到时间保存
bool sts_db_file_save(const char *dbpath_, s_sts_db *db_);
bool sts_db_file_save_aof(const char *dbpath_, s_sts_db *db_);
// 开机时加载数据
bool sts_db_file_load(const char *dbpath_, s_sts_db *db_);

// 检查本地配置和数据库存盘的结构是否一致，如果不一致返回错误
bool sts_db_file_check(const char *dbpath_, s_sts_db *db_);

// bool stsdb_file_save_json(const char *service_, const char *db_);
// bool stsdb_file_load_json(const char *service_, const char *db_);

#endif  /* _STS_DB_FILE_H */
