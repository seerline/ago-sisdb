#include "sis_modules.h"
#include "worker.h"

#include "sisdb_sub.h"

//////////////////////////////////////////////////////////////////
//------------------------s_sisdb_sub_unit -----------------------//
//////////////////////////////////////////////////////////////////
uint8 _set_sub_unit_type(s_sisdb_sub_unit *unit, s_sis_sds key, int ishead)
{
    if (!sis_strcasecmp(key, "*"))
    {
        return SIS_SUB_ONEKEY_ALL;
    }
    if (!sis_strcasecmp(key, "*.*"))
    {
        return SIS_SUB_MULKEY_ALL;
    }
    size_t ksize = sis_sdslen(key);
    if (sis_str_exist_ch(key, ksize, ".", 1))
    {
        s_sis_sds keys = NULL; s_sis_sds sdbs = NULL;
        sis_str_divide_sds(key, '.', &keys, &sdbs);
        unit->sub_sdbs = sdbs;
        unit->sub_keys = keys;
        
        if (ishead)
        {
            return SIS_SUB_MULKEY_CMP;
        }
        else
        {
            if (sis_str_exist_ch(keys, sis_sdslen(keys), ",", 1) ||
                sis_str_exist_ch(sdbs, sis_sdslen(sdbs), ",", 1))
            {
                return SIS_SUB_MULKEY_MUL;
            }
            else
            {
                return SIS_SUB_MULKEY_ONE;
            }
        }
    }
    else
    {
        unit->sub_keys = sis_sdsdup(key);
        if (ishead)
        {
            return SIS_SUB_ONEKEY_CMP;
        }
        else
        {
            if (sis_str_exist_ch(key, ksize, ",", 1))
            {
                return SIS_SUB_ONEKEY_MUL;
            }
            else
            {
                return SIS_SUB_ONEKEY_ONE;
            }
        }
    }
} 

s_sisdb_sub_unit *sisdb_sub_unit_create(s_sis_sds key_, int ishead)
{
    s_sisdb_sub_unit *o = SIS_MALLOC(s_sisdb_sub_unit, o);
    o->netmsgs = sis_pointer_list_create();
    o->netmsgs->vfree = sis_net_message_decr;
    o->sub_type = _set_sub_unit_type(o, key_, ishead);
    return o;
}

void sisdb_sub_unit_destroy(void *reader_)
{
    s_sisdb_sub_unit *o = (s_sisdb_sub_unit *)reader_;
    sis_sdsfree(o->sub_keys);
    sis_sdsfree(o->sub_sdbs);
    sis_pointer_list_destroy(o->netmsgs);
    sis_free(o);
}

s_sisdb_sub_cxt *sisdb_sub_cxt_create()
{
    s_sisdb_sub_cxt *context = SIS_MALLOC(s_sisdb_sub_cxt, context);
    context->sub_onekeys = sis_map_pointer_create_v(sisdb_sub_unit_destroy);
    context->sub_mulkeys = sis_map_pointer_create_v(sisdb_sub_unit_destroy);
    // printf("==2.2==%p=%p=\n", context->sub_onekeys, context->sub_mulkeys);
    return context;
}
void  sisdb_sub_cxt_destroy(s_sisdb_sub_cxt *cxt_)
{
    sis_map_pointer_destroy(cxt_->sub_onekeys);
    sis_map_pointer_destroy(cxt_->sub_mulkeys);
    sis_free(cxt_);
}
void  sisdb_sub_cxt_clear(s_sisdb_sub_cxt *cxt_)
{
    sis_map_pointer_clear(cxt_->sub_onekeys);
    sis_map_pointer_clear(cxt_->sub_mulkeys);
}
void  sisdb_sub_cxt_init(s_sisdb_sub_cxt *cxt_, void *source_, sis_method_define *cb_)
{
    cxt_->cb_source = source_;
    cxt_->cb_net_message = cb_;
}

