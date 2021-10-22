
#include <sis_net.msg.h>
#include <sis_malloc.h>
#include <sis_net.node.h>

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
    if (in_->map)
    {
        sis_map_pointer_clear(in_->map); 
    }
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
// 只拷贝数据和key
void sis_net_message_copy(s_sis_net_message *agomsg_, s_sis_net_message *newmsg_, int cid_, s_sis_sds name_)
{
    newmsg_->cid = cid_;
    newmsg_->ver = agomsg_->ver;
    newmsg_->name = name_ ? sis_sdsdup(name_) : NULL;

    newmsg_->format = agomsg_->format;
    memmove(&newmsg_->switchs, &agomsg_->switchs, sizeof(s_sis_net_switch));

    if (agomsg_->argvs && agomsg_->argvs->count > 0)
    {
        if (!newmsg_->argvs)
        {
            newmsg_->argvs = sis_pointer_list_create();
            newmsg_->argvs->vfree = sis_object_decr;
        }
        else
        {
            sis_pointer_list_clear(newmsg_->argvs);
        }
        for (int i = 0; i < agomsg_->argvs->count; i++)
        {
            s_sis_object *obj = sis_pointer_list_get(agomsg_->argvs, i);
            sis_object_incr(obj);
            sis_pointer_list_push(newmsg_->argvs, obj);
        }
    }
    newmsg_->service = agomsg_->service ? sis_sdsdup(agomsg_->service) : NULL;
    newmsg_->cmd = agomsg_->cmd ? sis_sdsdup(agomsg_->cmd) : NULL;
    newmsg_->key = agomsg_->key ? sis_sdsdup(agomsg_->key) : NULL;
    newmsg_->ask = agomsg_->ask ? sis_sdsdup(agomsg_->ask) : NULL;

    newmsg_->rans = agomsg_->rans;
    newmsg_->rnext = agomsg_->rnext;
    newmsg_->rmsg = agomsg_->rmsg ? sis_sdsdup(agomsg_->rmsg) : NULL;
    newmsg_->rfmt = agomsg_->rfmt;
   
}

#define SIS_NET_SET_STR(w,s,f) { w = f ? 1 : 0; sis_sdsfree(s); s = f ? sis_sdsnew(f) : NULL; }
#define SIS_NET_SET_BUF(w,s,f,l) { w = (f && l > 0) ? 1 : 0; sis_sdsfree(s); s = (f && l > 0) ? sis_sdsnewlen(f,l) : NULL; }

void sis_net_message_publish(s_sis_net_message *agomsg_, s_sis_net_message *newmsg_, int cid_, s_sis_sds name_, s_sis_sds cmd_, s_sis_sds key_)
{
    newmsg_->cid = cid_;
    newmsg_->name = name_ ? sis_sdsdup(name_) : NULL;

    SIS_NET_SET_STR(newmsg_->switchs.has_cmd, newmsg_->cmd, cmd_);
    SIS_NET_SET_STR(newmsg_->switchs.has_key, newmsg_->key, key_);
    // newmsg_->key = key_ ? sis_sdsdup(key_) : NULL;    
    // newmsg_->switchs.has_key = newmsg_->key ? 1 : 0;
    
    newmsg_->switchs.is_reply = agomsg_->switchs.is_reply;
    newmsg_->switchs.has_ans = agomsg_->switchs.has_ans;
    newmsg_->switchs.fmt_msg = agomsg_->switchs.fmt_msg;
    newmsg_->switchs.fmt_argv = agomsg_->switchs.fmt_argv;
    if (agomsg_->ask)
    {
        newmsg_->ask = agomsg_->ask ? sis_sdsdup(agomsg_->ask) : NULL;
        newmsg_->switchs.has_ask = newmsg_->ask ? 1 : 0;
    } 
    if (agomsg_->rmsg)
    {
        newmsg_->rans = agomsg_->rans;
        newmsg_->rnext = agomsg_->rnext;
        newmsg_->rfmt = agomsg_->rfmt;
        
        newmsg_->rmsg = agomsg_->rmsg ? sis_sdsdup(agomsg_->rmsg) : NULL;
        newmsg_->switchs.has_msg= newmsg_->rmsg ? 1 : 0;
        newmsg_->switchs.is_reply = 1; // 强制判断一下 如果有 rmsg 默认为应答包
    } 
    if (agomsg_->argvs && agomsg_->argvs->count > 0)
    {
        newmsg_->format = SIS_NET_FORMAT_BYTES;
        newmsg_->switchs.has_argvs = 1;
        if (!newmsg_->argvs)
        {
            newmsg_->argvs = sis_pointer_list_create();
            newmsg_->argvs->vfree = sis_object_decr;
        }
        else
        {
            sis_pointer_list_clear(newmsg_->argvs);
        }
        for (int i = 0; i < agomsg_->argvs->count; i++)
        {
            s_sis_object *obj = sis_pointer_list_get(agomsg_->argvs, i);
            sis_object_incr(obj);
            sis_pointer_list_push(newmsg_->argvs, obj);
        }        
    }
}

