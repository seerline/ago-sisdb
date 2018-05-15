
#include "lw_map.h"
#include "lw_list.h"

uint64_t dictSdsHash(const void *key) {
	return dictGenHashFunction((unsigned char*)key, sdslen((char*)key));
}

uint64_t dictSdsCaseHash(const void *key) {
	return dictGenCaseHashFunction((unsigned char*)key, sdslen((char*)key));
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
	destroy_struct_list((s_struct_list *)val);
}


dictType bufferDictType = {
	dictSdsCaseHash,                /* hash function */
	NULL,                       /* key dup */
	NULL,                       /* val dup */
	dictSdsKeyCaseCompare,          /* key compare */
	dictSdsDestructor,         /* key destructor */
	dictBufferDestructor          /* val destructor */
};
dictType numberDictType = {
	dictSdsCaseHash,                /* hash function */
	NULL,                       /* key dup */
	NULL,                       /* val dup */
	dictSdsKeyCaseCompare,          /* key compare */
	dictSdsDestructor,         /* key destructor */
	NULL						/* val destructor */
};
dictType sdsDictType = {
	dictSdsCaseHash,                /* hash function */
	NULL,                       /* key dup */
	NULL,                       /* val dup */
	dictSdsKeyCaseCompare,          /* key compare */
	dictSdsDestructor,         /* key destructor */
	dictSdsDestructor			/* val destructor */
};
dictType listDictType = {
	dictSdsCaseHash,                /* hash function */
	NULL,                       /* key dup */
	NULL,                       /* val dup */
	dictSdsKeyCaseCompare,          /* key compare */
	dictSdsDestructor,         /* key destructor */
	dictListDestructor          /* val destructor */
};

//////////////////////////////////////////
//  s_map_buffer 基础定义
///////////////////////////////////////////////

s_map_buffer *create_map_buffer(){  //明确知道val的长度
	s_map_buffer *map = dictCreate(&bufferDictType, NULL);
	return map;
};
void destroy_map_buffer(s_map_buffer *map_)
{
	dictRelease(map_);
};
void clear_map_buffer(s_map_buffer *map_)
{
	dictEmpty(map_, NULL);
};
void *get_map_buffer(s_map_buffer *map_, const char *key_)
{
	dictEntry *he;
	he = dictFind(map_, key_);
	if (!he)
	{
		return NULL;
	}
	return dictGetVal(he);
};
int   set_map_buffer(s_map_buffer *map_, const char *key_, void *value_){
	dictEntry *he;
	he = dictFind(map_, key_);
	if (!he)
	{
		return NULL;
	}
	dictSetVal(map_, he, value_);
	return 0;
}

//////////////////////////////////////////
//  s_map_int 基础定义
//////////////////////////////////////////
s_map_int *create_map_int(){ 
	s_map_int *map = dictCreate(&numberDictType, NULL);
	return map;
};
uint64_t  get_map_int(s_map_int *map_, const char *key_){
	dictEntry *he;
	he = dictFind(map_, key_);
	if (!he)
	{
		return NULL;
	}
	return dictGetUnsignedIntegerVal(he);
};
int  set_map_int(s_map_int *map_, const char *key_, uint64_t value_){
	dictEntry *he;
	he = dictFind(map_, key_);
	if (!he)
	{
		return NULL;
	}
	dictSetUnsignedIntegerVal(he, value_);
	return 0;
}
//////////////////////////////////////////
//  s_map_sds 基础定义
//////////////////////////////////////////
s_map_sds *create_map_sds(){
	s_map_sds *map = dictCreate(&sdsDictType, NULL);
	return map;
};


