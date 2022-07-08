
#include "sis_map.h"
#include "sis_list.h"
#include "sis_obj.h"

uint64_t _sis_dict_str_hash(const void *key)
{
	return sis_dict_get_hash_func((unsigned char *)key, sis_strlen((char *)key));
}
uint64_t _sis_dict_strcase_hash(const void *key)
{
	return sis_dict_get_casehash_func((unsigned char *)key, sis_strlen((char *)key));
}
uint64_t _sis_dict_int_hash(const void *key) // 只能是 int64
{
	return sis_dict_get_casehash_func((unsigned char *)key, sizeof(int64));
}
/**
 * @brief 键值为int64时的默认hash函数，added by zhangchao
 * @param key 
 * @return uint64_t 
 */
uint64_t _sis_dict_int64_hash(const void *key) // 只能是 int64
{
	return (uint64_t)key;
}

uint64_t _sis_dict_obj_hash(const void *key)
{
	return sis_dict_get_casehash_func((unsigned char *)SIS_OBJ_GET_CHAR((s_sis_object *)key), SIS_OBJ_GET_SIZE((s_sis_object *)key));
}
int _sis_dict_strcase_compare(const void *key1, const void *key2)
{
	return sis_strcasecmp((const char *)key1, (const char *)key2) == 0;
}
int _sis_dict_int_compare(const void *key1, const void *key2)
{
	return *((int64 *)key1) == *((int64 *)key2);
}
/**
 * @brief 键类型为int64时的键比较函数，add by zhangchao
 * @param key1 
 * @param key2 
 * @return int 
 */
int _sis_dict_int64_compare(const void *key1, const void *key2)
{
	return key1 == key2;
}

int _sis_dict_obj_compare(const void *key1, const void *key2)
{
	// return sis_strcasecmp(SIS_OBJ_GET_CHAR((s_sis_object *)key1), SIS_OBJ_GET_CHAR((s_sis_object *)key2)) == 0;
	return sis_strcasecmp((const char *)key1, SIS_OBJ_GET_CHAR((s_sis_object *)key2)) == 0;
}
void *_sis_dict_sds_dup(const void *val)
{
	return sis_sdsnew(val);
}
// 字符串复制函数
void *_sis_dict_str_dup(const void *val)
{
	int size = sis_strlen((char *)val);
	char *o = sis_malloc(size + 1);
	sis_strcpy(o, size + 1, (char *)val);
	return o;
}
void *_sis_dict_int_dup(const void *val)
{
	int64 *o = sis_malloc(sizeof(int64));
	memmove(o, val, sizeof(int64));	
	// printf("===== %lld,\n", *o);
	return o;
}
/**
 * @brief 键类型为int64时的默认键复制函数，added by zhangchao
 * @param val 
 * @return void* 
 */
void *_sis_dict_int64_dup(const void *val)
{
	return (void *)val;
}

void *_sis_dict_obj_dup(const void *val)
{
	s_sis_object *obj = (s_sis_object *)val;
	sis_object_incr(obj);
	return obj;
}
void _sis_dict_sds_free(void *val)
{
	sis_sdsfree((s_sis_sds)val);
}
void _sis_dict_str_free(void *val)
{
	sis_free(val);
}
void _sis_dict_obj_free(void *val)
{
	s_sis_object *obj = (s_sis_object *)val;
	sis_object_decr(obj);
}
// 键为字符串的字典类型的默认初始化实现（不包含对值的析构函数）
s_sis_dict_type _sis_dict_type_nofree_val_s = {
	_sis_dict_strcase_hash,	   /* hash function */
	_sis_dict_str_dup,		   /* key dup */
	NULL,					   /* val dup */
	_sis_dict_strcase_compare, /* key compare */
	_sis_dict_str_free,		   /* key destructor */
	NULL					   /* val destructor */
};

/**
 * @brief 键为64位整数的默认初始化实现，added by zhangchao
 */
