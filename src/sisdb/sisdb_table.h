
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISDB_TABLE_H
#define _SISDB_TABLE_H

#include "sis_core.h"
#include "sis_list.h"
#include "sis_json.h"
#include "sis_time.h"

#include "sisdb.h"

#pragma pack(push,1)

typedef struct s_sisdb_table_control {
	uint8  type;         // 数据表类型 目前没什么用
	uint8  scale;        // 时序压缩的步长
	uint32 limits;       // 每个collection的最大记录数

	uint8  issys;        // 具备system的数据表，具备优先存储的特性，并且不使用info指针
	uint8  isinit;       // 是否需要初始化， 开盘时间到需要清理这个表
	uint8  issubs;       // 是否对collect建立 sub 缓存 catch；
	uint8  iszip; 		 // 数据表是否压缩存储
}s_sisdb_table_control;

// #define SIS_TABLE_LINK_COVER  0
// #define SIS_TABLE_LINK_INCR   1

typedef struct s_sisdb_table {
	uint32    version;      		     // 数据表的版本号time_t格式
	s_sis_sds name;                      // 表的名字
	s_sis_db *father;                    // 数据库的指针，在install表格时赋值
	uint16    append_method;             // 插入数据方式
	s_sisdb_table_control control;       // 表控制定义
	s_sis_string_list  *publishs;        // 当修改本数据表时，同时需要修改的其他数据表
	s_sis_string_list  *field_name;      // 按顺序排的名字
	s_sis_map_pointer  *field_map;       // 字段定义字典表，按字段名存储的字段内存块，指向 s_sisdb_field
	s_sis_string_list  *collect_list;		 // 仅仅当iscfg为真时，把collect的key在创建时新串push
}s_sisdb_table;

#pragma pack(pop)

s_sisdb_table *sisdb_table_create(s_sis_db *db_,const char *name_, s_sis_json_node *command);  //command为一个json格式字段定义
void sisdb_table_destroy(s_sisdb_table *);  //删除一个表

s_sisdb_table *sisdb_get_table(s_sis_db *db_, const char *dbname_);
s_sisdb_table *sisdb_get_table_from_key(s_sis_db *db_, const char *key_);

int sisdb_table_set_fields(s_sis_db *db_,s_sisdb_table *, s_sis_json_node *fields_); //command为一个json格式字段定义
// 返回记录的长度
int sisdb_table_get_fields_size(s_sisdb_table *);
// 

#endif  /* _SIS_TABLE_H */
