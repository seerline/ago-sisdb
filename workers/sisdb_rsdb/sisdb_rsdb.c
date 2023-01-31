#include "worker.h"
#include "server.h"

#include "sis_modules.h"
#include "sisdb_rsdb.h"
#include "sis_utils.h"
#include <sis_obj.h>

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method _sisdb_rsdb_methods[] = {
    {"get",    cmd_sisdb_rsdb_get,   0, NULL},
    {"sub",    cmd_sisdb_rsdb_sub,   0, NULL},
    {"unsub",  cmd_sisdb_rsdb_unsub, 0, NULL},
    {"setcb",  cmd_sisdb_rsdb_setcb, 0, NULL},
};
// 通用文件存取接口
s_sis_modules sis_modules_sisdb_rsdb = {
    sisdb_rsdb_init,
    NULL,
    sisdb_rsdb_working,
    NULL,
    sisdb_rsdb_uninit,
    NULL,
    NULL,
    sizeof(_sisdb_rsdb_methods) / sizeof(s_sis_method),
    _sisdb_rsdb_methods,
};

bool sisdb_rsdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_rsdb_cxt *context = SIS_MALLOC(s_sisdb_rsdb_cxt, context);
    worker->context = context;

    s_sis_json_node *work_date = sis_json_cmp_child_node(node, "work-date");
    if (work_date)
    {
        context->work_date.start = sis_json_get_int(work_date, "0", 0);
        context->work_date.stop  = sis_json_get_int(work_date, "1", 0);
    }
    {
        context->work_path = sis_sds_save_create(sis_json_get_str(node, "work-path"), "data");   
        context->work_name = sis_sds_save_create(sis_json_get_str(node, "work-name"), node->key);    
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
void sisdb_rsdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)worker->context;

    if (context->status == SIS_RSDB_WORK || context->status == SIS_RSDB_CALL )
    {
        sisdb_rsdb_sub_stop(context);
        context->status = SIS_RSDB_EXIT;
    }

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
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)context_;
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
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)context_;
    printf("sdb sub ok. %d cost : %lld\n", idate, sis_time_get_now_msec() - _speed_sno);
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
    context->work_date.move = idate;
}
static void cb_break(void *context_, int idate)
{
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)context_;
    printf("sdb sub break. %d cost : %lld\n", idate, sis_time_get_now_msec() - _speed_sno);
     // stop 放这里
    if (context->work_ziper)
    {
        sisdb_incr_zip_stop(context->work_ziper);
        sisdb_incr_destroy(context->work_ziper);
        context->work_ziper = NULL;
    }
    // if (context->status != SIS_RSNO_BREAK)
    {
        if (context->cb_sub_stop)
        {
            context->cb_sub_stop(context->cb_source, "0");
        } 
    }
    context->work_date.move = 0;
}
static void cb_dict_keys(void *context_, void *key_, size_t size) 
{
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)context_;
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
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)context_;
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
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)context_;
    if (context->cb_sub_incrzip)
    {
        s_sis_db_incrzip inmem = {0};
        inmem.data = (uint8 *)in_;
        inmem.size = ilen_;
        inmem.init = sis_incrzip_isinit(inmem.data, inmem.size);
        context->cb_sub_incrzip(context->cb_source, &inmem);
    }
    return 0;
} 
// #include "stk_struct.v3.h"
// static int _read_nums = 0;
static void cb_chardata(void *context_, const char *kname_, const char *sname_, void *out_, size_t olen_)
{
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)context_;
    // _read_nums++;
    // if (_read_nums % 100000 == 0)
    // {
    //     printf("%s %s %zu | %d\n", kname_, sname_, olen_,  _read_nums);
    // }
    // if (!sis_strcasecmp(kname_, "SH600600") && !sis_strcasecmp(sname_, "stk_snapshot"))
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
    if (context->cb_sub_incrzip)
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
static void *_thread_rsdb_read_sub(void *argv_)
{
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)argv_;

    s_sis_disk_reader_cb *rsdb_cb = SIS_MALLOC(s_sis_disk_reader_cb, rsdb_cb);
    rsdb_cb->cb_source = context;
    rsdb_cb->cb_start = cb_start;
    rsdb_cb->cb_dict_keys = cb_dict_keys;
    rsdb_cb->cb_dict_sdbs = cb_dict_sdbs;
    rsdb_cb->cb_chardata = cb_chardata;
    rsdb_cb->cb_stop = cb_stop;
    rsdb_cb->cb_break = cb_break;

    context->work_reader = sis_disk_reader_create(
        sis_sds_save_get(context->work_path), 
        sis_sds_save_get(context->work_name), 
        SIS_DISK_TYPE_SDB, rsdb_cb);

    LOG(5)("sub sdb open. [%d]\n", context->work_date);
    s_sis_msec_pair smsec;
    smsec.start = sis_time_make_time(context->work_date.start, 0) * 1000;
    smsec.stop = sis_time_make_time(context->work_date.stop, 235959) * 1000 + 999;
    sis_disk_reader_sub_sdb(context->work_reader, context->work_keys, context->work_sdbs, &smsec);
    LOG(5)("sub sdb stop. [%d]\n", context->work_date);

    sis_disk_reader_destroy(context->work_reader);
    context->work_reader = NULL;

    sis_free(rsdb_cb);

    if (context->work_ziper)
    {
        sisdb_incr_zip_stop(context->work_ziper);
        sisdb_incr_destroy(context->work_ziper);
        context->work_ziper = NULL;
    }

    LOG(5)("sub sdb stop. ok [%d - %d] %d\n", context->work_date.start, context->work_date.stop, context->status);
    context->status = SIS_RSDB_NONE;
    return NULL;
}

