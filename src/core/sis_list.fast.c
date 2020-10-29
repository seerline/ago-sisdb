
#include "sis_list.ctrl.h"
#include "sis_list.fast.h"

#ifdef MAKE_LIST_FAST

/////////////////////////////////////////////////
//  s_sis_unlock_node
/////////////////////////////////////////////////
s_sis_unlock_node *sis_unlock_node_create(s_sis_object *obj_)
{
    s_sis_unlock_node *node = SIS_MALLOC(s_sis_unlock_node, node);
    if (obj_)
    {
        sis_object_incr(obj_);
        node->obj = obj_;
    }
    node->next = NULL;
    return node;    
}
void sis_unlock_node_destroy(s_sis_unlock_node *node_)
{
    // if(node_->obj)
    // {
    //     sis_object_decr(node_->obj);
    // }
    sis_free(node_);
}

/////////////////////////////////////////////////
//  s_sis_unlock_queue
/////////////////////////////////////////////////

/////////////////////////////////////////////////
//  主备缓存 自增的无锁队列 以写入为优先 如果不能写主区就写到备区
//  读取时判断是否合并备区到主区 如果有人写 就直接去主区读数据 主区没有数据就返回NULL
//  由下次读取时再合并数据
//  单进单出 1M  261 ms
//  1进5出  1M  1673 -1687 ms
//  1进5类出  1M 1975 - 3800
/////////////////////////////////////////////////
s_sis_unlock_queue *sis_unlock_queue_create()
{
    s_sis_unlock_queue *o = SIS_MALLOC(s_sis_unlock_queue, o);
    o->rhead = sis_unlock_node_create(NULL);
    o->rtail = o->rhead;
    o->whead = sis_unlock_node_create(NULL);
    o->wtail = o->whead;
    return  o;
}
void sis_unlock_queue_destroy(s_sis_unlock_queue *queue_)
{
    // printf("exit 0 : %d %d\n", queue_->rnums, queue_->wnums);
    while (queue_->rnums > 0)
    {
        s_sis_object *obj = sis_unlock_queue_pop(queue_);
        sis_object_decr(obj);
    }
    // printf("exit 1 : %d %d\n", queue_->rnums, queue_->wnums);
    sis_unlock_node_destroy(queue_->rtail);
    sis_unlock_node_destroy(queue_->wtail);
    sis_free(queue_);
}
static inline s_sis_unlock_node *sis_unlock_queue_head(s_sis_unlock_queue *queue_)
{
    s_sis_unlock_node *node = NULL;
    while (!BCAS(&queue_->rlock, 0, 1))
    {
    }
    node = queue_->rhead;
    SUBF(&queue_->rlock, 1);
    return node;
}
static inline s_sis_unlock_node *sis_unlock_queue_head_next(s_sis_unlock_queue *queue_)
{
    s_sis_unlock_node *node = NULL;
    while (!BCAS(&queue_->rlock, 0, 1))
    {
    }
    node = queue_->rhead->next;
    SUBF(&queue_->rlock, 1);
    return node;
}
static inline s_sis_unlock_node *sis_unlock_queue_tail(s_sis_unlock_queue *queue_)
{
    s_sis_unlock_node *node = NULL;
    while (!BCAS(&queue_->rlock, 0, 1))
    {
    }
    node = queue_->rtail;
    SUBF(&queue_->rlock, 1);
    return node;    
}
static inline void sis_unlock_queue_merge(s_sis_unlock_queue *queue_)
{
    if (BCAS(&queue_->wlock, 0, 1)) 
    {
        // 锁写成功 不成功就不合并
        // printf("pop w ok. rnums = %d wnums = %d\n", queue_->rnums,queue_->wnums);
        if (queue_->wnums > 0) // 有数据
        {
            while (!BCAS(&queue_->rlock, 0, 1))
            {
                sis_sleep(1);
            }
            queue_->rtail->next = queue_->whead->next;
            queue_->rtail = queue_->wtail;
            queue_->rnums += queue_->wnums;
            queue_->whead->next = NULL;
            queue_->wtail = queue_->whead;
            queue_->wnums = 0;
            SUBF(&queue_->rlock, 1);
        }
        SUBF(&queue_->wlock, 1);
    }
}
// static inline void sis_unlock_queue_merge(s_sis_unlock_queue *queue_)
// {
//     if (queue_->wnums > 0) 
//     {
//         if (BCAS(&queue_->wlock, 0, 1, &queue_->rlock, 0, 1)) 
//         {
//             // 锁写成功 不成功就不合并
//             queue_->rtail->next = queue_->whead->next;
//             queue_->rtail = queue_->wtail;
//             queue_->rnums += queue_->wnums;
//             SUBF(&queue_->rlock, 1);
//             queue_->whead->next = NULL;
//             queue_->wtail = queue_->whead;
//             queue_->wnums = 0;
//             SUBF(&queue_->wlock, 1);
//             // SUBF(&queue_->rlock, 1);
//         }
//     }
// }
// static inline s_sis_unlock_node *sis_unlock_queue_next(s_sis_unlock_queue *queue_, s_sis_unlock_node *node_)
// {
//     // 这里如果不尝试合并 就会当没有新数据时永远无法合并
//     sis_unlock_queue_merge(queue_);
//     s_sis_unlock_node *node = NULL;
//     if (BCAS(&queue_->rlock, 0, 1))
//     {
//         node = node_->next;
//         SUBF(&queue_->rlock, 1);
//     }
//     return node;      
// }
static inline s_sis_unlock_node *sis_unlock_queue_next(s_sis_unlock_queue *queue_, s_sis_unlock_node *node_)
{
    // 这里如果不尝试合并 就会当没有新数据时永远无法合并
    sis_unlock_queue_merge(queue_);
    s_sis_unlock_node *node = NULL;
    while (!BCAS(&queue_->rlock, 0, 1))
    {
    }
    node = node_->next;
    SUBF(&queue_->rlock, 1);
    return node;      
}
void sis_unlock_queue_push(s_sis_unlock_queue *queue_, s_sis_object *obj_)
{  
    s_sis_unlock_node *new_node = sis_unlock_node_create(obj_);
    if (BCAS(&queue_->rlock, 0, 1))  // 如果可用 设置为正在写
    {
        queue_->rtail->next = new_node;
        queue_->rtail = new_node;
        queue_->rnums++;
        SUBF(&queue_->rlock, 1);       
        // ADDF(&queue_->count, 1);
    }
    else
    {
        // 优先保证写入队列忙式等待
        while (!BCAS(&queue_->wlock, 0, 1))
        {
            // printf("wait wlock %d\n", queue_->wlock);
            sis_sleep(1);
        }
        queue_->wtail->next = new_node;
        queue_->wtail = new_node;
        queue_->wnums++;
        SUBF(&queue_->wlock, 1);
        // 这里count必须锁住rlock
        // ADDF(&queue_->count, 1);
    }
}
// 返回的数据 需要 sis_object_decr 释放 
s_sis_object *sis_unlock_queue_pop(s_sis_unlock_queue *queue_)
{  
    s_sis_object *obj = NULL;
    sis_unlock_queue_merge(queue_);
    {
        // 锁备区不成功 就不合并 忙式等读权利
        while (!BCAS(&queue_->rlock, 0, 1))
        {
            sis_sleep(1);
        }
        s_sis_unlock_node *ago_head = queue_->rhead;
        // printf("pop r ok %p %p\n", ago_head, ago_head->next);
        if (ago_head->next)
        {
            obj = ago_head->next->obj;
            ago_head->next->obj = NULL;
            queue_->rhead = ago_head->next;
            queue_->rnums--;
            sis_unlock_node_destroy(ago_head);
        }
        SUBF(&queue_->rlock, 1);
    }
    return obj;
}

