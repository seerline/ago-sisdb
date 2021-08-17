#include "sis_modules.h"
#include "worker.h"

#include "sis_notice.h"
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sis_notice_methods[] = {
    {"sub",   sis_notice_sub, 0, NULL},
    {"unsub", sis_notice_unsub, 0, NULL},
    {"pub",   sis_notice_pub, 0, NULL},
};
// 共享内存数据库 不落盘
s_sis_modules sis_modules_sis_notice = {
    sis_notice_init,
    NULL,
    NULL,
    NULL,
    sis_notice_uninit,
    NULL,
    NULL,
    sizeof(sis_notice_methods) / sizeof(s_sis_method),
    sis_notice_methods,
};

//////////////////////////////////////////////////////////////////
//------------------------s_sisdb_sub_unit -----------------------//
//////////////////////////////////////////////////////////////////
int _set_sub_unit_key(s_sisdb_sub_unit *unit, s_sis_sds wkey)
{
    // 如果头有 * 需要去掉
    size_t isize = sis_sdslen(wkey);
    const char *ptr = (const char *)wkey;
    if (ptr[0] == '*' && ptr[1] == ',')
    {
        unit->sub_keys = sis_sdsnewlen(&ptr[2], isize - 2);
        return 1;
    }
    unit->sub_keys = sis_sdsdup(wkey);
    return 0;
}
uint8 _set_sub_unit_type(s_sisdb_sub_unit *unit, s_sis_sds key)
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
        
        if (sis_str_exist_ch(keys, sis_sdslen(keys), ",", 1) ||
            sis_str_exist_ch(sdbs, sis_sdslen(sdbs), ",", 1))
        {
            unit->sub_sdbs = sdbs;
            if (_set_sub_unit_key(unit, keys))
            {
                sis_sdsfree(keys);
                return SIS_SUB_MULKEY_CMP;
            }
            sis_sdsfree(keys);
            return SIS_SUB_MULKEY_MUL;
        }
        else
        {
            return SIS_SUB_MULKEY_ONE;
        }
    }
    // else
    {
        if (sis_str_exist_ch(key, ksize, ",", 1))
        {
            if (_set_sub_unit_key(unit, key))
            {
                return SIS_SUB_ONEKEY_CMP;
            }
            return SIS_SUB_ONEKEY_MUL;
        }
        else
        {
            return SIS_SUB_ONEKEY_ONE;
        }
    }
} 

s_sisdb_sub_unit *sisdb_sub_unit_create(s_sis_sds key_)
{
    s_sisdb_sub_unit *o = SIS_MALLOC(s_sisdb_sub_unit, o);
    o->netmsgs = sis_pointer_list_create();
    o->netmsgs->vfree = sis_net_message_decr;
    o->sub_type = _set_sub_unit_type(o, key_);
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


bool  sis_notice_init(void *worker_, void *node_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_notice_cxt *context = SIS_MALLOC(s_sis_notice_cxt, context);
    worker->context = context;
    context->sub_onekeys = sis_map_pointer_create_v(sisdb_sub_unit_destroy);
    context->sub_mulkeys = sis_map_pointer_create_v(sisdb_sub_unit_destroy);

    return true;
}
void  sis_notice_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_notice_cxt *context = (s_sis_notice_cxt *)worker->context;
    sis_map_pointer_destroy(context->sub_onekeys);
    sis_map_pointer_destroy(context->sub_mulkeys);
    sis_free(context);
}

