
#ifndef _OS_MALLOC_H
#define _OS_MALLOC_H

// #define __RELEASE__

#ifdef __RELEASE__ 

#define sis_malloc  malloc
#define sis_calloc(a)  calloc(1,a)
#define sis_realloc  realloc
#define sis_free free

static inline void safe_memory_start(){};
static inline void safe_memory_stop(){};

#define SIS_MALLOC(__type__,__val__) (__type__ *)sis_malloc(sizeof(__type__)); \
    memset(__val__, 0, sizeof(__type__));

#else

#include <string.h>
#include <stdlib.h>
#include <os_thread.h>

#define MEMORY_INFO_SIZE  32

// #pragma pack(push,1)
// 不可以对齐为 1 个字节 对于某些操作体可能会出问题 
typedef struct s_memory_node {
    char   info[MEMORY_INFO_SIZE];
    unsigned int size; 
    unsigned int line;  
	struct s_memory_node * prev;     
	struct s_memory_node * next;
}s_memory_node;

#define MEMORY_NODE_SIZE  (sizeof(s_memory_node))

extern s_memory_node *__memory_first, *__memory_last;
extern s_sis_mutex_t  __memory_mutex;

void safe_memory_start();
void safe_memory_stop();

static inline void safe_memory_newnode(void *__p__,unsigned int size_, int line_,const char *func_)
{
    sis_mutex_lock(&__memory_mutex);
    s_memory_node *__n = (s_memory_node *)__p__; 
    __n->size = size_;
    __n->prev = NULL; 
    __n->next = NULL; 
    __n->line = line_; 
    // memmove(__n->info, func_, MEMORY_INFO_SIZE); __n->info[MEMORY_INFO_SIZE - 1] = 0; 
    for (int i = 0; i < MEMORY_INFO_SIZE - 1; i++)
    {
        __n->info[i] = func_[i];
        if (!func_[i]) { break; }
    }
    __n->info[MEMORY_INFO_SIZE - 1] = 0; 
    
    if (!__memory_first) { 
        __n->prev = NULL;  
        __memory_first = __n; 
        __memory_last = __memory_first; 
    }  else { 
        __n->prev = __memory_last; 
        __memory_last->next = __n; 
        __memory_last = __n; 
    } 
    sis_mutex_unlock(&__memory_mutex);    
}   
static inline void safe_memory_freenode(void *__p__)
{   
    sis_mutex_lock(&__memory_mutex);
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
    sis_mutex_unlock(&__memory_mutex);    
}

#define sis_malloc(__size__) ({   \
    void *__p = malloc(__size__ + MEMORY_NODE_SIZE); \
    safe_memory_newnode(__p, __size__, __LINE__,__func__); \
    (void *)((char *)__p + MEMORY_NODE_SIZE); \
}) \

#define SIS_MALLOC(__type__,__val__) (__type__ *)sis_malloc(sizeof(__type__)); \
    memset(__val__, 0, sizeof(__type__));

#define sis_calloc(__size__)  ({   \
    void *__p = calloc(1, __size__ + MEMORY_NODE_SIZE); \
    safe_memory_newnode(__p, __size__, __LINE__,__func__); \
    (void *)((char *)__p + MEMORY_NODE_SIZE); \
}) \

/*
#define sis_calloc(__size__)  ({   \
    void *__p = malloc(__size__ + MEMORY_NODE_SIZE); \
    memset(__p, 0, __size__ + MEMORY_NODE_SIZE); \
    safe_memory_newnode(__p, __size__, __LINE__,__func__); \
    (void *)((char *)__p + MEMORY_NODE_SIZE); \
}) \
*/
#define sis_free(__m__) ({ \
    if (__m__) {  \
        void *__p = (char *)__m__ - MEMORY_NODE_SIZE;  \
        safe_memory_freenode(__p); \
        free(__p); \
    } \
}) \

#define sis_realloc(__m__, __size__)  ({   \
    void *__p = NULL; \
    if (!__m__) { \
        __p = malloc(__size__ + MEMORY_NODE_SIZE); \
    } else { \
        s_memory_node *__s = (s_memory_node *)((char *)__m__ - MEMORY_NODE_SIZE); \
        safe_memory_freenode((void *)__s); \
        __p = realloc((void *)__s, __size__ + MEMORY_NODE_SIZE); \
    } \
    safe_memory_newnode(__p, __size__, __LINE__,__func__); \
    (void *)((char *)__p + MEMORY_NODE_SIZE); \
}) \

#endif

#endif //_OS_MALLOC_H
