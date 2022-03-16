#ifndef _SIS_MAP_LOCK_H
#define _SIS_MAP_LOCK_H

#include "sis_map.h"
#include "sis_thread.h"

//////////////////////////////////////////
//  s_sis_safe_map 基础定义
///////////////////////////////////////////////
#pragma pack(push,1)

typedef struct s_sis_safe_map {
	s_sis_mutex_t        lock;  
	s_sis_map_pointer   *map;   // 存入的整数就是list的索引
}s_sis_safe_map;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif

s_sis_safe_map *sis_safe_map_create();
s_sis_safe_map *sis_safe_map_create_v(void *vfree_);
void sis_safe_map_destroy(s_sis_safe_map *map_);

void  sis_safe_map_lock(s_sis_safe_map *);
void  sis_safe_map_unlock(s_sis_safe_map *);

void  sis_safe_map_clear(s_sis_safe_map *);
void *sis_safe_map_get(s_sis_safe_map *, const char *key_);
void *sis_safe_map_find(s_sis_safe_map *, const char *key_);
int   sis_safe_map_set(s_sis_safe_map *, const char *key_, void *value_); 
int   sis_safe_map_getsize(s_sis_safe_map *);
void  sis_safe_map_del(s_sis_safe_map *, const char *key_);

#ifdef __cplusplus
}
#endif

#endif