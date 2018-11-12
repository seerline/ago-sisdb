
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

#define SIS_FIELD_MAXLEN 32

typedef struct s_sisdb_field_flags
{
	unsigned type : 5;      // 数据类型  一共32种数据类型
	unsigned len : 7;       // 字段长度(按字节)  0...63 最大不超过64个字符串，公司名称最大限制
	unsigned dot : 4;       // 小数点 最大15个小数位
} s_sisdb_field_flags;

typedef struct s_sisdb_field
{
	uint8  index;				 // 位置，结构中第几个字段 最多64个字段
	uint16 offset;				 // 偏移位置，偏移多少字节
	char name[SIS_FIELD_MAXLEN]; // 字段名，
	s_sisdb_field_flags flags;	 // attribute字段属性
	//----以下有些难受，不过先这样处理了，以后再说----//
	uint8 subscribe_method;					// 当fields-catch开启时，由该字段判断评选方法
	char  subscribe_refer_fields[SIS_FIELD_MAXLEN]; //subscribe_refer_fields 时参考的字段索引
} s_sisdb_field;

//对table来说，定义一个map指向一个多记录sis_fieldsis_t数据结构

#pragma pack(pop)

//用户传入字段定义时的结构，根据类型来确定其他

s_sisdb_field *sisdb_field_create(int index, const char *name_, s_sisdb_field_flags *flags_);
void sisdb_field_destroy(s_sisdb_field *);

bool sisdb_field_is_time(s_sisdb_field *unit_);

#define sisdb_field_is_whole(f) (!f || !strncmp(f, "*", 1))

uint64 sisdb_field_get_uint(s_sisdb_field *unit_, const char *val_);
int64 sisdb_field_get_int(s_sisdb_field *unit_, const char *val_);
double sisdb_field_get_float(s_sisdb_field *unit_, const char *val_, int dot_);

// 实际上只是写入val中，
void sisdb_field_set_uint(s_sisdb_field *unit_, char *val_, uint64 u64_);
void sisdb_field_set_int(s_sisdb_field *unit_, char *val_, int64 i64_);
void sisdb_field_set_float(s_sisdb_field *unit_, char *val_, double f64_, int dot_);

//根据名字获取字段
s_sisdb_field *sisdb_field_get_from_key(s_sisdb_table *tb_, const char *key_);
int sisdb_field_get_offset(s_sisdb_table *tb_, const char *key_);

const char * sisdb_field_get_char_from_key(s_sisdb_table *tb_, const char *key_, const char *val_, size_t *len_);
uint64 sisdb_field_get_uint_from_key(s_sisdb_table *tb_, const char *key_, const char *val_);
// int64 sisdb_field_get_int_from_key(s_sisdb_table *tb_, const char *key_, const char *val_);
// double sisdb_field_get_float_from_key(s_sisdb_table *tb_, const char *key_, const char *val_);

void sisdb_field_set_uint_from_key(s_sisdb_table *tb_, const char *key_, char *val_, uint64 u64_);
// void sisdb_field_set_int_from_key(s_sisdb_table *tb_, const char *key_, char *val_, int64 i64_);
// void sisdb_field_set_double_from_key(s_sisdb_table *tb_, const char *key_, char *val_, double f64_);

void sisdb_field_copy(char *des_, const char *src_, size_t len_);

void sisdb_field_json_to_struct(s_sis_sds in_, s_sisdb_field *fu_, 
			char *key_, s_sis_json_node *node_,s_sisdb_cfg_info  *);

#endif /* _SIS_FIELDS_H */
