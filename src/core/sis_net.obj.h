#ifndef _SIS_NET_OBJ_H
#define _SIS_NET_OBJ_H

#include "sis_core.h"
#include "sis_malloc.h"
#include "sis_thread.h"
#include <sis_math.h>

#define SIS_OBJECT_MAX_REFS 0x7FFFFFFF

// 定义一个网络数据的结构，减少内存拷贝，方便数据传递，增加程序阅读性
#pragma pack(push,1)

#define SIS_OBJECT_SDS   0  // 字符串
#define SIS_OBJECT_LIST  1  // sis_struct_list 结构体的指针
#define SIS_OBJECT_NODE  2  // sis_socket_node 专用于网络传输的链表

typedef struct s_sis_object {
    unsigned char style;
    int  refs;  // 引用数 一般为1, 最大嵌套不超过 MAX_INT 超过就会出错 
    void *ptr;
} s_sis_object;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif
s_sis_object *sis_object_create(int , void *);
void sis_object_incr(s_sis_object *o); // 再活一次
void sis_object_decr(s_sis_object *o); 
#define sis_object_destroy sis_object_decr

void sis_object_set(s_sis_object *obj_, int style_, void *ptr);

#ifdef __cplusplus
}
#endif
typedef void (_cb_free)(void *in_);

typedef struct s_sis_share_node {
    struct s_sis_share_node *prev;
    struct s_sis_share_node *next;
    unsigned int size;
    void *value;
    void (*free)(void *ptr);  // 释放
} s_sis_share_node;

typedef struct s_sis_share_list {
    s_sis_sds           key;   // 链表名字 一般通过该名字来隔离
    s_sis_mutex_rw      mutex;   // 多读一写
    size_t              limit_;  // 最大长度
    size_t              count;   // node 的节点数
    s_sis_share_node   *head;
    s_sis_share_node   *tail;
} s_sis_share_list;

#ifdef __cplusplus
extern "C" {
#endif
s_sis_share_list *sis_share_list_create(const char *, int64 limit_);
void sis_share_list_destroy(s_sis_share_list *); 

// 会发生一次拷贝，push后用户可以释放内存
int sis_share_list_push(s_sis_share_list *obj_, void *in_, size_t ilen_); // 写权限
// 不发生拷贝，但需要指定传入的数据的释放函数
int sis_share_list_push_void(s_sis_share_list *obj_, void *in_, _cb_free *free_); // 写权限
s_sis_share_node *sis_share_list_get(s_sis_share_list *obj_, int *count_);  
// 0 为全部取出 1 取一个  返回实际的数量 写权限
void sis_share_list_clear(s_sis_share_list *obj_);

#ifdef __cplusplus
}
#endif

#endif
// _SIS_NET_OBJ_H
