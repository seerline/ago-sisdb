
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

	uint8  isinit;       // 是否需要初始化， 开盘时间到需要清理这个表
	uint8  issubs;       // 是否对collect建立 sub 缓存 catch；
	uint8  iszip; 		 // 数据表是否压缩存储
	uint8  isloading;    // 是否磁盘加载中，是的话就不做分发
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

}s_sisdb_table;

#pragma pack(pop)

s_sisdb_table *sisdb_table_create(s_sis_db *db_,const char *name_, s_sis_json_node *command);  //command为一个json格式字段定义
void sisdb_table_destroy(s_sisdb_table *);  //删除一个表

int sisdb_table_set_fields(s_sisdb_table *, s_sis_json_node *fields_); //command为一个json格式字段定义
// 返回记录的长度
int sisdb_table_get_fields_size(s_sisdb_table *);
// 

// int sisdb_table_update_publish(s_sisdb_table *table_,const char *key_, 
// 	s_sisdb_collect *collect_,s_sis_sds val_)

// 获取时间序列,默认为第一个字段，若第一个字段不符合标准，往下找
// uint64 sisdb_table_struct_trans_time(uint64 in_, int inscale_, s_sisdb_table *out_tb_, int outscale_);

// 一个数据同时写多个库
// int sisdb_table_update_and_pubs(int type_, s_sisdb_table *, const char *key_, const char *in_, size_t ilen_);
// //  直接从库里加载，需要整块无校验加载，加快加载速度
// int sisdb_table_update_load(int type_, s_sisdb_table *table_, const char *key_, const char *in_, size_t ilen_);

// // 来源数据是json或者struct，table是struct数据
// // int sisdb_table_update(int type_, s_sisdb_table *, const char *key_, const char * in_, size_t ilen_);
// //修改数据，key_为股票代码或市场编号，value_为二进制结构化数据或json数据
// s_sis_sds sisdb_table_get_sds(s_sisdb_table *, const char *key_, const char *com_);  //返回数据需要释放
// s_sis_sds sisdb_table_get_code_sds(s_sisdb_table *, const char *key_, const char *com_);  //返回数据需要释放

// s_sis_sds sisdb_table_get_search_sds(s_sisdb_table *tb_, const char *code_, int min_,int max_);


#endif  /* _SIS_TABLE_H */
