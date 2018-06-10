
#include "digger_io.h"
#include "os_thread.h"
#include "digger_tools.h"

/********************************/
// 一定要用static定义，不然内存混乱
static s_digger_server server = {
    .id = 0,
    .status = STS_SERVER_STATUS_NOINIT,
    .config = NULL,
    .context = NULL,
    .catch_code = NULL,
    .command = NULL};
/********************************/

void _get_fixed_path(char *srcpath_, const char *inpath_, char *outpath_, int size_)
{
    if (!inpath_) {
        sts_sprintf(outpath_,size_,srcpath_);
    } else {
        if (*inpath_=='/') {
            // 如果为根目录，就直接使用
            sts_sprintf(outpath_,size_,inpath_);
        } else {
            // 如果为相对目录，就合并配置文件的目录
            sts_sprintf(outpath_,size_,"%s%s", srcpath_,inpath_);
        }
    }
    // 创建目录
    sts_path_complete(outpath_,STS_PATH_LEN);
    if(!sts_path_mkdir(outpath_))
    {
		sts_out_error(3)("cann't create dir [%s].\n", outpath_);   
    }
    printf("outpath_:%s\n",outpath_);
}


// void *_thread_save_plan_task(void *argv_)
// {
//     s_sts_db *db = (s_sts_db *)argv_;

//     sts_thread_wait_start(&db->thread_wait);
//     while (server.status != STS_SERVER_STATUS_CLOSE)
//     {
//         //  printf("server.status ... %d\n",server.status);
//        // 处理
//         if (db->save_type==STS_SERVER_SAVE_GAPS) {
//             if(sts_thread_wait_sleep(&db->thread_wait, db->save_gaps) == STS_ETIMEDOUT)
//             {
//                 sts_mutex_lock(&server.db->save_mutex);
//                 sts_db_file_save(server.dbpath,db);
//                 sts_mutex_unlock(&server.db->save_mutex);
//             }
//         } else {
//             if(sts_thread_wait_sleep(&db->thread_wait, 30)== STS_ETIMEDOUT)// 30秒判断一次
//             {
//                 int min = sts_time_get_iminute(0);
//                 printf("save plan ... -- -- -- %d \n",min);
//                 for (int k=0;k<db->save_plans->count;k++)
//                 {
//                     uint16 *lm = sts_struct_list_get(db->save_plans, k);
//                     if(min == *lm) {
//                         sts_mutex_lock(&server.db->save_mutex);
//                         sts_db_file_save(server.dbpath,db);
//                         sts_mutex_unlock(&server.db->save_mutex);
//                         printf("save plan ... %d -- -- %d \n",*lm, min);
//                     }
//                 }
//             }
//         }
//         // printf("server.status ... %d\n",server.status);
//     }
//     sts_thread_wait_stop(&db->thread_wait);
//     return NULL;
// }

char * digger_open(const char *conf_)
{
    server.id = 0; // 这里设置当前秒数
    // 加载command list
    server.command = sts_command_create();

    server.config = sts_conf_open(conf_);
    if (!server.config)
    {
        sts_out_error(1)("load conf %s fail.\n", conf_);
        return NULL;
    }
    // 加载可包含的配置文件，方便后面使用
    sts_strcpy(server.conf_name, STS_PATH_LEN, conf_);

    char conf_path[STS_PATH_LEN];
    sts_file_getpath(server.conf_name, conf_path, STS_PATH_LEN);

    _get_fixed_path(conf_path,sts_json_get_str(server.config->node, "dbpath"),
                server.dbpath, STS_PATH_LEN);
    // sts_path_complete(server.dbpath,STS_PATH_LEN);

    s_sts_json_node *lognode = sts_json_cmp_child_node(server.config->node, "log");
    if(lognode) {
        _get_fixed_path(conf_path,sts_json_get_str(lognode, "path"),
                server.logpath, STS_PATH_LEN);
        // sts_path_complete(server.logpath,STS_PATH_LEN);

        server.logsize = sts_json_get_int(lognode,"level",5);
        server.loglevel = sts_json_get_int(lognode,"maxsize",10) * 1024 * 1024;
    }

    s_sts_json_node *service = sts_json_cmp_child_node(server.config->node, "service");
    if(!service) {
        sts_out_error(1)("no find service define.\n");
        goto error;
    }
    sts_strcpy(server.service_name, STS_DIGGER_MAXLEN, sts_json_get_str(service,"name"));
    sts_strcpy(server.source_name, STS_DIGGER_MAXLEN, sts_json_get_str(service,"source"));
    if(strlen(server.source_name) < 1) {
        sts_strcpy(server.source_name, STS_DIGGER_MAXLEN, "stsdb");
    }
    // 设置文件版本号
    sts_json_object_add_int(service, "version", STS_DIGGER_FILE_VERSION);


    server.status = STS_SERVER_STATUS_INITED;
    // sts_out_error(3)("server.status: %d\n", server.status);
    // server.status = STS_SERVER_STATUS_LOADED;
    sts_conf_close(server.config);
    return server.service_name;
error:
    sts_conf_close(server.config);
    sts_command_destroy(server.command);
    return NULL;
}

