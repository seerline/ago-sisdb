
#include "sis_list.def.h"
#include "sis_list.lock.h"

#ifdef MAKE_LOCK_LIST

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

s_sis_unlock_queue *sis_unlock_queue_create()
{
    s_sis_unlock_queue *o = SIS_MALLOC(s_sis_unlock_queue, o);
    o->head = sis_unlock_node_create(NULL);
    o->tail = o->head;
    o->count = 0;
    sis_mutex_init(&o->lock, NULL);
    return  o;
}
void sis_unlock_queue_destroy(s_sis_unlock_queue *queue_)
{
    while (queue_->count > 0)
    {
        s_sis_object *obj = sis_unlock_queue_pop(queue_);
        sis_object_decr(obj);
    }
    sis_unlock_node_destroy(queue_->tail);
    sis_free(queue_);
}

void sis_unlock_queue_push(s_sis_unlock_queue *queue_, s_sis_object *obj_)
{  
    s_sis_unlock_node *new_node = sis_unlock_node_create(obj_);
    sis_mutex_lock(&queue_->lock);
    queue_->tail->next = new_node;
    queue_->tail = new_node;
    queue_->count++;
    // ADDF(&(queue_->count), 1);
    sis_mutex_unlock(&queue_->lock);
}
// 返回的数据 需要 sis_object_decr 释放 
s_sis_object *sis_unlock_queue_pop(s_sis_unlock_queue *queue_)
{  
    s_sis_object *obj = NULL;
    s_sis_unlock_node *head = NULL;

    sis_mutex_lock(&queue_->lock);
    head = queue_->head;
    if (head->next)
    {
        obj = head->next->obj;
        queue_->head = head->next;
        sis_unlock_node_destroy(head);
    }
    if (obj)
    {
        queue_->count--;  // cost 438 us
        // SUBF(&(queue_->count), 1); // cost 2978 us
    }
    sis_mutex_unlock(&queue_->lock);
    return obj;
}

/////////////////////////////////////////////////
//  s_sis_unlock_reader
/////////////////////////////////////////////////

void *_thread_reader(void *argv_)
{
    s_sis_unlock_reader *reader = (s_sis_unlock_reader *)argv_;

    s_sis_wait *wait = sis_wait_get(reader->notice_wait);

    sis_thread_wait_start(wait);

    bool surpass_waittime = false;
    int waitmsec = 3000;
    if (reader->work_mode & SIS_UNLOCK_READER_ZERO)
    {
        waitmsec = reader->zeromsec;
    }
    while (reader->work_status != SIS_UNLOCK_STATUS_EXIT)
    {
        if (reader->work_status == SIS_UNLOCK_STATUS_WORK)
        {
            s_sis_object *obj = sis_unlock_queue_pop(reader->work_queue);
            while(obj)
            {
                if (reader->cb_recv)
                {
                    reader->cb_recv(reader->cb_source, obj);
                }
                reader->cursor = obj;
                sis_object_decr(obj);
                obj = sis_unlock_queue_pop(reader->work_queue);
            }
            if(!obj)
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
                if (surpass_waittime && (reader->work_mode & SIS_UNLOCK_READER_ZERO))
                {
                    reader->cb_recv(reader->cb_source, NULL);
                }
            }
        }
        // 为了写入数据立即返回 把定时放到后面
        // 这里的save_wait比较特殊,数据满也会即时处理
        surpass_waittime = false;
        // printf("reader ready. %p\n", reader);
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

    s_sis_wait *wait = sis_wait_get(reader->notice_wait);

    sis_thread_wait_start(wait);

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
        if (sis_thread_wait_sleep_msec(wait, 3000) != SIS_ETIMEDOUT)
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
    // if (other->work_mode & SIS_UNLOCK_READER_WAIT)
    // {
    //     o->isrealtime = 0;
    // }

    o->notice_wait = sis_wait_malloc();

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
        sis_thread_wait_notice(sis_wait_get(reader->notice_wait));
        while (reader->work_status != SIS_UNLOCK_STATUS_NONE)
        {
            sis_sleep(10);
        }
    }
    sis_wait_free(reader->notice_wait);

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
void _unlock_reader_sendsave(s_sis_unlock_reader *reader_)
{
    if (reader_->work_mode & SIS_UNLOCK_READER_HEAD)
    {
        s_sis_unlock_list *ullist = (s_sis_unlock_list *)reader_->father;
        sis_mutex_lock(&ullist->objslock);
        if (reader_->sendsave < ullist->objs->count)
        {
            for (int k = reader_->sendsave; k < ullist->objs->count; k++)
            {
                sis_unlock_queue_push(reader_->work_queue, (s_sis_object *)sis_pointer_list_get(ullist->objs, k));
            }
            reader_->sendsave = ullist->objs->count;
        }
        sis_mutex_unlock(&ullist->objslock);
    }
}

