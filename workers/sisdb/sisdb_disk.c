//******************************************************
// Copyright (C) 2018, coollyer <seerlinecoin@gmail.com>
//*******************************************************

#include "sisdb_io.h"
#include "sis_net.msg.h"
#include "sis_disk.h"

// 先把数据根据索引处理到safe中 处理成功后 移动原始文件 到时间为目录的安全目录下 然后把处理的好的文件移动到工作目录中
// safe目录 只保留最近5个目录
int sisdb_disk_pack(s_sisdb_cxt *context)
{
    if (context->wfile_status)
    {
        return -1;
    }
    context->wfile_status = 1;
    sis_mutex_lock(&context->wlog_lock);

    // 只处理 sdb 的数据 sno 数据本来就是没有冗余的
    // s_sis_disk_v1_class *srcfile = sis_disk_v1_class_create();
    // sis_disk_v1_class_init(srcfile, SIS_DISK_TYPE_SDB, context->work_path, dbname, 0);
    // sis_disk_v1_file_move(srcfile, context->safe_path);
    // sis_disk_v1_class_init(srcfile, SIS_DISK_TYPE_SDB, context->safe_path, dbname, 0);

    // s_sis_disk_v1_class *desfile = sis_disk_v1_class_create();
    // sis_disk_v1_class_init(desfile, SIS_DISK_TYPE_SDB, context->work_path, dbname, 0);

    // size_t size = sis_disk_v1_file_pack(srcfile, desfile);

    // if (size == 0)
    // {
    //     sis_disk_v1_file_delete(desfile);
    //     sis_disk_v1_file_move(srcfile, context->work_path);
    // }   
    // sis_disk_v1_class_destroy(desfile);
    // sis_disk_v1_class_destroy(srcfile);

    // if (size == 0)
    // {
    //     return SIS_METHOD_ERROR;
    // }

    sis_mutex_unlock(&context->wlog_lock);
    context->wfile_status = 0;
    return 0;
}

// int cmd_sisdb_wsdb_pack(void *worker_, void *argv_)
// {
//     s_sis_message *msg = (s_sis_message *)argv_;
//     s_sis_sds dbname = sis_message_get_str(msg, "dbname");
//     if (!dbname)
//     {
//         return SIS_METHOD_ERROR;
//     }
//     s_sis_worker *worker = (s_sis_worker *)worker_; 
//     s_sisdb_wsdb_cxt *context = (s_sisdb_wsdb_cxt *)worker->context;

//     // 只处理 sdb 的数据 sno 数据本来就是没有冗余的
//     s_sis_disk_v1_class *srcfile = sis_disk_v1_class_create();
//     sis_disk_v1_class_init(srcfile, SIS_DISK_TYPE_SDB, context->work_path, dbname, 0);
//     sis_disk_v1_file_move(srcfile, context->safe_path);
//     sis_disk_v1_class_init(srcfile, SIS_DISK_TYPE_SDB, context->safe_path, dbname, 0);

//     s_sis_disk_v1_class *desfile = sis_disk_v1_class_create();
//     sis_disk_v1_class_init(desfile, SIS_DISK_TYPE_SDB, context->work_path, dbname, 0);

//     size_t size = sis_disk_v1_file_pack(srcfile, desfile);

//     if (size == 0)
//     {
//         sis_disk_v1_file_delete(desfile);
//         sis_disk_v1_file_move(srcfile, context->work_path);
//     }   
//     sis_disk_v1_class_destroy(desfile);
//     sis_disk_v1_class_destroy(srcfile);

//     if (size == 0)
//     {
//         return SIS_METHOD_ERROR;
//     }
//     return SIS_METHOD_OK;
// }


s_sis_sds _sisdb_get_keys(s_sisdb_cxt *cxt)
{
    int nums = 0;
    s_sis_sds msg = sis_sdsempty();
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(cxt->work_keys);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
            if (collect->style == SISDB_COLLECT_TYPE_TABLE)
            {
                char keyn[128], sdbn[128]; 
                int cmds = sis_str_divide(collect->name, '.', keyn, sdbn);
                if (cmds == 2)
                {
                    if (nums > 0)
                    {
                        msg = sis_sdscat(msg, ",");
                    }
                    nums++;
                    msg = sis_sdscatfmt(msg, "%s", keyn);
                }                
            }
        }
        sis_dict_iter_free(di);
    }  
    printf("keys = %s\n", msg);
    return msg;    
}

