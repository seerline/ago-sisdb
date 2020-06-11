#ifndef _SIS_MESSAGE_H
#define _SIS_MESSAGE_H

#include <sis_core.h>
#include <sis_sds.h>
#include <sis_map.h>
#include <sis_list.h>
#include <sis_method.h>

#define SIS_MESSGE_TYPE_OWNER    0  // 自定义
#define SIS_MESSGE_TYPE_INT      1
#define SIS_MESSGE_TYPE_DOUBLE   2
#define SIS_MESSGE_TYPE_SDS      3
#define SIS_MESSGE_TYPE_STRLIST  4  // 字符串列表
#define SIS_MESSGE_TYPE_NODE     5
#define SIS_MESSGE_TYPE_BOOL     6
#define SIS_MESSGE_TYPE_METHOD   9

// 增加动态消息结构
typedef struct s_sis_message_unit {
    int8       style;
    void      *value;       // 数据或函数指针
	void(*free)(void *);    // 当前数值的释放函数 默认为 sis_free
} s_sis_message_unit;
/////////////////////////////////////////////////
//  message
/////////////////////////////////////////////////
// 分开定义是为了解决key同民问题，支持同一个名字下各种类型数据
typedef struct s_sis_message {
    void              *parent;   // 信息的来源 - 主要用于回调函数的参数
    s_sis_map_pointer *maps ;     // 映射表
} s_sis_message;

#ifdef __cplusplus
extern "C" {
#endif

s_sis_message *sis_message_create();
void sis_message_destroy(s_sis_message *);

void sis_message_del(s_sis_message *, const char*);

void sis_message_set_int(s_sis_message *, const char*, int64);
void sis_message_set_bool(s_sis_message *msg_, const char* key_, bool in_);
void sis_message_set_double(s_sis_message *, const char*, double);
void sis_message_set_str(s_sis_message *, const char*, char*, size_t );
// void sis_message_set_strlist(s_sis_message *, const char*, s_sis_string_list *);
void sis_message_set_method(s_sis_message *, const char*, sis_method_define *);
// 用户自定义结构体 如果 sis_free_define = NULL 默认调用 sis_free
void sis_message_set(s_sis_message *, const char*, void *, sis_free_define *);

int64 sis_message_get_int(s_sis_message *, const char*);
bool sis_message_get_bool(s_sis_message *msg_, const char*);
double sis_message_get_double(s_sis_message *, const char*);
s_sis_sds sis_message_get_str(s_sis_message *, const char*);
// s_sis_string_list *sis_message_get_strlist(s_sis_message *, const char*);
sis_method_define *sis_message_get_method(s_sis_message *, const char*);
// 用户自定义结构体 如果 sis_free_define = NULL 默认调用 sis_free
void *sis_message_get(s_sis_message *, const char*);

#ifdef __cplusplus
}
#endif


#endif