void digger_close()
{
    if (server.status==STS_SERVER_STATUS_CLOSE) {
        return ;
    }
    server.status = STS_SERVER_STATUS_CLOSE;
    // 停止所有的正在运行的策略
    // digger_stop();

    sts_command_destroy(server.command);

}

s_sts_sds digger_start_sds(const char *name_, const char *com_)
{
    return sdsnew("my is digger_start\n");
}
int digger_stop(const char *key_)
{
    return STS_SERVER_OK;
}

s_sts_sds digger_get_sds(const char *db_,const char *key_, const char *com_)
{
    if (server.status != STS_SERVER_STATUS_INITED)
    {
        sts_out_error(3)("no init stsdb.\n");
        return NULL;
    }
    // s_sts_table *table = sts_db_get_table(server.db, db_);
    // if (!table)
    // {
    //     sts_out_error(3)("no find %s db.\n", db_);
    //     return NULL;
    // }
    // return sts_table_get_sds(table, key_, com_);
    return sdsnew("my is digger_get\n");
}
// int digger_get(s_sts_module_context *ctx_, const char *key_, const char *com_)
// {
//     sts_module_memory_init(ctx_);

//     const char *dbpath = sts_conf_get_str(server.config->node, "dbpath");
//     printf("%s/%s/%s\n", server.conf_path, dbpath, key_);
//     char *fn = sts_str_sprintf(128, "%s/%s/%s.json",
//                                server.conf_path,
//                                dbpath,
//                                sts_str_replace(key_,'.','/'));
// 	s_sts_sds buffer = sts_file_read_to_sds(fn);
// 	if (!buffer)
//     {
//         sts_free(fn);
//         sts_module_reply_with_error(ctx_, "no file!");
//         return STS_MODULE_ERROR;
//     }
//     sts_module_reply_with_string(ctx_, sts_module_string_create(ctx_, buffer, sts_sdslen(buffer)));
//     sts_sdsfree(buffer);
//     sts_free(fn);
//     return STS_MODULE_OK;
// }
// int stsdb_get(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *com_)
// {
//     sts_module_memory_init(ctx_);

//     const char *dbpath = sts_conf_get_str(server.config->node, "dbpath");
//     printf("%s/%s/stock/%s.%s\n", server.conf_path, dbpath, db_, key_);
//     char *fn = sts_str_sprintf(128, "%s/%s/stock/%s.%s.json",
//                                server.conf_path,
//                                dbpath,
//                                key_,
//                                db_
//                                );
// 	s_sts_sds buffer = sts_file_read_to_sds(fn);
// 	if (!buffer)
//     {
//         sts_free(fn);
//         sts_module_reply_with_error(ctx_, "no file!");
//         return STS_MODULE_ERROR;
//     }
//     sts_module_reply_with_string(ctx_, sts_module_string_create(ctx_, buffer, sts_sdslen(buffer)));
//     sts_sdsfree(buffer);
//     sts_free(fn);
//     return STS_MODULE_OK;
// }
int digger_set(const char *db_, const char *key_, const char *com_)
{
    return STS_SERVER_OK;
}

// s_sts_sds digger_sub_sds(const char *db_, const char *key_)
// {
//     return sdsnew("my is digger_sub\n");
// }    

// int digger_pub(const char *db_, const char *key_, const char *com_)
// {
//     return STS_SERVER_OK;
// }

s_sts_sds digger_call_sds(void *context_,const char *func_, const char *argv_)
{
    if (server.status != STS_SERVER_STATUS_INITED)
    {
        sts_out_error(3)("no init stsdb.\n");
        return NULL;
    }
    s_sts_command *func = sts_command_get(server.command, func_);
    if (!func)
    {
        sts_out_error(3)("no find %s command.\n", func_);
        return NULL;
    }   
    server.context = context_;
    return func->proc(&server, argv_);

}


// int stsdb_set(const char *dt_, const char *db_, const char *key_, const char *val_, size_t len_)
// {
//     if (server.status != STS_SERVER_STATUS_INITED)
//     {
//         sts_out_error(3)("no init stsdb.\n");
//         return STS_SERVER_ERROR;
//     }

// 	int uid = sts_db_find_map_uid(server.db, dt_, STS_MAP_DEFINE_DATA_TYPE);
// 	if (uid != STS_DATA_STRUCT && uid != STS_DATA_JSON && uid != STS_DATA_ARRAY)
// 	{
//         sts_out_error(3)("set data type error.\n");
//         return STS_SERVER_ERROR;
// 	}

//     // 如果保存aof失败就返回错误
//     if (!sts_db_file_save_aof(server.dbpath, server.db, uid, db_, key_, val_, len_))
//     {
//         sts_out_error(3)("save aof error.\n");
//         return STS_SERVER_ERROR;
//     }
//     if (sts_mutex_trylock(&server.db->save_mutex)) {
//         // == 0 才是锁住
//         sts_out_error(3)("saveing... set fail.\n");
//         return STS_SERVER_OK;
//     };
//     int o = stsdb_set_format(uid,db_,key_,val_,len_);
//     sts_mutex_unlock(&server.db->save_mutex);

//     return o;
// }
