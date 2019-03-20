#ifndef _SIS_COMM_H
#define _SIS_COMM_H


//////////////////////////////////////////////
//  转换redis的函数定义方式，方便未来转移到其他平台
//////////////////////////////////////////////

#define REDISMODULE_EXPERIMENTAL_API

#include "redismodule.h"
// const define
#define SIS_MODULE_OK    REDISMODULE_OK
#define SIS_MODULE_ERROR REDISMODULE_ERR
#define SIS_MODULE_VER   REDISMODULE_APIVER_1

#define SIS_MODULE_ERROR_WRONGTYPE REDISMODULE_ERRORMSG_WRONGTYPE

#define SIS_MODULE_KEYTYPE_STRING REDISMODULE_KEYTYPE_STRING
#define SIS_MODULE_KEYTYPE_EMPTY  REDISMODULE_KEYTYPE_EMPTY

#define SIS_MODULE_REPLY_UNKNOWN  REDISMODULE_REPLY_UNKNOWN
#define SIS_MODULE_REPLY_STRING   REDISMODULE_REPLY_STRING
#define SIS_MODULE_REPLY_ERROR    REDISMODULE_REPLY_ERROR
#define SIS_MODULE_REPLY_INTEGER  REDISMODULE_REPLY_INTEGER
#define SIS_MODULE_REPLY_ARRAY    REDISMODULE_REPLY_ARRAY
#define SIS_MODULE_REPLY_NULL     REDISMODULE_REPLY_NULL

#define SIS_MODULE_READ  REDISMODULE_READ
#define SIS_MODULE_WRITE REDISMODULE_WRITE

#define SIS_MODULE_LIST_HEAD REDISMODULE_LIST_HEAD
#define SIS_MODULE_LIST_TAIL REDISMODULE_LIST_TAIL

#define SIS_MODULE_TYPE_METHOD_VERSION REDISMODULE_TYPE_METHOD_VERSION
// struct define

#pragma pack(push,1)
typedef struct s_sis_object_r {
    unsigned type:4;
    unsigned encoding:4;
    unsigned lru:24; 
    int refcount;
    void *ptr;
} s_sis_object_r;
#pragma pack(pop)

#define s_sis_module_context RedisModuleCtx
#define s_sis_module_string RedisModuleString
#define s_sis_module_key RedisModuleKey
#define s_sis_module_call_reply RedisModuleCallReply
#define s_sis_module_block_client RedisModuleBlockedClient
#define s_sis_module_type_methods  RedisModuleTypeMethods
#define s_sis_module_type RedisModuleType
#define s_sis_module_io RedisModuleIO
#define s_sis_module_digest RedisModuleDigest

// function defined
#define sis_module_on_load RedisModule_OnLoad
#define sis_module_on_unload RedisModule_OnUnLoad
#define sis_module_init RedisModule_Init
#define sis_module_create_command RedisModule_CreateCommand
// -------------------- //
#define sis_module_memory_init RedisModule_AutoMemory
#define sis_module_wrong_arity RedisModule_WrongArity

#define sis_module_call RedisModule_Call
#define sis_module_call_reply_len RedisModule_CallReplyLength
#define sis_module_call_reply_array RedisModule_CallReplyArrayElement
#define sis_module_call_reply_free RedisModule_FreeCallReply
#define sis_module_call_reply_int64 RedisModule_CallReplyInteger

#define sis_module_call_reply_type RedisModule_CallReplyType
#define sis_module_call_reply_with_string RedisModule_CallReplyStringPtr

#define sis_module_reply_with_int64 RedisModule_ReplyWithLongLong
#define sis_module_reply_with_error RedisModule_ReplyWithError
#define sis_module_reply_with_string RedisModule_ReplyWithString
#define sis_module_reply_with_buffer RedisModule_ReplyWithStringBuffer
#define sis_module_reply_with_null RedisModule_ReplyWithNull
#define sis_module_reply_with_simple_string RedisModule_ReplyWithSimpleString
#define sis_module_reply_with_reply RedisModule_ReplyWithCallReply

#define sis_module_string_create RedisModule_CreateString
#define sis_module_string_create_printf RedisModule_CreateStringPrintf
#define sis_module_string_get RedisModule_StringPtrLen
#define sis_module_string_free RedisModule_FreeString

#define sis_module_key_open RedisModule_OpenKey
#define sis_module_key_type RedisModule_KeyType
#define sis_module_key_get_value RedisModule_StringDMA
#define sis_module_key_set_value RedisModule_StringSet
#define sis_module_key_close RedisModule_CloseKey

#define sis_module_value_len RedisModule_ValueLength

#define sis_module_list_push RedisModule_ListPush
#define sis_module_list_pop RedisModule_ListPop

#define sis_module_block_client RedisModule_BlockClient
#define sis_module_block_abort RedisModule_AbortBlock
#define sis_module_unblock_client RedisModule_UnblockClient
#define sis_module_block_getdata RedisModule_GetBlockedClientPrivateData

#define sis_module_get_safe_context RedisModule_GetThreadSafeContext
#define sis_module_free_safe_context RedisModule_FreeThreadSafeContext

#define sis_module_type_create RedisModule_CreateDataType
#define sis_module_type_emit_aof RedisModule_EmitAOF
#define sis_module_type_get_type RedisModule_ModuleTypeGetType

#define sis_module_type_set_value RedisModule_ModuleTypeSetValue
#define sis_module_type_get_value RedisModule_ModuleTypeGetValue
#define sis_module_type_save_uint64 RedisModule_SaveUnsigned
#define sis_module_type_load_uint64 RedisModule_LoadUnsigned
#define sis_module_type_save_int64 RedisModule_SaveSigned
#define sis_module_type_load_int64 RedisModule_LoadSigned
#define sis_module_type_save_str RedisModule_SaveString
#define sis_module_type_load_str RedisModule_LoadString
#define sis_module_type_save_buffer RedisModule_SaveStringBuffer
#define sis_module_type_load_buffer RedisModule_LoadStringBuffer
#define sis_module_type_save_double RedisModule_SaveDouble
#define sis_module_type_load_double RedisModule_LoadDouble
#define sis_module_type_save_float RedisModule_SaveFloat
#define sis_module_type_load_float RedisModule_LoadFloat

#define sis_module_alloc RedisModule_Alloc
#define sis_module_free RedisModule_Free

#define sis_module_not_used REDISMODULE_NOT_USED



#endif  /* _SIS_COMM_H */
