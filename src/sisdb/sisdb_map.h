
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISDB_MAP_H
#define _SISDB_MAP_H

#include "sis_core.h"
#include "sis_map.h"

#define SIS_MAP_DEFINE_TABLE_TYPE   0   // 表类型 -  没用
#define SIS_MAP_DEFINE_TIME_FORMAT  1   // 时间格式
#define SIS_MAP_DEFINE_FIELD_TYPE   2   // 字段类型
#define SIS_MAP_DEFINE_TIME_SCALE   3   // 时间尺度
#define SIS_MAP_DEFINE_ADD_METHOD   4   // 如何插入数据
#define SIS_MAP_DEFINE_ZIP_MODE     5   // 压缩类型
#define SIS_MAP_DEFINE_SUBS_METHOD  6   // 订阅数据写入方法
#define SIS_MAP_DEFINE_DATA_TYPE    7   // 读取写入数据类型

// 数据转换只能从低到高，如果day要转min一律失败
#define SIS_TABLE_TYPE_STS     0 // struct time serial  默认格式
#define SIS_TABLE_TYPE_JSON    1 // "json" 

// #define SIS_TABLE_TYPE_MARKET    100 //市场表
// #define SIS_TABLE_TYPE_STOCKS    101 //股票表

// 数据转换只能从低到高，如果day要转min一律失败
// #define SIS_TIME_FORMAT_NONE    0 // "NONE"  //
// #define SIS_TIME_FORMAT_INCR    1 // "INCR"  //int16 0开始的递增数，对应开市分钟
// #define SIS_TIME_FORMAT_MSEC    2 // "MSEC"  //int64 格式，精确到毫秒  
// #define SIS_TIME_FORMAT_SECOND  3 // "SECOND"  //int32 time_t格式，精确到秒  
// #define SIS_TIME_FORMAT_DATE    4 // "DATE"  //int32 20170101格式，精确到天

/////////////////////////////////////////////////////////
//  字段类型定义
/////////////////////////////////////////////////////////
#define SIS_FIELD_TYPE_NONE    0  // "NONE"  //
#define SIS_FIELD_TYPE_INT     1  // "INT"    //int 类型
#define SIS_FIELD_TYPE_UINT    2  // "UINT"    //unsigned int 类型
#define SIS_FIELD_TYPE_FLOAT   3  // "FLOAT"  //float
#define SIS_FIELD_TYPE_CHAR    4  // "CHAR"  定长字符串
#define SIS_FIELD_TYPE_STRING  5  // "STRING"  // 不定长字符串;
//传入格式为 field名称:数据类型:长度; SIS_FIELD_TYPE_STRING不填长度默认为16;SIS
#define SIS_FIELD_TYPE_JSON    11  // "JSON"    // json格式字符串;
#define SIS_FIELD_TYPE_PRICE   12  // "PRICE"   // 专指价格，需要和股票info中的小数点匹配
#define SIS_FIELD_TYPE_VOLUME  13  // "VOLUME"  // 专指成交量，特殊类型zint存储，
// zint 1个符号位，2位取值为 00 = 1 | 01 = 16 | 10 = 256 | 11 = 65536
// 尾数乘以幂，最大可以表示为32万亿的数量和金额；
#define SIS_FIELD_TYPE_AMOUNT  14  // "AMOUNT"  // 专指成交金额，特殊类型zint存储，
// 定义该类型主要是为了压缩时方便
#define SIS_FIELD_TYPE_MSEC    15 // "MSEC"  //int64 格式，精确到毫秒  
#define SIS_FIELD_TYPE_SECOND  16 // "SECOND"  //int32 time_t格式，精确到秒  
#define SIS_FIELD_TYPE_DATE    17 // "DATE"  //int32 20170101格式，精确到天


