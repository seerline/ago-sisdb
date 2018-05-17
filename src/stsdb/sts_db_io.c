
#include "sts_db_io.h"

/********************************/
// 一定要用static定义，不然内存混乱
static s_stsdb_server server = {
    .status = STS_SERVER_STATUS_NOINIT,
    .config = NULL};
/********************************/

int stsdb_init(const char *conf_)
{
    server.config = sts_conf_open(conf_);
    if (!server.config)
    {
        sts_out_error(1)("load conf %s fail.\n", conf_);
        return STS_MODULE_ERROR;
    }
    // size_t len;
    // printf("%p\n%s\n",server.config,sts_conf_to_json(server.config->node,&len));
    // 加载可包含的配置文件，方便后面使用

    sts_strcpy(server.conf_name, STS_FILE_PATH_LEN, conf_);
    sts_file_getpath(server.conf_name, server.conf_path, STS_FILE_PATH_LEN);

    sts_db_create();
    s_sts_json_node *node = sts_json_cmp_child_node(server.config->node, "tables");
    s_sts_json_node *info = sts_conf_first_node(node);
    while(info) {
        s_sts_table *table = sts_table_create(info->key, info);
        sts_db_install_table(table);
        info = info->next;
    }    
    server.status = STS_SERVER_STATUS_INITED;
    // sts_out_error(3)("server.status: %d\n", server.status);
    // server.status = STS_SERVER_STATUS_LOADED;
    return STS_MODULE_OK;
}
int stsdb_list(s_sts_module_context *ctx_)
{
    if (server.status != STS_SERVER_STATUS_INITED)
    {
        sts_out_error(3)("no init stsdb.\n");
        return -1;//sts_module_reply_with_error(ctx_, "no init stsdb.\n");
    }
    sds tables = sts_db_get_tables();
    sts_module_reply_with_simple_string(ctx_, tables);
    int count = sts_str_substr_nums(tables, ',');
    sdsfree(tables);
    return count;
}

// int stsdb_start(s_sts_module_context *ctx_)
// {
//     if (server.status != STS_SERVER_STATUS_LOADED)
//     {
//         return sts_module_reply_with_error(ctx_, "start stsdb error.\n");
//     }
//     // .. 这里从硬盘上加载所有的键值到redis中，

//     sts_db_create();
//     s_sts_json_node *node = sts_json_cmp_child_node(server.config->node, "tables");
//     s_sts_json_node *info = sts_conf_first_node(node);
//     while(info) {
//         s_sts_table *table = sts_table_create(info->key, info);
//         sts_db_install_table(table);
//         info = info->next;
//     }    

//     server.status = STS_SERVER_STATUS_INITED;
//     sts_module_reply_with_simple_string(ctx_, "OK");
//     return STS_MODULE_OK;
// }

int stsdb_get(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *com_)
{
    if (server.status != STS_SERVER_STATUS_INITED)
    {
        return sts_module_reply_with_error(ctx_, "no init stsdb.\n");
    }
    // sts_module_reply_with_simple_string(ctx_, sss.conf_name);
    // sts_module_memory_init(ctx_);

    // const char *dbpath = sts_conf_get_str(server.config->node, "dbpath");
    // printf("%s/%s/stock/%s.%s\n", server.conf_path, dbpath, db_, key_);
    // char *fn = sts_str_sprintf(128, "%s/%s/stock/%s.%s.json",
    //                            server.conf_path,
    //                            dbpath,
    //                            key_,
    //                            db_
    //                            );
    // size_t size = 0;
    // char *buffer = sts_file_open_and_read(fn, &size);
    // if (size == 0 || !buffer)
    // {
    //     sts_free(fn);
    //     return sts_module_reply_with_error(ctx_, "no file!");
    //     // return STS_MODULE_ERROR;
    // }
    // sts_module_reply_with_string(ctx_, sts_module_string_create(ctx_, buffer, size));
    // sts_free(buffer);
    // sts_free(fn);
    s_sts_table *table = sts_db_get_table(db_);
    printf("%p.%s\n", table, "table->name");
    if(!table) {
       sts_out_error(3)("no find %s db.\n", db_);
    }
    sds o = sts_table_get_m(table, key_, com_);
    printf("get %.*s\n",(int)sdslen(o), o);

    sts_module_reply_with_simple_string(ctx_, "OK");
    return STS_MODULE_OK;
}

int stsdb_set_json(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *val_, size_t len_)
{
    if (server.status != STS_SERVER_STATUS_INITED)
    {
        return sts_module_reply_with_error(ctx_, "no init stsdb.\n");
    }
    printf("%s.%s\n", key_, db_);
    s_sts_table *table = sts_db_get_table(db_);
    // 来源是json的，如果要写数据到结构体中，需要先转换成结构体数据，
    // 然后再写数据必须只能往结构体table写数据，
    if (table->control.data_type != STS_DATA_STRUCT) {

        // sts_table_update_json(table, key_, val_, len_);
    } else { // STS_DATA_STRUCT
        // sds value = sts_table_json_to_struct(table, val_, len_); 
        // sts_table_update_struct(table, key_, value, sdslen(value)); 
        // sdsfree(value);
    }

    sts_module_reply_with_simple_string(ctx_, "OK");
    return STS_MODULE_OK;
}

int stsdb_set_struct(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *val_, size_t len_)
{
    if (server.status != STS_SERVER_STATUS_INITED)
    {
        return sts_module_reply_with_error(ctx_, "no init stsdb.\n");
    }
    printf("%s.%s\n", key_, db_);

    s_sts_table *table = sts_db_get_table(db_);
    printf("%p.%s\n", table, "table->name");
    if(!table) {
       sts_out_error(3)("no find %s db.\n", db_);
    }
    // 来源是结构体数据的，必须只能往结构体table写数据，
    if (table->control.data_type != STS_DATA_STRUCT) {
        return sts_module_reply_with_error(ctx_, "data-type no struct.\n");
    }
    sts_table_update_struct(table, key_, val_, len_);

    sts_module_reply_with_simple_string(ctx_, "OK");
    return STS_MODULE_OK;
}