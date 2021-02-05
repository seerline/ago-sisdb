#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"

#include "sisdb_wlog.h"

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sisdb_wlog_methods[] = {
    {"read",   cmd_sisdb_wlog_read,   0, NULL},   
    {"open",   cmd_sisdb_wlog_open,   0, NULL},   
    {"write",  cmd_sisdb_wlog_write,  0, NULL},  
    {"close",  cmd_sisdb_wlog_close,  0, NULL},   
    {"move",   cmd_sisdb_wlog_move,  0, NULL},  
    {"exist",  cmd_sisdb_wlog_exist,  0, NULL},  
};
// 共享内存数据库
s_sis_modules sis_modules_sisdb_wlog = {
    sisdb_wlog_init,
    NULL,
    NULL,
    NULL,
    sisdb_wlog_uninit,
    sisdb_wlog_method_init,
    sisdb_wlog_method_uninit,
    sizeof(sisdb_wlog_methods) / sizeof(s_sis_method),
    sisdb_wlog_methods,
};

bool sisdb_wlog_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_wlog_cxt *context = SIS_MALLOC(s_sisdb_wlog_cxt, context);
    worker->context = context;
    {
        s_sis_json_node *sonnode = sis_json_cmp_child_node(node, "work-path");
        if (sonnode)
        {
            context->work_path = sis_sdsnew(sonnode->value);
        }
        else
        {
            context->work_path = sis_sdsnew("data/wlog/");
        }
        if (!sis_path_exists(context->work_path))
        {
            sis_path_mkdir(context->work_path);
        }
    }    
    return true;
}
void sisdb_wlog_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wlog_cxt *context = (s_sisdb_wlog_cxt *)worker->context;
    if (context->wlog)
    {
        sis_disk_file_write_stop(context->wlog);
        sis_disk_class_destroy(context->wlog);  
    }

    sis_sdsfree(context->work_path);
    sis_free(context);
}
void sisdb_wlog_method_init(void *worker_)
{
}
void sisdb_wlog_method_uninit(void *worker_)
{
}

static void cb_begin(void *source_, msec_t idate)
{
    // printf("%s : %llu\n", __func__, idate);
    s_sisdb_wlog_cxt *context = (s_sisdb_wlog_cxt *)source_;
    if (context->cb_sub_start)
    {
        char sdate[32];
        sis_lldtoa(idate, sdate, 32, 10);
        context->cb_sub_start(context->cb_source, sdate);
    }
}
static void cb_read(void *source_, const char *key_, const char *sdb_, void *out_, size_t olen_)
{
    s_sisdb_wlog_cxt *context = (s_sisdb_wlog_cxt *)source_;
    if (context->cb_recv)
    {
        s_sis_net_message *netmsg = sis_net_message_create();
        // sis_out_binary("wlog read", out_, olen_);
        s_sis_memory *memory = sis_memory_create_size(olen_);
        sis_memory_cat(memory, out_, olen_);
        sis_net_decoded_normal(memory, netmsg);
        context->cb_recv(context->cb_source, netmsg);
        sis_memory_destroy(memory);
        sis_net_message_destroy(netmsg);
    }
    // 这里通过回调把数据传递出去 
} 
static void cb_end(void *source_, msec_t idate)
{
    // printf("%s : %llu\n", __func__, idate);
    s_sisdb_wlog_cxt *context = (s_sisdb_wlog_cxt *)source_;
    if (context->cb_sub_stop)
    {
        char sdate[32];
        sis_lldtoa(idate, sdate, 32, 10);
        context->cb_sub_stop(context->cb_source, sdate);
    }
}

