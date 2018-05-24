
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _STS_DB_H
#define _STS_DB_H

#include "sts_map.h"
#include "sts_table.h"

#define STS_MAP_DEFINE_FIELD_TYPE   0
#define STS_MAP_DEFINE_DATA_TYPE    1
#define STS_MAP_DEFINE_SCALE        2
#define STS_MAP_DEFINE_OPTION_MODE  3
#define STS_MAP_DEFINE_ZIP_MODE     4

// 数据转换只能从低到高，如果day要转min一律失败
#define STS_SCALE_NONE    0 // "NONE"  //
#define STS_SCALE_MSEC    1 // "MSEC"  //int64 格式，精确到毫秒  
#define STS_SCALE_SECOND  2 // "SECOND"  //int32 time_t格式，精确到秒  
#define STS_SCALE_MIN1    3 // "MIN1"  //int32 time_t格式，精确到1分钟
#define STS_SCALE_MIN5    4 // "MIN5"  //int32 time_t格式 ，精确到5分钟
#define STS_SCALE_MIN30   5 // "MIN30"  //int32 time_t格式，精确到半小时
#define STS_SCALE_DAY     6 // "DAY"  //int32 20170101格式，精确到天

#pragma pack(push,1)
typedef struct s_sts_map_define{
	const char *key;
	uint8	style;  // 类别
	uint16	uid;
	uint32  size;
}s_sts_map_define;

typedef struct s_sts_db{
	s_sts_map_pointer  db; // 一个字典表
	const char key;
	uint8	style;  // 类别
	uint16	uid;
	uint32  size;
}s_sts_map_define;
#pragma pack(pop)

// #define s_sts_db s_sts_map_pointer 

#define STATIC_STS_DB

#ifdef STATIC_STS_DB

void sts_db_create();  
void sts_db_destroy();  
//取数据和写数据
s_sts_table *sts_db_get_table(const char *name_); // 由此函数判断数据库是否有指定表
void sts_db_install_table(s_sts_table *);

sds sts_db_get_tables();

#else
s_sts_db *sts_db_create();  //数据库的名称，为空建立一个sys的数据库名
void sts_db_destroy(s_sts_db *);  //关闭一个数据库
//取数据和写数据
s_sts_table *sts_db_get_table(s_sts_db *, const char *name_); //name -- table name
void sts_db_install_table(s_sts_db *, s_sts_table *); //装入表

sds sts_db_get_tables(s_sts_db *);

#endif

s_sts_map_define *sts_db_find_map_define(const char *name_, uint8 style_);
int sts_db_find_map_uid(const char *name_, uint8 style_);


#endif  /* _STS_DB_H */
