
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SIS_COLLECT_H
#define _SIS_COLLECT_H

#include "sis_core.h"
#include "sis_math.h"

#include "sis_malloc.h"

#include "sis_table.h"
#include "sis_fields.h"
/////////////////////////////////////////////////////////
//  数据库数据搜索模式
/////////////////////////////////////////////////////////
#define SIS_SEARCH_NONE -1 // 没有数据符合条件
#define SIS_SEARCH_NEAR  1  // 附近的数据
#define SIS_SEARCH_LEFT  2  // 附近的数据
#define SIS_SEARCH_RIGHT 3  // 附近的数据
#define SIS_SEARCH_OK    0	// 准确匹配的数据

#define SIS_SEARCH_CHECK_INIT  0  // 当日最新记录，先简单按日期来判定
#define SIS_SEARCH_CHECK_NEW   1   // 当日新增记录，新来的时间大于最后一条记录
#define SIS_SEARCH_CHECK_OLD   2   // 记录，新来的时间小于最后一条记录
#define SIS_SEARCH_CHECK_OK    3   // 等于最后一条记录
#define SIS_SEARCH_CHECK_ERROR 4 // 错误，不处理

#define SIS_JSON_KEY_ARRAY ("value")   // 获取一个股票一条数据
#define SIS_JSON_KEY_ARRAYS ("values") //获取一个股票多个数据
#define SIS_JSON_KEY_GROUPS ("groups")  //获取多个股票数据
#define SIS_JSON_KEY_FIELDS ("fields")  // 字段定义
#define SIS_JSON_KEY_COLLECTS ("collects") // 获取一个db的所有股票代码

// #define SIS_UPDATE_MODE_NO     0   // 放弃数据
// #define SIS_UPDATE_MODE_UPDATE   1   // 修改某条记录
// #define SIS_UPDATE_MODE_INSERT   2   // 插入在某条记录之前
// #define SIS_UPDATE_MODE_OK    1   // 生产新数据

#pragma pack(push, 1)
// 根据结构化数组的时间序列，自动生成头尾时间，和平均间隔时间
typedef struct s_sis_step_index
{
	uint32 count;
	uint64 left; // 最小时间
	uint64 right;
	double step; // 间隔时间，每条记录大约间隔时间，
} s_sis_step_index;

// 单个股票的数据包
typedef struct s_sis_collect_unit
{
	s_sis_table *father;		// 表的指针，可以获得字段定义的相关信息
	s_sis_step_index *stepinfo; // 时间索引表，这里会保存时间序列key，每条记录的指针(不申请内存)，
	s_sis_struct_list *value;   // 结构化数据

	s_sis_sds front;  // 前一分钟的记录 catch=true生效 vol为全量 -- 存盘时一定要保存
	s_sis_sds lasted; // 当前那一分钟的记录 catch=true生效 vol为全量 -- 存盘时一定要保存

	s_sis_sds refer;  // 实际数据的前一条参考数据 zip 时生效 vol为当量 -- 需要保存
	// s_sis_sds moved;  // 当前数据，直接从数据区获取
} s_sis_collect_unit;

#pragma pack(pop)

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_step_index --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sis_step_index *sis_stepindex_create();
void sis_stepindex_destroy(s_sis_step_index *);
void sis_stepindex_rebuild(s_sis_step_index *, uint64 left_, uint64 right_, int count_);

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_collect_unit --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sis_collect_unit *sis_collect_unit_create(s_sis_table *tb_, const char *key_);
void sis_collect_unit_destroy(s_sis_collect_unit *);

uint64 sis_collect_unit_get_time(s_sis_collect_unit *unit_, int index_);

int sis_collect_unit_recs(s_sis_collect_unit *unit_);
int sis_collect_unit_search(s_sis_collect_unit *unit_, uint64 index_);
//检查是否增加记录，只和最后一条记录做比较，返回3个，一是当日最新记录，一是新记录，一是老记录
int sis_collect_unit_search_check(s_sis_collect_unit *unit_, uint64 index_);
int sis_collect_unit_search_left(s_sis_collect_unit *unit_, uint64 index_, int *mode_);
int sis_collect_unit_search_right(s_sis_collect_unit *unit_, uint64 index_, int *mode_);

int sis_collect_unit_delete_of_range(s_sis_collect_unit *, int start_, int stop_);  // 定位后删除
int sis_collect_unit_delete_of_count(s_sis_collect_unit *, int start_, int count_); // 定位后删除

s_sis_sds sis_collect_unit_get_of_range_sds(s_sis_collect_unit *, int start_, int stop_);
s_sis_sds sis_collect_unit_get_of_count_sds(s_sis_collect_unit *, int start_, int count_);
//  按股票代码直接从表中获取一组数据
s_sis_sds sis_table_get_of_range_sds(s_sis_table *tb_, const char *code_, int start_, int stop_);

int sis_collect_unit_update(s_sis_collect_unit *, const char *in_, size_t ilen_);
// 从磁盘加载，整块写入，
int sis_collect_unit_update_block(s_sis_collect_unit *, const char *in_, size_t ilen_);

//传入json数据时通过该函数转成二进制结构数据
s_sis_sds sis_collect_json_to_struct_sds(s_sis_collect_unit *, const char *in_, size_t ilen_);

//传入array数据时通过该函数转成二进制结构数据
s_sis_sds sis_collect_array_to_struct_sds(s_sis_collect_unit *, const char *in_, size_t ilen_);

//输出数据时，把二进制结构数据转换成json格式数据，或者array的数据，json 数据要求带fields结构
s_sis_sds sis_collect_struct_filter_sds(s_sis_collect_unit *unit_, s_sis_sds in_, const char *fields_);
s_sis_sds sis_collect_struct_to_json_sds(s_sis_collect_unit *unit_, s_sis_sds in_, const char *fields_);
s_sis_sds sis_collect_struct_to_array_sds(s_sis_collect_unit *unit_, s_sis_sds in_, const char *fields_);

void sis_collect_struct_trans(s_sis_sds ins_, s_sis_field_unit *infu_, s_sis_table *indb_, s_sis_sds outs_, s_sis_field_unit *outfu_, s_sis_table *outdb_);
// void sis_collect_struct_trans_incr(s_sis_sds ins_,s_sis_sds dbs_, s_sis_field_unit *infu_, s_sis_table *indb_, s_sis_sds outs_, s_sis_field_unit *outfu_,s_sis_table *outdb_);

bool sis_trans_of_count(s_sis_collect_unit *unit_, int *start_, int *count_);
bool sis_trans_of_range(s_sis_collect_unit *unit_, int *start_, int *stop_);


#endif /* _SIS_COLLECT_H */
