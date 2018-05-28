﻿
#include "sts_db_io.h"
#include "os_thread.h"

/********************************/
// 一定要用static定义，不然内存混乱
static s_stsdb_server server = {
    .status = STS_SERVER_STATUS_NOINIT,
    .config = NULL};
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
}

void *_thread_save_file(void *argv_)
{
    s_sts_db *db = (s_sts_db *)argv_;

    sts_thread_wait_start(&db->thread_wait);
    while (server.status != STS_SERVER_STATUS_CLOSE)
    {
        if (db->save_type==STS_SERVER_SAVE_GAPS) {
            if(sts_thread_wait_sleep(&db->thread_wait, db->save_gaps) == STS_ETIMEDOUT)
            {
                sts_db_file_save(server.dbpath,db);
            }
        } else {
            if(sts_thread_wait_sleep(&db->thread_wait, 30)== STS_ETIMEDOUT)// 30秒判断一次
            {
                int min = sts_time_get_iminute(0);
                for (int k=0;k<db->save_plans->count;k++)
                {
                    uint16 *lm = sts_struct_list_get(db->save_plans, k);
                    if(min == *lm) {
                        sts_db_file_save(server.dbpath,db);
                    }
                }
            }
        }
    }
    sts_thread_wait_stop(&db->thread_wait);
    return NULL;
}

char * stsdb_init(const char *conf_)
{
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
 
    s_sts_json_node *lognode = sts_json_cmp_child_node(server.config->node, "log");
    if(lognode) {
        _get_fixed_path(conf_path,sts_json_get_str(lognode, "path"),
                server.logpath, STS_PATH_LEN);

        server.logsize = sts_json_get_int(lognode,"level",5);
        server.loglevel = sts_json_get_int(lognode,"maxsize",10) * 1024 * 1024;
    }

    s_sts_json_node *service = sts_json_cmp_child_node(server.config->node, "service");
    if(!service) {
        sts_out_error(1)("no find service define.\n");
        sts_conf_close(server.config);
        return NULL;
    }
    sts_strcpy(server.service_name, STS_TABLE_MAXLEN, sts_json_get_str(service,"name"));

    //-------- db start ----------//
    server.db = sts_db_create(server.service_name);

    s_sts_json_node *wtime = sts_json_cmp_child_node(service, "work-time");
    if (!wtime) {
        sts_out_error(1)("no find work time define.\n");
        goto error;   
    }
    server.db->work_time.first = sts_json_get_int(wtime,"open",900);
    server.db->work_time.second = sts_json_get_int(wtime,"close",1530);

    s_sts_json_node *ttime = sts_json_cmp_child_node(service, "trade-time");
    if (!ttime) {
        sts_out_error(1)("no find trade time define.\n");
        goto error;   
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
        goto error;   
    }

    server.db->save_type = STS_SERVER_SAVE_NONE;
    
    s_sts_json_node *stime = sts_json_cmp_child_node(service, "save-gaps");
    if (stime) {
        server.db->save_type = STS_SERVER_SAVE_GAPS;
        // 默认15秒存盘一次，
        server.db->save_gaps = sts_json_get_int(service, "save-gaps", 30); 
        if (server.db->save_gaps < 5) {
            server.db->save_gaps = 5;
        }
    }

    stime = sts_json_cmp_child_node(service, "save-plans");
    if (stime) {
        server.db->save_type = STS_SERVER_SAVE_PLANS;
        int count = sts_json_get_size(stime);
        char key[16];
        for (int k=0;k<count;k++){
            sts_sprintf(key,10, "%d",k);
            uint16 min = sts_json_get_int(stime,key,930); 
            sts_struct_list_push(server.db->save_plans, &min);
            // printf("save time [%d]\n",min);
        }
    }
	server.db->save_format = sts_db_find_map_uid(server.db, 
            sts_json_get_str(service, "save-format"), 
            STS_MAP_DEFINE_DATA_TYPE);

    s_sts_json_node *node = sts_json_cmp_child_node(service, "tables");
    server.db->conf = sts_conf_to_json(node);

    s_sts_json_node *info = sts_conf_first_node(node);
    while (info)
    {
        // s_sts_table *table = 
        sts_table_create(server.db, info->key, info);
        info = info->next;
    }
    
    if(!sts_db_file_check(server.dbpath, server.db))
    {
        sts_out_error(1)("file format ver error.\n");
        goto error;
    }

    if (!sts_db_file_load(server.dbpath, server.db))
    {
        sts_out_error(1)("load sdb fail. exit\n");
        goto error;        
    }
    // 启动存盘线程
    sts_thread_wait_create(&server.db->thread_wait);
    
    // 启动存盘线程
    server.db->save_pid = 0;
    sts_mutex_rw_create(&server.db->save_mutex);

    if (server.db->save_type!=STS_SERVER_SAVE_NONE) 
    {
        if (sts_thread_create(_thread_save_file, server.db, &server.db->save_pid) != 0)
        {
            sts_out_error(1)("can't start thread\n");
            goto error;       
        }  
    }
    server.status = STS_SERVER_STATUS_INITED;
    // sts_out_error(3)("server.status: %d\n", server.status);
    // server.status = STS_SERVER_STATUS_LOADED;
    sts_conf_close(server.config);
    return server.service_name;
error:
    sts_db_destroy(server.db);
    sts_conf_close(server.config);
    return NULL;
}

