
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISDB_TABLE_H
#define _SISDB_TABLE_H

#include "sis_core.h"
// #include "dict.h"
// #include "sis_map.h"
#include "sis_list.h"
#include "sis_json.h"
// #include "sis_math.h"
#include "sis_time.h"

// #include "sisdb_fields.h"
#include "sisdb.h"


#pragma pack(push,1)

typedef struct s_sis_table_control {
	uint32 version;      // 数据表的版本号time_t格式
	uint8  data_type;    // 数据类型 目前没什么用
	uint8  time_scale;   // 时序压缩的步长
	uint32 limit_rows;   // 每个collection的最大记录数
	uint16 insert_mode;  // 插入数据方式
	// uint8  update_mode;  // 修改数据方式
	uint8  isinit;       // 是否需要初始化， 开盘时间到需要清理这个表
}s_sis_table_control;

// #define SIS_TABLE_LINK_COVER  0
// #define SIS_TABLE_LINK_INCR   1

typedef struct s_sis_table {
	s_sis_sds name;            //表的名字
	s_sis_db *father;            //数据库的指针，在install表格时赋值
	s_sis_table_control control;       // 表控制定义
	s_sis_string_list  *links;         // 当修改本数据表时，同时需要修改的其他数据表
	s_sis_string_list  *field_name;      // 按顺序排的名字
	s_sis_map_pointer  *field_map;       // 字段定义字典表，按字段名存储的字段内存块，指向sis_field
	s_sis_map_pointer  *collect_map;     // 数据定义字典表，按股票名存储的数据内存块，指向sis_collect

	bool catch;   // 是否对collect建立catch；
	bool zip; 
	bool loading;  // 是否磁盘加载中，是的话就不做分发

}s_sis_table;

#pragma pack(pop)

s_sis_table *sis_table_create(s_sis_db *db_,const char *name_, s_sis_json_node *command);  //command为一个json格式字段定义
// command为json命令
//用户传入的command中关键字的定义如下：
//字段定义：  "fields":  []
//  # 字段名| 数据类型| 长度| io 放大还是缩小| 缩放比例 zoom|压缩类型|压缩参考字段索引
//  [name, string, 16, 0, 0, 0, 0],

//记录数限制："limit":  0 记录数限制  
//时间序列的尺度  scale
//插入数据方式：  "insert":  push 不做判断直接增加 incr-time 根据时间增加增加记录，不增加就刷新老记录
	//  incr-vol 根据成交量递增来增加数据，
	//  如果limit为1，则总是修改第一条记录

void sis_table_destroy(s_sis_table *);  //删除一个表
void sis_table_collect_clear(s_sis_table *);    //清理一个表的所有数据

//取数据和写数据
s_sis_table *sisdb_get_table(s_sis_db *, const char *name_); //name -- table name

//对数据库的各种属性设置
void sis_table_set_ver(s_sis_table *, uint32);  // time_t格式
void sis_table_set_limit_rows(s_sis_table *, uint32); // 0 -- 不限制  1 -- 只保留最新的一条  n 
void sis_table_set_insert_mode(s_sis_table *, uint16); // 1 -- 判断后修改 0 2

void sis_table_set_fields(s_sis_table *, s_sis_json_node *fields_); //command为一个json格式字段定义
//得到记录的长度
int sis_table_get_fields_size(s_sis_table *);

// 获取时间序列,默认为第一个字段，若第一个字段不符合标准，往下找

uint64 sis_table_struct_trans_time(uint64 in_, int inscale_, s_sis_table *out_tb_, int outscale_);

// 一个数据同时写多个库
int sis_table_update_and_pubs(int type_, s_sis_table *, const char *key_, const char *in_, size_t ilen_);
//  直接从库里加载，需要整块无校验加载，加快加载速度
int sis_table_update_load(int type_, s_sis_table *table_, const char *key_, const char *in_, size_t ilen_);

// 来源数据是json或者struct，table是struct数据
// int sis_table_update(int type_, s_sis_table *, const char *key_, const char * in_, size_t ilen_);
//修改数据，key_为股票代码或市场编号，value_为二进制结构化数据或json数据
s_sis_sds sis_table_get_sds(s_sis_table *, const char *key_, const char *com_);  //返回数据需要释放
s_sis_sds sis_table_get_code_sds(s_sis_table *, const char *key_, const char *com_);  //返回数据需要释放
s_sis_sds sis_table_get_table_sds(s_sis_table *tb_, const char *com_);

s_sis_sds sis_table_get_collects_sds(s_sis_table *, const char *com_);  //返回数据需要释放

s_sis_sds sis_table_get_search_sds(s_sis_table *tb_, const char *code_, int min_,int max_);

// command为json命令
//读表中代码为key的数据，key为*表示所有股票数据，由command定义数据范围和字段范围
//用户传入的command中关键字的定义如下：
//返回数据格式："format":"json" --> SIS_DSIS_JSON
//						 "array" --> SIS_DATA_TYPE_ARRAY
//						 "bin" --> SIS_DATA_TYPE_STRUCT  ----> 默认
//					     "string" --> SIS_DATA_TYPE_STRING
//						 "zip" --> SIS_DATA_TYPE_ZIP
//字段：    "fields":  "time,close,vol,name" 表示一共4个字段  
//	
//                      空,*---->表示全部字段
//---------<以下区域没有表示全部数据>--------
//数据范围："search":   min,max 按时序索引取数据
//						count(和max互斥，正表示向后，负表示向前),
//						force为0表示按实际取，为1若无数据就取min前一个数据，
//数据范围："range":    start，stop 按记录号取数据 0，-1-->表示全部数据
//						count(和stop互斥，正表示向后，负表示向前),

int sis_table_delete(s_sis_table *, const char *key_, const char *command);// command为json命令
//删除
//用户传入的command中关键字的定义如下：
//数据范围："search":   min,max 按时序索引取数据
//						count(和max互斥，正表示向后，负表示向前),
//数据范围："range":    start，stop 按记录号取数据 0，-1-->表示全部数据
//						count(和stop互斥，正表示向后，负表示向前),


#endif  /* _SIS_TABLE_H */
