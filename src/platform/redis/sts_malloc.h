
#ifndef _STS_MALLOC_H
#define _STS_MALLOC_H

#include <stdio.h>
// #include <redismodule.h>

#define STS_MALLOC

#include <sds.h>
#include <adlist.h>
#include <dict.h>
// 这部分以后需要独立出去
#define STS_NOTUSED(V) ((void) V)

#define s_sts_list list
#define s_sts_list_create listCreate
#define s_sts_list_destroy listRelease

#define s_sts_list_node listNode

#define s_sts_sds sds
#define sts_sdsfree sdsfree
#define sts_sdsempty sdsempty
#define sts_sdsnew sdsnew
#define sts_sdsnewlen sdsnewlen
#define sts_sdscpylen sdscpylen
#define sts_sdslen sdslen
#define sts_sdsdup sdsdup
#define sts_sdscatlen sdscatlen

#define s_sts_dict dict
#define s_sts_dict_type dictType
#define s_sts_dict_entry dictEntry
#define s_sts_dict_iter dictIterator

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

// #define __RELEASE__

#ifdef __RELEASE__
#define sts_malloc  malloc
#define sts_calloc(a)  calloc(1,a)
#define sts_realloc  realloc
#define sts_free free

inline void check_memory_start(){};
inline void check_memory_stop(){};

#else

#define MEMORY_INFO_SIZE  32

#pragma pack(push,1)
typedef struct s_memory_node {
    char   info[MEMORY_INFO_SIZE];
    unsigned short line;  // 行数 
	struct s_memory_node * prev;     
	struct s_memory_node * next;
}s_memory_node;
#pragma pack(pop)

#define MEMORY_NODE_SIZE  (sizeof(s_memory_node))

void check_memory_start();
void check_memory_stop();

void check_memory_newnode(void *__p__,int line_,const char *func_);
void check_memory_freenode(void *__p__);

void sts_free(void *__p__);

#define sts_malloc(__size__) ({   \
    void *__p = malloc(__size__ + MEMORY_NODE_SIZE); \
    check_memory_newnode(__p, __LINE__,__func__); \
    (void *)((char *)__p + MEMORY_NODE_SIZE); \
}) \

#define sts_calloc(__size__)  ({   \
    void *__p = calloc(1, __size__ + MEMORY_NODE_SIZE); \
    check_memory_newnode(__p, __LINE__,__func__); \
    (void *)((char *)__p + MEMORY_NODE_SIZE); \
}) \

    // __m__==0 ??? 
#define sts_realloc(__m__, __size__)  ({   \
    void *__p = NULL; \
    if (!__m__) { \
        __p = malloc(__size__ + MEMORY_NODE_SIZE); \
    } else { \
        void *__s = (char *)__m__ - MEMORY_NODE_SIZE; \
        __p = realloc(__s, __size__ + MEMORY_NODE_SIZE); \
        if(__p != __s) { check_memory_freenode(__s); } \
    } \
    check_memory_newnode(__p, __LINE__,__func__); \
    (void *)((char *)__p + MEMORY_NODE_SIZE); \
}) \

#endif

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
