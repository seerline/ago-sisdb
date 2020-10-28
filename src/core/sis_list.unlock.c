
#include "sis_list.def.h"
#include "sis_list.unlock.h"

#ifdef MAKE_UNLOCK_LIST

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
//  单任务的链表
//  单进单出 1M 25ms 基本为内存申请的时间 
/////////////////////////////////////////////////
// s_sis_unlock_queue *sis_unlock_queue_create()
// {
//     s_sis_unlock_queue *o = SIS_MALLOC(s_sis_unlock_queue, o);
//     o->head = sis_unlock_node_create(NULL);
//     o->tail = o->head;
//     return  o;
// }
// void sis_unlock_queue_destroy(s_sis_unlock_queue *queue_)
// {
//     while (queue_->count > 0)
//     {
//         s_sis_object *obj = sis_unlock_queue_pop(queue_);
//         sis_object_decr(obj);
//     }
//     sis_unlock_node_destroy(queue_->tail);
//     sis_free(queue_);
// }
// void sis_unlock_queue_push(s_sis_unlock_queue *queue_, s_sis_object *obj_)
// {  
//     s_sis_unlock_node *new_node = sis_unlock_node_create(obj_);
//     queue_->tail->next = new_node;
//     queue_->tail = new_node;
//     queue_->count++;
// }
// s_sis_object *sis_unlock_queue_pop(s_sis_unlock_queue *queue_)
// {  
//     s_sis_object *obj = NULL;
//     if (queue_->head->next)
//     {
//         s_sis_unlock_node *head = queue_->head;
//         obj = head->next->obj;
//         queue_->head = head->next;
//         queue_->count--;
//         sis_unlock_node_destroy(head);
//     }
//     return obj;
// }
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
    sis_unlock_mutex_init(&o->rlock, 0);
    sis_unlock_mutex_init(&o->wlock, 0);
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
    sis_unlock_node_destroy(queue_->rtail);
    sis_unlock_node_destroy(queue_->wtail);
    sis_free(queue_);
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
//  环形数组的无锁队列 缺点是队列过多无法处理
//  单进单出 1M 118 - 258ms
//  1进5出  1M  1500 -1887 ms
//  1进5类出  1M 4580 - 5325
/////////////////////////////////////////////////
// s_sis_unlock_queue *sis_unlock_queue_create()
// {
//     s_sis_unlock_queue *o = SIS_MALLOC(s_sis_unlock_queue, o);
//     o->size = 4 * 1024 * 1024;
//     o->flags = sis_malloc(o->size);
//     memset(o->flags, 0, o->size);
//     o->array = sis_malloc(o->size * sizeof(s_sis_unlock_node));
//     memset(o->array, 0, o->size * sizeof(s_sis_unlock_node));
//     o->count = 0;
//     sis_unlock_mutex_init(&o->growlock, 0);
//     return  o;
// }
// void sis_unlock_queue_destroy(s_sis_unlock_queue *queue_)
// {
//     // printf("exit : %d\n", queue_->count);
//     while (queue_->count > 0)
//     {
//         s_sis_object *obj = sis_unlock_queue_pop(queue_);
//         sis_object_decr(obj);
//     }
//     sis_free(queue_->flags);
//     sis_free(queue_->array);
//     sis_free(queue_);
// }
// bool sis_unlock_queue_grow(s_sis_unlock_queue *queue_)
// {
//     // 数组不够大咋办呢
//     int size = queue_->size;
//     if (queue_->count >= size - 5 )
//     {
//         LOG(1)("unlock array size too small.\n");
//         if (!sis_unlock_mutex_try_lock(&queue_->growlock))
//         {
//             // sis_out_binary("old", queue_->flags, queue_->size);
//             if (queue_->size == size)
//             {
//                 int new_size = size * 4;
//                 char *new_flags = sis_calloc(new_size);
//                 memmove(new_flags, queue_->flags, queue_->size);
//                 // memset(new_flags + queue_->size, 0, new_size - queue_->size);
//                 s_sis_unlock_node *new_array = sis_calloc(new_size * sizeof(s_sis_unlock_node));
//                 memmove(new_array, queue_->array, queue_->size * sizeof(s_sis_unlock_node));
//                 // memset(new_array + queue_->size, 0, (new_size - queue_->size) * sizeof(s_sis_unlock_node));
//                 // __sync_lock_test_and_set(&queue_->flags, new_flags);
//                 // __sync_lock_test_and_set(&queue_->array, new_array);
//                 sis_free(queue_->flags);
//                 sis_free(queue_->array);
//                 queue_->flags = new_flags;
//                 queue_->array = new_array;
//                 // queue_->size = new_size;
//                 __sync_lock_test_and_set(&queue_->size, new_size);
//             }
//             // sis_out_binary("new", queue_->flags, queue_->size);
//             sis_unlock_mutex_unlock(&queue_->growlock);
//         }
//         else
//         {
//             return false;
//         }
//         LOG(1)("unlock array size %p %d.\n", queue_, queue_->size);
//     }
//     return true;    
// }
// void sis_unlock_queue_push(s_sis_unlock_queue *queue_, s_sis_object *obj_)
// {  
//     if (queue_->count >= queue_->size)
//     {
//         LOG(1)("unlock array size too small.\n");
//         return;
//     }
//     // while (!sis_unlock_queue_grow(queue_))
//     // {
//     //     sis_sleep(1);
//     // }
//     // printf("1.%d\n",queue_->size);
//     s_sis_unlock_node new_node;
//     sis_object_incr(obj_);
//     new_node.obj = obj_;
//     new_node.next = NULL;

