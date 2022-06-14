
#include "sis_net.node.h"
#include "sis_json.h"
#include "sis_math.h"
#include "sis_obj.h"
#include "os_str.h"

/////////////////////////////////////////////////
//  s_sis_net_mem_node
/////////////////////////////////////////////////
s_sis_net_mem_node *sis_net_mem_node_create(size_t size_)
{
	s_sis_net_mem_node *node = SIS_MALLOC(s_sis_net_mem_node, node);
	node->maxsize = size_ + 1;
	node->memory = sis_malloc(node->maxsize);
	// printf("new node %p\n", node->memory);
	return node;
}
void sis_net_mem_node_destroy(s_sis_net_mem_node *node_)
{
	// printf("del node %p\n", node_->memory);
	sis_free(node_->memory);
	sis_free(node_);
}
void sis_net_mem_node_clear(s_sis_net_mem_node *node_)
{
	node_->size = 0;
	node_->rpos = 0;
	node_->nums = 0;
}
void sis_net_mem_node_renew(s_sis_net_mem_node *node_, size_t size_)
{
	if (size_ > node_->maxsize)
	{
		node_->maxsize = size_ + 1;
		char *newmem = sis_malloc(node_->maxsize);
		if (node_->size > 0)
		{
			memmove(newmem, node_->memory, node_->size);
		}
		sis_free(node_->memory);
		node_->memory = newmem;
	}
}
/////////////////////////////////////////////////
//  s_sis_net_mems
/////////////////////////////////////////////////
// 永远保证有一个节点
s_sis_net_mems *sis_net_mems_create()
{
    s_sis_net_mems *o = SIS_MALLOC(s_sis_net_mems, o);
	sis_mutex_init(&o->lock, NULL);
	o->nouses = 64; // 大约保留1G的缓存
	s_sis_net_mem_node *node = sis_net_mem_node_create(SIS_NET_MEMSIZE);
	o->wnums = 1;
	o->whead = node;
	o->wtail = node;
	o->wnode = node;
	return o;
}
void sis_net_mems_destroy(s_sis_net_mems *nodes_)
{
    sis_mutex_lock(&nodes_->lock);
	{
		s_sis_net_mem_node *next = nodes_->rhead;
		while (next)
		{
			s_sis_net_mem_node *node = next->next;
			sis_net_mem_node_destroy(next);
			next = node;
		}
	}
	{
		s_sis_net_mem_node *next = nodes_->whead;
		while (next)
		{
			s_sis_net_mem_node *node = next->next;
			sis_net_mem_node_destroy(next);
			next = node;
		}
	}
	sis_mutex_unlock(&nodes_->lock);
	sis_mutex_destroy(&nodes_->lock);
	sis_free(nodes_);
}

// 清理写缓存
void _net_mems_move_write(s_sis_net_mems *nodes_)
{
	s_sis_net_mem_node *next = nodes_->whead;	
	while (next)
	{
		s_sis_net_mem_node *node = next->next;
		sis_net_mem_node_clear(next);
		next = node;
	}
	nodes_->wsize = 0;
	nodes_->wuses = 0;
	nodes_->wnode = nodes_->whead;
}
// static int64 _send_nums = 0;
// static int64 _send_size = 0;
// static int64 _recv_nums = 0;
// static int64 _recv_size = 0;

// 清理读缓存
void _net_mems_move_read(s_sis_net_mems *nodes_)
{
	s_sis_net_mem_node *next = nodes_->rhead;	
	while (next)
	{
		s_sis_net_mem_node *node = next->next;
		// _recv_nums++;
		// _recv_size += next->size;
		// if (_send_nums % 100 == 0)
		// {
		// 	printf("read  : %lld %lld %lld %lld\n", _send_nums, _send_size, _recv_nums, _recv_size);
		// }
		sis_net_mem_node_clear(next);
		next = node;
	}
	if (nodes_->rhead)
	{
		int nouse = nodes_->rnums + (nodes_->wnums - nodes_->wuses);
		if (nouse > nodes_->nouses)
		{
			next = nodes_->rhead;	
			while (next && nouse > nodes_->nouses)
			{
				s_sis_net_mem_node *node = next->next;
				nodes_->wtail->next = next;
				nodes_->wtail = next;
				nodes_->wnums ++;
				next = node;
				nouse--;
			}
			while (next)
			{
				s_sis_net_mem_node *node = next->next;
				sis_net_mem_node_destroy(next);
				next = node;
			}
		}
		else
		{
			nodes_->wtail->next = nodes_->rhead;
			nodes_->wtail = nodes_->rtail;
			nodes_->wnums += nodes_->rnums;
		}
		nodes_->wtail->next = NULL;
	}
	nodes_->rnums = 0;
	nodes_->rhead = NULL;
	nodes_->rtail = NULL;	
	nodes_->rsize = 0;
}
void sis_net_mems_clear(s_sis_net_mems *nodes_)
{
    sis_mutex_lock(&nodes_->lock);
	_net_mems_move_write(nodes_);
	_net_mems_move_read(nodes_);
    sis_mutex_unlock(&nodes_->lock);
}
// 传入新增数据的长度
// 返回当前可写的节点
s_sis_net_mem_node *_net_mems_reset_node(s_sis_net_mems *nodes_, size_t isize_)
{
	s_sis_net_mem_node *node = nodes_->wnode;
	while (node)
	{
		if ((isize_ + node->size) > node->maxsize)
		{
			if (node->size == 0) // 表示进入空节点 如果大小不够就重新申请
			{
				sis_net_mem_node_renew(node, isize_);
				break;
			}
		}
		else
		{
			break;
		}
		node = node->next;
	}
	if (node)
	{
		return node;
	}
	node = sis_net_mem_node_create(SIS_NET_MEMSIZE);
	nodes_->wtail->next = node;
	nodes_->wtail = node;
	// nodes_->wtail->next = NULL;
	nodes_->wnode = node;
	nodes_->wnums++;
	return node;
}
/**
 * @brief 将数据添加到缓存区，后续经过解压缩、协议处理后才能使用
 * @param nodes_ 数据缓存s_sis_net_mems
 * @param in_ 收到的数据
 * @param isize_ 收到的数据大小
 * @return int 
 */
