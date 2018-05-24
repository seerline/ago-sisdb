
#include "sts_map.h"
#include "sts_list.h"

uint64_t dictSdsHash(const void *key)
{
	return dictGenHashFunction((unsigned char *)key, sdslen((char *)key));
}

uint64_t dictSdsCaseHash(const void *key)
{
	return dictGenCaseHashFunction((unsigned char *)key, sdslen((char *)key));
}

int dictSdsKeyCaseCompare(void *privdata, const void *key1, const void *key2)
{
	DICT_NOTUSED(privdata);
	return strcasecmp((const char *)key1, (const char *)key2) == 0;
}

void dictBufferDestructor(void *privdata, void *val)
{
	DICT_NOTUSED(privdata);
	zfree(val);
}
void dictSdsDestructor(void *privdata, void *val)
{
	DICT_NOTUSED(privdata);
	sdsfree((sds)val);
}
void dictListDestructor(void *privdata, void *val)
{
	DICT_NOTUSED(privdata);
	sts_struct_list_destroy((s_sts_struct_list *)val);
}

dictType bufferDictType = {
	dictSdsCaseHash,	   /* hash function */
	NULL,				   /* key dup */
	NULL,				   /* val dup */
	dictSdsKeyCaseCompare, /* key compare */
	dictSdsDestructor,	 /* key destructor */
	dictBufferDestructor   /* val destructor */
};
dictType numberDictType = {
	dictSdsCaseHash,	   /* hash function */
	NULL,				   /* key dup */
	NULL,				   /* val dup */
	dictSdsKeyCaseCompare, /* key compare */
	dictSdsDestructor,	 /* key destructor */
	NULL				   /* val destructor */
};
dictType sdsDictType = {
	dictSdsCaseHash,	   /* hash function */
	NULL,				   /* key dup */
	NULL,				   /* val dup */
	dictSdsKeyCaseCompare, /* key compare */
	dictSdsDestructor,	 /* key destructor */
	dictSdsDestructor	  /* val destructor */
};
dictType listDictType = {
	dictSdsCaseHash,	   /* hash function */
	NULL,				   /* key dup */
	NULL,				   /* val dup */
	dictSdsKeyCaseCompare, /* key compare */
	dictSdsDestructor,	 /* key destructor */
	dictListDestructor	 /* val destructor */
};

//////////////////////////////////////////
//  s_sts_map_buffer 基础定义
///////////////////////////////////////////////

s_sts_map_buffer *sts_map_buffer_create()
{ //明确知道val的长度
	s_sts_map_buffer *map = dictCreate(&bufferDictType, NULL);
	return map;
};
void sts_map_buffer_destroy(s_sts_map_buffer *map_)
{
	dictRelease(map_);
};
void sts_map_buffer_clear(s_sts_map_buffer *map_)
{
	dictEmpty(map_, NULL);
};
void *sts_map_buffer_get(s_sts_map_buffer *map_, const char *key_)
{
	//??? 这里可能有问题
	dictEntry *he;
	sds key = sdsnew(key_);
	he = dictFind(map_, key);
	sdsfree(key);
	if (!he)
	{
		return NULL;
	}
	return dictGetVal(he);
};
int sts_map_buffer_set(s_sts_map_buffer *map_, const char *key_, void *value_)
{
	dictEntry *he;
	sds key = sdsnew(key_);
	he = dictFind(map_, key_);
	sdsfree(key);
	if (!he)
	{
		dictAdd(map_, sdsnew(key_), value_);
		return 0;
	}
	
	dictSetVal(map_, he, value_);
	return 0;
}

//////////////////////////////////////////
//  s_sts_map_int 基础定义
//////////////////////////////////////////
s_sts_map_pointer *sts_map_pointer_create()
{
	s_sts_map_pointer *map = dictCreate(&bufferDictType, NULL);
	return map;
};
//////////////////////////////////////////
//  s_sts_map_int 基础定义
//////////////////////////////////////////
s_sts_map_int *sts_map_int_create()
{
	s_sts_map_int *map = dictCreate(&numberDictType, NULL);
	return map;
};
uint64_t sts_map_int_get(s_sts_map_int *map_, const char *key_)
{
	dictEntry *he;
	he = dictFind(map_, key_);
	if (!he)
	{
		return 0;
	}
	return dictGetUnsignedIntegerVal(he);
};
int sts_map_int_set(s_sts_map_int *map_, const char *key_, uint64_t value_)
{
	dictEntry *he;
	he = dictFind(map_, key_);
	if (!he)
	{
		return 0;
	}
	dictSetUnsignedIntegerVal(he, value_);
	return 0;
}
//////////////////////////////////////////
//  s_sts_map_sds 基础定义
//////////////////////////////////////////
s_sts_map_sds *sts_map_sds_create()
{
	s_sts_map_sds *map = dictCreate(&sdsDictType, NULL);
	return map;
};