static int sisdb_sub_notice(s_sis_map_pointer *map, s_sis_net_message *msg, int ishead)
{
	s_sisdb_sub_unit *subunit = (s_sisdb_sub_unit *)sis_map_pointer_get(map, msg->subject);
	if (!subunit)
	{
        subunit = sisdb_sub_unit_create(msg->subject, ishead);
		sis_map_pointer_set(map, msg->subject, subunit);
	}
	for (int i = 0; i < subunit->netmsgs->count; i++)
	{
		s_sis_net_message *info = (s_sis_net_message *)sis_pointer_list_get(subunit->netmsgs, i);
		if (info->cid ==  msg->cid)
		{
			return 0;
		}
	}
    sis_net_message_incr(msg);
    sis_pointer_list_push(subunit->netmsgs, msg);
    return 1;
}

int sisdb_sub_cxt_sub(s_sisdb_sub_cxt *cxt_, s_sis_net_message *netmsg_)
{
    int o = 0;
    if (netmsg_->subject && sis_sdslen(netmsg_->subject) > 0)
    {
        if (sis_str_exist_ch(netmsg_->subject, sis_sdslen(netmsg_->subject), "*,", 2))
        {
            o = sisdb_sub_notice(cxt_->sub_mulkeys, netmsg_, 0);
        }
        else
        {
            // 单键
            o = sisdb_sub_notice(cxt_->sub_onekeys, netmsg_, 0);
        }
    }
    return o;
}
int sisdb_sub_cxt_hsub(s_sisdb_sub_cxt *cxt_, s_sis_net_message *netmsg_)
{
    SIS_NET_SHOW_MSG("hsub === ", netmsg_);
    int o = 0;
    if (netmsg_->subject && sis_sdslen(netmsg_->subject) > 0)
    {
        o = sisdb_sub_notice(cxt_->sub_mulkeys, netmsg_, 1);
    }
    return o;
}
static int sisdb_unsub_notice(s_sis_map_pointer *map, int cid_)
{
    int count = 0;
    s_sis_dict_entry *de;
    s_sis_dict_iter *di = sis_dict_get_iter(map);
    while ((de = sis_dict_next(di)) != NULL)
    {
        s_sisdb_sub_unit *subunit = (s_sisdb_sub_unit *)sis_dict_getval(de);
        if (cid_ == -1)
        {
            count = subunit->netmsgs->count;
            sis_pointer_list_clear(subunit->netmsgs);
            sis_map_pointer_del(map, sis_dict_getkey(de));
        }
        else
        {
            for (int i = 0; i < subunit->netmsgs->count; )
            {
                s_sis_net_message *info = (s_sis_net_message *)sis_pointer_list_get(subunit->netmsgs, i);
                if (info->cid == cid_)
                {
                    sis_pointer_list_delete(subunit->netmsgs, i, 1);
                    count++;
                    if (subunit->netmsgs->count == 0)
                    {
                        sis_map_pointer_del(map, sis_dict_getkey(de));
                    }
                    break;
                }
                else
                {
                    i++;
                }
            }
        }
    }
    sis_dict_iter_free(di);  
    return count;
}

int sisdb_sub_cxt_unsub(s_sisdb_sub_cxt *cxt_, int cid_)
{
    // 直接取消某客户所有订阅 后续有需求再定制化
    int o = 0;
    o += sisdb_unsub_notice(cxt_->sub_onekeys, cid_);
    o += sisdb_unsub_notice(cxt_->sub_mulkeys, cid_);

    return o;
}

static void _make_notice_send(s_sisdb_sub_cxt *context, s_sis_net_message *inetmsg, s_sis_net_message *onetmsg)
{
    s_sis_net_message *newmsg = sis_net_message_create();

    sis_net_message_relay(inetmsg, newmsg, onetmsg->cid, onetmsg->name, inetmsg->service, inetmsg->cmd, inetmsg->subject);
    newmsg->format = SIS_NET_FORMAT_BYTES;
    // 这里暂时不处理格式转换问题 广播什么数据就发送什么数据
    // 需要转换时 根据数据表的结构 自动转换
    // printf("%p\n", context->cb_net_message);
    // SIS_NET_SHOW_MSG("notice:", newmsg);
    if (context->cb_net_message)
    {
        context->cb_net_message(context->cb_source, newmsg);
    }
    sis_net_message_destroy(newmsg);
}

