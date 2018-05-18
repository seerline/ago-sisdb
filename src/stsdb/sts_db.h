
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

#pragma pack(push,1)
typedef struct s_sts_map_define{
	const char *key;
	uint8	style;  // 类别
	uint16	uid;
	uint32  size;
}s_sts_map_define;
#pragma pack(pop)

#define s_sts_db s_sts_map_pointer 

#define STATIC_STS_DB

#ifdef STATIC_STS_DB

void sts_db_create();  
void sts_db_destroy();  
//取数据和写数据
s_sts_table *sts_db_get_table(const char *name_); // 由此函数判断数据库是否有指定表
void sts_db_install_table(s_sts_table *);

sds sts_db_get_tables();

s_sts_map_define *sts_db_find_map_define(const char *name_, uint8 style_);

#else
s_sts_db *sts_db_create();  //数据库的名称，为空建立一个sys的数据库名
void sts_db_destroy(s_sts_db *);  //关闭一个数据库
//取数据和写数据
s_sts_table *sts_db_get_table(s_sts_db *, const char *name_); //name -- table name
void sts_db_install_table(s_sts_db *, s_sts_table *); //装入表
#endif



#endif  /* _STS_DB_H */
