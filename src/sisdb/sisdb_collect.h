
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISDB_COLLECT_H
#define _SISDB_COLLECT_H

#include "sis_core.h"
#include "sis_math.h"
#include "sis_malloc.h"

#include "sisdb.h"
#include "sisdb_sys.h"
#include "sisdb_table.h"

/////////////////////////////////////////////////////////
//  数据库数据搜索模式
/////////////////////////////////////////////////////////
#define SIS_SEARCH_NONE -1 // 没有数据符合条件
#define SIS_SEARCH_NEAR  1  // 附近的数据
#define SIS_SEARCH_LEFT  2  // 虽然发现了数据，但返回的数据比请求查询的更早一些
#define SIS_SEARCH_RIGHT 3  // 虽然发现了数据，但返回的数据比请求查询的更晚一些
#define SIS_SEARCH_OK    0	// 准确匹配的数据，时间一致

#define SIS_CHECK_LASTTIME_INIT  0   // 当日最新记录，先简单按日期来判定
#define SIS_CHECK_LASTTIME_NEW   1   // 当日新增记录，新来的时间大于最后一条记录
#define SIS_CHECK_LASTTIME_OLD   2   // 记录，新来的时间小于最后一条记录
#define SIS_CHECK_LASTTIME_OK    3   // 等于最后一条记录
#define SIS_CHECK_LASTTIME_ERROR 4   // 错误，不处理

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
// 实际上定义了一个立体表，外部访问直接通过SH600600.DAY访问到collect，
// 而collect结构和归属分别作为两个轴，来唯一确定，这两个属性缺一不可，
// 设计上这样做的目的是快速定位目标数据，同时舍弃冗余数据，
// 
typedef struct s_sisdb_collect
{
	s_sisdb_table     *db;    // 表的指针，可以获得字段定义的相关信息

	////////////////////////////////////////////////////////////
	//   以下两个结构仅仅在专用数据库时使用
	s_sisdb_sys_exch  *spec_exch; // 市场的信息
	s_sisdb_sys_info  *spec_info; // 股票的信息
	////////////////////////////////////////////////////////////
	s_sis_step_index  *stepinfo; // 时间索引表，这里会保存时间序列key，每条记录的指针(不申请内存)，
	s_sis_struct_list *value;    // 结构化数据

	s_sis_sds front;  // 前一分钟的记录 catch=true生效 vol为全量 -- 存盘时一定要保存
	s_sis_sds lasted; // 当前那一分钟的记录 catch=true生效 vol为全量 -- 存盘时一定要保存

	s_sis_sds refer;  // 实际数据的前一条参考数据 zip 时生效 vol为当量 -- 需要保存
} s_sisdb_collect;

#pragma pack(pop)

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_step_index --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sis_step_index *sisdb_stepindex_create();
void sisdb_stepindex_destroy(s_sis_step_index *);
void sisdb_stepindex_clear(s_sis_step_index *);
void sisdb_stepindex_rebuild(s_sis_step_index *, uint64 left_, uint64 right_, int count_);

///////////////////////////////////////////////////////////////////////////
//------------------------s_sisdb_collect --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sisdb_collect *sisdb_collect_create(s_sis_db *db_, const char *key_);
void sisdb_collect_destroy(s_sisdb_collect *);
void sisdb_collect_clear(s_sisdb_collect *unit_);

s_sisdb_collect *sisdb_get_collect(s_sis_db *db_, const char *key_);

int sisdb_collect_recs(s_sisdb_collect *unit_);

int sisdb_collect_search(s_sisdb_collect *unit_, uint64 index_);
int sisdb_collect_search_left(s_sisdb_collect *unit_, uint64 index_, int *mode_);
int sisdb_collect_search_right(s_sisdb_collect *unit_, uint64 index_, int *mode_);

s_sis_sds sisdb_collect_get_of_range_sds(s_sisdb_collect *, int start_, int stop_);
s_sis_sds sisdb_collect_get_of_count_sds(s_sisdb_collect *, int start_, int count_);

