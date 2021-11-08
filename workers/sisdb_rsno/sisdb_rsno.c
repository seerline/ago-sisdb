
#include "worker.h"
#include "server.h"

#include <sis_modules.h>
#include <sisdb_rsno.h>
#include <sis_obj.h>
#include "sis_utils.h"

// 从行情流文件中获取数据源
static s_sis_method _sisdb_rsno_methods[] = {
  {"sub",    cmd_sisdb_rsno_sub, 0, NULL},
  {"unsub",  cmd_sisdb_rsno_unsub, 0, NULL},
  {"setcb",  cmd_sisdb_rsno_setcb, 0, NULL}
};

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////
s_sis_modules sis_modules_sisdb_rsno = {
    sisdb_rsno_init,
    NULL,
    sisdb_rsno_working,
    NULL,
    sisdb_rsno_uninit,
    NULL,
    NULL,
    sizeof(_sisdb_rsno_methods)/sizeof(s_sis_method),
    _sisdb_rsno_methods,
};

bool sisdb_rsno_init(void *worker_, void *node_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)node_;
    s_sisdb_rsno_cxt *context = SIS_MALLOC(s_sisdb_rsno_cxt, context);
    worker->context = context;

    context->work_date = sis_json_get_int(node, "work-date", sis_time_get_idate(0));
    {
        context->work_path = sis_sds_save_create(sis_json_get_str(node, "work-path"), "data");   
        context->work_name = sis_sds_save_create(sis_json_get_str(node, "work-name"), "snodb");    
    }
    {
        const char *str = sis_json_get_str(node, "sub-sdbs");
        if (str)
        {
            context->work_sdbs = sis_sdsnew(str);
        }
        else
        {
            context->work_sdbs = sis_sdsnew("*");
        }
    }
    {     
        const char *str = sis_json_get_str(node, "sub-keys");
        if (str)
        {
            context->work_keys = sis_sdsnew(str);
        }
        else
        {
            context->work_keys = sis_sdsnew("*");
        }    
    }
    
    return true;
}

void sisdb_rsno_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)worker->context;

    sisdb_rsno_sub_stop(context);
    context->status = SIS_RSNO_EXIT;

    sis_sds_save_destroy(context->work_path);
    sis_sds_save_destroy(context->work_name);
    sis_sdsfree(context->work_keys);
    sis_sdsfree(context->work_sdbs);
    sis_sdsfree(context->ziper_keys);
    sis_sdsfree(context->ziper_sdbs);

    if (context->work_ziper)
    {
        sisdb_incr_zip_stop(context->work_ziper);
        sisdb_incr_destroy(context->work_ziper);
        context->work_ziper = NULL;
    }

    sis_free(context);
    worker->context = NULL;
}


///////////////////////////////////////////
//  callback define begin
///////////////////////////////////////////
static msec_t _speed_sno = 0;

static void cb_start(void *context_, int idate)
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;
    if (context->cb_sub_start)
    {
        char sdate[32];
        sis_llutoa(idate, sdate, 32, 10);
        context->cb_sub_start(context->cb_source, sdate);
    } 
    _speed_sno = sis_time_get_now_msec();
}
static void cb_stop(void *context_, int idate)
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;
    printf("sno sub ok. %d cost : %lld\n", idate, sis_time_get_now_msec() - _speed_sno);
     // stop 放这里
    if (context->work_ziper)
    {
        sisdb_incr_zip_stop(context->work_ziper);
        sisdb_incr_destroy(context->work_ziper);
        context->work_ziper = NULL;
    }
    if (context->cb_sub_stop)
    {
        char sdate[32];
        sis_llutoa(idate, sdate, 32, 10);
        context->cb_sub_stop(context->cb_source, sdate);
    } 
}
static void cb_break(void *context_, int idate)
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;
    printf("sno sub break. %d cost : %lld\n", idate, sis_time_get_now_msec() - _speed_sno);
     // stop 放这里
    if (context->work_ziper)
    {
        sisdb_incr_zip_stop(context->work_ziper);
        sisdb_incr_destroy(context->work_ziper);
        context->work_ziper = NULL;
    }
    if (context->cb_sub_stop)
    {
        context->cb_sub_stop(context->cb_source, "0");
    } 
}
static void cb_dict_keys(void *context_, void *key_, size_t size) 
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;
	s_sis_sds srckeys = sis_sdsnewlen((char *)key_, size);
    sis_sdsfree(context->ziper_keys);
	context->ziper_keys = sis_match_key(context->work_keys, srckeys);
    if (!context->ziper_keys)
    {
        context->ziper_keys =  sis_sdsdup(srckeys);
    } 
    if (context->cb_dict_keys)
    {
        context->cb_dict_keys(context->cb_source, context->ziper_keys);
    } 
	sis_sdsfree(srckeys);
}
static void cb_dict_sdbs(void *context_, void *sdb_, size_t size)  
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;
	s_sis_sds srcsdbs = sis_sdsnewlen((char *)sdb_, size);
	sis_sdsfree(context->ziper_sdbs);
    context->ziper_sdbs = sis_match_sdb_of_sds(context->work_sdbs, srcsdbs);
    if (!context->ziper_sdbs)
    {
        context->ziper_sdbs =  sis_sdsdup(srcsdbs);
    } 
    if (context->cb_dict_sdbs)
    {
        context->cb_dict_sdbs(context->cb_source, context->ziper_sdbs);
    } 
	sis_sdsfree(srcsdbs); 
}

