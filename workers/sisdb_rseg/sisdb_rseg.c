
#include "worker.h"
#include "server.h"

#include <sis_modules.h>
#include <sisdb_rseg.h>
#include <sis_obj.h>
#include "sis_utils.h"

// 从行情流文件中获取数据源
static s_sis_method _sisdb_rseg_methods[] = {
  {"get",    cmd_sisdb_rseg_get, 0, NULL},
  {"sub",    cmd_sisdb_rseg_sub, 0, NULL},
  {"unsub",  cmd_sisdb_rseg_unsub, 0, NULL},
};

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////
s_sis_modules sis_modules_sisdb_rseg = {
    sisdb_rseg_init,
    NULL,
    NULL,
    NULL,
    sisdb_rseg_uninit,
    NULL,
    NULL,
    sizeof(_sisdb_rseg_methods)/sizeof(s_sis_method),
    _sisdb_rseg_methods,
};

bool sisdb_rseg_init(void *worker_, void *node_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)node_;
    s_sisdb_rseg_cxt *context = SIS_MALLOC(s_sisdb_rseg_cxt, context);
    worker->context = context;

    context->work_path = sis_sds_save_create(sis_json_get_str(node, "work-path"), "data");   
    context->work_name = sis_sds_save_create(sis_json_get_str(node, "work-name"), "segdb");    

    context->status = SIS_RSEG_NONE;

    return true;
}

void sisdb_rseg_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rseg_cxt *context = (s_sisdb_rseg_cxt *)worker->context;

    // printf("%s : %d\n", __func__, context->status);
    if (context->status == SIS_RSEG_WORK)
    {
        sisdb_rseg_sub_stop(context);
        context->status = SIS_RSEG_EXIT;
    }
    sisdb_rseg_clear(context);

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
void sisdb_rseg_clear(s_sisdb_rseg_cxt *context)
{
    sis_sdsfree(context->curr_keys); context->curr_keys = NULL;
    sis_sdsfree(context->curr_sdbs); context->curr_sdbs = NULL;
    if (context->maps_sdbs)
    {
        sis_map_list_destroy(context->maps_sdbs);    
        context->maps_sdbs = NULL;
    }
}

static msec_t _speed_sno = 0;
static void cb_start(void *context_, int idate)
{
    s_sisdb_rseg_cxt *context = (s_sisdb_rseg_cxt *)context_;
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
    s_sisdb_rseg_cxt *context = (s_sisdb_rseg_cxt *)context_;
    // printf("sno sub ok. %d cost : %lld\n", idate, sis_time_get_now_msec() - _speed_sno);
    if (context->cb_sub_stop)
    {
        char sdate[32];
        sis_llutoa(idate, sdate, 32, 10);
        context->cb_sub_stop(context->cb_source, sdate);
    } 
    sisdb_rseg_clear(context);
}
static void cb_break(void *context_, int idate)
{
    s_sisdb_rseg_cxt *context = (s_sisdb_rseg_cxt *)context_;
    // printf("sno sub break. %d cost : %lld\n", idate, sis_time_get_now_msec() - _speed_sno);
    if (context->cb_sub_stop)
    {
        context->cb_sub_stop(context->cb_source, "0");
    } 
    sisdb_rseg_clear(context);
}
static void cb_dict_keys(void *context_, void *key_, size_t size) 
{
    s_sisdb_rseg_cxt *context = (s_sisdb_rseg_cxt *)context_;
	s_sis_sds srckeys = sis_sdsnewlen((char *)key_, size);
    // printf("====%s \n === %s\n",srckeys, (char *)key_);
    sis_sdsfree(context->curr_keys);
	context->curr_keys = sis_match_key(context->work_keys, srckeys);
    if (!context->curr_keys)
    {
        context->curr_keys =  sis_sdsdup(srckeys);
    } 
    if (context->cb_dict_keys)
    {
        context->cb_dict_keys(context->cb_source, context->curr_keys);
    } 
	sis_sdsfree(srckeys);
}
static void cb_dict_sdbs(void *context_, void *sdb_, size_t size)  
{
    s_sisdb_rseg_cxt *context = (s_sisdb_rseg_cxt *)context_;
	s_sis_sds srcsdbs = sis_sdsnewlen((char *)sdb_, size);
	sis_sdsfree(context->curr_sdbs);
    context->curr_sdbs = sis_match_sdb_of_sds(context->work_sdbs, srcsdbs);
    if (!context->curr_sdbs)
    {
        context->curr_sdbs =  sis_sdsdup(srcsdbs);
    } 
    if (context->cb_dict_sdbs)
    {
        context->cb_dict_sdbs(context->cb_source, context->curr_sdbs);
    } 
	sis_sdsfree(srcsdbs); 
}