int  sis_net_mems_push(s_sis_net_mems *nodes_, void *in_, size_t isize_)
{
	sis_mutex_lock(&nodes_->lock);
	s_sis_net_mem_node *node = _net_mems_reset_node(nodes_, isize_ + sizeof(int));
	nodes_->wnode = node;
	if (node->size == 0)
	{
		nodes_->wuses++;
	}
	memmove(node->memory + node->size, &isize_, sizeof(int));
	node->size += sizeof(int);
	memmove(node->memory + node->size, in_, isize_);
	node->size += isize_;
	node->nums++;
	// printf("push: %zu %d %d %d\n", node->size, node->nums, node->rpos, node->maxsize);
	// 拷贝内存结束
	nodes_->wsize += (isize_+ sizeof(int));
	sis_mutex_unlock(&nodes_->lock);
	return nodes_->wuses;
}

int  sis_net_mems_push_sign(s_sis_net_mems *nodes_, int8 sign_, void *in_, size_t isize_)
{
	sis_mutex_lock(&nodes_->lock);
	s_sis_net_mem_node *node = _net_mems_reset_node(nodes_, isize_ + sizeof(int8) + sizeof(int));
	nodes_->wnode = node;
	if (node->size == 0)
	{
		nodes_->wuses++;
	}
	size_t isize = isize_ + sizeof(int8);
	memmove(node->memory + node->size, &isize, sizeof(int));
	node->size += sizeof(int);
	memmove(node->memory + node->size, &sign_, sizeof(int8));
	node->size += sizeof(int8);
	if (in_ && isize_ > 0)
	{
		memmove(node->memory + node->size, in_, isize_);
		node->size += isize_;
	}
	node->nums ++;
	// if (node->nums % 100000 == 0)
	// printf("push sign: %zu %d %d %d\n", node->size, node->nums, node->rpos, node->maxsize);
	// 拷贝内存结束
	// _send_nums++;
	// _send_size+=isize;
	// if (_send_nums % 100000 == 0)
	// {
	// 	printf("write : %lld %lld %lld %lld\n", _send_nums, _send_size, _recv_nums, _recv_size);
	// }
	nodes_->wsize += (isize + sizeof(int));
	sis_mutex_unlock(&nodes_->lock);
	return nodes_->wuses;	
}

int  sis_net_mems_push_kv(s_sis_net_mems *nodes_, int kidx_, int sidx_, void *in_, size_t isize_)
{
	sis_mutex_lock(&nodes_->lock);
	s_sis_net_mem_node *node = _net_mems_reset_node(nodes_, isize_ + 2 * sizeof(int) + sizeof(int));
	nodes_->wnode = node;
	if (node->size == 0)
	{
		nodes_->wuses++;
	}
	size_t isize = isize_ + 2 * sizeof(int);
	memmove(node->memory + node->size, &isize, sizeof(int));
	node->size += sizeof(int);
	memmove(node->memory + node->size, &kidx_, sizeof(int));
	node->size += sizeof(int);
	memmove(node->memory + node->size, &sidx_, sizeof(int));
	node->size += sizeof(int);
	if (in_ && isize_ > 0)
	{
		memmove(node->memory + node->size, in_, isize_);
		node->size += isize_;
	}
	node->nums ++;
	nodes_->wsize += (isize + sizeof(int));
	sis_mutex_unlock(&nodes_->lock);
	return nodes_->wuses;	
}

int  sis_net_mems_cat(s_sis_net_mems *nodes_, void *in_, size_t isize_)
{
	sis_mutex_lock(&nodes_->lock);
	s_sis_net_mem_node *node = _net_mems_reset_node(nodes_, isize_);
	nodes_->wnode = node;
	if (node->size == 0)
	{
		nodes_->wuses++;
		// _send_nums++;
	}
	// _send_size+=isize_;
	// if (_send_nums % 100000 == 0)
	// {
	// 	printf("write : %lld %lld %lld %lld\n", _send_nums, _send_size, _recv_nums, _recv_size);
	// }
	memmove(node->memory + node->size, in_, isize_);
	node->size += isize_;
	node->nums = 1; // 此时所有数据都在一起 nums 失去意义恒定为 1
	// 拷贝内存结束
	nodes_->wsize += isize_;
	sis_mutex_unlock(&nodes_->lock);
	return nodes_->wuses;
}

