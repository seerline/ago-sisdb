
#ifndef _STS_MEMORY_H
#define _STS_MEMORY_H

#include <stdio.h>
#include <redismodule.h>

#define STS_MALLOC  

#define sts_malloc  RedisModule_Alloc
#define sts_realloc  RedisModule_Realloc
#define sts_free RedisModule_Free

#define sts_sprintf snprintf


#endif //_STS_OS_H
