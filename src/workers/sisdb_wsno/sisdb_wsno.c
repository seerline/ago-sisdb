
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
    const char *str = sis_json_get_str(node, "work-path");
    if (str)
    {
        context->work_path = sis_sdsnew(str);
    } 
    else
    {
        context->work_path = sis_sdsnew("./");
    } 
    context->page_size =  sis_json_get_int(node, "page-size", 0) * 1000000;   

    context->write_class = sis_disk_class_create(); 
    return true;
}

void sisdb_wsno_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;
    sis_sdsfree(context->work_path);

    sis_disk_class_destroy(context->write_class);
    if (context->wsno_unzip)
    {
        sdcdb_worker_destroy(context->wsno_unzip);
    }
    sis_sdsfree(context->wsno_date);
    sis_sdsfree(context->wsno_keys);
    sis_sdsfree(context->wsno_sdbs);
    sis_free(context);
    worker->context = NULL;
}

///////////////////////////////////////////
//  callback define begin
///////////////////////////////////////////
static int cb_unzip_info(void *source, int kidx, int sidx, char *in, size_t ilen);

static int _write_head(s_sisdb_wsno_cxt *context, bool iszip)
{
    if (context->iswhead != 1)
    {
        return 0;
    }
    sis_disk_class_init(context->write_class, SIS_DISK_TYPE_SNO , context->work_path, context->wsno_date); 
    if (context->page_size > 1024 * 1024)
    {
        context->write_class->work_fps->max_page_size = context->page_size; 
    }
    int rtn = sis_disk_file_write_start(context->write_class);

    if (rtn)
    {
        LOG(5)("[%s] create fail.\n", context->wsno_date);
        return 0;
    }
    {
        // printf("kkk = %s\n", context->wsno_keys);
        int count = sis_disk_class_set_key(context->write_class, true, context->wsno_keys, sis_sdslen(context->wsno_keys));
        LOG(5)
        ("keys = %d \n", count);
    }
    {
        int count = sis_disk_class_set_sdb(context->write_class, true, context->wsno_sdbs, sis_sdslen(context->wsno_sdbs));
        LOG(5)
        ("sdbs = %d \n", count);
    }
    if (iszip)
    {
        context->wsno_unzip = sdcdb_worker_create();
        sdcdb_worker_unzip_init(context->wsno_unzip, context, cb_unzip_info);
        sdcdb_worker_set_keys(context->wsno_unzip, context->wsno_keys);
        sdcdb_worker_set_sdbs(context->wsno_unzip, context->wsno_sdbs);
    }
    context->iswhead = 2;
    return 1;
}
static int cb_sub_start(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;
    
    sis_sdsfree(context->wsno_date);
    context->wsno_date = sis_sdsnew((char *)argv_);
    LOG(5)("read wfile start. %s\n", context->wsno_date);
    // 有可能没有收到订阅结束 但是文件已经被打开并写入了数据此时要关闭老的文件
     if (context->write_class->status == SIS_DISK_STATUS_OPENED)
    {
        sis_disk_file_write_stop(context->write_class);
    }
    context->iswhead = 1;
    return SIS_METHOD_OK;
}
static int cb_sub_stop(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;

    sis_disk_file_write_stop(context->write_class);

    if (context->wsno_unzip)
    {
        sdcdb_worker_destroy(context->wsno_unzip);
        context->wsno_unzip = NULL;
    }
    sis_sdsfree(context->wsno_date); context->wsno_date = NULL;
    sis_sdsfree(context->wsno_keys); context->wsno_keys = NULL;
    sis_sdsfree(context->wsno_sdbs); context->wsno_sdbs = NULL;
    context->iswhead = 0;
    return SIS_METHOD_OK;
}
static int cb_dict_keys(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;
    sis_sdsfree(context->wsno_keys);
    context->wsno_keys = sis_sdsdup(argv_);
    return SIS_METHOD_OK;
}
static int cb_dict_sdbs(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;
    sis_sdsfree(context->wsno_sdbs);
    context->wsno_sdbs = sis_sdsdup(argv_);
    return SIS_METHOD_OK;
}
static int cb_sisdb_bytes(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;
    _write_head(context, 0);
	// printf("%s\n",__func__);
    s_sis_db_chars *inmem = (s_sis_db_chars *)argv_;
    sis_disk_file_write_sdb(context->write_class, inmem->kname, inmem->sname, inmem->data, inmem->size);
    // s_v0_cf_snapshot *snapshot = (s_v0_cf_snapshot *)argv_;
        // snapshot->code, MARKET_SDB_CF_SNAPSHOT, 
        // snapshot->data, sizeof(s_v3_cf_snapshot));

	return SIS_METHOD_OK;
}
static int cb_sdcdb_compress(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;
    _write_head(context, 1);
	sdcdb_worker_unzip_set(context->wsno_unzip, (s_sdcdb_compress *)argv_);
    return SIS_METHOD_OK;
}
static int cb_unzip_info(void *source, int kidx, int sidx, char *in, size_t ilen)
{
    if (!in)
    {
        // 表示一个包解析完成
        return 0;
    }
    s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)source;
    s_sis_sds kname = sis_map_list_geti(context->wsno_unzip->keys, kidx);
    s_sis_dynamic_db *db = sis_map_list_geti(context->wsno_unzip->sdbs, sidx);
    if (!kname || !db)
    {
        return 0;
    }
    sis_disk_file_write_sdb(context->write_class, kname, db->name, in, ilen);
    return 0;
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
    // s_sisdb_wsno_cxt *context = (s_sisdb_wsno_cxt *)worker->context;

    s_sis_message *msg = (s_sis_message *)argv_;
    sis_message_set(msg, "source", worker, NULL);
    sis_message_set_method(msg, "cb_sub_start"      ,cb_sub_start);
    sis_message_set_method(msg, "cb_sub_stop"       ,cb_sub_stop);
    sis_message_set_method(msg, "cb_dict_sdbs"      ,cb_dict_sdbs);
    sis_message_set_method(msg, "cb_dict_keys"      ,cb_dict_keys);
    sis_message_set_method(msg, "cb_sdcdb_compress" ,cb_sdcdb_compress);
    sis_message_set_method(msg, "cb_sisdb_bytes"    ,cb_sisdb_bytes);

    return SIS_METHOD_OK; 
}
