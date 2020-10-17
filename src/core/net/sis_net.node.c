
#include "sis_net.node.h"
#include "sis_json.h"
#include "sis_math.h"
#include "sis_obj.h"
///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_net_message -----------------------------//
///////////////////////////////////////////////////////////////////////////

s_sis_net_message *sis_net_message_create()
{
	s_sis_net_message *o = (s_sis_net_message *)sis_malloc(sizeof(s_sis_net_message));
	memset(o, 0, sizeof(s_sis_net_message));
	o->refs = 1;
	return o;
}

void _sis_net_message_del(s_sis_net_message *in_)
{
	if(in_->argvs)
	{
		sis_pointer_list_destroy(in_->argvs);
	}
	sis_sdsfree(in_->source);
	sis_sdsfree(in_->cmd);
	sis_sdsfree(in_->key);
	sis_sdsfree(in_->val);
	sis_sdsfree(in_->rval);

	sis_free(in_);	
}
void sis_net_message_destroy(void *in_)
{
	sis_net_message_decr(in_);
}
void sis_net_message_incr(s_sis_net_message *in_)
{
    if (in_ && in_->refs != 0xFFFFFFFF) 
    {
        in_->refs++;
    }
}
void sis_net_message_decr(void *in_)
{
    s_sis_net_message *in = (s_sis_net_message *)in_;
    if (!in)
    {
        return ;
    }
    if (in->refs == 1) 
    {
        _sis_net_message_del(in);
    } 
    else 
    {
        in->refs--;
    }	
}

void sis_net_message_clear(s_sis_net_message *in_)
{
	sis_sdsfree(in_->source);
	sis_sdsfree(in_->cmd);
	sis_sdsfree(in_->key);
	sis_sdsfree(in_->val);
	sis_sdsfree(in_->rval);
	in_->source = NULL;
	in_->cmd = NULL;
	in_->key = NULL;
	in_->val = NULL;
	in_->rval = NULL;

	if(in_->argvs)
	{
		sis_pointer_list_clear(in_->argvs);
	}
	in_->cid = 0;
	in_->style = 0;
	in_->rcmd = 0;
	// refs 不能改
}
size_t _sis_net_message_list_size(s_sis_pointer_list *list_)
{
	size_t size = 0;
	if (list_)
	{
		size += list_->count * sizeof(void *);
		for (int i = 0; i < list_->count; i++)
		{
			s_sis_object *obj = (s_sis_object *)sis_pointer_list_get(list_, i);
			size += SIS_OBJ_GET_SIZE(obj);
		}
	}
	return size;
}
size_t sis_net_message_get_size(s_sis_net_message *msg_)
{
	size_t size = sizeof(s_sis_net_message);
	size += msg_->source ? sis_sdslen(msg_->source) : 0;
	size += msg_->cmd ? sis_sdslen(msg_->cmd) : 0;
	size += msg_->key ? sis_sdslen(msg_->key) : 0;
	size += msg_->val ? sis_sdslen(msg_->val) : 0;
	size += msg_->rval ? sis_sdslen(msg_->rval) : 0;
	size += _sis_net_message_list_size(msg_->argvs);
	return size;
}

void _sis_net_message_list_clone(s_sis_pointer_list *inlist_, s_sis_pointer_list *outlist_)
{
	for (int i = 0; i < inlist_->count; i++)
	{
		s_sis_object *obj = (s_sis_object *)sis_pointer_list_get(inlist_, i);
		sis_object_incr(obj);
		sis_pointer_list_push(outlist_, obj);
	}
}

s_sis_net_message *sis_net_message_clone(s_sis_net_message *in_)
{
	if (in_ == NULL)
	{
		return NULL;
	}
	s_sis_net_message *o = sis_net_message_create();
	o->cid = in_->cid;
	if (in_->source)
	{
		o->source = sis_sdsdup(in_->source);
	}
	o->style = in_->style;
	o->format = in_->format;
	if (in_->cmd)
	{
		o->cmd = sis_sdsdup(in_->cmd);
	}
	if (in_->key)
	{
		o->key = sis_sdsdup(in_->key);
	}
	if (in_->val)
	{
		o->val = sis_sdsdup(in_->val);
	}
	o->rcmd = in_->rcmd;
	if (in_->rval)
	{
		o->rval = sis_sdsdup(in_->rval);
	}
	if (in_->argvs)
	{
		o->argvs = sis_pointer_list_create();
		o->argvs->vfree = sis_object_decr;
		_sis_net_message_list_clone(in_->argvs, o->argvs);
	}
	return o;
}

