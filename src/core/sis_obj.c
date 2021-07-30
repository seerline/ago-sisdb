
#include "sis_obj.h"

// #define OBJ_ONE_THREAD

s_sis_object *sis_object_create(int style_, void *ptr)
{
    s_sis_object *o = sis_malloc(sizeof(*o));
    o->style = style_;
    o->ptr = ptr;
    o->refs = 1;
    return o;
}

//  再活一次
s_sis_object *sis_object_incr(s_sis_object *obj_) 
{
    if (obj_ && obj_->refs != SIS_OBJECT_MAX_REFS) 
    {
#ifdef OBJ_ONE_THREAD  
        obj_->refs++;
#else
        ADDF(&(obj_->refs), 1);
#endif
        return obj_;
    }
    return NULL;
}

void sis_object_decr(void *obj_) 
{
    s_sis_object *obj = (s_sis_object *)obj_;
    if (!obj || !obj->ptr)
    {
        return ;
    }
#ifndef OBJ_ONE_THREAD  
    unsigned int refs = obj->refs; 
    unsigned int newrefs = refs - 1; 
    while(!BCAS(&(obj->refs), refs, newrefs))
    {
        refs = obj->refs;
        newrefs = refs - 1;
        sis_sleep(1);
    }
    if (refs == 1)
#else
    if (obj->refs <= 1)
#endif
    {        
        switch(obj->style) 
        {
            case SIS_OBJECT_SDS: 
            {
                // printf("free [%s]\n",(s_sis_sds)(obj->ptr));
                sis_sdsfree(obj->ptr); 
            }
            break;
            case SIS_OBJECT_MEMORY: 
            {
                // printf("free [%p]\n",obj);
                sis_memory_destroy(obj->ptr); 
                break;
            }
            case SIS_OBJECT_NETMSG: sis_net_message_destroy(obj->ptr); break;
            case SIS_OBJECT_LIST: sis_struct_list_destroy(obj->ptr); break;
            case SIS_OBJECT_INT : break;
            default: LOG(5)("unknown object type.\n"); break;
        }
        sis_free(obj);
    }
#ifdef OBJ_ONE_THREAD
    else
    {
        obj->refs--;
    }
#endif
    
}
size_t sis_object_getsize(void *obj)
{
    s_sis_object *obj_ = (s_sis_object *)obj;
    size_t size = 0;
    switch(obj_->style) 
    {
        case SIS_OBJECT_SDS: size = sis_sdslen(obj_->ptr); break;
        case SIS_OBJECT_MEMORY: size = sis_memory_get_size(obj_->ptr); break;
        case SIS_OBJECT_NETMSG: size = sis_net_message_get_size(obj_->ptr); break;
        case SIS_OBJECT_LIST: 
            {
                s_sis_struct_list *list = (s_sis_struct_list *)obj_->ptr;
                size = list->len * list->count;
            }
            break;
        case SIS_OBJECT_INT : break;
        default: LOG(5)("unknown object type\n"); break;
    }
    return size;
}
char * sis_object_getchar(void *obj)
{
    s_sis_object *obj_ = (s_sis_object *)obj;
    char *ptr = NULL;
    switch(obj_->style) 
    {
        case SIS_OBJECT_SDS: ptr = (char *)(obj_->ptr); break;
        case SIS_OBJECT_MEMORY: ptr = sis_memory(obj_->ptr); break;
        case SIS_OBJECT_LIST: 
            {
                s_sis_struct_list *list = (s_sis_struct_list *)obj_->ptr;
                ptr = (char *)sis_struct_list_first(list);
            }
            break;
        case SIS_OBJECT_INT : break;
        default: 
            {
                ptr = (char *)(obj_->ptr); 
                LOG(5)("unknown object type.\n"); 
            }

            break;
    }
    return ptr;
}
#if 0
// cost: 3692
// cost: 2359
// cost: 1250
int main()
{
    int count = 10*1024*1024;
    msec_t msec = sis_time_get_now_msec();
    for (int i = 0; i < count; i++)
    {
        s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create());
        sis_object_destroy(obj);   
    }
    printf("cost: %llu\n", sis_time_get_now_msec() - msec);
    msec = sis_time_get_now_msec();
    for (int i = 0; i < count; i++)
    {
        s_sis_memory *obj = sis_memory_create();
        sis_memory_destroy(obj);   
    }
    printf("cost: %llu\n", sis_time_get_now_msec() - msec);
    msec = sis_time_get_now_msec();
    for (int i = 0; i < count; i++)
    {
        char *obj = sis_malloc(255);
        sis_free(obj);   
    }
    printf("cost: %llu\n", sis_time_get_now_msec() - msec);
    return 0;
}

