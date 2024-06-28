
#include "worker.h"
#include "server.h"

#include <sis_modules.h>
#include <sisdb_rsno.h>
#include <sis_obj.h>
#include "sis_utils.h"

// 从行情流文件中获取数据源
static s_sis_method _sisdb_rsno_methods[] = {
  {"get",    cmd_sisdb_rsno_get, 0, NULL},
  {"getdb",  cmd_sisdb_rsno_getdb, 0, NULL},
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

    printf("%s : %d\n", __func__, context->status);
    if (context->status == SIS_RSNO_WORK || context->status == SIS_RSNO_CALL )
    {
        sisdb_rsno_sub_stop(context);
        context->status = SIS_RSNO_EXIT;
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
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;
    if (context->cb_sub_start)
    {
        char sdate[32];
        sis_llutoa(idate, sdate, 32, 10);
        context->cb_sub_start(context->cb_source, sdate);
    } 
    _speed_sno = sis_time_get_now_msec();
    // printf("======. %d cost : %lld\n", idate, _speed_sno);
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
    // if (context->status != SIS_RSNO_BREAK)
    {
        if (context->cb_sub_stop)
        {
            context->cb_sub_stop(context->cb_source, "0");
        } 
    }
}
static void cb_dict_keys(void *context_, void *key_, size_t size) 
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;
	s_sis_sds srckeys = sis_sdsnewlen((char *)key_, size);
    // printf("====%s \n === %s\n",srckeys, (char *)key_);
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
static void cb_bytedata(void *context_, int kidx_, int sidx_, void *out_, size_t olen_)
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;
    if (context->cb_sub_bytes)
    {
        s_sis_db_bytes inmem = {0};
        inmem.kidx= kidx_;
        inmem.sidx= sidx_;
        inmem.data = out_;
        inmem.size = olen_;
        context->cb_sub_bytes(context->cb_source, &inmem);
    }
}
// #include "stk_struct.v4.h"
// static int _read_nums = 0;
static void cb_chardata(void *context_, const char *kname_, const char *sname_, void *out_, size_t olen_)
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)context_;
    // _read_nums++;
    // if (_read_nums % 100000 == 0)
    // {
    //     printf("%s %s %zu | %d\n", kname_, sname_, olen_,  _read_nums);
    // }
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

/**
 * @brief 
 * @param argv_ 工作者上下文
 * @return void* 
 */
static void *_thread_snos_read_sub(void *argv_)
{
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)argv_;

    s_sis_disk_reader_cb *rsno_cb = SIS_MALLOC(s_sis_disk_reader_cb, rsno_cb);
    rsno_cb->cb_source = context;
    rsno_cb->cb_start = cb_start;
    rsno_cb->cb_dict_keys = cb_dict_keys;
    rsno_cb->cb_dict_sdbs = cb_dict_sdbs;
    if (context->cb_sub_bytes)
    {
        rsno_cb->cb_bytedata = cb_bytedata;
    }
    else
    {
        rsno_cb->cb_chardata = cb_chardata;
    }
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
/**
 * @brief 启动一天的行情订阅
 * @param context 工作者上下文
 */
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
        printf("stop sub..0.. %d %d\n", context->status, context->work_reader->status_sub);
        sis_disk_reader_unsub(context->work_reader);
        // 下面代码在线程中死锁
        while (context->status != SIS_RSNO_NONE)
        {
            printf("stop sub... %d %d\n", context->status, context->work_reader->status_sub);
            sis_sleep(1000);
        }
        printf("stop sub..1.. %d\n", context->status);
    }
}
///////////////////////////////////////////
//  method define
/////////////////////////////////////////