////////////////////////////////////////////////////////
//  s_sis_net_message 操作类函数
////////////////////////////////////////////////////////

static inline void sis_net_new_argvs(s_sis_net_message *netmsg_,  s_sis_object *obj_, int clear_)
{
	if (!netmsg_->argvs)
	{
		netmsg_->argvs = sis_pointer_list_create();
		netmsg_->argvs->vfree = sis_object_decr;
	}
    if (clear_)
    {
        sis_pointer_list_clear(netmsg_->argvs);
    }
    sis_pointer_list_push(netmsg_->argvs, obj_);    
}

void sis_message_set_key(s_sis_net_message *netmsg_, const char *kname_, const char *sname_)
{
    sis_sdsfree(netmsg_->key);
    netmsg_->key = NULL;
    if (kname_ || sname_)
    {
        if (sname_)
        {
            netmsg_->key =sis_sdsempty();
            netmsg_->key = sis_sdscatfmt(netmsg_->key, "%s.%s", kname_, sname_);
        }
        else
        {
            netmsg_->key = sis_sdsnew(kname_);
        }
    }
    netmsg_->switchs.has_key = netmsg_->key ? 1 : 0;
}
void sis_message_set_cmd(s_sis_net_message *netmsg_, const char *cmd_)
{
    sis_sdsfree(netmsg_->service);
    sis_sdsfree(netmsg_->cmd);
    netmsg_->service = NULL;
    netmsg_->cmd = NULL;
    if (cmd_)
    {
        s_sis_sds service = NULL;
        s_sis_sds command = NULL; 
        int cmds = sis_str_divide_sds(cmd_, '.', &service, &command);
        if (cmds == 2)
        {
            netmsg_->service = service;
            netmsg_->cmd = command;
        }
        else
        {
            netmsg_->cmd = service;
        }
    }
    netmsg_->switchs.has_service = netmsg_->service ? 1 : 0;
    netmsg_->switchs.has_cmd = netmsg_->cmd ? 1 : 0;
}
void sis_message_set_ans(s_sis_net_message *netmsg_, int ans_, int isclear_)
{
    if (isclear_)
    {
        netmsg_->rans = 0;
        netmsg_->switchs.has_ans = 0;
    }
    else
    {
        netmsg_->rans = ans_;
        netmsg_->switchs.has_ans = 1;
    }
    netmsg_->switchs.is_reply = 1;
}
void sis_message_set_argvs(s_sis_net_message *netmsg_, const char *in_, size_t ilen_, int isclear_)
{
    netmsg_->format = SIS_NET_FORMAT_BYTES;
    netmsg_->switchs.has_argvs = 1;
    s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnewlen(in_, ilen_));
    sis_net_new_argvs(netmsg_, obj, isclear_);
}
void sis_message_set_object(s_sis_net_message *netmsg_, void *obj_, int isclear_)
{
    netmsg_->format = SIS_NET_FORMAT_BYTES;
    netmsg_->switchs.has_argvs = 1;
    s_sis_object *obj = (s_sis_object *)obj_;
    sis_object_incr(obj);
    sis_net_new_argvs(netmsg_, obj, isclear_);
}

// 下面直接指定是应答还是请求包
void sis_net_ask_with_chars(s_sis_net_message *netmsg_, 
    char *cmd_, char *key_, char *val_, size_t vlen_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 0;
    netmsg_->switchs.has_ans = 0;
    sis_message_set_cmd(netmsg_, cmd_);
    SIS_NET_SET_STR(netmsg_->switchs.has_key, netmsg_->key, key_);
    SIS_NET_SET_BUF(netmsg_->switchs.has_ask, netmsg_->ask, val_, vlen_);
}
void sis_net_ask_with_bytes(s_sis_net_message *netmsg_, char *val_, size_t vlen_)
{
    netmsg_->format = SIS_NET_FORMAT_BYTES;
    netmsg_->switchs.is_reply = 0;
    netmsg_->switchs.has_ans = 0;
    netmsg_->switchs.has_argvs = 1;
    s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnewlen(val_, vlen_));
	sis_net_new_argvs(netmsg_, obj, 1);
}