//     int cur_tail_idx = queue_->tail_idx;
//     char *cur_tail_flag = queue_->flags + cur_tail_idx;
//     // 忙式等待
//     // while中的原子操作：如果当前tail的标记为“”已占用(1)“，则更新 cur_tail_flag,
//     // 继续循环；否则，将tail标记设为已经占用
//     while (!BCAS(cur_tail_flag, 0, 1))
//     {
//         cur_tail_idx = queue_->tail_idx;
//         cur_tail_flag = queue_->flags + cur_tail_idx;
//         // printf("push %d %d | %d %d\n",queue_->size, queue_->count, cur_tail_idx, cur_tail_flag[0]);
//     }
//     // printf("3 %d\n",queue_->size);
//     // 两个入队线程之间的同步
//     // 取模操作可以优化
//     int new_tail_idx = (cur_tail_idx + 1) % queue_->size;
//     // 如果已经被其他的线程更新过，则不需要更新；
//     // 否则，更新为 (cur_tail_idx+1) % size_;
//     BCAS(&queue_->tail_idx, cur_tail_idx, new_tail_idx);
//     // 申请到可用的存储空间
//     *(queue_->array + cur_tail_idx) = new_node;
//     // 写入完毕
//     ADDF(cur_tail_flag, 1); // == 2
//     // 更新count;入队线程与出队线程之间的协作
//     ADDF(&(queue_->count), 1);
// }
// // 返回的数据 需要 sis_object_decr 释放 
// s_sis_object *sis_unlock_queue_pop(s_sis_unlock_queue *queue_)
// {  
//     s_sis_object *obj = NULL;
//     if (queue_->count <= 0)
//     {
//         return NULL;
//     }
//     if (!sis_unlock_mutex_try_lock(&queue_->growlock))
//     {
//         int cur_head_idx = queue_->head_idx;
//         char *cur_head_flag = queue_->flags + cur_head_idx;
//         while (!BCAS(cur_head_flag, 2, 3)) 
//         {
//             cur_head_idx = queue_->head_idx;
//             cur_head_flag = queue_->flags + cur_head_idx;
//             // printf("pop %d %d | %d %d\n",queue_->size, queue_->count, cur_head_idx, cur_head_flag[0]);
//         }
//         // 取模操作可以优化
//         int new_head_idx = (cur_head_idx + 1) % queue_->size;
//         BCAS(&queue_->head_idx, cur_head_idx, new_head_idx);

