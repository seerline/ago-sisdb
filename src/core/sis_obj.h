#ifndef _SIS_OBJ_H
#define _SIS_OBJ_H

#include "sis_core.h"
#include "sis_malloc.h"
#include "sis_memory.h"
#include "sis_net.node.h"
#include "sis_list.h"


#define SIS_OBJECT_MAX_REFS 0xFFFFFFFF

// 定义一个网络数据的结构，减少内存拷贝，方便数据传递，增加程序阅读性
#pragma pack(push,1)

#define SIS_OBJECT_SDS     0  // 字符串
#define SIS_OBJECT_MEMORY  1  // sis_memory 结构体的指针
#define SIS_OBJECT_LIST    2  // sis_struct_list 结构体的指针
#define SIS_OBJECT_NETMSG  3  // s_sis_net_message 指针
#define SIS_OBJECT_NODES   4  // 节点列表

typedef struct s_sis_object {
    unsigned char  style;
    volatile unsigned int   refs;  // 引用数 一般为1, 最大嵌套不超过 SIS_OBJECT_MAX_REFS 超过返回空
    void          *ptr;
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

#define SIS_OBJ_GET_SIZE(v) sis_object_getsize(v)
#define SIS_OBJ_GET_CHAR(v) sis_object_getchar(v)

#define SIS_OBJ_SDS(v) (v->style == SIS_OBJECT_SDS ? (s_sis_sds)(v->ptr) : NULL)
#define SIS_OBJ_MEMORY(v) (v->style == SIS_OBJECT_MEMORY ? (s_sis_memory *)(v->ptr) : NULL)
#define SIS_OBJ_NETMSG(v) (v->style == SIS_OBJECT_NETMSG ? (s_sis_net_message *)(v->ptr) : NULL)
 

#ifdef __cplusplus
}
#endif

// typedef struct s_sis_share_node {
//     // struct s_sis_share_node *prev;
//     struct s_sis_share_node *next;
//     volatile int16 cites;    // 引用数,避免反复拷贝 必须是原子操作
//     uint64 serial;   // 序列值,定位查询时起作用
//     uint32 size;     // key + value 的总长度
//     void *key;       // 头描述
//     void *value;     // 数据区
//     // void (*kfree)(void *ptr);  // key释放
//     void (*vfree)(void *);  // value释放
// } s_sis_share_node;

// // 一写多读的共享数据，
// typedef struct s_sis_share_list {
//     s_sis_sds           key;     // 链表名字 一般通过该名字来隔离
//     s_sis_mutex_rw      mutex;   // 多读一写
//     size_t              limit;   // 最大限度
//     size_t              size;    // 数据的长度 + count*sizeof(s_sis_share_node)为总长度
//     size_t              count;   // node 的节点数
//     uint64              serial;  // 每个消息一个唯一标记 比tail的永远大1
//     s_sis_share_node   *head;
//     s_sis_share_node   *tail;
// } s_sis_share_list;

// #ifdef __cplusplus
// extern "C" {
// #endif
// // s_sis_share_node *sis_share_node_create();
// void sis_share_node_destroy(s_sis_share_node *); 

// s_sis_share_list *sis_share_list_create(const char *, int64 limit_);
// void sis_share_list_destroy(s_sis_share_list *); 
// s_sis_share_node *sis_share_node_clone(s_sis_share_node *src_);

// // 会发生一次拷贝，push后用户可以释放内存 写权限
// int sis_share_list_push(s_sis_share_list *obj_, 
//     void *key_, size_t klen_,                       // vfree_ == NULL 表示要申请内存
//     void *val_, size_t vlen_, sis_free_define *vfree_); // vfree_ 不发生拷贝，但需要指定传入的数据的释放函数

// // 以下三个函数如果返回不为空 不用的时候需要执行 sis_share_node_destroy
// s_sis_share_node *sis_share_list_first(s_sis_share_list *obj_);
// s_sis_share_node *sis_share_list_last(s_sis_share_list *obj_);
// s_sis_share_node *sis_share_list_next(s_sis_share_list *obj_, s_sis_share_node *cursor_);

// // s_sis_share_node *cursor = sis_share_list_first(...)
// // s_sis_share_node *node = sis_share_list_next(..., cursor);
// // while(node)
// // {
// //     node = sis_share_list_next(..., cursor);
// //     if (node)
// //     {
// //         cursor = node;
// //         .... option .....
// //     }
// // }
// // sis_share_node_destroy(cursor);

// // s_sis_share_node *sis_share_list_cut(s_sis_share_list *obj_, int *count_);  
// // 0 为全部取出 1 取一个  返回实际的数量 写权限
// void sis_share_list_clear(s_sis_share_list *obj_);

// #ifdef __cplusplus
// }
// #endif

#endif
// _SIS_NET_OBJ_H
