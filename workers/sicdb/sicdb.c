#include "sis_modules.h"
#include "worker.h"

#include "sicdb.h"
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sicdb_methods[] = {
    {"open",   cmd_sicdb_open,   SIS_METHOD_ACCESS_NONET, NULL},
    {"close",  cmd_sicdb_close,  SIS_METHOD_ACCESS_NONET, NULL},
    {"start",  cmd_sicdb_start,  SIS_METHOD_ACCESS_RDWR, NULL},   // 开始发送数据 
    {"stop",   cmd_sicdb_stop,   SIS_METHOD_ACCESS_RDWR, NULL},   // 数据流发布完成 此时不再接收数据
    {"get",    cmd_sicdb_get,    SIS_METHOD_ACCESS_READ, NULL},
    {"set",    cmd_sicdb_set,    SIS_METHOD_ACCESS_RDWR, NULL},
    {"sub",    cmd_sicdb_sub,    SIS_METHOD_ACCESS_READ, NULL},
    {"hsub",   cmd_sicdb_hsub,   SIS_METHOD_ACCESS_READ, NULL},
    {"pub",    cmd_sicdb_pub,    SIS_METHOD_ACCESS_RDWR, NULL},
    {"zpub",   cmd_sicdb_zpub,   SIS_METHOD_ACCESS_NONET, NULL},  // 发布数据流 只有完成的压缩数据块
    {"unsub",  cmd_sicdb_unsub,  SIS_METHOD_ACCESS_READ, NULL},
};
// 共享内存数据库 不落盘
s_sis_modules sis_modules_sicdb = {
  sicdb_init,
  NULL,
  NULL,
  NULL,
  sicdb_uninit,
  NULL,
  NULL,
  sizeof(sicdb_methods)/sizeof(s_sis_method),
  sicdb_methods,
};

s_sicdb_unit *sicdb_unit_create(s_sis_net_message *netmsg)
{
    if (netmsg->switchs.has_argvs)
    {
        s_sicdb_unit *o = SIS_MALLOC(s_sicdb_unit, o);
        o->style = 1;
        o->obj = sis_pointer_list_get(netmsg->argvs, 0);
        sis_object_incr(o->obj);
    }
    else if (netmsg->ask)
    {
        s_sicdb_unit *o = SIS_MALLOC(s_sicdb_unit, o);
        o->obj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(netmsg->ask));
        return o;
    }
    return NULL;
}
void sicdb_unit_destroy(void *unit_)
{
    s_sicdb_unit *unit = (s_sicdb_unit *)unit_;
    sis_object_decr(unit->obj);
    sis_free(unit);
}