s_sis_dict_type _sis_dict_type_int_key_s = {
	_sis_dict_int64_hash,	     /* hash function */
	_sis_dict_int64_dup,		 /* key dup */
	NULL,					     /* val dup */
	_sis_dict_int64_compare,     /* key compare */
	NULL,		                 /* key destructor */
	NULL					     /* val destructor */
};

s_sis_dict_type _sis_dict_type_obj_key_s = {
	_sis_dict_obj_hash,	       /* hash function */
	_sis_dict_obj_dup,		   /* key dup */
	NULL,					   /* val dup */
	_sis_dict_obj_compare,     /* key compare */
	_sis_dict_obj_free,		   /* key destructor */
	NULL					   /* val destructor */
};

s_sis_dict_type _sis_dict_type_sds_s = {
	_sis_dict_strcase_hash,	/* hash function */
	_sis_dict_str_dup,		   /* key dup */
	_sis_dict_sds_dup,		   /* val dup */
	_sis_dict_strcase_compare, /* key compare */
	_sis_dict_str_free,		   /* key destructor */
	_sis_dict_sds_free		   /* val destructor */
};

//////////////////////////////////////////
//  s_sis_map_list 基础定义
//////////////////////////////////////////

s_sis_map_list *sis_map_list_create(void *vfree_)
{
	s_sis_map_list *o = SIS_MALLOC(s_sis_map_list, o);
	sis_mutex_init(&o->rwlock, NULL);
	o->map = sis_map_int_create();
	o->list = sis_pointer_list_create();
	o->list->vfree = vfree_;
	return o;
}
void sis_map_list_destroy(void *mlist_)
{
	s_sis_map_list *mlist = (s_sis_map_list *)mlist_;
	sis_map_int_destroy(mlist->map);
	sis_pointer_list_destroy(mlist->list);
	sis_free(mlist);
}
#define SIS_MLIST_LOCK(m) { if (m->safe) { sis_mutex_lock(&m->rwlock); } }
#define SIS_MLIST_UNLOCK(m) { if (m->safe) { sis_mutex_unlock(&m->rwlock); } }

void sis_map_list_clear(s_sis_map_list *mlist_)
{
	SIS_MLIST_LOCK(mlist_);
	sis_map_int_clear(mlist_->map);
	sis_pointer_list_clear(mlist_->list);
	SIS_MLIST_UNLOCK(mlist_);
}

int sis_map_list_get_index(s_sis_map_list *mlist_, const char *key_)
{
	if (!mlist_ || !key_ )
	{
		return -1;
	}
	// if (!mlist_ || !key_ || sis_map_list_getsize(mlist_) < 1)
	// {
	// 	return -1;
	// }
	SIS_MLIST_LOCK(mlist_);
	int i = sis_map_int_get(mlist_->map, key_);
	SIS_MLIST_UNLOCK(mlist_);
	return i; 
}
void *sis_map_list_geti(s_sis_map_list *mlist_, int index_)
{
	SIS_MLIST_LOCK(mlist_);
	void *r = sis_pointer_list_get(mlist_->list, index_);
	SIS_MLIST_UNLOCK(mlist_);
	return r;
}

void *sis_map_list_get(s_sis_map_list *mlist_, const char *key_)
{
	if (!mlist_ || !key_ )
	{
		return NULL;
	}
	void *o = NULL;
	SIS_MLIST_LOCK(mlist_);
	int index = sis_map_int_get(mlist_->map, key_);
	if (index >= 0 && index < mlist_->list->count)
	{
		o = sis_pointer_list_get(mlist_->list, index);
	}
	SIS_MLIST_UNLOCK(mlist_);
	return o;
}

// map有变化必须全部重索引
int sis_map_list_set(s_sis_map_list *mlist_, const char *key_, void *value_)
{
	SIS_MLIST_LOCK(mlist_);
	int index = sis_map_int_get(mlist_->map, key_);
	// printf(" %s index = %d\n",key_, index);
	if (index >= 0)
	{
		// sis_map_int_set(mlist_->map, key_, index);
		sis_pointer_list_update(mlist_->list, index, value_);
	}
	else
	{
		sis_map_int_set(mlist_->map, key_, mlist_->list->count);
		sis_pointer_list_push(mlist_->list, value_);
		index = mlist_->list->count - 1;
	}
	SIS_MLIST_UNLOCK(mlist_);
	return index;
}
int sis_map_list_getsize(s_sis_map_list *mlist_)
{	
	int count = 0;
	SIS_MLIST_LOCK(mlist_);
	count = mlist_->list->count;
	SIS_MLIST_UNLOCK(mlist_);
	return count;
}

