
#include "sis_queue.h"

// type可以是1,2,4或8字节长度的int类型，即：
// int8_t / uint8_t
// int16_t / uint16_t
// int32_t / uint32_t
// int64_t / uint64_t

// 返回更新前的值（先fetch再计算）
// type __sync_fetch_and_add (type *ptr, type value, ...)
// type __sync_fetch_and_sub (type *ptr, type value, ...)
// type __sync_fetch_and_or (type *ptr, type value, ...)
// type __sync_fetch_and_and (type *ptr, type value, ...)
// type __sync_fetch_and_xor (type *ptr, type value, ...)
// type __sync_fetch_and_nand (type *ptr, type value, ...)
// 返回更新后的值（先计算再fetch）
// type __sync_add_and_fetch (type *ptr, type value, ...)
// type __sync_sub_and_fetch (type *ptr, type value, ...)
// type __sync_or_and_fetch (type *ptr, type value, ...)
// type __sync_and_and_fetch (type *ptr, type value, ...)
// type __sync_xor_and_fetch (type *ptr, type value, ...)
// type __sync_nand_and_fetch (type *ptr, type value, ...)
// 后面的可扩展参数(...)用来指出哪些变量需要memory barrier,因为目前gcc实现的是full barrier（
// 类似于linux kernel 中的mb(),表示这个操作之前的所有内存操作不会被重排序到这个操作之后）,所以可以略掉这个参数。


// 两个函数提供原子的比较和交换，如果*ptr == oldval,就将newval写入*ptr,
// 第一个函数在相等并写入的情况下返回true.
// 第二个函数在返回操作之前的值。
// bool __sync_bool_compare_and_swap (type *ptr, type oldval type newval, ...)
// type __sync_val_compare_and_swap (type *ptr, type oldval type newval, ...)

// 将*ptr设为value并返回*ptr操作之前的值。
// type __sync_lock_test_and_set (type *ptr, type value, ...)
// 将*ptr置0
// void __sync_lock_release (type *ptr, ...)

/////////////////////////////////////////////////
//  s_sis_queue
/////////////////////////////////////////////////

s_sis_queue *sis_queue_create()
{
    s_sis_queue *o = SIS_MALLOC(s_sis_queue, o);
    s_sis_queue_node *node = SIS_MALLOC(s_sis_queue_node, node);
    node->value = NULL;
    o->head = node;
    o->tail = node;
    o->count = 0;
    sis_mutex_init(&(o->headlock), NULL);
    sis_mutex_init(&(o->taillock), NULL);
    return  o;
}
void sis_queue_destroy(s_sis_queue *queue_)
{
    while (queue_->count > 0)
    {
        sis_queue_pop(queue_);
    }
    sis_free(queue_->tail);
    sis_free(queue_);
}

int   sis_queue_push(s_sis_queue *queue_, void *value_)
{
    s_sis_queue_node *node = SIS_MALLOC(s_sis_queue_node, node);
    node->value = value_;
    sis_mutex_lock(&(queue_->taillock));
    queue_->tail->next = node;
    queue_->tail = node;
    queue_->count++;
    sis_mutex_unlock(&(queue_->taillock));
    return queue_->count;
}
void *sis_queue_pop(s_sis_queue *queue_)
{
    sis_mutex_lock(&(queue_->headlock));
    s_sis_queue_node *head = queue_->head;
    s_sis_queue_node *new_head = head->next;
    if (!new_head)
    {
        sis_mutex_unlock(&(queue_->headlock));
        return NULL;
    }
    void *value = new_head->value;
    queue_->head = new_head;
    queue_->count--;
    sis_mutex_unlock(&(queue_->headlock));
    sis_free(head);
    return value;
}

/////////////////////////////////////////////////
//  s_sis_share_node
/////////////////////////////////////////////////

s_sis_share_node *sis_share_node_create(s_sis_object *value_)
{
    s_sis_share_node *node = SIS_MALLOC(s_sis_share_node, node);
    if (value_)
    {
        sis_object_incr(value_);
        node->value = value_;
    }
    node->next = NULL;
    return node;    
}
// void sis_share_node_inc(s_sis_share_node *node_)
// {
//     FAA(&(node_->cites), 1); 
// }
// void _sis_share_node_del(s_sis_share_node *node_)
// {  
//     if (node_->value)
//     {
//         sis_object_decr(node_->value);
//     }
//     sis_free(node_);
// }
// void sis_share_node_dec(s_sis_share_node *node_)
// {
//     FAS(&(node_->cites), 1);
//     if (node_->cites == 0)
//     {
//         _sis_share_node_del(node_);
//     }
// }

void sis_share_node_destroy(s_sis_share_node *node_)
{
    if (node_->value)
    {
        sis_object_decr(node_->value);
    }
    sis_free(node_);
} 

/////////////////////////////////////////////////
//  s_sis_share_reader
/////////////////////////////////////////////////

void *_thread_reader(void *argv_)
{
    s_sis_share_reader *reader = (s_sis_share_reader *)argv_;
    s_sis_share_list *share = reader->slist;

    reader->notice_wait = sis_wait_malloc();
    s_sis_wait *wait = sis_wait_get(reader->notice_wait);

    sis_thread_wait_start(wait);

    while (reader->work_status != SIS_SHARE_STATUS_EXIT)
    {
        // printf("reader start.....\n");
        // 这里的save_wait比较特殊,数据满也会即时处理
        // sis_thread_wait_notice(wait); // 通知立即处理
        if (sis_thread_wait_sleep_msec(wait, 1000) != SIS_ETIMEDOUT)
        {
            if (reader->work_status == SIS_SHARE_STATUS_EXIT)
            {
                break;
            }
            // printf("reader notice.\n");
        }
        if (reader->work_status != SIS_SHARE_STATUS_WORK)
        {
            continue;
        }

        // 不管什么情况这里都会读取数据
        if (reader->cursor == NULL)
        {
            // 第一次 得到开始的位置
            if (reader->from == SIS_SHARE_FROM_HEAD)
            {
                s_sis_share_node *node = share->head->next;
                if (node && reader->cb)
                {
                    reader->cb(reader->cbobj, node->value);
                }
                reader->cursor = node;
            }
            else
            {
                reader->cursor = share->tail;
            }
        }
        if (reader->cursor)
        {
            while(reader->cursor->next)   
            {
                s_sis_share_node *node = reader->cursor->next;
                if (reader->cb)
                {
                    reader->cb(reader->cbobj, node->value);
                }
                reader->cursor = node;
            } 
            if (reader->from == SIS_SHARE_FROM_HEAD && !reader->istail)
            {
                // 只发一次
                if (reader->cb_eof)
                {
                    reader->cb_eof(reader->cbobj);
                }
                reader->istail = true;
            }
        }
    }
    sis_thread_wait_stop(wait);
    sis_wait_free(reader->notice_wait);
    sis_thread_finish(&reader->work_thread);
    reader->work_status = SIS_SHARE_STATUS_NONE;
    return NULL;
}
s_sis_share_reader *sis_share_reader_create(
    s_sis_share_list *slist_, sis_share_from from_, void *cbobj_, cb_sis_share_reader *cb_)
{
    s_sis_share_reader *o = SIS_MALLOC(s_sis_share_reader, o);
    sis_str_get_time_id(o->sign, 16);
    o->slist = slist_;
    o->cbobj = cbobj_;
    o->cb = cb_;
    
    o->cursor = NULL;
    // 如果从头读 第一次读到队尾发布一条空数据 后面不再发
    o->from = from_;
    o->istail = false;

    o->work_status = SIS_SHARE_STATUS_NONE;
    return o;
}
void sis_share_reader_destroy(void *reader_)
{
    s_sis_share_reader *reader = (s_sis_share_reader *)reader_;
    if (reader->work_status != SIS_SHARE_STATUS_NONE)
    {
        reader->work_status = SIS_SHARE_STATUS_EXIT;
        // 通知退出
        sis_thread_wait_notice(sis_wait_get(reader->notice_wait));
        while (reader->work_status != SIS_SHARE_STATUS_NONE)
        {
            sis_sleep(10);
        }
    }
    sis_free(reader); 
}
bool sis_share_reader_work(s_sis_share_reader *reader_, sis_share_reader_working *work_thread_)
{
    reader_->work_status = SIS_SHARE_STATUS_WORK;
    if (!sis_thread_create(work_thread_, reader_, &reader_->work_thread))
    {
        LOG(1)("can't start reader thread.\n");
        return false;
    }
    return true;
}
/////////////////////////////////////////////////
//  s_sis_share_list
/////////////////////////////////////////////////
s_sis_share_node *sis_share_list_pop(s_sis_share_list *cls_);

// void *_thread_watcher(void *argv_)
// {
//     s_sis_share_reader *reader = (s_sis_share_reader *)argv_;
//     s_sis_share_list *share = reader->slist;

//     reader->notice_wait = sis_wait_malloc();
//     s_sis_wait *wait = sis_wait_get(reader->notice_wait);

//     sis_thread_wait_start(wait);
//     while (reader->work_status != SIS_SHARE_STATUS_EXIT)
//     {
//         // 这里的save_wait比较特殊,数据满也会即时处理
//         // sis_thread_wait_notice(wait); // 通知立即处理
//         if (sis_thread_wait_sleep_msec(wait, 3000) != SIS_ETIMEDOUT)
//         {
//             if (reader->work_status == SIS_SHARE_STATUS_EXIT)
//             {
//                 break;
//             }
//             // 是被通知进来的 所有这里向所有wait通知
//         }
        
//         if (reader->work_status != SIS_SHARE_STATUS_WORK)
//         {
//             continue;
//         }
//         // 不管什么情况这里都会读取数据
//         // 求出share->cursize 如果大于 maxsize 就

