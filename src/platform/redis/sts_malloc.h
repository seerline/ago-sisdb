
#ifndef _STS_MALLOC_H
#define _STS_MALLOC_H

#include <stdio.h>
#include <redismodule.h>

#define STS_MALLOC

#include <sds.h>
#include <adlist.h>
#include <dict.h>
// 这部分以后需要独立出去
#define s_sts_sds sds
#define s_sts_list list
#define s_sts_list_node listNode

#define s_sts_dict dict
#define s_sts_dict_type dictType
#define s_sts_dict_entry dictEntry
#define s_sts_dict_iter dictIterator

#define sts_sdsfree sdsfree
#define sts_sdsempty sdsempty
#define sts_sdsnew sdsnew
#define sts_sdsnewlen sdsnewlen
#define sts_sdscpylen sdscpylen
#define sts_sdslen sdslen

#define STS_NOTUSED(V) ((void) V)
#define sts_dict_add dictAdd
#define sts_dict_find dictFind
#define sts_dict_empty dictEmpty
#define sts_dict_setval dictSetVal
#define sts_dict_getval dictGetVal
#define sts_dict_getkey dictGetKey

#define sts_dict_get_iter dictGetSafeIterator
#define sts_dict_next dictNext
#define sts_dict_iter_free dictReleaseIterator

#define sts_dict_create dictCreate
#define sts_dict_destroy dictRelease
#define sts_dict_get_uint dictGetUnsignedIntegerVal
#define sts_dict_set_uint dictSetUnsignedIntegerVal
#define sts_dict_hash_func dictGenHashFunction
#define sts_dict_casehash_func dictGenCaseHashFunction

#define sts_malloc  malloc
#define sts_calloc(a)  calloc(1,a)
#define sts_realloc  realloc
#define sts_free free

// #define sts_malloc  RedisModule_Alloc
// #define sts_calloc(a)  RedisModule_Calloc(1,a)
// #define sts_realloc  RedisModule_Realloc
// #define sts_free RedisModule_Free

// #include "zmalloc.h"
// #define sts_malloc  zmalloc
// #define sts_calloc(a)  zcalloc(1,a)
// #define sts_realloc  zrealloc
// #define sts_free zfree

#endif //_STS_OS_H
