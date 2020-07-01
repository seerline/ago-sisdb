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
    int nums = 0;
    int count = sis_map_list_getsize(cxt->keys);
    s_sis_sds msg = sis_sdsempty();
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
    s_sis_json_handle *injson = sis_json_load(in_, ilen_);
    if (!injson)
    {
        sis_json_close(injson);
        return 0;
    }
    s_sis_json_node *innode = sis_json_first_node(injson->node); 
    while (innode)
    {   
        s_sis_sds key = sis_sdsnew(innode->key);
        sis_map_list_set(cxt->keys, innode->key, key);
        innode = sis_json_next_node(innode);
    }
    sis_json_close(injson);
    return  sis_map_list_getsize(cxt->keys);    
}
// 设置结构体
int _sisdb_set_sdbs(s_sisdb_cxt *cxt, const char *in_, size_t ilen_)
{
    s_sis_json_handle *injson = sis_json_load(in_, ilen_);
    if (!injson)
    {
        sis_json_close(injson);
        return 0;
    }
   s_sis_json_node *innode = sis_json_first_node(injson->node); 
    while (innode)
    {
        s_sis_dynamic_db *sdb = sis_dynamic_db_create(innode);        
        if (sdb)
        {
            sis_map_list_set(cxt->sdbs, innode->key, sdb);
        }  
        innode = sis_json_next_node(innode);
    }
    sis_json_close(injson);    
    return  sis_map_list_getsize(cxt->sdbs);    
}

static void cb_key(void *worker_, void *key_, size_t size) 
{
    // printf("%s : %s\n", __func__, (char *)key_);
    s_sisdb_cxt *sisdb = (s_sisdb_cxt *)worker_; 
    _sisdb_set_keys(sisdb, key_, size);
}

static void cb_sdb(void *worker_, void *sdb_, size_t size)  
{
    // printf("%s : %s\n", __func__, (char *)sdb_);
    s_sisdb_cxt *sisdb = (s_sisdb_cxt *)worker_; 
    _sisdb_set_sdbs(sisdb, sdb_, size);
}

static void cb_read(void *worker_, const char *key_, const char *sdb_, s_sis_object *obj_)
{
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
        sisdb_collect_wpush(collect, SIS_OBJ_GET_CHAR(obj_), SIS_OBJ_GET_SIZE(obj_));
    }
    else
    {
        // 写any
        uint8 style = (uint8)sis_memory_get_byte(SIS_OBJ_MEMORY(obj_), 1);
        s_sisdb_collect *info = sisdb_kv_create(style, SIS_OBJ_GET_CHAR(obj_), SIS_OBJ_GET_SIZE(obj_));
        sis_map_pointer_set(sisdb->collects, key_, info);
    }

} 