//         obj = (queue_->array + cur_head_idx)->obj;
//         // 弹出完毕
//         SUBF(cur_head_flag, 3); // == 0 
//         // 更新size
//         SUBF(&(queue_->count), 1);
//         sis_unlock_mutex_unlock(&queue_->growlock);
//     }
    
//     return obj;
// }

/////////////////////////////////////////////////
//  s_sis_unlock_reader
/////////////////////////////////////////////////

void *_thread_reader(void *argv_)
{
    s_sis_unlock_reader *reader = (s_sis_unlock_reader *)argv_;

    msec_t agotime = sis_time_get_now_msec();
    while (reader->work_status != SIS_UNLOCK_STATUS_EXIT)
    {
        if (reader->work_status == SIS_UNLOCK_STATUS_WORK)
        {
            s_sis_object *obj = sis_unlock_queue_pop(reader->work_queue);
            bool isnewdata = obj ? true : false;
            while(obj)
            {
                if (reader->work_status == SIS_UNLOCK_STATUS_EXIT)
                {
                    // 加速退出
                    sis_object_decr(obj);
                    break;
                }
                if (reader->cb_recv)
                {
                    reader->cb_recv(reader->cb_source, obj);
                }
                reader->cursor = obj;
                sis_object_decr(obj);
                obj = sis_unlock_queue_pop(reader->work_queue);
            }
            if (isnewdata && reader->work_mode & SIS_UNLOCK_READER_ZERO)
            {
                agotime = sis_time_get_now_msec();
            }
            // if (!obj)
            {
                // 如果读完发送标志
                if ((reader->work_mode & SIS_UNLOCK_READER_HEAD) && reader->isrealtime == 0)
                {
                    // 只发一次
                    if (reader->cb_realtime)
                    {
                        reader->cb_realtime(reader->cb_source);
                    }
                    reader->isrealtime = 1;
                }  
                if (!isnewdata && (reader->work_mode & SIS_UNLOCK_READER_ZERO))
                {
                    msec_t nowtime = sis_time_get_now_msec();
                    if ((nowtime - agotime) > reader->zeromsec)
                    {
                        reader->cb_recv(reader->cb_source, NULL);
                        agotime = nowtime;
                    }
                }
            }
        }
        if(!sis_unlock_mutex_try_unlock(&reader->notice))
        {
            continue;
        }
        sis_sleep(3);
    }
    sis_thread_finish(&reader->work_thread);
    reader->work_status = SIS_UNLOCK_STATUS_NONE;
    return NULL;
}