s_sis_sds _sisdb_get_sdbs(s_sisdb_cxt *cxt)
{
    int count = sis_map_list_getsize(cxt->work_sdbs);
    s_sis_json_node *sdbs_node = sis_json_create_object();
    {
        for(int i = 0; i < count; i++)
        {
            s_sisdb_table *table = (s_sisdb_table *)sis_map_list_geti(cxt->work_sdbs, i);
            sis_json_object_add_node(sdbs_node, table->name, sis_dynamic_dbinfo_to_json(table));
        }
    }
    s_sis_sds msg = sis_json_to_sds(sdbs_node, true);
    printf("sdbs = %s\n", msg);
    sis_json_delete_node(sdbs_node);
    return msg;
}
int sisdb_disk_save(s_sisdb_cxt *context)
{
    if (context->wfile_status)
    {
        return -1;
    }
    context->wfile_status = 1;
    sis_mutex_lock(&context->wlog_lock);

    if (sis_map_pointer_getsize(context->work_keys) > 0)
    {
        s_sis_disk_writer *sdbfile = sis_disk_writer_create(context->work_path, context->work_name, SIS_DISK_TYPE_SDB);
        // 不能删除老文件的信息
        sis_disk_writer_open(sdbfile, 0);
        {
            s_sis_sds keys = _sisdb_get_keys(context);
            sis_disk_writer_set_kdict(sdbfile, keys, sis_sdslen(keys));
            s_sis_sds sdbs = _sisdb_get_sdbs(context);
            sis_disk_writer_set_sdict(sdbfile, sdbs, sis_sdslen(sdbs));
            sis_sdsfree(keys);
            sis_sdsfree(sdbs);
        }

        // 写入自由和结构键值
        {
            char keyn[128], sdbn[128]; 
            s_sis_memory *memory = sis_memory_create();
            s_sis_dict_entry *de;
            s_sis_dict_iter *di = sis_dict_get_iter(context->work_keys);
            while ((de = sis_dict_next(di)) != NULL)
            {
                s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
                if (collect->wflag)  // 数据更新的键才写入
                {
                    sis_str_divide(collect->name, '.', keyn, sdbn); 
                    if (collect->style == SISDB_COLLECT_TYPE_TABLE)
                    {
                        sis_disk_writer_sdb(sdbfile, keyn, collect->sdb->name, 
                            SIS_OBJ_GET_CHAR(collect->obj), SIS_OBJ_GET_SIZE(collect->obj));
                    }
                    else 
                    if (collect->style == SISDB_COLLECT_TYPE_CHARS || collect->style == SISDB_COLLECT_TYPE_BYTES)
                    {
                        sis_memory_clear(memory);
                        sis_memory_cat_byte(memory, collect->style, 1);
                        sis_memory_cat(memory, SIS_OBJ_GET_CHAR(collect->obj), SIS_OBJ_GET_SIZE(collect->obj));
                        sis_disk_writer_one(sdbfile, sis_dict_getkey(de), sis_memory(memory), sis_memory_get_size(memory));
                    }
                }
            }
            sis_dict_iter_free(di);
            sis_memory_destroy(memory);
        }
        sis_disk_writer_close(sdbfile);
        sis_disk_writer_destroy(sdbfile);
    }
    sis_mutex_unlock(&context->wlog_lock);
    context->wfile_status = 0;
    return 0;
}

// read
// int _sisdb_set_keys(s_sisdb_cxt *cxt, const char *in_, size_t ilen_)
// {

//     return 0; 
// }
// // 设置结构体
// int _sisdb_set_sdbs(s_sisdb_cxt *cxt, const char *in_, size_t ilen_)
// {
//     s_sis_json_handle *injson = sis_json_load(in_, ilen_);
//     if (!injson)
//     {
//         return 0;
//     }
//     s_sis_json_node *innode = sis_json_first_node(injson->node); 
//     while (innode)
//     {
//         s_sisdb_table *table = sisdb_table_create(innode);
//         sis_map_list_set(cxt->work_sdbs, innode->key, table);
//         innode = sis_json_next_node(innode);
//     }
//     sis_json_close(injson);    
//     return  sis_map_list_getsize(cxt->work_sdbs);    
// }

