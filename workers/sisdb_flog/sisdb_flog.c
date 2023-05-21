#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"

#include "sisdb_flog.h"
#include "sis_net.node.h"

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method _sisdb_flog_methods[] = {
    {"sub",     cmd_sisdb_flog_sub,     0, NULL},  
    {"unsub",   cmd_sisdb_flog_unsub,   0, NULL}, 
    {"open",    cmd_sisdb_flog_open,    0, NULL},   
    {"write",   cmd_sisdb_flog_write,   0, NULL},  
    {"close",   cmd_sisdb_flog_close,   0, NULL},   
    {"remove",  cmd_sisdb_flog_remove,  0, NULL},  
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
    sizeof(_sisdb_flog_methods) / sizeof(s_sis_method),
    _sisdb_flog_methods,
};

bool sisdb_flog_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_flog_cxt *context = SIS_MALLOC(s_sisdb_flog_cxt, context);
    worker->context = context;

    context->work_date = sis_time_get_idate(0);
	context->work_path = sis_sds_save_create(sis_json_get_str(node, "work-path"), "data");   
    context->work_name = sis_sds_save_create(sis_json_get_str(node, "work-name"), node->key);   
 
    context->write_memory = sis_memory_create();
    return true;
}

void sisdb_flog_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)worker->context;

    if (context->status == SIS_FLOG_READ)
    {
        cmd_sisdb_flog_unsub(worker, NULL);
    }
    if (context->status == SIS_FLOG_WRITE)
    {
        cmd_sisdb_flog_close(worker, NULL);
    }
    sis_sds_save_destroy(context->work_path);
    sis_sds_save_destroy(context->work_name);
    sis_memory_destroy(context->write_memory);
    sis_free(context);
}
//////////////////////////////////////
//  sub log callback
//////////////////////////////////////
static void cb_flog_start(void *source_, int idate)
{
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)source_;
    if (context->cb_sub_start)
    {
        char sdate[32];
        sis_lldtoa(idate, sdate, 32, 10);
        context->cb_sub_start(context->cb_source, sdate);
    }
}
static void cb_flog_stop(void *source_, int idate)
{
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)source_;
    if (context->cb_sub_stop)
    {
        char sdate[32];
        sis_lldtoa(idate, sdate, 32, 10);
        context->cb_sub_stop(context->cb_source, sdate);
    }
}
static void cb_flog_break(void *source_, int idate)
{
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)source_;
    if (context->cb_sub_stop)
    {
        context->cb_sub_stop(context->cb_source, "0");
    }
}
static void cb_flog_original(void *source_, s_sis_disk_head *head_, void *out_, size_t olen_)
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
    sis_sds_save_set(context->work_path, sis_message_get_str(msg, "work-path"));
    sis_sds_save_set(context->work_name, sis_message_get_str(msg, "work-name"));
    if (sis_message_exist(msg, "work-date"))
    {
        context->work_date = sis_message_get_int(msg, "work-date");
    }
    context->cb_source =  sis_message_get(msg, "cb_source");
    context->cb_sub_start =  sis_message_get_method(msg, "cb_sub_start");
    context->cb_sub_stop =  sis_message_get_method(msg, "cb_sub_stop");
    context->cb_netmsg =  sis_message_get_method(msg, "cb_netmsg");

    context->status = SIS_FLOG_READ;

    s_sis_disk_reader_cb *rlog_cb = SIS_MALLOC(s_sis_disk_reader_cb, rlog_cb);
    rlog_cb->cb_source = context;
    rlog_cb->cb_start    = cb_flog_start;
    rlog_cb->cb_original = cb_flog_original;
    rlog_cb->cb_stop     = cb_flog_stop;
    rlog_cb->cb_break    = cb_flog_break;

    context->reader = sis_disk_reader_create(
        sis_sds_save_get(context->work_path), 
        sis_sds_save_get(context->work_name), 
        SIS_DISK_TYPE_LOG, rlog_cb);

    LOG(5)("sub log open. [%d]\n", context->work_date);
    sis_disk_reader_sub_log(context->reader, context->work_date);
    LOG(5)("sub log close. [%d]\n", context->work_date);

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
    sis_sds_save_set(context->work_path, sis_message_get_str(msg, "work-path"));
    sis_sds_save_set(context->work_name, sis_message_get_str(msg, "work-name"));
    if (sis_message_exist(msg, "work-date"))
    {
        context->work_date = sis_message_get_int(msg, "work-date");
    }
    context->status = SIS_FLOG_WRITE;
    context->writer = sis_disk_writer_create(
        sis_sds_save_get(context->work_path), 
        sis_sds_save_get(context->work_name), 
        SIS_DISK_TYPE_LOG);
    
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
        sis_net_encoded_normal(netmsg, context->write_memory);
        sis_disk_writer_log(context->writer, sis_memory(context->write_memory), sis_memory_get_size(context->write_memory));
    }
    return SIS_METHOD_OK;
}