bool sis_unlock_reader_open(s_sis_unlock_reader *reader_)
{
    // 先处理历史数据到读者队列中
    _unlock_reader_sendsave(reader_);
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
    sis_mutex_lock(&ullist->userslock);
    sis_pointer_list_push(ullist->users, reader_);
    sis_mutex_unlock(&ullist->userslock);

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
    sis_mutex_lock(&ullist->userslock);
    int index = sis_pointer_list_indexof(ullist->users, reader_);
    sis_pointer_list_delete(ullist->users, index , 1);
    sis_mutex_unlock(&ullist->userslock);
}   

/////////////////////////////////////////////////
//  s_sis_unlock_list
/////////////////////////////////////////////////
static void *_thread_watcher(void *argv_)
{
    s_sis_unlock_reader *reader = (s_sis_unlock_reader *)argv_;
    s_sis_unlock_list *ullist = (s_sis_unlock_list *)reader->father;

    s_sis_wait *wait = sis_wait_get(reader->notice_wait);
    sis_thread_wait_start(wait);
    
    while (reader->work_status != SIS_UNLOCK_STATUS_EXIT)
    {
        if (reader->work_status == SIS_UNLOCK_STATUS_WORK)
        {
            s_sis_object *obj = sis_unlock_queue_pop(ullist->work_queue);
            // 把得到的数据传送到读者的队列中
            while (obj)
            {
                sis_mutex_lock(&ullist->userslock);
                for (int i = 0; i < ullist->users->count; i++)
                {
                    s_sis_unlock_reader *other = (s_sis_unlock_reader *)sis_pointer_list_get(ullist->users, i);
                    // 这里需要再次处理历史数据 否则可能会漏掉数据
                    _unlock_reader_sendsave(other);
                    other->sendsave++;
                    sis_unlock_queue_push(other->work_queue, obj);
                    // 通知数据
                    sis_thread_wait_notice(sis_wait_get(other->notice_wait));
                }
                sis_mutex_unlock(&ullist->userslock);
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
bool _unlock_list_find_user(s_sis_unlock_list *ullist, s_sis_object *obj)
{
    sis_mutex_lock(&ullist->userslock);
    for (int i = 0; i < ullist->users->count; i++)
    {
        s_sis_unlock_reader *other = (s_sis_unlock_reader *)sis_pointer_list_get(ullist->users, i);
        if (other->cursor == obj)
        {
            return true;
        }
    }
    sis_mutex_unlock(&ullist->userslock); 
    return false;   
}
void _unlock_list_sub_sendsave(s_sis_unlock_list *ullist)
{
    sis_mutex_lock(&ullist->userslock);
    for (int i = 0; i < ullist->users->count; i++)
    {
        s_sis_unlock_reader *other = (s_sis_unlock_reader *)sis_pointer_list_get(ullist->users, i);
        other->sendsave--;
    }
    sis_mutex_unlock(&ullist->userslock); 
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
    sis_mutex_lock(&ullist->objslock);
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
    sis_mutex_unlock(&ullist->objslock);
    return 1;
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
    }
    switch (o->save_mode)
    {
    case SIS_UNLOCK_SAVE_ALL:
    case SIS_UNLOCK_SAVE_SIZE:
        sis_mutex_init(&o->objslock, NULL);
        o->objs = sis_pointer_list_create();
        o->objs->vfree = sis_object_decr;
        o->cursize = 0;
        o->maxsize = maxsize_;
        break;    
    default:
        break;
    }

    sis_mutex_init(&o->userslock, NULL);
    o->users = sis_pointer_list_create();
    o->users->vfree = sis_unlock_reader_destroy;

    o->watcher = sis_unlock_reader_create(o, SIS_UNLOCK_READER_HEAD, NULL, cb_watcher_recv, NULL);
    o->watcher->work_status = SIS_UNLOCK_STATUS_WORK;
    if (!sis_thread_create(_thread_watcher, o->watcher, &o->watcher->work_thread))
    {
        LOG(1)("can't start watcher thread.\n");
        sis_unlock_list_destroy(o);
        return NULL;
    }
    return o;
}

void sis_unlock_list_destroy(s_sis_unlock_list *ullist_)
{
    sis_mutex_lock(&ullist_->userslock);
    sis_pointer_list_destroy(ullist_->users);
    sis_mutex_unlock(&ullist_->userslock);
    // sis_mutex_destroy(&ullist_->userslock);
    sis_unlock_reader_destroy(ullist_->watcher);
    if (ullist_->save_mode != SIS_UNLOCK_SAVE_NONE)
    {
        sis_mutex_lock(&ullist_->objslock);
        sis_pointer_list_destroy(ullist_->objs);
        sis_mutex_unlock(&ullist_->objslock);
        // sis_mutex_rw_destroy(&ullist_->objslock);
    }
    sis_unlock_queue_destroy(ullist_->work_queue);
    sis_free(ullist_);
}
void sis_unlock_list_push(s_sis_unlock_list *ullist_, s_sis_object *obj_)
{
    sis_unlock_queue_push(ullist_->work_queue, obj_);
    // 通知数据
    sis_thread_wait_notice(sis_wait_get(ullist_->watcher->notice_wait));
}
void sis_unlock_list_clear(s_sis_unlock_list *ullist_)
{
    if (ullist_->save_mode == SIS_UNLOCK_SAVE_NONE)
    {
        return ;
    }
    while (ullist_->work_queue->count)
    {
        s_sis_object *obj = sis_unlock_queue_pop(ullist_->work_queue);
        sis_object_decr(obj);
    }
    sis_mutex_lock(&ullist_->objslock);
    sis_pointer_list_clear(ullist_->objs);
    ullist_->cursize = 0;
    sis_mutex_unlock(&ullist_->objslock);
}

#if 0

int nums = 100;
int maxnums = 1000*1000;
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
            s_sis_memory *mem = sis_memory_create();
            sis_memory_cat_byte(mem, series, 4);
            s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, mem);
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
    s_sis_memory *mem = SIS_OBJ_MEMORY(obj);
    int32 *data = (int32 *)sis_memory(mem);
    s_sis_unlock_reader *reader = (s_sis_unlock_reader *)src;
    ADDF(&pops, 1);
    ADDF(&read_pops[reader->zeromsec], 1);
    // printf("队列输出元素：------÷ %s : %d ||  %d\n", reader->sign, *data, ADDF(&pops, 1));
    // sis_sleep(1);
    return 0;
}

int main()
{
    safe_memory_start();

    { // test queue
        s_sis_unlock_queue *queue = sis_unlock_queue_create();
        msec_t start = sis_time_get_now_msec();
        int inums = 0;
        int onums = 0;
        while(inums < maxnums)
        {
            inums++;
            s_sis_memory *imem = sis_memory_create();
            sis_memory_cat_byte(imem, inums, 4);
            s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, imem);
            sis_unlock_queue_push(queue, obj); 
            // // printf("队列插入元素：%d %d\n", inums, queue->count);
            sis_object_destroy(obj);  
            obj = sis_unlock_queue_pop(queue);        
            if (obj)
            {
                s_sis_memory *omem = SIS_OBJ_MEMORY(obj);
                int32 *data = (int32 *)sis_memory(omem);
                // printf("队列输出元素：------ %d ||  %d\n", *data, ADDF(&onums, 1));
                sis_object_destroy(obj); 
            }
        }
        sis_unlock_queue_destroy(queue);
        printf("cost %llu us\n", sis_time_get_now_msec() - start);
    }
    {
        s_sis_unlock_list *sharedb = sis_unlock_list_create(0);

        pthread_t            write[10];
        s_sis_unlock_reader *read[10];

        int reader = 5;
        int writer = 1;
        // int reader = 10;
        // int writer = 5;

        //创建两个线程
         for(int i = 0;i < reader; i++)
        {
            read[i] = sis_unlock_reader_create(sharedb, SIS_UNLOCK_READER_HEAD, NULL, cb_recv, NULL);
            // read[i] = sis_unlock_reader_create(sharedb, SIS_UNLOCK_READER_TAIL, NULL, cb_recv, NULL);
            read[i]->zeromsec = i;
            sis_unlock_reader_open(read[i]);
            printf("read %d sendsave = %d\n", sharedb->users->count, read[i]->sendsave);
        }
        // 所有写和读都建设好再开始发送数据
        msec_t start = sis_time_get_now_msec();
        exit__ = 1;
        // sis_sleep(100);
        // printf("read %d \n", sharedb->users->count);
        for(int i = 0;i < writer; i++)
        {
            pthread_create(&write[i],NULL,thread_push,(void*)sharedb);
        }

        // sis_sleep(100);
        for(int i = 0; i < writer; i++)
        {
            pthread_join(write[i], NULL);
        }
        for(int i = 0; i < reader; i++)
        {
            // 这里需要等所有线程收到完整消息        
            while (read_pops[i] < series)  
            {
                sis_sleep(1);
            }
            printf("save %d  sendsave = %d count= %d\n", read[i]->work_queue->count, read[i]->sendsave, sharedb->objs->count);
            sis_unlock_reader_close(read[i]);
            printf("recv %d : %d\n", i, read_pops[i]);
        }
        // 不延时居然会有内存不释放 ??? 奇怪了
        // sis_sleep(100);
        // printf("sharedb->work_queue %d \n", sharedb->work_queue->count);
        sis_unlock_list_destroy(sharedb);

        printf("cost %llu us , %d ==> %d\n", sis_time_get_now_msec() - start, series, pops);
    
    }

    safe_memory_stop();
	return 1;
}
#endif

#endif