// 写入其他数据 argv 是 s_sis_object 底层用法 用户自行发送信息 一般不用 encoded 编码
int sis_net_message_write_argv(s_sis_net_message *mess_, s_sis_object *val_)
{
	if (!mess_->argvs)
	{
		mess_->argvs = sis_pointer_list_create();
		mess_->argvs->vfree = sis_object_decr;
	}
	sis_object_incr(val_);
	sis_pointer_list_push(mess_->argvs, val_);
    return mess_->argvs->count;     
}
void sis_net_message_clear_argv(s_sis_net_message *mess_)
{
	sis_pointer_list_clear(mess_->argvs);
}
////////////////////////////////////////////////////////
//  解码函数
////////////////////////////////////////////////////////

bool sis_net_encoded_chars(s_sis_net_message *in_, s_sis_memory *out_)
{
    s_sis_json_node *node = sis_json_create_object();
	if (in_->style & SIS_NET_CMD && in_->cmd) 
	{
		sis_json_object_set_string(node, "cmd", in_->cmd, sis_sdslen(in_->cmd));
	}
	if (in_->style & SIS_NET_KEY && in_->key)
	{
		sis_json_object_set_string(node, "key", in_->key, sis_sdslen(in_->key));
	}
	if (in_->style & SIS_NET_ASK)
	{
		if (in_->style & SIS_NET_VAL && in_->val)
		{
			sis_json_object_set_jstr(node, "val", in_->val, sis_sdslen(in_->val));
		}
	}
	else
	{
		// if (in_->style & SIS_NET_RCMD)
		{
			// 必须有rcmd 不然字符模式下无法判断是应答包
			sis_json_object_set_int(node, "rcmd", in_->rcmd); 
		} 
		if (in_->style & SIS_NET_VAL && in_->rval)
		{
			sis_json_object_set_string(node, "rval", in_->rval, sis_sdslen(in_->rval)); 
		}		
	}
	// printf(":::%d %d %s \n%s \n%s \n%s \n%s \n", 
	printf(":::%d %d %s \n%s \n%s \n%s \n", 
			in_->style, 
			(int)in_->rcmd,
			in_->source? in_->source : "nil",
			in_->cmd ? in_->cmd : "nil",
			in_->key? in_->key : "nil",
			in_->val? in_->val : "nil"
			// ,in_->rval? in_->rval : "nil"
			);

    size_t len = 0;
    char *str = sis_json_output_zip(node, &len);
    // printf("sis_net_encoded_json [%d]: %d %s\n",in_->style, (int)len, str);
    sis_memory_cat(out_, str, len);
    sis_free(str);
    sis_json_delete_node(node);
    
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
    if (node->type == SIS_JSON_INT || node->type == SIS_JSON_DOUBLE || node->type == SIS_JSON_STRING)
    {
        o = sis_sdsnew(node->value);
    }
    else if (node->type == SIS_JSON_ARRAY || node->type == SIS_JSON_OBJECT)
    {
		size_t len = 0;
		char *str = sis_json_output_zip(node, &len);
		o = sis_sdsnewlen(str, len);
		sis_free(str);
    }
    return o;
}
bool sis_net_decoded_chars(s_sis_memory *in_, s_sis_net_message *out_)
{
    s_sis_net_message *mess = (s_sis_net_message *)out_;

    size_t size = sis_memory_get_size(in_);
	sis_memory(in_)[size] = 0;
    s_sis_json_handle *handle = sis_json_load(sis_memory(in_), size);
    if (!handle)
    {
        LOG(5)("json parse error.\n");
        return false;
    }
    s_sis_json_node *rcmd = sis_json_cmp_child_node(handle->node, "rcmd");
	if (rcmd)
	{
		// 应答包
		mess->style = SIS_NET_RCMD;
		mess->rcmd = rcmd->value ? sis_atoll(rcmd->value) : 0; 	
        mess->rval = _sis_json_node_get_sds(handle->node, "rval");    
		mess->style = mess->rval ? (mess->style | SIS_NET_VAL) : mess->style;
	}
	else
	{
		// 表示为请求
		mess->style = SIS_NET_ASK;
        mess->val = _sis_json_node_get_sds(handle->node, "val");    
		mess->style = mess->val ? (mess->style | SIS_NET_VAL) : mess->style;
	}
	mess->cmd = _sis_json_node_get_sds(handle->node, "cmd");
	mess->style = mess->cmd ? (mess->style | SIS_NET_CMD) : mess->style;
	mess->key = _sis_json_node_get_sds(handle->node, "key");
	mess->style = mess->key ? (mess->style | SIS_NET_KEY) : mess->style;
	printf(":::%x %s \n%s \n%s \n%s \n", mess->style,
			mess->source? mess->source : "nil",
			mess->cmd ? mess->cmd : "nil",
			mess->key? mess->key : "nil",
			mess->val? mess->val : "nil");

    sis_json_close(handle);
	if (!mess->style)
	{
		return false;
	}
    return true;
}