int cmd_sisdb_flog_remove(void *worker_, void *argv_)
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
        sis_message_exist(msg, "work-date"))
    {
        sis_disk_control_remove(
            sis_message_get_str(msg, "work-path"), 
            sis_message_get_str(msg, "work-name"), 
            SIS_DISK_TYPE_LOG, 
            sis_message_get_int(msg, "work-date"));
    }
    return SIS_METHOD_OK;
}

// int cmd_sisdb_flog_exist(void *worker_, void *argv_)
// {
//     s_sis_worker *worker = (s_sis_worker *)worker_; 
//     s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)worker->context;
//     int o = 0;
//     s_sis_message *msg = (s_sis_message *)argv_;
//     if (sis_message_exist(msg, "work-path") &&
//         sis_message_exist(msg, "work-name") &&
//         sis_message_exist(msg, "work-date"))
//     {
//         o = sis_disk_control_exist(
//             sis_message_get_str(msg, "work-path"), 
//             sis_message_get_str(msg, "work-name"), 
//             SIS_DISK_TYPE_LOG, 
//             sis_message_get_int(msg, "work-date"));
//     }
//     return o == 1 ? SIS_METHOD_OK : SIS_METHOD_NULL;   
// }

#if 0

// 测试写入文件的速度
int _sendnum = 1*1000*1000;

int main()
{
    sis_server_init();

    s_sis_worker *rlog = sis_worker_create_of_conf(NULL, "wlog", "{ classname:sisdb_flog }");
    LOG(5)("rlog = %p\n", rlog);
    s_sis_message *msg = sis_message_create();
    sis_message_set_str(msg, "work-path", ".", 1);
    sis_message_set_str(msg, "work-name", "wlog", 4);
    sis_message_set_int(msg, "work-date", 20211010);
    // sis_message_set(msg, "cb_source", worker, NULL);
    // sis_message_set_method(msg, "cb_sub_start", NULL);
    // sis_message_set_method(msg, "cb_sub_stop", NULL);
    // sis_message_set_method(msg, "cb_netmsg", cb_rlog_netmsg);
    sis_worker_command(rlog, "open", msg);

    msg->key = sis_sdsnew("1234567890");
    s_sis_memory *imem = sis_memory_create();
    for (int i = 0; i < 1000; i++)
    {
        sis_memory_cat(imem, "1234567890", 10);
    }
    
    s_sisdb_flog_cxt *context = (s_sisdb_flog_cxt *)rlog->context;
    
    msec_t _openmsec = sis_time_get_now_msec(); 
    for (int i = 0; i < _sendnum; i++)
    {
        sis_disk_writer_log(context->writer, sis_memory(imem), sis_memory_get_size(imem));
        // sis_worker_command(rlog, "write", msg);    
    }
    sis_worker_command(rlog, "close", msg);
    printf("cost msec = %lld\n", sis_time_get_now_msec() - _openmsec); // 10G/3592 = 2.784G/s

    // _openmsec = sis_time_get_now_msec(); 
    // int wfp = sis_open("wlog/info.log", SIS_FILE_IO_TRUNC | SIS_FILE_IO_RDWR | SIS_FILE_IO_CREATE, SIS_FILE_MODE_NORMAL);
    // for (int i = 0; i < _sendnum; i++)
    // {
    //     sis_write(wfp, sis_memory(imem), sis_memory_get_size(imem));
    // }
    // sis_close(wfp);
    // printf("cost wfile msec = %lld\n", sis_time_get_now_msec() - _openmsec); // 344950

    {
        _openmsec = sis_time_get_now_msec();
        s_sis_file_handle wfp = sis_file_open("wlog/finfo.log", SIS_FILE_IO_READ | SIS_FILE_IO_WRITE | SIS_FILE_IO_CREATE, 0);
        for (int i = 0; i < _sendnum; i++)
        {
            sis_file_write(wfp, sis_memory(imem), sis_memory_get_size(imem)); // 10G/12062 = 829M/s
            // sis_file_fsync(wfp);  // 10G/66490 = 150M/s
        }
        sis_file_fsync(wfp);  // 10G/12803 = 810M/s
        sis_file_close(wfp);
        printf("cost ffile msec = %lld\n", sis_time_get_now_msec() - _openmsec); // 344950
    }

    sis_memory_destroy(imem);
    sis_message_destroy(msg);
    sis_worker_destroy(rlog);  

    sis_server_uninit();  
}

#endif