s_sis_net_mem_node *sis_net_mems_rhead(s_sis_net_mems *nodes_)
{
	// {
	// 	int count = 0;
	// 	s_sis_net_mem_node *next = nodes_->rhead;
	// 	while (next)
	// 	{
	// 		next = next->next;
	// 		count++;
	// 	}
	// 	printf("rsnum = %d\n", count);
	// }
	// {
	// 	int count = 0;
	// 	s_sis_net_mem_node *next = nodes_->whead;
	// 	while (next)
	// 	{
	// 		next = next->next;
	// 		count++;
	// 	}
	// 	printf("wsnum = %d\n", count);
	// }
	return nodes_->rhead;
}

s_sis_net_mem *sis_net_mems_pop(s_sis_net_mems *nodes_)
{
	s_sis_net_mem_node *node = nodes_->rhead;
	if (node->rpos >= node->size)
	{
		if (node->next)
		{
			nodes_->rtail->next = node;
			nodes_->rtail = node;
			nodes_->rhead = node->next;
			node->next = NULL;		
			sis_net_mem_node_clear(node);
			node = nodes_->rhead;
		}
		else
		{
			sis_net_mem_node_clear(node);
			return NULL;
		}
	}
	if (node && node->rpos < node->size)
	{
		s_sis_net_mem *mem = (s_sis_net_mem *)(node->memory + node->rpos);
		// printf("#### %p %p %d %d %d\n", mem, node->memory, node->rpos, node->size, node->maxsize);
		node->rpos += mem->size + sizeof(int);
		nodes_->rsize -= mem->size + sizeof(int);

		// _recv_nums++;
		// _recv_size+=mem->size;
		// if (_recv_nums % 100000 == 0)
		// {
		// 	printf("read : %lld %lld %lld %lld\n", _send_nums, _send_size, _recv_nums, _recv_size);
		// }
		return mem;
	}
	return NULL;
}
// 返回数据的尺寸
int sis_net_mems_read(s_sis_net_mems *nodes_, int readnums_)
{
	if (nodes_->rsize > 0)
	{
		int count = 0;
		s_sis_net_mem_node *node = nodes_->rhead;
		while (node && node->rpos < node->size)
		{
			count++;
			node = node->next;
		}
		if (count < 1)
		{
			LOG(5)("mems_read warn : %d %lld\n", count, (int64)nodes_->rsize);
		}
		return count;
	}
	// sis_mutex_lock(&nodes_->lock);
	if (!sis_mutex_trylock(&nodes_->lock))
	{	
		// 下面这句是为了防止未释放读队列
		_net_mems_move_read(nodes_);
		int readnums = readnums_ == 0 ? nodes_->wuses : readnums_;
		s_sis_net_mem_node *next = nodes_->whead;	
		sis_net_mems_rhead(nodes_);
		// printf("=== %p %d %d %d %d\n", next, nodes_->wuses, next ? next->nums : -1, nodes_->rnums, nodes_->wsize);
		while (next && next->size > 0 && nodes_->wuses > 0 && nodes_->rnums < readnums)
		{
			nodes_->wnums--;
			nodes_->wuses--;
			nodes_->wsize -= next->size;
			nodes_->rnums++;
			nodes_->rsize += next->size;
			if (!nodes_->rhead)
			{
				nodes_->rhead = next;
				nodes_->rtail = next;
			} 
			else
			{
				nodes_->rtail->next = next;
				nodes_->rtail = next;
			}
			next = next->next;
			if (next)
			{
				if (nodes_->whead == nodes_->wnode)
				{
					nodes_->wnode = next;
				}
				nodes_->whead = next;
			}
			else
			{
				s_sis_net_mem_node *node = sis_net_mem_node_create(SIS_NET_MEMSIZE);
				nodes_->wnums = 1;
				nodes_->whead = node;
				nodes_->wtail = node;
				nodes_->wnode = node;
			}
			nodes_->rtail->next = NULL;
		}
		// printf("=== %p %p %d %d\n", nodes_->whead, nodes_->wnode, nodes_->whead ? nodes_->whead->size : -1, nodes_->wnode ? nodes_->wnode->size : -1);
		sis_mutex_unlock(&nodes_->lock);
		return 	nodes_->rnums;
	}
	// printf("==3== lock ok. %d :: %d\n", nodes_->rnums, nodes_->wnums);
	return 0;
}
int sis_net_mems_free_read(s_sis_net_mems *nodes_)
{
	int count = nodes_->rnums;
    sis_mutex_lock(&nodes_->lock);
	_net_mems_move_read(nodes_);
    sis_mutex_unlock(&nodes_->lock);
	return count;
}
int  sis_net_mems_count(s_sis_net_mems *nodes_)
{
	sis_mutex_lock(&nodes_->lock);
	int count = nodes_->rnums + nodes_->wnums;
	sis_mutex_unlock(&nodes_->lock);
	return count;
}
int  sis_net_mems_nums(s_sis_net_mems *nodes_)
{
	sis_mutex_lock(&nodes_->lock);
	int count = 0;
	if (nodes_->rnums > 0)
	{
		s_sis_net_mem_node *next = nodes_->rhead;
		while (next && next->nums > 0)
		{
			count += next->nums;
			next = next->next;
		}
	}
	if (nodes_->wnums > 0)
	{
		s_sis_net_mem_node *next = nodes_->whead;
		while (next && next->nums > 0)
		{
			count += next->nums;
			next = next->next;
		}
	}
	sis_mutex_unlock(&nodes_->lock);
	return count;
}
size_t  sis_net_mems_size(s_sis_net_mems *nodes_)
{
	sis_mutex_lock(&nodes_->lock);
	size_t size = nodes_->rsize + nodes_->wsize;
	sis_mutex_unlock(&nodes_->lock);
	return size;
}

