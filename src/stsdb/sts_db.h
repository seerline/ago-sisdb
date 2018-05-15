
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _STS_DB_H
#define _STS_DB_H

#include "lw_map.h"
#include "sts_table.h"

//#define STS_DB_DATETYPE_STR "json,bin,zip,array" // 
#define STS_DB_DATETYPE_STR "json,struct" // 

typedef struct sts_db_node {
    sts_table *table; 
    struct sts_db_node *next;
}sts_db_node;

typedef struct sts_db {
    struct sts_db_node *head;
    size_t len; /* how many table. */
}sts_db;

#pragma pack(push,1)
typedef struct sts_map_define{
	const char *key;
	uint16_t	uid;
	uint32_t    size;
}sts_map_define;
#pragma pack(pop)

#define sts_db s_map_pointer 

#define STATIC_STS_DB

#ifdef STATIC_STS_DB
// extern sts_db *__sys_sts_db;
void create_sts_db();  
void destroy_sts_db();  
//取数据和写数据
sts_table *sts_db_get_table(const char *name_); // 由此函数判断数据库是否有指定表
void sts_db_install_table(sts_table *);

sts_map_define *sts_db_find_map_define(const char *name_);

#else
sts_db *create_sts_db();  //数据库的名称，为空建立一个sys的数据库名
void destroy_sts_db(sts_db *);  //关闭一个数据库
//取数据和写数据
sts_table *sts_db_get_table(sts_db *, const char *name_); //name -- table name
void sts_db_install_table(sts_db *, sts_table *); //装入表
#endif



#endif  /* _STS_DB_H */
