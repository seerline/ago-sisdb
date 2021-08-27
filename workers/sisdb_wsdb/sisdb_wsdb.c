#include "worker.h"
#include "sis_modules.h"

#include "server.h"
#include "sis_method.h"

#include "sisdb_wsdb.h"

static s_sis_method _sisdb_wsdb_methods[] = {
  {"start",  cmd_sisdb_wsdb_start, 0, NULL},
  {"stop",   cmd_sisdb_wsdb_stop, 0, NULL},
  {"write",  cmd_sisdb_wsdb_write, 0, NULL},
};
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////
// 通用文件存取接口
s_sis_modules sis_modules_sisdb_wsdb = {
    sisdb_wsdb_init,
    NULL,
    NULL,
    NULL,
    sisdb_wsdb_uninit,
    NULL,
    NULL,
    sizeof(_sisdb_wsdb_methods) / sizeof(s_sis_method),
    _sisdb_wsdb_methods,
};

bool sisdb_wsdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_wsdb_cxt *context = SIS_MALLOC(s_sisdb_wsdb_cxt, context);
    worker->context = context;
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
            context->work_name = sis_sdsempty();
        }
    }
    {
        s_sis_json_node *sonnode = sis_json_cmp_child_node(node, "safe-path");
        if (sonnode)
        {
            context->safe_path = sis_sdsnew(sonnode->value);
        }
        else
        {
            context->safe_path = sis_sdsnew("data/safe/");
        }
    }
    context->work_sdbs = sis_sdsnew("*");
    context->work_keys = sis_sdsnew("*");
    return true;
}
void sisdb_wsdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsdb_cxt *context = (s_sisdb_wsdb_cxt *)worker->context;

    context->status = SIS_WSDB_EXIT;
    sisdb_wsdb_stop(context);

    sis_sdsfree(context->work_path);
    sis_sdsfree(context->work_name);
    sis_sdsfree(context->safe_path);
    sis_sdsfree(context->work_keys);
    sis_sdsfree(context->work_sdbs);

    sis_free(context);
    
    sis_free(context);
    worker->context = NULL;
}

///////////////////////////////////////////
//  callback define begin
///////////////////////////////////////////

void sisdb_wsdb_start(s_sisdb_wsdb_cxt *context)
{
    // 老文件如果没有关闭再关闭一次
    sisdb_wsdb_stop(context);

    sis_sdsfree(context->work_keys); context->work_keys = NULL;
    sis_sdsfree(context->work_sdbs); context->work_sdbs = NULL;

    context->writer = sis_disk_writer_create(context->work_path, context->work_name, SIS_DISK_TYPE_SDB);
    sis_disk_writer_open(context->writer, 0);
    context->status = SIS_WSDB_OPEN;
}
void sisdb_wsdb_stop(s_sisdb_wsdb_cxt *context)
{
    if (context->writer)
    {
        sis_disk_writer_stop(context->writer);
        sis_disk_writer_close(context->writer);
        sis_disk_writer_destroy(context->writer);
        context->writer = NULL;
    }
     context->status = SIS_WSDB_NONE;
}

static int _write_wsdb_head(s_sisdb_wsdb_cxt *context, int iszip)
{
    if (context->wheaded)
    {
        return 0;
    }
    sis_disk_writer_set_kdict(context->writer, context->work_keys, sis_sdslen(context->work_keys));
    sis_disk_writer_set_sdict(context->writer, context->work_sdbs, sis_sdslen(context->work_sdbs));
    sis_disk_writer_start(context->writer);
    context->wheaded = 1;
    return 0;
}

///////////////////////////////////////////
//  callback define end.
///////////////////////////////////////////

///////////////////////////////////////////
//  method define
/////////////////////////////////////////
int cmd_sisdb_wsdb_start(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsdb_cxt *context = (s_sisdb_wsdb_cxt *)worker->context;

    sisdb_wsdb_start(context);
    s_sis_message *msg = (s_sis_message *)argv_; 
    {
        s_sis_sds str = sis_message_get_str(msg, "work-keys");
        if (str)
        {
            sis_sdsfree(context->work_keys);
            context->work_keys = sis_sdsdup(str);
        }
    }
    {
        s_sis_sds str = sis_message_get_str(msg, "work-sdbs");
        if (str)
        {
            sis_sdsfree(context->work_sdbs);
            context->work_sdbs = sis_sdsdup(str);
        }
    }
    context->wheaded = 0;
    context->status = SIS_WSDB_OPEN;
    return SIS_METHOD_OK; 
}    
int cmd_sisdb_wsdb_stop(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsdb_cxt *context = (s_sisdb_wsdb_cxt *)worker->context;
    sisdb_wsdb_stop(context);
    context->status = SIS_WSDB_NONE;
    return SIS_METHOD_OK; 
} 
void sisdb_wsdb_write(s_sisdb_wsdb_cxt *context, int style, s_sis_message *msg)
{
    _write_wsdb_head(context, 0);
    switch (style)
    {
    case SIS_SDB_STYLE_ONE:
    case SIS_SDB_STYLE_MUL:
        break;    
    case SIS_SDB_STYLE_NON:
    default:
        {
            s_sis_db_chars *chars = sis_message_get(msg, "chars");
            sis_disk_writer_sdb(context->writer, chars->kname, chars->sname, chars->data, chars->size);
        }
        break;
    }
}
int cmd_sisdb_wsdb_write(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsdb_cxt *context = (s_sisdb_wsdb_cxt *)worker->context;
    
    if (context->status != SIS_WSDB_NONE && context->status != SIS_WSDB_OPEN)
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
    int style = SIS_SDB_STYLE_SDB;
    if (sis_message_exist(msg, "style"))
    {
        style = sis_message_get_int(msg, "style");
    }
    
    if (context->status == SIS_WSDB_NONE)
    {
        // 打开文件
        sisdb_wsdb_start(context);
        // 写文件
        sisdb_wsdb_write(context, style, msg);
        // 关闭文件
        sisdb_wsdb_stop(context);
    }
    else
    {
        // 写文件
        sisdb_wsdb_write(context, style, msg);
    }
    return SIS_METHOD_OK; 
}