/////////////////////////////////////////////////
//  s_sis_net_node
/////////////////////////////////////////////////
s_sis_net_node *sis_net_node_create(s_sis_object *obj_)
{
    s_sis_net_node *node = SIS_MALLOC(s_sis_net_node, node);
    if (obj_)
    {
        sis_object_incr(obj_);
        node->obj = obj_;
    }
    node->next = NULL;
    return node;    
}
void sis_net_node_destroy(s_sis_net_node *node_)
{
    if(node_->obj)
    {
        sis_object_decr(node_->obj);
    }
    sis_free(node_);
}
s_sis_net_nodes *sis_net_nodes_create()
{
    s_sis_net_nodes *o = SIS_MALLOC(s_sis_net_nodes, o);
	sis_mutex_init(&o->lock, NULL);
	return o;
}
int _net_nodes_free_read(s_sis_net_nodes *nodes_)
{
	int count = 0;
	
	if (nodes_->rhead)
	{
		s_sis_net_node *next = nodes_->rhead;	
		while (next)
		{
			s_sis_net_node *node = next->next;
			nodes_->size -= SIS_OBJ_GET_SIZE(next->obj);
			sis_net_node_destroy(next);
			next = node;
			count++;
		}
		nodes_->rhead = NULL;
	}
	nodes_->rtail = NULL;	
	nodes_->rnums = 0;
	return count;	
}
void sis_net_nodes_destroy(s_sis_net_nodes *nodes_)
{
    sis_mutex_lock(&nodes_->lock);
	_net_nodes_free_read(nodes_);
	s_sis_net_node *next = nodes_->whead;	
	while (next)
	{
		s_sis_net_node *node = next->next;
		sis_net_node_destroy(next);
		next = node;
	}
    sis_mutex_unlock(&nodes_->lock);
	sis_mutex_destroy(&nodes_->lock);
	sis_free(nodes_);
}
void sis_net_nodes_clear(s_sis_net_nodes *nodes_)
{
    sis_mutex_lock(&nodes_->lock);
	_net_nodes_free_read(nodes_);
	s_sis_net_node *next = nodes_->whead;	
	while (next)
	{
		s_sis_net_node *node = next->next;
		sis_net_node_destroy(next);
		next = node;
	}
	nodes_->whead = NULL;
	nodes_->wtail = NULL;
	nodes_->wnums = 0;
	nodes_->size = 0;
    sis_mutex_unlock(&nodes_->lock);
}
int  sis_net_nodes_push(s_sis_net_nodes *nodes_, s_sis_object *obj_)
{
	s_sis_net_node *newnode = sis_net_node_create(obj_);
	sis_mutex_lock(&nodes_->lock);
	nodes_->wnums++;
	if (!nodes_->whead)
	{
		nodes_->whead = newnode;
		nodes_->wtail = newnode;
	}
	else if (nodes_->whead == nodes_->wtail)
	{
		nodes_->whead->next = newnode;
		nodes_->wtail = newnode;
	}
	else
	{
		nodes_->wtail->next = newnode;
		nodes_->wtail = newnode;
	}
	nodes_->size += SIS_OBJ_GET_SIZE(newnode->obj);
	sis_mutex_unlock(&nodes_->lock);
	return nodes_->wnums;
}