// static void cb_key(void *worker_, void *key_, size_t size) 
// {
//     printf("key %s : %s\n", __func__, (char *)key_);
//     s_sisdb_cxt *sisdb = (s_sisdb_cxt *)worker_; 
//     _sisdb_set_keys(sisdb, key_, size);
// }

// static void cb_sdb(void *worker_, void *sdb_, size_t size)  
// {
//     printf("sdb %s : %s\n", __func__, (char *)sdb_);
//     s_sisdb_cxt *sisdb = (s_sisdb_cxt *)worker_; 
//     _sisdb_set_sdbs(sisdb, sdb_, size);
// }

// static void cb_read(void *worker_, const char *key_, const char *sdb_, void *out_, size_t olen_)
// {
//     // printf("load cb_read : %s %s.\n", key_, sdb_);
//     s_sisdb_cxt *sisdb = (s_sisdb_cxt *)worker_; 
//     if (sdb_)
//     {
//         // 写sdb
//         char key[255];
//         sis_sprintf(key, 255, "%s.%s", key_, sdb_);
//         s_sisdb_collect *collect = sisdb_get_collect(sisdb, key);
//         if (!collect)
//         {
//             collect = sisdb_collect_create(sisdb, key);
//             if (!collect)
//             {
//                 LOG(5)("load %s fail.\n", key);
//                 return ;
//             }    
//         }
//         sisdb_collect_wpush(collect, out_, olen_);
//     }
//     else
//     {
//         if (olen_ > 1)
//         {
//             // 写any
//             uint8 *out  = (uint8 *)out_;
//             uint8 style = out[0];
//             s_sisdb_collect *info = sisdb_kv_create(style, key_, (char *)&out[1], olen_ - 1);
//             sis_map_pointer_set(sisdb->work_keys, key_, info);
//         }
//     }

// } 
// int _sisdb_rsdb_load_sdb(const char *workpath, s_sisdb_cxt *sisdb, s_sisdb_catch *config)
// {
//     s_sis_disk_v1_class *sdbfile = sis_disk_v1_class_create();  
//     sis_disk_v1_class_init(sdbfile, SIS_DISK_TYPE_SDB, workpath, sisdb->dbname, 0);
//     int ro = sis_disk_v1_file_read_start(sdbfile);

//     printf("%s , ro = %d  %s\n", __func__, ro, workpath);
//     if (ro != SIS_DISK_CMD_OK)
//     {
//         sis_disk_v1_class_destroy(sdbfile);
//         // 文件不存在也认为正确
//         return SIS_METHOD_OK;
//     }
//     s_sis_disk_reader_cb *callback = SIS_MALLOC(s_sis_disk_reader_cb, callback);
//     callback->cb_source = sisdb;
//     callback->cb_dict_keys = cb_key;
//     callback->cb_dict_sdbs = cb_sdb;
//     callback->cb_chardata = cb_read;

//     printf("%s , cb_sdb = %p\n", __func__, callback->cb_sdb);

//     s_sis_disk_v1_reader *reader = sis_disk_v1_reader_create(callback);
//     sis_disk_v1_reader_set_key(reader, "*");
//     sis_disk_v1_reader_set_sdb(reader, "*");
//     if (config->last_day > 0)
//     {
//         // 设置加载时间
//     }
//     reader->isone = 1; // 设置为一次性输出
//     sis_disk_v1_file_read_sub(sdbfile, reader);

//     sis_disk_v1_reader_destroy(reader);    
//     sis_free(callback);
//     sis_disk_v1_file_read_stop(sdbfile);
//     sis_disk_v1_class_destroy(sdbfile);
//     return SIS_METHOD_OK;
// }


