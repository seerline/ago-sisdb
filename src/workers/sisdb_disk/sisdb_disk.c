#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"

#include "sisdb_disk.h"
#include "sisdb.h"
#include "sisdb_server.h"
#include "sisdb_collect.h"
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sisdb_disk_methods[] = {
    {"load", cmd_sisdb_disk_load, NULL, NULL},  // json 格式
    {"save", cmd_sisdb_disk_save, NULL, NULL},  // json 格式
    {"pack", cmd_sisdb_disk_pack, NULL, NULL},  // json 格式
};
// 通用文件存取接口
s_sis_modules sis_modules_sisdb_disk = {
    sisdb_disk_init,
    NULL,
    NULL,
    NULL,
    sisdb_disk_uninit,
    sisdb_disk_method_init,
    sisdb_disk_method_uninit,
    sizeof(sisdb_disk_methods) / sizeof(s_sis_method),
    sisdb_disk_methods,
};

bool sisdb_disk_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_disk_cxt *context = SIS_MALLOC(s_sisdb_disk_cxt, context);
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
        if (!sis_path_exists(context->work_path))
        {
            sis_path_mkdir(context->work_path);
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
        if (!sis_path_exists(context->safe_path))
        {
            sis_path_mkdir(context->safe_path);
        }
    }
    context->page_size = sis_json_get_int(node, "page-size", 500) * 1000000;

    return true;
}
void sisdb_disk_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_disk_cxt *context = (s_sisdb_disk_cxt *)worker->context;
    sis_sdsfree(context->work_path);
    sis_sdsfree(context->safe_path);
    sis_free(context);

}
void sisdb_disk_method_init(void *worker_)
{
}
void sisdb_disk_method_uninit(void *worker_)
{
}

s_sis_sds _sisdb_get_keys(s_sisdb_cxt *cxt)
{
    // int nums = 0;
    // s_sis_sds msg = sis_sdsempty();
    // msg = sis_sdscat(msg, "{");
    // {
    //     s_sis_dict_entry *de;
    //     s_sis_dict_iter *di = sis_dict_get_iter(cxt->collects);
    //     while ((de = sis_dict_next(di)) != NULL)
    //     {
    //         s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
    //         if (collect->style == SISDB_COLLECT_TYPE_TABLE)
    //         {
    //             char keyn[128], sdbn[128]; 
    //             int cmds = sis_str_divide(collect->name, '.', keyn, sdbn);
    //             if (cmds == 2)
    //             {
    //                 if (nums > 0)
    //                 {
    //                     msg = sis_sdscat(msg, ",");
    //                 }
    //                 nums++;
    //                 msg = sis_sdscatfmt(msg, "\"%s\"", keyn);
    //             }                
    //         }
    //     }
    //     sis_dict_iter_free(di);
    // }  
    // msg = sis_sdscat(msg, "}");
    // printf("keys = %s\n", msg);
    // return msg;    
    int nums = 0;
    s_sis_sds msg = sis_sdsempty();
    int count = sis_map_list_getsize(cxt->keys);
    msg = sis_sdscat(msg, "{");
    for(int i = 0; i < count; i++)
    {
        s_sis_sds key = (s_sis_sds)sis_map_list_geti(cxt->keys, i);
        if (nums > 0)
        {
            msg = sis_sdscat(msg, ",");
        }
        nums++;
        msg = sis_sdscatfmt(msg, "\"%S\"", key);
    }
    msg = sis_sdscat(msg, "}");
    printf("keys = %s\n", msg);
    return msg;
}

s_sis_sds _sisdb_get_sdbs(s_sisdb_cxt *cxt, int style)
{
    int nums = 0;
    int count = sis_map_list_getsize(cxt->sdbs);
    s_sis_sds msg = sis_sdsempty();
    {
        msg = sis_sdscat(msg, "{");
        for(int i = 0; i < count; i++)
        {
            s_sisdb_table *sdb = (s_sisdb_table *)sis_map_list_geti(cxt->sdbs, i);
            if (sdb->style != style)
            {
                continue;
            }
            if (nums > 0)
            {
                msg = sis_sdscat(msg, ",");
            }
            nums++;
            msg = sis_sdscatfmt(msg, "\"%S\":", sdb->db->name);
            msg = sis_dynamic_dbinfo_to_json_sds(sdb->db, msg);
        }
        msg = sis_sdscat(msg, "}");
    }
    printf("sdbs = %s\n", msg);
    return msg;
}