int sis_net_nodes_read(s_sis_net_nodes *nodes_, int readnums_)
{
	if (nodes_->rnums > 0)
	{
		return nodes_->rnums;
	}
	if (!sis_mutex_trylock(&nodes_->lock))
	{	
		if (readnums_ == 0)
		{
			nodes_->rnums = nodes_->wnums;
			nodes_->rhead = nodes_->whead;
			nodes_->rtail = nodes_->wtail;
			nodes_->wnums = 0;
			nodes_->whead = NULL;
			nodes_->wtail = NULL;
		}
		else
		{
			s_sis_net_node *next = nodes_->whead;	
			while (next)
			{
				nodes_->rnums++;
				nodes_->wnums--;
				if (!nodes_->rhead)
				{
					nodes_->rhead = next;
					nodes_->rtail = next;
				} 
				else
				{
					nodes_->rtail->next = next;
					nodes_->rtail = next;
				}
				next = next->next;
				if (next)
				{
					nodes_->whead = next;
				}
				else
				{
					nodes_->whead = NULL;
					nodes_->wtail = NULL;
				}
				nodes_->rtail->next = NULL;
				if (nodes_->rnums >= readnums_)
				{
					break;
				}
			}
		}
		sis_mutex_unlock(&nodes_->lock);
		return 	nodes_->rnums;
	}
	// printf("==3== lock ok. %lld :: %d\n", nodes_->nums, nodes_->sendnums);
	return 0;
}
int sis_net_nodes_free_read(s_sis_net_nodes *nodes_)
{
	int count = 0;
    sis_mutex_lock(&nodes_->lock);
	count = _net_nodes_free_read(nodes_);
    sis_mutex_unlock(&nodes_->lock);
	return count;
}
int  sis_net_nodes_count(s_sis_net_nodes *nodes_)
{
	sis_mutex_lock(&nodes_->lock);
	int count = nodes_->rnums + nodes_->wnums;
	sis_mutex_unlock(&nodes_->lock);
	return count;
}
size_t  sis_net_nodes_size(s_sis_net_nodes *nodes_)
{
	// sis_mutex_lock(&nodes_->lock);
	// int count = nodes_->rnums + nodes_->wnums;
	// sis_mutex_unlock(&nodes_->lock);
	return nodes_->size;
}

///////////////////////////////////////////////////////////////////////////
//----------------------s_sis_net_list --------------------------------//
//  以整数为索引 存储指针的列表
//  如果有wait就说明释放index需要等待一个超时
///////////////////////////////////////////////////////////////////////////
s_sis_net_list *sis_net_list_create(void *vfree_)
{
	s_sis_net_list *o = SIS_MALLOC(s_sis_net_list, o);
	o->max_count = 1024;
	o->used = sis_malloc(o->max_count);
	memset(o->used, SIS_NET_NOUSE ,o->max_count);
	o->stop_sec = sis_malloc(sizeof(time_t) * o->max_count);
	memset(o->stop_sec, 0 ,sizeof(time_t) * o->max_count);
	o->buffer = sis_malloc(sizeof(void *) * o->max_count);
	memset(o->buffer, 0 ,sizeof(void *) * o->max_count);
	o->vfree = vfree_;
	o->wait_sec = 60; // 默认3分钟后释放资源
	return o;
}
void sis_net_list_destroy(s_sis_net_list *list_)
{
	sis_net_list_clear(list_);
	sis_free(list_->used);
	sis_free(list_->buffer);
	sis_free(list_->stop_sec);
	sis_free(list_);
}
void sis_net_list_clear(s_sis_net_list *list_)
{
	char **ptr = (char **)list_->buffer;
	for (int i = 0; i < list_->max_count; i++)
	{
		if (list_->vfree && ptr[i])
		{
			list_->vfree(ptr[i], 1);
			ptr[i] = NULL;
		}
		list_->used[i] = SIS_NET_NOUSE;
		list_->stop_sec[i] = 0;
	}
	list_->cur_count = 0;
}

int _net_list_set_grow(s_sis_net_list *list_, int count_)
{
	if (count_ > list_->max_count)
	{
		int maxcount = (count_ / 1024 + 1) * 1024;
		unsigned char *used = sis_malloc(maxcount);
		memset(used, SIS_NET_NOUSE , maxcount);
		time_t *stop_sec = sis_malloc(sizeof(time_t) * maxcount);
		memset(stop_sec, 0 ,sizeof(time_t) * maxcount);
		void *buffer = sis_malloc(sizeof(void *) * maxcount);
		memset(buffer, 0 ,sizeof(void *) * maxcount);

		memmove(list_->used, used, list_->max_count);
		memmove(list_->stop_sec, stop_sec, sizeof(time_t) * list_->max_count);
		memmove(list_->buffer, buffer, sizeof(void *) * list_->max_count);
		list_->max_count = maxcount;
	}
	return list_->max_count;
}

void _net_list_free(s_sis_net_list *list_, int index_)
{
	char **ptr = (char **)list_->buffer;
	if (list_->vfree && ptr[index_])
	{
		list_->vfree(ptr[index_], 1);
		ptr[index_] = NULL;
	}
	list_->stop_sec[index_] = 0;
}
/**
 * @brief 将新的session添加到session列表，并按如下方法分配SESSON ID，把已有的ID遍历一遍，
 * （1）如果发现有处于未使用状态的，直接拿来用
 * （2）如果发现有处于关闭状态且超过超时时间的，直接拿来用
 * （3）如果都没有，新增一个
 * @param list_ 服务器的SESSION列表
 * @param in_ 新添加的session
 * @return int 分配到的SESSON ID
 */