#endif

#if 0
// 测试读写锁
#include <signal.h>
#include <stdio.h>

int __exit = 0;
int __kill = 0;
s_sis_wait __thread_wait_a; //线程内部延时处理
s_sis_wait __thread_wait_b; //线程内部延时处理


s_sis_thread ta ;
s_sis_thread tb ;

s_sis_share_list *__list;
s_sis_share_node *__node = NULL;
s_sis_share_node *__node_b = NULL;
void *__task_ta(void *argv_)
{
    sis_thread_wait_start(&__thread_wait_a);
    int count =0;
    while (!__kill)
    {
		// printf(" test ..a.. \n");
        if(sis_thread_wait_sleep_msec(&__thread_wait_a, 300) == SIS_ETIMEDOUT)
        {
            // printf("timeout ..a.. %d \n",__kill);
        } else 
		{
            // printf("no timeout ..a.. %d \n",__kill);
		}
        if (!__node)
        {
            __node = sis_share_list_first(__list);
            count++;
        }
        else
        {
            s_sis_share_node *node = sis_share_list_next(__list, __node);
            while (node)
            {
                count++;
                // printf("get a ok! %lld  %lld -- \n", node->serial, __list->count);
                __node = node;
                node = sis_share_list_next(__list, __node);
            }
        }      
        printf("get a ok!  %d  %lld -- \n",  count, __list->count);  
        // sis_sleep(1000);

    }
    if (__node)
    {
        sis_share_node_destroy(__node);
    }
	sis_thread_wait_stop(&__thread_wait_a);
	printf("a ok . \n");
    ta.working = false;
	// pthread_detach(pthread_self());

    return NULL;
}
void *__task_tb(void *argv_)
{
    int count =0;
    sis_thread_wait_start(&__thread_wait_a);
    while (!__kill)
    {
		// printf(" test ..b.. \n");
        if(sis_thread_wait_sleep_msec(&__thread_wait_a, 300) == SIS_ETIMEDOUT)
        {
            // printf("timeout ..a.. %d \n",__kill);
        } else 
		{
            // printf("no timeout ..a.. %d \n",__kill);
		}
        if (!__node_b)
        {
            count++;
            __node_b = sis_share_list_first(__list);
        }
        else
        {
            s_sis_share_node *node = sis_share_list_next(__list, __node_b);
            while (node)
            {
                count++;
                // printf("get b ok! %lld  %lld -- \n", node->serial, __list->count);
                __node_b = node;
                node = sis_share_list_next(__list, __node_b);
            }
        }
        printf("get b ok!  %d  %lld -- \n",  count, __list->count);  
         // sis_sleep(1000);
    }
    if (__node_b)
    {
        sis_share_node_destroy(__node_b);
    }
    sis_thread_wait_stop(&__thread_wait_a);
	printf("b ok . \n");
	tb.working = false;
    return NULL;
}
void exithandle(int sig)
{
	__kill = 1;
    printf("sighup received kill=%d \n",__kill);

	printf("kill . \n");
    sis_thread_wait_kill(&__thread_wait_a);
	
	sis_thread_join(&ta);
	printf("a ok .... \n");
	sis_thread_join(&tb);
	printf("b ok .... \n");

	printf("free . \n");
    sis_thread_wait_destroy(&__thread_wait_a);
	printf("ok . \n");

	__exit = 1;
}

int main()
{
    safe_memory_start();
    __list = sis_share_list_create("test", 1);

	sis_thread_wait_create(&__thread_wait_a);
	sis_thread_wait_create(&__thread_wait_b);

	sis_thread_create(__task_ta, NULL, &ta);
	printf("thread a ok!\n");

	// s_sis_thread_id_t tb ;
	sis_thread_create(__task_tb, NULL, &tb);
	printf("thread b ok!\n");

	signal(SIGINT,exithandle);

    int i = 0;
    char buffer[1025];
    size_t size =0;
	while(!__exit)
	{
        i++;
        memset(buffer, i%255, 1024);
        sis_share_list_push(__list, "info", 4, buffer, 1024, NULL);
        // printf("pusk ok! %lld %llu %llu - %p \n", __list->serial, __list->count,__list->size, __list->tail);
		sis_sleep(1);
        if (size > 1024*32)
        {
            sis_thread_wait_notice(&__thread_wait_a);
            size = 0;
        }
	}
	printf("end . \n");
    sis_share_list_destroy(__list);
    safe_memory_stop();
	return 1;
}
#endif