//         if (reader->cursor == NULL)
//         {
//             // 第一次 得到开始的位置
//             s_sis_share_node *node = share->head->next;
//             if (node)
//             {
//                 share->cursize = SIS_OBJ_GET_SIZE(node->value);
//                 reader->cursor = node;
//             }
//         }
//         if (reader->cursor)
//         {
//             while(reader->cursor->next)   
//             {
//                 s_sis_share_node *node = reader->cursor->next;
//                 if (node)
//                 {
//                     share->cursize += SIS_OBJ_GET_SIZE(node->value);
//                     reader->cursor = node;
//                 }
//             } 
//         }
//         // 大于尺寸
//         while(share->cursize > share->maxsize && share->maxsize > 0)
//         {
//             s_sis_share_node *node = sis_share_list_pop(share);
//             if (node)
//             {
//                 // printf("node= %zu  %zu\n", SIS_OBJ_GET_SIZE(node->value), share->cursize);
//                 share->cursize -= SIS_OBJ_GET_SIZE(node->value);
//             }
//             else
//             {
//                 // printf("node= %p\n", node);
//                 break;
//             }
            
//         }
//     }
//     sis_thread_wait_stop(wait);
//     sis_wait_free(reader->notice_wait);
//     sis_thread_finish(&reader->work_thread);
//     reader->work_status = SIS_SHARE_STATUS_NONE;
//     return NULL;
// }

void *_thread_watcher(void *argv_)
{
    s_sis_share_reader *reader = (s_sis_share_reader *)argv_;
    s_sis_share_list *share = reader->slist;

    reader->notice_wait = sis_wait_malloc();
    s_sis_wait *wait = sis_wait_get(reader->notice_wait);

    sis_thread_wait_start(wait);
    while (reader->work_status != SIS_SHARE_STATUS_EXIT)
    {
        // 这里的save_wait比较特殊,数据满也会即时处理
        // sis_thread_wait_notice(wait); // 通知立即处理
        if (sis_thread_wait_sleep_msec(wait, 3000) != SIS_ETIMEDOUT)
        {
            if (reader->work_status == SIS_SHARE_STATUS_EXIT)
            {
                break;
            }
            // 是被通知进来的 所有这里向所有wait通知
        }
        
        if (reader->work_status != SIS_SHARE_STATUS_WORK)
        {
            continue;
        }
        // 不管什么情况这里都会读取数据
        // 只要没有被使用就清除

        if (reader->cursor == NULL)
        {
            // 第一次 得到开始的位置
            s_sis_share_node *node = share->head->next;
            if (node)
            {
                reader->cursor = node;
            }
        }
        if (reader->cursor)
        {
            while(reader->cursor->next)   
            {
                bool isuse = false;
                for (int i = 0; i < share->reader->count; i++)
                {
                    s_sis_share_reader *other = (s_sis_share_reader *)sis_pointer_list_get(share->reader, i);
                    if (other->cursor == reader->cursor)
                    {
                        isuse = true;
                    }
                }
                if (isuse)
                {
                    break;
                }
                reader->cursor = reader->cursor->next;
                sis_share_list_pop(share);
            } 
        }
    }
    sis_thread_wait_stop(wait);
    sis_wait_free(reader->notice_wait);
    sis_thread_finish(&reader->work_thread);
    reader->work_status = SIS_SHARE_STATUS_NONE;
    return NULL;
}
s_sis_share_list *sis_share_list_create(const char *key_, size_t limit_)
{
    s_sis_share_list *o = SIS_MALLOC(s_sis_share_list, o);
    // 先创建线程
    o->cursize = 0;
    o->maxsize = limit_;

    if (o->maxsize > 0)
    {
        // 不限制数据大小
        o->watcher = sis_share_reader_create(o, 0, NULL, NULL);
        if (!sis_share_reader_work(o->watcher, _thread_watcher))
        {
            sis_share_reader_destroy(o->watcher);
            sis_free(o);
            return NULL;
        }
        o->watcher->work_status = SIS_SHARE_STATUS_WORK;
    }

    s_sis_share_node *node = sis_share_node_create(NULL);

    o->head = node;
    o->tail = node;
    sis_mutex_init(&(o->headlock), NULL);
    sis_mutex_init(&(o->taillock), NULL);
    o->count = 0;

    o->name = sis_sdsnew(key_);

    o->reader = sis_pointer_list_create();
    o->reader->vfree = sis_share_reader_destroy;

    return o;
}

void sis_share_list_destroy(s_sis_share_list *obj_)
{
    sis_pointer_list_destroy(obj_->reader);
    if (obj_->maxsize > 0)
    {
        sis_share_reader_destroy(obj_->watcher);
    }

    sis_sdsfree(obj_->name);
    while (obj_->count > 0)
    {
        sis_share_list_pop(obj_);
    }
    sis_share_node_destroy(obj_->tail);
    sis_free(obj_);
}

int  sis_share_list_push(s_sis_share_list *cls_, s_sis_object *value_)
{
    s_sis_share_node *node = sis_share_node_create(value_);
    sis_mutex_lock(&(cls_->taillock));
    cls_->tail->next = node;
    cls_->tail = node;
    // FAA(&(cls_->count), 1);
    cls_->count++;
    sis_mutex_unlock(&(cls_->taillock));
    for (int i = 0; i < cls_->reader->count; i++)
    {
        s_sis_share_reader *v = (s_sis_share_reader *)sis_pointer_list_get(cls_->reader , i);
        sis_thread_wait_notice(sis_wait_get(v->notice_wait));
    }            
    return cls_->count;   
}
s_sis_share_node *sis_share_list_pop(s_sis_share_list *cls_) 
{
    sis_mutex_lock(&(cls_->headlock));
    s_sis_share_node *head = cls_->head;
    s_sis_share_node *new_head = head->next;
    if (!new_head)
    {
        sis_mutex_unlock(&(cls_->headlock));
        return NULL;
    }
    s_sis_share_node *node = new_head;
    cls_->head = new_head;
    // FAS(&(cls_->count), 1);
    cls_->count--;
    sis_mutex_unlock(&(cls_->headlock));
    sis_share_node_destroy(head);
    return node;
}
// 写入缓存数据 读取是依靠get按索引获取
int  sis_share_list_catch_set(s_sis_share_list *cls_, char *key_, s_sis_object *value_)
{
    s_sis_share_node *node = sis_share_node_create(value_);
    return sis_map_list_set(cls_->catch_infos, key_, node);
}

s_sis_object *sis_share_list_catch_get(s_sis_share_list *cls_, char *key_)
{
    s_sis_share_node *node = (s_sis_share_node *)sis_map_list_get(cls_->catch_infos, key_);
    if (node)
    {
        return node->value;
    }
    return NULL;
} 

s_sis_object *sis_share_list_catch_geti(s_sis_share_list *cls_, int index_)
{
    s_sis_share_node *node = (s_sis_share_node *)sis_map_list_geti(cls_->catch_infos, index_);
    if (node)
    {
        return node->value;
    }
    return NULL;
} 

int sis_share_list_catch_size(s_sis_share_list *cls_)
{
    return sis_map_list_getsize(cls_->catch_infos);
} 

void *_thread_user_reader(void *argv_)
{
    s_sis_share_reader *reader = (s_sis_share_reader *)argv_;
    s_sis_share_list *share = reader->slist;

    reader->notice_wait = sis_wait_malloc();
    s_sis_wait *wait = sis_wait_get(reader->notice_wait);

    sis_thread_wait_start(wait);

    while (reader->work_status != SIS_SHARE_STATUS_EXIT)
    {
        // printf("reader start.....\n");
        // 这里的save_wait比较特殊,数据满也会即时处理
        // sis_thread_wait_notice(wait); // 通知立即处理
        if (sis_thread_wait_sleep_msec(wait, 1000) != SIS_ETIMEDOUT)
        {
            if (reader->work_status == SIS_SHARE_STATUS_EXIT)
            {
                break;
            }
            // printf("reader notice.\n");
        }
        if (reader->work_status != SIS_SHARE_STATUS_WORK)
        {
            continue;
        }
        // 不管什么情况这里都会读取数据
        if (reader->cursor == NULL)
        {
            // 第一次 得到开始的位置
            if (reader->from == SIS_SHARE_FROM_HEAD)
            {
                s_sis_share_node *node = share->head->next;
                if (node && reader->cb)
                {
                    reader->work_status = SIS_SHARE_STATUS_WAIT;
                    reader->cb(reader->cbobj, node->value);
                }
                reader->cursor = node;
            }
            else
            {
                reader->cursor = share->tail;
            }
            continue;
        }
        else
        {
            if (reader->cursor->next)
            {
                s_sis_share_node *node = reader->cursor->next;
                if (reader->cb)
                {
                    reader->work_status = SIS_SHARE_STATUS_WAIT;
                    reader->cb(reader->cbobj, node->value);
                }
                reader->cursor = node;
            }
            continue;
        }
    }
    sis_thread_wait_stop(wait);
    sis_wait_free(reader->notice_wait);
    sis_thread_finish(&reader->work_thread);
    reader->work_status = SIS_SHARE_STATUS_NONE;
    return NULL;
}
s_sis_share_reader *sis_share_reader_open(s_sis_share_list *cls_, void *cbobj_, cb_sis_share_reader *cb_)
{
    s_sis_share_reader *reader = sis_share_reader_create(cls_, SIS_SHARE_FROM_HEAD, cbobj_, cb_);

    if (!sis_share_reader_work(reader, _thread_user_reader))
    {
        sis_share_reader_destroy(reader);
        return NULL;
    }
    sis_pointer_list_push(cls_->reader, reader);
    reader->work_status = SIS_SHARE_STATUS_WORK;

    return reader;
}
// 有新消息就先回调 然后由用户控制读下一条信息 next返回为空时重新激活线程的执行 
s_sis_object *sis_share_reader_next(s_sis_share_reader *reader_)
{
    if (reader_->work_status != SIS_SHARE_STATUS_WAIT)
    {
        return NULL;
    }
    s_sis_object *o = NULL;
    s_sis_share_node *node = reader_->cursor->next;
    if (node)
    {
        // s_sis_share_node *node = reader_->cursor->next;
        o = node->value;
        if (reader_->cb)
        {
            reader_->cb(reader_->cbobj, node->value);
        }
        reader_->cursor = node;
    }
    if (!o)  
    {  
        // 无数据就恢复正常
        reader_->work_status = SIS_SHARE_STATUS_WORK;
    }
    return o;
}
void sis_share_reader_close(s_sis_share_list *cls_, s_sis_share_reader *reader_)
{
    int index = sis_pointer_list_indexof(cls_->reader, reader_);
    sis_pointer_list_delete(cls_->reader, index , 1);
}   

