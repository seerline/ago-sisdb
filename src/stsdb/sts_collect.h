
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _STS_COLLECT_H
#define _STS_COLLECT_H

#include "sts_core.h"
#include "sts_math.h"

#include "sts_malloc.h"

#include "sts_table.h"

/////////////////////////////////////////////////////////
//  数据库数据搜索模式
/////////////////////////////////////////////////////////
#define STS_SEARCH_NONE -1 // 没有数据符合条件
#define STS_SEARCH_NEAR  1  // 附近的数据
#define STS_SEARCH_LEFT  2  // 附近的数据
#define STS_SEARCH_RIGHT 3  // 附近的数据
#define STS_SEARCH_OK    0	// 准确匹配的数据

#define STS_SEARCH_CHECK_INIT  0  // 当日最新记录，先简单按日期来判定
#define STS_SEARCH_CHECK_NEW   1   // 当日新增记录，新来的时间大于最后一条记录
#define STS_SEARCH_CHECK_OLD   2   // 记录，新来的时间小于最后一条记录
#define STS_SEARCH_CHECK_OK    3   // 等于最后一条记录
#define STS_SEARCH_CHECK_ERROR 4 // 错误，不处理

#define STS_JSON_KEY_ARRAY ("value")   // 获取一个股票一条数据
#define STS_JSON_KEY_ARRAYS ("values") //获取一个股票多个数据
#define STS_JSON_KEY_GROUPS ("groups")  //获取多个股票数据
#define STS_JSON_KEY_FIELDS ("fields")  // 字段定义
#define STS_JSON_KEY_COLLECTS ("collects") // 获取一个db的所有股票代码

#pragma pack(push, 1)
// 根据结构化数组的时间序列，自动生成头尾时间，和平均间隔时间
typedef struct s_sts_step_index
{
	uint32 count;
	uint64 left; // 最小时间
	uint64 right;
	double step; // 间隔时间，每条记录大约间隔时间，
} s_sts_step_index;

// 单个股票的数据包
typedef struct s_sts_collect_unit
{
	s_sts_table *father;		// 表的指针，可以获得字段定义的相关信息
	s_sts_step_index *stepinfo; // 时间索引表，这里会保存时间序列key，每条记录的指针(不申请内存)，
	s_sts_struct_list *value;   // 结构化数据

	s_sts_sds front;  // 前一分钟的记录 catch=true生效 vol为全量 -- 存盘时一定要保存
	s_sts_sds lasted; // 当前那一分钟的记录 catch=true生效 vol为全量 -- 存盘时一定要保存

	s_sts_sds refer;  // 实际数据的前一条参考数据 zip 时生效 vol为当量 -- 需要保存
	// s_sts_sds moved;  // 当前数据，直接从数据区获取
} s_sts_collect_unit;

#pragma pack(pop)

///////////////////////////////////////////////////////////////////////////
//------------------------s_sts_step_index --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sts_step_index *sts_stepindex_create();
void sts_stepindex_destroy(s_sts_step_index *);
void sts_stepindex_rebuild(s_sts_step_index *, uint64 left_, uint64 right_, int count_);

///////////////////////////////////////////////////////////////////////////
//------------------------s_sts_collect_unit --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sts_collect_unit *sts_collect_unit_create(s_sts_table *tb_, const char *key_);
void sts_collect_unit_destroy(s_sts_collect_unit *);

uint64 sts_collect_unit_get_time(s_sts_collect_unit *unit_, int index_);

int sts_collect_unit_recs(s_sts_collect_unit *unit_);
int sts_collect_unit_search(s_sts_collect_unit *unit_, uint64 index_);
//检查是否增加记录，只和最后一条记录做比较，返回3个，一是当日最新记录，一是新记录，一是老记录
int sts_collect_unit_search_check(s_sts_collect_unit *unit_, uint64 index_);
int sts_collect_unit_search_left(s_sts_collect_unit *unit_, uint64 index_, int *mode_);
int sts_collect_unit_search_right(s_sts_collect_unit *unit_, uint64 index_, int *mode_);

int sts_collect_unit_delete_of_range(s_sts_collect_unit *, int start_, int stop_);  // 定位后删除
int sts_collect_unit_delete_of_count(s_sts_collect_unit *, int start_, int count_); // 定位后删除

s_sts_sds sts_collect_unit_get_of_range_sds(s_sts_collect_unit *, int start_, int stop_);
s_sts_sds sts_collect_unit_get_of_count_sds(s_sts_collect_unit *, int start_, int count_);
//  按股票代码直接从表中获取一组数据
s_sts_sds sts_table_get_of_range_sds(s_sts_table *tb_, const char *code_, int start_, int stop_);

int sts_collect_unit_update(s_sts_collect_unit *, const char *in_, size_t ilen_);
// 从磁盘加载，整块写入，
int sts_collect_unit_update_block(s_sts_collect_unit *, const char *in_, size_t ilen_);

//传入json数据时通过该函数转成二进制结构数据
s_sts_sds sts_collect_json_to_struct_sds(s_sts_collect_unit *, const char *in_, size_t ilen_);

//传入array数据时通过该函数转成二进制结构数据
s_sts_sds sts_collect_array_to_struct_sds(s_sts_collect_unit *, const char *in_, size_t ilen_);

//输出数据时，把二进制结构数据转换成json格式数据，或者array的数据，json 数据要求带fields结构
s_sts_sds sts_collect_struct_filter_sds(s_sts_collect_unit *unit_, s_sts_sds in_, const char *fields_);
s_sts_sds sts_collect_struct_to_json_sds(s_sts_collect_unit *unit_, s_sts_sds in_, const char *fields_);
s_sts_sds sts_collect_struct_to_array_sds(s_sts_collect_unit *unit_, s_sts_sds in_, const char *fields_);

void sts_collect_struct_trans(s_sts_sds ins_, s_sts_field_unit *infu_, s_sts_table *indb_, s_sts_sds outs_, s_sts_field_unit *outfu_, s_sts_table *outdb_);
// void sts_collect_struct_trans_incr(s_sts_sds ins_,s_sts_sds dbs_, s_sts_field_unit *infu_, s_sts_table *indb_, s_sts_sds outs_, s_sts_field_unit *outfu_,s_sts_table *outdb_);

bool sts_trans_of_count(s_sts_collect_unit *unit_, int *start_, int *count_);
bool sts_trans_of_range(s_sts_collect_unit *unit_, int *start_, int *stop_);


#endif /* _STS_COLLECT_H */