/**
 * @brief 将字段和回调函数设置从通信上下文中取出并设置到工作者上下文中
 * @param context 工作者上下文
 * @param msg 通信上下文
 */
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
    context->cb_source      = sis_message_get(msg, "cb_source");
    context->cb_sub_start   = sis_message_get_method(msg, "cb_sub_start"  );
    context->cb_sub_stop    = sis_message_get_method(msg, "cb_sub_stop"   );
    context->cb_dict_sdbs   = sis_message_get_method(msg, "cb_dict_sdbs"  );
    context->cb_dict_keys   = sis_message_get_method(msg, "cb_dict_keys"  );
    context->cb_sub_incrzip = sis_message_get_method(msg, "cb_sub_incrzip");
    context->cb_sub_chars   = sis_message_get_method(msg, "cb_sub_chars"  );
    context->cb_sub_bytes   = sis_message_get_method(msg, "cb_sub_bytes"  );
}
int cmd_sisdb_rsno_get(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)worker->context;

    s_sis_message *msg = (s_sis_message *)argv_; 
    // 设置表结构
    // 设置数据对象
    context->work_reader = sis_disk_reader_create(
        sis_sds_save_get(context->work_path), 
        sis_sds_save_get(context->work_name), 
        SIS_DISK_TYPE_SNO, NULL);
    
    s_sis_msec_pair pair; 
    int subdate = sis_message_get_int(msg, "sub-date");
    pair.start = (msec_t)sis_time_make_time(subdate, 0) * 1000;
    pair.stop = (msec_t)sis_time_make_time(subdate, 235959) * 1000 + 999;
    const char *subkeys = sis_message_get_str(msg, "sub-keys");
    const char *subsdbs = sis_message_get_str(msg, "sub-sdbs");
    LOG(5)("get sno open. [%d] %d %s %s\n", context->work_date, subdate, subkeys, subsdbs);
    s_sis_object *obj = sis_disk_reader_get_obj(context->work_reader, subkeys, subsdbs, &pair);

    s_sis_dynamic_db *db = sis_disk_reader_getdb(context->work_reader, subsdbs);
    LOG(5)("get sno stop. [%d] %p %p %p\n", context->work_date, context->work_reader, obj, db);
    if (db)
    {
        sis_dynamic_db_incr(db);
        sis_message_set(msg, "dbinfo", db, sis_dynamic_db_destroy);
    }
    sis_disk_reader_destroy(context->work_reader);
    context->work_reader = NULL;

    LOG(5)("get sno stop. ok [%d] %d %d\n", context->work_date, subdate, context->status);
    if (!obj)
    {
        return SIS_METHOD_NIL;
    }
    else
    {
        // sis_out_binary("ooo", SIS_OBJ_GET_CHAR(obj), SIS_OBJ_GET_SIZE(obj));
        sis_message_set(msg, "object", obj, sis_object_destroy);
    }
    return SIS_METHOD_OK;
}
int cmd_sisdb_rsno_getdb(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsno_cxt *context = (s_sisdb_rsno_cxt *)worker->context;

    s_sis_message *msg = (s_sis_message *)argv_; 
    // 设置表结构
    // 设置数据对象
    s_sis_disk_reader *reader = sis_disk_reader_create(
        sis_sds_save_get(context->work_path), 
        sis_sds_save_get(context->work_name), 
        SIS_DISK_TYPE_SNO, NULL);
    if (!reader)
    {
        return SIS_METHOD_NIL;
    }
    int subdate = sis_message_get_int(msg, "sub-date");
    // 这里不筛选 直接返回全部结构数据
    s_sis_object *obj = sis_disk_reader_get_sdbs(reader, subdate);
    sis_disk_reader_destroy(reader);
    if (!obj)
    {
        return SIS_METHOD_NIL;
    }
    s_sis_sds dbinfo = sis_sdsdup(SIS_OBJ_SDS(obj));
    sis_object_destroy(obj);
    sis_message_set(msg, "dbinfo", dbinfo, sis_sdsfree_call);
    return SIS_METHOD_OK;
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
    // context->status = SIS_RSNO_BREAK;
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

#if 0
// 测试 snapshot 转 新格式的例子
const char *sisdb_rsno = "\"sisdb_rsno\" : { \
    \"work-path\" : \"../../data/\" }";

#include "server.h"
#include "sis_db.h"
#include "stk_struct.v0.h"

int _recv_count = 0;

int cb_sub_start1(void *worker_, void *argv_)
{
    printf("%s : %s\n", __func__, (char *)argv_);
    _recv_count = 0;
    return 0;
}

int cb_sub_stop1(void *worker_, void *argv_)
{
    printf("%s : %s\n", __func__, (char *)argv_);
    return 0;
}
int cb_dict_keys1(void *worker_, void *argv_)
{
    // printf("%s : %s\n", __func__, (char *)argv_);
    // printf("=====%s : \n", __func__);
    return 0;
}
int cb_dict_sdbs1(void *worker_, void *argv_)
{
    // printf("%s : %s\n", __func__, (char *)argv_);
    // printf("=====%s : \n", __func__);
    return 0;
}

int cb_sub_chars1(void *worker_, void *argv_)
{
    s_sis_db_chars *inmem = (s_sis_db_chars *)argv_;
    if (_recv_count % 1000000 == 0)
    {
        printf("%d %s %s : %d\n", _recv_count, inmem->kname, inmem->sname, inmem->size);
    }
    // if (!sis_strcasecmp(inmem->sname, "stk_snapshot"))
    // {
        // s_v4_stk_snapshot *snap = (s_v4_stk_snapshot *)inmem->data;
        // printf("%s %lld %d %lld\n", inmem->kname, snap->time, sis_zint32_i(snap->newp), snap->volume);
    // }
    if (!sis_strcasecmp(inmem->sname, "stk_orders") && inmem->kname[1] == 'H')
    {
        printf("inmem->sname %s \n", inmem->kname);
        // s_v4_stk_snapshot *snap = (s_v4_stk_snapshot *)inmem->data;
        // printf("%s %lld %d %lld\n", inmem->kname, snap->time, sis_zint32_i(snap->newp), snap->volume);
    }
    _recv_count++;
    return 0;
}

int main()
{
    sis_server_init();
    s_sis_worker *nowwork = NULL;
    {
        s_sis_json_handle *handle = sis_json_load(sisdb_rsno, sis_strlen(sisdb_rsno));
        nowwork = sis_worker_create(NULL, handle->node);
        sis_json_close(handle);
    }

    s_sis_message *msg = sis_message_create(); 
    sis_message_set(msg, "cb_source", nowwork, NULL);
    sis_message_set_int(msg, "sub-date", 20210601);
    sis_message_set_method(msg, "cb_sub_start"     ,cb_sub_start1      );
    sis_message_set_method(msg, "cb_sub_stop"      ,cb_sub_stop1       );
    sis_message_set_method(msg, "cb_dict_sdbs"     ,cb_dict_sdbs1      );
    sis_message_set_method(msg, "cb_dict_keys"     ,cb_dict_keys1      );
    sis_message_set_method(msg, "cb_sub_chars"     ,cb_sub_chars1    );
    sis_worker_command(nowwork, "sub", msg);
    sis_message_destroy(msg); 
    while (1)
    {
        sis_sleep(5000);
    }
    
    sis_worker_destroy(nowwork);
    sis_server_uninit();
}
#endif