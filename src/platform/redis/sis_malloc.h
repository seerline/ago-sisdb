
#ifndef _SIS_MALLOC_H
#define _SIS_MALLOC_H

#include <stdio.h>
// #include <redismodule.h>

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
#define sis_sdsempty sdsempty
#define sis_sdsnew sdsnew
#define sis_sdsnewlen sdsnewlen
#define sis_sdscpylen sdscpylen
#define sis_sdslen sdslen
#define sis_sdsdup sdsdup
#define sis_sdscatlen sdscatlen
#define sis_sdscat sdscat

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

#define __RELEASE__

#ifdef __RELEASE__
#define sis_malloc  malloc
#define sis_calloc(a)  calloc(1,a)
#define sis_realloc  realloc
#define sis_free free

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


#define sis_malloc(__size__) ({   \
    void *__p = malloc(__size__ + MEMORY_NODE_SIZE); \
    safe_memory_newnode(__p, __LINE__,__func__); \
    (void *)((char *)__p + MEMORY_NODE_SIZE); \
}) \

#define sis_calloc(__size__)  ({   \
    void *__p = calloc(1, __size__ + MEMORY_NODE_SIZE); \
    safe_memory_newnode(__p, __LINE__,__func__); \
    (void *)((char *)__p + MEMORY_NODE_SIZE); \
}) \

#define sis_realloc(__m__, __size__)  ({   \
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

#define sis_free(__m__) ({ \
    if (__m__) {  \
        void *__p = (char *)__m__ - MEMORY_NODE_SIZE;  \
        safe_memory_freenode(__p); \
        free(__p); \
    } \
}) \

#endif

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