s_sis_share_reader *sis_share_reader_login(s_sis_share_list *cls_, 
    sis_share_from from_, void *cbobj_, cb_sis_share_reader *cb_)
{
    s_sis_share_reader *reader = sis_share_reader_create(cls_, from_, cbobj_, cb_);

    if (!sis_share_reader_work(reader, _thread_reader))
    {
        sis_share_reader_destroy(reader);
        return NULL;
    }
    sis_pointer_list_push(cls_->reader, reader);
    reader->work_status = SIS_SHARE_STATUS_WORK;

    return reader;
}

void sis_share_reader_logout(s_sis_share_list *cls_, 
    s_sis_share_reader *reader_)
{
    int index = sis_pointer_list_indexof(cls_->reader, reader_);
    sis_pointer_list_delete(cls_->reader, index , 1);
}


#if 0

int nums = 20000*1000;
volatile int series = 10;
volatile int pops = 10;
int exit__ = 0;

void *thread_push(void *arg)
{
    s_sis_share_list *share = (s_sis_share_list *)arg;
    while(series < nums)
    {
        series++;
        int data = series;
        sis_share_list_push(share, &data,  sizeof(int), NULL); 
        printf("队列插入元素：%d %zu\n",data, share->count);
        sis_sleep(10);
    }
    return NULL;
}
int cb_recv(void *src, void *val, size_t vlen)
{
    int *data = (int *)val;
    pops++;
    s_sis_share_reader *reader = (s_sis_share_reader *)src;
    printf("队列输出元素：------ %s : %d %zu ||  %d\n", reader->sign, *data, ((s_sis_share_list *)reader->father)->count, pops);
    return 0;
}
void *thread_pop(void *arg)
{

    s_sis_share_list *share = (s_sis_share_list *)arg;
    s_sis_share_reader *reader = sis_share_reader_login(share, 0, NULL, cb_recv);
    reader->cbobj = reader;
    while(!exit__)
    {
        sis_sleep(300);
    }
    sis_share_reader_logout(share, reader);
    return NULL;
}

int main()
{
    safe_memory_start();

	pthread_t write[10];
    pthread_t read[10];

    struct timeval tv_begin, tv_end;
    gettimeofday(&tv_begin, NULL);

    s_sis_share_list *sharedb = sis_share_list_create("test", 1000*4);

    int reader = 5;
    int writer = 3;
    // int reader = 10;
    // int writer = 5;

    //创建两个线程
    for(int i = 0;i < reader; i++)
    {
        pthread_create(&read[i], NULL, thread_pop, (void*)sharedb);
    }
    for(int i = 0;i < writer; i++)
    {
	    pthread_create(&write[i],NULL,thread_push,(void*)sharedb);
    }
    for(int i = 0; i < writer; i++)
    {
		pthread_join(write[i], NULL);
    }
    exit__ = 1;
    for(int i = 0; i < reader; i++)
    {
        pthread_join(read[i], NULL);
    }

    sis_share_list_destroy(sharedb);

    gettimeofday(&tv_end, NULL);
    size_t timeinterval = (tv_end.tv_sec - tv_begin.tv_sec) * 1000000 + (tv_end.tv_usec - tv_begin.tv_usec);
    printf("cost %zu us\n", timeinterval);

    safe_memory_stop();
	return 1;
}
#endif
// #ifdef __CYGWIN__
// #define strtold(a,b) ((long double)strtod((a),(b)))
// #endif

/* ===================== Creation and parsing of objects ==================== */

// robj *createObject(int type, void *ptr) {
//     robj *o = zmalloc(sizeof(*o));
//     o->type = type;
//     o->encoding = OBJ_ENCODING_RAW;
//     o->ptr = ptr;
//     o->refcount = 1;

//     /* Set the LRU to the current lruclock (minutes resolution), or
//      * alternatively the LFU counter. */
//     if (server.maxmemory_policy & MAXMEMORY_FLAG_LFU) {
//         o->lru = (LFUGetTimeInMinutes()<<8) | LFU_INIT_VAL;
//     } else {
//         o->lru = LRU_CLOCK();
//     }
//     return o;
// }

// /* Set a special refcount in the object to make it "shared":
//  * incrRefCount and decrRefCount() will test for this special refcount
//  * and will not touch the object. This way it is free to access shared
//  * objects such as small integers from different threads without any
//  * mutex.
//  *
//  * A common patter to create shared objects:
//  *
//  * robj *myobject = makeObjectShared(createObject(...));
//  *
//  */
// robj *makeObjectShared(robj *o) {
//     serverAssert(o->refcount == 1);
//     o->refcount = OBJ_SHARED_REFCOUNT;
//     return o;
// }

// /* Create a string object with encoding OBJ_ENCODING_RAW, that is a plain
//  * string object where o->ptr points to a proper sds string. */
// robj *createRawStringObject(const char *ptr, size_t len) {
//     return createObject(OBJ_STRING, sdsnewlen(ptr,len));
// }

// /* Create a string object with encoding OBJ_ENCODING_EMBSTR, that is
//  * an object where the sds string is actually an unmodifiable string
//  * allocated in the same chunk as the object itself. */
// robj *createEmbeddedStringObject(const char *ptr, size_t len) {
//     robj *o = zmalloc(sizeof(robj)+sizeof(struct sdshdr8)+len+1);
//     struct sdshdr8 *sh = (void*)(o+1);

//     o->type = OBJ_STRING;
//     o->encoding = OBJ_ENCODING_EMBSTR;
//     o->ptr = sh+1;
//     o->refcount = 1;
//     if (server.maxmemory_policy & MAXMEMORY_FLAG_LFU) {
//         o->lru = (LFUGetTimeInMinutes()<<8) | LFU_INIT_VAL;
//     } else {
//         o->lru = LRU_CLOCK();
//     }

//     sh->len = len;
//     sh->alloc = len;
//     sh->flags = SDS_TYPE_8;
//     if (ptr == SDS_NOINIT)
//         sh->buf[len] = '\0';
//     else if (ptr) {
//         memcpy(sh->buf,ptr,len);
//         sh->buf[len] = '\0';
//     } else {
//         memset(sh->buf,0,len+1);
//     }
//     return o;
// }

// /* Create a string object with EMBSTR encoding if it is smaller than
//  * OBJ_ENCODING_EMBSTR_SIZE_LIMIT, otherwise the RAW encoding is
//  * used.
//  *
//  * The current limit of 44 is chosen so that the biggest string object
//  * we allocate as EMBSTR will still fit into the 64 byte arena of jemalloc. */
// #define OBJ_ENCODING_EMBSTR_SIZE_LIMIT 44
// robj *createStringObject(const char *ptr, size_t len) {
//     if (len <= OBJ_ENCODING_EMBSTR_SIZE_LIMIT)
//         return createEmbeddedStringObject(ptr,len);
//     else
//         return createRawStringObject(ptr,len);
// }

// robj *createStringObjectFromLongLong(long long value) {
//     robj *o;
//     if (value >= 0 && value < OBJ_SHARED_INTEGERS) {
//         incrRefCount(shared.integers[value]);
//         o = shared.integers[value];
//     } else {
//         if (value >= LONG_MIN && value <= LONG_MAX) {
//             o = createObject(OBJ_STRING, NULL);
//             o->encoding = OBJ_ENCODING_INT;
//             o->ptr = (void*)((long)value);
//         } else {
//             o = createObject(OBJ_STRING,sdsfromlonglong(value));
//         }
//     }
//     return o;
// }

// /* Create a string object from a long double. If humanfriendly is non-zero
//  * it does not use exponential format and trims trailing zeroes at the end,
//  * however this results in loss of precision. Otherwise exp format is used
//  * and the output of snprintf() is not modified.
//  *
//  * The 'humanfriendly' option is used for INCRBYFLOAT and HINCRBYFLOAT. */
// robj *createStringObjectFromLongDouble(long double value, int humanfriendly) {
//     char buf[MAX_LONG_DOUBLE_CHARS];
//     int len = ld2string(buf,sizeof(buf),value,humanfriendly);
//     return createStringObject(buf,len);
// }

// /* Duplicate a string object, with the guarantee that the returned object
//  * has the same encoding as the original one.
//  *
//  * This function also guarantees that duplicating a small integere object
//  * (or a string object that contains a representation of a small integer)
//  * will always result in a fresh object that is unshared (refcount == 1).
//  *
//  * The resulting object always has refcount set to 1. */
// robj *dupStringObject(const robj *o) {
//     robj *d;

//     serverAssert(o->type == OBJ_STRING);

//     switch(o->encoding) {
//     case OBJ_ENCODING_RAW:
//         return createRawStringObject(o->ptr,sdslen(o->ptr));
//     case OBJ_ENCODING_EMBSTR:
//         return createEmbeddedStringObject(o->ptr,sdslen(o->ptr));
//     case OBJ_ENCODING_INT:
//         d = createObject(OBJ_STRING, NULL);
//         d->encoding = OBJ_ENCODING_INT;
//         d->ptr = o->ptr;
//         return d;
//     default:
//         serverPanic("Wrong encoding.");
//         break;
//     }
// }

