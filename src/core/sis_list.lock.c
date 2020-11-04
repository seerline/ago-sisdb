
#include "sis_list.lock.h"

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
//  s_sis_lock_queue
/////////////////////////////////////////////////
//  单进单出 1M  261 ms
//  1进5类出  1M 1975 - 3800
/////////////////////////////////////////////////

s_sis_lock_queue *sis_lock_queue_create()
{
    s_sis_lock_queue *o = SIS_MALLOC(s_sis_lock_queue, o);
    o->rhead = sis_unlock_node_create(NULL);
    o->rtail = o->rhead;
    sis_rwlock_init(&o->rlock);
    return  o;
}
void sis_lock_queue_clear(s_sis_lock_queue *queue_)
{  
    sis_rwlock_lock_w(&queue_->rlock);
    while (queue_->rhead->next)
    {
        s_sis_unlock_node *head = queue_->rhead;
        sis_object_decr(head->next->obj);
        queue_->rhead = head->next;
        queue_->rnums--;
        sis_unlock_node_destroy(head);
    }         
    sis_rwlock_unlock(&queue_->rlock);
}
void sis_lock_queue_destroy(s_sis_lock_queue *queue_)
{
    sis_lock_queue_clear(queue_);
    sis_unlock_node_destroy(queue_->rtail);
    sis_rwlock_destroy(&queue_->rlock);
    sis_free(queue_);
}
static inline s_sis_unlock_node *sis_lock_queue_head(s_sis_lock_queue *queue_)
{
    s_sis_unlock_node *node = NULL;
    sis_rwlock_lock_r(&queue_->rlock);
    node = queue_->rhead;
    sis_rwlock_unlock(&queue_->rlock);
    return node;
}
static inline s_sis_unlock_node *sis_lock_queue_head_next(s_sis_lock_queue *queue_)
{
    s_sis_unlock_node *node = NULL;
    sis_rwlock_lock_r(&queue_->rlock);
    node = queue_->rhead->next;
    sis_rwlock_unlock(&queue_->rlock);
    return node;
}
static inline s_sis_unlock_node *sis_lock_queue_tail(s_sis_lock_queue *queue_)
{
    s_sis_unlock_node *node = NULL;
    sis_rwlock_lock_r(&queue_->rlock);
    node = queue_->rtail;
    sis_rwlock_unlock(&queue_->rlock);
    return node;    
}
// 返回的数据 需要 sis_object_decr 释放 
static inline s_sis_unlock_node *sis_lock_queue_next(s_sis_lock_queue *queue_, s_sis_unlock_node *node_)
{
    s_sis_unlock_node *node = NULL;
    sis_rwlock_lock_r(&queue_->rlock);
    node = node_->next;
    sis_rwlock_unlock(&queue_->rlock);
    return node;      
}
void sis_lock_queue_push(s_sis_lock_queue *queue_, s_sis_object *obj_)
{  
    s_sis_unlock_node *new_node = sis_unlock_node_create(obj_);
    sis_rwlock_lock_w(&queue_->rlock);
    queue_->rtail->next = new_node;
    queue_->rtail = new_node;
    queue_->rnums++;            
    sis_rwlock_unlock(&queue_->rlock);
}
// 返回的数据 需要 sis_object_decr 释放 
s_sis_object *sis_lock_queue_pop(s_sis_lock_queue *queue_)
{  
    s_sis_object *obj = NULL;    
    sis_rwlock_lock_w(&queue_->rlock);
    s_sis_unlock_node *head = queue_->rhead;
    if (head->next)
    {
        obj = head->next->obj;
        head->next->obj = NULL;
        queue_->rhead = head->next;
        queue_->rnums--;
        sis_unlock_node_destroy(head);
    }         
    sis_rwlock_unlock(&queue_->rlock);
    return obj;
}
/////////////////////////////////////////////////
//  s_sis_wait_queue
/////////////////////////////////////////////////
s_sis_object *sis_wait_queue_pop(s_sis_wait_queue *queue_)
{  
    s_sis_object *obj = NULL;
	if (queue_->count > 0)
	{
	    s_sis_unlock_node *head = queue_->head;
		if (head->next)
		{
			obj = head->next->obj;
			queue_->head = head->next;
			queue_->count--; 
			sis_unlock_node_destroy(head);
		}
	}
    return obj;
}