int sis_net_list_new(s_sis_net_list *list_, void *in_)
{
	//QQQ session的各种状态之间的切换，在哪里进行？这里需要考虑线程安全么？SESSION ID如果每个新连接都不一样的话，那这个东西存在的意义是什么？
	time_t now_sec = sis_time_get_now();
	int index = -1;
	for (int i = 0; i < list_->max_count; i++)
	{
		// printf("==new== %d %d %d | %d %d\n", i, list_->used[i], list_->wait_sec, now_sec, list_->stop_sec[i]);
		if (list_->used[i] == SIS_NET_NOUSE)
		{
			index = i;
			break;
		}
		if (list_->wait_sec > 0 && list_->used[i] == SIS_NET_CLOSE && 
			(now_sec - list_->stop_sec[i]) > list_->wait_sec)
		{
			_net_list_free(list_, i);
			list_->used[i] = SIS_NET_NOUSE;
			index = i;
			break;
		}
	}
	if (index < 0)
	{
		index = list_->max_count;
		_net_list_set_grow(list_, list_->max_count + 1);
	}
	char **ptr = (char **)list_->buffer;
	ptr[index] = (char *)in_;
	list_->used[index] = SIS_NET_USEED;
	// list_->stop_sec[index] = now_sec;
	list_->cur_count++;
	return index;
}
void *sis_net_list_get(s_sis_net_list *list_, int index_)
{
	if (index_ < 0 || index_ >= list_->max_count)
	{
		return NULL;
	}	
	if (list_->used[index_] != SIS_NET_USEED)
	{
		return NULL;
	}
	char **ptr = (char **)list_->buffer;
	return ptr[index_];
}
int sis_net_list_first(s_sis_net_list *list_)
{
	int index = -1;
	for (int i = 0; i < list_->max_count; i++)
	{
		if (list_->used[i] == SIS_NET_USEED)
		{
			index = i;
			break;
		}
	}
	return index;
}
int sis_net_list_next(s_sis_net_list *list_, int index_)
{
	if (list_->cur_count < 1)
	{
		return -1;
	}
	int index = index_;
	int start = index_ + 1;
	while (start != index_)
	{
		if (start > list_->max_count - 1)
		{
			start = 0;
		}
		if (list_->used[start] == SIS_NET_USEED)
		{
			index = start;
			break;
		}
		start++;		
	}
	return index;
}
int sis_net_list_uses(s_sis_net_list *list_)
{
	return list_->cur_count;
}

int sis_net_list_stop(s_sis_net_list *list_, int index_)
{
	if (index_ < 0 || index_ >= list_->max_count)
	{
		return -1;
	}	
	// printf("==stop=1= %d %d | %d %d\n", index_, list_->used[index_], list_->wait_sec, list_->stop_sec[index_]);
	if (list_->used[index_] != SIS_NET_USEED)
	{
		return -1;
	}
	if (list_->wait_sec > 0)
	{
		list_->stop_sec[index_] = sis_time_get_now();
		list_->used[index_] = SIS_NET_CLOSE;
		char **ptr = (char **)list_->buffer;
		if (list_->vfree && ptr[index_])
		{
			list_->vfree(ptr[index_], 0);
		}
		list_->cur_count--;
	}
	else
	{
		_net_list_free(list_, index_);
		list_->used[index_] = SIS_NET_NOUSE;
		list_->cur_count--;
	}
	// printf("==stop=2= %d %d | %d %d\n", index_, list_->used[index_], list_->wait_sec, list_->stop_sec[index_]);
	return index_;
}


////////////////////////////////////////////////////////
//  解码函数
////////////////////////////////////////////////////////

bool _net_encoded_chars(s_sis_net_message *netmsg_, s_sis_memory *out_)
{
    s_sis_json_node *node = sis_json_create_object();
	if (netmsg_->switchs.sw_sno) 
	{
		sis_json_object_set_int(node, "sno", netmsg_->sno);
	}
	if (netmsg_->switchs.sw_service)
	{
		sis_json_object_set_string(node, "service", netmsg_->service, SIS_SDS_SIZE(netmsg_->service)); 
	}		
	if (netmsg_->switchs.sw_cmd)
	{
		sis_json_object_set_string(node, "cmd", netmsg_->cmd, SIS_SDS_SIZE(netmsg_->cmd)); 
	}		
	if (netmsg_->switchs.sw_subject)
	{
		sis_json_object_set_string(node, "subject", netmsg_->subject, SIS_SDS_SIZE(netmsg_->subject)); 
	}	
	if (netmsg_->switchs.sw_tag)
	{
		sis_json_object_set_int(node, "tag", netmsg_->tag);
	}		
	if (netmsg_->tag == SIS_NET_TAG_CHARS || netmsg_->tag == SIS_NET_TAG_BYTES)
	{
		if (netmsg_->argvs)
		{
			// 这里增加数组信息
			for (int i = 0; i < netmsg_->argvs->count; i++)
			{
				// s_sis_object *obj = sis_pointer_list_get(netmsg_->argvs, i);
				// sis_memory_cat_ssize(out_, SIS_OBJ_GET_SIZE(obj));
				// sis_memory_cat(out_, SIS_OBJ_GET_CHAR(obj), SIS_OBJ_GET_SIZE(obj));
			}
		}
		else
		{
			sis_json_object_set_string(node, "infos", "[[]]", 4); 
		}
	}
	else
	{
		if (netmsg_->switchs.sw_info)
		{
			sis_json_object_set_string(node, "info", netmsg_->info, SIS_SDS_SIZE(netmsg_->info)); 
		}	
	}
	// 这里处理 more 的字段定义
	// printf("sis_net_encoded_json [%d]: %d %s\n",netmsg_->tag, netmsg_->switchs.sw_info, netmsg_->info);

    size_t len = 0;
    char *str = sis_json_output_zip(node, &len);
    // printf("sis_net_encoded_json [%d]: %d %s\n",in_->style, (int)len, str);
    sis_memory_cat(out_, str, len);
	// sis_out_binary("encode", str,  len);
    sis_free(str);
    sis_json_delete_node(node);
    
    return true;
}