/////////////////////////////////////////////////
//  s_sis_unlock_reader
/////////////////////////////////////////////////
void *_thread_reader(void *argv_)
{
    s_sis_unlock_reader *reader = (s_sis_unlock_reader *)argv_;
    s_sis_unlock_list *ullist = (s_sis_unlock_list *)reader->father;
    s_sis_wait *wait = sis_wait_get(reader->notice_wait);
    sis_thread_wait_start(wait);

    int waitmsec = 3000;
    if (reader->work_mode & SIS_UNLOCK_READER_ZERO)
    {
        waitmsec = reader->zeromsec;
    }
    bool surpass_waittime = false;
    while (reader->work_status != SIS_UNLOCK_STATUS_EXIT)
    {
        if (reader->work_status == SIS_UNLOCK_STATUS_WORK)
        {
            if (reader->cursor == NULL)
            {
                // 第一次 得到开始的位置
                if (reader->work_mode & SIS_UNLOCK_READER_HEAD)
                {
                    reader->cursor = sis_unlock_queue_head(ullist->work_queue);
                }
                else
                {
                    reader->cursor = sis_unlock_queue_tail(ullist->work_queue);
                }
            }
            if (reader->cursor)
            {
                s_sis_unlock_node *next = sis_unlock_queue_next(ullist->work_queue, reader->cursor);
                if (next)
                {
                    while(next)   
                    {
                        if (reader->work_status == SIS_UNLOCK_STATUS_EXIT) // 加速退出
                        {
                            break;
                        }
                        // printf("next = %p\n", next); 
                        if (next->obj)
                        {
                            reader->cb_recv(reader->cb_source, next->obj);
                        }
                        reader->cursor = next;
                        next = sis_unlock_queue_next(ullist->work_queue, reader->cursor);
                    } 
                }
                else
                {
                    if (surpass_waittime && (reader->work_mode & SIS_UNLOCK_READER_ZERO))
                    {
                        reader->cb_recv(reader->cb_source, NULL);
                    }
                }               
                // 如果读完发送标志
                if ((reader->work_mode & SIS_UNLOCK_READER_HEAD) && !reader->isrealtime)
                {
                    // 只发一次
                    if (reader->cb_realtime)
                    {
                        reader->cb_realtime(reader->cb_source);
                    }
                    reader->isrealtime = true;
                }
            }
        }
        if (sis_thread_wait_sleep_msec(wait, waitmsec) == SIS_ETIMEDOUT)
        {
            // printf("timeout exit. %d\n", waitmsec);
            surpass_waittime = reader->isrealtime ? true : false;
        }
    }
    sis_thread_wait_stop(wait);
    sis_thread_finish(&reader->work_thread);
    reader->work_status = SIS_UNLOCK_STATUS_NONE;
    return NULL;
}