// 数据转换只能从低到高，如果day要转min一律失败
#define SIS_TIME_SCALE_NONE    0 // "NONE"  //
#define SIS_TIME_SCALE_MSEC    1 // "MSEC"  //int64 格式，精确到毫秒  
#define SIS_TIME_SCALE_SECOND  2 // "SECOND"  //int32 time_t格式，精确到秒  
#define SIS_TIME_SCALE_INCR    3 // "INCR"  //int16 0开始的递增数，对应开市分钟
#define SIS_TIME_SCALE_MIN1    4 // "MIN1"  //int32 time_t格式，精确到1分钟
#define SIS_TIME_SCALE_MIN5    5 // "MIN5"  //int32 time_t格式 ，精确到5分钟
#define SIS_TIME_SCALE_HOUR    6 // "HOUR"  //int32 time_t格式，精确到半小时
#define SIS_TIME_SCALE_DATE    7 // "DATE"  //int32 20170101格式，精确到天
#define SIS_TIME_SCALE_WEEK    8 // "WEEK"  //int32 20170101格式，精确到天
#define SIS_TIME_SCALE_MONTH   9 // "MONTH"  //int32 20170101格式，精确到天
#define SIS_TIME_SCALE_YEAR   10 // "YEAR"  //int32 20170101格式，精确到天

/////////////////////////////////////////////////////////
//  数据库数据插入和修改模式
/////////////////////////////////////////////////////////
#define SIS_ADD_METHOD_ALWAYS      0  // 不做判断直接追加
#define SIS_ADD_METHOD_TIME        1  // 检查时间节点重复就覆盖老的数据，不重复就放对应位置
#define SIS_ADD_METHOD_VOL         2  // 检查成交量，成交量增加就写入
#define SIS_ADD_METHOD_CODE        4  // 不做判断直接追加
#define SIS_ADD_METHOD_SORT        8  // 按时间排序，同一个时间可以有多个数据
#define SIS_ADD_METHOD_NONE        16  // 不能插入只能修改 一般不用

// #define SIS_INIT_WAIT    0 // 900或刚开机 开始等待初始化
// #define SIS_INIT_WORK    1 // 收到now发现日期一样直接改状态，如果日期是新的，就初始化后改状态 
// #define SIS_INIT_STOP    2 // 收盘

#define SIS_SUBS_METHOD_COPY 0 // 直接替换
#define SIS_SUBS_METHOD_MIN  1   // 最小值，排除0
#define SIS_SUBS_METHOD_MAX  2   // 最大值
#define SIS_SUBS_METHOD_ONCE 3  // 如果老数据为0就替换，否则不替换
#define SIS_SUBS_METHOD_INCR 4  // 求增量

/////////////////////////////////////////////////////////
//  数据格是定义 
/////////////////////////////////////////////////////////
#define SIS_DATA_TYPE_ZIP     'Z'   // 后面开始为数据 压缩格式，头部应该有字段说明
#define SIS_DATA_TYPE_STRUCT  'B'   // 后面开始为数据 二进制结构化数据
#define SIS_DATA_TYPE_STRING  'S'   // 后面开始为数据 字符串
#define SIS_DATA_TYPE_JSON    'J'   // 直接传数据  json文档 数组可能消失，利用 groups 表示多支股票的数据
							        // value 表示最新的一维数组  values 表示二维数组	
#define SIS_DATA_TYPE_ARRAY   'A'   // 直接传数据
#define SIS_DATA_TYPE_CSV     'C'   // csv格式，需要处理字符串的信息

#pragma pack(push,1)
 
typedef struct s_sis_map_define{
	const char *key;
	uint8	style;  // 类别
	uint16	uid;
	uint32  size;
} s_sis_map_define;

#pragma pack(pop)

void sisdb_init_map_define(s_sis_map_pointer *fields_);

s_sis_map_define *sisdb_find_map_define(s_sis_map_define *, const char *name_, uint8 style_);
int sisdb_find_map_uid(s_sis_map_define *, const char *name_, uint8 style_);


#endif  /* _SISDB_H */
