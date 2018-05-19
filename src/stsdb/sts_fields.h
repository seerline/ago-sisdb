
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _STS_FIELDS_H
#define _STS_FIELDS_H

#include "sds.h"
#include "dict.h"
#include <assert.h>
#include "sdsalloc.h"
#include "sts_str.h"
#include "sts_map.h"


/////////////////////////////////////////////////////////
//  字段类型定义
/////////////////////////////////////////////////////////
//关于时间的定义
#define STS_FIELD_NONE    0 // "NONE"  //
#define STS_FIELD_INDEX   1 // "INDEX"  //int32 表示序号 0.....1000....
#define STS_FIELD_SECOND  2 // "SECOND"  //int32 time_t格式，精确到秒  
#define STS_FIELD_MIN1    3 // "MIN1"  //int32 time_t格式 / 60，精确到分
#define STS_FIELD_MIN5    4 // "MIN5"  //int32 time_t格式 / 60，精确到分
#define STS_FIELD_DAY     5 // "DAY"  //int32 20170101格式，精确到天
#define STS_FIELD_TIME    6 //"TIME"  //int64 毫秒
//8位代码定义
#define STS_FIELD_CODE    9 // "CODE"  //int64 -- char[8] 8位字符转换为一个int64为索引
//其他类型定义
#define STS_FIELD_STRING  10 // "STRING"  //string 类型 后面需跟长度;
//传入格式为 field名称:数据类型:长度; STS_FIELD_STRING不填长度默认为16;
#define STS_FIELD_INT     11 // "INT"    //int 类型 
#define STS_FIELD_UINT    12 // "UINT"    //unsigned int 类型 
#define STS_FIELD_FLOAT   13 // "FLOAT"  //float 
#define STS_FIELD_DOUBLE  14 // "DOUBLE" //double

//--------32种类型，保留4种类型-------//

#pragma pack(push,1)

typedef struct s_sts_fields_flags{
	unsigned type : 4;    // 数据类型  一共16种数据类型
	unsigned len : 6;     // 字段长度(按字节)  0...63 最大不超过64个字符串，公司名称最大限制
	unsigned io : 1;      // 0 输入时放大 输出时缩小，输入时为struct就不再放大了 float类型取小数点 int 取整
	unsigned zoom : 4;    // zoom = 0 不缩放，无论io为何值，实际保存的数据都是int32整型 
	// io = 0 输入时乘以zoom后取整，输出时除以zoom后保留的小数点个数 对float&double有效
	// 最大支持15位小数, 
	// io = 1 输入时除以zoom后取整，输出时乘以zoom后取整 对int有效
	// 最大支持10^15次方,足够表示极大数 
	unsigned ziper : 3;  // none 不压缩 up 上一条 local 和当前记录某字段比较压缩 multi 和指定字段相乘
	unsigned refer : 6;  // 当为up和local的时候，表示和第几个字段比较压缩，local情况下相等为自己压自己
				// ziper==refer==0 表示不压缩  64个字段
}s_sts_fields_flags;

#define STS_FIELD_MAXLEN  32

typedef struct s_sts_field_unit{
	uint8_t  index;   // 位置，结构中第几个字段 最多64个字段
	uint16 offset;  // 偏移位置，结构中第几个字段，偏移多少字节
	char name[STS_FIELD_MAXLEN];  // 字段名，
	s_sts_fields_flags flags; // attribute字段属性
}s_sts_field_unit;

//对table来说，定义一个map指向一个多记录sts_field_unit数据结构

#pragma pack(pop)


//用户传入字段定义时的结构，根据类型来确定其他
//   fields: [

//   # ------ io & zoom 说明 -------
//   # io = 0 输入时乘以zoom后取整，输出时除以zoom后保留的小数点个数 对float&double有效
//   # 最大支持15位小数,
//   # io = 1 输入时除以zoom后取整，输出时乘以zoom后取整 对int有效
//   # 最大支持10^15次方,足够表示极大数

//   # 字段名| 数据类型| 长度| io 放大还是缩小| 缩放比例 zoom|压缩类型|压缩参考字段索引

//   [name, string, 16, 0, 0, 0, 0],
//   [type, int, 1, 0, 0, 0, 0],  # 股票类型
//   [decimal, int, 1, 0, 0, 0, 0], # 价格小数点
//   [volunit, int, 1, 0, 0, 0, 0], # 成交量单位
//   [before, int, 4, 0, 2, local, before],
//   [stophigh, int, 4, 0, 2, local, before],
//   [stoplow, int, 4, 0, 2, local, before]
//   ]

s_sts_field_unit *sts_field_unit_create(int index, const char *name_, s_sts_fields_flags *flags_);
void sts_field_unit_destroy(s_sts_field_unit *);

bool sts_field_is_times(int t_);

#define sts_check_fields_all(f) (!f || !strncmp(f, "*", 1))

uint64 sts_fields_get_uint(s_sts_field_unit *fu_, const char *val_);
int64 sts_fields_get_int(s_sts_field_unit *fu_, const char *val_);
double sts_fields_get_double(s_sts_field_unit *fu_, const char *val_);

#endif  /* _STS_FIELDS_H */