bool  sicdb_init(void *worker_, void *node_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 

    s_sicdb_cxt *context = SIS_MALLOC(s_sicdb_cxt, context);
    worker->context = context;

	context->work_name = sis_sdsnew(worker->workername);

    context->work_sub_cxt = sisdb_sub_cxt_create();
    context->work_keys = sis_map_pointer_create();
    context->work_keys->type->vfree = sicdb_unit_destroy;

    context->status = 0; 
    return true;
}
void  sicdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sicdb_cxt *context = (s_sicdb_cxt *)worker->context;
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
int cmd_sicdb_open(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sicdb_cxt *context = (s_sicdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    
	context->cb_source = sis_message_get(msg, "cb_source");
	context->cb_net_message = sis_message_get_method(msg, "cb_net_message");
    
    sis_map_pointer_clear(context->work_keys);
    sisdb_sub_cxt_clear(context->work_sub_cxt);

    sisdb_sub_cxt_init(context->work_sub_cxt, context->cb_source, context->cb_net_message);

    context->status = 1;
    // sis_net_ans_with_ok(netmsg);
    return SIS_METHOD_OK;
}
int cmd_sicdb_close(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sicdb_cxt *context = (s_sicdb_cxt *)worker->context;
    // s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    sis_map_pointer_clear(context->work_keys);
    sisdb_sub_cxt_clear(context->work_sub_cxt);
    // sis_net_ans_with_ok(netmsg);
    return SIS_METHOD_OK;
}
int cmd_sicdb_start(void *worker_, void *argv_)
{
    // s_sis_worker *worker = (s_sis_worker *)worker_; 
    // s_sicdb_cxt *context = (s_sicdb_cxt *)worker->context;
    return SIS_METHOD_OK;
}
int cmd_sicdb_stop(void *worker_, void *argv_)
{
    // s_sis_worker *worker = (s_sis_worker *)worker_; 
    // s_sicdb_cxt *context = (s_sicdb_cxt *)worker->context;
    // sis_net_ans_with_ok(netmsg);
    return SIS_METHOD_OK;
}
int cmd_sicdb_get(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sicdb_cxt *context = (s_sicdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    s_sicdb_unit *unit = sis_map_pointer_get(context->work_keys, netmsg->key);
    if (!unit)
    {
        sis_net_ans_with_null(netmsg);
    }
    else
    {
        if (unit->style == 1)
        {
            sis_net_ans_with_bytes(netmsg, SIS_OBJ_GET_CHAR(unit->obj), SIS_OBJ_GET_SIZE(unit->obj));
        }
        else
        {
            sis_net_ans_with_chars(netmsg, SIS_OBJ_GET_CHAR(unit->obj), SIS_OBJ_GET_SIZE(unit->obj));
        }
    }
    return SIS_METHOD_OK;
}
int cmd_sicdb_set(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sicdb_cxt *context = (s_sicdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    s_sicdb_unit *unit = sicdb_unit_create(netmsg);
    if (unit)
    {
        sis_map_pointer_set(context->work_keys, netmsg->key, unit);
        sis_net_ans_with_ok(netmsg);
    }
    else
    {
        sis_net_ans_with_error(netmsg, "no data.", 0);
    }
    return SIS_METHOD_OK;
}
int cmd_sicdb_sub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sicdb_cxt *context = (s_sicdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    int o = sisdb_sub_cxt_sub(context->work_sub_cxt, netmsg);
    sis_net_ans_with_int(netmsg, o);
    return SIS_METHOD_OK;
}

int cmd_sicdb_hsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sicdb_cxt *context = (s_sicdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    int o = sisdb_sub_cxt_hsub(context->work_sub_cxt, netmsg);
    sis_net_ans_with_int(netmsg, o);
    return SIS_METHOD_OK;

}
int cmd_sicdb_pub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sicdb_cxt *context = (s_sicdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    sisdb_sub_cxt_pub(context->work_sub_cxt, netmsg);
    sis_net_ans_with_ok(netmsg);
    return SIS_METHOD_OK;
}
// int _snodb_write_bits(s_snodb_cxt *snodb_, s_snodb_compress *in_)
// {
// 	if (!snodb_->inited || snodb_->stoped)
// 	{
// 		return -1;
// 	}
// 	// 直接写入
// 	size_t size = sizeof(s_snodb_compress) + in_->size;
// 	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create_size(size));
// 	s_snodb_compress *memory = MAP_SNODB_BITS(obj);
// 	memmove(memory, in_, size);
// 	sis_memory_set_size(SIS_OBJ_MEMORY(obj), size);
// 	// printf("_snodb_write_bits = %d %d | %d %d\n", snodb_->inited , snodb_->stoped, memory->init,memory->size);
// 	if (memory->init == 1)
// 	{
// 		snodb_->last_object = obj;
// 		// printf("set last obj = %p\n", obj);
// 	}
// 	// printf("push 3 outmem->size = %d\n", MAP_SNODB_BITS(obj)->size);
// 	sis_lock_list_push(snodb_->outputs, obj);
// 	if ((snodb_->zipnums++) % 100 == 0)
// 	{
// 		LOG(8)("zpub nums = %d %d\n", snodb_->zipnums, snodb_->outputs->users->count);
// 	}
// 	sis_object_destroy(obj);
// 	return 0;
// }
int cmd_sicdb_zpub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sicdb_cxt *context = (s_sicdb_cxt *)worker->context;
    s_sis_db_incrzip *imem = (s_sis_db_incrzip *)argv_;
    
    // size_t size = sizeof(s_snodb_compress) + in_->size;
	// s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create_size(size));
	
    // sisdb_sub_cxt_pub(context->work_sub_cxt, netmsg);
    // sis_net_ans_with_ok(netmsg);
    return SIS_METHOD_OK;
}
int cmd_sicdb_unsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sicdb_cxt *context = (s_sicdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    int o = sisdb_sub_cxt_unsub(context->work_sub_cxt, netmsg->cid);
    sis_net_ans_with_int(netmsg, o);
     return SIS_METHOD_OK;
}