static inline void sis_net_str_set_memory(s_sis_memory *imem_, int isok_, const char *msg_, size_t size_)
{
	if (isok_) 
	{
		sis_memory_cat_ssize(imem_, msg_ ? size_ : 0);
		if (msg_ && size_ > 0)
		{
			sis_memory_cat(imem_, (char *)msg_, size_);
		}
	}
}
static inline void sis_net_int_set_memory(s_sis_memory *imem_, int isok_, int msg_)
{
	if (isok_) 
	{
		if (msg_ != 0)
		{
			char ii[32];
			sis_lldtoa(msg_, ii, 32, 10);
			sis_memory_cat_ssize(imem_, sis_strlen(ii));
			sis_memory_cat(imem_, ii, sis_strlen(ii));
		}
		else
		{
			sis_memory_cat_ssize(imem_, 0);
		}
	}
}
bool sis_net_encoded_normal(s_sis_net_message *in_, s_sis_memory *out_)
{
	// 编码 一般只写 argvs 中的数据
    sis_memory_clear(out_);
    if (in_->name)
    {
        sis_memory_cat(out_, in_->name, sis_sdslen(in_->name));
    }
    sis_memory_cat(out_, ":", 1);
	if (in_->format == SIS_NET_FORMAT_CHARS)
	{
		return _net_encoded_chars(in_, out_);
	}
	// 二进制流
	in_->switchs.sw_mark = 1;
	sis_memory_cat(out_, (char *)&in_->switchs, sizeof(s_sis_net_switch));
	sis_net_int_set_memory(out_, in_->switchs.sw_sno, in_->sno);
	sis_net_str_set_memory(out_, in_->switchs.sw_service, in_->service, SIS_SDS_SIZE(in_->service));
	sis_net_str_set_memory(out_, in_->switchs.sw_cmd, in_->cmd, SIS_SDS_SIZE(in_->cmd));
	sis_net_str_set_memory(out_, in_->switchs.sw_subject, in_->subject, SIS_SDS_SIZE(in_->subject));
	sis_net_int_set_memory(out_, in_->switchs.sw_tag, in_->tag);
	if (in_->tag == SIS_NET_TAG_CHARS || in_->tag == SIS_NET_TAG_BYTES)
	{
		if (in_->argvs)
		{
			sis_memory_cat_ssize(out_, in_->argvs->count);
			for (int i = 0; i < in_->argvs->count; i++)
			{
				s_sis_object *obj = sis_pointer_list_get(in_->argvs, i);
				sis_memory_cat_ssize(out_, SIS_OBJ_GET_SIZE(obj));
				sis_memory_cat(out_, SIS_OBJ_GET_CHAR(obj), SIS_OBJ_GET_SIZE(obj));
			}
		}
		else
		{
			sis_memory_cat_ssize(out_, 0);
		}
	}
	else
	{
		sis_net_str_set_memory(out_, in_->switchs.sw_info, in_->info, SIS_SDS_SIZE(in_->info));
	}
    return true;
}

s_sis_sds _sis_json_node_get_sds(s_sis_json_node *node_, const char *key_)
{
    s_sis_sds o = NULL;
    s_sis_json_node *node = sis_json_cmp_child_node(node_, key_);
    if (!node)
    {
        return NULL;
    }
    if (node->type == SIS_JSON_STRING)
    {
		// printf("%s : %s\n",key_, node->value);
        o = sis_sdsnew(node->value);
    }
    else if (node->type == SIS_JSON_ARRAY || node->type == SIS_JSON_OBJECT)
    {
		size_t size = 0;
		char *str = sis_json_output_zip(node, &size);
		o = sis_sdsnewlen(str, size);
		sis_free(str);
    }
    return o;
}
void sis_json_to_netmsg(s_sis_json_node* node_, s_sis_net_message *mess)
{
	s_sis_json_node *sno = sis_json_cmp_child_node(node_, "sno");  
	if (sno)
	{
        mess->sno = sis_json_get_int(node_, "sno", 0);    
		mess->switchs.sw_sno = 1;
	}
	mess->service = _sis_json_node_get_sds(node_, "service");    
	mess->switchs.sw_service = mess->service ? 1 : 0;
	mess->cmd = _sis_json_node_get_sds(node_, "cmd");    
	mess->switchs.sw_cmd = mess->cmd ? 1 : 0;
	mess->subject = _sis_json_node_get_sds(node_, "subject");    
	mess->switchs.sw_subject = mess->subject ? 1 : 0;
	s_sis_json_node *tag = sis_json_cmp_child_node(node_, "tag");  
	if (tag)
	{
        mess->tag = sis_json_get_int(node_, "tag", 0);    
		mess->switchs.sw_tag = 1;
	}
	mess->info = _sis_json_node_get_sds(node_, "info");    
	mess->switchs.sw_info = mess->info ? 1 : 0;

	s_sis_json_node *more = sis_json_cmp_child_node(node_, "more");  
	if (more)
	{
		// ......
		mess->switchs.sw_more = 1;
	}
}

