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
typedef void (_share_free)(void *in_);

typedef struct s_sis_share_node {
    struct s_sis_share_node *prev;
    struct s_sis_share_node *next;
    uint16 cites;  // 引用数，避免反复拷贝
    uint64 serial;   // 精确到秒 删除同一秒所有数据
    uint32 size;   //  key + value 的总长度
    void *key;   // 头描述
    void *value; // 数据区
    // void (*kfree)(void *ptr);  // key释放
    void (*vfree)(void *ptr);  // value释放
} s_sis_share_node;

typedef struct s_sis_share_list {
    s_sis_sds           key;     // 链表名字 一般通过该名字来隔离
    s_sis_mutex_rw      mutex;   // 多读一写
    size_t              limit;   // 最大长度
    size_t              size;    // 数据的长度
    size_t              count;   // node 的节点数
    uint64              serial;  // 每个消息一个唯一标记
    s_sis_share_node   *head;
    s_sis_share_node   *tail;
} s_sis_share_list;

#ifdef __cplusplus
extern "C" {
#endif
// s_sis_share_node *sis_share_node_create();
void sis_share_node_destroy(s_sis_share_node *); 


s_sis_share_list *sis_share_list_create(const char *, int64 limit_);
void sis_share_list_destroy(s_sis_share_list *); 
s_sis_share_node *sis_share_node_clone(s_sis_share_node *src_);

// 会发生一次拷贝，push后用户可以释放内存 写权限
int sis_share_list_push(s_sis_share_list *obj_, 
    void *key_, size_t klen_,                       // vfree_ == NULL 表示要申请内存
    void *val_, size_t vlen_, _share_free *vfree_); // vfree_ 不发生拷贝，但需要指定传入的数据的释放函数

// 以下三个函数如果返回不为空 不用的时候需要执行 sis_share_node_destroy
s_sis_share_node *sis_share_list_first(s_sis_share_list *obj_);
s_sis_share_node *sis_share_list_last(s_sis_share_list *obj_);
s_sis_share_node *sis_share_list_next(s_sis_share_list *obj_, s_sis_share_node *node_);

// s_sis_share_node *node = sis_share_list_first(...)
// while(node)
// {
//     node = sis_share_list_next(..., node);
// }
// sis_share_node_destroy(node);

// s_sis_share_node *sis_share_list_cut(s_sis_share_list *obj_, int *count_);  
// 0 为全部取出 1 取一个  返回实际的数量 写权限
void sis_share_list_clear(s_sis_share_list *obj_);

#ifdef __cplusplus
}
#endif

#endif
// _SIS_NET_OBJ_H
