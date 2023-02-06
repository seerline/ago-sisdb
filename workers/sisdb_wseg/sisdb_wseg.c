﻿
#include "worker.h"

#include <sis_modules.h>
#include <sisdb_wseg.h>
#include "sis_utils.h"

// 从行情流文件中获取数据源 需要支持直接写压缩数据然后解压写入

static s_sis_method _sisdb_wseg_methods[] = {
  {"getcb",  cmd_sisdb_wseg_getcb, 0, NULL},
};
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////
s_sis_modules sis_modules_sisdb_wseg = {
    sisdb_wseg_init,
    NULL,
    NULL,
    NULL,
    sisdb_wseg_uninit,
    NULL,
    NULL,
    sizeof(_sisdb_wseg_methods)/sizeof(s_sis_method),
    _sisdb_wseg_methods,
};

bool sisdb_wseg_init(void *worker_, void *node_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)node_;

    s_sisdb_wseg_cxt *context = SIS_MALLOC(s_sisdb_wseg_cxt, context);
    worker->context = context;
    {
        context->work_path = sis_sds_save_create(sis_json_get_str(node, "work-path"), "data");   
        context->work_name = sis_sds_save_create(sis_json_get_str(node, "work-name"), "snodb");    
    }     
    context->work_sdbs = sis_sdsnew("*");
    context->work_keys = sis_sdsnew("*");
    return true;
}

void sisdb_wseg_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wseg_cxt *context = (s_sisdb_wseg_cxt *)worker->context;

    context->status = SIS_WSEG_EXIT;
    sisdb_wseg_clear(context);

    sis_sds_save_destroy(context->work_path);
    sis_sds_save_destroy(context->work_name);
    sis_sdsfree(context->work_keys);
    sis_sdsfree(context->work_sdbs);

    sis_free(context);
    worker->context = NULL;
}
///////////////////////////////////////////
//  callback define begin
///////////////////////////////////////////


void sisdb_wseg_clear(s_sisdb_wseg_cxt *context)
{
    if (context->work_unzip)
    {
        sisdb_incr_unzip_stop(context->work_unzip);
        sisdb_incr_destroy(context->work_unzip);
        context->work_unzip = NULL;
    }
    for (int i = 0; i < 4; i++)
    {
        if (context->writer[i])
        {
            sis_disk_writer_stop(context->writer[i]);
            sis_disk_writer_close(context->writer[i]);
            sis_disk_writer_destroy(context->writer[i]);
            context->writer[i] = NULL;
        }
    }
    sis_sdsfree(context->wsno_keys); context->wsno_keys = NULL;
    sis_sdsfree(context->wsno_sdbs); context->wsno_sdbs = NULL;
    sis_map_list_destroy(context->maps_sdbs);
    
}

static msec_t _wsno_msec = 0;
static int cb_sub_start(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wseg_cxt *context = (s_sisdb_wseg_cxt *)worker->context;
    
    if (!argv_)
    {
        context->work_date = sis_time_get_idate(0);
    }
    else
    {
        context->work_date = sis_atoll((char *)argv_);
    }
    sisdb_wseg_clear(context);

    context->status = SIS_WSEG_OPEN;

    LOG(5)("wsno start. %d status : %d\n", context->work_date, context->status);
    _wsno_msec = sis_time_get_now_msec();
    return SIS_METHOD_OK;
}
const char *sisdb_wseg_get_sname(int idx)
{
    switch (idx)
    {
    case SIS_SEG_DATE:
        return "date";
    case SIS_SEG_MINU:
        return "minu";
    case SIS_SEG_MSEC:
        return "msec";
    case SIS_SEG_NONE:
        return "none";
    default:
        return "none";
    }
}
int sisdb_wseg_get_style(s_sis_dynamic_db *db)
{
    if (!db || !db->field_time)
    {
        return SIS_SEG_NONE;
    }
    switch (db->field_time->style)
    {
        case SIS_DYNAMIC_TYPE_WSEC:
        case SIS_DYNAMIC_TYPE_MSEC:
        case SIS_DYNAMIC_TYPE_TSEC:
            return SIS_SEG_MSEC;
        case SIS_DYNAMIC_TYPE_MINU:
            return SIS_SEG_MINU;
        case SIS_DYNAMIC_TYPE_DATE:
        case SIS_DYNAMIC_TYPE_YEAR:
            return SIS_SEG_DATE;
    }
    return SIS_SEG_DATE;
}

