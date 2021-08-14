#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"

#include "sisdb_flog.h"
#include "sis_net.node.h"

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sisdb_flog_methods[] = {
    {"sub",     cmd_sisdb_flog_sub,    0, NULL},  
    {"unsub",   cmd_sisdb_flog_unsub,  0, NULL}, 
    {"open",    cmd_sisdb_flog_open,    0, NULL},   
    {"write",   cmd_sisdb_flog_write,   0, NULL},  
    {"close",   cmd_sisdb_flog_close,   0, NULL},   
    {"move",    cmd_sisdb_flog_move,    0, NULL},  
};
// 共享内存数据库
s_sis_modules sis_modules_sisdb_flog = {
    sisdb_flog_init,
    NULL,
    NULL,
    NULL,
    sisdb_flog_uninit,
    NULL,
    NULL,
    sizeof(sisdb_flog_methods) / sizeof(s_sis_method),
    sisdb_flog_methods,
};

bool sisdb_flog_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_flog_cxt *context = SIS_MALLOC(s_sisdb_flog_cxt, context);
    worker->context = context;

    context->work_date = sis_time_get_idate(0);
    {
        s_sis_json_node *sonnode = sis_json_cmp_child_node(node, "work-path");
        if (sonnode)
        {
            context->work_path = sis_sdsnew(sonnode->value);
        }
        else
        {
            context->work_path = sis_sdsnew("data/");
        }  
    }
    {
        s_sis_json_node *sonnode = sis_json_cmp_child_node(node, "work-name");
        if (sonnode)
        {
            context->work_name = sis_sdsnew(sonnode->value);
        }
        else
        {
            context->work_name = sis_sdsnew("wlog");
        }  
    } 
    return true;
}

void sisdb_flog_stop(s_sis_worker *worker)
{
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)worker->context;
    if (context->status == SIS_FLOG_READ)
    {
        cmd_sisdb_flog_unsub(worker, NULL);
    }
    if (context->status == SIS_FLOG_WRITE)
    {
        cmd_sisdb_flog_close(worker, NULL);
    }
}