// 创建并初始化一个HASH表
s_sis_map_pointer *sis_map_pointer_create()
{
	// 首先为类对象分配内存空间，并将所有内存初始化为0
	s_sis_dict_type *type = SIS_MALLOC(s_sis_dict_type, type);
	// 赋予默认初始化实现
	memmove(type, &_sis_dict_type_nofree_val_s, sizeof(s_sis_dict_type));
	// 实例化一个HASH表对象并返回
	s_sis_map_pointer *map = sis_dict_create(type, NULL);
	return map;
};
// 创建一个HASH表，并指定HASH值的析构函数
s_sis_map_pointer *sis_map_pointer_create_v(void *vfree_)
{
	s_sis_map_pointer *map = sis_map_pointer_create();
	map->type->vfree = vfree_;
	return map;
};

void sis_map_pointer_destroy(s_sis_map_pointer *map_)
{
	s_sis_dict_type *type = map_->type;
	sis_dict_destroy(map_);
	sis_free(type);
};
void  sis_map_pointer_clear(s_sis_map_pointer *map_)
{
	sis_dict_empty(map_, NULL);
}
/* 从字典中返回指定键值key对应的值，如果找不到就返回NULL*/
void *sis_map_pointer_get(s_sis_map_pointer *map_, const char *key_)
{
	if (!map_)
	{
		return NULL;
	}
	s_sis_dict_entry *he = sis_dict_find(map_, key_);
	if (!he)
	{
		return NULL;
	}
	return sis_dict_getval(he);
}
void *sis_map_pointer_find(s_sis_map_pointer *map_, const char *key_)
{
	if (!map_)
	{
		return NULL;
	}
	s_sis_dict_entry *he = sis_dict_find(map_, key_);
	if (!he)
	{
		return NULL;
	}
	return sis_dict_getval(he);
}
int   sis_map_pointer_set(s_sis_map_pointer *map_, const char *key_, void *val_)
{
	sis_dict_replace(map_, (void *)key_, val_);
	return 0;	
}

void sis_map_pointer_del(s_sis_map_pointer *map_, const char *key_)
{
	sis_dict_delete(map_, key_);
}
//////////////////////////////////////////
//  s_sis_map_int 基础定义
//////////////////////////////////////////
s_sis_map_int *sis_map_int_create()
{
	s_sis_dict_type *type = SIS_MALLOC(s_sis_dict_type, type);
	memmove(type, &_sis_dict_type_nofree_val_s, sizeof(s_sis_dict_type));
	s_sis_map_int *map = sis_dict_create(type, NULL);
	return map;
};
int64 sis_map_int_get(s_sis_map_int *map_, const char *key_)
{
	s_sis_dict_entry *he;
	he = sis_dict_find(map_, key_);
	if (!he)
	{
		return -1;
	}
	return sis_dict_get_int(he);
};
int sis_map_int_set(s_sis_map_int *map_, const char *key_, int64 value_)
{
	return sis_dict_set_int(map_, (void *)key_, value_);
}
//////////////////////////////////////////
//  s_sis_map_sds 基础定义
//////////////////////////////////////////
/** 创建一个键类型为string，值类型为sds的字典 */
s_sis_map_sds *sis_map_sds_create()
{
	s_sis_dict_type *type = SIS_MALLOC(s_sis_dict_type, type);
	memmove(type, &_sis_dict_type_sds_s, sizeof(s_sis_dict_type));
	s_sis_map_sds *map = sis_dict_create(type, NULL);
	return map;
};
int sis_map_sds_set(s_sis_map_sds *map_, const char *key_, char *val_)
{
	sis_dict_replace(map_, (void *)key_, val_);
	return 0;
}

