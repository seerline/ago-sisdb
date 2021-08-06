
#include "worker.h"

#include <sis_modules.h>
#include <sisdb_wsno.h>

// 从行情流文件中获取数据源
static s_sis_method _sisdb_wsno_methods[] = {
  {"getcb",  cmd_sisdb_wsno_getcb, 0, NULL},
};

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////
s_sis_modules sis_modules_sisdb_wsno = {
    sisdb_wsno_init,
    NULL,
    NULL,
    NULL,
    sisdb_wsno_uninit,
    NULL,
    NULL,
    sizeof(_sisdb_wsno_methods)/sizeof(s_sis_method),
    _sisdb_wsno_methods,
};

bool sisdb_wsno_init(void *worker_, void *node_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)node_;

    s_sisdb_wsno_cxt *context = SIS_MALLOC(s_sisdb_wsno_cxt, context);
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
            context->work_name = sis_sdsnew("snodb");
        }  
    }     
    context->work_sdbs = sis_sdsnew("*");
    context->work_keys = sis_sdsnew("*");
    return true;
}

void sisdb_wsno_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;

    context->status = SIS_WSNO_EXIT;
    sisdb_wsno_stop(context);

    sis_sdsfree(context->work_path);
    sis_sdsfree(context->work_name);
    sis_sdsfree(context->work_keys);
    sis_sdsfree(context->work_sdbs);

    sis_sdsfree(context->wsno_keys);
    sis_sdsfree(context->wsno_sdbs);
    sis_free(context);
    worker->context = NULL;
}
///////////////////////////////////////////
//  callback define begin
///////////////////////////////////////////
static int cb_sub_stop(void *worker_, void *argv_);

void sisdb_wsno_start(s_sisdb_wsno_cxt *context)
{
    // 老文件如果没有关闭再关闭一次
    sisdb_wsno_stop(context);

    sis_sdsfree(context->wsno_keys); context->wsno_keys = NULL;
    sis_sdsfree(context->wsno_sdbs); context->wsno_sdbs = NULL;

    context->writer = sis_disk_writer_create(context->work_path, context->work_name, SIS_DISK_TYPE_SNO);
    sis_disk_writer_open(context->writer, context->work_date);
}
void sisdb_wsno_stop(s_sisdb_wsno_cxt *context)
{
    if (context->work_unzip)
    {
        sisdb_worker_unzip_stop(context->work_unzip);
        sisdb_worker_destroy(context->work_unzip);
        context->work_unzip = NULL;
    }
    if (context->writer)
    {
        sis_disk_writer_stop(context->writer);
        sis_disk_writer_close(context->writer);
        sis_disk_writer_destroy(context->writer);
        context->writer = NULL;
    }
}

msec_t _wsno_msec = 0;
static int cb_sub_start(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;
    
    if (!argv_)
    {
        context->work_date = sis_time_get_idate(0);
    }
    else
    {
        context->work_date = sis_atoll((char *)argv_);
    }
    LOG(5)("wsno start. %d\n", context->work_date);
    sisdb_wsno_start(context);
    _wsno_msec = sis_time_get_now_msec();
    context->status = SIS_WSNO_OPEN;
    return SIS_METHOD_OK;
}
static int cb_sub_stop(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;

    LOG(5)("wsno stop cost = %lld\n", sis_time_get_now_msec() - _wsno_msec);
    sisdb_wsno_stop(context);

    sis_sdsfree(context->wsno_keys); context->wsno_keys = NULL;
    sis_sdsfree(context->wsno_sdbs); context->wsno_sdbs = NULL;

    context->status = SIS_WSNO_INIT;
    return SIS_METHOD_OK;
}
static int cb_dict_keys(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;
    sis_sdsfree(context->wsno_keys);
    context->wsno_keys = sis_sdsdup((s_sis_sds)argv_);    
    return SIS_METHOD_OK;
}
static int cb_dict_sdbs(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;
    sis_sdsfree(context->wsno_sdbs);
    context->wsno_sdbs = sis_sdsdup((s_sis_sds)argv_);
    return SIS_METHOD_OK;
}
static int cb_decode(void *context_, int kidx_, int sidx_, char *in_, size_t ilen_)
{
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)context_;
    
    const char *kname = sisdb_worker_get_kname(context->work_unzip, kidx_);
    const char *sname = sisdb_worker_get_sname(context->work_unzip, sidx_);
    sis_disk_writer_sno(context->writer, kname, sname, in_, ilen_);
    return 0;
} 

