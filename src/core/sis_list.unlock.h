
#ifdef MAKE_UNLOCK_LIST

#ifndef _SIS_LIST_UNLOCK_H
#define _SIS_LIST_UNLOCK_H

#include "sis_core.h"
#include "sis_malloc.h"
#include "sis_thread.h"
#include "sis_map.h"
#include "sis_obj.h"

/////////////////////////////////////////////////
// 定义一个快速队列 环形数组无锁队列
// 不能在队列中释放 obj,申请和释放都必须是外部控制
/////////////////////////////////////////////////
// 本来1M 5个读者 达到了本机1秒 远机 6秒
// 莫名其妙就升到了 本机6秒 远机19秒
// 明天再查那里问题 先用lock
#pragma pack(push,1)

// 队列结点
typedef struct s_sis_unlock_node {
    s_sis_object             *obj;    // 数据区
    struct s_sis_unlock_node *next;
} s_sis_unlock_node;

// 无锁队列
typedef struct s_sis_unlock_queue {
    s_sis_unlock_node *array;
    char              *flags; // 标记位，标记某个位置的元素是否被占用
    // 0：空节点；1：已被申请，正在写入 2：已经写入，可以弹出;3,正在弹出操作;
    int                count; // 实际元素个数
    int                size;  // 环形数组大小
    int                head_idx; 
    int                tail_idx; 
    s_sis_unlock_mutex growlock; // 需要增长数组时使用
} s_sis_unlock_queue;

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
// **** 注意 这个队列只能单进单出 **** //
s_sis_unlock_queue *sis_unlock_queue_create();
void sis_unlock_queue_destroy(s_sis_unlock_queue *queue_);

void sis_unlock_queue_push(s_sis_unlock_queue *queue_, s_sis_object *obj_);

// 返回的数据 需要 sis_object_decr 释放 
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
    int                        sendsave;     // 0 未发送历史数据 n 已经发送多少历史数据 从尾读无效
                            // open时就需要发送 避免没有新数据进入不发送历史数据问题
    int                        isrealtime;   // 如果有realtime是否送出了该信号

    s_sis_object              *cursor;       // 最近读取的数据指针 便于列表对没用的数据进行清理
    s_sis_unlock_mutex         notice;       // 信号量
    s_sis_thread               work_thread;  // 工作线程

    s_sis_unlock_queue        *work_queue;   // 工作队列
} s_sis_unlock_reader;

// 多写多读的共享数据链表类
// 每个读者自己启动一个线程 当收到数据后 复制数据到每个读者
// 主动开启一个守护线程 相当于一个读者 用来保存历史数据 如果有客户请求从头开始就可以从存储中获取数据 ***加锁***
typedef struct s_sis_unlock_list {

    s_sis_unlock_queue  *work_queue; // 工作队列

    s_sis_unlock_mutex   userslock;  // 读者列表管理的锁  
    s_sis_pointer_list  *users;      // 读者列表 s_sis_unlock_reader 操作时 用 userslock 这个锁来处理

    s_sis_unlock_reader *watcher;    // 守护线程 处理数据发布和保存历史数据
 
    int                  save_mode;  // 保存历史数据
    s_sis_unlock_mutex   objslock;   // 历史数据列表管理的锁  
    s_sis_pointer_list  *objs;       // 历史数据列表 s_sis_object 操作时 用 objslock 这个锁来处理
    // 为避免历史数据积累 用户读过的数据定期删除 除非是约定保留全部数据的列表
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

s_sis_unlock_reader *sis_unlock_reader_create(s_sis_unlock_list *ullist_, 
    int mode_, void *cb_source_, 
    cb_unlock_reader *cb_recv_,
    cb_unlock_reader_realtime cb_realtime_);

void sis_unlock_reader_destroy(void *reader_);

// 设置该值后 即使没有数据 到时间也会返回一个NULL数据
void sis_unlock_reader_zero(s_sis_unlock_reader *reader_, int);

// 注册一个自由的读者 从头开始
bool sis_unlock_reader_open(s_sis_unlock_reader *);
// 有新消息就先回调 然后由用户控制读下一条信息
void sis_unlock_reader_next(s_sis_unlock_reader *reader_);
// 注销一个读者
void sis_unlock_reader_close(s_sis_unlock_reader *reader_);
// 设置该值后 即使没有数据 到时间也会返回一个NULL数据
void sis_unlock_reader_zero(s_sis_unlock_reader *reader_, int);

/////////////////////////////////////////////////
//  s_sis_unlock_list
/////////////////////////////////////////////////
// maxsize_ 为保存历史数据最大尺寸 需要统计数据大小 0 全部保存 1 不保存 n 数据大小
// 实际使用时 如果有数据客户未处理数据 数据会一直保留 直到所有用户都不再使用才会删除数据
s_sis_unlock_list *sis_unlock_list_create(size_t maxsize_);
void sis_unlock_list_destroy(s_sis_unlock_list *); 
// 广播写入入口 读是依靠回调实现 
void sis_unlock_list_push(s_sis_unlock_list *, s_sis_object *);
// 只清理历史数据
void sis_unlock_list_clear(s_sis_unlock_list *);

#ifdef __cplusplus
}
#endif

#endif

#endif