static int cb_encode(void *context_, char *in_, size_t ilen_)
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;
    if (context->cb_sub_inctzip)
    {
        s_sis_db_incrzip inmem = {0};
        inmem.data = (uint8 *)in_;
        inmem.size = ilen_;
        inmem.init = sis_incrzip_isinit(inmem.data, inmem.size);
        context->cb_sub_inctzip(context->cb_source, &inmem);
    }
    return 0;
} 
// #include "stk_struct.v4.h"
static int _read_nums = 0;
static void cb_chardata(void *context_, const char *kname_, const char *sname_, void *out_, size_t olen_)
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;
    _read_nums++;
    if (_read_nums % 100000 == 0)
    {
        printf("%s %s %zu | %d\n", kname_, sname_, olen_,  _read_nums);
    }
    // sis_out_binary(sname_, out_, olen_);
    // if (!sis_strcasecmp(kname_, "SH600600") || !sis_strcasecmp(kname_, "SZ000001"))
    // {
    //     if (!sis_strcasecmp(sname_, "stk_transact"))
    //     {
    //         s_v4_stk_transact *transact = (s_v4_stk_transact *)out_;
    //         printf("[transact] %s: %zu %c %c\n", kname_, olen_, transact->flag, transact->type);
    //     }
    //     if (!sis_strcasecmp(sname_, "stk_orders"))
    //     {
    //         s_v4_stk_orders *orders = (s_v4_stk_orders *)out_;
    //         printf("[orders] %s: %zu %c %c\n", kname_, olen_, orders->flag, orders->type);
    //     }
    // }
    // if (!sis_strcasecmp(kname_, "SH601318")|| !sis_strcasecmp(kname_, "SH688981")||!sis_strcasecmp(kname_,"SZ300987"))
    // {
    //     if (!sis_strcasecmp(sname_, "stk_snapshot"))
    //     {
    //         s_v4_stk_snapshot *snapshot = (s_v4_stk_snapshot *)out_;
    //         printf("--%s %s %zu | %6d %5d %10d\n", kname_, sname_, olen_, sis_time_get_itime(snapshot->time/1000), snapshot->newp, snapshot->volume);
    //     }
    //     else
    //     {
    //         printf("--%s %s %zu \n", kname_, sname_, olen_);
    //     }
    // }
    // else
    // {
    //     return ;
    // }
    if (context->cb_sub_chars)
    {
        s_sis_db_chars inmem = {0};
        inmem.kname= kname_;
        inmem.sname= sname_;
        inmem.data = out_;
        inmem.size = olen_;
        context->cb_sub_chars(context->cb_source, &inmem);
    }
    if (context->cb_sub_inctzip)
    {
        if (!context->work_ziper)
        {
            context->work_ziper = sisdb_incr_create();
    	    sisdb_incr_set_keys(context->work_ziper, context->ziper_keys);
    	    sisdb_incr_set_sdbs(context->work_ziper, context->ziper_sdbs);
    	    sisdb_incr_zip_start(context->work_ziper, context, cb_encode);
        }
        int kidx = sisdb_incr_get_kidx(context->work_ziper, kname_);
        int sidx = sisdb_incr_get_sidx(context->work_ziper, sname_);
        if (kidx < 0 || sidx < 0)
        {
            return ;
        }
        sisdb_incr_zip_set(context->work_ziper, kidx, sidx, out_, olen_);
    }
} 

