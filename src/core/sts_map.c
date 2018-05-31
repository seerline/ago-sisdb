
#include "sts_map.h"
#include "sts_list.h"

uint64_t _sts_dict_sds_hash(const void *key)
{
	return sts_dict_hash_func((unsigned char *)key, sts_sdslen((char *)key));
}

uint64_t _sts_dict_sdscase_hash(const void *key)
{
	return sts_dict_casehash_func((unsigned char *)key, sts_sdslen((char *)key));
}

int _sts_dict_sdscase_compare(void *privdata, const void *key1, const void *key2)
{
	STS_NOTUSED(privdata);
	return strcasecmp((const char *)key1, (const char *)key2) == 0;
}

void _sts_dict_buffer_free(void *privdata, void *val)
{
	STS_NOTUSED(privdata);
	sts_free(val);
}
void _sts_dict_sds_free(void *privdata, void *val)
{
	STS_NOTUSED(privdata);
	sts_sdsfree((s_sts_sds)val);
}
void _sts_dict_list_free(void *privdata, void *val)
{
	STS_NOTUSED(privdata);
	sts_struct_list_destroy((s_sts_struct_list *)val);
}

s_sts_dict_type _sts_dict_type_buffer_s = {
	_sts_dict_sdscase_hash,	   /* hash function */
	NULL,				   /* key dup */
	NULL,				   /* val dup */
	_sts_dict_sdscase_compare, /* key compare */
	_sts_dict_sds_free,	 /* key destructor */
	_sts_dict_buffer_free   /* val destructor */
};
s_sts_dict_type _sts_dict_type_sign_s = {
	_sts_dict_sdscase_hash,	   /* hash function */
	NULL,				   /* key dup */
	NULL,				   /* val dup */
	_sts_dict_sdscase_compare, /* key compare */
	_sts_dict_sds_free,	 /* key destructor */
	NULL				   /* val destructor */
};
s_sts_dict_type _sts_dict_type_sds_s = {
	_sts_dict_sdscase_hash,	   /* hash function */
	NULL,				   /* key dup */
	NULL,				   /* val dup */
	_sts_dict_sdscase_compare, /* key compare */
	_sts_dict_sds_free,	 /* key destructor */
	_sts_dict_sds_free	  /* val destructor */
};
s_sts_dict_type _sts_dict_list_sds_s = {
	_sts_dict_sdscase_hash,	   /* hash function */
	NULL,				   /* key dup */
	NULL,				   /* val dup */
	_sts_dict_sdscase_compare, /* key compare */
	_sts_dict_sds_free,	 /* key destructor */
	_sts_dict_list_free	 /* val destructor */
};

//////////////////////////////////////////
//  s_sts_map_buffer 基础定义
///////////////////////////////////////////////

s_sts_map_buffer *sts_map_buffer_create()
{ //明确知道val的长度
	s_sts_map_buffer *map = sts_dict_create(&_sts_dict_type_buffer_s, NULL);
	return map;
};
void sts_map_buffer_destroy(s_sts_map_buffer *map_)
{
	sts_dict_destroy(map_);
};
void sts_map_buffer_clear(s_sts_map_buffer *map_)
{
	sts_dict_empty(map_, NULL);
};
void *sts_map_buffer_get(s_sts_map_buffer *map_, const char *key_)
{
	//??? 这里可能有问题
	s_sts_dict_entry *he;
	s_sts_sds key = sts_sdsnew(key_);
	he = sts_dict_find(map_, key);
	sts_sdsfree(key);
	if (!he)
	{
		return NULL;
	}
	return sts_dict_getval(he);
};
int sts_map_buffer_set(s_sts_map_buffer *map_, const char *key_, void *value_)
{
	s_sts_dict_entry *he;
	s_sts_sds key = sts_sdsnew(key_);
	he = sts_dict_find(map_, key_);
	sts_sdsfree(key);
	if (!he)
	{
		sts_dict_add(map_, sts_sdsnew(key_), value_);
		return 0;
	}
	
	sts_dict_setval(map_, he, value_);
	return 0;
}

//////////////////////////////////////////
//  s_sts_map_int 基础定义
//////////////////////////////////////////
s_sts_map_pointer *sts_map_pointer_create()
{
	s_sts_map_pointer *map = sts_dict_create(&_sts_dict_type_buffer_s, NULL);
	return map;
};
s_sts_map_pointer *sts_map_sign_create()
{
	s_sts_map_pointer *map = sts_dict_create(&_sts_dict_type_sign_s, NULL);
	return map;
};
//////////////////////////////////////////
//  s_sts_map_int 基础定义
//////////////////////////////////////////
s_sts_map_int *sts_map_int_create()
{
	s_sts_map_int *map = sts_dict_create(&_sts_dict_type_sign_s, NULL);
	return map;
};
uint64_t sts_map_int_get(s_sts_map_int *map_, const char *key_)
{
	s_sts_dict_entry *he;
	he = sts_dict_find(map_, key_);
	if (!he)
	{
		return 0;
	}
	return sts_dict_get_uint(he);
};
int sts_map_int_set(s_sts_map_int *map_, const char *key_, uint64_t value_)
{
	s_sts_dict_entry *he;
	he = sts_dict_find(map_, key_);
	if (!he)
	{
		return 0;
	}
	sts_dict_set_uint(he, value_);
	return 0;
}
//////////////////////////////////////////
//  s_sts_map_sds 基础定义
//////////////////////////////////////////
s_sts_map_sds *sts_map_sds_create()
{
	s_sts_map_sds *map = sts_dict_create(&_sts_dict_type_sds_s, NULL);
	return map;
};