bool sis_net_encoded_normal(s_sis_net_message *in_, s_sis_memory *out_)
{
	// 编码 一般只写 argvs 中的数据
    sis_memory_clear(out_);
    if (in_->source)
    {
        sis_memory_cat(out_, in_->source, sis_sdslen(in_->source));
    }
    sis_memory_cat(out_, ":", 1);
	if (in_->format == SIS_NET_FORMAT_CHARS)
	{
		return sis_net_encoded_chars(in_, out_);
	}
	// 二进制流
	sis_memory_cat(out_, "B", 1); 
	sis_memory_cat_byte(out_, in_->style, 1);
	if (in_->style & SIS_NET_CMD) 
	{
		sis_memory_cat_ssize(out_, in_->cmd ? sis_sdslen(in_->cmd) : 0);
		if (in_->cmd)
		{
			sis_memory_cat(out_, in_->cmd, sis_sdslen(in_->cmd));
		}
	}
	if (in_->style & SIS_NET_KEY)
	{
		sis_memory_cat_ssize(out_, in_->key ? sis_sdslen(in_->key) : 0);
		if (in_->key)
		{
			sis_memory_cat(out_, in_->key, sis_sdslen(in_->key));
		}
	}
	if (in_->style & SIS_NET_VAL)
	{
		sis_memory_cat_ssize(out_, in_->val ? sis_sdslen(in_->val) : 0);
		if (in_->val)
		{
			sis_memory_cat(out_, in_->val, sis_sdslen(in_->val));
		}
	}
	if (in_->style & SIS_NET_ARGVS)
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
	}
    return true;
}
bool sis_net_decoded_normal(s_sis_memory *in_, s_sis_net_message *out_)
{
	size_t start = sis_memory_get_address(in_);
    int cursor = sis_str_pos(sis_memory(in_), sis_min((int)sis_memory_get_size(in_) ,64), ':');
    if (cursor < 0)
    {
        return false;
    }
    s_sis_net_message *mess = (s_sis_net_message *)out_;
    sis_net_message_clear(mess);
    if (cursor > 0)
    {
        mess->source = sis_sdsnewlen(sis_memory(in_), cursor);
    }
	sis_memory_move(in_, cursor + 1);

	if (*sis_memory(in_) != 'B')
	{
		out_->format = SIS_NET_FORMAT_CHARS;
		bool o = sis_net_decoded_chars(in_, out_);
		sis_memory_jumpto(in_, start);
		return o;
	}
	mess->format = SIS_NET_FORMAT_BYTES;
	sis_memory_move(in_, 1);
	mess->style = sis_memory_get_byte(in_, 1);
	if (mess->style & SIS_NET_CMD) 
	{
		size_t size = sis_memory_get_ssize(in_);
		if (size > 0)
		{
			mess->cmd = sis_sdsnewlen(sis_memory(in_), size);
		}
		sis_memory_move(in_, size);
	}
	if (mess->style & SIS_NET_KEY) 
	{
		size_t size = sis_memory_get_ssize(in_);
		if (size > 0)
		{
			mess->key = sis_sdsnewlen(sis_memory(in_), size);
		}
		sis_memory_move(in_, size);
	}
	if (mess->style & SIS_NET_VAL) 
	{
		size_t size = sis_memory_get_ssize(in_);
		if (size > 0)
		{
			mess->val = sis_sdsnewlen(sis_memory(in_), size);
		}
		sis_memory_move(in_, size);
	}
	if (mess->style & SIS_NET_ARGVS) 
	{
		size_t count = sis_memory_get_ssize(in_);
		for (int i = 0; i < count; i++)
		{
			size_t size = sis_memory_get_ssize(in_);
			s_sis_sds val = sis_sdsnewlen(sis_memory(in_), size);
			sis_memory_move(in_, size);
			s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, val);
			sis_net_message_write_argv(mess, obj);
			sis_object_destroy(obj);
		}
	}
	sis_memory_jumpto(in_, start);
    return true;
}