void *_thread_wait_reader(void *argv_)
{
    s_sis_wait_queue *wqueue = (s_sis_wait_queue *)argv_;
    s_sis_wait *wait = sis_wait_get(wqueue->notice_wait);
    sis_thread_wait_start(wait);
    wqueue->work_status = SIS_UNLOCK_STATUS_WORK;
    while (wqueue->work_status != SIS_UNLOCK_STATUS_EXIT)
    {
        // s_sis_object *obj = NULL;
        sis_mutex_lock(&wqueue->lock);
        if (!wqueue->busy)
        {
            s_sis_object *obj = sis_wait_queue_pop(wqueue);
            if (obj)
            {
                wqueue->busy = 1;
                if (wqueue->cb_reader)
                {
                    wqueue->cb_reader(wqueue->cb_source, obj);
                }
                sis_object_decr(obj);
            }
        }
        sis_mutex_unlock(&wqueue->lock);
        if (sis_thread_wait_sleep_msec(wait, 300) == SIS_ETIMEDOUT)
        {
            // printf("timeout exit. %d %p\n", waitmsec, reader);
        }
        else
        {
            // printf("notice exit. %d\n", waitmsec);
        }       
    }
    sis_thread_wait_stop(wait);
    sis_thread_finish(&wqueue->work_thread);
    wqueue->work_status = SIS_UNLOCK_STATUS_NONE;
    return NULL;
}

s_sis_wait_queue *sis_wait_queue_create(void *cb_source_, cb_lock_reader *cb_reader_)
{
    s_sis_wait_queue *o = SIS_MALLOC(s_sis_wait_queue, o);
    o->head = sis_unlock_node_create(NULL);
    o->tail = o->head;
    o->count = 0;
	o->busy = 0;
    o->cb_source = cb_source_ ? cb_source_ : o;
    o->cb_reader = cb_reader_;
    sis_mutex_init(&o->lock, NULL);
    o->notice_wait = sis_wait_malloc();
    
    // 最后再启动线程 顺序不能变 不然数据会等待
    if (!sis_thread_create(_thread_wait_reader, o, &o->work_thread))
    {
        LOG(1)("can't start reader thread.\n");
        sis_wait_queue_destroy(o);
        return false;
    }
    return  o;
}
void sis_wait_queue_destroy(s_sis_wait_queue *queue_)
{
    queue_->work_status = SIS_UNLOCK_STATUS_EXIT;
    // 通知退出
    sis_thread_wait_notice(sis_wait_get(queue_->notice_wait));
    while (queue_->work_status != SIS_UNLOCK_STATUS_NONE)
    {
        sis_sleep(10);
    }
    sis_wait_free(queue_->notice_wait);
    sis_mutex_lock(&queue_->lock);
    while (queue_->count > 0)
    {
        s_sis_object *obj = sis_wait_queue_pop(queue_);
        sis_object_decr(obj);
    }
    printf("=== clear wait. %d\n", queue_->count);
    sis_mutex_unlock(&queue_->lock);
    sis_unlock_node_destroy(queue_->tail);
    sis_free(queue_);
}
// busy 为 1 只能push 并返回 NULL, 否则直接 设置 busy = 1 并返回原 obj 
s_sis_object *sis_wait_queue_push(s_sis_wait_queue *queue_, s_sis_object *obj_)
{  
    s_sis_object *obj = NULL;
    sis_mutex_lock(&queue_->lock);
    if (!queue_->busy && queue_->count == 0)
    {
        obj = obj_; 
        queue_->busy = 1;
        sis_mutex_unlock(&queue_->lock);
    }
    else
    {
        s_sis_unlock_node *new_node = sis_unlock_node_create(obj_);
        queue_->tail->next = new_node;
        queue_->tail = new_node;
        queue_->count++;
        sis_mutex_unlock(&queue_->lock);
        sis_thread_wait_notice(sis_wait_get(queue_->notice_wait));
    }
	return obj;
}
// 如果队列为空 设置 busy = 0 返回空 | 如果有数据就弹出最早的消息 返回值需要 sis_object_decr 释放 
// pop只在上次发送回调时调用