int cmd_sisdb_wlog_read(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wlog_cxt *context = (s_sisdb_wlog_cxt *)worker->context;

    s_sis_message *message = (s_sis_message *)argv_;
    s_sis_sds dbname = sis_message_get_str(message, "dbname");

    s_sis_disk_class *rfile = sis_disk_class_create();
    char fn[255];
    sis_sprintf(fn, 255, "%s.log", dbname);
    sis_disk_class_init(rfile, SIS_DISK_TYPE_STREAM, context->work_path, fn);
    int ok = sis_disk_file_read_start(rfile);
    if (ok)
    {
        sis_disk_class_destroy(rfile);
        return SIS_METHOD_ERROR;
    }
    context->cb_source =  sis_message_get(message, "source");
    context->cb_sub_start =  sis_message_get_method(message, "cb_sub_start");
    context->cb_sub_stop =  sis_message_get_method(message, "cb_sub_stop");
    context->cb_recv =  sis_message_get_method(message, "cb_recv");

    s_sis_disk_callback cb;
    memset(&cb, 0, sizeof(s_sis_disk_callback));
    cb.source = context; 
    cb.cb_begin = cb_begin;
    cb.cb_end = cb_end;
    cb.cb_read = cb_read;
    s_sis_disk_reader *reader = sis_disk_reader_create(&cb); 

    sis_disk_file_read_sub(rfile, reader);

    sis_disk_reader_destroy(reader);
    sis_disk_file_read_stop(rfile);
    sis_disk_class_destroy(rfile);

    return SIS_METHOD_OK;
}

int cmd_sisdb_wlog_open(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wlog_cxt *context = (s_sisdb_wlog_cxt *)worker->context;

    if (!argv_)
    {
        return SIS_METHOD_ERROR;
    }
    char fn[255];
    sis_sprintf(fn, 255, "%s.log", (char *)argv_);
    context->wlog = sis_disk_class_create();
    sis_disk_class_init(context->wlog, SIS_DISK_TYPE_STREAM, context->work_path, fn);
    // 立即写盘 且不限制文件大小
    sis_disk_class_set_size(context->wlog, 0, 0);
    sis_disk_file_write_start(context->wlog);
    return SIS_METHOD_OK;
}

int cmd_sisdb_wlog_close(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wlog_cxt *context = (s_sisdb_wlog_cxt *)worker->context;
    if (!context->wlog)
    {
        return SIS_METHOD_ERROR;
    }
    sis_disk_file_write_stop(context->wlog);
    sis_disk_class_destroy(context->wlog);  
    context->wlog = NULL;
    return SIS_METHOD_OK;
}
int cmd_sisdb_wlog_write(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wlog_cxt *context = (s_sisdb_wlog_cxt *)worker->context;

    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    if (!context->wlog)
    {
        return SIS_METHOD_ERROR;
    }
    s_sis_memory *memory = sis_memory_create();
    sis_net_encoded_normal(netmsg, memory);
    sis_disk_file_write_stream(context->wlog, sis_memory(memory), sis_memory_get_size(memory));
    sis_memory_destroy(memory);

    return SIS_METHOD_OK;
}

int cmd_sisdb_wlog_move(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wlog_cxt *context = (s_sisdb_wlog_cxt *)worker->context;

    if (context->wlog)
    {
        sis_disk_file_write_stop(context->wlog);  
        sis_disk_file_delete(context->wlog);
        sis_disk_class_destroy(context->wlog);
        context->wlog = NULL;
    }
    else
    {
        s_sis_disk_class *rfile = sis_disk_class_create();
        char fn[255];
        sis_sprintf(fn, 255, "%s.log", (char *)argv_);
        sis_disk_class_init(rfile, SIS_DISK_TYPE_STREAM, context->work_path, fn);
        sis_disk_file_delete(rfile);
        sis_disk_class_destroy(rfile);
    }
    return SIS_METHOD_OK;
}

int cmd_sisdb_wlog_exist(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wlog_cxt *context = (s_sisdb_wlog_cxt *)worker->context;

    s_sis_disk_class *rfile = sis_disk_class_create();
    char fn[255];
    sis_sprintf(fn, 255, "%s.log", (char *)argv_);
    sis_disk_class_init(rfile, SIS_DISK_TYPE_STREAM, context->work_path, fn);
    int ok = sis_disk_file_read_start(rfile) == 0 ? SIS_METHOD_OK : SIS_METHOD_ERROR;
    sis_disk_file_read_stop(rfile);
    sis_disk_class_destroy(rfile);
    return ok;
}