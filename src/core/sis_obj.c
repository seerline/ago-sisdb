
#include "sis_obj.h"

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
        obj_->refs++;
        return obj_;
    }
    return NULL;
    // {
    //     void *newptr = NULL;
    //     if (obj_->ptr)
    //     {
    //         switch(obj_->style) 
    //         {
    //             case SIS_OBJECT_SDS: 
    //             {
    //                 newptr = sis_sdsdup((s_sis_sds)obj_->ptr); 
    //             }
    //             break;
    //             case SIS_OBJECT_MEMORY: sis_memory_destroy(obj_->ptr); break;
    //             {
    //                 newptr = sis_memory_create();
    //                 s_sis_memory *oldptr = (s_sis_memory *)obj_->ptr;
    //                 sis_memory_clone(oldptr, newptr);
    //             }
    //             break;
    //             case SIS_OBJECT_LIST: 
    //             {
    //                 s_sis_struct_list *oldptr = (s_sis_struct_list *)obj_->ptr;
    //                 newptr = sis_struct_list_create(oldptr->len);
    //                 sis_struct_list_clone(oldptr, newptr);
    //             }
    //             break;
    //             default: 
    //                 LOG(5)("unknown object type"); 
    //             break;
    //         }
    //     } 
    //     // 引用数满了，copy一个新的
    //     return sis_object_create(obj_->style, newptr);
    // }
}

// int sis_object_set(s_sis_object *obj_, int style_, void *ptr) 
// {
//     if (!obj_)
//     {
//         return -1;
//     }
//     // if (obj_->style != style_)
//     // {
//     //     return -2;
//     // }
//     if (obj_->ptr)
//     {
//         switch(obj_->style) 
//         {
//             case SIS_OBJECT_SDS: sis_sdsfree(obj_->ptr); break;
//             case SIS_OBJECT_MEMORY: sis_memory_destroy(obj_->ptr); break;
//             case SIS_OBJECT_LIST: sis_struct_list_destroy(obj_->ptr); break;
//             default: LOG(5)("unknown object type"); break;
//         }
//     }
//     obj_->style = style_;
//     obj_->ptr = ptr;
//     return 0;
// }

void sis_object_decr(void *obj_) 
{
    s_sis_object *obj = (s_sis_object *)obj_;
    if (!obj)
    {
        return ;
    }
    if (obj->refs == 1) 
    {
        
        switch(obj->style) 
        {
            case SIS_OBJECT_SDS: 
            {
                // printf("free [%s]\n",(s_sis_sds)(obj->ptr));
                sis_sdsfree(obj->ptr); 
            }
            break;
            case SIS_OBJECT_MEMORY: sis_memory_destroy(obj->ptr); break;
            case SIS_OBJECT_NETMSG: sis_net_message_destroy(obj->ptr); break;
            case SIS_OBJECT_LIST: sis_struct_list_destroy(obj->ptr); break;
            default: LOG(5)("unknown object type"); break;
        }
        sis_free(obj);
    } 
    else 
    {
        obj->refs--;
    }
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
        default: LOG(5)("unknown object type"); break;
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
        default: 
            {
                ptr = (char *)(obj_->ptr); 
                LOG(5)("unknown object type"); 
            }
            break;
    }
    return ptr;
}


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