///////////////////////////////////////////
//  callback define end.
///////////////////////////////////////////
static void *_thread_snos_read_sub(void *argv_)
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)argv_;

    s_sis_disk_reader_cb *rsno_cb = SIS_MALLOC(s_sis_disk_reader_cb, rsno_cb);
    rsno_cb->cb_source = context;
    rsno_cb->cb_start = cb_start;
    rsno_cb->cb_dict_keys = cb_dict_keys;
    rsno_cb->cb_dict_sdbs = cb_dict_sdbs;
    rsno_cb->cb_chardata = cb_chardata;
    rsno_cb->cb_stop = cb_stop;
    rsno_cb->cb_break = cb_break;

    context->work_reader = sis_disk_reader_create(
        sis_sds_save_get(context->work_path), 
        sis_sds_save_get(context->work_name), 
        SIS_DISK_TYPE_SNO, rsno_cb);

    LOG(5)("sub sno open. [%d] %d\n", context->work_date, context->status);
    sis_disk_reader_sub_sno(context->work_reader, context->work_keys, context->work_sdbs, context->work_date);
    LOG(5)("sub sno stop. [%d] %d\n", context->work_date, context->status);

    sis_disk_reader_destroy(context->work_reader);
    context->work_reader = NULL;

    sis_free(rsno_cb);

    if (context->work_ziper)
    {
        sisdb_incr_zip_stop(context->work_ziper);
        sisdb_incr_destroy(context->work_ziper);
        context->work_ziper = NULL;
    }
    LOG(5)("sub sno stop. ok [%d] %d\n", context->work_date, context->status);
    context->status = SIS_RSNO_NONE;
    return NULL;
}

void sisdb_rsno_sub_start(s_sisdb_rsno_cxt *context) 
{
    // 有值就干活 完毕后释放
    if (context->status == SIS_RSNO_WORK)
    {
        _thread_snos_read_sub(context);
    }
    else
    {
        sis_thread_create(_thread_snos_read_sub, context, &context->work_thread);
    }
}
void sisdb_rsno_sub_stop(s_sisdb_rsno_cxt *context)
{
    if (context->work_reader)
    {
        sis_disk_reader_unsub(context->work_reader);
        // 下面代码在线程中死锁
        while (context->status != SIS_RSNO_NONE)
        {
            printf("stop sub... %d\n", context->status);
            sis_sleep(1000);
        }
        printf("stop sub..1.. %d\n", context->status);
    }
}
///////////////////////////////////////////
//  method define
/////////////////////////////////////////
void _sisdb_rsno_init(s_sisdb_rsno_cxt *context, s_sis_message *msg)
{
    sis_sds_save_set(context->work_path, sis_message_get_str(msg, "work-path"));
    sis_sds_save_set(context->work_name, sis_message_get_str(msg, "work-name"));
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
    if (sis_message_exist(msg, "sub-date"))
    {
        context->work_date = sis_message_get_int(msg, "sub-date");
    }
    else
    {
        context->work_date = sis_time_get_idate(0);
    }
    context->cb_source      = sis_message_get(msg, "source");
    context->cb_sub_start   = sis_message_get_method(msg, "cb_sub_start"  );
    context->cb_sub_stop    = sis_message_get_method(msg, "cb_sub_stop"   );
    context->cb_dict_sdbs   = sis_message_get_method(msg, "cb_dict_sdbs"  );
    context->cb_dict_keys   = sis_message_get_method(msg, "cb_dict_keys"  );
    context->cb_sub_inctzip = sis_message_get_method(msg, "cb_sub_incrzip");
    context->cb_sub_chars   = sis_message_get_method(msg, "cb_sub_chars"  );
}
int cmd_sisdb_rsno_sub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)worker->context;

    SIS_WAIT_OR_EXIT(context->status == SIS_RSNO_NONE);  

    s_sis_message *msg = (s_sis_message *)argv_; 
    if (!msg)
    {
        return SIS_METHOD_ERROR;
    }
    _sisdb_rsno_init(context, msg);
    
    context->status = SIS_RSNO_CALL;
    
    sisdb_rsno_sub_start(context);

    return SIS_METHOD_OK;
}
int cmd_sisdb_rsno_unsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)worker->context;

    sisdb_rsno_sub_stop(context);

    return SIS_METHOD_OK;
}

int cmd_sisdb_rsno_setcb(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)worker->context;

    if (context->status != SIS_RSNO_NONE)
    {
        return SIS_METHOD_ERROR;
    }
    s_sis_message *msg = (s_sis_message *)argv_; 
    if (!msg)
    {
        return SIS_METHOD_ERROR;
    }
    _sisdb_rsno_init(context, msg);
    
    context->status = SIS_RSNO_WORK;

    return SIS_METHOD_OK;
}

void sisdb_rsno_working(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)worker->context;
    
    SIS_WAIT_OR_EXIT(context->status == SIS_RSNO_WORK);  
    if (context->status == SIS_RSNO_WORK)
    {
        LOG(5)("sub history start. [%d]\n", context->work_date);
        sisdb_rsno_sub_start(context);
        LOG(5)("sub history end. [%d]\n", context->work_date);
    }
}