// robj *createQuicklistObject(void) {
//     quicklist *l = quicklistCreate();
//     robj *o = createObject(OBJ_LIST,l);
//     o->encoding = OBJ_ENCODING_QUICKLIST;
//     return o;
// }

// robj *createZiplistObject(void) {
//     unsigned char *zl = ziplistNew();
//     robj *o = createObject(OBJ_LIST,zl);
//     o->encoding = OBJ_ENCODING_ZIPLIST;
//     return o;
// }

// robj *createSetObject(void) {
//     dict *d = dictCreate(&setDictType,NULL);
//     robj *o = createObject(OBJ_SET,d);
//     o->encoding = OBJ_ENCODING_HT;
//     return o;
// }

// robj *createIntsetObject(void) {
//     intset *is = intsetNew();
//     robj *o = createObject(OBJ_SET,is);
//     o->encoding = OBJ_ENCODING_INTSET;
//     return o;
// }

// robj *createHashObject(void) {
//     unsigned char *zl = ziplistNew();
//     robj *o = createObject(OBJ_HASH, zl);
//     o->encoding = OBJ_ENCODING_ZIPLIST;
//     return o;
// }

// robj *createZsetObject(void) {
//     zset *zs = zmalloc(sizeof(*zs));
//     robj *o;

//     zs->dict = dictCreate(&zsetDictType,NULL);
//     zs->zsl = zslCreate();
//     o = createObject(OBJ_ZSET,zs);
//     o->encoding = OBJ_ENCODING_SKIPLIST;
//     return o;
// }

// robj *createZsetZiplistObject(void) {
//     unsigned char *zl = ziplistNew();
//     robj *o = createObject(OBJ_ZSET,zl);
//     o->encoding = OBJ_ENCODING_ZIPLIST;
//     return o;
// }

// robj *createStreamObject(void) {
//     stream *s = streamNew();
//     robj *o = createObject(OBJ_STREAM,s);
//     o->encoding = OBJ_ENCODING_STREAM;
//     return o;
// }

// robj *createModuleObject(moduleType *mt, void *value) {
//     moduleValue *mv = zmalloc(sizeof(*mv));
//     mv->type = mt;
//     mv->value = value;
//     return createObject(OBJ_MODULE,mv);
// }

// void freeStringObject(robj *o) {
//     if (o->encoding == OBJ_ENCODING_RAW) {
//         sdsfree(o->ptr);
//     }
// }

// void freeListObject(robj *o) {
//     if (o->encoding == OBJ_ENCODING_QUICKLIST) {
//         quicklistRelease(o->ptr);
//     } else {
//         serverPanic("Unknown list encoding type");
//     }
// }

// void freeSetObject(robj *o) {
//     switch (o->encoding) {
//     case OBJ_ENCODING_HT:
//         dictRelease((dict*) o->ptr);
//         break;
//     case OBJ_ENCODING_INTSET:
//         zfree(o->ptr);
//         break;
//     default:
//         serverPanic("Unknown set encoding type");
//     }
// }

// void freeZsetObject(robj *o) {
//     zset *zs;
//     switch (o->encoding) {
//     case OBJ_ENCODING_SKIPLIST:
//         zs = o->ptr;
//         dictRelease(zs->dict);
//         zslFree(zs->zsl);
//         zfree(zs);
//         break;
//     case OBJ_ENCODING_ZIPLIST:
//         zfree(o->ptr);
//         break;
//     default:
//         serverPanic("Unknown sorted set encoding");
//     }
// }

// void freeHashObject(robj *o) {
//     switch (o->encoding) {
//     case OBJ_ENCODING_HT:
//         dictRelease((dict*) o->ptr);
//         break;
//     case OBJ_ENCODING_ZIPLIST:
//         zfree(o->ptr);
//         break;
//     default:
//         serverPanic("Unknown hash encoding type");
//         break;
//     }
// }

// void freeModuleObject(robj *o) {
//     moduleValue *mv = o->ptr;
//     mv->type->free(mv->value);
//     zfree(mv);
// }

// void freeStreamObject(robj *o) {
//     freeStream(o->ptr);
// }

// void incrRefCount(robj *o) {
//     if (o->refcount != OBJ_SHARED_REFCOUNT) o->refcount++;
// }

// void decrRefCount(robj *o) {
//     if (o->refcount == 1) {
//         switch(o->type) {
//         case OBJ_STRING: freeStringObject(o); break;
//         case OBJ_LIST: freeListObject(o); break;
//         case OBJ_SET: freeSetObject(o); break;
//         case OBJ_ZSET: freeZsetObject(o); break;
//         case OBJ_HASH: freeHashObject(o); break;
//         case OBJ_MODULE: freeModuleObject(o); break;
//         case OBJ_STREAM: freeStreamObject(o); break;
//         default: serverPanic("Unknown object type"); break;
//         }
//         zfree(o);
//     } else {
//         if (o->refcount <= 0) serverPanic("decrRefCount against refcount <= 0");
//         if (o->refcount != OBJ_SHARED_REFCOUNT) o->refcount--;
//     }
// }

// /* This variant of decrRefCount() gets its argument as void, and is useful
//  * as free method in data structures that expect a 'void free_object(void*)'
//  * prototype for the free method. */
// void decrRefCountVoid(void *o) {
//     decrRefCount(o);
// }

// /* This function set the ref count to zero without freeing the object.
//  * It is useful in order to pass a new object to functions incrementing
//  * the ref count of the received object. Example:
//  *
//  *    functionThatWillIncrementRefCount(resetRefCount(CreateObject(...)));
//  *
//  * Otherwise you need to resort to the less elegant pattern:
//  *
//  *    *obj = createObject(...);
//  *    functionThatWillIncrementRefCount(obj);
//  *    decrRefCount(obj);
//  */
// robj *resetRefCount(robj *obj) {
//     obj->refcount = 0;
//     return obj;
// }

// int checkType(client *c, robj *o, int type) {
//     if (o->type != type) {
//         addReply(c,shared.wrongtypeerr);
//         return 1;
//     }
//     return 0;
// }

// int isSdsRepresentableAsLongLong(sds s, long long *llval) {
//     return string2ll(s,sdslen(s),llval) ? C_OK : C_ERR;
// }

// int isObjectRepresentableAsLongLong(robj *o, long long *llval) {
//     serverAssertWithInfo(NULL,o,o->type == OBJ_STRING);
//     if (o->encoding == OBJ_ENCODING_INT) {
//         if (llval) *llval = (long) o->ptr;
//         return C_OK;
//     } else {
//         return isSdsRepresentableAsLongLong(o->ptr,llval);
//     }
// }

// /* Try to encode a string object in order to save space */
// robj *tryObjectEncoding(robj *o) {
//     long value;
//     sds s = o->ptr;
//     size_t len;

//     /* Make sure this is a string object, the only type we encode
//      * in this function. Other types use encoded memory efficient
//      * representations but are handled by the commands implementing
//      * the type. */
//     serverAssertWithInfo(NULL,o,o->type == OBJ_STRING);

//     /* We try some specialized encoding only for objects that are
//      * RAW or EMBSTR encoded, in other words objects that are still
//      * in represented by an actually array of chars. */
//     if (!sdsEncodedObject(o)) return o;

//     /* It's not safe to encode shared objects: shared objects can be shared
//      * everywhere in the "object space" of Redis and may end in places where
//      * they are not handled. We handle them only as values in the keyspace. */
//      if (o->refcount > 1) return o;

//     /* Check if we can represent this string as a long integer.
//      * Note that we are sure that a string larger than 20 chars is not
//      * representable as a 32 nor 64 bit integer. */
//     len = sdslen(s);
//     if (len <= 20 && string2l(s,len,&value)) {
//         /* This object is encodable as a long. Try to use a shared object.
//          * Note that we avoid using shared integers when maxmemory is used
//          * because every object needs to have a private LRU field for the LRU
//          * algorithm to work well. */
//         if ((server.maxmemory == 0 ||
//             !(server.maxmemory_policy & MAXMEMORY_FLAG_NO_SHARED_INTEGERS)) &&
//             value >= 0 &&
//             value < OBJ_SHARED_INTEGERS)
//         {
//             decrRefCount(o);
//             incrRefCount(shared.integers[value]);
//             return shared.integers[value];
//         } else {
//             if (o->encoding == OBJ_ENCODING_RAW) sdsfree(o->ptr);
//             o->encoding = OBJ_ENCODING_INT;
//             o->ptr = (void*) value;
//             return o;
//         }
//     }

//     /* If the string is small and is still RAW encoded,
//      * try the EMBSTR encoding which is more efficient.
//      * In this representation the object and the SDS string are allocated
//      * in the same chunk of memory to save space and cache misses. */
//     if (len <= OBJ_ENCODING_EMBSTR_SIZE_LIMIT) {
//         robj *emb;

//         if (o->encoding == OBJ_ENCODING_EMBSTR) return o;
//         emb = createEmbeddedStringObject(s,sdslen(s));
//         decrRefCount(o);
//         return emb;
//     }

//     /* We can't encode the object...
//      *
//      * Do the last try, and at least optimize the SDS string inside
//      * the string object to require little space, in case there
//      * is more than 10% of free space at the end of the SDS string.
//      *
//      * We do that only for relatively large strings as this branch
//      * is only entered if the length of the string is greater than
//      * OBJ_ENCODING_EMBSTR_SIZE_LIMIT. */
//     if (o->encoding == OBJ_ENCODING_RAW &&
//         sdsavail(s) > len/10)
//     {
//         o->ptr = sdsRemoveFreeSpace(o->ptr);
//     }

//     /* Return the original object. */
//     return o;
// }

// /* Get a decoded version of an encoded object (returned as a new object).
//  * If the object is already raw-encoded just increment the ref count. */
// robj *getDecodedObject(robj *o) {
//     robj *dec;

//     if (sdsEncodedObject(o)) {
//         incrRefCount(o);
//         return o;
//     }
//     if (o->type == OBJ_STRING && o->encoding == OBJ_ENCODING_INT) {
//         char buf[32];

