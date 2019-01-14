#ifndef _SIS_MAP_H
#define _SIS_MAP_H

#include "sis_core.h"

#include "sis_malloc.h"

// 定义一个指针类型的字典  string -- void*
// 定义一个整数类型的字典  string -- int
#pragma pack(push,1)

typedef struct s_sis_kv_int{
	const char *key;
	uint64_t	val;
}s_sis_kv_int;
typedef struct s_sis_kv_pair{
	const char *key;
	const char *val;
}s_sis_kv_pair;

#pragma pack(pop)

//////////////////////////////////////////
//  s_sis_map_buffer 基础定义
///////////////////////////////////////////////

#define s_sis_map_buffer dict
#define s_sis_map_pointer dict
#define s_sis_map_int dict
#define s_sis_map_sds dict

s_sis_map_buffer *sis_map_buffer_create();
void sis_map_buffer_destroy(s_sis_map_buffer *);
void sis_map_buffer_clear(s_sis_map_buffer *);
void *sis_map_buffer_get(s_sis_map_buffer *, const char *key_);
int  sis_map_buffer_set(s_sis_map_buffer *, const char *key_, void *value_); 
#define sis_map_buffer_getsize dictSize
//设置key对应的数据引用，必须为一个指针，并不提供实体，

s_sis_map_pointer *sis_map_custom_create(s_sis_dict_type *);

s_sis_map_pointer *sis_map_pointer_create();
#define sis_map_pointer_destroy sis_map_buffer_destroy
#define sis_map_pointer_clear sis_map_buffer_clear
#define sis_map_pointer_get sis_map_buffer_get
#define sis_map_pointer_set sis_map_buffer_set

s_sis_map_int *sis_map_int_create();
#define sis_map_int_destroy sis_map_buffer_destroy
#define sis_map_int_clear sis_map_buffer_clear
uint64_t sis_map_int_get(s_sis_map_int *, const char *key_);
int sis_map_int_set(s_sis_map_int *, const char *key_, uint64_t value_);

s_sis_map_sds *sis_map_sds_create();
#define sis_map_sds_destroy sis_map_buffer_destroy
#define sis_map_sds_clear sis_map_buffer_clear
#define sis_map_sds_get sis_map_buffer_get
int sis_map_sds_set(s_sis_map_sds *, const char *key_, char *val_);

uint64_t _sis_dict_sds_hash(const void *key);
uint64_t _sis_dict_sdscase_hash(const void *key);
int _sis_dict_sdscase_compare(void *privdata, const void *key1, const void *key2);
void _sis_dict_buffer_free(void *privdata, void *val);
void *_sis_dict_sds_dup(void *privdata, const void *val);
void _sis_dict_sds_free(void *privdata, void *val);
void _sis_dict_list_free(void *privdata, void *val);
#endif