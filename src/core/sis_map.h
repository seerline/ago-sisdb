#ifndef _SIS_MAP_H
#define _SIS_MAP_H

#include "sis_core.h"

#include "sis_malloc.h"
#include "sis_list.h"
#include "sis_sds.h"

#define s_sis_map_pointer s_sis_dict
#define s_sis_map_int     s_sis_dict
#define s_sis_map_sds     s_sis_dict
#define s_sis_map_kint    s_sis_dict
#define s_sis_map_kobj    s_sis_dict

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


typedef struct s_sis_map_list{
	int                 safe;   // 0 不锁 1 锁
	s_sis_mutex_t       rwlock; // 是否增加读写锁
	s_sis_map_int      *map;   // 存入的整数就是list的索引
	s_sis_pointer_list *list;  // 实际数据存在这里
}s_sis_map_list;

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

// 一种带顺序检索的字典,如果有free就释放，如果没有就由用户自己释放
// 为避免同名的key引起的混乱，内部存储时list会把同名的覆盖，取值时一定要判断是否为NULL 切记
s_sis_map_list *sis_map_list_create(void *vfree_);
void sis_map_list_destroy(void *);
void sis_map_list_clear(s_sis_map_list *);

void *sis_map_list_get(s_sis_map_list *, const char *key_);
void *sis_map_list_geti(s_sis_map_list *, int );
// void *sis_map_list_first(s_sis_map_list *);
// void *sis_map_list_next(s_sis_map_list *, void *);
int sis_map_list_get_index(s_sis_map_list *mlist_, const char *key_);
int sis_map_list_set(s_sis_map_list *, const char *key_, void *value_); 
int sis_map_list_getsize(s_sis_map_list *);

s_sis_map_pointer *sis_map_pointer_create();
s_sis_map_pointer *sis_map_pointer_create_v(void *vfree_);

void sis_map_pointer_destroy(s_sis_map_pointer *map_);

void  sis_map_pointer_clear(s_sis_map_pointer *);
void *sis_map_pointer_get(s_sis_map_pointer *, const char *key_);
void *sis_map_pointer_find(s_sis_map_pointer *, const char *key_);
int   sis_map_pointer_set(s_sis_map_pointer *, const char *key_, void *value_); 
#define sis_map_pointer_getsize sis_dict_getsize
void sis_map_pointer_del(s_sis_map_pointer *, const char *key_);
//设置key对应的数据引用，必须为一个指针，并不提供实体，

// 变量为整数
s_sis_map_int *sis_map_int_create();
#define sis_map_int_destroy sis_map_pointer_destroy
#define sis_map_int_clear sis_map_pointer_clear
int64 sis_map_int_get(s_sis_map_int *, const char *key_);
int sis_map_int_set(s_sis_map_int *, const char *key_, int64 value_);
#define sis_map_int_getsize sis_dict_getsize

s_sis_map_sds *sis_map_sds_create();
#define sis_map_sds_destroy sis_map_pointer_destroy
#define sis_map_sds_clear sis_map_pointer_clear
#define sis_map_sds_get sis_map_pointer_get
int sis_map_sds_set(s_sis_map_sds *, const char *key_, char *val_);

// key为整数
s_sis_map_kint *sis_map_kint_create();
#define sis_map_kint_destroy sis_map_pointer_destroy
#define sis_map_kint_clear sis_map_pointer_clear
void *sis_map_kint_get(s_sis_map_kint *, int64 );
int sis_map_kint_set(s_sis_map_kint *, int64, void *value_);
#define sis_map_kint_getsize sis_dict_getsize
void sis_map_kint_del(s_sis_map_kint *, int64);

// key为 s_sis_object 避免多次申请内存
s_sis_map_kobj *sis_map_kobj_create();
#define sis_map_kobj_destroy sis_map_pointer_destroy
#define sis_map_kobj_clear sis_map_pointer_clear
#define sis_map_kobj_get sis_map_pointer_get
#define sis_map_kobj_set sis_map_pointer_set
#define sis_map_kobj_getsize sis_dict_getsize
#define sis_map_kobj_del sis_map_pointer_del

#ifdef __cplusplus
}
#endif

#endif