void sis_wait_queue_set_busy(s_sis_wait_queue *queue_, int isbusy_)
{
    sis_mutex_lock(&queue_->lock);
    queue_->busy = isbusy_;
    sis_mutex_unlock(&queue_->lock);
    if (isbusy_ == 0)
    {
        sis_thread_wait_notice(sis_wait_get(queue_->notice_wait));
    }
}
/////////////////////////////////////////////////
//  s_sis_fast_queue
/////////////////////////////////////////////////

int _fast_queue_move(s_sis_fast_queue *queue_)
{
    if (queue_->wnums < 1)
    {
        return 0;
    }
    queue_->rtail->next = queue_->whead->next;
    queue_->rtail = queue_->wtail;
    queue_->rnums += queue_->wnums;
    queue_->whead->next = NULL;
    queue_->wtail = queue_->whead;
    queue_->wnums = 0;
    printf("move ok %p %p %p %d %d\n", queue_->rtail, queue_->whead->next, queue_->wtail, queue_->rnums, queue_->wnums);
    return queue_->wnums;
}
void *_thread_fast_reader(void *argv_)
{
    s_sis_fast_queue *wqueue = (s_sis_fast_queue *)argv_;
    s_sis_wait *wait = sis_wait_get(wqueue->notice_wait);
    sis_thread_wait_start(wait);

    int waitmsec = wqueue->wait_msec == 0 ? 1000 : wqueue->wait_msec;
    if (wqueue->zero_msec)
    {
        waitmsec = wqueue->zero_msec;
    }
    wqueue->work_status = SIS_UNLOCK_STATUS_WORK;
    bool surpass_waittime = false;
    while (wqueue->work_status != SIS_UNLOCK_STATUS_EXIT)
    {
        // s_sis_object *obj = NULL;
        if (!sis_mutex_trylock(&wqueue->wlock))
        {
            // 如果锁住
            _fast_queue_move(wqueue);
            sis_mutex_unlock(&wqueue->wlock);
        }
        // 读不用加锁
        s_sis_unlock_node *node = wqueue->rhead;
        if (node->next)
        {
            while (node->next)
            {
                s_sis_unlock_node *new_head = node->next;
                if (wqueue->cb_reader)
                {
                    wqueue->cb_reader(wqueue->cb_source, new_head->obj);
                }
                sis_object_decr(new_head->obj);
                sis_unlock_node_destroy(node);
                wqueue->rnums--;
                wqueue->rhead = new_head;
                node = new_head;
            }
        }
        else
        {
            if (surpass_waittime && wqueue->zero_msec)
            {
                wqueue->cb_reader(wqueue->cb_source, NULL);
            }
        }
        
        if (sis_thread_wait_sleep_msec(wait, waitmsec) == SIS_ETIMEDOUT)
        {
            // printf("timeout exit. %d %p\n", waitmsec, reader);
            surpass_waittime = true;
        } 
        else
        {
            surpass_waittime = false;
        }            
    }
    sis_thread_wait_stop(wait);
    sis_thread_finish(&wqueue->work_thread);
    wqueue->work_status = SIS_UNLOCK_STATUS_NONE;
    return NULL;
}

s_sis_fast_queue *sis_fast_queue_create(
    void *cb_source_, cb_lock_reader *cb_reader_,
    int wait_msec_, int zero_msec_)
{
    s_sis_fast_queue *o = SIS_MALLOC(s_sis_fast_queue, o);
    o->rhead = sis_unlock_node_create(NULL);
    o->rtail = o->rhead;
    o->rnums = 0;

    o->whead = sis_unlock_node_create(NULL);
    o->wtail = o->whead;
    o->wnums = 0;
    sis_mutex_init(&o->wlock, NULL);

    o->cb_source = cb_source_ ? cb_source_ : o;
    o->cb_reader = cb_reader_;
    o->notice_wait = sis_wait_malloc();

    o->wago_msec = 0;
    o->wait_msec = wait_msec_ > 1000 ? 1000 : wait_msec_;   
    o->zero_msec = zero_msec_;   

    // 最后再启动线程 顺序不能变 不然数据会等待
    if (!sis_thread_create(_thread_fast_reader, o, &o->work_thread))
    {
        LOG(1)("can't start reader thread.\n");
        sis_fast_queue_destroy(o);
        return false;
    }
    return  o;
}
void sis_fast_queue_destroy(s_sis_fast_queue *queue_)
{
    queue_->work_status = SIS_UNLOCK_STATUS_EXIT;
    // 通知退出
    sis_thread_wait_notice(sis_wait_get(queue_->notice_wait));
    while (queue_->work_status != SIS_UNLOCK_STATUS_NONE)
    {
        sis_sleep(10);
    }
    sis_wait_free(queue_->notice_wait);
    sis_mutex_lock(&queue_->wlock);
    _fast_queue_move(queue_); // 从w迁移到r
    sis_mutex_unlock(&queue_->wlock);

    // 此时没有人在读 直接清理所有 rhead
    s_sis_unlock_node *node = queue_->rhead;
    while (node->next)
    {
        s_sis_unlock_node *new_head = node->next;
        sis_object_decr(new_head->obj);
        sis_unlock_node_destroy(node);
        node = new_head;
        queue_->rnums--;
    }
    
    sis_unlock_node_destroy(queue_->rtail);
    sis_unlock_node_destroy(queue_->wtail);
    sis_free(queue_);
}