int _sisdb_set_keys(s_sisdb_cxt *cxt, const char *in_, size_t ilen_)
{
    char *in = sis_malloc(ilen_ + 1);
    memmove(in, in_, ilen_);
    in[ilen_] = 0;
    s_sis_json_handle *injson = sis_json_load(in, ilen_);
    if (!injson)
    {
        sis_free(in);
        return 0;
    }
    s_sis_json_node *innode = sis_json_first_node(injson->node); 
    while (innode)
    {   
        s_sis_sds key = sis_sdsnew(innode->key);
        sis_map_list_set(cxt->keys, innode->key, key);
        innode = sis_json_next_node(innode);
    }
    sis_free(in);
    sis_json_close(injson);
    return  sis_map_list_getsize(cxt->keys);    
}
// 设置结构体
int _sisdb_set_sdbs(s_sisdb_cxt *cxt, bool issno_, const char *in_, size_t ilen_)
{
    char *in = sis_malloc(ilen_ + 1);
    memmove(in, in_, ilen_);
    in[ilen_] = 0;
    s_sis_json_handle *injson = sis_json_load(in, ilen_);
    if (!injson)
    {
        sis_free(in);
        return 0;
    }
	// int iii=1;
	// sis_json_show(injson->node,&iii);

    s_sis_json_node *innode = sis_json_first_node(injson->node); 
    while (innode)
    {
        s_sisdb_table *table = sisdb_table_create(innode);
        table->style = issno_ ? SISDB_TB_STYLE_SNO : SISDB_TB_STYLE_SDB;
        sis_map_list_set(cxt->sdbs, innode->key, table);
        innode = sis_json_next_node(innode);
    }
    sis_free(in);
    sis_json_close(injson);    
    return  sis_map_list_getsize(cxt->sdbs);    
}

static void cb_key(void *worker_, void *key_, size_t size) 
{
    printf("key %s : %s\n", __func__, (char *)key_);
    s_sisdb_cxt *sisdb = (s_sisdb_cxt *)worker_; 
    _sisdb_set_keys(sisdb, key_, size);
}

static void cb_sdb(void *worker_, void *sdb_, size_t size)  
{
    printf("sdb %s : %s\n", __func__, (char *)sdb_);
    s_sisdb_cxt *sisdb = (s_sisdb_cxt *)worker_; 
    _sisdb_set_sdbs(sisdb, false, sdb_, size);
}

static void cb_sdb_sno(void *worker_, void *sdb_, size_t size)  
{
    printf("sno %s : %s\n", __func__, (char *)sdb_);
    s_sisdb_cxt *sisdb = (s_sisdb_cxt *)worker_; 
    _sisdb_set_sdbs(sisdb, true, sdb_, size);
}
static void cb_read_sdb(void *worker_, const char *key_, const char *sdb_, s_sis_object *obj_)
{
    // printf("load cb_read : %s %s.\n", key_, sdb_);
    s_sisdb_cxt *sisdb = (s_sisdb_cxt *)worker_; 
    if (sdb_)
    {
        // 写sdb
        char key[255];
        sis_sprintf(key, 255, "%s.%s", key_, sdb_);
        s_sisdb_collect *collect = sisdb_get_collect(sisdb, key);
        if (!collect)
        {
            collect = sisdb_collect_create(sisdb, key);
            if (!collect)
            {
                LOG(5)("load %s fail.\n", key);
                return ;
            }    
        }
        int start = SIS_OBJ_LIST(collect->obj)->count;
        sisdb_collect_wpush(collect, SIS_OBJ_GET_CHAR(obj_), SIS_OBJ_GET_SIZE(obj_));
        if (collect->sdb->style == SISDB_TB_STYLE_SNO)
        {  // 序号需要增加
            int stop = SIS_OBJ_LIST(collect->obj)->count - 1;
            s_sisdb_collect_sno sno;
            sno.collect = collect;
            for (int i = start; i <= stop; i++)
            {
                sno.recno = i;
                sis_node_list_push(collect->father->series, &sno);
            }
        }
    }
    else
    {
        // 写any
        uint8 style = (uint8)sis_memory_get_byte(SIS_OBJ_MEMORY(obj_), 1);
        s_sisdb_collect *info = sisdb_kv_create(style, key_, SIS_OBJ_GET_CHAR(obj_), SIS_OBJ_GET_SIZE(obj_));
        sis_map_pointer_set(sisdb->collects, key_, info);
    }

} 
int _sisdb_disk_load(const char *pathname, bool issno, s_sisdb_cxt *sisdb, s_sisdb_catch *config)
{
    s_sis_disk_class *sdbfile = sis_disk_class_create();  
    if (issno)
    {
        char snoname[32];
        if (sisdb->work_date == 0)
        {
            sis_llutoa(sis_time_get_idate(0), snoname, 32, 10);
        }
        else
        {
            sis_llutoa(sisdb->work_date, snoname, 32, 10);
        }
        sis_disk_class_init(sdbfile, SIS_DISK_TYPE_SNO, pathname, snoname);
    }
    else
    {
        sis_disk_class_init(sdbfile, SIS_DISK_TYPE_SDB, pathname, sisdb->name);
    }
    int ro = sis_disk_file_read_start(sdbfile);

    // printf("%s , issno = %d ro = %d\n", __func__, issno, ro, sdbfile->work_fps->cur_name);
    if (ro != SIS_DISK_CMD_OK)
    {
        sis_disk_class_destroy(sdbfile);
        // 文件不存在也认为正确
        return SIS_METHOD_OK;
    }
    s_sis_disk_callback *callback = SIS_MALLOC(s_sis_disk_callback, callback);
    callback->source = sisdb;
    callback->cb_begin = NULL;
    callback->cb_key = cb_key;
    if (issno)
    {
        callback->cb_sdb = cb_sdb_sno;
    }
    else
    {
        callback->cb_sdb = cb_sdb;
    } 
    callback->cb_read = cb_read_sdb;
    callback->cb_end = NULL;

    printf("%s , issno = %d cb_sdb = %p\n", __func__, issno, callback->cb_sdb);

    s_sis_disk_reader *reader = sis_disk_reader_create(callback);
    sis_disk_reader_set_key(reader, "*");
    sis_disk_reader_set_sdb(reader, "*");
    if (issno)
    {
        if (config->last_sno > 0)
        {
            // 加载历史的数据
            // 不支持 只支持当日数据加载
        }
    }
    else
    {
        if (config->last_day > 0)
        {
            // 设置加载时间
        }
    }
    reader->isone = 1; // 设置为一次性输出
    sis_disk_file_read_sub(sdbfile, reader);

    sis_disk_reader_destroy(reader);    
    sis_free(callback);
    sis_disk_file_read_stop(sdbfile);
    sis_disk_class_destroy(sdbfile);
    return SIS_METHOD_OK;
}