// #include "stk_struct.v4.h"
// static int _read_nums = 0;
static void cb_chardata(void *context_, const char *kname_, const char *sname_, void *out_, size_t olen_)
{
    s_sisdb_rseg_cxt *context = (s_sisdb_rseg_cxt *)context_;
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
} 

///////////////////////////////////////////
//  callback define end.
///////////////////////////////////////////

int sisdb_rseg_get_style(s_sisdb_rseg_cxt *context)
{
    int style = SIS_SEG_FSEC_INCR;
    if (!context->work_sdbs || context->work_sdbs[0] == '*')
    {
        int count = sis_map_list_getsize(context->maps_sdbs);
        for (int i = 0; i < count; i++)
        {
            s_sis_dynamic_db *db = sis_map_list_geti(context->maps_sdbs, i);
            style = sisdb_wseg_get_style(db);
            if (style != SIS_SEG_FSEC_INCR)
            {
                break;
            }
        }  
    }
    else
    {
        s_sis_string_list *klist = sis_string_list_create();
        sis_string_list_load(klist, context->work_sdbs, sis_sdslen(context->work_sdbs), ",");
        int count = sis_string_list_getsize(klist);
        for (int i = 0; i < count; i++)
        {
            const char *dbname = sis_string_list_get(klist, i);
            s_sis_dynamic_db *db = sis_map_list_get(context->maps_sdbs, dbname);
            style = sisdb_wseg_get_style(db);
            if (style != SIS_SEG_FSEC_INCR)
            {
                break;
            }
        }
        sis_string_list_destroy(klist);
    }
    return style;
}

static void *sis_thread_snos_read_sub(void *argv_)
{
    s_sisdb_rseg_cxt *context = (s_sisdb_rseg_cxt *)argv_;

    s_sis_disk_reader_cb *rsno_cb = SIS_MALLOC(s_sis_disk_reader_cb, rsno_cb);
    rsno_cb->cb_source = context;
    rsno_cb->cb_start = cb_start;
    rsno_cb->cb_dict_keys = cb_dict_keys;
    rsno_cb->cb_dict_sdbs = cb_dict_sdbs;
    rsno_cb->cb_chardata = cb_chardata;
    rsno_cb->cb_stop = cb_stop;
    rsno_cb->cb_break = cb_break;

    // 获得当前订阅的时区 只支持一个时区
    int index = sisdb_rseg_get_style(context);
    // 设置数据对象
    s_sis_sds rpath = sis_sdsdup(sis_sds_save_get(context->work_path));
    rpath = sis_sdscatfmt(rpath, "/%s/", sis_sds_save_get(context->work_name));
    context->work_reader = sis_disk_reader_create(rpath, sisdb_wseg_get_sname(index), SIS_DISK_TYPE_SNO, rsno_cb);

    LOG(5)("sub sno open. [%d] %d\n", context->work_date, context->status);
    sis_disk_reader_sub_sno(context->work_reader, context->work_keys, context->work_sdbs, context->work_date);
    LOG(5)("sub sno stop. [%d] %d\n", context->work_date, context->status);

    sis_disk_reader_destroy(context->work_reader);
    context->work_reader = NULL;

    sis_free(rsno_cb);

    LOG(5)("sub sno stop. ok [%d] %d\n", context->work_date, context->status);
    context->status = SIS_RSEG_NONE;
    return NULL;
}

