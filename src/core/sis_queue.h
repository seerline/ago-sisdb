#ifndef _SIS_QUEUE_H
#define _SIS_QUEUE_H

#include "sis_core.h"
#include "sis_malloc.h"
#include "sis_thread.h"
#include "sis_map.h"
#include "sis_obj.h"

#define CAS __sync_bool_compare_and_swap
#define FAA __sync_fetch_and_add
#define FAS __sync_fetch_and_sub


/////////////////////////////////////////////////
// s_sis_queue
// 定义一个快速队列
// 不能在队列中释放 value,申请和释放都必须是外部控制
/////////////////////////////////////////////////

#pragma pack(push,1)

typedef struct s_sis_queue_node{
    void   *value;
    struct s_sis_queue_node *next;
} s_sis_queue_node;

typedef struct s_sis_queue{
    volatile int      count;
    s_sis_queue_node *head;
    s_sis_queue_node *tail;
    s_sis_mutex_t     headlock;
    s_sis_mutex_t     taillock;
} s_sis_queue;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif

s_sis_queue *sis_queue_create();
void sis_queue_destroy(s_sis_queue *);

int   sis_queue_push(s_sis_queue *, void *);
void *sis_queue_pop(s_sis_queue *); // 返回 value


#ifdef __cplusplus
}
#endif

#define SIS_SHARE_STATUS_NONE 0
#define SIS_SHARE_STATUS_WORK 1
#define SIS_SHARE_STATUS_WAIT 2
#define SIS_SHARE_STATUS_EXIT 3

typedef enum {
  SIS_SHARE_FROM_HEAD =  0,
  SIS_SHARE_FROM_TAIL = -1,
} sis_share_from;

/////////////////////////////////////////////////
//  s_sis_share_list
// 一个无堵塞的共享数据列表 多路同时写不堵塞
// 支持多个订阅者获取数据,每个订阅者都有自己的读取位置
// 超出大小的数据按先后直接截掉,
/////////////////////////////////////////////////

// 注册一个阅读者，阅读者只有读权利，且不能读最后一条记录，除非最后没有写，
// 对最后一条记录的读取可以利用 try_lock来判断，如果被锁住就下次来取，
// 返回一个指针

// 0xFFFFFFFF00000000 高位表示key 低位表示 sdb 的方式来处理
typedef unsigned long long sdbkey_t;
typedef int (cb_sis_share_reader)(void *, s_sis_object *);
typedef int (cb_sis_share_reader_eof)(void *);
typedef void *(sis_share_reader_working)(void *);

// 最大支持 60k 个订阅者
// 为了快速定位 数据 采用 
typedef struct s_sis_share_node {
    s_sis_object   *value;    // 数据区
    struct s_sis_share_node *next;
} s_sis_share_node;

struct s_sis_share_list;

typedef struct s_sis_share_reader
{
    char                      sign[20];     // 索引编号
    struct s_sis_share_list  *slist;        // 共享列表指针
    void*                     cbobj;
    cb_sis_share_reader      *cb;           // 回调后立即增加引用 
    cb_sis_share_reader_eof  *cb_eof;       // 
    bool                      istail;        // 如果是从头开始读 第一次读到队尾需要发送一条空表示没有数据了, 
    s_sis_share_node *        cursor;       // 当前指针地址
    sis_share_from            from;         // 从什么位置开始取数据 
    
    s_sis_wait_handle         notice_wait;  // 信号量
    s_sis_thread              work_thread;  // 线程
    int                       work_status;
} s_sis_share_reader;

// 一写多读的共享数据链表类
// 每个读者自己启动一个线程 当写入数据成功后 通知每个线程，
// sis_thread_wait_notice(&reader->notice_wait);
// 线程根据自己上次读取的位置继续读 并回调，直到数据读完
// class主动开启一个守护线程 相当于一个读者 统计数据大小 如果数据超出界限就清理数据 
typedef struct s_sis_share_list {
    s_sis_sds            name;     // 链表名字 一般通过该名字来隔离

    int                  zeromsec; // 0 表示不返回NULL >0 表示返回NULL值
    s_sis_share_node    *head;
    s_sis_share_node    *tail;
    s_sis_mutex_t        headlock;
    s_sis_mutex_t        taillock;
    volatile size_t      count;   // node 的节点数
    // 不用消费者模式 是因为可能的内存释放问题 
    // bool                 consume;  // 是否生产消费模式 consume = 1 且 maxsize = 0
    s_sis_map_list      *catch_infos; // 保存需要缓存的信息 不会删除 按名字检索

    s_sis_share_reader  *watcher;    // 守护线程  
    size_t               cursize;    // 数据总的长度 由 watcher 统计
    size_t               maxsize;    // 最大数据长度 由 watcher 控制 *** maxsize 为 0 表示单进单出 ***

    s_sis_pointer_list  *reader;     // 订阅者列表 s_sis_share_reader

} s_sis_share_list;

#ifdef __cplusplus
extern "C" {
#endif
/////////////////////////////////////////////////
//  s_sis_share_node
/////////////////////////////////////////////////

s_sis_share_node *sis_share_node_create(s_sis_object *value_);
void sis_share_node_destroy(s_sis_share_node *); 

/////////////////////////////////////////////////
//  s_sis_share_reader
/////////////////////////////////////////////////

// s_sis_share_reader *sis_share_reader_create(s_sis_share_list *slist_,
//     sis_share_from from_, void *cbobj_, cb_sis_share_reader *cb_);

// void sis_share_reader_destroy(void *);

// bool sis_share_reader_work(s_sis_share_reader *reader_,
//     sis_share_reader_working *work_thread_);

/////////////////////////////////////////////////
//  s_sis_share_list
/////////////////////////////////////////////////

s_sis_share_list *sis_share_list_create(const char *, size_t maxsize_);
void sis_share_list_destroy(s_sis_share_list *); 

// 广播写入入口 读是依靠回调实现 
int  sis_share_list_push(s_sis_share_list *, s_sis_object *value_);

void sis_share_list_zero(s_sis_share_list *, int);

// 写入缓存数据 读取是依靠get按索引获取
int  sis_share_list_catch_set(s_sis_share_list *, char *key_, s_sis_object *value_);

s_sis_object *sis_share_list_catch_get(s_sis_share_list *, char *key_); 

s_sis_object *sis_share_list_catch_geti(s_sis_share_list *, int index_); 

int sis_share_list_catch_size(s_sis_share_list *); 

// 注册一个自由的读者 从头开始
s_sis_share_reader *sis_share_reader_open(s_sis_share_list *, void *cbobj_, cb_sis_share_reader *cb_);
// 有新消息就先回调 然后由用户控制读下一条信息 next返回为空时重新激活线程的执行 
s_sis_object *sis_share_reader_next(s_sis_share_reader *reader_);
void sis_share_reader_close(s_sis_share_list *cls_, s_sis_share_reader *reader_);

// 注册一个读者
s_sis_share_reader *sis_share_reader_login(s_sis_share_list *, 
    sis_share_from from_, void *cbobj_, cb_sis_share_reader *cb_);
//注销一个读者
void sis_share_reader_logout(s_sis_share_list *cls_, 
    s_sis_share_reader *reader_);


#ifdef __cplusplus
}
#endif

#endif
// _SIS_QUEUE_H