static int _write_wsno_head(s_sisdb_wsno_cxt *context, int iszip)
{
    if (context->status != SIS_WSNO_OPEN)
    {
        return 0;
    }
    if (iszip)
    {
        // 如果接收的是压缩格式 必须要在这里过滤代码 和 结构
        s_sis_sds newkeys = sis_match_key(context->work_keys, context->wsno_keys);
        if (!newkeys)
        {
            newkeys =  sis_sdsdup(context->wsno_keys);
        }
        s_sis_sds newsdbs = sis_match_sdb_of_sds(context->work_sdbs, context->wsno_sdbs);
        if (!newsdbs)
        {
            newsdbs =  sis_sdsdup(context->wsno_sdbs);
        } 
        sis_disk_writer_set_kdict(context->writer, newkeys, sis_sdslen(newkeys));
        sis_disk_writer_set_sdict(context->writer, newsdbs, sis_sdslen(newsdbs));
        sis_disk_writer_start(context->writer);
        context->work_unzip = sisdb_worker_create(); 
        sisdb_worker_set_keys(context->work_unzip, newkeys);
        sisdb_worker_set_sdbs(context->work_unzip,  newsdbs);
        sisdb_worker_unzip_start(context->work_unzip, context, cb_decode);

        sis_sdsfree(newkeys); 
        sis_sdsfree(newsdbs); 
    }
    else
    {
        sis_disk_writer_set_kdict(context->writer, context->wsno_keys, sis_sdslen(context->wsno_keys));
        sis_disk_writer_set_sdict(context->writer, context->wsno_sdbs, sis_sdslen(context->wsno_sdbs));
        sis_disk_writer_start(context->writer);
    }
    context->status = SIS_WSNO_HEAD;
    return 0;
}
static int cb_sub_chars(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;

    _write_wsno_head(context, 0);

    s_sis_db_chars *inmem = (s_sis_db_chars *)argv_;
    sis_disk_writer_sno(context->writer, inmem->kname, inmem->sname, inmem->data, inmem->size);
	return SIS_METHOD_OK;
}
static int cb_sub_incrzip(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;

    _write_wsno_head(context, 1);

    s_sis_db_incrzip *inmem = (s_sis_db_incrzip *)argv_;
    sisdb_worker_unzip_set(context->work_unzip, inmem);
	return SIS_METHOD_OK;
}
///////////////////////////////////////////
//  callback define end.
///////////////////////////////////////////

///////////////////////////////////////////
//  method define
/////////////////////////////////////////

int cmd_sisdb_wsno_getcb(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;
    if (context->status != SIS_WSNO_NONE && context->status != SIS_WSNO_INIT)
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
    {
        s_sis_sds str = sis_message_get_str(msg, "sub-keys");
        if (str)
        {
            sis_sdsfree(context->work_keys);
            context->work_keys = sis_sdsdup(str);
        }
    }
    {
        s_sis_sds str = sis_message_get_str(msg, "sub-sdbs");
        if (str)
        {
            sis_sdsfree(context->work_sdbs);
            context->work_sdbs = sis_sdsdup(str);
        }
    }
    sis_message_set(msg, "source", worker, NULL);
    sis_message_set_method(msg, "cb_sub_start"   ,cb_sub_start);
    sis_message_set_method(msg, "cb_sub_stop"    ,cb_sub_stop);
    sis_message_set_method(msg, "cb_dict_sdbs"   ,cb_dict_sdbs);
    sis_message_set_method(msg, "cb_dict_keys"   ,cb_dict_keys);
    sis_message_set_method(msg, "cb_sub_chars"   ,cb_sub_chars);
    sis_message_set_method(msg, "cb_sub_incrzip" ,cb_sub_incrzip);

    context->status = SIS_WSNO_INIT;
    return SIS_METHOD_OK; 
}