int cmd_sisdb_disk_load(void *worker_, void *argv_)
{
    s_sis_message *msg = (s_sis_message *)argv_;
    LOG(5)("load disk ...\n");
    s_sisdb_cxt *sisdb = (s_sisdb_cxt *)sis_message_get(msg, "sisdb");
    if (!sisdb)
    {
        return SIS_METHOD_ERROR;
    }
    s_sisdb_catch config;
    config.last_day = 0;
    config.last_min = 0;
    config.last_sec = 0;
    config.last_sno = 0;
    if (sis_message_get(msg, "config"))
    {
        memmove(&config, sis_message_get(msg, "config"), sizeof(s_sisdb_catch));
    }

    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_disk_cxt *context = (s_sisdb_disk_cxt *)worker->context;
    char pathname[255];
    sis_sprintf(pathname, 255, "%s/%s/", context->work_path, sisdb->name);

    ///////////////////////////////////////
    // 先读 sno 数据
    ///////////////////////////////////////
    if (_sisdb_disk_load(pathname, true, sisdb, &config) != SIS_METHOD_OK)
    {
        return SIS_METHOD_ERROR;
    }
    ///////////////////////////////////////
    // 再读sdb数据  
    ///////////////////////////////////////
    if (_sisdb_disk_load(pathname, false, sisdb, &config) != SIS_METHOD_OK)
    {
        return SIS_METHOD_ERROR;
    }

    return SIS_METHOD_OK;    
}
int cmd_sisdb_disk_save(void *worker_, void *argv_)
{
    s_sis_message *msg = (s_sis_message *)argv_;
    s_sisdb_cxt *sisdb = (s_sisdb_cxt *)sis_message_get(msg, "sisdb");
    if (!sisdb)
    {
        return SIS_METHOD_ERROR;
    }
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_disk_cxt *context = (s_sisdb_disk_cxt *)worker->context;

    int work_date = sis_message_get_int(msg, "workdate");
    // 先写 sno 数据
    char pathname[255];
    sis_sprintf(pathname, 255, "%s/%s/", context->work_path, sisdb->name);
    int count = sis_node_list_get_size(sisdb->series);
    if (count > 0)
    {
        s_sis_disk_class *snofile = sis_disk_class_create();
        char snoname[32];
        sis_llutoa(work_date, snoname, 32, 10);
        sis_disk_class_init(snofile, SIS_DISK_TYPE_SNO, pathname, snoname);
        // 必须全部重写 对内存有要求
        sis_disk_file_delete(snofile);
        sis_disk_file_write_start(snofile);

        {
            s_sis_sds keys = _sisdb_get_keys(sisdb);
            sis_disk_class_set_key(snofile, true, keys, sis_sdslen(keys));
            s_sis_sds sdbs = _sisdb_get_sdbs(sisdb, SISDB_TB_STYLE_SNO);
            sis_disk_class_set_sdb(snofile, true, sdbs, sis_sdslen(sdbs));
            sis_sdsfree(keys);
            sis_sdsfree(sdbs);
        }

        char keyn[128], sdbn[128]; 
        for (int i = 0; i < count; i++)
        {
            s_sisdb_collect_sno *sno = (s_sisdb_collect_sno *)sis_node_list_get(sisdb->series, i);  
            sis_str_divide(sno->collect->name, '.', keyn, sdbn);      
            sis_disk_file_write_sdb(snofile, keyn, sno->collect->sdb->db->name, 
                sis_struct_list_get(SIS_OBJ_LIST(sno->collect->obj), sno->recno) , sno->collect->sdb->db->size);
        }
        
        sis_disk_file_write_stop(snofile);
        sis_disk_class_destroy(snofile);
    }

    // 再写sdb数据
    if (sis_map_pointer_getsize(sisdb->collects) > 0)
    {
        s_sis_disk_class *sdbfile = sis_disk_class_create();
        sis_disk_class_init(sdbfile, SIS_DISK_TYPE_SDB, pathname, sisdb->name);
        // 不能删除老文件的信息
        sis_disk_file_write_start(sdbfile);
        {
            s_sis_sds keys = _sisdb_get_keys(sisdb);
            sis_disk_class_set_key(sdbfile, true, keys, sis_sdslen(keys));
            s_sis_sds sdbs = _sisdb_get_sdbs(sisdb, SISDB_TB_STYLE_SDB);
            sis_disk_class_set_sdb(sdbfile, true, sdbs, sis_sdslen(sdbs));
            sis_sdsfree(keys);
            sis_sdsfree(sdbs);
        }

        // 写入自由和结构键值
        {
            char keyn[128], sdbn[128]; 
            s_sis_memory *memory = sis_memory_create();
            s_sis_dict_entry *de;
            s_sis_dict_iter *di = sis_dict_get_iter(sisdb->collects);
            while ((de = sis_dict_next(di)) != NULL)
            {
                s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
                sis_str_divide(collect->name, '.', keyn, sdbn); 
                if (collect->style == SISDB_COLLECT_TYPE_TABLE)
                {
                    sis_disk_file_write_sdb(sdbfile, keyn, collect->sdb->db->name, 
                        SIS_OBJ_GET_CHAR(collect->obj), SIS_OBJ_GET_SIZE(collect->obj));
                }
                else 
                if (collect->style == SISDB_COLLECT_TYPE_CHARS || collect->style == SISDB_COLLECT_TYPE_BYTES)
                {
                    sis_memory_clear(memory);
                    sis_memory_cat_byte(memory, collect->style, 1);
                    sis_memory_cat(memory, SIS_OBJ_GET_CHAR(collect->obj), SIS_OBJ_GET_SIZE(collect->obj));
                    sis_disk_file_write_any(sdbfile, sis_dict_getkey(de), sis_memory(memory), sis_memory_get_size(memory));
                }
            }
            sis_dict_iter_free(di);
            sis_memory_destroy(memory);
        }
        sis_disk_file_write_stop(sdbfile);
        sis_disk_class_destroy(sdbfile);
    }

    return SIS_METHOD_OK;
}