// int cmd_sisdb_rsdb_load(void *worker_, void *argv_)
// {
//     s_sis_message *msg = (s_sis_message *)argv_;
//     LOG(5)("load disk ...\n");
//     s_sisdb_cxt *sisdb = (s_sisdb_cxt *)sis_message_get(msg, "sisdb");
//     if (!sisdb)
//     {
//         return SIS_METHOD_ERROR;
//     }
//     s_sisdb_catch config;
//     config.last_day = 0;
//     config.last_min = 0;
//     config.last_sec = 0;
//     config.last_sno = 0;
//     if (sis_message_get(msg, "config"))
//     {
//         memmove(&config, sis_message_get(msg, "config"), sizeof(s_sisdb_catch));
//     }

//     s_sis_worker *worker = (s_sis_worker *)worker_; 
//     s_sisdb_rsdb_cxt *context = (s_sisdb_rsdb_cxt *)worker->context;
//     ///////////////////////////////////////
//     // 再读sdb数据  
//     ///////////////////////////////////////
//     if (_sisdb_rsdb_load_sdb(context->work_path, sisdb, &config) != SIS_METHOD_OK)
//     {
//         return SIS_METHOD_ERROR;
//     }

//     return SIS_METHOD_OK;    
// }


// write
// 加载数据结构
s_sis_object *sisdb_read_disk(s_sisdb_cxt *context, s_sis_net_message *netmsg)
{

    return NULL;
}
int sisdb_read_sdbs(s_sisdb_cxt *context)
{

    return 0;
}

static int cb_rlog_netmsg(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    sis_worker_command(worker, netmsg->cmd, netmsg);
    return 0;
}
// 从磁盘中加载log
int sisdb_rlog_read(s_sis_worker *worker)
{
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_message *msg = sis_message_create();
    // 这里需要加载 log 中的数据 这可能是一个漫长的过程

    sis_mutex_lock(&context->wlog_lock);
    // 读完就销毁
    s_sis_worker *rlog = sis_worker_create_of_conf(NULL, context->work_name, "{classname:sisdb_flog}");

    sis_message_set_str(msg, "work-path", context->work_path, sis_sdslen(context->work_path));
    sis_message_set_str(msg, "work-name", context->work_name, sis_sdslen(context->work_name));
    sis_message_set_int(msg, "work-date", context->work_date);
    sis_message_set(msg, "source", worker, NULL);
    // sis_message_set_method(msg, "cb_sub_start", NULL);
    // sis_message_set_method(msg, "cb_sub_stop", NULL);
    sis_message_set_method(msg, "cb_netmsg", cb_rlog_netmsg);
    int o = sis_worker_command(rlog, "sub", msg);

    sis_worker_destroy(rlog);
    sis_mutex_unlock(&context->wlog_lock);
    sis_message_destroy(msg);
    return o;
}
void sisdb_wlog_open(s_sisdb_cxt *context)
{
    s_sis_message *msg = sis_message_create();
    sis_message_set_str(msg, "work-path", context->work_path, sis_sdslen(context->work_path));
    sis_message_set_str(msg, "work-name", context->work_name, sis_sdslen(context->work_name));
    sis_message_set_int(msg, "work-date", context->work_date);
    sis_worker_command(context->wlog_worker, "open", msg);
    sis_message_destroy(msg);
    context->wlog_open = 1;
}
void sisdb_wlog_close(s_sisdb_cxt *context)
{
    s_sis_message *msg = sis_message_create();
    sis_message_set_str(msg, "work-path", context->work_path, sis_sdslen(context->work_path));
    sis_message_set_str(msg, "work-name", context->work_name, sis_sdslen(context->work_name));
    sis_message_set_int(msg, "work-date", context->work_date);
    sis_worker_command(context->wlog_worker, "close", msg);
    sis_message_destroy(msg);
    context->wlog_open = 0;
}
void sisdb_wlog_move(s_sisdb_cxt *context)
{
    s_sis_message *msg = sis_message_create();
    sis_message_set_str(msg, "work-path", context->work_path, sis_sdslen(context->work_path));
    sis_message_set_str(msg, "work-name", context->work_name, sis_sdslen(context->work_name));
    sis_message_set_int(msg, "work-date", context->work_date);
    if (context->wlog_open)
    {
        sis_worker_command(context->wlog_worker, "close", msg);
        context->wlog_open = 0;
    }
    sis_worker_command(context->wlog_worker, "move", msg);
    sis_message_destroy(msg);
}