void sisdb_rsdb_sub_start(s_sisdb_rsdb_cxt *context) 
{
    // 有值就干活 完毕后释放
    if (context->status == SIS_RSDB_WORK)
    {
        _thread_rsdb_read_sub(context);
    }
    else
    {
        sis_thread_create(_thread_rsdb_read_sub, context, &context->work_thread);
    }
}
void sisdb_rsdb_sub_stop(s_sisdb_rsdb_cxt *context)
{
    if (context->work_reader)
    {
        sis_disk_reader_unsub(context->work_reader);
        while (context->status != SIS_RSDB_NONE)
        {
            sis_sleep(30);
        }
    }
}
s_sis_object *sisdb_rsdb_get_obj(s_sisdb_rsdb_cxt *context, int rfmt)
{
    context->work_reader = sis_disk_reader_create(
        sis_sds_save_get(context->work_path), 
        sis_sds_save_get(context->work_name), 
        SIS_DISK_TYPE_SDB, NULL);

    if (sis_disk_reader_open(context->work_reader))
    {
        sis_disk_reader_destroy(context->work_reader);
        context->work_reader = NULL;
        return NULL;
    }
    LOG(5)("get sdb open. [%d - %d]\n", context->work_date.start, context->work_date.stop);
    s_sis_msec_pair smsec;
    smsec.start = sis_time_make_time(context->work_date.start, 0) * 1000;
    smsec.stop = sis_time_make_time(context->work_date.stop, 235959) * 1000 + 999;
    s_sis_object *obj = sis_disk_reader_get_obj(context->work_reader, context->work_keys, context->work_sdbs, &smsec);
    if (obj)
    {
        LOG(5)("get sdb stop. [%d] %p %zu\n", context->work_date.stop, obj, SIS_OBJ_GET_SIZE(obj));
        // 这里拿到的应该是原始二进制数据
        // sis_out_binary("..1..", SIS_OBJ_GET_CHAR(obj), SIS_OBJ_GET_SIZE(obj));
        if (rfmt & SISDB_FORMAT_CHARS)
        {
            s_sis_dynamic_db *db = sis_disk_reader_getdb(context->work_reader, context->work_sdbs);
            if (db)
            {
                // 这里未来可以做格式转换处理
                s_sis_sds omem = sis_db_format_sds(db, NULL, rfmt, SIS_OBJ_GET_CHAR(obj),SIS_OBJ_GET_SIZE(obj), 0);
                sis_object_decr(obj);
                obj = sis_object_create(SIS_OBJECT_SDS, omem);
            }
            else
            {
                sis_object_decr(obj);
                obj = NULL;
            }			
        }
    }

    sis_disk_reader_close(context->work_reader);
    sis_disk_reader_destroy(context->work_reader);
    context->work_reader = NULL;
    return obj;
}
///////////////////////////////////////////
//  method define
/////////////////////////////////////////
void _sisdb_rsdb_init(s_sisdb_rsdb_cxt *context, s_sis_message *msg)
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
    int curdate = sis_time_get_idate(0);
    if (sis_message_exist(msg, "sub-date"))
    {
        curdate = sis_message_get_int(msg, "sub-date");
    }
    if (sis_message_exist(msg, "start-date"))
    {
        context->work_date.start = sis_message_get_int(msg, "start-date");
    }
    else
    {
        context->work_date.start = curdate;
    }
    if (sis_message_exist(msg, "stop-date"))
    {
        context->work_date.stop = sis_message_get_int(msg, "stop-date");
    }
    else
    {
        context->work_date.stop = curdate;
    }
    context->cb_source      = sis_message_get(msg, "cb_source");
    context->cb_sub_start   = sis_message_get_method(msg, "cb_sub_start"  );
    context->cb_sub_stop    = sis_message_get_method(msg, "cb_sub_stop"   );
    context->cb_dict_sdbs   = sis_message_get_method(msg, "cb_dict_sdbs"  );
    context->cb_dict_keys   = sis_message_get_method(msg, "cb_dict_keys"  );
    context->cb_sub_incrzip = sis_message_get_method(msg, "cb_sub_incrzip");
    context->cb_sub_chars   = sis_message_get_method(msg, "cb_sub_chars"  );
}
int cmd_sisdb_rsdb_get(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)worker->context;

    SIS_WAIT_OR_EXIT(context->status == SIS_RSDB_NONE); 

    s_sis_message *msg = (s_sis_message *)argv_; 
    if (!msg)
    {
        return SIS_METHOD_ERROR;
    }
    _sisdb_rsdb_init(context, msg);
    context->status = SIS_RSDB_CALL;

    int rfmt = sis_message_get_int(msg, "format");
    s_sis_object *obj = sisdb_rsdb_get_obj(context, rfmt);
    if (obj)
    {
        sis_message_set(msg, "info", obj, sis_object_destroy);
    }

    context->status = SIS_RSDB_NONE;
    return SIS_METHOD_OK;
}