// busy 为 1 只能push 并返回 NULL, 否则直接 设置 busy = 1 并返回原 obj 
int sis_fast_queue_push(s_sis_fast_queue *queue_, s_sis_object *obj_)
{  
    s_sis_unlock_node *new_node = sis_unlock_node_create(obj_);
    sis_mutex_lock(&queue_->wlock);
    queue_->wtail->next = new_node;
    queue_->wtail = new_node;
    queue_->wnums++;
    sis_mutex_unlock(&queue_->wlock);
    // if (queue_->wait_msec == 0)
    // {
    //     sis_thread_wait_notice(sis_wait_get(queue_->notice_wait));
    // }
    // else
    // {
    //     msec_t now_msec = sis_time_get_now_msec();
    //     if (now_msec - queue_->wago_msec > queue_->wait_msec / 3) 
    //     {
    //         sis_thread_wait_notice(sis_wait_get(queue_->notice_wait));
    //         queue_->wago_msec = now_msec;
    //     }        
    // }    
	return 1;
}
/////////////////////////////////////////////////
//  s_sis_lock_reader
/////////////////////////////////////////////////
void *_thread_reader(void *argv_)
{
    s_sis_lock_reader *reader = (s_sis_lock_reader *)argv_;
    s_sis_lock_list *ullist = (s_sis_lock_list *)reader->father;
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
                    reader->cursor = sis_lock_queue_head(ullist->work_queue);
                }
                else
                {
                    reader->cursor = sis_lock_queue_tail(ullist->work_queue);
                }
            }
            if (reader->cursor)
            {
                s_sis_unlock_node *next = sis_lock_queue_next(ullist->work_queue, reader->cursor);
                // if (surpass_waittime)
                // {
                //     printf("next = %p\n", next); 
                // }
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
                            if (reader->cb_recv)
                            reader->cb_recv(reader->cb_source, next->obj);
                        }
                        reader->cursor = next;
                        next = sis_lock_queue_next(ullist->work_queue, reader->cursor);
                    } 
                }
                else
                {
                    if (surpass_waittime && (reader->work_mode & SIS_UNLOCK_READER_ZERO))
                    {
                        if (reader->cb_recv)
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
            // printf("timeout exit. %d %p\n", waitmsec, reader);
            surpass_waittime = reader->isrealtime ? true : false;
        }
        else
        {
            // printf("notice exit. %d\n", waitmsec);
            // 有数据来
            surpass_waittime = false;
        }       
    }
    sis_thread_wait_stop(wait);
    sis_thread_finish(&reader->work_thread);
    reader->work_status = SIS_UNLOCK_STATUS_NONE;
    return NULL;
}

