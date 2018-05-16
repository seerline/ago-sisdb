

#include "sts_db_io.h"

/********************************/
// 一定要用static定义，不然内存混乱
static s_stsdb_server server = {
    .status = STS_SERVER_STATUS_NOINIT,
    .config = NULL};
/********************************/
s_stsdb_server sss;
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

    // sss = malloc(sizeof(s_stsdb_server));
    // sprintf(sss.conf_name,"dzd ok");

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
    return STS_MODULE_OK;
}

int stsdb_start(s_sts_module_context *ctx_)
{
    if (server.status == STS_SERVER_STATUS_NOINIT)
    {
        return sts_module_reply_with_error(ctx_, "goto: stsdb.start \n");
    }
    if (server.status == STS_SERVER_STATUS_LOADED)
    {
        return sts_module_reply_with_error(ctx_, "stsdb already start.\n");
    }
    // .. 这里从硬盘上加载所有的键值到redis中，
    //
    // sss = sts_malloc(sizeof(s_stsdb_server));
    // sprintf(sss->conf_name,"dzd ok");

    server.status = STS_SERVER_STATUS_LOADED;
    return STS_MODULE_OK;
}

int stsdb_get(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *com_)
{
    if (server.status != STS_SERVER_STATUS_LOADED)
    {
        // return sts_module_reply_with_error(ctx_, "no start stsdb.\n");
    }
    sts_module_reply_with_simple_string(ctx_, sss.conf_name);
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
    return STS_MODULE_OK;
}

int stsdb_set_json(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *val_)
{
    if (server.status != STS_SERVER_STATUS_LOADED)
    {
        return sts_module_reply_with_error(ctx_, "no start stsdb.\n");
    }
    printf("%s.%s\n", db_, key_);
    sts_module_reply_with_simple_string(ctx_, "OK");
    return STS_MODULE_OK;
}

int stsdb_set_struct(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *val_)
{
    if (server.status != STS_SERVER_STATUS_LOADED)
    {
        return sts_module_reply_with_error(ctx_, "no start stsdb.\n");
    }
    printf("%s.%s\n", db_, key_);
    sts_module_reply_with_simple_string(ctx_, "OK");
    return STS_MODULE_OK;
}