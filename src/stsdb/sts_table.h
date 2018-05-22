
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _STS_TABLE_H
#define _STS_TABLE_H

#include "sts_core.h"
// #include "dict.h"
// #include "sts_map.h"
#include "sts_list.h"
#include "sts_json.h"
// #include "sts_math.h"
#include "sts_time.h"

#include "sts_fields.h"

/////////////////////////////////////////////////////////
//  数据库数据插入和修改模式
/////////////////////////////////////////////////////////
#define STS_OPTION_ALWAYS      0  // 不做判断直接追加
#define STS_OPTION_TIME        1  // 检查时间节点重复就覆盖老的数据，不重复就放对应位置
#define STS_OPTION_VOL         2  // 检查成交量，成交量增加就写入
#define STS_OPTION_NONE        3  // 不做判断直接追加
#define STS_OPTION_MUL_CHECK   4  // 最少3个以上数据才能确认数据的准确性，暂不用

/////////////////////////////////////////////////////////
//  数据格是定义 
/////////////////////////////////////////////////////////
#define STS_DATA_ZIP     'R'   // 后面开始为数据 压缩格式，头部应该有字段说明
#define STS_DATA_STRUCT  'B'   // 后面开始为数据 二进制结构化数据
#define STS_DATA_STRING  'S'   // 后面开始为数据 字符串
#define STS_DATA_JSON    '{'   // 直接传数据  json文档 数组可能消失，利用 groups 表示多支股票的数据
							   // value 表示最新的一维数组  values 表示二维数组	
#define STS_DATA_ARRAY   '['   // 直接传数据

#pragma pack(push,1)

typedef struct s_sts_db s_sts_db;

typedef struct s_sts_table_control {
	uint32 version;      // 数据表的版本号time_t格式
	uint8  data_type;    // 数据类型 目前没什么用
	uint8  time_scale;   // 时序压缩的步长
	uint32 limit_rows;   // 每个collection的最大记录数
	uint8  insert_mode;  // 插入数据方式
	uint8  update_mode;  // 修改数据方式
}s_sts_table_control;

#define STS_TABLE_LINK_COVER  0
#define STS_TABLE_LINK_INCR   1

typedef struct s_sts_table {
	sds name;            //表的名字
	s_sts_db *father;            //数据库的指针，在install表格时赋值
	s_sts_table_control control;       // 表控制定义
	s_sts_string_list  *links;         // 当修改本数据表时，同时需要修改的其他数据表
	s_sts_string_list  *field_name;      // 按顺序排的名字
	s_sts_map_pointer  *field_map;       // 字段定义字典表，按字段名存储的字段内存块，指向sts_field_unit
	s_sts_map_pointer  *collect_map;     // 数据定义字典表，按股票名存储的数据内存块，指向sts_collect_unit

	bool catch;   // 是否对collect建立catch；

}s_sts_table;

#pragma pack(pop)

s_sts_table *sts_table_create(s_sts_db *db_,const char *name_, s_sts_json_node *command);  //command为一个json格式字段定义
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

void sts_table_destroy(s_sts_table *);  //删除一个表
void sts_table_clear(s_sts_table *);    //清理一个表的所有数据
//对数据库的各种属性设置
void sts_table_set_ver(s_sts_table *, uint32);  // time_t格式
void sts_table_set_limit_rows(s_sts_table *, uint32); // 0 -- 不限制  1 -- 只保留最新的一条  n 
void sts_table_set_insert_mode(s_sts_table *, uint8_t); // 1 -- 判断后修改 0 2

void sts_table_set_fields(s_sts_table *, s_sts_json_node *fields_); //command为一个json格式字段定义
//得到记录的长度
int sts_table_get_fields_size(s_sts_table *);

// 获取时间序列,默认为第一个字段，若第一个字段不符合标准，往下找

uint64 sts_table_struct_trans_time(uint64 in_, int inscale_, s_sts_table *out_tb_, int outscale_);

// 一个数据同时写多个库
int sts_table_update_mul(int type_, s_sts_table *, const char *key_, const char *in_, size_t ilen_);

// 来源数据是json或者struct，table是struct数据
int sts_table_update(int type_, s_sts_table *, const char *key_, const char * in_, size_t ilen_);
//修改数据，key_为股票代码或市场编号，value_为二进制结构化数据或json数据
sds sts_table_get_m(s_sts_table *, const char *key_, const char *command);  //返回数据需要释放

// command为json命令
//读表中代码为key的数据，key为*表示所有股票数据，由command定义数据范围和字段范围
//用户传入的command中关键字的定义如下：
//返回数据格式："format":"json" --> STS_DATA_JSON
//						 "array" --> STS_DATA_ARRAY
//						 "bin" --> STS_DATA_STRUCT  ----> 默认
//					     "string" --> STS_DATA_STRING
//						 "zip" --> STS_DATA_ZIP
//字段：    "fields":  "time,close,vol,name" 表示一共4个字段  
//	
//                      空,*---->表示全部字段
//---------<以下区域没有表示全部数据>--------
//数据范围："search":   min,max 按时序索引取数据
//						count(和max互斥，正表示向后，负表示向前),
//						force为0表示按实际取，为1若无数据就取min前一个数据，
//数据范围："range":    start，stop 按记录号取数据 0，-1-->表示全部数据
//						count(和stop互斥，正表示向后，负表示向前),

int sts_table_delete(s_sts_table *, const char *key_, const char *command);// command为json命令
//删除
//用户传入的command中关键字的定义如下：
//数据范围："search":   min,max 按时序索引取数据
//						count(和max互斥，正表示向后，负表示向前),
//数据范围："range":    start，stop 按记录号取数据 0，-1-->表示全部数据
//						count(和stop互斥，正表示向后，负表示向前),

#endif  /* _STS_TABLE_H */