static void *_thread_reader_wait(void *argv_)
{
    s_sis_unlock_reader *reader = (s_sis_unlock_reader *)argv_;
    s_sis_unlock_list *ullist = (s_sis_unlock_list *)reader->father;
    s_sis_wait *wait = sis_wait_get(reader->notice_wait);
    sis_thread_wait_start(wait);

    while (reader->work_status != SIS_UNLOCK_STATUS_EXIT)
    {
        if (reader->work_status == SIS_UNLOCK_STATUS_WORK)
        {
            // 只有工作状态才会读取数据
            if (reader->cursor == NULL)
            {
                // 第一次 得到开始的位置
                if (reader->work_mode & SIS_UNLOCK_READER_HEAD)
                {
                    s_sis_unlock_node *next = sis_unlock_queue_head_next(ullist->work_queue);
                    if (next)
                    {
                        reader->work_status = SIS_UNLOCK_STATUS_WAIT;
                        reader->cb_recv(reader->cb_source, next->obj);
                    }
                    reader->cursor = next;
                }
                else
                {
                    reader->cursor = sis_unlock_queue_tail(ullist->work_queue);
                }
            }
            else
            {
                s_sis_unlock_node *next = sis_unlock_queue_next(ullist->work_queue, reader->cursor);
                if (next)
                {
                    reader->work_status = SIS_UNLOCK_STATUS_WAIT;
                    reader->cb_recv(reader->cb_source, next->obj);
                    reader->cursor = next;
                }         
            }
        }
        if (sis_thread_wait_sleep_msec(wait, 300) != SIS_ETIMEDOUT)
        {
            // printf("reader notice.\n");
        }
    }
    sis_thread_wait_stop(wait);
    sis_thread_finish(&reader->work_thread);
    reader->work_status = SIS_UNLOCK_STATUS_NONE;
    return NULL;
}
s_sis_unlock_reader *sis_unlock_reader_create(s_sis_unlock_list *ullist_, 
    int mode_, void *cb_source_, 
    cb_unlock_reader *cb_recv_,
    cb_unlock_reader_realtime cb_realtime_)
{
    s_sis_unlock_reader *o = SIS_MALLOC(s_sis_unlock_reader, o);
    sis_str_get_time_id(o->sign, 16);
    // printf("sign=%s\n", o->sign);
    o->father = ullist_;
    o->cb_source = cb_source_ ? cb_source_ : o;
    o->cb_recv = cb_recv_;
    o->cb_realtime = cb_realtime_;
    
    o->work_mode = mode_ == 0 ? SIS_UNLOCK_READER_TAIL : mode_;
    if (o->work_mode & SIS_UNLOCK_READER_TAIL)
    {
        o->work_mode &= ~SIS_UNLOCK_READER_HEAD;
        o->isrealtime = 1;
    }

    o->notice_wait = sis_wait_malloc();
    // printf("new %p %d | %p\n", o, o->notice_wait, o->father);

    o->work_status = SIS_UNLOCK_STATUS_NONE;
    return o;
}
void sis_unlock_reader_destroy(void *reader_)
{
    s_sis_unlock_reader *reader = (s_sis_unlock_reader *)reader_;
    if (reader->work_status != SIS_UNLOCK_STATUS_NONE)
    {
        reader->work_status = SIS_UNLOCK_STATUS_EXIT;
        // 通知退出
        sis_thread_wait_notice(sis_wait_get(reader->notice_wait));
        while (reader->work_status != SIS_UNLOCK_STATUS_NONE)
        {
            sis_sleep(10);
        }
    }
    sis_wait_free(reader->notice_wait);
    sis_free(reader); 
}
void sis_unlock_reader_zero(s_sis_unlock_reader *reader_, int zeromsec_)
{
    if (zeromsec_ > 0)
    {
        reader_->zeromsec = zeromsec_ > 10 ? zeromsec_ : 10;
        reader_->work_mode |= SIS_UNLOCK_READER_ZERO;
    }
}
volatile int numslock = 0;
bool sis_unlock_reader_open(s_sis_unlock_reader *reader_)
{
    // 设置工作状态
    reader_->work_status = SIS_UNLOCK_STATUS_WORK;
    // 最后再启动线程 顺序不能变 不然数据会等待
    if (reader_->work_mode & SIS_UNLOCK_READER_WAIT)
    {
        // 用户通过next控制获取数据 不启动线程
        if (!sis_thread_create(_thread_reader_wait, reader_, &reader_->work_thread))
        {
            LOG(1)("can't start reader thread.\n");
            sis_unlock_reader_destroy(reader_);
            return false;
        }
    }
    else
    {        
        if (!sis_thread_create(_thread_reader, reader_, &reader_->work_thread))
        {
            LOG(1)("can't start reader thread.\n");
            sis_unlock_reader_destroy(reader_);
            return false;
        }
    }
    s_sis_unlock_list *ullist = (s_sis_unlock_list *)reader_->father;
    sis_mutex_lock(&ullist->userslock);
    // printf("userslock lock 1 %d ok = %p\n",numslock++, &ullist->userslock);
    sis_pointer_list_push(ullist->users, reader_);
    sis_mutex_unlock(&ullist->userslock);
    // printf("userslock unlock 1 %d ok = %p\n",numslock--, &ullist->userslock);

    return true;
}
// 有新消息就先回调 然后由用户控制读下一条信息 next返回为空时重新激活线程的执行 
void sis_unlock_reader_next(s_sis_unlock_reader *reader_)
{
    if (!(reader_->work_mode & SIS_UNLOCK_READER_WAIT))
    {
        return ;
    }
    if (reader_->work_status != SIS_UNLOCK_STATUS_WAIT)
    {
        return ;
    }
    s_sis_unlock_list *ullist = (s_sis_unlock_list *)reader_->father;
    s_sis_unlock_node *next = sis_unlock_queue_next(ullist->work_queue, reader_->cursor);
    if (next)
    {
        if (reader_->cb_recv)
        {
            reader_->cb_recv(reader_->cb_source, next->obj);
        }
        reader_->cursor = next;
    }
    else  // 读到队尾
    {  
        reader_->work_status = SIS_UNLOCK_STATUS_WORK;
    }
}
void sis_unlock_reader_close(s_sis_unlock_reader *reader_)
{
    s_sis_unlock_list *ullist = (s_sis_unlock_list *)reader_->father;
    sis_mutex_lock(&ullist->userslock);
    // printf("userslock lock 2 %d ok = %p\n",numslock++, &ullist->userslock);
    int index = sis_pointer_list_indexof(ullist->users, reader_);
    sis_pointer_list_delete(ullist->users, index , 1);
    sis_mutex_unlock(&ullist->userslock);
    // printf("userslock unlock 2 %d ok = %p\n",numslock--, &ullist->userslock);
}   

