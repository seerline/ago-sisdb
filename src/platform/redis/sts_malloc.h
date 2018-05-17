
#ifndef _STS_MALLOC_H
#define _STS_MALLOC_H

#include <stdio.h>
#include <redismodule.h>

#define STS_MALLOC

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

#define sts_sprintf snprintf

#endif //_STS_OS_H