int cmd_sisdb_rsdb_sub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)worker->context;

    SIS_WAIT_OR_EXIT(context->status == SIS_RSDB_NONE);  

    s_sis_message *msg = (s_sis_message *)argv_; 
    if (!msg)
    {
        return SIS_METHOD_ERROR;
    }
    _sisdb_rsdb_init(context, msg);
    
    context->status = SIS_RSDB_CALL;
    
    sisdb_rsdb_sub_start(context);

    return SIS_METHOD_OK;
}
int cmd_sisdb_rsdb_unsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)worker->context;

    sisdb_rsdb_sub_stop(context);

    return SIS_METHOD_OK;
}

int cmd_sisdb_rsdb_setcb(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)worker->context;

    if (context->status != SIS_RSDB_NONE)
    {
        return SIS_METHOD_ERROR;
    }
    s_sis_message *msg = (s_sis_message *)argv_; 
    if (!msg)
    {
        return SIS_METHOD_ERROR;
    }
    _sisdb_rsdb_init(context, msg);
    
    context->status = SIS_RSDB_WORK;

    return SIS_METHOD_OK;
}

void sisdb_rsdb_working(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)worker->context;
    
    SIS_WAIT_OR_EXIT(context->status == SIS_RSDB_WORK);  
	if (context->work_date.start > 0 && context->work_date.stop >= context->work_date.start)
	{
		int start = context->work_date.start;
		int stop = context->work_date.stop;
        context->work_date.move = start - 1;
        while (start <= stop)
        {
            SIS_EXIT_SIGNAL
            msec_t costmsec = sis_time_get_now_msec();
            LOG(5)("sub history start. [%d]\n", start);
            sisdb_rsdb_sub_start(context);
            SIS_WAIT_OR_EXIT(context->work_date.move == start || context->work_date.move == 0);
            LOG(5)("sub history end. [%d] cost :%lld\n", start, sis_time_get_now_msec() - costmsec);
            if (context->work_date.move == 0)
            {
                break;
            }
            start = sis_time_get_offset_day(start, 1);
        }
	}
    // if (context->status == SIS_RSDB_WORK)
    // {
    //     LOG(5)("sub history start. [%d]\n", context->work_date.start);
    //     sisdb_rsdb_sub_start(context);
    //     LOG(5)("sub history end. [%d]\n", context->work_date.start);
    // }
}