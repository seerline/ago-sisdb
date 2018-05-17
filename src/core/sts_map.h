#ifndef _STS_MAP_H
#define _STS_MAP_H

#include "sts_core.h"
#include "sds.h"
#include "dict.h"

// 定义一个指针类型的字典  string -- void*
// 定义一个整数类型的字典  string -- int

#pragma pack(push,1)

typedef struct s_sts_kv_int{
	const char *key;
	uint64_t	val;
}s_sts_kv_int;
typedef struct s_sts_kv_pair{
	const char *key;
	const char *val;
}s_sts_kv_pair;

#pragma pack(pop)

//////////////////////////////////////////
//  s_sts_map_buffer 基础定义
///////////////////////////////////////////////

#define s_sts_map_buffer dict
#define s_sts_map_pointer dict
#define s_sts_map_int dict
#define s_sts_map_sds dict

s_sts_map_buffer *sts_map_buffer_create();
void sts_map_buffer_destroy(s_sts_map_buffer *);
void sts_map_buffer_clear(s_sts_map_buffer *);
void *sts_map_buffer_get(s_sts_map_buffer *, const char *key_);
int  sts_map_buffer_set(s_sts_map_buffer *, const char *key_, void *value_); 
//设置key对应的数据引用，必须为一个指针，并不提供实体，

s_sts_map_pointer *sts_map_pointer_create();
#define sts_map_pointer_destroy sts_map_buffer_destroy
#define sts_map_pointer_clear sts_map_buffer_clear
#define sts_map_pointer_get sts_map_buffer_get
#define sts_map_pointer_set sts_map_buffer_set

s_sts_map_int *sts_map_int_create();
#define sts_map_int_destroy sts_map_buffer_destroy
#define sts_map_int_clear sts_map_buffer_clear
uint64_t sts_map_int_get(s_sts_map_int *, const char *key_);
int sts_map_int_set(s_sts_map_int *, const char *key_, uint64_t value_);

s_sts_map_sds *sts_map_sds_create();
#define sts_map_sds_destroy sts_map_buffer_destroy
#define sts_map_sds_clear sts_map_buffer_clear
#define sts_map_sds_get sts_map_buffer_get
#define sts_map_sds_set sts_map_buffer_set

#endif