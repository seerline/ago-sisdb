
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

int stsdb_get(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *com_)
{
    if (server.status != STS_SERVER_STATUS_INITED)
    {
        return sts_module_reply_with_error(ctx_, "no init stsdb.\n");
    }
    s_sts_table *table = sts_db_get_table(db_);
    if(!table) {
       sts_out_error(3)("no find %s db.\n", db_);
    }
    sds o = sts_table_get_m(table, key_, com_);
    if (o) {
        sts_module_reply_with_simple_string(ctx_, o);
        sdsfree(o);
        return STS_MODULE_OK; 
    } 
    sts_module_reply_with_null(ctx_);
    return STS_MODULE_ERROR;
}

int stsdb_set_json(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *val_, size_t len_)
{
    if (server.status != STS_SERVER_STATUS_INITED)
    {
        return sts_module_reply_with_error(ctx_, "no init stsdb.\n");
    }
    printf("json : %s.%s %s\n", key_, db_, val_);
    s_sts_table *table = sts_db_get_table(db_);
    if(!table) {
       sts_out_error(3)("no find %s db.\n", db_);
    }
    // 来源是json的，如果要写数据到结构体中，需要根据字段有选择的写数据
    sts_table_update(table, key_, STS_DATA_JSON, val_, len_);

    sts_module_reply_with_simple_string(ctx_, "OK");
    return STS_MODULE_OK;
}

int stsdb_set_struct(s_sts_module_context *ctx_, const char *db_, const char *key_, const char *val_, size_t len_)
{
    if (server.status != STS_SERVER_STATUS_INITED)
    {
        return sts_module_reply_with_error(ctx_, "no init stsdb.\n");
    }
    printf("%s.%s  %ld\n", key_, db_, len_);

    s_sts_table *table = sts_db_get_table(db_);
    if(!table) {
       sts_out_error(3)("no find %s db.\n", db_);
    }
    // 来源是结构体数据的，必须只能往结构体table写数据，
    sts_out_binary("set", val_, 30);
    sts_table_update(table, key_, STS_DATA_STRUCT, val_, len_);

    sts_module_reply_with_simple_string(ctx_, "OK");
    return STS_MODULE_OK;
}