static int sisdb_sub_notice(s_sis_map_pointer *map, s_sis_net_message *msg)
{
	s_sisdb_sub_unit *subunit = (s_sisdb_sub_unit *)sis_map_pointer_get(map, msg->key);
	if (!subunit)
	{
        subunit = sisdb_sub_unit_create(msg->key);
		sis_map_pointer_set(map, msg->key, subunit);
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

int sis_notice_sub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_notice_cxt *context = (s_sis_notice_cxt *)worker->context;
  // s_sis_worker *worker = (s_sis_worker *)worker_;
    s_sis_message *msg = (s_sis_message *)argv_;

    if (msg->key && sis_sdslen(msg->key) > 0)
    {
        if (sis_str_exist_ch(msg->key, sis_sdslen(msg->key), "*,", 2))
        {
            sisdb_sub_notice(context->sub_mulkeys, msg);
        }
        else
        {
            // 单键
            sisdb_sub_notice(context->sub_onekeys, msg);
        }
    }
    return SIS_METHOD_OK;
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

int sis_notice_unsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_notice_cxt *context = (s_sis_notice_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    // 直接取消某客户所有订阅 后续有需求再定制化
    sisdb_unsub_notice(context->sub_onekeys, msg->cid);
    sisdb_unsub_notice(context->sub_mulkeys, msg->cid);

    return SIS_METHOD_OK;
}

static void _make_notice_send(s_sis_notice_cxt *context, s_sis_net_message *inetmsg, s_sis_net_message *onetmsg)
{
    s_sis_net_message *newmsg = sis_net_message_create();

    sis_net_message_copy(inetmsg, newmsg, onetmsg->cid, onetmsg->name);

    // 这里暂时不处理格式转换问题 广播什么数据就发送什么数据
    // 需要转换时 根据数据表的结构 自动转换

    if (context->cb_net_message)
    {
        context->cb_net_message(context->cb_source, newmsg);
    }
}
static void _make_notice(s_sis_notice_cxt *context, s_sis_message *imsg)
{
    // 先处理单键值订阅
    {
        s_sisdb_sub_unit *subunit = (s_sisdb_sub_unit *)sis_map_pointer_get(context->sub_onekeys, imsg->key);
        // printf("subunit pub: %p %s\n", subunit, collect_->name);
        if (subunit)
        {
            for (int i = 0; i < subunit->netmsgs->count; i++)
            {
                s_sis_net_message *omsg = (s_sis_net_message *)sis_pointer_list_get(subunit->netmsgs, i);
                // printf("subunit one: %d  %s  format = %d\n", subunit->netmsgs->count, info->key, info->rfmt);
                _make_notice_send(context, imsg, omsg);
            }
        }
    }
    // 再处理多键值订阅 这个可能耗时 先这样处理
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(context->sub_mulkeys);
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
                if (sis_str_subcmp(imsg->key,  subunit->sub_keys, ',') >= 0)
                {
                    notice = true;
                }
                break;
            case SIS_SUB_ONEKEY_MUL:
                if (sis_str_subcmp_strict(imsg->key,  subunit->sub_keys, ',') >= 0)
                {
                    notice = true;
                }
                break;            
            case SIS_SUB_MULKEY_CMP:
                {
                    // 都是小串
                    char kname[128], sname[128]; 
                    int cmds = sis_str_divide(imsg->key, '.', kname, sname);
                    if (cmds == 2 && 
                        (!sis_strcasecmp(subunit->sub_keys, "*") || sis_str_subcmp(kname, subunit->sub_keys, ',') >= 0) &&
                        (!sis_strcasecmp(subunit->sub_sdbs, "*") || sis_str_subcmp_strict(sname, subunit->sub_sdbs, ',') >= 0))
                    {
                        notice = true;
                    }
                }                
                break;
            case SIS_SUB_MULKEY_MUL:
                {
                    // 都是小串
                    char kname[128], sname[128]; 
                    int cmds = sis_str_divide(imsg->key, '.', kname, sname);
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
                    // printf("subunit sdb: %d  %s  format = %d\n", subunit->netmsgs->count, info->key, info->rfmt);
                    _make_notice_send(context, imsg, omsg);
                }
            }
        }
        sis_dict_iter_free(di);
    }    
}

int sis_notice_pub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_notice_cxt *context = (s_sis_notice_cxt *)worker->context;

    s_sis_message *msg = (s_sis_message *)argv_;
    context->cb_source = sis_message_get(msg, "source");
    context->cb_net_message = sis_message_get_method(msg, "cb_net_message");

    // 根据输入数据生成广播数据然后通过回调返回 可使用线程来处理
    _make_notice(context, msg);

    return SIS_METHOD_OK;
}
