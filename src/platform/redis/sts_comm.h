#ifndef _STS_COMM_H
#define _STS_COMM_H


//////////////////////////////////////////////
//  转换redis的函数定义方式，方便未来转移到其他平台
//////////////////////////////////////////////


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

#define STS_MODULE_LIST_HEAD REDISMODULE_LIST_HEAD
#define STS_MODULE_LIST_TAIL REDISMODULE_LIST_TAIL

#define STS_MODULE_TYPE_METHOD_VERSION REDISMODULE_TYPE_METHOD_VERSION
// struct define
#define s_sts_module_context RedisModuleCtx
#define s_sts_module_string RedisModuleString
#define s_sts_module_key RedisModuleKey
#define s_sts_module_call_reply RedisModuleCallReply
#define s_sts_module_block_client RedisModuleBlockedClient
#define s_sts_module_type_methods  RedisModuleTypeMethods
#define s_sts_module_type RedisModuleType
#define s_sts_module_io RedisModuleIO
#define s_sts_module_digest RedisModuleDigest

// function defined
#define sts_module_on_load RedisModule_OnLoad
#define sts_module_init RedisModule_Init
#define sts_module_create_command RedisModule_CreateCommand

#define sts_module_memory_init RedisModule_AutoMemory
#define sts_module_wrong_arity RedisModule_WrongArity

#define sts_module_call RedisModule_Call
#define sts_module_call_reply_len RedisModule_CallReplyLength
#define sts_module_call_reply_array RedisModule_CallReplyArrayElement
#define sts_module_call_reply_free RedisModule_FreeCallReply
#define sts_module_call_reply_int64 RedisModule_CallReplyInteger

#define sts_module_reply_with_int64 RedisModule_ReplyWithLongLong
#define sts_module_reply_with_error RedisModule_ReplyWithError
#define sts_module_reply_with_string RedisModule_ReplyWithString
#define sts_module_reply_with_null RedisModule_ReplyWithNull
#define sts_module_reply_with_simple_string RedisModule_ReplyWithSimpleString

#define sts_module_string_create RedisModule_CreateString
#define sts_module_string_create_printf RedisModule_CreateStringPrintf
#define sts_module_string_get RedisModule_StringPtrLen
#define sts_module_string_free RedisModule_FreeString

#define sts_module_key_open RedisModule_OpenKey
#define sts_module_key_type RedisModule_KeyType
#define sts_module_key_get_value RedisModule_StringDMA
#define sts_module_key_set_value RedisModule_StringSet
#define sts_module_key_close RedisModule_CloseKey

#define sts_module_value_len RedisModule_ValueLength

#define sts_module_list_push RedisModule_ListPush
#define sts_module_list_pop RedisModule_ListPop

#define sts_module_block_client RedisModule_BlockClient
#define sts_module_block_abort RedisModule_AbortBlock
#define sts_module_unblock_client RedisModule_UnblockClient
#define sts_module_block_getdata RedisModule_GetBlockedClientPrivateData

#define sts_module_get_safe_context RedisModule_GetThreadSafeContext
#define sts_module_free_safe_context RedisModule_FreeThreadSafeContext

#define sts_module_type_create RedisModule_CreateDataType
#define sts_module_type_emit_aof RedisModule_EmitAOF
#define sts_module_type_get_type RedisModule_ModuleTypeGetType

#define sts_module_type_set_value RedisModule_ModuleTypeSetValue
#define sts_module_type_get_value RedisModule_ModuleTypeGetValue
#define sts_module_type_save_uint64 RedisModule_SaveUnsigned
#define sts_module_type_load_uint64 RedisModule_LoadUnsigned
#define sts_module_type_save_int64 RedisModule_SaveSigned
#define sts_module_type_load_int64 RedisModule_LoadSigned
#define sts_module_type_save_str RedisModule_SaveString
#define sts_module_type_load_str RedisModule_LoadString
#define sts_module_type_save_buffer RedisModule_SaveStringBuffer
#define sts_module_type_load_buffer RedisModule_LoadStringBuffer
#define sts_module_type_save_double RedisModule_SaveDouble
#define sts_module_type_load_double RedisModule_LoadDouble
#define sts_module_type_save_float RedisModule_SaveFloat
#define sts_module_type_load_float RedisModule_LoadFloat

#define sts_module_alloc RedisModule_Alloc
#define sts_module_free RedisModule_Free

#define sts_module_not_used REDISMODULE_NOT_USED



#endif  /* _STS_COMM_H */
