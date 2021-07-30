
#include "sis_net.node.h"
#include "sis_json.h"
#include "sis_math.h"
#include "sis_obj.h"
#include "os_str.h"

// static s_sis_net_errinfo _rinfo[] = {
// 	{ SIS_NET_ANS_INVALID, "ask is invalid."},
// 	{ SIS_NET_ANS_NOAUTH,  "need auth."},
// 	{ SIS_NET_ANS_NIL,     "ask data is nil."},
// 	{ SIS_NET_ANS_ERROR,   "noknown error."},
// };

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
        sis_net_message_clear(in_);
		sis_free(in_);	
    } 
    else 
    {
        in->refs--;
    }	
}

void sis_net_message_clear(s_sis_net_message *in_)
{
	in_->cid = -1;
	in_->ver = 0;
	// refs 不能改
	sis_sdsfree(in_->name);
	in_->name = NULL;

	in_->comp = 0;
	if(in_->memory)
	{
		sis_memory_clear(in_->memory);
	}
	in_->format = SIS_NET_FORMAT_CHARS;
	memset(&in_->switchs, 0, sizeof(s_sis_net_switch));
	if(in_->argvs)
	{
		sis_pointer_list_destroy(in_->argvs);
		in_->argvs = NULL;
	}
	sis_sdsfree(in_->service);
	in_->service = NULL;
	sis_sdsfree(in_->cmd);
	in_->cmd = NULL;
	sis_sdsfree(in_->key);
	in_->key = NULL;
	sis_sdsfree(in_->ask);
	in_->ask = NULL;
	in_->rans = 0;
	sis_sdsfree(in_->rmsg);
	in_->rmsg = NULL;
	in_->rfmt = SIS_NET_FORMAT_CHARS;
}
static size_t _net_message_list_size(s_sis_pointer_list *list_)
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
	size += msg_->name ? sis_sdslen(msg_->name) : 0;
	size += msg_->service ? sis_sdslen(msg_->service) : 0;
	size += msg_->cmd ? sis_sdslen(msg_->cmd) : 0;
	size += msg_->key ? sis_sdslen(msg_->key) : 0;
	size += msg_->ask ? sis_sdslen(msg_->ask) : 0;
	size += msg_->rmsg ? sis_sdslen(msg_->rmsg) : 0;
	size += msg_->memory ? sis_memory_get_maxsize(msg_->memory) : 0;
	size += _net_message_list_size(msg_->argvs);
	return size;
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
		sis_json_object_set_int(node, "ans", in_->rans); 
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
		sis_net_int_set_memory(out_, in_->switchs.is_reply, in_->rans);
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
bool sis_net_decoded_chars(s_sis_memory *in_, s_sis_net_message *out_)
{
    s_sis_net_message *mess = (s_sis_net_message *)out_;
    s_sis_json_handle *handle = sis_json_load(sis_memory(in_), sis_memory_get_size(in_));
    if (!handle)
    {
        LOG(5)("json parse error.\n");
        return false;
    }
	s_sis_json_node *ver = sis_json_cmp_child_node(handle->node, "ver");  
	if (ver)
	{
        mess->ver = sis_json_get_int(handle->node, "ver", 0);    
		mess->switchs.has_ver = 1;
	}
	s_sis_json_node *fmt = sis_json_cmp_child_node(handle->node, "fmt");  
	if (fmt)
	{
        mess->format = sis_json_get_int(handle->node, "fmt", 0);   
		mess->switchs.has_fmt = 1;
	}
    s_sis_json_node *rans = sis_json_cmp_child_node(handle->node, "ans");
	if (rans)
	{
		// 应答包
		mess->switchs.is_reply = 1;
		mess->rans = rans->value ? sis_atoll(rans->value) : 0; 	
        mess->rmsg = _sis_json_node_get_sds(handle->node, "msg");    
		mess->switchs.has_msg = mess->rmsg ? 1 : 0;
        mess->rnext = sis_json_get_int(handle->node, "next", 0);   
		mess->switchs.has_next = mess->rnext > 0 ? 1 : 0;
	}
	else
	{
		// 表示为请求
		mess->switchs.is_reply = 0;
        mess->service = _sis_json_node_get_sds(handle->node, "service");    
		mess->switchs.has_service = mess->service ? 1 : 0;
        mess->cmd = _sis_json_node_get_sds(handle->node, "cmd");    
		mess->switchs.has_cmd = mess->cmd ? 1 : 0;
        mess->key = _sis_json_node_get_sds(handle->node, "key");    
		mess->switchs.has_key = mess->key ? 1 : 0;
        mess->ask = _sis_json_node_get_sds(handle->node, "ask");    
		mess->switchs.has_ask = mess->ask ? 1 : 0;
	}
	s_sis_json_node *argvs = sis_json_cmp_child_node(handle->node, "argvs");  
	if (argvs)
	{
		// mess->switchs.has_argvs = 1;
		// 这里以后有需求再补
	} 
	// 
	// printf("sis_net_decoded_chars:%x %llx %s \n%s \n%s \n%s \n%s \n", mess->style, mess->rcmd,
	// 		mess->serial? mess->serial : "nil",
	// 		mess->cmd ? mess->cmd : "nil",
	// 		mess->key? mess->key : "nil",
	// 		mess->val? mess->val : "nil",
	// 		mess->rval? mess->rval : "nil");

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
		mess->rans = sis_net_memory_get_int(in_, mess->switchs.is_reply);
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
