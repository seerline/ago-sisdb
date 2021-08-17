
//******************************************************
// Copyright (C) 2018, Coollyer <48707400@qq.com>
//*******************************************************

#ifndef _SISDB_COLLECT_H
#define _SISDB_COLLECT_H

#include "sis_core.h"
#include "sis_math.h"
#include "sis_malloc.h"

#include "sisdb.h"
#include "sis_json.h"
#include "sis_dynamic.h"

/////////////////////////////////////////////////////////
//  数据库数据搜索模式
/////////////////////////////////////////////////////////
#define SIS_SEARCH_NONE -1  // 没有数据符合条件
#define SIS_SEARCH_OK    0	// 准确匹配的数据，时间一致
#define SIS_SEARCH_LEFT  1  // 虽然发现了数据，但返回的数据比请求查询的更早一些
#define SIS_SEARCH_RIGHT 2  // 虽然发现了数据，但返回的数据比请求查询的更晚一些

#define SIS_CHECK_LASTTIME_NEW   1   // 当日新增记录，新来的时间大于最后一条记录
#define SIS_CHECK_LASTTIME_OLD   2   // 记录，新来的时间小于最后一条记录
#define SIS_CHECK_LASTTIME_OK    3   // 等于最后一条记录
#define SIS_CHECK_LASTTIME_ERROR 4   // 错误，不处理

#define SIS_SEARCH_MIN     0x01
#define SIS_SEARCH_MAX     0x02
#define SIS_SEARCH_START   0x04
#define SIS_SEARCH_COUNT   0x08
#define SIS_SEARCH_OFFSET  0x10
// #define SIS_UPDATE_MODE_NO     0   // 放弃数据
// #define SIS_UPDATE_MODE_UPDATE   1   // 修改某条记录
// #define SIS_UPDATE_MODE_INSERT   2   // 插入在某条记录之前
// #define SIS_UPDATE_MODE_OK    1   // 生产新数据

#pragma pack(push,1)
// 根据结构化数组的时间序列，自动生成头尾时间，和平均间隔时间
typedef struct s_sis_step_index
{
	uint64 count;
	uint64 left; // 最小时间
	uint64 right;
	double step; // 间隔时间，每条记录大约间隔时间，
} s_sis_step_index;

// 单个股票的数据包
// 实际上定义了一个立体表，外部访问直接通过SH600600.DAY访问到collect，
// 而collect结构和归属分别作为两个轴，来唯一确定，这两个属性缺一不可，
// 设计上这样做的目的是快速定位目标数据，同时舍弃冗余数据，
// 

#define SISDB_COLLECT_TYPE_TABLE   0   // s_sis_struct_list
#define SISDB_COLLECT_TYPE_CHARS   1   // s_sis_sds
#define SISDB_COLLECT_TYPE_BYTES   2   // s_sis_sds

// 对于TICK类型的数据表 存储方式不同 是一个nodelist 单元包含 块号+时间区间+索引列表+数据列表 s_struct_list
// 获取数据时根据不同块号 分批按索引列表 排序后发送数据 需要独立启动一个线程 
// 历史订阅直接从 sno 中获取

typedef struct s_sisdb_collect
{
	uint8                style;  // 数据类型 SISDB_COLLECT_TYPE_TABLE ...
	s_sis_object        *obj;    // 值

	s_sis_sds            name;   // collect的值
	s_sisdb_table       *sdb;    // 数据表的指针，可以获得字段定义的相关信息

	s_sisdb_cxt         *father;
	
	s_sis_step_index    *stepinfo;    // 时间索引表，这里会保存时间序列key，每条记录的指针(不申请内存)，

} s_sisdb_collect;

#pragma pack(pop)

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_step_index --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sis_step_index *sisdb_stepindex_create();
void sisdb_stepindex_destroy(s_sis_step_index *);
void sisdb_stepindex_clear(s_sis_step_index *);
void sisdb_stepindex_rebuild(s_sis_step_index *, uint64 left_, uint64 right_, int count_);
int sisdb_stepindex_goto(s_sis_step_index *si_, uint64 curr_);

///////////////////////////////////////////////////////////////////////////
//------------------------s_sisdb_collect --------------------------------//
///////////////////////////////////////////////////////////////////////////
s_sisdb_collect *sisdb_get_collect(s_sisdb_cxt *sisdb_, const char *key_);
s_sisdb_table *sisdb_get_table(s_sisdb_cxt *sisdb_, const char *dbname_);

s_sisdb_collect *sisdb_kv_create(uint8 style_, const char *key_, char *in_, size_t ilen_);
void sisdb_kv_destroy(void *info_);

s_sisdb_collect *sisdb_collect_create(s_sisdb_cxt *sisdb_,const char *key_);
void sisdb_collect_destroy(void *);
void sisdb_collect_clear(s_sisdb_collect *);

msec_t sisdb_collect_get_time(s_sisdb_collect *, int index_);
uint64 sisdb_collect_get_mindex(s_sisdb_collect *, int index_);

int sisdb_collect_recs(s_sisdb_collect *);

///////////////////////////////////////////////////////////////////////////
//  get
///////////////////////////////////////////////////////////////////////////

bool sisdb_collect_trans_of_range(s_sisdb_collect *collect_, int *start_, int *stop_);
bool sisdb_collect_trans_of_count(s_sisdb_collect *collect_, int *start_, int *stop_);

