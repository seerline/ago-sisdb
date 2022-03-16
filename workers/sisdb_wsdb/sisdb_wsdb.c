#include "worker.h"
#include "sis_modules.h"

#include "server.h"
#include "sis_method.h"

#include "sisdb_wsdb.h"

static s_sis_method _sisdb_wsdb_methods[] = {
  {"start",  cmd_sisdb_wsdb_start, 0, NULL},
  {"stop",   cmd_sisdb_wsdb_stop,  0, NULL},
  {"write",  cmd_sisdb_wsdb_write, 0, NULL},  // 直接把数据写入磁盘
  {"merge",  cmd_sisdb_wsdb_merge, 0, NULL},  // 仅仅合并和整理日频数据 如果写入时为整年写入
  {"push",   cmd_sisdb_wsdb_push,  0, NULL},  // 追加数据到末尾 直到 stop 时才存入磁盘
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
        context->work_path = sis_sds_save_create(sis_json_get_str(node, "work-path"), "data");   
        context->work_name = sis_sds_save_create(sis_json_get_str(node, "work-name"), "snodb");    
    } 
    {
        s_sis_json_node *sonnode = sis_json_cmp_child_node(node, "safe-path");
        if (sonnode)
        {
            context->safe_path = sis_sdsnew(sonnode->value);
        }
        else
        {
            context->safe_path = sis_sdsnew("safe");
        }
    }
    context->work_sdbs = sis_sdsnew("*");
    context->work_keys = sis_sdsnew("*");

    context->work_datas = sis_map_pointer_create_v(sis_struct_list_destroy);
    // printf("==2.8==%p==\n", context->work_datas);
    return true;
}
void sisdb_wsdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsdb_cxt *context = (s_sisdb_wsdb_cxt *)worker->context;

    context->status = SIS_WSDB_EXIT;
    sisdb_wsdb_stop(context);

    sis_map_pointer_destroy(context->work_datas);
    sis_sds_save_destroy(context->work_path);
    sis_sds_save_destroy(context->work_name);
    sis_sdsfree(context->safe_path);
    sis_sdsfree(context->work_keys);
    sis_sdsfree(context->work_sdbs);

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

    context->writer = sis_disk_writer_create(
        sis_sds_save_get(context->work_path), 
        sis_sds_save_get(context->work_name), 
        SIS_DISK_TYPE_SDB);
    sis_disk_writer_open(context->writer, 0); // 这里传入 0 可能只会写入当天的文件中
    context->status = SIS_WSDB_OPEN;
}
void sisdb_wsdb_stop(s_sisdb_wsdb_cxt *context)
{
    if (context->writer)
    {
        int size = sis_map_pointer_getsize(context->work_datas);
        if (size > 0)
        {
            char kname[128],sname[128];
            s_sis_dict_entry *de;
            s_sis_dict_iter *di = sis_dict_get_iter(context->work_datas);
            while ((de = sis_dict_next(di)) != NULL)
            {
                s_sis_struct_list *slist = (s_sis_struct_list *)sis_dict_getval(de);
                sis_str_divide((char *)sis_dict_getkey(de), '.', kname, sname);
                sis_disk_writer_sdb(context->writer, kname, sname, 
                    sis_struct_list_first(slist), slist->count * slist->len);
                // printf("---- %s : [%s] %s\n", work->classname, (char *)sis_dict_getkey(de), work->workername);
            }
            sis_dict_iter_free(di);        
        }
        sis_disk_writer_stop(context->writer);
        sis_disk_writer_close(context->writer);
        sis_disk_writer_destroy(context->writer);
        context->writer = NULL;
    }
    sis_map_pointer_clear(context->work_datas);
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
///////////////////////////////////////////
int cmd_sisdb_wsdb_start(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsdb_cxt *context = (s_sisdb_wsdb_cxt *)worker->context;

    s_sis_message *msg = (s_sis_message *)argv_; 
    sis_sds_save_set(context->work_path, sis_message_get_str(msg, "work-path"));
    sis_sds_save_set(context->work_name, sis_message_get_str(msg, "work-name"));
    sisdb_wsdb_start(context);
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
            if (chars)
            {
                sis_disk_writer_sdb(context->writer, chars->kname, chars->sname, chars->data, chars->size);
            }
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

    int style = SIS_SDB_STYLE_SDB;
    if (sis_message_exist(msg, "style"))
    {
        style = sis_message_get_int(msg, "style");
    }
    
    if (context->status == SIS_WSDB_NONE)
    {
        sis_sds_save_set(context->work_path, sis_message_get_str(msg, "work-path"));
        sis_sds_save_set(context->work_name, sis_message_get_str(msg, "work-name"));
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

int cmd_sisdb_wsdb_merge(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsdb_cxt *context = (s_sisdb_wsdb_cxt *)worker->context;
    
    if (context->status != SIS_WSDB_NONE)
    {
        return SIS_METHOD_ERROR;
    }    
    s_sis_message *msg = (s_sis_message *)argv_; 
    sis_sds_save_set(context->work_path, sis_message_get_str(msg, "work-path"));
    sis_sds_save_set(context->work_name, sis_message_get_str(msg, "work-name"));

    // 仅仅处理日上数据
    int idate = sis_net_msg_info_as_date(msg);
    s_sis_disk_ctrl *ctrl = sis_disk_ctrl_create(
        SIS_DISK_TYPE_SDB_YEAR,
        sis_sds_save_get(context->work_path),
        sis_sds_save_get(context->work_name),
        idate);

    sis_disk_ctrl_merge(ctrl);
    // 其他时间尺度的数据也需要合并
    sis_disk_ctrl_destroy(ctrl);
    return SIS_METHOD_OK; 
}
int cmd_sisdb_wsdb_push(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsdb_cxt *context = (s_sisdb_wsdb_cxt *)worker->context;
    
    // 必须已经打开
    if (context->status != SIS_WSDB_OPEN)
    {
        return SIS_METHOD_ERROR;
    }    
    s_sis_message *msg = (s_sis_message *)argv_; 
    int style = SIS_SDB_STYLE_SDB;
    if (sis_message_exist(msg, "style"))
    {
        style = sis_message_get_int(msg, "style");
    }
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
            s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(context->writer->map_sdbs, chars->sname);
            s_sis_dynamic_db *sdb = sis_disk_sdict_last(sdict);
            if (sdb)
            {
                char name[255];
                sis_sprintf(name, 255, "%s.%s", chars->kname, chars->sname);
                s_sis_struct_list *slist = (s_sis_struct_list *)sis_map_pointer_get(context->work_datas, name);
                if (!slist)
                {
                    slist = sis_struct_list_create(sdb->size);
                    sis_map_pointer_set(context->work_datas, name, slist);
                }
                int count = chars->size / sdb->size;
                sis_struct_list_pushs(slist, chars->data, count);
            }
        }
        break;
    }
    return SIS_METHOD_OK; 
}