void sisdb_rseg_sub_stop(s_sisdb_rseg_cxt *context)
{
    if (context->status == SIS_RSEG_WORK)
    {
        if (context->work_reader)
        {
            printf("stop sub..0.. %d %d\n", context->status, context->work_reader->status_sub);
            sis_disk_reader_unsub(context->work_reader);
            // 下面代码在线程中死锁
            while (context->status != SIS_RSEG_NONE)
            {
                printf("stop sub... %d %d\n", context->status, context->work_reader->status_sub);
                sis_sleep(1000);
            }
            printf("stop sub..1.. %d\n", context->status);
        }
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
void _sisdb_rseg_init(s_sisdb_rseg_cxt *context, s_sis_message *msg)
{
    sis_sds_save_set(context->work_path, sis_message_get_str(msg, "work-path"));
    sis_sds_save_set(context->work_name, sis_message_get_str(msg, "work-name"));
    
    int subdate = sis_time_get_idate(0);
    if (sis_message_exist(msg, "sub-date"))
    {
        subdate = sis_message_get_int(msg, "sub-date");
    }
    // printf("=== %d %d %lld \n", subdate, sis_message_exist(msg, "sub-date"), sis_message_get_int(msg, "sub-date"));
    if (!context->maps_sdbs || context->work_date == 0 || context->work_date != subdate)
    {
        sisdb_rseg_init_sdbs(context, subdate);
        context->work_date = subdate;
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
    context->cb_source      = sis_message_get(msg, "cb_source");
    context->cb_sub_start   = sis_message_get_method(msg, "cb_sub_start"  );
    context->cb_sub_stop    = sis_message_get_method(msg, "cb_sub_stop"   );
    context->cb_dict_sdbs   = sis_message_get_method(msg, "cb_dict_sdbs"  );
    context->cb_dict_keys   = sis_message_get_method(msg, "cb_dict_keys"  );
    context->cb_sub_chars   = sis_message_get_method(msg, "cb_sub_chars"  );
}
int sisdb_rseg_init_sdbs(s_sisdb_rseg_cxt *context, int idate)
{
    if (!context->maps_sdbs)
    {
        context->maps_sdbs = sis_map_list_create(sis_dynamic_db_destroy);
    }
    sis_map_list_clear(context->maps_sdbs);
    // 读取全部的数据结构
    s_sis_sds rpath = sis_sdsdup(sis_sds_save_get(context->work_path));
    rpath = sis_sdscatfmt(rpath, "/%s/", sis_sds_save_get(context->work_name));
    for (int i = 0; i < 2; i++)
    {
        s_sis_disk_reader *reader = sis_disk_reader_create(rpath, sisdb_wseg_get_sname(i), SIS_DISK_TYPE_SNO, NULL);
        if (!reader)
        {
            continue;
        }
        s_sis_object *obj = sis_disk_reader_get_sdbs(reader, idate);
        if (obj)
        {
            printf("read sdbs: %s \n", SIS_OBJ_SDS(obj));
            sis_get_map_sdbs(SIS_OBJ_SDS(obj), context->maps_sdbs);
            sis_object_destroy(obj);
        }
        sis_disk_reader_destroy(reader);
    }    
    if (sis_map_list_getsize(context->maps_sdbs) < 1)
    {
        sis_map_list_destroy(context->maps_sdbs);
        context->maps_sdbs = NULL;
    }
    return 0;
}

int cmd_sisdb_rseg_get(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rseg_cxt *context = (s_sisdb_rseg_cxt *)worker->context;

    s_sis_message *msg = (s_sis_message *)argv_; 
    
    s_sis_msec_pair pair; 
    int subdate = sis_message_get_int(msg, "sub-date");
    pair.start = (msec_t)sis_time_make_time(subdate, 0) * 1000;
    pair.stop = (msec_t)sis_time_make_time(subdate, 235959) * 1000 + 999;
    const char *subkeys = sis_message_get_str(msg, "sub-keys");
    const char *subsdbs = sis_message_get_str(msg, "sub-sdbs");
    const char *subtype = sis_message_get_str(msg, "sub-type");
    
    LOG(5)("get sno open. [%d] %d %s %s %s\n", context->wget_date, subdate, subtype, subkeys, subsdbs);
    if (!subtype)
    {
        if (!context->maps_sdbs || context->wget_date == 0 || context->wget_date != subdate)
        {
            sisdb_rseg_init_sdbs(context, subdate);
            context->wget_date = subdate;
        }
        // 设置表结构
        s_sis_dynamic_db *db = sis_map_list_get(context->maps_sdbs, subsdbs);
        if (!db)
        {
            return SIS_METHOD_NIL;
        }
        int index = sisdb_wseg_get_style(db);
        subtype = sisdb_wseg_get_sname(index);
    }
    // 设置数据对象
    s_sis_sds rpath = sis_sdsdup(sis_sds_save_get(context->work_path));
    rpath = sis_sdscatfmt(rpath, "/%s/", sis_sds_save_get(context->work_name));
    s_sis_disk_reader *reader = sis_disk_reader_create(rpath, subtype, SIS_DISK_TYPE_SNO, NULL);
    s_sis_object *obj = sis_disk_reader_get_obj(reader, subkeys, subsdbs, &pair);
    sis_disk_reader_destroy(reader);
    sis_sdsfree(rpath);

    LOG(5)("get sno stop. ok [%d] %d %p\n", context->wget_date, context->status, obj);
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
int cmd_sisdb_rseg_sub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rseg_cxt *context = (s_sisdb_rseg_cxt *)worker->context;

    SIS_WAIT_OR_EXIT(context->status == SIS_RSEG_NONE);  

    s_sis_message *msg = (s_sis_message *)argv_; 
    if (!msg)
    {
        return SIS_METHOD_ERROR;
    }
    _sisdb_rseg_init(context, msg);
    
    if (context->maps_sdbs && sis_map_list_getsize(context->maps_sdbs) > 0)
    {
        context->status = SIS_RSEG_WORK;
        sis_thread_create(sis_thread_snos_read_sub, context, &context->work_thread);
    }
    else
    {
        cb_stop(context, sis_message_get_int(msg, "sub-date"));
    }

    return SIS_METHOD_OK;
}
int cmd_sisdb_rseg_unsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rseg_cxt *context = (s_sisdb_rseg_cxt *)worker->context;

    sisdb_rseg_sub_stop(context);

    return SIS_METHOD_OK;
}


#if 0
// 测试 snapshot 转 新格式的例子
const char *sisdb_rseg = "\"sisdb_rseg\" : { \
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
    printf("=====%s : \n", __func__);
    return 0;
}
int cb_dict_sdbs1(void *worker_, void *argv_)
{
    // printf("%s : %s\n", __func__, (char *)argv_);
    printf("=====%s : \n", __func__);
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
    _recv_count++;
    return 0;
}

int main()
{
    sis_server_init();
    s_sis_worker *nowwork = NULL;
    {
        s_sis_json_handle *handle = sis_json_load(sisdb_rseg, sis_strlen(sisdb_rseg));
        nowwork = sis_worker_create(NULL, handle->node);
        sis_json_close(handle);
    }

    s_sis_message *msg = sis_message_create(); 
    sis_message_set(msg, "cb_source", nowwork, NULL);
    sis_message_set_int(msg, "sub-date", 20210617);
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