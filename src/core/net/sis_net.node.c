
#include "sis_net.node.h"
#include "sis_json.h"
#include "sis_math.h"
#include "sis_obj.h"
#include "os_str.h"



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
void _net_nodes_free_read(s_sis_net_nodes *nodes_)
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
	nodes_->rnums = 0;	
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
	sis_mutex_unlock(&nodes_->lock);
	return nodes_->wnums;
}

int sis_net_nodes_read(s_sis_net_nodes *nodes_, int readnums_)
{
	if (nodes_->rnums >= readnums_)
	{
		return nodes_->rnums;
	}
    sis_mutex_lock(&nodes_->lock);
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
	sis_mutex_unlock(&nodes_->lock);
	// printf("==3== lock ok. %lld :: %d\n", nodes_->nums, nodes_->sendnums);
	return 	nodes_->rnums;
}
void sis_net_nodes_free_read(s_sis_net_nodes *nodes_)
{
    sis_mutex_lock(&nodes_->lock);
	_net_nodes_free_read(nodes_);
    sis_mutex_unlock(&nodes_->lock);
}
int  sis_net_nodes_none(s_sis_net_nodes *nodes_)
{
	sis_mutex_lock(&nodes_->lock);
	int count = nodes_->rnums + nodes_->wnums;
	sis_mutex_unlock(&nodes_->lock);
	return count;
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
	return index_;
}


////////////////////////////////////////////////////////
//  解码函数
////////////////////////////////////////////////////////