int sisdb_collect_search(s_sisdb_collect *, uint64 finder_);
int sisdb_collect_search_left(s_sisdb_collect *, uint64 finder_, int *mode_);
int sisdb_collect_search_right(s_sisdb_collect *, uint64 finder_, int *mode_);
int sisdb_collect_search_last(s_sisdb_collect *, uint64 finder_, int *mode_);

s_sis_sds sisdb_collect_get_of_range_sds(s_sisdb_collect *, int start_, int stop_);
s_sis_sds sisdb_collect_get_of_count_sds(s_sisdb_collect *, int start_, int count_);
s_sis_sds sisdb_collect_get_last_sds(s_sisdb_collect *);

// 得到二进制原始数据，
s_sis_sds sisdb_collect_get_original_sds(s_sisdb_collect *collect_, s_sis_json_node *);

s_sis_sds sisdb_collect_fastget_sds(s_sisdb_collect *collect_,const char *, int format_);

#define sisdb_field_is_whole(f) (!f || !sis_strncmp(f, "*", 1))
// 得到格式化的数据
s_sis_sds sisdb_get_chars_format_sds(s_sisdb_table *tb_, const char *key_, int iformat_, const char *in_, size_t ilen_, const char *fields_);
// 得到处理过的数据
s_sis_sds sisdb_collect_get_sds(s_sisdb_collect *collect_, const char *key_, int iformat_, s_sis_json_node *);
// 用户传入的 argv 参数的关键字的定义如下：
// 数据格式："format":"json" --> SIS_DSIS_JSON
//						"array" --> SIS_DATA_TYPE_ARRAY
//						"csv" --> SIS_DATA_TYPE_ARRAY
//						"bytes" --> SIS_DATA_TYPE_STRUCT  ----> 默认
// 字段：   "fields":  "time,close,vol,name" 表示一共4个字段  
//	
//                      空,*---->表示全部字段 默认全部字段
// ---------<以下区域没有表示全部数据>--------
// 数据范围："search":  min 和 start 互斥，min表示按数值取数，start 表示按记录号取 
// 					  min,max 按时序索引取数据
//						count(和max互斥，正表示向后，负表示向前),
//						force为 0 表示按实际取，为 1 若无数据就取min前一个数据，
//				    start，stop 按记录号取数据 0，-1-->表示全部数据
//						count(和stop互斥，正表示向后，负表示向前),
// 得到所有股票最新的一条记录

// 取多个股票信息默认只返回最后一条数据集合
s_sis_json_node *sisdb_collects_get_last_node(s_sisdb_collect *, s_sis_json_node *);

///////////////////////////
//	fromat trans
///////////////////////////
//输出数据时，把二进制结构数据转换成json格式数据，或者array的数据，json 数据要求带fields结构
s_sis_json_node *sis_collect_get_fields_of_json(s_sisdb_collect *collect_, s_sis_string_list *fields_);
s_sis_sds sis_collect_get_fields_of_csv(s_sisdb_collect *collect_, s_sis_string_list *fields_);


s_sis_sds sisdb_collect_struct_to_sds(s_sis_dynamic_db *db_, const char *in_, size_t ilen_, s_sis_string_list *fields_);

s_sis_sds sisdb_collect_struct_to_json_sds(s_sis_dynamic_db *db_, const char *in_, size_t ilen_,
										   const char *key_, s_sis_string_list *fields_, bool isfields_, bool zip_);

s_sis_sds sisdb_collect_struct_to_array_sds(s_sis_dynamic_db *db_, const char *in_, size_t ilen_, s_sis_string_list *fields_, bool zip_);

s_sis_sds sisdb_collect_struct_to_csv_sds(s_sis_dynamic_db *db_, const char *in_, size_t ilen_, s_sis_string_list *fields_, bool isfields_);
//传入json数据时通过该函数转成二进制结构数据
s_sis_sds sisdb_collect_json_to_struct_sds(s_sisdb_collect *, s_sis_sds in_);

//传入array数据时通过该函数转成二进制结构数据
s_sis_sds sisdb_collect_array_to_struct_sds(s_sisdb_collect *, s_sis_sds in_);

///////////////////////////
//	delete     ////
///////////////////////////
int sisdb_collect_delete_of_range(s_sisdb_collect *, int start_, int stop_);  // 定位后删除
int sisdb_collect_delete_of_count(s_sisdb_collect *, int start_, int count_); // 定位后删除

int sisdb_collect_delete(s_sisdb_collect  *, s_sis_json_node *jsql); // jsql 为json命令
//删除
//用户传入的command中关键字的定义如下：
//数据范围："search":   min 和 start 互斥，min表示按数值取数，start表示按记录号取 
// 					  min,max 按时序索引取数据
//						count(和max互斥，正表示向后，负表示向前),
//                    start，stop 按记录号取数据 0，-1-->表示全部数据
//						count(和stop互斥，正表示向后，负表示向前),

///////////////////////////
//			set        ////
///////////////////////////
// 写通用sdb数据 需要检验数据合法性
int sisdb_collect_update(s_sisdb_collect *, s_sis_sds in_);

// 从磁盘中整块写入，不逐条进行校验 直接追加数据
int sisdb_collect_wpush(s_sisdb_collect *, char *in_, size_t ilen_);

#endif /* _SIS_COLLECT_H */