void sisdb_wseg_opendbs(s_sisdb_wseg_cxt *context)
{
    s_sis_sds rpath = sis_sdsdup(sis_sds_save_get(context->work_path));
    rpath = sis_sdscatfmt(rpath, "/%s/", sis_sds_save_get(context->work_name));
    int count = sis_map_list_getsize(context->maps_sdbs);
    for (int i = 0; i < count; i++)
    {
        s_sis_dynamic_db *db = sis_map_list_geti(context->maps_sdbs, i);
        int index = sisdb_wseg_get_style(db);
        if (!context->writer[index])
        {
            context->writer[index] = sis_disk_writer_create(rpath, sisdb_wseg_get_sname(index), SIS_DISK_TYPE_SNO);
            if (sis_disk_writer_open(context->writer[index], context->work_date) == 0)
            {
                LOG(5)("open wsno fail. %s %s\n", rpath, sisdb_wseg_get_sname(index));
                sis_disk_writer_close(context->writer[index]);
                sis_disk_writer_destroy(context->writer[index]);
                context->writer[index] = NULL;
            }
        }
    }
    sis_sdsfree(rpath);
}
s_sis_sds sisdb_wseg_getdbs_sds(s_sisdb_wseg_cxt *context, int style)
{
    s_sis_sds curr_sdbs = NULL;
    int count = sis_map_list_getsize(context->maps_sdbs);
    for (int i = 0; i < count; i++)
    {
        s_sis_dynamic_db *db = sis_map_list_geti(context->maps_sdbs, i);
        int index = sisdb_wseg_get_style(db);
        if (!context->writer[index])
        {
            continue;
        }
        if (index == style)
        {
            if (curr_sdbs)
            {
                curr_sdbs = sis_sdscatfmt(curr_sdbs, ",%s", db->name);
            }
            else
            {
                curr_sdbs = sis_sdsnew(db->name);
            }
        }
    }
    if (curr_sdbs)
    {
        s_sis_sds sdbs = sis_match_sdb_of_map(curr_sdbs, context->maps_sdbs);
        sis_sdsfree(curr_sdbs);
        return sdbs;
    }
    return NULL;
}
static int cb_sub_stop(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wseg_cxt *context = (s_sisdb_wseg_cxt *)worker->context;
    if (context->status == SIS_WSEG_FAIL)
    {
        context->status = SIS_WSEG_INIT;
        return SIS_METHOD_ERROR;
    }
    LOG(5)("wsno stop cost = %lld\n", sis_time_get_now_msec() - _wsno_msec);
    sisdb_wseg_clear(context);
    LOG(5)("wsno stop cost = %lld\n", sis_time_get_now_msec() - _wsno_msec);

    context->status = SIS_WSEG_INIT;
    return SIS_METHOD_OK;
}
static int cb_dict_keys(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wseg_cxt *context = (s_sisdb_wseg_cxt *)worker->context;
    sis_sdsfree(context->wsno_keys);
    context->wsno_keys = sis_sdsdup((s_sis_sds)argv_);    
    return SIS_METHOD_OK;
}
static int cb_dict_sdbs(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wseg_cxt *context = (s_sisdb_wseg_cxt *)worker->context;
    sis_sdsfree(context->wsno_sdbs);
    context->wsno_sdbs = sis_sdsdup((s_sis_sds)argv_);
    if (!context->maps_sdbs)
    {
        context->maps_sdbs = sis_map_list_create(sis_dynamic_db_destroy);
    }
    sis_map_list_clear(context->maps_sdbs);
    sis_get_map_sdbs(context->wsno_sdbs, context->maps_sdbs);

    sisdb_wseg_opendbs(context);
    
    return SIS_METHOD_OK;
}
static int cb_decode(void *context_, int kidx_, int sidx_, char *in_, size_t isize_)
{
    s_sisdb_wseg_cxt *context = (s_sisdb_wseg_cxt *)context_;
    
    if (in_ && isize_ && kidx_>=0 && sidx_>=0)
    {
        const char *kname = sisdb_incr_get_kname(context->work_unzip, kidx_);
        const char *sname = sisdb_incr_get_sname(context->work_unzip, sidx_);

        s_sis_dynamic_db *db = sis_map_list_get(context->maps_sdbs, sname);
        int idx = sisdb_wseg_get_style(db);
        sis_disk_writer_sno(context->writer[idx], kname, sname, in_, isize_);
    }
    return 0;
} 

