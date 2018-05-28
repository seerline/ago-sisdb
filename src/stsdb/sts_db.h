
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _STS_DB_H
#define _STS_DB_H

#include "sts_map.h"
#include "sts_table.h"
#include "sts_table.h"

#define STS_TABLE_MAXLEN 32
#define STS_CODE_MAXLEN  9


#define STS_MAP_DEFINE_FIELD_TYPE   0
#define STS_MAP_DEFINE_DATA_TYPE    1
#define STS_MAP_DEFINE_SCALE        2
#define STS_MAP_DEFINE_OPTION_MODE  3
#define STS_MAP_DEFINE_ZIP_MODE     4
#define STS_MAP_DEFINE_FIELD_METHOD 5

// 数据转换只能从低到高，如果day要转min一律失败
#define STS_SCALE_NONE    0 // "NONE"  //
#define STS_SCALE_MSEC    1 // "MSEC"  //int64 格式，精确到毫秒  
#define STS_SCALE_SECOND  2 // "SECOND"  //int32 time_t格式，精确到秒  
#define STS_SCALE_INDEX   3 // "INDEX"  //int16 0开始的递增数，对应开市分钟
#define STS_SCALE_MIN1    4 // "MIN1"  //int32 time_t格式，精确到1分钟
#define STS_SCALE_MIN5    5 // "MIN5"  //int32 time_t格式 ，精确到5分钟
#define STS_SCALE_MIN30   6 // "MIN30"  //int32 time_t格式，精确到半小时
#define STS_SCALE_DAY     7 // "DAY"  //int32 20170101格式，精确到天
#define STS_SCALE_MONTH   8 // "MONTH"  //int32 20170101格式，精确到天

#pragma pack(push,1)
typedef struct s_sts_map_define{
	const char *key;
	uint8	style;  // 类别
	uint16	uid;
	uint32  size;
}s_sts_map_define;

typedef struct s_sts_time_pair{
	uint16	first;
	uint16	second;
}s_sts_time_pair;

typedef struct s_sts_db {
	s_sts_sds name;  // 数据库名字
	s_sts_map_pointer  *db; // 一个字典表
	s_sts_map_pointer  *map; // 关键字查询表
	s_sts_time_pair     work_time; // 900 --1530 工作时间
	s_sts_struct_list  *trade_time; // [[930,1130],[1300,1500]] 交易时间

	sts_sds conf;    // 保存时使用
	int save_format; // 存盘文件的方式

	int  save_type;  // 存盘方式，间隔时间存，还是指定时间存
	int  save_gaps;  // 存盘的间隔秒数	
	s_sts_struct_list  *save_plans; // uin16的时间序列，如果类型为gap，就表示秒为单位的间隔时间
	s_sts_thread_id_t save_pid;
	s_sts_mutex_rw save_mutex;  // save时的锁定

	s_sts_wait thread_wait; //线程内部延时处理

}s_sts_db;
#pragma pack(pop)


s_sts_db *sts_db_create(char *name);  //数据库的名称，为空建立一个sys的数据库名
void sts_db_destroy(s_sts_db *);  //关闭一个数据库
//取数据和写数据
s_sts_table *sts_db_get_table(s_sts_db *, const char *name_); //name -- table name

s_sts_sds sts_db_get_table_info_sds(s_sts_db *);


s_sts_map_define *sts_db_find_map_define(s_sts_db *, const char *name_, uint8 style_);
int sts_db_find_map_uid(s_sts_db *, const char *name_, uint8 style_);


#endif  /* _STS_DB_H */
