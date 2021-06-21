#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"

#include "sisdb_rsdb.h"
#include "sisdb.h"
#include "sisdb_server.h"
#include "sisdb_collect.h"
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sisdb_rsdb_methods[] = {
    {"load", cmd_sisdb_rsdb_load, 0, NULL},  // json 格式
};
// 通用文件存取接口
s_sis_modules sis_modules_sisdb_rsdb = {
    sisdb_rsdb_init,
    NULL,
    NULL,
    NULL,
    sisdb_rsdb_uninit,
    sisdb_rsdb_method_init,
    sisdb_rsdb_method_uninit,
    sizeof(sisdb_rsdb_methods) / sizeof(s_sis_method),
    sisdb_rsdb_methods,
};

bool sisdb_rsdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_rsdb_cxt *context = SIS_MALLOC(s_sisdb_rsdb_cxt, context);
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
    return true;
}
void sisdb_rsdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)worker->context;
    sis_sdsfree(context->work_path);
    sis_free(context);

}
void sisdb_rsdb_method_init(void *worker_)
{
}
void sisdb_rsdb_method_uninit(void *worker_)
{
}

int _sisdb_set_keys(s_sisdb_cxt *cxt, const char *in_, size_t ilen_)
{
    // char *in = sis_malloc(ilen_ + 1);
    // memmove(in, in_, ilen_);
    // in[ilen_] = 0;
    // s_sis_json_handle *injson = sis_json_load(in, ilen_);
    // if (!injson)
    // {
    //     sis_free(in);
    //     return 0;
    // }
    // s_sis_json_node *innode = sis_json_first_node(injson->node); 
    // while (innode)
    // {   
    //     s_sis_sds key = sis_sdsnew(innode->key);
    //     sis_map_list_set(cxt->keys, innode->key, key);
    //     innode = sis_json_next_node(innode);
    // }
    // sis_free(in);
    // sis_json_close(injson);
    // return  sis_map_list_getsize(cxt->keys);   
    return 0; 
}
// 设置结构体
int _sisdb_set_sdbs(s_sisdb_cxt *cxt, const char *in_, size_t ilen_)
{
    s_sis_json_handle *injson = sis_json_load(in_, ilen_);
    if (!injson)
    {
        return 0;
    }
    s_sis_json_node *innode = sis_json_first_node(injson->node); 
    while (innode)
    {
        s_sisdb_table *table = sisdb_table_create(innode);
        sis_map_list_set(cxt->work_sdbs, innode->key, table);
        innode = sis_json_next_node(innode);
    }
    sis_json_close(injson);    
    return  sis_map_list_getsize(cxt->work_sdbs);    
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
    _sisdb_set_sdbs(sisdb, sdb_, size);
}

static void cb_read(void *worker_, const char *key_, const char *sdb_, void *out_, size_t olen_)
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
        sisdb_collect_wpush(collect, out_, olen_);
    }
    else
    {
        if (olen_ > 1)
        {
            // 写any
            uint8 *out  = (uint8 *)out_;
            uint8 style = out[0];
            s_sisdb_collect *info = sisdb_kv_create(style, key_, (char *)&out[1], olen_ - 1);
            sis_map_pointer_set(sisdb->work_keys, key_, info);
        }
    }

} 
int _sisdb_rsdb_load_sdb(const char *workpath, s_sisdb_cxt *sisdb, s_sisdb_catch *config)
{
    s_sis_disk_v1_class *sdbfile = sis_disk_v1_class_create();  
    sis_disk_v1_class_init(sdbfile, SIS_DISK_TYPE_SDB, workpath, sisdb->dbname, 0);
    int ro = sis_disk_v1_file_read_start(sdbfile);

    printf("%s , ro = %d  %s\n", __func__, ro, workpath);
    if (ro != SIS_DISK_CMD_OK)
    {
        sis_disk_v1_class_destroy(sdbfile);
        // 文件不存在也认为正确
        return SIS_METHOD_OK;
    }
    s_sis_disk_v1_callback *callback = SIS_MALLOC(s_sis_disk_v1_callback, callback);
    callback->source = sisdb;
    callback->cb_begin = NULL;
    callback->cb_key = cb_key;
    callback->cb_sdb = cb_sdb;
    callback->cb_read = cb_read;
    callback->cb_end = NULL;

    printf("%s , cb_sdb = %p\n", __func__, callback->cb_sdb);

    s_sis_disk_v1_reader *reader = sis_disk_v1_reader_create(callback);
    sis_disk_v1_reader_set_key(reader, "*");
    sis_disk_v1_reader_set_sdb(reader, "*");
    if (config->last_day > 0)
    {
        // 设置加载时间
    }
    reader->isone = 1; // 设置为一次性输出
    sis_disk_v1_file_read_sub(sdbfile, reader);

    sis_disk_v1_reader_destroy(reader);    
    sis_free(callback);
    sis_disk_v1_file_read_stop(sdbfile);
    sis_disk_v1_class_destroy(sdbfile);
    return SIS_METHOD_OK;
}


int cmd_sisdb_rsdb_load(void *worker_, void *argv_)
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
    s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)worker->context;
    ///////////////////////////////////////
    // 再读sdb数据  
    ///////////////////////////////////////
    if (_sisdb_rsdb_load_sdb(context->work_path, sisdb, &config) != SIS_METHOD_OK)
    {
        return SIS_METHOD_ERROR;
    }

    return SIS_METHOD_OK;    
}
