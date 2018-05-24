
#include "sts_db_io.h"

/********************************/
// 一定要用static定义，不然内存混乱
static s_stsdb_server server = {
    .status = STS_SERVER_STATUS_NOINIT,
    .config = NULL};
/********************************/

char * stsdb_init(const char *conf_)
{
    server.config = sts_conf_open(conf_);
    if (!server.config)
    {
        sts_out_error(1)("load conf %s fail.\n", conf_);
        return NULL;
    }
    // 加载可包含的配置文件，方便后面使用

    sts_strcpy(server.conf_name, STS_FILE_PATH_LEN, conf_);
    sts_file_getpath(server.conf_name, server.conf_path, STS_FILE_PATH_LEN);

    s_sts_json_node *service = sts_json_cmp_child_node(server.config->node, "service");
    if(!service) {
        sts_out_error(1)("no find service define.\n");
         return NULL;
    }
    sts_strcpy(server.service_name, STS_STR_LEN, sts_json_get_str(service,"name"));

    sts_db_create();
    s_sts_json_node *node = sts_json_cmp_child_node(service, "tables");
    s_sts_json_node *info = sts_conf_first_node(node);
    while (info)
    {
        s_sts_table *table = sts_table_create(info->key, info);
        sts_db_install_table(table);
        info = info->next;
    }
    server.status = STS_SERVER_STATUS_INITED;
    // sts_out_error(3)("server.status: %d\n", server.status);
    // server.status = STS_SERVER_STATUS_LOADED;
    return server.service_name;
}
sds stsdb_list()
{
    if (server.status != STS_SERVER_STATUS_INITED)
    {
        sts_out_error(3)("no init stsdb.\n");
        return NULL;
    }
    return sts_db_get_tables();
}

sds stsdb_get(const char *db_, const char *key_, const char *com_)
{
    if (server.status != STS_SERVER_STATUS_INITED)
    {
        sts_out_error(3)("no init stsdb.\n");
        return NULL;
    }
    s_sts_table *table = sts_db_get_table(db_);
    if (!table)
    {
        sts_out_error(3)("no find %s db.\n", db_);
        return NULL;
    }
    return sts_table_get_m(table, key_, com_);
}

// int stsdb_set(int type_, const char *db_, const char *key_, const char *val_, size_t len_)
// {
//     if (server.status != STS_SERVER_STATUS_INITED)
//     {
//         sts_out_error(3)("no init stsdb.\n");
//         return STS_SERVER_ERROR;
//     }
//     printf("%s.%s  %ld\n", key_, db_, len_);
//     if (type_ == STS_DATA_STRUCT)
//     {
//          sts_out_binary("set", val_, 30);
//     }   
//     else{
//         printf("val : %s\n", val_);
//     }

//     s_sts_table *table = sts_db_get_table(db_);
//     if (!table)
//     {
//         sts_out_error(3)("no find %s db.\n", db_);
//         return STS_SERVER_ERROR;
//     }
//     // 来源是结构体数据的，必须只能往结构体table写数据，
//     int o = sts_table_update(table, key_, type_, val_, len_);
//     if(o) {
//         sts_out_error(5)("set data ok,[%d].\n", o);
//         return STS_SERVER_OK;
//     }
//     return STS_SERVER_ERROR;
// }

int stsdb_set(int type_, const char *db_, const char *key_, const char *val_, size_t len_)
{
    if (server.status != STS_SERVER_STATUS_INITED)
    {
        sts_out_error(3)("no init stsdb.\n");
        return STS_SERVER_ERROR;
    }
    printf("%s.%s  %ld\n", key_, db_, len_);
    if (type_ == STS_DATA_STRUCT)
    {
         sts_out_binary("set", val_, 30);
    }   
    else{
        printf("val : %s\n", val_);
    }

    // 来源是结构体数据的，必须只能往结构体table写数据，
    int o = sts_table_update_mul(type_, db_, key_, val_, len_);
    if(o) {
        sts_out_error(5)("set data ok,[%d].\n", o);
        return STS_SERVER_OK;
    }
    return STS_SERVER_ERROR;
}