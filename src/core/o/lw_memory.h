

#ifndef _LW_MEMORY_H
#define _LW_MEMORY_H

#include <sts_core.h>
#include <lw_public.h>
#include <lw_thread.h>
#include "zmalloc.h"
#include "adlist.h"

typedef struct s_memory_pool {
	list   *memory_node_list;
	size_t  memory_total_count;
	size_t  memory_used_count;
	pthread_mutex_t memory_mutex;
} s_memory_pool;

typedef struct s_safe_memory {
	list   *safe_memory_list;
	pthread_mutex_t memory_mutex;
} s_safe_memory;

//-------------------------------------//
#define MEMORY_BLOCK_STEP   1024 //4096
#define MEMORY_POOL_STEP  100
#define MEMORY_POOL_MIN   10

typedef struct s_memory_node {
	char  *str;    //实际存储地址
	int    cite;   //引用次数
	size_t len;
	size_t maxlen;
} s_memory_node;

void memory_pool_start();
void memory_pool_stop();
size_t  memory_pool_free_count();
size_t  memory_pool_used_count();
size_t  memory_pool_total_count();

s_memory_node *pmalloc(size_t len_); //从这里获取的内存可以不断传递，直到不想再使用了，就调用decr_memory;
void incr_memory(s_memory_node *str_); //增加引用
void decr_memory(s_memory_node *str_); //已经回收的内存，再次运行不会再次回收
#define pfree decr_memory

//调用约定；
// s_memory_node *get_node (){  
// 	s_memory_node *node= pmalloc(); 
// 	...
// 	incr_memory(node); //需要返回该节点，就增加一次引用，再decr_memory
// 	decr_memory(node); //增加decr_memory只是为了程序可读性强，pmalloc和pfree配对
// 	return node;
// }
// void main (){  s_memory_node *node= get_node(); ... decr_memory(node);}

inline s_memory_node *create_memory_node()
{
	s_memory_node *n = zmalloc(sizeof(s_memory_node));
	n->cite = 0;
	n->len = 0;
	n->maxlen = 128;
	n->str = (char *)zmalloc(n->maxlen);
	return n;
};
inline void destroy_memory_node(s_memory_node *node_)
{
	if (node_->str) {
		zfree(node_->str);
	}
}
//////////////////////////////////////////
//  s_safe_memory 
//////////////////////////////////////////

#endif