int cmd_sisdb_disk_load(void *worker_, void *argv_)
{
    s_sis_message *msg = (s_sis_message *)argv_;

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

    // 先读 sno 数据
    if (config.last_sno > 0)
    {

    }
    // 再读sdb数据  
    s_sis_disk_class *sdbfile = sis_disk_class_create();  
    sis_disk_class_init(sdbfile, SIS_DISK_TYPE_SDB, context->work_path, sisdb->name);
    int ro = sis_disk_file_read_start(sdbfile);
    if (ro < SIS_DISK_CMD_NO_IGNORE )
    {
        return SIS_METHOD_ERROR;
    }
    s_sis_disk_callback *callback = SIS_MALLOC(s_sis_disk_callback, callback);
    callback->source = sisdb;
    callback->cb_begin = NULL;
    callback->cb_key = cb_key;
    callback->cb_sdb = cb_sdb;
    callback->cb_read = cb_read;
    callback->cb_end = NULL;

    s_sis_disk_reader *reader = sis_disk_reader_create(callback);
    sis_disk_reader_set_key(reader, "*");
    sis_disk_reader_set_sdb(reader, "*");
    if (config.last_day > 0)
    {

    }

    // sub 是一条一条的输出
    sis_disk_file_read_sub(sdbfile, reader);

    sis_disk_reader_destroy(reader);    
    sis_free(callback);
    sis_disk_file_read_stop(sdbfile);
    sis_disk_class_destroy(sdbfile);

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
    s_sis_disk_class *snofile = sis_disk_class_create();
    char snoname[32];
    sis_llutoa(work_date, snoname, 32, 10);
    sis_disk_class_init(snofile, SIS_DISK_TYPE_SNO, context->work_path, snoname);
    {
        s_sis_sds keys = _sisdb_get_keys(sisdb);
        sis_disk_class_set_key(snofile, keys, sis_sdslen(keys));
        s_sis_sds sdbs = _sisdb_get_sdbs(sisdb, SISDB_TB_STYLE_SNO);
        sis_disk_class_set_sdb(snofile, sdbs, sis_sdslen(sdbs));
        sis_sdsfree(keys);
        sis_sdsfree(sdbs);
    }
    sis_disk_file_write_start(snofile, SIS_DISK_ACCESS_CREATE);
    
    int count = sis_node_list_get_size(sisdb->series);
    for (int i = 0; i < count; i++)
    {
        s_sisdb_collect_sno *sno = (s_sisdb_collect_sno *)sis_node_list_get(sisdb->series, i);        
        sis_disk_file_write_sdb(snofile, sno->collect->key, sno->collect->sdb->db->name, 
            sis_struct_list_get(SIS_OBJ_LIST(sno->collect->obj), sno->recno) , sno->collect->sdb->db->size);
    }
    
    sis_disk_file_write_stop(snofile);
    sis_disk_class_destroy(snofile);

    // 再写sdb数据
    s_sis_disk_class *sdbfile = sis_disk_class_create();
    sis_disk_class_init(sdbfile, SIS_DISK_TYPE_SDB, context->work_path, sisdb->name);
    {
        s_sis_sds keys = _sisdb_get_keys(sisdb);
        sis_disk_class_set_key(snofile, keys, sis_sdslen(keys));
        s_sis_sds sdbs = _sisdb_get_sdbs(sisdb, SISDB_TB_STYLE_SDB);
        sis_disk_class_set_sdb(snofile, sdbs, sis_sdslen(sdbs));
        sis_sdsfree(keys);
        sis_sdsfree(sdbs);
    }
    sis_disk_file_write_start(sdbfile, SIS_DISK_ACCESS_APPEND);
    // 写入自由和结构键值
    {
        s_sis_memory *memory = sis_memory_create();
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb->collects);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
            if (collect->style == SISDB_COLLECT_TYPE_TABLE)
            {
                sis_disk_file_write_sdb(sdbfile, collect->key, collect->sdb->db->name, 
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
    // 只处理 sdb 的数据 sno 数据本来就是没有冗余的
    s_sis_disk_class *srcfile = sis_disk_class_create();
    sis_disk_class_init(srcfile, SIS_DISK_TYPE_SDB, context->work_path, sisdb->name);
    sis_disk_file_move(srcfile, context->safe_path);
    sis_disk_class_init(srcfile, SIS_DISK_TYPE_SDB, context->safe_path, sisdb->name);

    s_sis_disk_class *desfile = sis_disk_class_create();
    sis_disk_class_init(desfile, SIS_DISK_TYPE_SDB, context->work_path, sisdb->name);

    size_t size = sis_disk_file_pack(srcfile, desfile);

    if (size == 0)
    {
        sis_disk_file_delete(desfile);
        sis_disk_file_move(srcfile, context->work_path);
    }   
    sis_disk_class_destroy(desfile);
    sis_disk_class_destroy(srcfile);

    if (size == 0)
    {
        return SIS_METHOD_ERROR;
    }
    return SIS_METHOD_OK;
}


