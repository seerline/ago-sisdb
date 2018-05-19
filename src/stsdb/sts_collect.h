
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _STS_COLLECT_H
#define _STS_COLLECT_H

#include "sts_core.h"
#include "sds.h"
#include "sts_math.h"
// #include "zmalloc.h"
// #include "sdsalloc.h"

#include "sts_table.h"

/////////////////////////////////////////////////////////
//  数据库数据搜索模式
/////////////////////////////////////////////////////////
#define STS_SEARCH_NONE     -1  // 没有数据符合条件
#define STS_SEARCH_NEAR      1  // 附近的数据
#define STS_SEARCH_LEFT      2  // 附近的数据
#define STS_SEARCH_RIGHT     3  // 附近的数据
#define STS_SEARCH_OK        0  // 准确匹配的数据

#define STS_JSON_KEY_ARRAY    ("value")   
#define STS_JSON_KEY_ARRAYS   ("values")   
#define STS_JSON_KEY_GROUPS   ("groups")   
#define STS_JSON_KEY_FIELDS   ("fields")   

#pragma pack(push,1)
// 根据结构化数组的时间序列，自动生成头尾时间，和平均间隔时间
typedef struct s_sts_step_index {
	uint64 left;       // 最小时间
	uint64 right;   
	int count;
	uint64 step;       // 间隔时间，每条记录大约间隔时间，
}s_sts_step_index;

// 单个股票的数据包
typedef struct s_sts_collect_unit{
	s_sts_table        *father;  // 表的指针，可以获得字段定义的相关信息
	s_sts_step_index   *stepinfo;    // 时间索引表，这里会保存时间序列key，每条记录的指针(不申请内存)，
	s_sts_struct_list  *value;   // 结构化数据
}s_sts_collect_unit;

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

int	sts_collect_unit_recs(s_sts_collect_unit *unit_);
int sts_collect_unit_search(s_sts_collect_unit *unit_, uint64 index_);
int sts_collect_unit_search_left(s_sts_collect_unit *unit_, uint64 index_, int *mode_);
int sts_collect_unit_search_right(s_sts_collect_unit *unit_, uint64 index_, int *mode_);

int	sts_collect_unit_delete_of_range(s_sts_collect_unit *, int start_, int stop_); // 定位后删除
int	sts_collect_unit_delete_of_count(s_sts_collect_unit *, int start_, int count_); // 定位后删除

sds sts_collect_unit_get_of_range_m(s_sts_collect_unit *, int start_, int stop_);
sds sts_collect_unit_get_of_count_m(s_sts_collect_unit *, int start_, int count_);

int sts_collect_unit_update(s_sts_collect_unit *, const char *in_, size_t ilen_);

//传入json数据时通过该函数转成二进制结构数据
sds sts_collect_json_to_struct(s_sts_collect_unit *, const char *in_, size_t ilen_);

//传入array数据时通过该函数转成二进制结构数据
sds sts_collect_array_to_struct(s_sts_collect_unit *, const char *in_, size_t ilen_);

//输出数据时，把二进制结构数据转换成json格式数据，或者array的数据，json 数据要求带fields结构
sds sts_collect_struct_filter(s_sts_collect_unit *unit_, sds in_, const char *fields_);
sds sts_collect_struct_to_json(s_sts_collect_unit *unit_, sds in_, const char *fields_);
sds sts_collect_struct_to_array(s_sts_collect_unit *unit_, sds in_, const char *fields_);

#endif  /* _STS_COLLECT_H */
