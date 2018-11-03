
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SIS_DB_H
#define _SIS_DB_H

#include "sis_core.h"
#include "sis_map.h"
#include "sis_malloc.h"
#include "sis_time.h"
#include "sis_list.h"

#define SIS_TABLE_MAXLEN 32
#define SIS_CODE_MAXLEN  9

#define SIS_MAP_DEFINE_FIELD_TYPE   0
#define SIS_MAP_DEFINE_DATA_TYPE    1
#define SIS_MAP_DEFINE_SCALE        2
#define SIS_MAP_DEFINE_OPTION_MODE  3
#define SIS_MAP_DEFINE_ZIP_MODE     4
#define SIS_MAP_DEFINE_FIELD_METHOD 5

// 数据转换只能从低到高，如果day要转min一律失败
#define SIS_SCALE_NONE    0 // "NONE"  //
#define SIS_SCALE_MSEC    1 // "MSEC"  //int64 格式，精确到毫秒  
#define SIS_SCALE_SECOND  2 // "SECOND"  //int32 time_t格式，精确到秒  
#define SIS_SCALE_INDEX   3 // "INDEX"  //int16 0开始的递增数，对应开市分钟
#define SIS_SCALE_MIN1    4 // "MIN1"  //int32 time_t格式，精确到1分钟
#define SIS_SCALE_MIN5    5 // "MIN5"  //int32 time_t格式 ，精确到5分钟
#define SIS_SCALE_MIN30   6 // "MIN30"  //int32 time_t格式，精确到半小时
#define SIS_SCALE_DAY     7 // "DAY"  //int32 20170101格式，精确到天
#define SIS_SCALE_MONTH   8 // "MONTH"  //int32 20170101格式，精确到天

/////////////////////////////////////////////////////////
//  数据库数据插入和修改模式
/////////////////////////////////////////////////////////
#define SIS_OPTION_ALWAYS      0  // 不做判断直接追加
#define SIS_OPTION_TIME        1  // 检查时间节点重复就覆盖老的数据，不重复就放对应位置
#define SIS_OPTION_VOL         2  // 检查成交量，成交量增加就写入
#define SIS_OPTION_CODE        4  // 不做判断直接追加
#define SIS_OPTION_SORT        8  // 按时间排序，同一个时间可以有多个数据
#define SIS_OPTION_NONE        16  // 不能插入只能修改 一般不用

/////////////////////////////////////////////////////////
//  数据格是定义 
/////////////////////////////////////////////////////////
#define SIS_DATA_ZIP     'R'   // 后面开始为数据 压缩格式，头部应该有字段说明
#define SIS_DATA_STRUCT  'B'   // 后面开始为数据 二进制结构化数据
#define SIS_DATA_STRING  'S'   // 后面开始为数据 字符串
#define SIS_DATA_JSON    '{'   // 直接传数据  json文档 数组可能消失，利用 groups 表示多支股票的数据
							   // value 表示最新的一维数组  values 表示二维数组	
#define SIS_DATA_ARRAY   '['   // 直接传数据
#define SIS_DATA_CSV     'C'   // csv格式，需要处理字符串的信息

// #define SIS_INIT_WAIT    0 // 900或刚开机 开始等待初始化
// #define SIS_INIT_WORK    1 // 收到now发现日期一样直接改状态，如果日期是新的，就初始化后改状态 
// #define SIS_INIT_STOP    2 // 收盘

#define SIS_FIELD_METHOD_COVER 0 // 直接替换
#define SIS_FIELD_METHOD_MIN 1   // 最小值，排除0
#define SIS_FIELD_METHOD_MAX 2   // 最大值
#define SIS_FIELD_METHOD_INIT 3  // 如果老数据为0就替换，否则不替换
#define SIS_FIELD_METHOD_INCR 4  // 求增量

/////////////////////////////////////////////////////////
//  字段类型定义
/////////////////////////////////////////////////////////
//关于时间的定义
#define SIS_FIELD_NONE 0 // "NONE"  //
// #define SIS_FIELD_TIME    7 // "TIME"  //int64 毫秒
// #define SIS_FIELD_INDEX   8 // "INDEX" //int32 一个递增的值
//8位代码定义
// #define SIS_FIELD_CODE    9 // "CODE"  //int64 -- char[8] 8位字符转换为一个int64为索引
//其他类型定义
#define SIS_FIELD_STRING 10 // "STRING"  //string 类型 后面需跟长度;
//传入格式为 field名称:数据类型:长度; SIS_FIELD_SIS_NG不填长度默认为16;
#define SIS_FIELD_INT 11	// "INT"    //int 类型
#define SIS_FIELD_UINT 12   // "UINT"    //unsigned int 类型
// #define SIS_FIELD_FLOAT 13  // "FLOAT"  //float
#define SIS_FIELD_DOUBLE 14 // "DOUBLE" //double & float


#pragma pack(push,1)
typedef struct s_sis_map_define{
	const char *key;
	uint8	style;  // 类别
	uint16	uid;
	uint32  size;
}s_sis_map_define;

typedef struct s_sis_db {
	s_sis_sds name;  // 数据库名字
	s_sis_map_pointer  *db; // 一个字典表
	s_sis_map_pointer  *map; // 关键字查询表

	s_sis_struct_list  *trade_time; // [[930,1130],[1300,1500]] 交易时间，给分钟线用

    // int    work_mode;
    // s_sis_struct_list  *work_plans;   // plans-work 定时任务 uint16 的数组
    // s_sis_time_gap      work_always;  // always-work 循环运行的配置

    int    save_mode;   // 存盘方式，间隔时间存，还是指定时间存
    s_sis_struct_list  *save_plans;   // plans-work 定时任务 uint16 的数组
    s_sis_time_gap      save_always;  // always-work 循环运行的配置

	// int  save_type;  // 存盘方式，间隔时间存，还是指定时间存
	// int  save_gaps;  // 存盘的间隔秒数	
	// s_sis_struct_list  *save_plans; // uin16的时间序列，如果类型为gap，就表示秒为单位的间隔时间
	s_sis_thread_id_t save_pid;
	s_sis_mutex_t save_mutex;  // save时的锁定

	s_sis_sds conf;    // 保存时使用
	int save_format;   // 存盘文件的方式

	s_sis_wait thread_wait; //线程内部延时处理

}s_sis_db;
#pragma pack(pop)


s_sis_db *sis_db_create(char *name);  //数据库的名称，为空建立一个sys的数据库名
void sis_db_destroy(s_sis_db *);  //关闭一个数据库

s_sis_sds sis_db_get_table_info_sds(s_sis_db *);


s_sis_map_define *sis_db_find_map_define(s_sis_db *, const char *name_, uint8 style_);
int sis_db_find_map_uid(s_sis_db *, const char *name_, uint8 style_);


#endif  /* _SIS_DB_H */
