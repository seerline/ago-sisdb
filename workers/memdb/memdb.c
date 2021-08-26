#include "sis_modules.h"
#include "worker.h"

#include "memdb.h"
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method memdb_methods[] = {
  {"open",   memdb_open,  0, NULL},
  {"close",  memdb_close, 0, NULL},
  {"get",    memdb_get,   0, NULL},
  {"set",    memdb_set,   0, NULL},
  {"sub",    memdb_sub,   0, NULL},
  {"hsub",   memdb_hsub,  0, NULL},
  {"pub",    memdb_pub,   0, NULL},
  {"unsub",  memdb_unsub, 0, NULL},
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



bool  memdb_init(void *worker_, void *node_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 

    s_memdb_cxt *context = SIS_MALLOC(s_memdb_cxt, context);
    worker->context = context;

	context->work_name = sis_sdsnew(worker->workername);

    context->work_sub_cxt = sisdb_sub_cxt_create();
    context->work_keys = sis_map_sds_create();

    context->status = 0; 
    return true;
}
void  memdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    if (context->work_keys)
    {
        sis_map_sds_destroy(context->work_keys);
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
int memdb_open(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    
	context->cb_source = sis_message_get(msg, "cb_source");
	context->cb_net_message = sis_message_get_method(msg, "cb_net_message");
    
    sis_map_sds_clear(context->work_keys);
    sisdb_sub_cxt_clear(context->work_sub_cxt);

    sisdb_sub_cxt_init(context->work_sub_cxt, context->cb_source, context->cb_net_message);

    context->status = 1;
    
    return SIS_METHOD_OK;
}
int memdb_close(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    sis_map_sds_clear(context->work_keys);
    sisdb_sub_cxt_clear(context->work_sub_cxt);
    return SIS_METHOD_OK;
}
int memdb_get(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    s_sis_sds info = sis_map_sds_get(context->work_keys, netmsg->key);
    if (!info)
    {
        sis_net_ans_with_null(netmsg);
    }
    else
    {
        sis_net_ans_with_bytes(netmsg, info, sis_sdslen(info));
    }
    return SIS_METHOD_OK;
}
int memdb_set(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    sis_map_sds_set(context->work_keys, netmsg->key, netmsg->ask);

    return SIS_METHOD_OK;
}
int memdb_sub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    int o = sisdb_sub_cxt_sub(context->work_sub_cxt, netmsg);
    sis_net_ans_with_int(netmsg, o);
    return SIS_METHOD_OK;
}

int memdb_hsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    int o = sisdb_sub_cxt_hsub(context->work_sub_cxt, netmsg);
    sis_net_ans_with_int(netmsg, o);
    return SIS_METHOD_OK;

}
int memdb_pub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    sisdb_sub_cxt_pub(context->work_sub_cxt, netmsg);
    return SIS_METHOD_OK;
}
int memdb_unsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_memdb_cxt *context = (s_memdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    int o = sisdb_sub_cxt_unsub(context->work_sub_cxt, netmsg->cid);
    sis_net_ans_with_int(netmsg, o);
     return SIS_METHOD_OK;
}