int cmd_sisdb_disk_pack(void *worker_, void *argv_)
{
    s_sis_message *msg = (s_sis_message *)argv_;
    s_sisdb_cxt *sisdb = (s_sisdb_cxt *)sis_message_get(msg, "sisdb");
    if (!sisdb)
    {
        return SIS_METHOD_ERROR;
    }
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_disk_cxt *context = (s_sisdb_disk_cxt *)worker->context;

    char pathname[255];
    sis_sprintf(pathname, 255, "%s/%s/", context->work_path, sisdb->name);
    // 只处理 sdb 的数据 sno 数据本来就是没有冗余的
    s_sis_disk_class *srcfile = sis_disk_class_create();
    sis_disk_class_init(srcfile, SIS_DISK_TYPE_SDB, pathname, sisdb->name);
    sis_disk_file_move(srcfile, context->safe_path);
    sis_disk_class_init(srcfile, SIS_DISK_TYPE_SDB, context->safe_path, sisdb->name);

    s_sis_disk_class *desfile = sis_disk_class_create();
    sis_disk_class_init(desfile, SIS_DISK_TYPE_SDB, pathname, sisdb->name);

    size_t size = sis_disk_file_pack(srcfile, desfile);

    if (size == 0)
    {
        sis_disk_file_delete(desfile);
        sis_disk_file_move(srcfile, pathname);
    }   
    sis_disk_class_destroy(desfile);
    sis_disk_class_destroy(srcfile);

    if (size == 0)
    {
        return SIS_METHOD_ERROR;
    }
    return SIS_METHOD_OK;
}