/////////////////////////////////////////////////
//  s_sis_unlock_list
/////////////////////////////////////////////////
bool _unlock_check_user_cursor(s_sis_unlock_list *ullist, s_sis_unlock_node *node)
{
    sis_mutex_lock(&ullist->userslock);
    bool o = false;
    // printf("userslock lock 3 %d ok = %p\n",numslock++, &ullist->userslock);
    for (int i = 0; i < ullist->users->count; i++)
    {
        s_sis_unlock_reader *reader = (s_sis_unlock_reader *)sis_pointer_list_get(ullist->users, i);
        if (reader->cursor == node)
        {
            o = true;
            break;
        }
    }
    sis_mutex_unlock(&ullist->userslock);  
    // printf("userslock unlock 3 %d ok = %p\n",numslock--, &ullist->userslock);
    return o;
}
static void *_thread_watcher(void *argv_)
{
    s_sis_unlock_reader *reader = (s_sis_unlock_reader *)argv_;
    s_sis_unlock_list *ullist = (s_sis_unlock_list *)reader->father;
    s_sis_wait *wait = sis_wait_get(reader->notice_wait);
    sis_thread_wait_start(wait);
    while (reader->work_status != SIS_UNLOCK_STATUS_EXIT)
    {
        if (reader->work_status == SIS_UNLOCK_STATUS_WORK 
            && ullist->save_mode != SIS_UNLOCK_SAVE_ALL) // 需要全部保留就什么也不干
        {
            if (reader->cursor == NULL)
            {
                s_sis_unlock_node *node = sis_unlock_queue_head_next(ullist->work_queue);
                if (node)
                {
                    reader->cursor = node;
                }   
            }
            if (reader->cursor)
            {
                s_sis_unlock_node *next = sis_unlock_queue_next(ullist->work_queue, reader->cursor);
                while(next)   
                {
                    if (_unlock_check_user_cursor(ullist, reader->cursor))
                    {
                        break;
                    }
                    if (ullist->save_mode == SIS_UNLOCK_SAVE_NONE)
                    {
                        // 销毁一个大家都不用的数据
                        s_sis_object *obj = sis_unlock_queue_pop(ullist->work_queue);
                        sis_object_decr(obj);   
                    }
                    else // SIS_UNLOCK_SAVE_SIZE
                    {
                        if (reader->cursor->obj)
                        {
                            ullist->cursize += sizeof(s_sis_object) + SIS_OBJ_GET_SIZE(reader->cursor->obj);
                        }
                        if (ullist->cursize > ullist->maxsize)
                        {
                            s_sis_object *obj = sis_unlock_queue_pop(ullist->work_queue);
                            ullist->cursize -= sizeof(s_sis_object) + SIS_OBJ_GET_SIZE(obj);
                            sis_object_decr(obj);
                        }                       
                    }
                    reader->cursor = next;
                    next = sis_unlock_queue_next(ullist->work_queue, reader->cursor);
                }
            }            
        }
        if (sis_thread_wait_sleep_msec(wait, 3000) != SIS_ETIMEDOUT)
        {
            // printf("reader notice.\n");
            // 如果被通知就一定有数据 同步消息给其他读者
            if (ullist->users->count > 0)
            {
                sis_mutex_lock(&ullist->userslock);
                // printf("userslock lock 5 %d ok = %p\n",numslock++, &ullist->userslock);
                for (int i = 0; i < ullist->users->count; i++)
                {
                    s_sis_unlock_reader *other = (s_sis_unlock_reader *)sis_pointer_list_get(ullist->users, i);
                    sis_thread_wait_notice(sis_wait_get(other->notice_wait));
                }
                sis_mutex_unlock(&ullist->userslock); 
                // printf("userslock unlock 5 %d ok = %p\n",numslock--, &ullist->userslock);
            }
        }
    }
    sis_thread_wait_stop(wait);
    sis_thread_finish(&reader->work_thread);
    reader->work_status = SIS_UNLOCK_STATUS_NONE;
    return NULL;
}