static int _write_wsno_head(s_sisdb_wseg_cxt *context, int iszip)
{
    if (context->status != SIS_WSEG_OPEN)
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
        for (int i = 0; i < 4; i++)
        {
            if (!context->writer[i])
            {
                continue;
            }
            s_sis_sds newsdbs = sisdb_wseg_getdbs_sds(context, i);
            if (!newsdbs)
            {
                newsdbs =  sis_sdsdup(context->wsno_sdbs);
            } 
            sis_disk_writer_set_kdict(context->writer[i], newkeys, sis_sdslen(newkeys));
            sis_disk_writer_set_sdict(context->writer[i], newsdbs, sis_sdslen(newsdbs));
            printf("new %s %s\n", newkeys, newsdbs);
            sis_sdsfree(newkeys); 
            sis_sdsfree(newsdbs); 
            sis_disk_writer_start(context->writer[i]);
        }
        
        context->work_unzip = sisdb_incr_create(); 
        sisdb_incr_set_keys(context->work_unzip, context->wsno_keys);
        sisdb_incr_set_sdbs(context->work_unzip,  context->wsno_sdbs);
        sisdb_incr_unzip_start(context->work_unzip, context, cb_decode);
        printf("sno %s %s\n", context->wsno_keys, context->wsno_sdbs);
        
    }
    else
    {
        for (int i = 0; i < 4; i++)
        {
            if (!context->writer[i])
            {
                continue;
            }
            s_sis_sds newsdbs = sisdb_wseg_getdbs_sds(context, i);
            if (!newsdbs)
            {
                newsdbs =  sis_sdsdup(context->wsno_sdbs);
            } 
            sis_disk_writer_set_kdict(context->writer[i], context->wsno_keys, sis_sdslen(context->wsno_keys));
            sis_disk_writer_set_sdict(context->writer[i], newsdbs, sis_sdslen(newsdbs));
            sis_disk_writer_start(context->writer[i]);
            sis_sdsfree(newsdbs); 
        }
    }
    context->status = SIS_WSEG_HEAD;
    return 0;
}
static int cb_sub_chars(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wseg_cxt *context = (s_sisdb_wseg_cxt *)worker->context;
    if (context->status == SIS_WSEG_FAIL)
    {
        return SIS_METHOD_ERROR;
    }
    _write_wsno_head(context, 0);
    s_sis_db_chars *inmem = (s_sis_db_chars *)argv_;
    s_sis_dynamic_db *db = sis_map_list_get(context->maps_sdbs, inmem->sname);
    int idx = sisdb_wseg_get_style(db);
    sis_disk_writer_sno(context->writer[idx], inmem->kname, inmem->sname, inmem->data, inmem->size);
    // if (!sis_strcasecmp(inmem->kname, "SH600745"))
    // {
    //     printf("%s %zu\n", __func__, inmem->size);
    // }
	return SIS_METHOD_OK;
}
static int cb_sub_incrzip(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wseg_cxt *context = (s_sisdb_wseg_cxt *)worker->context;
    if (context->status == SIS_WSEG_FAIL)
    {
        return SIS_METHOD_ERROR;
    }
    _write_wsno_head(context, 1);

    s_sis_db_incrzip *inmem = (s_sis_db_incrzip *)argv_;
    if (context->work_unzip)
    {
        sisdb_incr_unzip_set(context->work_unzip, inmem);
    }
	return SIS_METHOD_OK;
}
///////////////////////////////////////////
//  callback define end.
///////////////////////////////////////////

///////////////////////////////////////////
//  method define
/////////////////////////////////////////

int cmd_sisdb_wseg_getcb(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wseg_cxt *context = (s_sisdb_wseg_cxt *)worker->context;
    if (context->status != SIS_WSEG_NONE && context->status != SIS_WSEG_INIT)
    {
        return SIS_METHOD_ERROR;
    }
    s_sis_message *msg = (s_sis_message *)argv_; 
    sis_sds_save_set(context->work_path, sis_message_get_str(msg, "work-path"));
    sis_sds_save_set(context->work_name, sis_message_get_str(msg, "work-name"));

    sis_message_set(msg, "cb_source", worker, NULL);
    sis_message_set_method(msg, "cb_sub_start"   ,cb_sub_start);
    sis_message_set_method(msg, "cb_sub_stop"    ,cb_sub_stop);
    sis_message_set_method(msg, "cb_dict_sdbs"   ,cb_dict_sdbs);
    sis_message_set_method(msg, "cb_dict_keys"   ,cb_dict_keys);
    sis_message_set_method(msg, "cb_sub_chars"   ,cb_sub_chars);
    sis_message_set_method(msg, "cb_sub_incrzip" ,cb_sub_incrzip);

    context->status = SIS_WSEG_INIT;
    return SIS_METHOD_OK; 
}