static void *_thread_reader_wait(void *argv_)
{
    s_sis_unlock_reader *reader = (s_sis_unlock_reader *)argv_;

    while (reader->work_status != SIS_UNLOCK_STATUS_EXIT)
    {
        if (reader->work_status == SIS_UNLOCK_STATUS_WORK)
        {
            // 只有工作状态才会读取数据
            s_sis_object *obj = sis_unlock_queue_pop(reader->work_queue);
            if (obj)
            {
                reader->work_status = SIS_UNLOCK_STATUS_WAIT;
                if (reader->cb_recv)
                {
                    reader->cb_recv(reader->cb_source, obj);
                }
                reader->cursor = obj;
                sis_object_decr(obj);
            }
        }
        if(!sis_unlock_mutex_try_unlock(&reader->notice))
        {
            continue;
        }
        sis_sleep(3);
    }
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
    o->sendsave = 0; // 未发送历史数据
    sis_unlock_mutex_init(&o->notice, 0);

    o->work_queue = sis_unlock_queue_create();

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
        while (reader->work_status != SIS_UNLOCK_STATUS_NONE)
        {
            sis_sleep(1);
        }
    }
    sis_unlock_queue_destroy(reader->work_queue);
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
void _unlock_reader_sendsave(s_sis_unlock_reader *reader_, s_sis_object *obj_);
bool sis_unlock_reader_open(s_sis_unlock_reader *reader_)
{
    // 发送历史数据 
    _unlock_reader_sendsave(reader_, NULL);
    // 设置工作状态
    reader_->work_status = SIS_UNLOCK_STATUS_WORK;
    // 最后再启动线程 顺序不能变 不然数据会等待
    if (reader_->work_mode & SIS_UNLOCK_READER_WAIT)
    {
        // work_status 一直等于 SIS_UNLOCK_STATUS_NONE
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
    sis_unlock_mutex_lock(&ullist->userslock);
    sis_pointer_list_push(ullist->users, reader_);
    sis_unlock_mutex_unlock(&ullist->userslock);

    return true;
}

// 有新消息就先回调 然后由用户控制读下一条信息 next返回为空时重新激活线程的执行 
void sis_unlock_reader_next(s_sis_unlock_reader *reader_)
{
    if (reader_->work_mode & SIS_UNLOCK_READER_WAIT)
    {
        if (reader_->work_status != SIS_UNLOCK_STATUS_WAIT)
        {
            return ;
        }
        s_sis_object *obj = sis_unlock_queue_pop(reader_->work_queue);
        if (obj)
        {
            if (reader_->cb_recv)
            {
                reader_->cb_recv(reader_->cb_source, obj);
            }  
            reader_->cursor = obj;
            sis_object_decr(obj);          
        }
        else
        {
            reader_->work_status = SIS_UNLOCK_STATUS_WORK;
        }
        
    }
}
void sis_unlock_reader_close(s_sis_unlock_reader *reader_)
{
    s_sis_unlock_list *ullist = (s_sis_unlock_list *)reader_->father;
    sis_unlock_mutex_lock(&ullist->userslock);
    int index = sis_pointer_list_indexof(ullist->users, reader_);
    sis_pointer_list_delete(ullist->users, index , 1);
    sis_unlock_mutex_unlock(&ullist->userslock);
}   

/////////////////////////////////////////////////
//  s_sis_unlock_list
/////////////////////////////////////////////////
void _unlock_reader_sendsave(s_sis_unlock_reader *reader_, s_sis_object *obj_)
{
    int agos = reader_->sendsave;
    s_sis_unlock_list *ullist = (s_sis_unlock_list *)reader_->father;
    if (reader_->work_mode & SIS_UNLOCK_READER_HEAD)
    {
        if (reader_->sendsave < ullist->objs->count)
        {
            sis_unlock_mutex_lock(&ullist->objslock);
            for (int k = reader_->sendsave; k < ullist->objs->count; k++)
            {
                reader_->sendsave++;
                sis_unlock_queue_push(reader_->work_queue, (s_sis_object *)sis_pointer_list_get(ullist->objs, k));
            }
            sis_unlock_mutex_unlock(&ullist->objslock);    
        }
    }
    if (obj_)
    {
        reader_->sendsave ++;
        sis_unlock_queue_push(reader_->work_queue, obj_);
    }
    // printf("reader_->sendsave [%d:%d] %d --> %d\n", reader_->zeromsec, ullist->users->count, agos, reader_->sendsave);
    if (reader_->sendsave > agos)
    {
        sis_unlock_mutex_try_lock(&reader_->notice);
    }
}

// 使用notice_wait 太耗时了 采用CAS方式
static void *_thread_watcher(void *argv_)
{
    s_sis_unlock_reader *reader = (s_sis_unlock_reader *)argv_;
    s_sis_unlock_list *ullist = (s_sis_unlock_list *)reader->father;

    while (reader->work_status != SIS_UNLOCK_STATUS_EXIT)
    {
        if (reader->work_status == SIS_UNLOCK_STATUS_WORK)
        {
            s_sis_object *obj = sis_unlock_queue_pop(ullist->work_queue);
            if (obj)
            {
                while (obj)
                {
                    if (reader->work_status == SIS_UNLOCK_STATUS_EXIT)
                    {
                        sis_object_decr(obj);
                        break;
                    }
                    sis_unlock_mutex_lock(&ullist->userslock);
                    for (int i = 0; i < ullist->users->count; i++)
                    {
                        s_sis_unlock_reader *other = (s_sis_unlock_reader *)sis_pointer_list_get(ullist->users, i);
                        // 这里需要再次处理历史数据 否则可能会漏掉数据
                        _unlock_reader_sendsave(other, obj);
                    }
                    sis_unlock_mutex_unlock(&ullist->userslock);
                    if (ullist->save_mode != SIS_UNLOCK_SAVE_NONE)
                    {
                        if (reader->cb_recv)
                        {
                            reader->cb_recv(reader->cb_source, obj);
                        }
                    }
                    sis_object_decr(obj);
                    obj = sis_unlock_queue_pop(ullist->work_queue);
                }
            }
            else
            {
                sis_unlock_mutex_lock(&ullist->userslock);
                for (int i = 0; i < ullist->users->count; i++)
                {
                    s_sis_unlock_reader *other = (s_sis_unlock_reader *)sis_pointer_list_get(ullist->users, i);
                    // 这里需要再次处理历史数据 否则可能会漏掉数据
                    _unlock_reader_sendsave(other, NULL);
                }
                sis_unlock_mutex_unlock(&ullist->userslock);            
            }
        }
        // 把得到的数据传送到读者的队列中
        if(!sis_unlock_mutex_try_unlock(&ullist->watcher->notice))
        {
            continue;
        }
        sis_sleep(3);
    }
    sis_thread_finish(&reader->work_thread);
    reader->work_status = SIS_UNLOCK_STATUS_NONE;
    return NULL;
}
bool _unlock_list_find_user(s_sis_unlock_list *ullist, s_sis_object *obj)
{
    sis_unlock_mutex_lock(&ullist->userslock);
    for (int i = 0; i < ullist->users->count; i++)
    {
        s_sis_unlock_reader *other = (s_sis_unlock_reader *)sis_pointer_list_get(ullist->users, i);
        if (other->cursor == obj)
        {
            return true;
        }
    }
    sis_unlock_mutex_unlock(&ullist->userslock); 
    return false;   
}
void _unlock_list_sub_sendsave(s_sis_unlock_list *ullist)
{
    sis_unlock_mutex_lock(&ullist->userslock);
    for (int i = 0; i < ullist->users->count; i++)
    {
        s_sis_unlock_reader *other = (s_sis_unlock_reader *)sis_pointer_list_get(ullist->users, i);
        other->sendsave--;
    }
    sis_unlock_mutex_unlock(&ullist->userslock); 
}
static int cb_watcher_recv(void *source_, s_sis_object *obj_)
{
    // 保存历史数据 可能耗时
    s_sis_unlock_reader *reader = (s_sis_unlock_reader *)source_;
    s_sis_unlock_list *ullist = (s_sis_unlock_list *)reader->father;
    if (ullist->save_mode == SIS_UNLOCK_SAVE_NONE)
    {
        return 0;
    }
    sis_unlock_mutex_lock(&ullist->objslock);
    sis_object_incr(obj_);
    sis_pointer_list_push(ullist->objs, obj_);
    ullist->cursize += sizeof(s_sis_object) + SIS_OBJ_GET_SIZE(obj_);
    if (ullist->save_mode == SIS_UNLOCK_SAVE_SIZE)
    {
        while(ullist->cursize > ullist->maxsize)
        {
            s_sis_object *obj = (s_sis_object *)sis_pointer_list_get(ullist->objs, 0);
            if (!obj)
            {
                break;
            }
            if (!_unlock_list_find_user(ullist, obj))
            {
                ullist->cursize -= sizeof(s_sis_object) + SIS_OBJ_GET_SIZE(obj);
                sis_pointer_list_delete(ullist->objs, 0, 1);
                _unlock_list_sub_sendsave(ullist);
            }
        }
    }
    sis_unlock_mutex_unlock(&ullist->objslock);
    return 1;
}

s_sis_unlock_list *sis_unlock_list_create(size_t maxsize_)
{
    s_sis_unlock_list *o = SIS_MALLOC(s_sis_unlock_list, o);
    
    o->work_queue = sis_unlock_queue_create();

    if (maxsize_ == 0 || maxsize_ == 2)
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
    }
    switch (o->save_mode)
    {
    case SIS_UNLOCK_SAVE_ALL:
    case SIS_UNLOCK_SAVE_SIZE:
        sis_unlock_mutex_init(&o->objslock, 0);
        o->objs = sis_pointer_list_create();
        o->objs->vfree = sis_object_decr;
        o->cursize = 0;
        o->maxsize = maxsize_;
        break;    
    default:
        break;
    }
    sis_unlock_mutex_init(&o->userslock, 0);

    o->users = sis_pointer_list_create();
    o->users->vfree = sis_unlock_reader_destroy;

    if (maxsize_ != 2)
    {
        o->watcher = sis_unlock_reader_create(o, SIS_UNLOCK_READER_HEAD, NULL, cb_watcher_recv, NULL);
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
    sis_unlock_mutex_lock(&ullist_->userslock);
    sis_pointer_list_destroy(ullist_->users);
    sis_unlock_mutex_unlock(&ullist_->userslock);
    // 必须先清理所有读者 否则有可能清理 objs 后 watch仍然写入数据到objs中
    if (ullist_->save_mode != SIS_UNLOCK_SAVE_NONE)
    {
        sis_unlock_mutex_lock(&ullist_->objslock);
        sis_pointer_list_destroy(ullist_->objs);
        sis_unlock_mutex_unlock(&ullist_->objslock);
    }
    sis_unlock_queue_destroy(ullist_->work_queue);
    sis_free(ullist_);
}
void sis_unlock_list_push(s_sis_unlock_list *ullist_, s_sis_object *obj_)
{
    sis_unlock_queue_push(ullist_->work_queue, obj_);
    // 通知数据
    if (ullist_->watcher)
    {
        sis_unlock_mutex_try_lock(&ullist_->watcher->notice);
    }
}
void sis_unlock_list_clear(s_sis_unlock_list *ullist_)
{
    if (ullist_->save_mode == SIS_UNLOCK_SAVE_NONE)
    {
        return ;
    }
    while (ullist_->work_queue->rnums > 0)
    {
        s_sis_object *obj = sis_unlock_queue_pop(ullist_->work_queue);
        sis_object_decr(obj);
    }
    sis_unlock_mutex_lock(&ullist_->objslock);
    sis_pointer_list_clear(ullist_->objs);
    ullist_->cursize = 0;
    sis_unlock_mutex_unlock(&ullist_->objslock);
}

#endif

#if 0

int nums = 100;
int maxnums = 1000000;//*1000;
volatile int series = 0;
volatile int pops = 0;
int exit__ = 0; 

static void *thread_push(void *arg)
{
    s_sis_unlock_list *share = (s_sis_unlock_list *)arg;
    while(exit__ == 0)
    {
        sis_sleep(1);
    }
    // sis_sleep(300);
    // while(nums--)
    {
        while(series < maxnums)
        {
            series++;
            s_sis_object *obj = sis_object_create(SIS_OBJECT_INT, (void *)series);
            sis_unlock_list_push(share, obj); 
            // printf("队列插入元素：%d %d\n", series, share->work_queue->count);
            sis_object_destroy(obj);
            // sis_sleep(10);
        }
        // series = 0;
        // sis_sleep(3000);
    }
    return NULL;
}
int reader_id = 0;
int read_pops[10] ={0};

static int cb_recv(void *src, s_sis_object *obj)
{
    s_sis_unlock_reader *reader = (s_sis_unlock_reader *)src;
    ADDF(&pops, 1);
    ADDF(&read_pops[reader->zeromsec], 1);
    // printf("队列输出元素：------÷ %s : %d ||  %d\n", reader->sign, (int)obj->ptr, ADDF(&pops, 1));
    // sis_sleep(1);
    return 0;
}
s_sis_unlock_queue *read_queue[10];
static void *thread_pop(void *arg)
{
    int index= (int)arg;
    s_sis_unlock_queue *reader = read_queue[index];
    read_pops[index] = 0;
    while (read_pops[index] < maxnums)
    {
        s_sis_object *obj = sis_unlock_queue_pop(reader);        
        while (obj)
        {
            // pops++;
            // read_pops[index]++;
            ADDF(&pops, 1);
            ADDF(&read_pops[index], 1);
            sis_object_decr(obj);
            obj = sis_unlock_queue_pop(reader); 
        }
        sis_sleep(1);
    }  
    return NULL;  
}
int main1()
{
    // 测试 s_sis_unlock_mutex 的cpu占用
    // s_sis_unlock_mutex   objslock = 0;
    // while (sis_unlock_mutex_try_unlock(&objslock))
    // {
    //     sis_sleep(1);
    // } 
    // 测试 sis_unlock_node_create 的耗时
    msec_t start = sis_time_get_now_msec();

    for (int i = 0; i < 1000000; i++)
    {
        s_sis_memory *imem = sis_memory_create_size(4); // 耗时 150 - 200
        sis_memory_cat_byte(imem, i, 4);
        s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, imem); // 耗时 60 - 90
        s_sis_unlock_node *new_node = sis_unlock_node_create(obj);  // 耗时 70 - 90
        sis_object_decr(obj);
    }
    // 整体耗时 // 285 - 355 毫秒
    printf("cost %llu us\n", sis_time_get_now_msec() - start);
    
    // 测试 ADDF SUBF 的耗时
    {
        msec_t start = sis_time_get_now_msec();
        int number = 0;
        for (int i = 0; i < 1000000; i++)
        {
            ADDF(&number, 1);
            SUBF(&number, 1);
        }
        printf("cost %llu us\n", sis_time_get_now_msec() - start);
    }
    return 0;
}
int main()
{
    safe_memory_start();

    // { // test queue
    //     s_sis_unlock_queue *queue = sis_unlock_queue_create();
    //     msec_t start = sis_time_get_now_msec();
    //     int inums = 0;
    //     int onums = 0;
    //     while(inums < maxnums)
    //     {
    //         inums++;
    //         s_sis_object *obj = sis_object_create(SIS_OBJECT_INT, (void *)inums);
    //         sis_unlock_queue_push(queue, obj); 
    //         // printf("队列插入元素：%d %d\n", inums, queue->count);
    //         sis_object_destroy(obj);  
    //     }
    //     printf("cost %llu us\n", sis_time_get_now_msec() - start);
    //     while(onums < maxnums)
    //     {
    //         s_sis_object *obj = sis_unlock_queue_pop(queue);        
    //         if (obj)
    //         {
    //             onums++;
    //             // printf("队列输出元素：------ %d ||  %d\n", (int)(obj->ptr), ADDF(&onums, 1));
    //             sis_object_destroy(obj); 
    //         }
    //     }
    //     sis_unlock_queue_destroy(queue);
    //     printf("cost %llu us\n", sis_time_get_now_msec() - start);
    // }
    { // test list
        s_sis_unlock_queue *input = sis_unlock_queue_create();
        int inums = 0;
        int reader = 5;
        pops = 0;
        pthread_t   read[10];
        for (int i = 0; i < reader; i++)
        {
            read_queue[i] = sis_unlock_queue_create();
            pthread_create(&read[i],NULL,thread_pop,(void*)i);
        }
        msec_t start = sis_time_get_now_msec();
        while(inums < maxnums)
        {
            inums++;
            s_sis_object *obj = sis_object_create(SIS_OBJECT_INT, (void *)inums);
            sis_unlock_queue_push(input, obj); 
            // printf("队列插入元素：%d %d\n", inums, input->count);
            sis_object_destroy(obj);  
        }   
        printf("cost 1 %llu us\n", sis_time_get_now_msec() - start);         
        s_sis_object *obj = sis_unlock_queue_pop(input);        
        while (obj)
        {
            for (int i = 0; i < reader; i++)
            {
                sis_unlock_queue_push(read_queue[i], obj);
            }
            sis_object_destroy(obj); 
            obj = sis_unlock_queue_pop(input); 
        }
        printf("cost 2 %llu us\n", sis_time_get_now_msec() - start);
        for(int i = 0; i < reader; i++)
        {
            printf("out 1 [%d] = %d -- %d + %d\n", i, read_pops[i], read_queue[i]->rnums, read_queue[i]->wnums);
        }
        
        for(int i = 0; i < reader; i++)
        {
            pthread_join(read[i], NULL);
            printf("out 2 [%d] = %d -- %d + %d\n", i, read_pops[i], read_queue[i]->rnums, read_queue[i]->wnums);
        }
        // for (int i = 0; i < reader; i++)
        // {
        //     s_sis_object *readerobj = sis_unlock_queue_pop(read_queue[i]);
        //     while (readerobj)
        //     {
        //         ADDF(&pops, 1);
        //         sis_object_destroy(readerobj); 
        //         readerobj = sis_unlock_queue_pop(read_queue[i]);
        //     }           
        // }
        printf("cost 3 %llu us %d\n", sis_time_get_now_msec() - start, pops);
        for (int i = 0; i < reader; i++)
        {
            sis_unlock_queue_destroy(read_queue[i]);
        }
        printf("cost 4 %llu us\n", sis_time_get_now_msec() - start);
        sis_unlock_queue_destroy(input);
        printf("cost 5 %llu us\n", sis_time_get_now_msec() - start);
    }
    { // test thread & list
        s_sis_unlock_list *sharedb = sis_unlock_list_create(0);

        pthread_t            write[10];
        s_sis_unlock_reader *read[10];

        int reader = 5;
        int writer = 1;
        // int reader = 10;
        // int writer = 5;
        pops = 0;

        //创建两个线程
        for(int i = 0;i < reader; i++)
        {
            read_pops[i] = 0;
            read[i] = sis_unlock_reader_create(sharedb, SIS_UNLOCK_READER_HEAD, NULL, cb_recv, NULL);
            // read[i] = sis_unlock_reader_create(sharedb, SIS_UNLOCK_READER_TAIL, NULL, cb_recv, NULL);
            read[i]->zeromsec = i;
            sis_unlock_reader_open(read[i]);
            printf("read %d sendsave = %d\n", sharedb->users->count, read[i]->sendsave);
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
            printf("save %d  sendsave = %d %d count= %d\n", 
                read[i]->work_queue->rnums + read[i]->work_queue->wnums, 
                read[i]->sendsave, read_pops[i], sharedb->objs->count);
            sis_unlock_reader_close(read[i]);
            printf("recv %d : %d\n", i, read_pops[i]);
        }
        // 不延时居然会有内存不释放 ??? 奇怪了
        // sis_sleep(100);
        // printf("sharedb->work_queue %d \n", sharedb->work_queue->count);
        printf("cost pop %llu us , %d ==> %d\n", sis_time_get_now_msec() - start, series, pops);  
        sis_unlock_list_destroy(sharedb);
        printf("cost %llu us , %d ==> %d\n", sis_time_get_now_msec() - start, series, pops);  

    }

    safe_memory_stop();
	return 1;
}
#endif