// 应答包
void sis_net_ans_with_chars(s_sis_net_message *netmsg_, const char *in_, size_t ilen_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 1;
    // netmsg_->rans = SIS_NET_ANS_OK; // 若没有 has_ans 默认为 OK
    // netmsg_->switchs.has_ans = 1;
    SIS_NET_SET_BUF(netmsg_->switchs.has_msg, netmsg_->rmsg, in_, ilen_);
}

void sis_net_ans_with_bytes(s_sis_net_message *netmsg_, const char *in_, size_t ilen_)
{
    netmsg_->format = SIS_NET_FORMAT_BYTES;
    netmsg_->switchs.is_reply = 1;
    // netmsg_->rans = SIS_NET_ANS_OK; // 若没有 has_ans 默认为 OK
    // netmsg_->switchs.has_ans = 1;
    netmsg_->switchs.has_argvs = 1;
    s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnewlen(in_, ilen_));
	sis_net_new_argvs(netmsg_, obj, 1);
}

s_sis_sds sis_net_get_val(s_sis_net_message *netmsg_)
{
    if (netmsg_->switchs.is_reply)
    {
        return netmsg_->rmsg;
    }
    return netmsg_->ask;
}

s_sis_sds sis_net_get_argvs(s_sis_net_message *netmsg_, int index)
{
    if (!netmsg_ || !netmsg_->argvs)
    {
        return NULL;
    }
    s_sis_object *obj = sis_pointer_list_get(netmsg_->argvs, index);
    if (!obj)
    {
        return NULL;
    }
    return SIS_OBJ_SDS(obj);
}
void sis_net_ans_with_noreply(s_sis_net_message *netmsg_)
{
    netmsg_->switchs.is_inside = 1;
}

void sis_net_ans_with_int(s_sis_net_message *netmsg_, int iint_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 1;
    // netmsg_->rans = SIS_NET_ANS_OK;  // 若没有 has_ans 默认为 OK
    // netmsg_->switchs.has_ans = 1;
    sis_sdsfree(netmsg_->rmsg);
    netmsg_->rmsg = sis_sdsnewlong(iint_);
    netmsg_->switchs.has_msg = 1;
}
void sis_net_ans_with_ok(s_sis_net_message *netmsg_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 1;
    netmsg_->rans = SIS_NET_ANS_OK;
    netmsg_->switchs.has_ans = 1;
}
void sis_net_ans_with_error(s_sis_net_message *netmsg_, char *rval_, size_t vlen_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 1;
    netmsg_->rans = SIS_NET_ANS_ERROR;
    netmsg_->switchs.has_ans = 1;
    if (vlen_ == 0)
    {
        vlen_ = sis_strlen(rval_);
    }
    SIS_NET_SET_BUF(netmsg_->switchs.has_msg, netmsg_->rmsg, rval_, vlen_);
}
void sis_net_ans_with_null(s_sis_net_message *netmsg_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 1;
    netmsg_->rans = SIS_NET_ANS_NIL;
    netmsg_->switchs.has_ans = 1;
}
void sis_net_ans_with_sub_start(s_sis_net_message *netmsg_, const char *info_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 1;
    netmsg_->rans = SIS_NET_ANS_SUB_OPEN;
    netmsg_->switchs.has_ans = 1;
    SIS_NET_SET_STR(netmsg_->switchs.has_msg, netmsg_->rmsg, info_);
}
void sis_net_ans_with_sub_wait(s_sis_net_message *netmsg_, const char *info_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 1;
    netmsg_->rans = SIS_NET_ANS_SUB_WAIT;
    netmsg_->switchs.has_ans = 1;
    SIS_NET_SET_STR(netmsg_->switchs.has_msg, netmsg_->rmsg, info_);
}
void sis_net_ans_with_sub_stop(s_sis_net_message *netmsg_, const char *info_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 1;
    netmsg_->rans = SIS_NET_ANS_SUB_STOP;
    netmsg_->switchs.has_ans = 1;
    SIS_NET_SET_STR(netmsg_->switchs.has_msg, netmsg_->rmsg, info_);
}