void stsdb_close()
{
    server.status = STS_SERVER_STATUS_CLOSE;

    sts_thread_wait_kill(&server.db->thread_wait);

    if(server.db&&!server.db->save_pid){
        sts_thread_join(server.db->save_pid);
        sts_mutex_rw_destroy(&server.db->save_mutex);
    }
    sts_db_destroy(server.db);

    sts_thread_wait_destroy(&server.db->thread_wait);
}

s_sts_sds stsdb_list()
{
    if (server.status != STS_SERVER_STATUS_INITED)
    {
        sts_out_error(3)("no init stsdb.\n");
        return NULL;
    }
    return sts_db_get_table_info_sds(server.db);
}

s_sts_sds stsdb_get(const char *db_, const char *key_, const char *com_)
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
    return sts_table_get_sds(table, key_, com_);
}


int stsdb_set_format(int format_, const char *db_, const char *key_, const char *val_, size_t len_)
{
    // if (sts_strcasecmp(key_,"SH600048")) return STS_SERVER_ERROR;
    // printf("%s.%s  %ld\n", key_, db_, len_);
    if (format_ == STS_DATA_STRUCT)
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
    int o = sts_table_update_mul(format_, table, key_, val_, len_);
    if(o) {
        sts_out_error(5)("set data ok,[%d].\n", o);
        return STS_SERVER_OK;
    }
    return STS_SERVER_ERROR;
}

int stsdb_set(const char *dt_, const char *db_, const char *key_, const char *val_, size_t len_)
{
    if (server.status != STS_SERVER_STATUS_INITED)
    {
        sts_out_error(3)("no init stsdb.\n");
        return STS_SERVER_ERROR;
    }

	int uid = sts_db_find_map_uid(server.db, dt_, STS_MAP_DEFINE_DATA_TYPE);
	if (uid != STS_DATA_STRUCT && uid != STS_DATA_JSON && uid != STS_DATA_ARRAY)
	{
        sts_out_error(3)("set data type error.\n");
        return STS_SERVER_ERROR;
	}

    // 如果保存aof失败就返回错误
    if (!sts_db_file_save_aof(server.dbpath, server.db, uid, db_, key_, val_, len_))
    {
        sts_out_error(3)("set data type error.\n");
        return STS_SERVER_ERROR;
    }

    return stsdb_set_format(uid,db_,key_,val_,len);
}