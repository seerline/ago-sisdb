#ifndef _SIS_OBJ_H
#define _SIS_OBJ_H

#include "sis_core.h"
#include "sis_malloc.h"
#include "sis_memory.h"
#include "sis_net.msg.h"
#include "sis_list.h"
#include "sis_nodelist.h"

#define SIS_OBJECT_MAX_REFS 0xFFFFFFFF

// 定义一个网络数据的结构，减少内存拷贝，方便数据传递，增加程序阅读性
#pragma pack(push,1)

#define SIS_OBJECT_INT     0xFF  // 整数 测试时使用
#define SIS_OBJECT_SDS     0  // 字符串
#define SIS_OBJECT_MEMORY  1  // sis_memory 结构体的指针
#define SIS_OBJECT_LIST    2  // sis_struct_list 结构体的指针
#define SIS_OBJECT_NETMSG  3  // s_sis_net_message 指针
/**
 * @brief 网络数据的顶层封装结构，用以封装SDS字符串、sis_memory、sis_struct_list和s_sis_net_message，其作用与C#里面Object类相似
 */
typedef struct s_sis_object {
    //QQQ 这里是否可以使用枚举类型？
    unsigned char           style;
    unsigned int            refs;  // 引用数 一般为1, 最大嵌套不超过 SIS_OBJECT_MAX_REFS 超过返回空
    void                   *ptr;
} s_sis_object;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif
s_sis_object *sis_object_create(int , void *);
// 可能地址变更
s_sis_object *sis_object_incr(s_sis_object *o); // 再活一次
void sis_object_decr(void *o); 
#define sis_object_destroy sis_object_decr

size_t sis_object_getsize(void *);
char * sis_object_getchar(void *);

#define SIS_OBJ_GET_SIZE(v) ((v) ? sis_object_getsize(v) : 0)
#define SIS_OBJ_GET_CHAR(v) ((v) ? sis_object_getchar(v) : NULL)

#define SIS_OBJ_SDS(v)    ((v)->style == SIS_OBJECT_SDS    ? (s_sis_sds)((v)->ptr) : NULL)
#define SIS_OBJ_LIST(v)   ((v)->style == SIS_OBJECT_LIST   ? (s_sis_struct_list *)((v)->ptr) : NULL)
#define SIS_OBJ_MEMORY(v) ((v)->style == SIS_OBJECT_MEMORY ? (s_sis_memory *)((v)->ptr) : NULL)
#define SIS_OBJ_NETMSG(v) ((v)->style == SIS_OBJECT_NETMSG ? (s_sis_net_message *)((v)->ptr) : NULL)
 

#ifdef __cplusplus
}
#endif


#endif
// _SIS_NET_OBJ_H
