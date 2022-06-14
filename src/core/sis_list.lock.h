
#ifndef _SIS_LIST_LOCK_H
#define _SIS_LIST_LOCK_H

#include "sis_core.h"
#include "sis_malloc.h"
#include "sis_thread.h"
#include "sis_map.h"
#include "sis_obj.h"

// 返回数据的回调函数类型定义 obj = NULL 表示超时无数据回调
typedef int (cb_lock_reader)(void *, s_sis_object *);
// 针对从头开始的读者 表示累计的数据已经发送完毕 仅仅发送一次
typedef int (cb_lock_reader_realtime)(void *);


/////////////////////////////////////////////////
// 不能在队列中释放 obj,申请和释放都必须是外部控制
/////////////////////////////////////////////////
#pragma pack(push,1)

// 队列结点
typedef struct s_sis_unlock_node {
    s_sis_object             *obj;    // 数据区
    struct s_sis_unlock_node *next;
} s_sis_unlock_node;

/////////////////////////////////////////////////
// s_sis_lock_queue  
// 队列采用了读写锁 CPU占用高 慎用
/////////////////////////////////////////////////
typedef struct s_sis_lock_queue {
    int                rnums;  // 可读元素个数
    s_sis_unlock_node *rhead;  // 队列头部节点
    s_sis_unlock_node *rtail;   //队列尾部节点

    s_sis_rwlock_t     rlock;   // cpu占用相对较高 不用理会
} s_sis_lock_queue;

/////////////////////////////////////////////////
//  s_sis_wait_queue
//  这是一个等待式队列 除非busy = 0 才能提取数据
/////////////////////////////////////////////////
// 这个队列会造成写入和读取不均匀 但是CPU占用少
typedef struct s_sis_wait_queue {
    volatile int       busy;  // 返回的数据是否已经处理
    s_sis_mutex_t      lock;  
    int                count;
    s_sis_unlock_node *head;
    s_sis_unlock_node *tail;
    void              *cb_source;
    cb_lock_reader    *cb_reader;
    s_sis_wait_thread *work_thread;  // 工作线程
} s_sis_wait_queue;

/////////////////////////////////////////////////
//  s_sis_fast_queue
//  设置需求每秒处理1M的插入和读取
//  这是一个一写一读的快速队列 优先保证写入速度 适用于需要快速插入 提取数据处理耗时且缓慢的场景
/////////////////////////////////////////////////
typedef struct s_sis_fast_queue {
    s_sis_mutex_t      rlock;  // 在清理时使用
    int                rnums;  
    s_sis_unlock_node *rhead;  // 读时try锁wlock 锁住就 直接截断whead -> rtail 
    s_sis_unlock_node *rtail;

    s_sis_mutex_t      wlock;  // 只要加了锁就直接写
    int                wnums;  
    s_sis_unlock_node *whead;  
    s_sis_unlock_node *wtail;

    void              *cb_source;
    cb_lock_reader    *cb_reader;
    int                work_status;
    int                zero_msec;    // 0 正常 1 没有数据时间到也回调

    s_sis_wait_thread *work_thread;  // 工作线程
} s_sis_fast_queue;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////
//  s_sis_unlock_node
/////////////////////////////////////////////////

s_sis_unlock_node *sis_unlock_node_create(s_sis_object *value_);
void sis_unlock_node_destroy(s_sis_unlock_node *); 

/////////////////////////////////////////////////
//  s_sis_lock_queue
/////////////////////////////////////////////////
s_sis_lock_queue *sis_lock_queue_create();
void sis_lock_queue_destroy(s_sis_lock_queue *queue_);

void sis_lock_queue_push(s_sis_lock_queue *queue_, s_sis_object *obj_);

// 提取的数据 需要 sis_object_decr 释放 
s_sis_object *sis_lock_queue_pop(s_sis_lock_queue *queue_);

/////////////////////////////////////////////////
//  s_sis_wait_queue
/////////////////////////////////////////////////
// **** 注意 这个队列可以多进多出 **** //
s_sis_wait_queue *sis_wait_queue_create(void *, cb_lock_reader *);
void sis_wait_queue_destroy(s_sis_wait_queue *queue_);
// busy 为 1 只能push 并返回 NULL, 否则直接 设置 busy = 1 并返回原 obj 
s_sis_object *sis_wait_queue_push(s_sis_wait_queue *queue_, s_sis_object *obj_);
// 如果队列为空 设置 busy = 0 返回空 | 如果有数据就弹出最早的消息 busy = 1 返回值需要 sis_object_decr 释放 
// pop只在内部使用 
// s_sis_object *sis_wait_queue_pop(s_sis_wait_queue *queue_);
// 前次数据处理完后 设置 busy 队列会从线程获取下一个数据通过回调返回
// 返回上次弹出的obj
void sis_wait_queue_set_busy(s_sis_wait_queue *queue_, int );

/////////////////////////////////////////////////
//  s_sis_fast_queue
/////////////////////////////////////////////////

s_sis_fast_queue *sis_fast_queue_create(
    void *cb_source_, cb_lock_reader *cb_reader_,
    int wait_msec_, int zero_msec_);
void sis_fast_queue_destroy(s_sis_fast_queue *queue_);
void sis_fast_queue_clear(s_sis_fast_queue *queue_);
int sis_fast_queue_push(s_sis_fast_queue *queue_, s_sis_object *obj_);

#ifdef __cplusplus
}
#endif

#define SIS_UNLOCK_READER_HEAD   1  // 从头返回数据
#define SIS_UNLOCK_READER_TAIL   2  // 从尾返回数据 SIS_UNLOCK_MODE_HEAD 互斥
#define SIS_UNLOCK_READER_ZERO   4  // 没有数据但是时间超过也回调