s_sis_unlock_list *sis_unlock_list_create(size_t maxsize_)
{
    s_sis_unlock_list *o = SIS_MALLOC(s_sis_unlock_list, o);
    
    o->work_queue = sis_unlock_queue_create();

    if (maxsize_ == 0)
    {
        o->save_mode = SIS_UNLOCK_SAVE_ALL;
    }
    else if (maxsize_ == 1)
    {
        o->save_mode = SIS_UNLOCK_SAVE_NONE;
    }
    else
    {
        o->save_mode = SIS_UNLOCK_SAVE_SIZE;
        o->cursize = 0;
        o->maxsize = maxsize_ < 1024 ? 1024 : maxsize_;
    }
    sis_mutex_init(&o->userslock, 0);

    o->users = sis_pointer_list_create();
    o->users->vfree = sis_unlock_reader_destroy;

    // if (o->save_mode != SIS_UNLOCK_SAVE_ALL)
    {
        o->watcher = sis_unlock_reader_create(o, SIS_UNLOCK_READER_HEAD, NULL, NULL, NULL);
        o->watcher->work_status = SIS_UNLOCK_STATUS_WORK;
        if (!sis_thread_create(_thread_watcher, o->watcher, &o->watcher->work_thread))
        {
            LOG(1)("can't start watcher thread.\n");
            sis_unlock_list_destroy(o);
            return NULL;
        }
    }
    return o;
}

