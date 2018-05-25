
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
    sts_strcpy(server.service_name, STS_NAME_LEN, sts_json_get_str(service,"name"));

    server.db = sts_db_create(server.service_name);

    s_sts_json_node *wtime = sts_json_cmp_child_node(service, "work-time");
    if (!wtime) {
        sts_out_error(1)("no find work time define.\n");
         return NULL;
    }
    server.db->work_time.first = sts_json_get_int(wtime,"open",900);
    server.db->work_time.second = sts_json_get_int(wtime,"close",1530);

    s_sts_json_node *ttime = sts_json_cmp_child_node(service, "trade-time");
    if (!ttime) {
        sts_out_error(1)("no find trade time define.\n");
         return NULL;
    }
    s_sts_time_pair pair;
    s_sts_json_node *next = sts_json_first_node(ttime);
    while(next)
    {
        pair.first = sts_json_get_int(next,"0",930);
        pair.second = sts_json_get_int(next,"1",1130);
        sts_struct_list_push(server.db->trade_time, &pair);
        // printf("trade time [%d, %d]\n",pair.begin,pair.end);
        next = next->next;
    }
    if (server.db->trade_time->count < 1) {
        sts_out_error(1)("trade time < 1.\n");
        return NULL;
    }

    s_sts_json_node *stime = sts_json_cmp_child_node(service, "save-plans");
    if (stime) {
        int count = sts_json_get_size(stime);
        char key[16];
        for (int k=0;k<count;k++){
            sts_sprintf(key,10, "%d",k);
            uint16 min = sts_json_get_int(stime,key,930); 
            sts_struct_list_push(server.db->save_plans, &min);
            // printf("save time [%d]\n",min);
        }
    }

    s_sts_json_node *node = sts_json_cmp_child_node(service, "tables");
    s_sts_json_node *info = sts_conf_first_node(node);
    while (info)
    {
        // s_sts_table *table = 
        sts_table_create(server.db, info->key, info);
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
    return sts_db_get_table_info(server.db);
}

sds stsdb_get(const char *db_, const char *key_, const char *com_)
{
    if (server.status != STS_SERVER_STATUS_INITED)
    {
        sts_out_error(3)("no init stsdb.\n");
        return NULL;
    }
    s_sts_table *table = sts_db_get_table(server.db, db_);
    if (!table)
    {
        sts_out_error(3)("no find %s db.\n", db_);
        return NULL;
    }
    return sts_table_get_m(table, key_, com_);
}


int stsdb_set(const char *dt_, const char *db_, const char *key_, const char *val_, size_t len_)
{
	int uid = sts_db_find_map_uid(server.db, dt_, STS_MAP_DEFINE_DATA_TYPE);
	if (uid != STS_DATA_STRUCT && uid != STS_DATA_JSON && uid != STS_DATA_ARRAY)
	{
        sts_out_error(3)("set data type error.\n");
        return STS_SERVER_ERROR;
	}

    if (server.status != STS_SERVER_STATUS_INITED)
    {
        sts_out_error(3)("no init stsdb.\n");
        return STS_SERVER_ERROR;
    }
    // if (sts_strcasecmp(key_,"SH600048")) return STS_SERVER_ERROR;
    // printf("%s.%s  %ld\n", key_, db_, len_);
    if (uid == STS_DATA_STRUCT)
    {
         sts_out_binary("set", val_, 30);
    }   
    else{
        printf("val : %s\n", val_);
    }
    s_sts_table *table = sts_db_get_table(server.db, db_);
    if (!table)
    {
        sts_out_error(3)("no find %s db.\n", db_);
        return STS_SERVER_ERROR;
    }
    // 来源是结构体数据的，必须只能往结构体table写数据，
    int o = sts_table_update_mul(uid, table, key_, val_, len_);
    if(o) {
        sts_out_error(5)("set data ok,[%d].\n", o);
        return STS_SERVER_OK;
    }
    return STS_SERVER_ERROR;
}