//         ll2string(buf,32,(long)o->ptr);
//         dec = createStringObject(buf,strlen(buf));
//         return dec;
//     } else {
//         serverPanic("Unknown encoding type");
//     }
// }

// /* Compare two string objects via strcmp() or strcoll() depending on flags.
//  * Note that the objects may be integer-encoded. In such a case we
//  * use ll2string() to get a string representation of the numbers on the stack
//  * and compare the strings, it's much faster than calling getDecodedObject().
//  *
//  * Important note: when REDIS_COMPARE_BINARY is used a binary-safe comparison
//  * is used. */

// #define REDIS_COMPARE_BINARY (1<<0)
// #define REDIS_COMPARE_COLL (1<<1)

// int compareStringObjectsWithFlags(robj *a, robj *b, int flags) {
//     serverAssertWithInfo(NULL,a,a->type == OBJ_STRING && b->type == OBJ_STRING);
//     char bufa[128], bufb[128], *astr, *bstr;
//     size_t alen, blen, minlen;

//     if (a == b) return 0;
//     if (sdsEncodedObject(a)) {
//         astr = a->ptr;
//         alen = sdslen(astr);
//     } else {
//         alen = ll2string(bufa,sizeof(bufa),(long) a->ptr);
//         astr = bufa;
//     }
//     if (sdsEncodedObject(b)) {
//         bstr = b->ptr;
//         blen = sdslen(bstr);
//     } else {
//         blen = ll2string(bufb,sizeof(bufb),(long) b->ptr);
//         bstr = bufb;
//     }
//     if (flags & REDIS_COMPARE_COLL) {
//         return strcoll(astr,bstr);
//     } else {
//         int cmp;

//         minlen = (alen < blen) ? alen : blen;
//         cmp = memcmp(astr,bstr,minlen);
//         if (cmp == 0) return alen-blen;
//         return cmp;
//     }
// }

// /* Wrapper for compareStringObjectsWithFlags() using binary comparison. */
// int compareStringObjects(robj *a, robj *b) {
//     return compareStringObjectsWithFlags(a,b,REDIS_COMPARE_BINARY);
// }

// /* Wrapper for compareStringObjectsWithFlags() using collation. */
// int collateStringObjects(robj *a, robj *b) {
//     return compareStringObjectsWithFlags(a,b,REDIS_COMPARE_COLL);
// }

// /* Equal string objects return 1 if the two objects are the same from the
//  * point of view of a string comparison, otherwise 0 is returned. Note that
//  * this function is faster then checking for (compareStringObject(a,b) == 0)
//  * because it can perform some more optimization. */
// int equalStringObjects(robj *a, robj *b) {
//     if (a->encoding == OBJ_ENCODING_INT &&
//         b->encoding == OBJ_ENCODING_INT){
//         /* If both strings are integer encoded just check if the stored
//          * long is the same. */
//         return a->ptr == b->ptr;
//     } else {
//         return compareStringObjects(a,b) == 0;
//     }
// }

// size_t stringObjectLen(robj *o) {
//     serverAssertWithInfo(NULL,o,o->type == OBJ_STRING);
//     if (sdsEncodedObject(o)) {
//         return sdslen(o->ptr);
//     } else {
//         return sdigits10((long)o->ptr);
//     }
// }

// int getDoubleFromObject(const robj *o, double *target) {
//     double value;
//     char *eptr;

//     if (o == NULL) {
//         value = 0;
//     } else {
//         serverAssertWithInfo(NULL,o,o->type == OBJ_STRING);
//         if (sdsEncodedObject(o)) {
//             errno = 0;
//             value = strtod(o->ptr, &eptr);
//             if (sdslen(o->ptr) == 0 ||
//                 isspace(((const char*)o->ptr)[0]) ||
//                 (size_t)(eptr-(char*)o->ptr) != sdslen(o->ptr) ||
//                 (errno == ERANGE &&
//                     (value == HUGE_VAL || value == -HUGE_VAL || value == 0)) ||
//                 isnan(value))
//                 return C_ERR;
//         } else if (o->encoding == OBJ_ENCODING_INT) {
//             value = (long)o->ptr;
//         } else {
//             serverPanic("Unknown string encoding");
//         }
//     }
//     *target = value;
//     return C_OK;
// }

// int getDoubleFromObjectOrReply(client *c, robj *o, double *target, const char *msg) {
//     double value;
//     if (getDoubleFromObject(o, &value) != C_OK) {
//         if (msg != NULL) {
//             addReplyError(c,(char*)msg);
//         } else {
//             addReplyError(c,"value is not a valid float");
//         }
//         return C_ERR;
//     }
//     *target = value;
//     return C_OK;
// }

// int getLongDoubleFromObject(robj *o, long double *target) {
//     long double value;
//     char *eptr;

//     if (o == NULL) {
//         value = 0;
//     } else {
//         serverAssertWithInfo(NULL,o,o->type == OBJ_STRING);
//         if (sdsEncodedObject(o)) {
//             errno = 0;
//             value = strtold(o->ptr, &eptr);
//             if (sdslen(o->ptr) == 0 ||
//                 isspace(((const char*)o->ptr)[0]) ||
//                 (size_t)(eptr-(char*)o->ptr) != sdslen(o->ptr) ||
//                 (errno == ERANGE &&
//                     (value == HUGE_VAL || value == -HUGE_VAL || value == 0)) ||
//                 isnan(value))
//                 return C_ERR;
//         } else if (o->encoding == OBJ_ENCODING_INT) {
//             value = (long)o->ptr;
//         } else {
//             serverPanic("Unknown string encoding");
//         }
//     }
//     *target = value;
//     return C_OK;
// }

// int getLongDoubleFromObjectOrReply(client *c, robj *o, long double *target, const char *msg) {
//     long double value;
//     if (getLongDoubleFromObject(o, &value) != C_OK) {
//         if (msg != NULL) {
//             addReplyError(c,(char*)msg);
//         } else {
//             addReplyError(c,"value is not a valid float");
//         }
//         return C_ERR;
//     }
//     *target = value;
//     return C_OK;
// }

// int getLongLongFromObject(robj *o, long long *target) {
//     long long value;

//     if (o == NULL) {
//         value = 0;
//     } else {
//         serverAssertWithInfo(NULL,o,o->type == OBJ_STRING);
//         if (sdsEncodedObject(o)) {
//             if (string2ll(o->ptr,sdslen(o->ptr),&value) == 0) return C_ERR;
//         } else if (o->encoding == OBJ_ENCODING_INT) {
//             value = (long)o->ptr;
//         } else {
//             serverPanic("Unknown string encoding");
//         }
//     }
//     if (target) *target = value;
//     return C_OK;
// }

// int getLongLongFromObjectOrReply(client *c, robj *o, long long *target, const char *msg) {
//     long long value;
//     if (getLongLongFromObject(o, &value) != C_OK) {
//         if (msg != NULL) {
//             addReplyError(c,(char*)msg);
//         } else {
//             addReplyError(c,"value is not an integer or out of range");
//         }
//         return C_ERR;
//     }
//     *target = value;
//     return C_OK;
// }

// int getLongFromObjectOrReply(client *c, robj *o, long *target, const char *msg) {
//     long long value;

//     if (getLongLongFromObjectOrReply(c, o, &value, msg) != C_OK) return C_ERR;
//     if (value < LONG_MIN || value > LONG_MAX) {
//         if (msg != NULL) {
//             addReplyError(c,(char*)msg);
//         } else {
//             addReplyError(c,"value is out of range");
//         }
//         return C_ERR;
//     }
//     *target = value;
//     return C_OK;
// }

// char *strEncoding(int encoding) {
//     switch(encoding) {
//     case OBJ_ENCODING_RAW: return "raw";
//     case OBJ_ENCODING_INT: return "int";
//     case OBJ_ENCODING_HT: return "hashtable";
//     case OBJ_ENCODING_QUICKLIST: return "quicklist";
//     case OBJ_ENCODING_ZIPLIST: return "ziplist";
//     case OBJ_ENCODING_INTSET: return "intset";
//     case OBJ_ENCODING_SKIPLIST: return "skiplist";
//     case OBJ_ENCODING_EMBSTR: return "embstr";
//     default: return "unknown";
//     }
// }

// /* =========================== Memory introspection ========================= */


// /* This is an helper function with the goal of estimating the memory
//  * size of a radix tree that is used to store Stream IDs.
//  *
//  * Note: to guess the size of the radix tree is not trivial, so we
//  * approximate it considering 128 bytes of data overhead for each
//  * key (the ID), and then adding the number of bare nodes, plus some
//  * overhead due by the data and child pointers. This secret recipe
//  * was obtained by checking the average radix tree created by real
//  * workloads, and then adjusting the constants to get numbers that
//  * more or less match the real memory usage.
//  *
//  * Actually the number of nodes and keys may be different depending
//  * on the insertion speed and thus the ability of the radix tree
//  * to compress prefixes. */
// size_t streamRadixTreeMemoryUsage(rax *rax) {
//     size_t size;
//     size = rax->numele * sizeof(streamID);
//     size += rax->numnodes * sizeof(raxNode);
//     /* Add a fixed overhead due to the aux data pointer, children, ... */
//     size += rax->numnodes * sizeof(long)*30;
//     return size;
// }

// /* Returns the size in bytes consumed by the key's value in RAM.
//  * Note that the returned value is just an approximation, especially in the
//  * case of aggregated data types where only "sample_size" elements
//  * are checked and averaged to estimate the total size. */
// #define OBJ_COMPUTE_SIZE_DEF_SAMPLES 5 /* Default sample size. */
// size_t objectComputeSize(robj *o, size_t sample_size) {
//     sds ele, ele2;
//     dict *d;
//     dictIterator *di;
//     struct dictEntry *de;
//     size_t asize = 0, elesize = 0, samples = 0;