//////////////////////////////////////////
//  s_sis_map_kint 基础定义
//////////////////////////////////////////
/** 创建一个键类型为int的字典 */
s_sis_map_kint *sis_map_kint_create()
{
	s_sis_dict_type *type = SIS_MALLOC(s_sis_dict_type, type);
	memmove(type, &_sis_dict_type_int_key_s, sizeof(s_sis_dict_type));
	s_sis_map_kint *map = sis_dict_create(type, NULL);
	return map;

}

/** 从字典中查找数据，键类型为int */
void *sis_map_kint_get(s_sis_map_kint *map_, int64 key_)
{
	if (!map_)
	{
		return NULL;
	}
	// s_sis_dict_entry *he = sis_dict_find(map_, (void *)&key_);
	s_sis_dict_entry *he = sis_dict_find(map_, (const void *)key_);
	if (!he)
	{
		return NULL;
	}
	return sis_dict_getval(he);

}
int sis_map_kint_set(s_sis_map_kint *map_, int64 key_, void *val_)
{
	sis_dict_replace(map_,  (void *)key_,val_);	
	// sis_dict_replace(map_, (void *)&key_, val_);	
	return 0;
}

void sis_map_kint_del(s_sis_map_kint *map_, int64 key_)
{
	sis_dict_delete(map_, (const void*)key_);
	// sis_dict_delete(map_, (void *)&key_);
}
//////////////////////////////////////////
//  s_sis_map_kobj 基础定义
//////////////////////////////////////////

s_sis_map_kobj *sis_map_kobj_create()
{
	s_sis_dict_type *type = SIS_MALLOC(s_sis_dict_type, type);
	memmove(type, &_sis_dict_type_obj_key_s, sizeof(s_sis_dict_type));
	s_sis_map_kobj *map = sis_dict_create(type, NULL);
	return map;

}
#if 0
int main()
{
	s_sis_map_kint *map = sis_map_kint_create();
	for (size_t i = 0; i < 10000000; i++)
	{
		sis_map_kint_set(map, i, i);
	}
	for (size_t i = 0; i < 10000000; i++)
	{
		if(i!=sis_map_kint_get(map, i)){
			LOG(5)
			("error,%ld\n",i);
			break;
		}
	}
	LOG(5)
	("success");
	// sis_map_kint_set(map, 0, NULL);
	// sis_map_kint_set(map, 1, NULL);
	// sis_map_kint_del(map, 0);
	sis_map_kint_destroy(map);
}
// int main()
// {
// 	safe_memory_start();
// 	char var1[128];
// 	sis_strcpy(var1, 128, "my is dingzhengdong.");
// 	char var2[128];
// 	sis_strcpy(var2, 128, "my is xuping.");
// 	char *outstr;
// 	{
// 		s_sis_map_pointer *map = sis_map_pointer_create_v(sis_free_call);
// 		{
// 			char *str = sis_malloc(128);
// 			memmove(str, var1, 127);
// 			sis_map_pointer_set(map, "ding", str);
// 		}
// 		outstr = sis_map_pointer_get(map, "ding");
// 		printf("out : %s\n", outstr);

// 		{
// 			char *str = sis_malloc(128);
// 			memmove(str, var2, 127);
// 			sis_map_pointer_set(map, "ding", str);
// 		}
// 		outstr = sis_map_pointer_get(map, "ding");
// 		printf("out : %s\n", outstr);
// 		sis_map_pointer_destroy(map);
// 	}
// 	safe_memory_stop();
// 	return 0;
// }