bool sis_net_decoded_chars(s_sis_memory *in_, s_sis_net_message *out_)
{
    // s_sis_net_message *mess = (s_sis_net_message *)out_;
    s_sis_json_handle *handle = sis_json_load(sis_memory(in_), sis_memory_get_size(in_));
    if (!handle)
    {
        LOG(5)("json parse error.\n");
        return false;
    }
	sis_json_to_netmsg(handle->node, out_);
    sis_json_close(handle);
    return true;
}

static inline s_sis_sds sis_net_memory_get_sds(s_sis_memory *imem_, int isok_)
{
	s_sis_sds o = NULL;
	if (isok_) 
	{
		size_t size = sis_memory_get_ssize(imem_);
		if (size > 0)
		{
			o = sis_sdsnewlen(sis_memory(imem_), size);
		}
		sis_memory_move(imem_, size);
	}
	return o;
}
static inline int64 sis_net_memory_get_int(s_sis_memory *imem_, int isok_)
{
	int64 o = 0;
	if (isok_) 
	{
		size_t size = sis_memory_get_ssize(imem_);
		if (size > 0)
		{
			char ii[32];
			sis_strncpy(ii, 32, sis_memory(imem_), size);
			o = sis_atoll(ii);
		}
		sis_memory_move(imem_, size);
	}
	return o;
}
bool sis_net_decoded_normal(s_sis_memory *in_, s_sis_net_message *out_)
{
	size_t start = sis_memory_get_address(in_);
    int cursor = sis_str_pos(sis_memory(in_), sis_min((int)sis_memory_get_size(in_) ,128), ':');
    if (cursor < 0)
    {
        return false;
    }
    s_sis_net_message *mess = (s_sis_net_message *)out_;
    sis_net_message_clear(mess);
    if (cursor > 0)
    {
        mess->name = sis_sdsnewlen(sis_memory(in_), cursor);
    }
	sis_memory_move(in_, cursor + 1);
	s_sis_net_switch *sw = (s_sis_net_switch *)sis_memory(in_);
	// printf("switch : %x %c\n", *((uint8 *)sw), *((char *)sw));
	if (sw->sw_mark == 0)
	{
		// 判断数据格式
		mess->format = SIS_NET_FORMAT_CHARS;
		bool o = sis_net_decoded_chars(in_, out_);
		sis_memory_jumpto(in_, start);
		return o;
	}
	mess->format = SIS_NET_FORMAT_BYTES;
	memmove(&mess->switchs, sis_memory(in_), sizeof(s_sis_net_switch));
	sis_memory_move(in_, sizeof(s_sis_net_switch));
	mess->sno = sis_net_memory_get_int(in_, mess->switchs.sw_sno);
	mess->service = sis_net_memory_get_sds(in_, mess->switchs.sw_service);
	mess->cmd = sis_net_memory_get_sds(in_, mess->switchs.sw_cmd);
	mess->subject = sis_net_memory_get_sds(in_, mess->switchs.sw_subject);
	mess->tag = sis_net_memory_get_int(in_, mess->switchs.sw_tag);
	// 这里可以增加列表数据的存储
	switch (mess->tag)
	{
	case SIS_NET_TAG_CHARS:
	case SIS_NET_TAG_BYTES:
		/* code */
		break;	
	default:
		mess->info = sis_net_memory_get_sds(in_, mess->switchs.sw_info);
		break;
	}
	
	// if (mess->switchs.has_argvs)
	// {
	// 	size_t count = sis_memory_get_ssize(in_);
	// 	if (!mess->argvs)
	// 	{
	// 		mess->argvs = sis_pointer_list_create();
	// 		mess->argvs->vfree = sis_object_decr;
	// 	}
	// 	for (int i = 0; i < count; i++)
	// 	{
	// 		size_t size = sis_memory_get_ssize(in_);
	// 		s_sis_sds val = sis_sdsnewlen(sis_memory(in_), size);
	// 		sis_memory_move(in_, size);
	// 		s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, val);
	// 		sis_object_incr(obj);
	// 		sis_pointer_list_push(mess->argvs, obj);
	// 		sis_object_destroy(obj);
	// 	}
	// }
	// 最后处理扩展字段问题
	if (mess->switchs.sw_more)
	{
		
	}
	sis_memory_jumpto(in_, start);
    return true;
}