void sis_unlock_list_destroy(s_sis_unlock_list *ullist_)
{
    // 必须先清理watch
    if (ullist_->watcher)
    {
        sis_unlock_reader_destroy(ullist_->watcher);
    }
    // 再清理所有读者
    sis_mutex_lock(&ullist_->userslock);
    // printf("userslock lock 4 %d ok = %p\n",numslock++, &ullist_->userslock);
    sis_pointer_list_destroy(ullist_->users);
    sis_mutex_unlock(&ullist_->userslock);
    // printf("userslock unlock 4 %d ok = %p\n",numslock--, &ullist_->userslock);

    sis_unlock_queue_destroy(ullist_->work_queue);
    sis_free(ullist_);
}
void sis_unlock_list_push(s_sis_unlock_list *ullist_, s_sis_object *obj_)
{
    // printf("push.%p -- %d -- ", ullist_, ullist_->users->count);
    sis_unlock_queue_push(ullist_->work_queue, obj_);
    // 只对 watcher 通知
    if (ullist_->watcher)
    {
        sis_thread_wait_notice(sis_wait_get(ullist_->watcher->notice_wait));
    }
    // printf("ok----------.%p.\n", ullist_);
}
void sis_unlock_list_clear(s_sis_unlock_list *ullist_)
{
    // if (ullist_->save_mode == SIS_UNLOCK_SAVE_NONE)
    // {
    //     return ;
    // }
    // 这里一定会出错 可能有读者还没有读完
    // while (ullist_->work_queue->rnums > 0)
    // {
    //     s_sis_object *obj = sis_unlock_queue_pop(ullist_->work_queue);
    //     sis_object_decr(obj);
    // }
    // ullist_->cursize = 0;
}

#endif