void sisdb_flog_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)worker->context;

    sisdb_flog_stop(worker);

    sis_sdsfree(context->work_path);
    sis_sdsfree(context->work_name);
    sis_free(context);
}
//////////////////////////////////////
//  sub log callback
//////////////////////////////////////
static void cb_start(void *source_, int idate)
{
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)source_;
    if (context->cb_sub_start)
    {
        char sdate[32];
        sis_lldtoa(idate, sdate, 32, 10);
        context->cb_sub_start(context->cb_source, sdate);
    }
}
static void cb_stop(void *source_, int idate)
{
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)source_;
    if (context->cb_sub_stop)
    {
        char sdate[32];
        sis_lldtoa(idate, sdate, 32, 10);
        context->cb_sub_stop(context->cb_source, sdate);
    }
}
static void cb_original(void *source_, s_sis_disk_head *head_, void *out_, size_t olen_)
{
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)source_;
    if (context->cb_netmsg)
    {
        s_sis_net_message *netmsg = sis_net_message_create();
        s_sis_memory *memory = sis_memory_create_size(olen_);
        sis_memory_cat(memory, out_, olen_);
        sis_net_decoded_normal(memory, netmsg);
        context->cb_netmsg(context->cb_source, netmsg);
        sis_memory_destroy(memory);
        sis_net_message_destroy(netmsg);
    }
    // 这里通过回调把数据传递出去 
} 
int cmd_sisdb_flog_sub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)worker->context;
    if (context->status != SIS_FLOG_NONE)
    {
        return SIS_METHOD_ERROR;
    }
    s_sis_message *msg = (s_sis_message *)argv_;
    {
        s_sis_sds str = sis_message_get_str(msg, "work-path");
        if (str)
        {
            sis_sdsfree(context->work_path);
            context->work_path = sis_sdsdup(str);
        }
    }
    {
        s_sis_sds str = sis_message_get_str(msg, "work-name");
        if (str)
        {
            sis_sdsfree(context->work_name);
            context->work_name = sis_sdsdup(str);
        }
    }
    if (sis_message_exist(msg, "sub-date"))
    {
        context->work_date = sis_message_get_int(msg, "sub-date");
    }
    else
    {
        context->work_date = sis_time_get_idate(0);
    }
    context->cb_source =  sis_message_get(msg, "source");
    context->cb_sub_start =  sis_message_get_method(msg, "cb_sub_start");
    context->cb_sub_stop =  sis_message_get_method(msg, "cb_sub_stop");
    context->cb_netmsg =  sis_message_get_method(msg, "cb_netmsg");

    context->status = SIS_FLOG_READ;

    s_sis_disk_reader_cb *rlog_cb = SIS_MALLOC(s_sis_disk_reader_cb, rlog_cb);
    rlog_cb->cb_source = context;
    rlog_cb->cb_start = cb_start;
    rlog_cb->cb_original = cb_original;
    rlog_cb->cb_stop = cb_stop;
    rlog_cb->cb_break = cb_stop;

    context->reader = sis_disk_reader_create(context->work_path, context->work_name, SIS_DISK_TYPE_LOG, rlog_cb);

    LOG(5)("sub log open. [%d]\n", context->work_date);
    sis_disk_reader_sub_log(context->reader, context->work_date);
    LOG(5)("sub log stop. [%d]\n", context->work_date);

    sis_disk_reader_destroy(context->reader);
    context->reader = NULL;
    sis_free(rlog_cb);

    context->status = SIS_FLOG_NONE;

    return SIS_METHOD_OK;
}
int cmd_sisdb_flog_unsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)worker->context;
    if (context->reader)
    {
        sis_disk_reader_unsub(context->reader);
        while (context->status != SIS_FLOG_NONE)
        {
            sis_sleep(30);
        }
    } 
    return SIS_METHOD_OK;   
}
//////////////////////////////////////
//  write log callback
//////////////////////////////////////
int cmd_sisdb_flog_open(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)worker->context;

    if (context->status != SIS_FLOG_NONE)
    {
        return SIS_METHOD_ERROR;
    }
    s_sis_message *msg = (s_sis_message *)argv_;
    {
        s_sis_sds str = sis_message_get_str(msg, "work-path");
        if (str)
        {
            sis_sdsfree(context->work_path);
            context->work_path = sis_sdsdup(str);
        }
    }
    {
        s_sis_sds str = sis_message_get_str(msg, "work-name");
        if (str)
        {
            sis_sdsfree(context->work_name);
            context->work_name = sis_sdsdup(str);
        }
    }
    if (sis_message_exist(msg, "sub-date"))
    {
        context->work_date = sis_message_get_int(msg, "sub-date");
    }
    else
    {
        context->work_date = sis_time_get_idate(0);
    }
    context->status = SIS_FLOG_WRITE;
    context->writer = sis_disk_writer_create(context->work_path, context->work_name, SIS_DISK_TYPE_LOG);
    
    sis_disk_writer_open(context->writer, context->work_date);

    return SIS_METHOD_OK;
}

int cmd_sisdb_flog_close(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)worker->context;
    if (context->writer)
    {
        sis_disk_writer_close(context->writer);
        sis_disk_writer_destroy(context->writer);
        context->writer = NULL;
    }
    context->status = SIS_FLOG_NONE;

    return SIS_METHOD_OK;
}
int cmd_sisdb_flog_write(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    if (context->writer)
    {
        s_sis_memory *memory = sis_memory_create();
        sis_net_encoded_normal(netmsg, memory);
        sis_disk_writer_log(context->writer, sis_memory(memory), sis_memory_get_size(memory));
        sis_memory_destroy(memory);
    }
    return SIS_METHOD_OK;
}

int cmd_sisdb_flog_move(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)worker->context;
    if (context->status != SIS_FLOG_NONE)
    {
        return SIS_METHOD_ERROR;
    }
    s_sis_message *msg = (s_sis_message *)argv_;
    if (sis_message_exist(msg, "work-path") &&
        sis_message_exist(msg, "work-name") &&
        sis_message_exist(msg, "sub-date"))
    {
        sis_disk_control_move(
            sis_message_get_str(msg, "work-path"), 
            sis_message_get_str(msg, "work-name"), 
            SIS_DISK_TYPE_LOG, 
            sis_message_get_int(msg, "sub-date"));
    }
    return SIS_METHOD_OK;
}
