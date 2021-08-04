
#include "sis_net.uv.h"
#include "sis_obj.h"
#include "os_str.h"
#include "sis_math.h"

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
s_sis_net_uv_nodes *sis_net_uv_nodes_create(int maxsends_)
{
    s_sis_net_uv_nodes *o = SIS_MALLOC(s_sis_net_uv_nodes, o);
	sis_mutex_init(&o->lock, NULL);
	o->maxsends = maxsends_ < 1024 ? 1024 : maxsends_;
	o->sendnums = 0; 
	o->readbuff = sis_malloc(sizeof(uv_buf_t) * o->maxsends);
	memset(o->readbuff, 0, sizeof(uv_buf_t) * o->maxsends);
	return o;
}
void _net_nodes_free_read(s_sis_net_uv_nodes *nodes_)
{
	if (nodes_->rhead)
	{
		s_sis_net_node *next = nodes_->rhead;	
		while (next)
		{
			s_sis_net_node *node = next->next;
			sis_net_node_destroy(next);
			next = node;
		}
		nodes_->rhead = NULL;
	}
	nodes_->rtail = NULL;	
	nodes_->sendnums = 0;	
}
void sis_net_uv_nodes_destroy(s_sis_net_uv_nodes *nodes_)
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
	sis_free(nodes_->readbuff);
    sis_mutex_unlock(&nodes_->lock);
	sis_mutex_destroy(&nodes_->lock);
	sis_free(nodes_);
}
void sis_net_uv_nodes_clear(s_sis_net_uv_nodes *nodes_)
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
	nodes_->nums = 0;
    sis_mutex_unlock(&nodes_->lock);
}
int  sis_net_uv_nodes_push(s_sis_net_uv_nodes *nodes_, s_sis_object *obj_)
{
	s_sis_net_node *newnode = sis_net_node_create(obj_);
	sis_mutex_lock(&nodes_->lock);
	nodes_->nums++;
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
	sis_mutex_unlock(&nodes_->lock);
	return nodes_->nums;
}

int  sis_net_uv_nodes_read(s_sis_net_uv_nodes *nodes_)
{
	if (nodes_->sendnums >= nodes_->maxsends)
	{
		return nodes_->sendnums;
	}
    sis_mutex_lock(&nodes_->lock);
	s_sis_net_node *next = nodes_->whead;	
	while (next)
	{
		nodes_->readbuff[nodes_->sendnums].base = SIS_OBJ_GET_CHAR(next->obj);
		nodes_->readbuff[nodes_->sendnums].len = SIS_OBJ_GET_SIZE(next->obj);
		nodes_->sendnums++;

		nodes_->nums--;

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
		if (nodes_->sendnums >= nodes_->maxsends)
		{
			break;
		}
	}
	sis_mutex_unlock(&nodes_->lock);
	// printf("==3== lock ok. %lld :: %d\n", nodes_->nums, nodes_->sendnums);
	return 	nodes_->sendnums;
}
void sis_net_uv_nodes_free_read(s_sis_net_uv_nodes *nodes_)
{
    sis_mutex_lock(&nodes_->lock);
	_net_nodes_free_read(nodes_);
    sis_mutex_unlock(&nodes_->lock);
}


///////////////////////////////////////////////////////////////////////////
//----------------------s_sis_net_list --------------------------------//
//  以整数为索引 存储指针的列表
//  如果有wait就说明释放index需要等待一个超时
///////////////////////////////////////////////////////////////////////////
s_sis_net_list *sis_net_list_create(void *vfree_)
{
	s_sis_net_list *o = (s_sis_net_list *)sis_malloc(sizeof(s_sis_net_list));
	o->max_count = 1024;
	o->used = sis_malloc(o->max_count);
	memset(o->used, SIS_NET_NOUSE ,o->max_count);
	o->stop_sec = sis_malloc(sizeof(time_t) * o->max_count);
	memset(o->stop_sec, 0 ,sizeof(time_t) * o->max_count);
	o->buffer = sis_malloc(sizeof(void *) * o->max_count);
	memset(o->buffer, 0 ,sizeof(void *) * o->max_count);
	o->vfree = vfree_;
	o->wait_sec = 180; // 默认3分钟后释放资源
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
			list_->vfree(ptr[i]);
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

void _index_list_free(s_sis_net_list *list_, int index_)
{
	char **ptr = (char **)list_->buffer;
	if (list_->vfree && ptr[index_])
	{
		list_->vfree(ptr[index_]);
		ptr[index_] = NULL;
	}
	list_->stop_sec[index_] = 0;
}

int sis_net_list_new(s_sis_net_list *list_, void *in_)
{
	time_t now_sec = sis_time_get_now();
	int index = -1;
	for (int i = 0; i < list_->max_count; i++)
	{
		if (list_->used[i] == SIS_NET_NOUSE)
		{
			index = i;
			break;
		}
		if (list_->wait_sec > 0 && list_->used[i] == SIS_NET_CLOSE && 
			(now_sec - list_->stop_sec[i]) > list_->wait_sec)
		{
			_index_list_free(list_, i);
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
	if (list_->used[index_] != SIS_NET_USEED)
	{
		return -1;
	}
	if (list_->wait_sec > 0)
	{
		list_->stop_sec[index_] = sis_time_get_now();
		list_->used[index_] = SIS_NET_CLOSE;
		list_->cur_count--;
	}
	else
	{
		_index_list_free(list_, index_);
		list_->used[index_] = SIS_NET_NOUSE;
		list_->cur_count--;
	}
	return index_;
}

//////////////////////////////////////////////////////////////
// s_sis_socket_session
//////////////////////////////////////////////////////////////

s_sis_socket_session *sis_socket_session_create(void *father_)
{
	s_sis_socket_session *session = SIS_MALLOC(s_sis_socket_session, session); 
	session->sid = -1;
	session->father = father_;
	session->uv_w_handle.data = session;
	session->uv_r_buffer = uv_buf_init((char*)sis_malloc(MAX_NET_UV_BUFFSIZE), MAX_NET_UV_BUFFSIZE);
	session->send_nodes = sis_net_uv_nodes_create(64 * 1024);
	return session;	
}
void sis_socket_session_set_rwcb(s_sis_socket_session *session_,
								cb_socket_recv_after cb_recv_, 
								cb_socket_send_after cb_send_)
{
	session_->cb_recv_after = cb_recv_;
	session_->cb_send_after = cb_send_;
}

void sis_socket_session_destroy(void *session_)
{
	s_sis_socket_session *session = (s_sis_socket_session *)session_;
	if (session->uv_r_buffer.base)
	{
		sis_free(session->uv_r_buffer.base);
	}
	session->uv_r_buffer.base = NULL;
	session->uv_r_buffer.len = 0;
	sis_net_uv_nodes_destroy(session->send_nodes);
	sis_free(session);
}

void sis_socket_session_init(s_sis_socket_session *session_)
{
	session_->sid = -1;
	memset(&session_->uv_w_handle, 0, sizeof(uv_tcp_t));
	session_->uv_w_handle.data = session_;
	sis_net_uv_nodes_clear(session_->send_nodes);
}
