
#ifdef MAKE_LIST_FAST

#ifndef _SIS_LIST_FAST_H
#define _SIS_LIST_FAST_H

#include "sis_core.h"
#include "sis_malloc.h"
#include "sis_thread.h"
#include "sis_map.h"
#include "sis_obj.h"

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
// *** 注意 这是一个单写多读的队列 多写时不能保证顺序  //
/////////////////////////////////////////////////
// #define UNLOCK_QUEUE
#ifdef UNLOCK_QUEUE
typedef struct s_sis_unlock_queue {
    int                rnums; // 可读元素个数
    s_sis_unlock_node *rhead;
    s_sis_unlock_node *rtail;

    int                wnums; // 可写元素个数
    s_sis_unlock_node *whead;
    s_sis_unlock_node *wtail;

    int                rlock;  // 读锁 锁定后可在 rnodes 增加元素
    int                wlock;  // 读锁 锁定后可在 wnodes 增加元素
} s_sis_unlock_queue;
#else
typedef struct s_sis_unlock_queue {
    int                rnums;  // 可读元素个数
    s_sis_unlock_node *rhead;
    s_sis_unlock_node *rtail;

    s_sis_rwlock_t     rlock;  
} s_sis_unlock_queue;
#endif
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
//  s_sis_unlock_queue
/////////////////////////////////////////////////
// *** 注意 这是一个单写多读的队列 多写时不能保证顺序  //
s_sis_unlock_queue *sis_unlock_queue_create();
void sis_unlock_queue_destroy(s_sis_unlock_queue *queue_);

void sis_unlock_queue_push(s_sis_unlock_queue *queue_, s_sis_object *obj_);

// 提取的数据 需要 sis_object_decr 释放 
s_sis_object *sis_unlock_queue_pop(s_sis_unlock_queue *queue_);


#ifdef __cplusplus
}
#endif

#define SIS_UNLOCK_STATUS_NONE 0
#define SIS_UNLOCK_STATUS_WORK 1
#define SIS_UNLOCK_STATUS_WAIT 2
#define SIS_UNLOCK_STATUS_EXIT 3

#define SIS_UNLOCK_READER_HEAD   1  // 从头返回数据
#define SIS_UNLOCK_READER_TAIL   2  // 从尾返回数据 SIS_UNLOCK_MODE_HEAD 互斥
#define SIS_UNLOCK_READER_WAIT   4  // 等待用户执行next才返回下一条
#define SIS_UNLOCK_READER_ZERO   8  // 没有数据但是时间超过也回调

#define SIS_UNLOCK_SAVE_ALL      1  // 保存全部数据
#define SIS_UNLOCK_SAVE_NONE     2  // 不保存数据
#define SIS_UNLOCK_SAVE_SIZE     3  // 保存一定量数据

/////////////////////////////////////////////////
//  s_sis_unlock_list
// 一个无堵塞的共享数据列表 多路同时写不堵塞
// 支持多个订阅者获取数据,每个订阅者都有自己的读取位置
// 数据提取出来处理完毕就删除
/////////////////////////////////////////////////

// 返回数据的回调函数类型定义 obj = NULL 表示超时无数据回调
typedef int (cb_unlock_reader)(void *, s_sis_object *);
// 针对从头开始的读者 表示累计的数据已经发送完毕 仅仅发送一次
typedef int (cb_unlock_reader_realtime)(void *);

// 读者不需要释放 只需要注册和启动
typedef struct s_sis_unlock_reader
{
    char                       sign[20];     // 索引编号
    void                      *father;      // s_sis_unlock_list *
    void                      *cb_source;   // 回调的参数
    cb_unlock_reader          *cb_recv;     // 正常数据
    cb_unlock_reader_realtime *cb_realtime; // 历史数据读取完毕

    int                        work_mode;    // 工作模式
    int                        work_status;  // 工作状态
    
    int                        zeromsec;     // 返回NULL数据

    bool                       isrealtime;   // 如果是从头读 是否已经送出了该信号

    s_sis_unlock_node         *cursor;       // 当前读者的结点指针 读者只能读 不能删除
    s_sis_wait_handle          notice_wait;  // 信号量
    s_sis_thread               work_thread;  // 工作线程
} s_sis_unlock_reader;

// *** 注意 这是一个单写多读的服务队列 多写时不能保证写入和出栈顺序  //
// 每个读者自己启动一个线程 当收到数据后 复制数据到每个读者
// 主动开启一个守护线程 相当于一个读者 用来保存历史数据 如果有客户请求从头开始就可以从存储中获取数据 ***加锁***
typedef struct s_sis_unlock_list {

    s_sis_unlock_queue  *work_queue; // 工作队列

    s_sis_mutex_t        userslock;  // 读者列表管理的锁  
    s_sis_pointer_list  *users;      // 读者列表 s_sis_unlock_reader 操作时 用 userslock 这个锁来处理

    s_sis_unlock_reader *watcher;    // 守护线程 处理数据发布 如果不需要保留所有数据 就定时清理不用的数据
    int                  save_mode;  // 历史数据处理模式 
 
    size_t               cursize;    // 数据总的长度 由 watcher 统计
    size_t               maxsize;    // 最大数据长度 由 watcher 控制 
    // *** maxsize 为 0 表示全部保存 | < sizeof(s_sis_object) 表示不需要处理历史数据 ***
} s_sis_unlock_list;

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////
//  s_sis_unlock_reader
/////////////////////////////////////////////////
// mode_  从什么位置开始读取
s_sis_unlock_reader *sis_unlock_reader_create(s_sis_unlock_list *ullist_, 
    int mode_, void *cb_source_, 
    cb_unlock_reader *cb_recv_,
    cb_unlock_reader_realtime cb_realtime_);

// 设置该值后 即使没有数据 到时间也会返回一个NULL数据
void sis_unlock_reader_zero(s_sis_unlock_reader *reader_, int zeromsec_);

// 注册一个自由的读者 并开始读取数据 
bool sis_unlock_reader_open(s_sis_unlock_reader *);
// 有新消息就先回调 然后由用户控制读下一条信息
void sis_unlock_reader_next(s_sis_unlock_reader *reader_);
// 注销一个读者
void sis_unlock_reader_close(s_sis_unlock_reader *reader_);

/////////////////////////////////////////////////
//  s_sis_unlock_list
/////////////////////////////////////////////////
// maxsize_ 为保存历史数据最大尺寸 需要统计数据大小 0 全部保存 1 不保存 2 不启动watcher n 保留的数据大小
// 实际使用时 如果有数据客户未处理数据 数据会一直保留 直到所有用户都不再使用才会删除数据
s_sis_unlock_list *sis_unlock_list_create(size_t maxsize_);
void sis_unlock_list_destroy(s_sis_unlock_list *); 
// 广播写入入口 读是依靠回调实现 
void sis_unlock_list_push(s_sis_unlock_list *, s_sis_object *);
// 只清理已经读过的数据 
void sis_unlock_list_clear(s_sis_unlock_list *);

#ifdef __cplusplus
}
#endif

#endif

#endif
