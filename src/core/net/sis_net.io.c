
#include <sis_net.io.h>
#include <sis_malloc.h>
#include <sis_net.node.h>

static inline void sis_net_set_cmd(s_sis_net_message *netmsg_, char *cmd_)
{
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
            netmsg_->service = NULL;
            netmsg_->cmd = service;
        }
    }
    else
    {
        netmsg_->service = NULL;
        netmsg_->cmd = NULL;
    }
    netmsg_->switchs.has_service = netmsg_->service ? 1 : 0;
    netmsg_->switchs.has_cmd = netmsg_->cmd ? 1 : 0;
}

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

#define SIS_NET_SET_STR(w,s,f) { w = f ? 1 : 0; sis_sdsfree(s); s = f ? sis_sdsnew(f) : NULL; }
#define SIS_NET_SET_BUF(w,s,f,l) { w = f ? 1 : 0; sis_sdsfree(s); s = f ? sis_sdsnewlen(f,l) : NULL; }

void sis_net_ask_with_chars(s_sis_net_message *netmsg_, 
    char *cmd_, char *key_, char *val_, size_t vlen_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 0;
    sis_net_set_cmd(netmsg_, cmd_);
    SIS_NET_SET_STR(netmsg_->switchs.has_key, netmsg_->key, key_);
    SIS_NET_SET_BUF(netmsg_->switchs.has_ask, netmsg_->ask, val_, vlen_);
}
void sis_net_ask_with_bytes(s_sis_net_message *netmsg_,
    char *cmd_, char *key_, char *val_, size_t vlen_)
{
    netmsg_->format = SIS_NET_FORMAT_BYTES;
    netmsg_->switchs.is_reply = 0;
    sis_net_set_cmd(netmsg_, cmd_);
    SIS_NET_SET_STR(netmsg_->switchs.has_key, netmsg_->key, key_);
    netmsg_->switchs.has_argvs = 1;
    s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnewlen(val_, vlen_));
	sis_net_new_argvs(netmsg_, obj, 1);
}

void sis_net_ask_with_argvs(s_sis_net_message *netmsg_, const char *in_, size_t ilen_)
{
    netmsg_->switchs.is_reply = 0;
    netmsg_->switchs.has_argvs = 1;
    s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnewlen(in_, ilen_));
    sis_net_new_argvs(netmsg_, obj, 0);
}

void sis_net_ans_with_chars(s_sis_net_message *netmsg_, const char *in_, size_t ilen_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 1;
    netmsg_->rans = SIS_NET_ANS_OK;
    SIS_NET_SET_BUF(netmsg_->switchs.has_msg, netmsg_->rmsg, in_, ilen_);
    netmsg_->switchs.has_key = netmsg_->key ? 1 : 0;
}
void sis_net_ans_set_key(s_sis_net_message *netmsg_, const char *kname_, const char *sname_)
{
    sis_sdsfree(netmsg_->key);
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
void sis_net_ans_with_bytes(s_sis_net_message *netmsg_, const char *in_, size_t ilen_)
{
    netmsg_->format = SIS_NET_FORMAT_BYTES;
    netmsg_->switchs.is_reply = 1;
    netmsg_->rans = SIS_NET_ANS_OK;
    SIS_NET_SET_BUF(netmsg_->switchs.has_msg, netmsg_->rmsg, in_, ilen_);
    netmsg_->switchs.has_key = netmsg_->key ? 1 : 0;

    netmsg_->switchs.has_argvs = 1;
    // 可能in来源于argvs 所以先拷贝
    s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnewlen(in_, ilen_));
    sis_net_new_argvs(netmsg_, obj, 1);
}
void sis_net_ans_with_argvs(s_sis_net_message *netmsg_, const char *in_, size_t ilen_)
{
    netmsg_->format = SIS_NET_FORMAT_BYTES;
    netmsg_->switchs.is_reply = 1;
    netmsg_->rans = SIS_NET_ANS_OK;
    SIS_NET_SET_BUF(netmsg_->switchs.has_msg, netmsg_->rmsg, in_, ilen_);
    netmsg_->switchs.has_key = netmsg_->key ? 1 : 0;

    netmsg_->switchs.has_argvs = 1;
    s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnewlen(in_, ilen_));
    sis_net_new_argvs(netmsg_, obj, 0);
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
    netmsg_->rans = SIS_NET_ANS_OK;
    char sint[32];
    sis_lldtoa(iint_, sint, 32, 10);
    SIS_NET_SET_STR(netmsg_->switchs.has_msg, netmsg_->rmsg, sint);
}
void sis_net_ans_with_ok(s_sis_net_message *netmsg_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 1;
    netmsg_->rans = SIS_NET_ANS_OK;
}
void sis_net_ans_with_error(s_sis_net_message *netmsg_, char *rval_, size_t vlen_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 1;
    netmsg_->rans = SIS_NET_ANS_ERROR;
    SIS_NET_SET_BUF(netmsg_->switchs.has_msg, netmsg_->rmsg, rval_, vlen_);
}
void sis_net_ans_with_null(s_sis_net_message *netmsg_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 1;
    netmsg_->rans = SIS_NET_ANS_NIL;
}
void sis_net_ans_with_sub_start(s_sis_net_message *netmsg_, const char *info_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 1;
    netmsg_->rans = SIS_NET_ANS_SUB_OPEN;
    SIS_NET_SET_STR(netmsg_->switchs.has_msg, netmsg_->rmsg, info_);
}
void sis_net_ans_with_sub_wait(s_sis_net_message *netmsg_, const char *info_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 1;
    netmsg_->rans = SIS_NET_ANS_SUB_WAIT;
    SIS_NET_SET_STR(netmsg_->switchs.has_msg, netmsg_->rmsg, info_);
}
void sis_net_ans_with_sub_stop(s_sis_net_message *netmsg_, const char *info_)
{
    netmsg_->format = SIS_NET_FORMAT_CHARS;
    netmsg_->switchs.is_reply = 1;
    netmsg_->rans = SIS_NET_ANS_SUB_STOP;
    SIS_NET_SET_STR(netmsg_->switchs.has_msg, netmsg_->rmsg, info_);
}
