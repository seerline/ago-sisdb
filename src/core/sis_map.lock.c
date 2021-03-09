
#include "sis_map.lock.h"

//////////////////////////////////////////
//  s_sis_safe_map 基础定义
//////////////////////////////////////////

s_sis_safe_map *sis_safe_map_create()
{
	s_sis_safe_map *o = SIS_MALLOC(s_sis_safe_map, o);
	o->map = sis_map_pointer_create();
	sis_mutex_init(&o->lock, NULL);
	return o;
};

s_sis_safe_map *sis_safe_map_create_v(void *vfree_)
{
	s_sis_safe_map *o = SIS_MALLOC(s_sis_safe_map, o);
	o->map = sis_map_pointer_create_v(vfree_);
	sis_mutex_init(&o->lock, NULL);
	return o;
};

void sis_safe_map_destroy(s_sis_safe_map *map_)
{
	sis_map_pointer_destroy(map_->map);
	sis_free(map_);
};
void  sis_safe_map_lock(s_sis_safe_map *map_)
{
	sis_mutex_lock(&map_->lock);
}
void  sis_safe_map_unlock(s_sis_safe_map *map_)
{
	sis_mutex_unlock(&map_->lock);
}

void  sis_safe_map_clear(s_sis_safe_map *map_)
{
	sis_mutex_lock(&map_->lock);
	sis_map_pointer_clear(map_->map);
	sis_mutex_unlock(&map_->lock);
}
void *sis_safe_map_get(s_sis_safe_map *map_, const char *key_)
{
	sis_mutex_lock(&map_->lock);
	void *o = sis_map_pointer_get(map_->map, key_);
	sis_mutex_unlock(&map_->lock);
	return o;
}
void *sis_safe_map_find(s_sis_safe_map *map_, const char *key_)
{
	sis_mutex_lock(&map_->lock);
	void *o = sis_map_pointer_find(map_->map, key_);
	sis_mutex_unlock(&map_->lock);
	return o;
}
int   sis_safe_map_set(s_sis_safe_map *map_, const char *key_, void *val_)
{
	sis_mutex_lock(&map_->lock);
	sis_map_pointer_set(map_->map, key_, val_);
	sis_mutex_unlock(&map_->lock);
	return 0;	
}

void sis_safe_map_del(s_sis_safe_map *map_, const char *key_)
{
	sis_mutex_lock(&map_->lock);
	sis_map_pointer_del(map_->map, key_);
	sis_mutex_unlock(&map_->lock);
}