//     if (o->type == OBJ_STRING) {
//         if(o->encoding == OBJ_ENCODING_INT) {
//             asize = sizeof(*o);
//         } else if(o->encoding == OBJ_ENCODING_RAW) {
//             asize = sdsAllocSize(o->ptr)+sizeof(*o);
//         } else if(o->encoding == OBJ_ENCODING_EMBSTR) {
//             asize = sdslen(o->ptr)+2+sizeof(*o);
//         } else {
//             serverPanic("Unknown string encoding");
//         }
//     } else if (o->type == OBJ_LIST) {
//         if (o->encoding == OBJ_ENCODING_QUICKLIST) {
//             quicklist *ql = o->ptr;
//             quicklistNode *node = ql->head;
//             asize = sizeof(*o)+sizeof(quicklist);
//             do {
//                 elesize += sizeof(quicklistNode)+ziplistBlobLen(node->zl);
//                 samples++;
//             } while ((node = node->next) && samples < sample_size);
//             asize += (double)elesize/samples*ql->len;
//         } else if (o->encoding == OBJ_ENCODING_ZIPLIST) {
//             asize = sizeof(*o)+ziplistBlobLen(o->ptr);
//         } else {
//             serverPanic("Unknown list encoding");
//         }
//     } else if (o->type == OBJ_SET) {
//         if (o->encoding == OBJ_ENCODING_HT) {
//             d = o->ptr;
//             di = dictGetIterator(d);
//             asize = sizeof(*o)+sizeof(dict)+(sizeof(struct dictEntry*)*dictSlots(d));
//             while((de = dictNext(di)) != NULL && samples < sample_size) {
//                 ele = dictGetKey(de);
//                 elesize += sizeof(struct dictEntry) + sdsAllocSize(ele);
//                 samples++;
//             }
//             dictReleaseIterator(di);
//             if (samples) asize += (double)elesize/samples*dictSize(d);
//         } else if (o->encoding == OBJ_ENCODING_INTSET) {
//             intset *is = o->ptr;
//             asize = sizeof(*o)+sizeof(*is)+is->encoding*is->length;
//         } else {
//             serverPanic("Unknown set encoding");
//         }
//     } else if (o->type == OBJ_ZSET) {
//         if (o->encoding == OBJ_ENCODING_ZIPLIST) {
//             asize = sizeof(*o)+(ziplistBlobLen(o->ptr));
//         } else if (o->encoding == OBJ_ENCODING_SKIPLIST) {
//             d = ((zset*)o->ptr)->dict;
//             zskiplist *zsl = ((zset*)o->ptr)->zsl;
//             zskiplistNode *znode = zsl->header->level[0].forward;
//             asize = sizeof(*o)+sizeof(zset)+(sizeof(struct dictEntry*)*dictSlots(d));
//             while(znode != NULL && samples < sample_size) {
//                 elesize += sdsAllocSize(znode->ele);
//                 elesize += sizeof(struct dictEntry) + zmalloc_size(znode);
//                 samples++;
//                 znode = znode->level[0].forward;
//             }
//             if (samples) asize += (double)elesize/samples*dictSize(d);
//         } else {
//             serverPanic("Unknown sorted set encoding");
//         }
//     } else if (o->type == OBJ_HASH) {
//         if (o->encoding == OBJ_ENCODING_ZIPLIST) {
//             asize = sizeof(*o)+(ziplistBlobLen(o->ptr));
//         } else if (o->encoding == OBJ_ENCODING_HT) {
//             d = o->ptr;
//             di = dictGetIterator(d);
//             asize = sizeof(*o)+sizeof(dict)+(sizeof(struct dictEntry*)*dictSlots(d));
//             while((de = dictNext(di)) != NULL && samples < sample_size) {
//                 ele = dictGetKey(de);
//                 ele2 = dictGetVal(de);
//                 elesize += sdsAllocSize(ele) + sdsAllocSize(ele2);
//                 elesize += sizeof(struct dictEntry);
//                 samples++;
//             }
//             dictReleaseIterator(di);
//             if (samples) asize += (double)elesize/samples*dictSize(d);
//         } else {
//             serverPanic("Unknown hash encoding");
//         }
//     } else if (o->type == OBJ_STREAM) {
//         stream *s = o->ptr;
//         asize = sizeof(*o);
//         asize += streamRadixTreeMemoryUsage(s->rax);

//         /* Now we have to add the listpacks. The last listpack is often non
//          * complete, so we estimate the size of the first N listpacks, and
//          * use the average to compute the size of the first N-1 listpacks, and
//          * finally add the real size of the last node. */
//         raxIterator ri;
//         raxStart(&ri,s->rax);
//         raxSeek(&ri,"^",NULL,0);
//         size_t lpsize = 0, samples = 0;
//         while(samples < sample_size && raxNext(&ri)) {
//             unsigned char *lp = ri.data;
//             lpsize += lpBytes(lp);
//             samples++;
//         }
//         if (s->rax->numele <= samples) {
//             asize += lpsize;
//         } else {
//             if (samples) lpsize /= samples; /* Compute the average. */
//             asize += lpsize * (s->rax->numele-1);
//             /* No need to check if seek succeeded, we enter this branch only
//              * if there are a few elements in the radix tree. */
//             raxSeek(&ri,"$",NULL,0);
//             raxNext(&ri);
//             asize += lpBytes(ri.data);
//         }
//         raxStop(&ri);

//         /* Consumer groups also have a non trivial memory overhead if there
//          * are many consumers and many groups, let's count at least the
//          * overhead of the pending entries in the groups and consumers
//          * PELs. */
//         if (s->cgroups) {
//             raxStart(&ri,s->cgroups);
//             raxSeek(&ri,"^",NULL,0);
//             while(raxNext(&ri)) {
//                 streamCG *cg = ri.data;
//                 asize += sizeof(*cg);
//                 asize += streamRadixTreeMemoryUsage(cg->pel);
//                 asize += sizeof(streamNACK)*raxSize(cg->pel);

//                 /* For each consumer we also need to add the basic data
//                  * structures and the PEL memory usage. */
//                 raxIterator cri;
//                 raxStart(&cri,cg->consumers);
//                 while(raxNext(&cri)) {
//                     streamConsumer *consumer = cri.data;
//                     asize += sizeof(*consumer);
//                     asize += sdslen(consumer->name);
//                     asize += streamRadixTreeMemoryUsage(consumer->pel);
//                     /* Don't count NACKs again, they are shared with the
//                      * consumer group PEL. */
//                 }
//                 raxStop(&cri);
//             }
//             raxStop(&ri);
//         }
//     } else if (o->type == OBJ_MODULE) {
//         moduleValue *mv = o->ptr;
//         moduleType *mt = mv->type;
//         if (mt->mem_usage != NULL) {
//             asize = mt->mem_usage(mv->value);
//         } else {
//             asize = 0;
//         }
//     } else {
//         serverPanic("Unknown object type");
//     }
//     return asize;
// }

// /* Release data obtained with getMemoryOverheadData(). */
// void freeMemoryOverheadData(struct redisMemOverhead *mh) {
//     zfree(mh->db);
//     zfree(mh);
// }

// /* Return a struct redisMemOverhead filled with memory overhead
//  * information used for the MEMORY OVERHEAD and INFO command. The returned
//  * structure pointer should be freed calling freeMemoryOverheadData(). */
// struct redisMemOverhead *getMemoryOverheadData(void) {
//     int j;
//     size_t mem_total = 0;
//     size_t mem = 0;
//     size_t zmalloc_used = zmalloc_used_memory();
//     struct redisMemOverhead *mh = zcalloc(sizeof(*mh));

//     mh->total_allocated = zmalloc_used;
//     mh->startup_allocated = server.initial_memory_usage;
//     mh->peak_allocated = server.stat_peak_memory;
//     mh->total_frag =
//         (float)server.cron_malloc_stats.process_rss / server.cron_malloc_stats.zmalloc_used;
//     mh->total_frag_bytes =
//         server.cron_malloc_stats.process_rss - server.cron_malloc_stats.zmalloc_used;
//     mh->allocator_frag =
//         (float)server.cron_malloc_stats.allocator_active / server.cron_malloc_stats.allocator_allocated;
//     mh->allocator_frag_bytes =
//         server.cron_malloc_stats.allocator_active - server.cron_malloc_stats.allocator_allocated;
//     mh->allocator_rss =
//         (float)server.cron_malloc_stats.allocator_resident / server.cron_malloc_stats.allocator_active;
//     mh->allocator_rss_bytes =
//         server.cron_malloc_stats.allocator_resident - server.cron_malloc_stats.allocator_active;
//     mh->rss_extra =
//         (float)server.cron_malloc_stats.process_rss / server.cron_malloc_stats.allocator_resident;
//     mh->rss_extra_bytes =
//         server.cron_malloc_stats.process_rss - server.cron_malloc_stats.allocator_resident;

//     mem_total += server.initial_memory_usage;

//     mem = 0;
//     if (server.repl_backlog)
//         mem += zmalloc_size(server.repl_backlog);
//     mh->repl_backlog = mem;
//     mem_total += mem;

//     mem = 0;
//     if (listLength(server.slaves)) {
//         listIter li;
//         listNode *ln;

//         listRewind(server.slaves,&li);
//         while((ln = listNext(&li))) {
//             client *c = listNodeValue(ln);
//             mem += getClientOutputBufferMemoryUsage(c);
//             mem += sdsAllocSize(c->querybuf);
//             mem += sizeof(client);
//         }
//     }
//     mh->clients_slaves = mem;
//     mem_total+=mem;

//     mem = 0;
//     if (listLength(server.clients)) {
//         listIter li;
//         listNode *ln;