// int main()
// {
// 	safe_memory_start();
// 	char var1[128];
// 	sis_strcpy(var1, 128, "my is dingzhengdong.");
// 	char var2[128];
// 	sis_strcpy(var2, 128, "my is xuping.");
// 	char *outstr;
// 	{
// 		s_sis_map_pointer *map = sis_map_pointer_create_v(sis_free_call);
// 		{
// 			char *str = sis_malloc(128);
// 			memmove(str, var1, 127);
// 			sis_map_pointer_set(map, "ding", str);
// 		}
// 		// {
// 		// 	char *str = sis_malloc(128);
// 		// 	memmove(str, var1, 127);
// 		// 	sis_map_pointer_set(map, "xu", str);
// 		// }
// 		// sis_map_pointer_del(map, "xu");
// 		outstr = sis_map_pointer_get(map, "ding");
// 		printf("out : %s\n", outstr);
// 		{
// 			char *str = sis_malloc(128);
// 			memmove(str, var2, 127);
// 			sis_map_pointer_set(map, "ding", str);
// 		}
// 		outstr = sis_map_pointer_get(map, "ding");
// 		printf("out : %s\n", outstr);
// 		sis_map_pointer_destroy(map);
// 	}
// 	safe_memory_stop();
// 	return 0;
// }

// int main()
// {
// 	safe_memory_start();
// 	char var1[128];
// 	sis_strcpy(var1, 128, "my is dingzhengdong.");
// 	char var2[128];
// 	sis_strcpy(var2, 128, "my is xuping.");
// 	char *outstr;
// 	{
// 		s_sis_map_sds *map = sis_map_sds_create();
// 		sis_map_sds_set(map, "ding", var1);
// 		sis_map_sds_set(map, "xu", var1);
// 		outstr = sis_map_sds_get(map, "ding");
// 		printf("out : %s\n", outstr);
// 		sis_map_sds_set(map, "ding", var2);
// 		outstr = sis_map_sds_get(map, "ding");
// 		printf("out : %s\n", outstr);
// 		sis_map_sds_destroy(map);
// 	}
// 	safe_memory_stop();
// 	return 0;
// }

// int main()
// {
// 	safe_memory_start();
// 	s_sis_map_pointer *map = sis_map_pointer_create_v(sis_free_call);
// 	char key[128];
// 	sis_sprintf(key, 128, "ask.%s", "dingzhengdong");

// 	char *str = sis_malloc(200);
// 	sis_map_pointer_set(map, key, str);
// 	printf("in %p %p\n",map, str);
// 	char *sss = sis_map_pointer_get(map, key);
// 	printf("out %p %p\n",map, sss);

// 	str = sis_malloc(100);
// 	sis_map_pointer_set(map, key, str);
// 	printf("in %p %p\n",map, str);
// 	sss = sis_map_pointer_get(map, key);
// 	printf("out %p %p\n",map, sss);

// 	str = sis_malloc(100);
// 	sis_map_pointer_set(map, key, str);
// 	printf("in %p %p\n",map, str);
// 	sss = sis_map_pointer_get(map, key);
// 	printf("out %p %p\n",map, sss);

// 	for(size_t i = 0; i < 1000000; i++)
// 	{
// 		str = sis_malloc(rand() % 200);		
// 		sis_map_pointer_set(map, key, str);	
// 		printf("rand %d %p\n",(int)i, str);	
// 		sss = sis_map_pointer_get(map, key);
// 		printf("out %p %p\n",map, sss);
// 	}
// 	// str = sis_malloc(rand() % 100);	
// 	sis_map_pointer_destroy(map);
// 	safe_memory_stop();
// }
#if 0

int main()
{
	safe_memory_start();
	s_sis_map_kint *map = sis_map_kint_create();
	map->type->vfree = sis_free_call;

	{
		char *str = sis_malloc(200);
		sis_sprintf(str, 100, "123456");
		sis_map_kint_set(map, 0, str);
		printf("in %p %p\n", map, str);
		char *sss = sis_map_kint_get(map, 0);
		printf("out %p %p\n", map, sss);
	}

	{
		char *str = sis_malloc(200);
		sis_sprintf(str, 100, "654321");
		sis_map_kint_set(map, 0, str);
		printf("in %p %p\n", map, str);
		char *sss = sis_map_kint_get(map, 0);
		printf("out %p %p\n", map, sss);
	}

	sis_map_kint_destroy(map);
	safe_memory_stop();
}
#endif
#endif
