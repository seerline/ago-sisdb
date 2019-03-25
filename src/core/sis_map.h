#ifndef _SIS_MAP_H
#define _SIS_MAP_H

#include "sis_core.h"

#include "sis_malloc.h"
#include "sis_list.h"

#define s_sis_map_buffer dict
#define s_sis_map_pointer dict
#define s_sis_map_int dict
#define s_sis_map_sds dict

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

// typedef struct s_sis_map_list{
// 	s_sis_map_pointer  *map;   // 不保存实际数据，仅仅有key
// 	s_sis_pointer_list *list;  // 实际数据存在这里
// }s_sis_map_list;

// typedef struct s_sis_pointer_list {
// 	int		     maxcount; // 总数
// 	int		     count;    // 当前个数
// 	int          len;      // 每条记录的长度
// 	void        *buffer;   // 必须是mallco申请的char*类型
// 	void(*free)(void *);   // == NULL 不释放对应内存
// } s_sis_pointer_list;

#pragma pack(pop)

//////////////////////////////////////////
//  s_sis_map_buffer 基础定义
///////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif
//设置key对应的数据引用，必须为一个指针，并不提供实体，
s_sis_map_buffer *sis_map_buffer_create();
void sis_map_buffer_destroy(s_sis_map_buffer *);
void sis_map_buffer_clear(s_sis_map_buffer *);
void *sis_map_buffer_get(s_sis_map_buffer *, const char *key_);
int  sis_map_buffer_set(s_sis_map_buffer *, const char *key_, void *value_); 
#define sis_map_buffer_getsize dictSize

// 一种带顺序检索的字典,如果有free就释放，如果没有就由用户自己释放
// s_sis_map_list *sis_map_list_create();
// void sis_map_list_destroy(s_sis_map_list *);
// void sis_map_list_clear(s_sis_map_list *);
// void *sis_map_list_get(s_sis_map_list *, const char *key_);
// int sis_map_list_set(s_sis_map_list *, const char *key_, void *value_); 
// int sis_map_list_getsize(s_sis_map_list *);


s_sis_map_pointer *sis_map_custom_create(s_sis_dict_type *);

s_sis_map_pointer *sis_map_pointer_create();
#define sis_map_pointer_destroy sis_map_buffer_destroy
#define sis_map_pointer_clear sis_map_buffer_clear
#define sis_map_pointer_get sis_map_buffer_get
#define sis_map_pointer_set sis_map_buffer_set
#define sis_map_pointer_getsize dictSize

s_sis_map_int *sis_map_int_create();
#define sis_map_int_destroy sis_map_buffer_destroy
#define sis_map_int_clear sis_map_buffer_clear
int64_t sis_map_int_get(s_sis_map_int *, const char *key_);
int sis_map_int_set(s_sis_map_int *, const char *key_, int64_t value_);
#define sis_map_int_getsize dictSize

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

#ifdef __cplusplus
}
#endif

#endif