s_sis_lock_reader *sis_lock_reader_create(s_sis_lock_list *ullist_, 
    int mode_, void *cb_source_, 
    cb_lock_reader *cb_recv_,
    cb_lock_reader_realtime cb_realtime_)
{
    s_sis_lock_reader *o = SIS_MALLOC(s_sis_lock_reader, o);
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
void sis_lock_reader_destroy(void *reader_)
{
    s_sis_lock_reader *reader = (s_sis_lock_reader *)reader_;
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
void sis_lock_reader_zero(s_sis_lock_reader *reader_, int zeromsec_)
{
    if (zeromsec_ > 0)
    {
        reader_->zeromsec = zeromsec_ > 10 ? zeromsec_ : 10;
        reader_->work_mode |= SIS_UNLOCK_READER_ZERO;
    }
}
bool sis_lock_reader_open(s_sis_lock_reader *reader_)
{
    // 设置工作状态
    reader_->work_status = SIS_UNLOCK_STATUS_WORK;
    // 最后再启动线程 顺序不能变 不然数据会等待
    if (!sis_thread_create(_thread_reader, reader_, &reader_->work_thread))
    {
        LOG(1)("can't start reader thread.\n");
        sis_lock_reader_destroy(reader_);
        return false;
    }
    s_sis_lock_list *ullist = (s_sis_lock_list *)reader_->father;
    sis_rwlock_lock_w(&ullist->userslock);
    sis_pointer_list_push(ullist->users, reader_);
    sis_rwlock_unlock(&ullist->userslock);
    return true;
}

void sis_lock_reader_close(s_sis_lock_reader *reader_)
{
    s_sis_lock_list *ullist = (s_sis_lock_list *)reader_->father;
    sis_rwlock_lock_w(&ullist->userslock);
    int index = sis_pointer_list_indexof(ullist->users, reader_);
    sis_pointer_list_delete(ullist->users, index , 1);
    sis_rwlock_unlock(&ullist->userslock);
}   

/////////////////////////////////////////////////
//  s_sis_lock_list
//  单进单出 1秒 1M条记录
/////////////////////////////////////////////////
bool _unlock_check_user_cursor(s_sis_lock_list *ullist, s_sis_unlock_node *node)
{
    sis_rwlock_lock_r(&ullist->userslock);
    bool o = false;
    for (int i = 0; i < ullist->users->count; i++)
    {
        s_sis_lock_reader *reader = (s_sis_lock_reader *)sis_pointer_list_get(ullist->users, i);
        if (reader->cursor == node)
        {
            o = true;
            break;
        }
    }
    sis_rwlock_unlock(&ullist->userslock);  
    return o;
}
static void *_thread_watcher(void *argv_)
{
    s_sis_lock_reader *reader = (s_sis_lock_reader *)argv_;
    s_sis_lock_list *ullist = (s_sis_lock_list *)reader->father;
    s_sis_wait *wait = sis_wait_get(reader->notice_wait);
    sis_thread_wait_start(wait);
    while (reader->work_status != SIS_UNLOCK_STATUS_EXIT)
    {
        if (sis_thread_wait_sleep_msec(wait, 3000) != SIS_ETIMEDOUT)
        {
            // printf("reader notice.\n");
            // 如果被通知就一定有数据 同步消息给其他读者
            if (ullist->users->count > 0)
            {
                sis_rwlock_lock_r(&ullist->userslock);
                for (int i = 0; i < ullist->users->count; i++)
                {
                    s_sis_lock_reader *other = (s_sis_lock_reader *)sis_pointer_list_get(ullist->users, i);
                    sis_thread_wait_notice(sis_wait_get(other->notice_wait));
                }
                sis_rwlock_unlock(&ullist->userslock); 
            }
            // 只有超过3秒才去处理数据清理
            // sis_sleep(10);
            continue;
        }
        if (reader->work_status == SIS_UNLOCK_STATUS_EXIT)
        {
            break;
        }
        if (reader->work_status != SIS_UNLOCK_STATUS_WORK)
        {
            continue;
        }
        if (ullist->save_mode != SIS_UNLOCK_SAVE_ALL) // 需要全部保留就什么也不干
        {
            if (reader->cursor == NULL)
            {
                s_sis_unlock_node *node = sis_lock_queue_head_next(ullist->work_queue);
                if (node)
                {
                    reader->cursor = node;
                }   
            }
            if (reader->cursor)
            {
                s_sis_unlock_node *next = sis_lock_queue_next(ullist->work_queue, reader->cursor);
                while(next)   
                {
                    if (_unlock_check_user_cursor(ullist, reader->cursor))
                    {
                        break;
                    }
                    if (ullist->save_mode == SIS_UNLOCK_SAVE_NONE)
                    {
                        // 销毁一个大家都不用的数据
                        s_sis_object *obj = sis_lock_queue_pop(ullist->work_queue);
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
                            s_sis_object *obj = sis_lock_queue_pop(ullist->work_queue);
                            ullist->cursize -= sizeof(s_sis_object) + SIS_OBJ_GET_SIZE(obj);
                            sis_object_decr(obj);
                        }                       
                    }
                    reader->cursor = next;
                    next = sis_lock_queue_next(ullist->work_queue, reader->cursor);
                }
            }            
        }
    }
    sis_thread_wait_stop(wait);
    sis_thread_finish(&reader->work_thread);
    reader->work_status = SIS_UNLOCK_STATUS_NONE;
    return NULL;
}

s_sis_lock_list *sis_lock_list_create(size_t maxsize_)
{
    s_sis_lock_list *o = SIS_MALLOC(s_sis_lock_list, o);
    
    o->work_queue = sis_lock_queue_create();

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
    sis_rwlock_init(&o->userslock);

    o->users = sis_pointer_list_create();
    o->users->vfree = sis_lock_reader_destroy;

    {
        o->watcher = sis_lock_reader_create(o, SIS_UNLOCK_READER_HEAD, NULL, NULL, NULL);
        o->watcher->work_status = SIS_UNLOCK_STATUS_WORK;
        if (!sis_thread_create(_thread_watcher, o->watcher, &o->watcher->work_thread))
        {
            LOG(1)("can't start watcher thread.\n");
            sis_lock_list_destroy(o);
            return NULL;
        }
    }
    return o;
}

void sis_lock_list_destroy(s_sis_lock_list *ullist_)
{
    // 必须先清理watch
    if (ullist_->watcher)
    {
        sis_lock_reader_destroy(ullist_->watcher);
    }
    // 再清理所有读者
    sis_rwlock_lock_w(&ullist_->userslock);
    sis_pointer_list_destroy(ullist_->users);
    sis_rwlock_unlock(&ullist_->userslock);

    sis_rwlock_destroy(&ullist_->userslock);
    sis_lock_queue_destroy(ullist_->work_queue);
    sis_free(ullist_);
}
void sis_lock_list_push(s_sis_lock_list *ullist_, s_sis_object *obj_)
{
    sis_lock_queue_push(ullist_->work_queue, obj_);
    // 只对 watcher 通知
    if (ullist_->watcher)
    {
        sis_thread_wait_notice(sis_wait_get(ullist_->watcher->notice_wait));
    }
}
void sis_lock_list_clear(s_sis_lock_list *ullist_)
{
    if (ullist_->save_mode == SIS_UNLOCK_SAVE_NONE)
    {
        return ;
    }
    sis_lock_queue_clear(ullist_->work_queue);
    ullist_->cursize = 0;
}

#if 1

int maxnums = 1000*1000;
volatile int series = 0;
volatile int pops = 0;
int exit__ = 0; 
int reader_id = 0;
int read_pops[10] ={0};

static void *thread_push(void *arg)
{
    s_sis_lock_list *share = (s_sis_lock_list *)arg;
    while(exit__ == 0)
    {
        sis_sleep(1);
    }
    series = 0;
    while(series < maxnums)
    {
        series++;
        s_sis_object *obj = sis_object_create(SIS_OBJECT_INT, (void *)series);
        sis_lock_list_push(share, obj); 
        // printf("队列插入元素：%d %d\n", series, share->work_queue->count);
        // if ((series % 100) == 0)
        // if (share->work_queue->wnums > 0)
        //     printf("%d %d| %d %d %d\n", share->work_queue->rnums, share->work_queue->wnums, read_pops[0], read_pops[1], read_pops[2]);
        sis_object_destroy(obj);
        // sis_sleep(10);
    }
    return NULL;
}

static void *thread_fast_push(void *arg)
{
    s_sis_fast_queue *queue = (s_sis_fast_queue *)arg;
    while(exit__ == 0)
    {
        sis_sleep(1);
    }
    series = 0;
    while(series < maxnums)
    {
        series++;
        s_sis_object *obj = sis_object_create(SIS_OBJECT_INT, (void *)series);
        sis_fast_queue_push(queue, obj); 
        // printf("队列插入元素：%d %d\n", series, share->work_queue->count);
        sis_object_destroy(obj);
    }
    return NULL;
}
static int cb_recv(void *src, s_sis_object *obj)
{
    s_sis_lock_reader *reader = (s_sis_lock_reader *)src;
    ADDF(&pops, 1);
    ADDF(&read_pops[reader->zeromsec], 1);
    // printf("队列输出元素：------÷ %s : %d ||  %d\n", reader->sign, (int)obj->ptr, ADDF(&pops, 1));
    // sis_sleep(1);
    return 0;
}
static int cb_fast_recv(void *src, s_sis_object *obj)
{
    ADDF(&pops, 1);
    // printf("队列输出元素：------÷ %s : %d ||  %d\n", reader->sign, (int)obj->ptr, ADDF(&pops, 1));
    if(pops%100==0) sis_sleep(1);
    return 0;
}
s_sis_lock_queue *read_queue[10];
static void *thread_pop(void *arg)
{
    int index= (int)arg;
    s_sis_lock_queue *reader = read_queue[index];
    read_pops[index] = 0;
    while (read_pops[index] < maxnums)
    {
        s_sis_object *obj = sis_lock_queue_pop(reader);        
        while (obj)
        {
            // pops++;
            // read_pops[index]++;
            ADDF(&pops, 1);
            ADDF(&read_pops[index], 1);
            sis_object_decr(obj);
            obj = sis_lock_queue_pop(reader); 
        }
        sis_sleep(1);
    }  
    return NULL;  
}

int main1()
{
    safe_memory_start();

    { // test queue
        s_sis_lock_queue *queue = sis_lock_queue_create();
        msec_t start = sis_time_get_now_msec();
        int inums = 0;
        int onums = 0;
        while(inums < maxnums)
        {
            inums++;
            s_sis_object *obj = sis_object_create(SIS_OBJECT_INT, (void *)inums);
            sis_lock_queue_push(queue, obj); 
            // printf("队列插入元素：%d %d\n", inums, queue->count);
            sis_object_destroy(obj);  
        }
        printf("cost %llu us\n", sis_time_get_now_msec() - start);
        while(onums < maxnums)
        {
            s_sis_object *obj = sis_lock_queue_pop(queue);        
            if (obj)
            {
                onums++;
                // printf("队列输出元素：------ %d ||  %d\n", (int)(obj->ptr), ADDF(&onums, 1));
                sis_object_destroy(obj); 
            }
        }
        sis_lock_queue_destroy(queue);
        printf("cost %llu us\n", sis_time_get_now_msec() - start);
    }
    // { // test list
    //     s_sis_lock_queue *input = sis_lock_queue_create();
    //     int inums = 0;
    //     int reader = 5;
    //     pops = 0;
    //     pthread_t   read[10];
    //     for (int i = 0; i < reader; i++)
    //     {
    //         read_queue[i] = sis_lock_queue_create();
    //         pthread_create(&read[i],NULL,thread_pop,(void*)i);
    //     }
    //     msec_t start = sis_time_get_now_msec();
    //     while(inums < maxnums)
    //     {
    //         inums++;
    //         s_sis_object *obj = sis_object_create(SIS_OBJECT_INT, (void *)inums);
    //         sis_lock_queue_push(input, obj); 
    //         // printf("队列插入元素：%d %d\n", inums, input->count);
    //         sis_object_destroy(obj);  
    //     }   
    //     printf("cost 1 %llu us\n", sis_time_get_now_msec() - start);         
    //     s_sis_object *obj = sis_lock_queue_pop(input);        
    //     while (obj)
    //     {
    //         for (int i = 0; i < reader; i++)
    //         {
    //             sis_lock_queue_push(read_queue[i], obj);
    //         }
    //         sis_object_destroy(obj); 
    //         obj = sis_lock_queue_pop(input); 
    //     }
    //     printf("cost 2 %llu us\n", sis_time_get_now_msec() - start);
    //     for(int i = 0; i < reader; i++)
    //     {
    //         printf("out 1 [%d] = %d -- %d + %d\n", i, read_pops[i], read_queue[i]->rnums, read_queue[i]->wnums);
    //     }
        
    //     for(int i = 0; i < reader; i++)
    //     {
    //         pthread_join(read[i], NULL);
    //         printf("out 2 [%d] = %d -- %d + %d\n", i, read_pops[i], read_queue[i]->rnums, read_queue[i]->wnums);
    //     }
    //     // for (int i = 0; i < reader; i++)
    //     // {
    //     //     s_sis_object *readerobj = sis_lock_queue_pop(read_queue[i]);
    //     //     while (readerobj)
    //     //     {
    //     //         ADDF(&pops, 1);
    //     //         sis_object_destroy(readerobj); 
    //     //         readerobj = sis_lock_queue_pop(read_queue[i]);
    //     //     }           
    //     // }
    //     printf("cost 3 %llu us %d\n", sis_time_get_now_msec() - start, pops);
    //     for (int i = 0; i < reader; i++)
    //     {
    //         sis_lock_queue_destroy(read_queue[i]);
    //     }
    //     printf("cost 4 %llu us\n", sis_time_get_now_msec() - start);
    //     sis_lock_queue_destroy(input);
    //     printf("cost 5 %llu us\n", sis_time_get_now_msec() - start);
    // }
    safe_memory_stop();
	return 1;
}
int main()
{
    safe_memory_start();
    { // test fast list
        s_sis_fast_queue *fastlist = sis_fast_queue_create(NULL, cb_fast_recv, 20, 0);
        
        pthread_t write[10];
        int       writer = 1;
        pops = 0;
        for(int i = 0;i < writer; i++)
        {
            pthread_create(&write[i],NULL,thread_fast_push,(void*)fastlist);
        }
        msec_t start = sis_time_get_now_msec();
        exit__ = 1;
        for(int i = 0; i < writer; i++)
        {
            pthread_join(write[i], NULL);
        }
        printf("cost push %llu us , %d ==> %d\n", sis_time_get_now_msec() - start, series, pops);  
        printf("recv %d count= %d + %d\n", pops, fastlist->rnums, fastlist->wnums);
        while (pops < series)  
        {
            sis_sleep(1);
        }
        printf("cost pop %llu us \n", sis_time_get_now_msec() - start);  
        sis_fast_queue_destroy(fastlist);
        printf("cost %llu us , %d ==> %d\n", sis_time_get_now_msec() - start, series, pops);  

    }
    { // test thread & list
        s_sis_lock_list *sharedb = sis_lock_list_create(0);

        pthread_t          write[10];
        s_sis_lock_reader *read[10];

        int reader = 1;
        int writer = 1;
        // int reader = 10;
        // int writer = 5;
        pops = 0;

        //创建两个线程
        for(int i = 0;i < reader; i++)
        {
            read_pops[i] = 0;
            read[i] = sis_lock_reader_create(sharedb, SIS_UNLOCK_READER_HEAD, NULL, cb_recv, NULL);
            // read[i] = sis_lock_reader_create(sharedb, SIS_UNLOCK_READER_TAIL, NULL, cb_recv, NULL);
            read[i]->zeromsec = i;
            sis_lock_reader_open(read[i]);
            printf("read %d \n", sharedb->users->count);
        }
        // 所有写和读都建设好再开始发送数据
        // sis_sleep(100);
        // printf("read %d \n", sharedb->users->count);
        for(int i = 0;i < writer; i++)
        {
            pthread_create(&write[i],NULL,thread_push,(void*)sharedb);
        }
        msec_t start = sis_time_get_now_msec();
        exit__ = 1;
        // sis_sleep(100);
        for(int i = 0; i < writer; i++)
        {
            pthread_join(write[i], NULL);
        }

        printf("cost push %llu us , %d ==> %d\n", sis_time_get_now_msec() - start, series, pops);  
        for(int i = 0; i < reader; i++)
        {
            // 这里需要等所有线程收到完整消息        
            while (read_pops[i] < series)  
            {
                sis_sleep(1);
            }
            printf("recv %d count= %d\n", 
                read_pops[i], sharedb->work_queue->rnums);
            sis_lock_reader_close(read[i]);
            printf("recv %d : %d\n", i, read_pops[i]);
        }
        // 不延时居然会有内存不释放 ??? 奇怪了
        // sis_sleep(100);
        // printf("sharedb->work_queue %d \n", sharedb->work_queue->count);
        printf("cost pop %llu us , %d ==> %d\n", sis_time_get_now_msec() - start, series, pops);  
        sis_lock_list_destroy(sharedb);
        printf("cost %llu us , %d ==> %d\n", sis_time_get_now_msec() - start, series, pops);  

    }

    safe_memory_stop();
	return 1;
}
#endif