int sisdb_sub_cxt_pub(s_sisdb_sub_cxt *cxt_, s_sis_net_message *netmsg_)
{
    // 先处理单键值订阅
    {
        s_sisdb_sub_unit *subunit = (s_sisdb_sub_unit *)sis_map_pointer_get(cxt_->sub_onekeys, netmsg_->subject);
        // printf("subunit pub: %p %s\n", subunit, collect_->name);
        if (subunit)
        {
            for (int i = 0; i < subunit->netmsgs->count; i++)
            {
                s_sis_net_message *omsg = (s_sis_net_message *)sis_pointer_list_get(subunit->netmsgs, i);
                // printf("subunit one: %d  %s  format = %d\n", subunit->netmsgs->count, omsg->key, omsg->rfmt);
                _make_notice_send(cxt_, netmsg_, omsg);
            }
        }
    }
    // 再处理多键值订阅 这个可能耗时 先这样处理
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(cxt_->sub_mulkeys);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_sub_unit *subunit = (s_sisdb_sub_unit *)sis_dict_getval(de);

            bool notice = false;
            switch (subunit->sub_type)
            {
            case SIS_SUB_ONEKEY_ALL:
            case SIS_SUB_MULKEY_ALL:
                notice = true;
                break;
            case SIS_SUB_ONEKEY_CMP:
                if (sis_str_subcmp(netmsg_->subject,  subunit->sub_keys, ',') >= 0)
                {
                    notice = true;
                }
                break;
            case SIS_SUB_ONEKEY_MUL:
                if (sis_str_subcmp_strict(netmsg_->subject,  subunit->sub_keys, ',') >= 0)
                {
                    notice = true;
                }
                break;            
            case SIS_SUB_MULKEY_CMP:
                {
                    // 都是小串
                    char kname[128], sname[128]; 
                    int cmds = sis_str_divide(netmsg_->subject, '.', kname, sname);
                    if (cmds == 2 && 
                        (!sis_strcasecmp(subunit->sub_keys, "*") || sis_str_subcmp(kname, subunit->sub_keys, ',') >= 0) &&
                        (!sis_strcasecmp(subunit->sub_sdbs, "*") || sis_str_subcmp(sname, subunit->sub_sdbs, ',') >= 0))
                    {
                        notice = true;
                    }
                }                
                break;
            case SIS_SUB_MULKEY_MUL:
                {
                    // 都是小串
                    char kname[128], sname[128]; 
                    int cmds = sis_str_divide(netmsg_->subject, '.', kname, sname);
                    if (cmds == 2 && 
                        (!sis_strcasecmp(subunit->sub_keys, "*") || sis_str_subcmp_strict(kname, subunit->sub_keys, ',') >= 0) &&
                        (!sis_strcasecmp(subunit->sub_sdbs, "*") || sis_str_subcmp_strict(sname, subunit->sub_sdbs, ',') >= 0))
                    {
                        notice = true;
                    }
                }
                break;                     
            default:
                break;
            }
            if (notice)
            {
                for (int i = 0; i < subunit->netmsgs->count; i++)
                {
                    s_sis_net_message *omsg = (s_sis_net_message *)sis_pointer_list_get(subunit->netmsgs, i);
                    // printf("subunit sdb: %d  %s  format = %d\n", subunit->netmsgs->count, omsg->key, omsg->rfmt);
                    _make_notice_send(cxt_, netmsg_, omsg);
                }
            }
        }
        sis_dict_iter_free(di);
    }    
    return SIS_METHOD_OK;
}