#define SIS_UNLOCK_SAVE_ALL      1  // 保存全部数据
#define SIS_UNLOCK_SAVE_NONE     2  // 不保存数据
#define SIS_UNLOCK_SAVE_SIZE     3  // 保存一定量数据

/////////////////////////////////////////////////
//  s_sis_lock_list
// 一个无堵塞的共享数据列表 多路同时写
// 支持多个订阅者获取数据,每个订阅者都有自己的读取位置
// 数据提取出来处理完毕就删除
/////////////////////////////////////////////////

// 读者不需要释放 只需要注册和启动
typedef struct s_sis_lock_reader
{
    char                       sign[20];    // 索引编号
    void                      *father;      // s_sis_lock_list *
    void                      *cb_source;   // 回调的参数
    cb_lock_reader            *cb_recv;     // 正常数据
    cb_lock_reader_realtime   *cb_realtime; // 历史数据读取完毕

    int                        work_mode;    // 工作模式
    
    int                        zero_msec;     // 返回NULL数据

    bool                       isrealtime;   // 如果是从头读 是否已经送出了该信号

    s_sis_unlock_node         *cursor;       // 当前读者的结点指针 读者只能读 不能删除
    s_sis_wait_thread         *work_thread;  // 工作线程
} s_sis_lock_reader;


// 每个读者自己启动一个线程 当收到数据后 各自处理 适用于各个用户处理时间严重不平衡的情况 
// 因为每个读者读取数据会有锁动作 因此速度比fast稍慢
// 优点是可以设置数据不销毁 对有历史数据需求的特别适用
// 主动开启一个守护线程 相当于一个读者 用来及时销毁过期的数据
typedef struct s_sis_lock_list {

    s_sis_lock_queue    *work_queue; // 工作队列

    s_sis_rwlock_t       userslock;  // 读者列表管理的锁  
    s_sis_pointer_list  *users;      // 读者列表 s_sis_lock_reader 操作时 用 userslock 这个锁来处理

    s_sis_mutex_t        watchlock;  // 对数据进行操作时必须加锁
    s_sis_lock_reader   *watcher;    // 守护线程 处理数据发布 如果不需要保留所有数据 就定时清理不用的数据
    //QQQ 历史数据处理模式，如何实现以及如何使用？
    int                  save_mode;  // 历史数据处理模式 
 
    size_t               cursize;    // 数据总的长度 由 watcher 统计
    size_t               maxsize;    // 最大数据长度 由 watcher 控制 
    // *** maxsize 为 0 表示全部保存 | 1 表示数据全部保留 ***
} s_sis_lock_list;

#ifdef __cplusplus
extern "C" {
#endif

// /////////////////////////////////////////////////
// //  s_sis_fast_reader
// /////////////////////////////////////////////////
// // mode_  从什么位置开始读取
// s_sis_fast_reader *sis_fast_reader_create(
//     s_sis_fast_list *ullist_, 
//     int level_,
//     void *cb_source_, 
//     cb_lock_reader *cb_recv_,
//     cb_lock_reader_realtime cb_realtime_);

// // 设置该值后 即使没有数据 到时间也会返回一个NULL数据
// void sis_fast_reader_zero(s_sis_fast_reader *reader_, int zero_msec_);

// // 注册一个自由的读者 并开始读取数据 
// bool sis_lock_reader_open(s_sis_fast_reader *);
// // 注销一个读者
// void sis_lock_reader_close(s_sis_fast_reader *reader_);

// /////////////////////////////////////////////////
// //  s_sis_fast_list
// /////////////////////////////////////////////////
// // maxsize_ 为保存历史数据最大尺寸 需要统计数据大小 0 全部保存 1 不保存 n 保留的数据大小
// // 实际使用时 n 表示如果有数据客户未处理数据 数据会一直保留 直到所有用户都不再使用才会删除数据
// s_sis_fast_list *sis_fast_list_create(int wait_msec_);
// void sis_fast_list_destroy(s_sis_fast_list *); 
// // 广播写入入口 读是依靠回调实现 
// void sis_fast_list_push(s_sis_fast_list *, s_sis_object *);
// // 只清理已经读过的数据 
// void sis_fast_list_clear(s_sis_fast_list *);


/////////////////////////////////////////////////
//  s_sis_lock_reader
/////////////////////////////////////////////////
// mode_  从什么位置开始读取

s_sis_lock_reader *sis_lock_reader_create(s_sis_lock_list *ullist_, 
    int mode_, void *cb_source_, 
    cb_lock_reader *cb_recv_,
    cb_lock_reader_realtime cb_realtime_);

// 设置该值后 即使没有数据 到时间也会返回一个NULL数据
void sis_lock_reader_zero(s_sis_lock_reader *reader_, int zero_msec_);

// 注册一个自由的读者 并开始读取数据 
bool sis_lock_reader_open(s_sis_lock_reader *);
// 注销一个读者
void sis_lock_reader_close(s_sis_lock_reader *reader_);

/////////////////////////////////////////////////
//  s_sis_lock_list
/////////////////////////////////////////////////
// maxsize_ 为保存历史数据最大尺寸 需要统计数据大小 0 全部保存 1 不保存 n 保留的数据大小
// 实际使用时 n 表示如果有数据客户未处理数据 数据会一直保留 直到所有用户都不再使用才会删除数据
s_sis_lock_list *sis_lock_list_create(size_t maxsize_);
void sis_lock_list_destroy(s_sis_lock_list *); 
// 广播写入入口 读是依靠回调实现 
void sis_lock_list_push(s_sis_lock_list *, s_sis_object *);
// 只清理已经读过的数据 
void sis_lock_list_clear(s_sis_lock_list *);

#ifdef __cplusplus
}
#endif

#endif

