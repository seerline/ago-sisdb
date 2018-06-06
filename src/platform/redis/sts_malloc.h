
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
#define sts_dict_delete dictDelete
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

#define __RELEASE__

#ifdef __RELEASE__
#define sts_malloc  malloc
#define sts_calloc(a)  calloc(1,a)
#define sts_realloc  realloc
#define sts_free free

inline void safe_memory_start(){};
inline void safe_memory_stop(){};

#else

#include <string.h>
#include <stdlib.h>

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

void safe_memory_start();
void safe_memory_stop();

extern s_memory_node *__memory_first, *__memory_last;

inline void safe_memory_newnode(void *__p__,int line_,const char *func_)
{
    s_memory_node *__n = (s_memory_node *)__p__; 
    __n->next = NULL; 
    __n->line = line_; 
    // memmove(__n->info, func_, MEMORY_INFO_SIZE); __n->info[MEMORY_INFO_SIZE - 1] = 0; 

    if (!__memory_first) { 
        __n->prev = NULL;  
        __memory_first = __n; 
        __memory_last = __memory_first; 
    }  else { 
        __n->prev = __memory_last; 
        __memory_last->next = __n; 
        __memory_last = __n; 
    }        
}   
inline void safe_memory_freenode(void *__p__)
{   
    s_memory_node *__n = (s_memory_node *)__p__; 
    if(__n->prev) { 
        __n->prev->next = __n->next; 
    } 
    else { 
        __memory_first = __n->next; 
    }  

    if(__n->next)  { 
        __n->next->prev = __n->prev; 
    } 
    else { 
        __memory_last = __n->prev; 
    } 
}


#define sts_malloc(__size__) ({   \
    void *__p = malloc(__size__ + MEMORY_NODE_SIZE); \
    safe_memory_newnode(__p, __LINE__,__func__); \
    (void *)((char *)__p + MEMORY_NODE_SIZE); \
}) \

#define sts_calloc(__size__)  ({   \
    void *__p = calloc(1, __size__ + MEMORY_NODE_SIZE); \
    safe_memory_newnode(__p, __LINE__,__func__); \
    (void *)((char *)__p + MEMORY_NODE_SIZE); \
}) \

#define sts_realloc(__m__, __size__)  ({   \
    void *__p = NULL; \
    if (!__m__) { \
        __p = malloc(__size__ + MEMORY_NODE_SIZE); \
    } else { \
        s_memory_node *__s = (s_memory_node *)((char *)__m__ - MEMORY_NODE_SIZE); \
        safe_memory_freenode(__s); \
        __p = realloc(__s, __size__ + MEMORY_NODE_SIZE); \
    } \
    safe_memory_newnode(__p, __LINE__,__func__); \
    (void *)((char *)__p + MEMORY_NODE_SIZE); \
}) \

#define sts_free(__m__) ({ \
    if (__m__) {  \
        void *__p = (char *)__m__ - MEMORY_NODE_SIZE;  \
        safe_memory_freenode(__p); \
        free(__p); \
    } \
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