//         listRewind(server.clients,&li);
//         while((ln = listNext(&li))) {
//             client *c = listNodeValue(ln);
//             if (c->flags & CLIENT_SLAVE && !(c->flags & CLIENT_MONITOR))
//                 continue;
//             mem += getClientOutputBufferMemoryUsage(c);
//             mem += sdsAllocSize(c->querybuf);
//             mem += sizeof(client);
//         }
//     }
//     mh->clients_normal = mem;
//     mem_total+=mem;

//     mem = 0;
//     if (server.aof_state != AOF_OFF) {
//         mem += sdslen(server.aof_buf);
//         mem += aofRewriteBufferSize();
//     }
//     mh->aof_buffer = mem;
//     mem_total+=mem;

//     for (j = 0; j < server.dbnum; j++) {
//         redisDb *db = server.db+j;
//         long long keyscount = dictSize(db->dict);
//         if (keyscount==0) continue;

//         mh->total_keys += keyscount;
//         mh->db = zrealloc(mh->db,sizeof(mh->db[0])*(mh->num_dbs+1));
//         mh->db[mh->num_dbs].dbid = j;

//         mem = dictSize(db->dict) * sizeof(dictEntry) +
//               dictSlots(db->dict) * sizeof(dictEntry*) +
//               dictSize(db->dict) * sizeof(robj);
//         mh->db[mh->num_dbs].overhead_ht_main = mem;
//         mem_total+=mem;

//         mem = dictSize(db->expires) * sizeof(dictEntry) +
//               dictSlots(db->expires) * sizeof(dictEntry*);
//         mh->db[mh->num_dbs].overhead_ht_expires = mem;
//         mem_total+=mem;

//         mh->num_dbs++;
//     }

//     mh->overhead_total = mem_total;
//     mh->dataset = zmalloc_used - mem_total;
//     mh->peak_perc = (float)zmalloc_used*100/mh->peak_allocated;

//     /* Metrics computed after subtracting the startup memory from
//      * the total memory. */
//     size_t net_usage = 1;
//     if (zmalloc_used > mh->startup_allocated)
//         net_usage = zmalloc_used - mh->startup_allocated;
//     mh->dataset_perc = (float)mh->dataset*100/net_usage;
//     mh->bytes_per_key = mh->total_keys ? (net_usage / mh->total_keys) : 0;

//     return mh;
// }

// /* Helper for "MEMORY allocator-stats", used as a callback for the jemalloc
//  * stats output. */
// void inputCatSds(void *result, const char *str) {
//     /* result is actually a (sds *), so re-cast it here */
//     sds *info = (sds *)result;
//     *info = sdscat(*info, str);
// }

// /* This implements MEMORY DOCTOR. An human readable analysis of the Redis
//  * memory condition. */
// sds getMemoryDoctorReport(void) {
//     int empty = 0;          /* Instance is empty or almost empty. */
//     int big_peak = 0;       /* Memory peak is much larger than used mem. */
//     int high_frag = 0;      /* High fragmentation. */
//     int high_alloc_frag = 0;/* High allocator fragmentation. */
//     int high_proc_rss = 0;  /* High process rss overhead. */
//     int high_alloc_rss = 0; /* High rss overhead. */
//     int big_slave_buf = 0;  /* Slave buffers are too big. */
//     int big_client_buf = 0; /* Client buffers are too big. */
//     int num_reports = 0;
//     struct redisMemOverhead *mh = getMemoryOverheadData();

//     if (mh->total_allocated < (1024*1024*5)) {
//         empty = 1;
//         num_reports++;
//     } else {
//         /* Peak is > 150% of current used memory? */
//         if (((float)mh->peak_allocated / mh->total_allocated) > 1.5) {
//             big_peak = 1;
//             num_reports++;
//         }

//         /* Fragmentation is higher than 1.4 and 10MB ?*/
//         if (mh->total_frag > 1.4 && mh->total_frag_bytes > 10<<20) {
//             high_frag = 1;
//             num_reports++;
//         }

//         /* External fragmentation is higher than 1.1 and 10MB? */
//         if (mh->allocator_frag > 1.1 && mh->allocator_frag_bytes > 10<<20) {
//             high_alloc_frag = 1;
//             num_reports++;
//         }

//         /* Allocator fss is higher than 1.1 and 10MB ? */
//         if (mh->allocator_rss > 1.1 && mh->allocator_rss_bytes > 10<<20) {
//             high_alloc_rss = 1;
//             num_reports++;
//         }

//         /* Non-Allocator fss is higher than 1.1 and 10MB ? */
//         if (mh->rss_extra > 1.1 && mh->rss_extra_bytes > 10<<20) {
//             high_proc_rss = 1;
//             num_reports++;
//         }

//         /* Clients using more than 200k each average? */
//         long numslaves = listLength(server.slaves);
//         long numclients = listLength(server.clients)-numslaves;
//         if (mh->clients_normal / numclients > (1024*200)) {
//             big_client_buf = 1;
//             num_reports++;
//         }

//         /* Slaves using more than 10 MB each? */
//         if (numslaves > 0 && mh->clients_slaves / numslaves > (1024*1024*10)) {
//             big_slave_buf = 1;
//             num_reports++;
//         }
//     }

//     sds s;
//     if (num_reports == 0) {
//         s = sdsnew(
//         "Hi Sam, I can't find any memory issue in your instance. "
//         "I can only account for what occurs on this base.\n");
//     } else if (empty == 1) {
//         s = sdsnew(
//         "Hi Sam, this instance is empty or is using very little memory, "
//         "my issues detector can't be used in these conditions. "
//         "Please, leave for your mission on Earth and fill it with some data. "
//         "The new Sam and I will be back to our programming as soon as I "
//         "finished rebooting.\n");
//     } else {
//         s = sdsnew("Sam, I detected a few issues in this Redis instance memory implants:\n\n");
//         if (big_peak) {
//             s = sdscat(s," * Peak memory: In the past this instance used more than 150% the memory that is currently using. The allocator is normally not able to release memory after a peak, so you can expect to see a big fragmentation ratio, however this is actually harmless and is only due to the memory peak, and if the Redis instance Resident Set Size (RSS) is currently bigger than expected, the memory will be used as soon as you fill the Redis instance with more data. If the memory peak was only occasional and you want to try to reclaim memory, please try the MEMORY PURGE command, otherwise the only other option is to shutdown and restart the instance.\n\n");
//         }
//         if (high_frag) {
//             s = sdscatprintf(s," * High total RSS: This instance has a memory fragmentation and RSS overhead greater than 1.4 (this means that the Resident Set Size of the Redis process is much larger than the sum of the logical allocations Redis performed). This problem is usually due either to a large peak memory (check if there is a peak memory entry above in the report) or may result from a workload that causes the allocator to fragment memory a lot. If the problem is a large peak memory, then there is no issue. Otherwise, make sure you are using the Jemalloc allocator and not the default libc malloc. Note: The currently used allocator is \"%s\".\n\n", ZMALLOC_LIB);
//         }
//         if (high_alloc_frag) {
//             s = sdscatprintf(s," * High allocator fragmentation: This instance has an allocator external fragmentation greater than 1.1. This problem is usually due either to a large peak memory (check if there is a peak memory entry above in the report) or may result from a workload that causes the allocator to fragment memory a lot. You can try enabling 'activedefrag' config option.\n\n");
//         }
//         if (high_alloc_rss) {
//             s = sdscatprintf(s," * High allocator RSS overhead: This instance has an RSS memory overhead is greater than 1.1 (this means that the Resident Set Size of the allocator is much larger than the sum what the allocator actually holds). This problem is usually due to a large peak memory (check if there is a peak memory entry above in the report), you can try the MEMORY PURGE command to reclaim it.\n\n");
//         }
//         if (high_proc_rss) {
//             s = sdscatprintf(s," * High process RSS overhead: This instance has non-allocator RSS memory overhead is greater than 1.1 (this means that the Resident Set Size of the Redis process is much larger than the RSS the allocator holds). This problem may be due to LUA scripts or Modules.\n\n");
//         }
//         if (big_slave_buf) {
//             s = sdscat(s," * Big slave buffers: The slave output buffers in this instance are greater than 10MB for each slave (on average). This likely means that there is some slave instance that is struggling receiving data, either because it is too slow or because of networking issues. As a result, data piles on the master output buffers. Please try to identify what slave is not receiving data correctly and why. You can use the INFO output in order to check the slaves delays and the CLIENT LIST command to check the output buffers of each slave.\n\n");
//         }
//         if (big_client_buf) {
//             s = sdscat(s," * Big client buffers: The clients output buffers in this instance are greater than 200K per client (on average). This may result from different causes, like Pub/Sub clients subscribed to channels bot not receiving data fast enough, so that data piles on the Redis instance output buffer, or clients sending commands with large replies or very large sequences of commands in the same pipeline. Please use the CLIENT LIST command in order to investigate the issue if it causes problems in your instance, or to understand better why certain clients are using a big amount of memory.\n\n");
//         }
//         s = sdscat(s,"I'm here to keep you safe, Sam. I want to help you.\n");
//     }
//     freeMemoryOverheadData(mh);
//     return s;
// }

// /* ======================= The OBJECT and MEMORY commands =================== */

// /* This is a helper function for the OBJECT command. We need to lookup keys
//  * without any modification of LRU or other parameters. */
// robj *objectCommandLookup(client *c, robj *key) {
//     dictEntry *de;

//     if ((de = dictFind(c->db->dict,key->ptr)) == NULL) return NULL;
//     return (robj*) dictGetVal(de);
// }

// robj *objectCommandLookupOrReply(client *c, robj *key, robj *reply) {
//     robj *o = objectCommandLookup(c,key);

//     if (!o) addReply(c, reply);
//     return o;
// }

// /* Object command allows to inspect the internals of an Redis Object.
//  * Usage: OBJECT <refcount|encoding|idletime|freq> <key> */
// void objectCommand(client *c) {
//     robj *o;

