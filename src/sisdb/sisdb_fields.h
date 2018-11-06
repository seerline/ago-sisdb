
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISDB_FIELDS_H
#define _SISDB_FIELDS_H

#include "sis_str.h"
#include "sis_map.h"

#include "sis_malloc.h"
#include "sisdb_table.h"


//--------32种类型，保留4种类型-------//

#pragma pack(push, 1)

typedef struct s_sis_fields_flags
{
	unsigned type : 4; // 数据类型  一共16种数据类型
	unsigned len : 6;  // 字段长度(按字节)  0...63 最大不超过64个字符串，公司名称最大限制
	unsigned io : 1;   // 0 输入时放大 输出时缩小，输入时为struct就不再放大了 float类型取小数点 int 取整
	unsigned zoom : 4; // zoom = 0 不缩放，无论io为何值，实际保存的数据都是int32整型
	// io = 0 输入时乘以zoom后取整，输出时除以zoom后保留的小数点个数 对float&double有效
	// 最大支持15位小数,
	// io = 1 输入时除以zoom后取整，输出时乘以zoom后取整 对int有效
	// 最大支持10^15次方,足够表示极大数
	unsigned ziper : 3; // 0 不压缩 up 上一条 local 和当前记录某字段比较压缩 multi 和指定字段相乘
	unsigned refer : 6; // 当为up和local的时候，表示和第几个字段比较压缩，local情况下相等为自己压自己
		// ziper==refer==0 表示不压缩  64个字段
} s_sis_fields_flags;

#define SIS_FIELD_MAXLEN 32


typedef struct s_sis_field_unit
{
	uint8_t index;				 // 位置，结构中第几个字段 最多64个字段
	uint16 offset;				 // 偏移位置，偏移多少字节
	char name[SIS_FIELD_MAXLEN]; // 字段名，
	s_sis_fields_flags flags;	 // attribute字段属性
	//----以下有些难受，不过先这样处理了，以后再说----//
	uint8_t catch_method;					// 当fields-catch开启时，由该字段判断评选方法
	char catch_initfield[SIS_FIELD_MAXLEN]; //catch_initfield 时参考的字段索引
} s_sis_field_unit;

//对table来说，定义一个map指向一个多记录sis_fieldsis_t数据结构

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

s_sis_field_unit *sis_field_unit_create(int index, const char *name_, s_sis_fields_flags *flags_);
void sis_field_unit_destroy(s_sis_field_unit *);

bool sis_field_is_time(s_sis_field_unit *unit_);

#define sis_check_fields_all(f) (!f || !strncmp(f, "*", 1))

uint64 sis_fields_get_uint(s_sis_field_unit *fu_, const char *val_);
int64 sis_fields_get_int(s_sis_field_unit *fu_, const char *val_);
double sis_fields_get_double(s_sis_field_unit *fu_, const char *val_);

void sis_fields_set_uint(s_sis_field_unit *fu_, char *val_, uint64 u64_);
void sis_fields_set_int(s_sis_field_unit *fu_, char *val_, int64 i64_);
void sis_fields_set_double(s_sis_field_unit *fu_, char *val_, double f64_);

//根据名字获取字段
s_sis_field_unit *sis_field_get_from_key(s_sis_table *tb_, const char *key_);
int sis_field_get_offset(s_sis_table *tb_, const char *key_);

const char * sis_fields_get_string_from_key(s_sis_table *tb_, const char *key_, const char *val_, size_t *len_);
uint64 sis_fields_get_uint_from_key(s_sis_table *tb_, const char *key_, const char *val_);
int64 sis_fields_get_int_from_key(s_sis_table *tb_, const char *key_, const char *val_);
double sis_fields_get_double_from_key(s_sis_table *tb_, const char *key_, const char *val_);

void sis_fields_set_uint_from_key(s_sis_table *tb_, const char *key_, char *val_, uint64 u64_);
void sis_fields_set_int_from_key(s_sis_table *tb_, const char *key_, char *val_, int64 i64_);
void sis_fields_set_double_from_key(s_sis_table *tb_, const char *key_, char *val_, double f64_);

void sis_fields_copy(char *des_, const char *src_, size_t len_);

#endif /* _SIS_FIELDS_H */
