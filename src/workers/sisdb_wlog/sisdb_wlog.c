#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"

#include "sisdb_wlog.h"

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sisdb_wlog_methods[] = {
    {"read",  cmd_sisdb_wlog_read, NULL, NULL},   
    {"write", cmd_sisdb_wlog_write, NULL, NULL},  
    {"clear", cmd_sisdb_wlog_clear, NULL, NULL},  
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

s_sisdb_wlog_unit *sisdb_wlog_unit_create(s_sisdb_wlog_cxt *cxt_, const char *dsname_)
{
    s_sisdb_wlog_unit *o = SIS_MALLOC(s_sisdb_wlog_unit, o);
    sis_sprintf(o->dsnames, 128, "%s.aof", dsname_);
    o->wlog = sis_disk_class_create();
    sis_disk_class_init(o->wlog, SIS_DISK_TYPE_STREAM, cxt_->work_path, o->dsnames);
    // 立即写盘 且不限制文件大小
    sis_disk_class_set_size(o->wlog, 0, 0);
    sis_map_pointer_set(cxt_->datasets, dsname_, o);

    return o;
}

void sisdb_wlog_unit_destroy(void *unit_)
{
    s_sisdb_wlog_unit *unit = (s_sisdb_wlog_unit *)unit_;

    sis_disk_file_write_stop(unit->wlog);
    sis_disk_class_destroy(unit->wlog);
    sis_free(unit);
}

bool sisdb_wlog_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_wlog_cxt *context = SIS_MALLOC(s_sisdb_wlog_cxt, context);
    worker->context = context;
    {
        s_sis_json_node *sonnode = sis_json_cmp_child_node(node, "wlog-path");
        if (sonnode)
        {
            context->work_path = sis_sdsnew(sonnode->value);
        }
        else
        {
            context->work_path = sis_sdsnew("data/wlog/");
        }
    }
    context->datasets = sis_map_pointer_create_v(sisdb_wlog_unit_destroy);
    
    return true;
}
void sisdb_wlog_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wlog_cxt *context = (s_sisdb_wlog_cxt *)worker->context;
    sis_sdsfree(context->work_path);
    sis_map_pointer_destroy(context->datasets);
    sis_free(context);
}
void sisdb_wlog_method_init(void *worker_)
{
}
void sisdb_wlog_method_uninit(void *worker_)
{
}

static void cb_begin(void *src, msec_t tt)
{
    printf("%s : %llu\n", __func__, tt);
}
static void cb_read_stream(void *source_, const char *key_, const char *sdb_, s_sis_object *obj_)
{
    // __nums++;
    // __size += sis_memory_get_size((s_sis_memory *)obj_->ptr);
    // // if (__nums < 2 || __nums % 1000 == 0)
    // {
    //     printf("%s : %d %zu\n", __func__, __nums, __size);
    //     sis_out_binary(" ", sis_memory((s_sis_memory *)obj_->ptr), 16);
    //             // sis_memory_get_size((s_sis_memory *)obj_->ptr));
    // }
    sis_method_define *method = (sis_method_define *)source_;
    if (method)
    {
        s_sis_net_message *netmsg = sis_net_message_create();
        sis_net_decoded_normal(SIS_OBJ_MEMORY(obj_), netmsg);
        method(NULL, netmsg);
        sis_net_message_destroy(netmsg);
    }
    // 这里通过回调把数据传递出去 
} 
static void cb_end(void *src, msec_t tt)
{
    printf("%s : %llu\n", __func__, tt);
}

int cmd_sisdb_wlog_read(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wlog_cxt *context = (s_sisdb_wlog_cxt *)worker->context;

    s_sis_message *message = (s_sis_message *)argv_;
    s_sis_sds dsname = sis_message_get_str(message, "dataset");

    s_sis_disk_class *rfile = sis_disk_class_create();
    char fn[255];
    sis_sprintf(fn, 255, "%s.aof", dsname);
    sis_disk_class_init(rfile, SIS_DISK_TYPE_STREAM, context->work_path, fn);
    sis_disk_file_read_start(rfile);

    s_sis_disk_callback cb;
    memset(&cb, 0, sizeof(s_sis_disk_callback));
    cb.source = sis_message_get_method(message, "cb_recv");
    cb.cb_begin = cb_begin;
    cb.cb_end = cb_end;
    cb.cb_read = cb_read_stream;
    s_sis_disk_reader *reader = sis_disk_reader_create(&cb); 

    sis_disk_file_read_sub(rfile, reader);

    sis_disk_reader_destroy(reader);
    sis_disk_file_read_stop(rfile);
    sis_disk_class_destroy(rfile);
    return 0;
}

int cmd_sisdb_wlog_write(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wlog_cxt *context = (s_sisdb_wlog_cxt *)worker->context;

    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    char argv[2][128]; 
    int cmds = sis_str_divide(netmsg->cmd, '.', argv[0], argv[1]);
    if (cmds != 2)
    {
        return SIS_METHOD_ERROR;
    }
    s_sisdb_wlog_unit *unit = sis_map_pointer_get(context->datasets, argv[0]);
    if (!unit)
    {
        unit = sisdb_wlog_unit_create(context, argv[0]);
        sis_disk_file_write_start(unit->wlog, SIS_DISK_ACCESS_APPEND);
    } 
    s_sis_memory *memory = sis_memory_create();
    sis_net_encoded_normal(netmsg, memory);
    sis_disk_file_write_stream(unit->wlog, sis_memory(memory), sis_memory_get_size(memory));
    sis_memory_destroy(memory);

    return SIS_METHOD_OK;
}

int cmd_sisdb_wlog_clear(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wlog_cxt *context = (s_sisdb_wlog_cxt *)worker->context;

    s_sisdb_wlog_unit *unit = sis_map_pointer_get(context->datasets, (char *)argv_);
    if (!unit)
    {
        return SIS_METHOD_ERROR;
    }
    sis_disk_file_write_stop(unit->wlog);
    // 删除该文件
    sis_disk_file_delete(unit->wlog);
    // 建立新文件
    sis_disk_file_write_start(unit->wlog, SIS_DISK_ACCESS_CREATE);

    return SIS_METHOD_OK;
}