//     if (c->argc == 2 && !strcasecmp(c->argv[1]->ptr,"help")) {
//         const char *help[] = {
// "ENCODING <key> -- Return the kind of internal representation used in order to store the value associated with a key.",
// "FREQ <key> -- Return the access frequency index of the key. The returned integer is proportional to the logarithm of the recent access frequency of the key.",
// "IDLETIME <key> -- Return the idle time of the key, that is the approximated number of seconds elapsed since the last access to the key.",
// "REFCOUNT <key> -- Return the number of references of the value associated with the specified key.",
// NULL
//         };
//         addReplyHelp(c, help);
//     } else if (!strcasecmp(c->argv[1]->ptr,"refcount") && c->argc == 3) {
//         if ((o = objectCommandLookupOrReply(c,c->argv[2],shared.nullbulk))
//                 == NULL) return;
//         addReplyLongLong(c,o->refcount);
//     } else if (!strcasecmp(c->argv[1]->ptr,"encoding") && c->argc == 3) {
//         if ((o = objectCommandLookupOrReply(c,c->argv[2],shared.nullbulk))
//                 == NULL) return;
//         addReplyBulkCString(c,strEncoding(o->encoding));
//     } else if (!strcasecmp(c->argv[1]->ptr,"idletime") && c->argc == 3) {
//         if ((o = objectCommandLookupOrReply(c,c->argv[2],shared.nullbulk))
//                 == NULL) return;
//         if (server.maxmemory_policy & MAXMEMORY_FLAG_LFU) {
//             addReplyError(c,"An LFU maxmemory policy is selected, idle time not tracked. Please note that when switching between policies at runtime LRU and LFU data will take some time to adjust.");
//             return;
//         }
//         addReplyLongLong(c,estimateObjectIdleTime(o)/1000);
//     } else if (!strcasecmp(c->argv[1]->ptr,"freq") && c->argc == 3) {
//         if ((o = objectCommandLookupOrReply(c,c->argv[2],shared.nullbulk))
//                 == NULL) return;
//         if (!(server.maxmemory_policy & MAXMEMORY_FLAG_LFU)) {
//             addReplyError(c,"An LFU maxmemory policy is not selected, access frequency not tracked. Please note that when switching between policies at runtime LRU and LFU data will take some time to adjust.");
//             return;
//         }
//         /* LFUDecrAndReturn should be called
//          * in case of the key has not been accessed for a long time,
//          * because we update the access time only
//          * when the key is read or overwritten. */
//         addReplyLongLong(c,LFUDecrAndReturn(o));
//     } else {
//         addReplyErrorFormat(c, "Unknown subcommand or wrong number of arguments for '%s'. Try OBJECT help", (char *)c->argv[1]->ptr);
//     }
// }

// /* The memory command will eventually be a complete interface for the
//  * memory introspection capabilities of Redis.
//  *
//  * Usage: MEMORY usage <key> */
// void memoryCommand(client *c) {
//     robj *o;

//     if (!strcasecmp(c->argv[1]->ptr,"usage") && c->argc >= 3) {
//         long long samples = OBJ_COMPUTE_SIZE_DEF_SAMPLES;
//         for (int j = 3; j < c->argc; j++) {
//             if (!strcasecmp(c->argv[j]->ptr,"samples") &&
//                 j+1 < c->argc)
//             {
//                 if (getLongLongFromObjectOrReply(c,c->argv[j+1],&samples,NULL)
//                      == C_ERR) return;
//                 if (samples < 0) {
//                     addReply(c,shared.syntaxerr);
//                     return;
//                 }
//                 if (samples == 0) samples = LLONG_MAX;;
//                 j++; /* skip option argument. */
//             } else {
//                 addReply(c,shared.syntaxerr);
//                 return;
//             }
//         }
//         if ((o = objectCommandLookupOrReply(c,c->argv[2],shared.nullbulk))
//                 == NULL) return;
//         size_t usage = objectComputeSize(o,samples);
//         usage += sdsAllocSize(c->argv[2]->ptr);
//         usage += sizeof(dictEntry);
//         addReplyLongLong(c,usage);
//     } else if (!strcasecmp(c->argv[1]->ptr,"stats") && c->argc == 2) {
//         struct redisMemOverhead *mh = getMemoryOverheadData();

//         addReplyMultiBulkLen(c,(24+mh->num_dbs)*2);

//         addReplyBulkCString(c,"peak.allocated");
//         addReplyLongLong(c,mh->peak_allocated);

//         addReplyBulkCString(c,"total.allocated");
//         addReplyLongLong(c,mh->total_allocated);

//         addReplyBulkCString(c,"startup.allocated");
//         addReplyLongLong(c,mh->startup_allocated);

//         addReplyBulkCString(c,"replication.backlog");
//         addReplyLongLong(c,mh->repl_backlog);

//         addReplyBulkCString(c,"clients.slaves");
//         addReplyLongLong(c,mh->clients_slaves);

//         addReplyBulkCString(c,"clients.normal");
//         addReplyLongLong(c,mh->clients_normal);

//         addReplyBulkCString(c,"aof.buffer");
//         addReplyLongLong(c,mh->aof_buffer);

//         for (size_t j = 0; j < mh->num_dbs; j++) {
//             char dbname[32];
//             snprintf(dbname,sizeof(dbname),"db.%zd",mh->db[j].dbid);
//             addReplyBulkCString(c,dbname);
//             addReplyMultiBulkLen(c,4);

//             addReplyBulkCString(c,"overhead.hashtable.main");
//             addReplyLongLong(c,mh->db[j].overhead_ht_main);

//             addReplyBulkCString(c,"overhead.hashtable.expires");
//             addReplyLongLong(c,mh->db[j].overhead_ht_expires);
//         }

//         addReplyBulkCString(c,"overhead.total");
//         addReplyLongLong(c,mh->overhead_total);

//         addReplyBulkCString(c,"keys.count");
//         addReplyLongLong(c,mh->total_keys);

//         addReplyBulkCString(c,"keys.bytes-per-key");
//         addReplyLongLong(c,mh->bytes_per_key);

//         addReplyBulkCString(c,"dataset.bytes");
//         addReplyLongLong(c,mh->dataset);

//         addReplyBulkCString(c,"dataset.percentage");
//         addReplyDouble(c,mh->dataset_perc);

//         addReplyBulkCString(c,"peak.percentage");
//         addReplyDouble(c,mh->peak_perc);

//         addReplyBulkCString(c,"allocator.allocated");
//         addReplyLongLong(c,server.cron_malloc_stats.allocator_allocated);

//         addReplyBulkCString(c,"allocator.active");
//         addReplyLongLong(c,server.cron_malloc_stats.allocator_active);

//         addReplyBulkCString(c,"allocator.resident");
//         addReplyLongLong(c,server.cron_malloc_stats.allocator_resident);

//         addReplyBulkCString(c,"allocator-fragmentation.ratio");
//         addReplyDouble(c,mh->allocator_frag);

//         addReplyBulkCString(c,"allocator-fragmentation.bytes");
//         addReplyLongLong(c,mh->allocator_frag_bytes);

//         addReplyBulkCString(c,"allocator-rss.ratio");
//         addReplyDouble(c,mh->allocator_rss);

//         addReplyBulkCString(c,"allocator-rss.bytes");
//         addReplyLongLong(c,mh->allocator_rss_bytes);

//         addReplyBulkCString(c,"rss-overhead.ratio");
//         addReplyDouble(c,mh->rss_extra);

//         addReplyBulkCString(c,"rss-overhead.bytes");
//         addReplyLongLong(c,mh->rss_extra_bytes);

//         addReplyBulkCString(c,"fragmentation"); /* this is the total RSS overhead, including fragmentation */
//         addReplyDouble(c,mh->total_frag); /* it is kept here for backwards compatibility */

//         addReplyBulkCString(c,"fragmentation.bytes");
//         addReplyLongLong(c,mh->total_frag_bytes);

//         freeMemoryOverheadData(mh);
//     } else if (!strcasecmp(c->argv[1]->ptr,"malloc-stats") && c->argc == 2) {
// #if defined(USE_JEMALLOC)
//         sds info = sdsempty();
//         je_malloc_stats_print(inputCatSds, &info, NULL);
//         addReplyBulkSds(c, info);
// #else
//         addReplyBulkCString(c,"Stats not supported for the current allocator");
// #endif
//     } else if (!strcasecmp(c->argv[1]->ptr,"doctor") && c->argc == 2) {
//         sds report = getMemoryDoctorReport();
//         addReplyBulkSds(c,report);
//     } else if (!strcasecmp(c->argv[1]->ptr,"purge") && c->argc == 2) {
// #if defined(USE_JEMALLOC)
//         char tmp[32];
//         unsigned narenas = 0;
//         size_t sz = sizeof(unsigned);
//         if (!je_mallctl("arenas.narenas", &narenas, &sz, NULL, 0)) {
//             sprintf(tmp, "arena.%d.purge", narenas);
//             if (!je_mallctl(tmp, NULL, 0, NULL, 0)) {
//                 addReply(c, shared.ok);
//                 return;
//             }
//         }
//         addReplyError(c, "Error purging dirty pages");
// #else
//         addReply(c, shared.ok);
//         /* Nothing to do for other allocators. */
// #endif
//     } else if (!strcasecmp(c->argv[1]->ptr,"help") && c->argc == 2) {
//         addReplyMultiBulkLen(c,5);
//         addReplyBulkCString(c,
// "MEMORY DOCTOR                        - Outputs memory problems report");
//         addReplyBulkCString(c,
// "MEMORY USAGE <key> [SAMPLES <count>] - Estimate memory usage of key");
//         addReplyBulkCString(c,
// "MEMORY STATS                         - Show memory usage details");
//         addReplyBulkCString(c,
// "MEMORY PURGE                         - Ask the allocator to release memory");
//         addReplyBulkCString(c,
// "MEMORY MALLOC-STATS                  - Show allocator internal stats");
//     } else {
//         addReplyError(c,"Syntax error. Try MEMORY HELP");
//     }
// }