///////////////////////////
//			get     ///////
///////////////////////////
//输出数据时，把二进制结构数据转换成json格式数据，或者array的数据，json 数据要求带fields结构
s_sis_sds sisdb_collect_struct_filter_sds(s_sisdb_collect *unit_, s_sis_sds in_, s_sis_string_list *fields_);
s_sis_sds sisdb_collect_struct_to_json_sds(s_sisdb_collect *unit_, s_sis_sds in_, s_sis_string_list *fields_, bool zip_);
s_sis_sds sisdb_collect_struct_to_array_sds(s_sisdb_collect *unit_, s_sis_sds in_, s_sis_string_list *fields_, bool zip_);

s_sis_sds sisdb_collect_struct_to_csv_sds(s_sisdb_collect *unit_, s_sis_sds in_, s_sis_string_list *fields_, bool zip_);

// 得到二进制原始数据，
s_sis_sds sisdb_collect_get_original_sds(s_sisdb_collect *collect, s_sis_json_handle *handle);
// 得到处理过的数据
s_sis_sds sisdb_collect_get_sds(s_sis_db *db_,const char *, const char *com_);
// command为json命令
// 读表中代码为key的数据，key为*表示所有股票数据，由command定义数据范围和字段范围
// 用户传入的command中关键字的定义如下：
// 返回数据格式："format":"json" --> SIS_DSIS_JSON
//						 "array" --> SIS_DATA_TYPE_ARRAY
//						 "bin" --> SIS_DATA_TYPE_STRUCT  ----> 默认
//					     "string" --> SIS_DATA_TYPE_STRING
//						 "zip" --> SIS_DATA_TYPE_ZIP
// 字段：    "fields":  "time,close,vol,name" 表示一共4个字段  
//	
//                      空,*---->表示全部字段
// ---------<以下区域没有表示全部数据>--------
// 数据范围："search":  min 和 start 互斥，min表示按数值取数，start表示按记录号取 
// 					min,max 按时序索引取数据
//						count(和max互斥，正表示向后，负表示向前),
//						force为0表示按实际取，为1若无数据就取min前一个数据，
//				    start，stop 按记录号取数据 0，-1-->表示全部数据
//						count(和stop互斥，正表示向后，负表示向前),
// 得到所有股票最新的一条记录
s_sis_json_node *sisdb_collect_groups_json_init(s_sis_string_list *fields_);
void sisdb_collect_groups_json_push(s_sis_json_node *node_, char *code, s_sisdb_collect *unit_, s_sis_sds in_, s_sis_string_list *fields_);
s_sis_sds sisdb_collect_groups_json_sds(s_sis_json_node *node_);
//////////////////////
// 得到多个股票的最后一条数据集合
s_sis_sds sisdb_collects_get_last_sds(s_sis_db *db_,const char *, const char *com_);

///////////////////////////
//			delete     ////
///////////////////////////
int sisdb_collect_delete_of_range(s_sisdb_collect *, int start_, int stop_);  // 定位后删除
int sisdb_collect_delete_of_count(s_sisdb_collect *, int start_, int count_); // 定位后删除

int sisdb_collect_delete(s_sis_db  *, const char *key_, const char *com_); // command为json命令
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
//传入json数据时通过该函数转成二进制结构数据
s_sis_sds sisdb_collect_json_to_struct_sds(s_sisdb_collect *, const char *in_, size_t ilen_);

//传入array数据时通过该函数转成二进制结构数据
s_sis_sds sisdb_collect_array_to_struct_sds(s_sisdb_collect *, const char *in_, size_t ilen_);

int sisdb_collect_update(s_sisdb_collect *unit_, s_sis_sds in_);

int sisdb_collect_update_publish(s_sisdb_collect *unit_,s_sis_sds val_, const char *code_);

// 从磁盘加载，整块写入，
int sisdb_collect_update_block(s_sisdb_collect *, const char *in_, size_t ilen_);

#endif /* _SIS_COLLECT_H */