bool _net_encoded_chars(s_sis_net_message *in_, s_sis_memory *out_)
{
    s_sis_json_node *node = sis_json_create_object();
	if (in_->switchs.has_ver) 
	{
		sis_json_object_set_int(node, "ver", in_->ver);
	}
	if (in_->switchs.has_fmt) 
	{
		sis_json_object_set_int(node, "fmt", in_->format);
	}
	if (in_->switchs.is_reply)
	{
		// 必须有 ans 不然字符模式下无法判断是应答包
		if (in_->switchs.has_ans)
		{
			sis_json_object_set_int(node, "ans", in_->rans); 
		}
		else
		{
			// 如果没有ans就返回一个为0的ans 字符通讯包以ans判断是否应答包
			sis_json_object_set_string(node, "ans", "0", 1); 
		}
		if (in_->switchs.has_cmd)
		{
			sis_json_object_set_string(node, "cmd", in_->cmd, SIS_SDS_SIZE(in_->cmd)); 
		}		
		if (in_->switchs.has_key)
		{
			sis_json_object_set_string(node, "key", in_->key, SIS_SDS_SIZE(in_->key)); 
		}	
		if (in_->switchs.has_msg)
		{
			sis_json_object_set_string(node, "msg", in_->rmsg, SIS_SDS_SIZE(in_->rmsg)); 
		}		
		if (in_->switchs.has_next)
		{
			sis_json_object_set_int(node, "next", in_->rnext); 
		}		
	}
	else
	{
		if (in_->switchs.has_service)
		{
			sis_json_object_set_string(node, "service", in_->service, SIS_SDS_SIZE(in_->service)); 
		}		
		if (in_->switchs.has_cmd)
		{
			sis_json_object_set_string(node, "cmd", in_->cmd, SIS_SDS_SIZE(in_->cmd)); 
		}		
		if (in_->switchs.has_key)
		{
			sis_json_object_set_string(node, "key", in_->key, SIS_SDS_SIZE(in_->key)); 
		}		
		if (in_->switchs.has_ask)
		{
			sis_json_object_set_string(node, "ask", in_->ask, SIS_SDS_SIZE(in_->ask)); 
		}		
	}
	if (in_->switchs.has_argvs) 
	{
		// 这里以后有需求再补
	}
	// printf("_net_encoded_chars: %x %d %s \n%s \n%s \n%s \n%s \n", 
	// 		in_->style, 
	// 		(int)in_->rcmd,
	// 		in_->serial? in_->serial : "nil",
	// 		in_->cmd ? in_->cmd : "nil",
	// 		in_->key? in_->key : "nil",
	// 		in_->val? in_->val : "nil",
	// 		in_->rval? in_->rval : "nil"
	// 		);

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
	sis_memory_cat(out_, "B", 1); 
	sis_memory_cat(out_, (char *)&in_->switchs, sizeof(s_sis_net_switch));
	sis_net_int_set_memory(out_, in_->switchs.has_ver, in_->ver);
	sis_net_int_set_memory(out_, in_->switchs.has_fmt, in_->format);
	if (in_->switchs.is_reply)
	{
		sis_net_int_set_memory(out_, in_->switchs.has_ans, in_->rans);
		sis_net_str_set_memory(out_, in_->switchs.has_cmd, in_->cmd, SIS_SDS_SIZE(in_->cmd));
		sis_net_str_set_memory(out_, in_->switchs.has_key, in_->key, SIS_SDS_SIZE(in_->key));
		sis_net_str_set_memory(out_, in_->switchs.has_msg, in_->rmsg, SIS_SDS_SIZE(in_->rmsg));
		sis_net_int_set_memory(out_, in_->switchs.has_next, in_->rnext);
	}
	else
	{
		sis_net_str_set_memory(out_, in_->switchs.has_service, in_->service, SIS_SDS_SIZE(in_->service));
		sis_net_str_set_memory(out_, in_->switchs.has_cmd, in_->cmd, SIS_SDS_SIZE(in_->cmd));
		sis_net_str_set_memory(out_, in_->switchs.has_key, in_->key, SIS_SDS_SIZE(in_->key));
		sis_net_str_set_memory(out_, in_->switchs.has_ask, in_->ask, SIS_SDS_SIZE(in_->ask));
	}
	if (in_->switchs.has_argvs)
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
	s_sis_json_node *ver = sis_json_cmp_child_node(node_, "ver");  
	if (ver)
	{
        mess->ver = sis_json_get_int(node_, "ver", 0);    
		mess->switchs.has_ver = 1;
	}
	s_sis_json_node *fmt = sis_json_cmp_child_node(node_, "fmt");  
	if (fmt)
	{
        mess->format = sis_json_get_int(node_, "fmt", 0);   
		mess->switchs.has_fmt = 1;
	}
    s_sis_json_node *rans = sis_json_cmp_child_node(node_, "ans");
	if (rans)
	{
		// 应答包
		mess->switchs.is_reply = 1;
		mess->switchs.has_ans = rans->type == SIS_JSON_STRING ? 0 : 1;
		mess->rans = rans->value ? sis_atoll(rans->value) : 0; 	
        mess->cmd = _sis_json_node_get_sds(node_, "cmd");    
		mess->switchs.has_cmd = mess->cmd ? 1 : 0;
        mess->key = _sis_json_node_get_sds(node_, "key");    
		mess->switchs.has_key = mess->key ? 1 : 0;
        mess->rmsg = _sis_json_node_get_sds(node_, "msg");    
		mess->switchs.has_msg = mess->rmsg ? 1 : 0;
        mess->rnext = sis_json_get_int(node_, "next", 0);   
		mess->switchs.has_next = mess->rnext > 0 ? 1 : 0;
	}
	else
	{
		// 表示为请求
		mess->switchs.is_reply = 0;
		mess->switchs.has_ans = 0;
        mess->service = _sis_json_node_get_sds(node_, "service");    
		mess->switchs.has_service = mess->service ? 1 : 0;
        mess->cmd = _sis_json_node_get_sds(node_, "cmd");    
		mess->switchs.has_cmd = mess->cmd ? 1 : 0;
        mess->key = _sis_json_node_get_sds(node_, "key");    
		mess->switchs.has_key = mess->key ? 1 : 0;
        mess->ask = _sis_json_node_get_sds(node_, "ask");    
		mess->switchs.has_ask = mess->ask ? 1 : 0;
	}
	s_sis_json_node *argvs = sis_json_cmp_child_node(node_, "argvs");  
	if (argvs)
	{
		// mess->switchs.has_argvs = 1;
		// 这里以后有需求再补
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
static inline int sis_net_memory_get_int(s_sis_memory *imem_, int isok_)
{
	int o = 0;
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
	if (*sis_memory(in_) != 'B')
	{
		mess->format = SIS_NET_FORMAT_CHARS;
		bool o = sis_net_decoded_chars(in_, out_);
		sis_memory_jumpto(in_, start);
		return o;
	}
	mess->format = SIS_NET_FORMAT_BYTES;
	sis_memory_move(in_, 1);
	memmove(&mess->switchs, sis_memory(in_), sizeof(s_sis_net_switch));
	sis_memory_move(in_, sizeof(s_sis_net_switch));
	mess->ver = sis_net_memory_get_int(in_, mess->switchs.has_ver);
	if (mess->switchs.has_fmt)
	{
		// 没有字段用默认的格式 或上层指定
		mess->format = sis_net_memory_get_int(in_, mess->switchs.has_fmt);
	}
	if (mess->switchs.is_reply)
	{
		mess->rans = sis_net_memory_get_int(in_, mess->switchs.has_ans);
		mess->cmd = sis_net_memory_get_sds(in_, mess->switchs.has_cmd);
		mess->key = sis_net_memory_get_sds(in_, mess->switchs.has_key);
		mess->rmsg = sis_net_memory_get_sds(in_, mess->switchs.has_msg);
		mess->rnext = sis_net_memory_get_int(in_, mess->switchs.has_next);
	}
	else
	{
		mess->service = sis_net_memory_get_sds(in_, mess->switchs.has_service);
		mess->cmd = sis_net_memory_get_sds(in_, mess->switchs.has_cmd);
		mess->key = sis_net_memory_get_sds(in_, mess->switchs.has_key);
		mess->ask = sis_net_memory_get_sds(in_, mess->switchs.has_ask);
	}
	if (mess->switchs.has_argvs)
	{
		size_t count = sis_memory_get_ssize(in_);
		if (!mess->argvs)
		{
			mess->argvs = sis_pointer_list_create();
			mess->argvs->vfree = sis_object_decr;
		}
		for (int i = 0; i < count; i++)
		{
			size_t size = sis_memory_get_ssize(in_);
			s_sis_sds val = sis_sdsnewlen(sis_memory(in_), size);
			sis_memory_move(in_, size);
			s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, val);
			sis_object_incr(obj);
			sis_pointer_list_push(mess->argvs, obj);
			sis_object_destroy(obj);
		}
	}
	sis_memory_jumpto(in_, start);
    return true;
}
