#ifndef _COMM_H
#define _COMM_H


//////////////////////////////////////////////
//  转换redis的函数定义方式，方便未来转移到其他平台
//////////////////////////////////////////////
#define _PLATFORM_REDIS

#ifdef _PLATFORM_REDIS

#include "redismodule.h"
// const define
#define STS_MODULE_OK REDISMODULE_OK
#define STS_MODULE_ERROR REDISMODULE_ERR
#define STS_MODULE_VER  REDISMODULE_APIVER_1

#define STS_MODULE_ERROR_WRONGTYPE REDISMODULE_ERRORMSG_WRONGTYPE

#define STS_MODULE_KEYTYPE_STRING REDISMODULE_KEYTYPE_STRING
#define STS_MODULE_KEYTYPE_EMPTY REDISMODULE_KEYTYPE_EMPTY

#define STS_MODULE_READ REDISMODULE_READ
#define STS_MODULE_WRITE REDISMODULE_WRITE

// struct define
#define s_sts_module_context RedisModuleCtx
#define s_sts_module_string RedisModuleString
#define s_sts_module_key RedisModuleKey

// function defined
#define sts_module_on_load RedisModule_OnLoad
#define sts_module_init RedisModule_Init
#define sts_module_create_command RedisModule_CreateCommand

#define sts_module_memory_init RedisModule_AutoMemory

#define sts_module_reply_with_int64 RedisModule_ReplyWithLongLong
#define sts_module_reply_with_error RedisModule_ReplyWithError
#define sts_module_reply_with_string RedisModule_ReplyWithString
#define sts_module_reply_with_null RedisModule_ReplyWithNull
#define sts_module_reply_with_simple_string RedisModule_ReplyWithSimpleString

#define sts_module_string_create RedisModule_CreateString
#define sts_module_string_create_printf RedisModule_CreateStringPrintf
#define sts_module_string_get RedisModule_StringPtrLen

#define sts_module_key_open RedisModule_OpenKey
#define sts_module_key_type RedisModule_KeyType
#define sts_module_key_get_value RedisModule_StringDMA
#define sts_module_key_set_value RedisModule_StringSet
#define sts_module_key_close RedisModule_CloseKey

#define sts_module_wrong_arity RedisModule_WrongArity

#define sts_module_not_used REDISMODULE_NOT_USED


#endif


#endif  /* _CONF_H */
