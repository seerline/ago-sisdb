
#ifndef _SIS_MALLOC_H
#define _SIS_MALLOC_H

#include <stdio.h>
// #include <redismodule.h>
#include <os_malloc.h>

#define SIS_MALLOC

#include <sds.h>
#include <adlist.h>
#include <dict.h>
// 这部分以后需要独立出去
#define SIS_NOTUSED(V) ((void) V)

#define s_sis_list list
#define s_sis_list_create listCreate
#define s_sis_list_destroy listRelease

#define s_sis_list_node listNode

#define s_sis_sds sds
#define sis_sdsfree sdsfree
#define sis_sdsclear sdsclear
#define sis_sdsempty sdsempty
#define sis_sdsnew sdsnew
#define sis_sdsnewlen sdsnewlen
#define sis_sdscpylen sdscpylen
#define sis_sdslen sdslen
#define sis_sdsdup sdsdup
#define sis_sdscatlen sdscatlen
#define sis_sdscat sdscat
#define sis_sdscatfmt sdscatfmt

#define s_sis_dict dict
#define s_sis_dict_type dictType
#define s_sis_dict_entry dictEntry
#define s_sis_dict_iter dictIterator

#define sis_dict_add dictAdd
#define sis_dict_delete dictDelete
#define sis_dict_fetch_value dictFetchValue
#define sis_dict_find dictFind
#define sis_dict_empty dictEmpty
#define sis_dict_setval dictSetVal
#define sis_dict_getval dictGetVal
#define sis_dict_getkey dictGetKey

#define sis_dict_get_iter dictGetSafeIterator
#define sis_dict_next dictNext
#define sis_dict_iter_free dictReleaseIterator

#define sis_dict_create dictCreate
#define sis_dict_destroy dictRelease
#define sis_dict_get_uint dictGetUnsignedIntegerVal
#define sis_dict_set_uint dictSetUnsignedIntegerVal
#define sis_dict_hash_func dictGenHashFunction
#define sis_dict_casehash_func dictGenCaseHashFunction


// #define sis_malloc  RedisModule_Alloc
// #define sis_calloc(a)  RedisModule_Calloc(1,a)
// #define sis_realloc  RedisModule_Realloc
// #define sis_free RedisModule_Free

// #include "zmalloc.h"
// #define sis_malloc  zmalloc
// #define sis_calloc(a)  zcalloc(1,a)
// #define sis_realloc  zrealloc
// #define sis_free zfree

#endif //_SIS_OS_H
