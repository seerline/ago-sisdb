
#ifndef _OS_MALLOC_H
#define _OS_MALLOC_H

#define __RELEASE__

#include <sis_os.h>

#ifdef __RELEASE__

#define sis_malloc  malloc
#define sis_calloc(a)  calloc(1,a)
#define sis_realloc  realloc
#define sis_free free

inline void safe_memory_start(){};
inline void safe_memory_stop(){};

#define SIS_MALLOC(__type__,__val__) (__type__ *)sis_malloc(sizeof(__type__)); \
    memset(__val__, 0, sizeof(__type__));

#else

#include <string.h>
#include <stdlib.h>

#define MEMORY_INFO_SIZE  32

typedef struct s_memory_node {
    char   info[MEMORY_INFO_SIZE];
    unsigned int line;  //  
	struct s_memory_node * prev;     
	struct s_memory_node * next;
}s_memory_node;

#define MEMORY_NODE_SIZE  (sizeof(s_memory_node))

extern s_memory_node *__memory_first, *__memory_last;

void safe_memory_start();
void safe_memory_stop();

inline void safe_memory_newnode(void *__p__,int line_,const char *func_)
{
    s_memory_node *__n = (s_memory_node *)__p__; 
    __n->next = NULL; 
    __n->line = line_; 
    memmove(__n->info, func_, MEMORY_INFO_SIZE); __n->info[MEMORY_INFO_SIZE - 1] = 0; 

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

#define sis_malloc(__size__)  ({   \
	void *__p = malloc(__size__ + MEMORY_NODE_SIZE); \
	safe_memory_newnode(__p, __LINE__, __func__); \
	(void *)((char *)__p + MEMORY_NODE_SIZE); \
})

#define SIS_MALLOC(__type__,__val__) (__type__ *)sis_malloc(sizeof(__type__)); \
    memset(__val__, 0, sizeof(__type__));

#define sis_calloc(__size__)  ({   \
    void *__p = calloc(1, __size__ + MEMORY_NODE_SIZE); \
    safe_memory_newnode(__p, __LINE__,__func__); \
    (void *)((char *)__p + MEMORY_NODE_SIZE); \
})

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
})

#define sis_free(__m__) ({ \
    if (__m__) {  \
        void *__p = (char *)__m__ - MEMORY_NODE_SIZE;  \
        safe_memory_freenode(__p); \
        free(__p); \
    } \
})

#endif

#endif //_OS_MALLOC_H
