#include "sis_modules.h"
#include "worker.h"

#include "memdb.h"
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method memdb_methods[] = {
    {"init",   cmd_memdb_init,  0, NULL},
    {"get",    cmd_memdb_get,   0, NULL},
    {"set",    cmd_memdb_set,   0, NULL},
    {"sub",    cmd_memdb_sub,   0, NULL},
    {"hsub",   cmd_memdb_hsub,  0, NULL},
    {"pub",    cmd_memdb_pub,   0, NULL},
    {"unsub",  cmd_memdb_unsub, 0, NULL},
};
// 共享内存数据库 不落盘
s_sis_modules sis_modules_memdb = {
  memdb_init,
  NULL,
  NULL,
  NULL,
  memdb_uninit,
  NULL,
  NULL,
  sizeof(memdb_methods)/sizeof(s_sis_method),
  memdb_methods,
};

s_memdb_unit *memdb_unit_create(s_sis_net_message *netmsg)
{
    if (netmsg->info)
    {
        s_memdb_unit *o = SIS_MALLOC(s_memdb_unit, o);
        o->obj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(netmsg->info));
        return o;
    }
    return NULL;
}
void memdb_unit_destroy(void *unit_)
{
    s_memdb_unit *unit = (s_memdb_unit *)unit_;
    sis_object_decr(unit->obj);
    sis_free(unit);
}

bool  memdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_memdb_cxt *context = SIS_MALLOC(s_memdb_cxt, context);
    worker->context = context;

	const char *workname = sis_json_get_str(node, "work-name");
	if (workname)
	{
		context->work_name = sis_sdsnew(workname);
	}
	else
	{
		context->work_name = sis_sdsnew(node->key);
	}    
    context->work_sub_cxt = sisdb_sub_cxt_create();
    context->work_keys = sis_map_pointer_create();
    context->work_keys->type->vfree = memdb_unit_destroy;

    context->status = 0; 
    return true;
}
void  memdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    if (context->work_keys)
    {
        sis_map_pointer_destroy(context->work_keys);
        context->work_keys = NULL;
    }
    if (context->work_sub_cxt)
    {
        sisdb_sub_cxt_destroy(context->work_sub_cxt);
        context->work_sub_cxt = NULL;
    }
    sis_sdsfree(context->work_name);
    sis_free(context);
}
int cmd_memdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    
	context->cb_source = sis_message_get(msg, "cb_source");
	context->cb_net_message = sis_message_get_method(msg, "cb_net_message");
    
    sis_map_pointer_clear(context->work_keys);
    sisdb_sub_cxt_clear(context->work_sub_cxt);

    sisdb_sub_cxt_init(context->work_sub_cxt, context->cb_source, context->cb_net_message);

    context->status = 1;
    // sis_net_msg_tag_ok(netmsg);
    return SIS_METHOD_OK;
}

int cmd_memdb_get(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    s_memdb_unit *unit = sis_map_pointer_get(context->work_keys, netmsg->subject);
    if (!unit)
    {
        sis_net_msg_tag_null(netmsg);
    }
    else
    {
        if (unit->style == 1)
        {
            sis_net_message_set_byte(netmsg, SIS_OBJ_GET_CHAR(unit->obj), SIS_OBJ_GET_SIZE(unit->obj));
        }
        else
        {
            sis_net_message_set_char(netmsg, SIS_OBJ_GET_CHAR(unit->obj), SIS_OBJ_GET_SIZE(unit->obj));
        }
    }
    return SIS_METHOD_OK;
}
int cmd_memdb_set(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    s_memdb_unit *unit = memdb_unit_create(netmsg);
    if (unit)
    {
        sis_map_pointer_set(context->work_keys, netmsg->subject, unit);
        sis_net_msg_tag_ok(netmsg);
    }
    else
    {
        sis_net_msg_tag_error(netmsg, "no data.", 0);
    }
    return SIS_METHOD_OK;
}
int cmd_memdb_sub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    int o = sisdb_sub_cxt_sub(context->work_sub_cxt, netmsg);
    sis_net_msg_tag_int(netmsg, o);
    return SIS_METHOD_OK;
}

int cmd_memdb_hsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    int o = sisdb_sub_cxt_hsub(context->work_sub_cxt, netmsg);
    sis_net_msg_tag_int(netmsg, o);
    return SIS_METHOD_OK;

}
int cmd_memdb_pub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    sisdb_sub_cxt_pub(context->work_sub_cxt, netmsg);
    // SIS_NET_SHOW_MSG("memdb pub", netmsg);
    sis_net_msg_set_inside(netmsg);
    return SIS_METHOD_OK;
}

int cmd_memdb_unsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    int o = sisdb_sub_cxt_unsub(context->work_sub_cxt, netmsg->cid);
    sis_net_msg_tag_int(netmsg, o);
     return SIS_METHOD_OK;
}