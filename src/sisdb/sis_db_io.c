
#include "sis_db_io.h"
#include "os_thread.h"

/********************************/
// 一定要用static定义，不然内存混乱
static s_sisdb_server server = {
    .status = SIS_SERVER_STATUS_NOINIT,
    .config = NULL,
    .db = NULL};
/********************************/

void *_thread_save_plan_task(void *argv_)
{
    s_sis_db *db = (s_sis_db *)argv_;

    // 创建等待信号通知
    sis_thread_wait_create(&server.db->thread_wait);

    sis_mutex_create(&server.db->save_mutex);

    sis_thread_wait_start(&db->thread_wait);

    while (server.status != SIS_SERVER_STATUS_CLOSE)
    {
        //  printf("server.status ... %d\n",server.status);
        // 处理
        if (db->save_mode == SIS_WORK_MODE_GAPS)
        {
            if (sis_thread_wait_sleep(&db->thread_wait, db->save_always.delay) == SIS_ETIMEDOUT)
            {
                sis_mutex_lock(&server.db->save_mutex);
                sis_db_file_save(server.dbpath, db);
                sis_mutex_unlock(&server.db->save_mutex);
            }
        }
        else
        {
            if (sis_thread_wait_sleep(&db->thread_wait, 30) == SIS_ETIMEDOUT) // 30秒判断一次
            {
                int min = sis_time_get_iminute(0);
                // printf("save plan ... -- -- -- %d \n", min);
                for (int k = 0; k < db->save_plans->count; k++)
                {
                    uint16 *lm = sis_struct_list_get(db->save_plans, k);
                    if (min == *lm)
                    {
                        sis_mutex_lock(&server.db->save_mutex);
                        sis_db_file_save(server.dbpath, db);
                        sis_mutex_unlock(&server.db->save_mutex);
                        printf("save plan ... %d -- -- %d \n", *lm, min);
                    }
                }
            }
        }
        // printf("server.status ... %d\n",server.status);
    }
    sis_thread_wait_stop(&db->thread_wait);

    sis_mutex_destroy(&server.db->save_mutex);

    sis_thread_wait_destroy(&server.db->thread_wait);

    return NULL;
}

char *sisdb_open(const char *conf_)
{

    server.config = sis_conf_open(conf_);
    if (!server.config)
    {
        sis_out_error(1)("load conf %s fail.\n", conf_);
        return NULL;
    }
    // 加载可包含的配置文件，方便后面使用

    sis_strcpy(server.conf_name, SIS_PATH_LEN, conf_);

    char conf_path[SIS_PATH_LEN];
    sis_file_getpath(server.conf_name, conf_path, SIS_PATH_LEN);

    sis_get_fixed_path(conf_path, sis_json_get_str(server.config->node, "dbpath"),
                       server.dbpath, SIS_PATH_LEN);
    // sis_path_complete(server.dbpath,SIS_PATH_LEN);

    s_sis_json_node *lognode = sis_json_cmp_child_node(server.config->node, "log");
    if (lognode)
    {
        sis_get_fixed_path(conf_path, sis_json_get_str(lognode, "path"),
                           server.logpath, SIS_PATH_LEN);
        // sis_path_complete(server.logpath,SIS_PATH_LEN);

        server.loglevel = sis_json_get_int(lognode, "level", 5);
        server.logsize = sis_json_get_int(lognode, "maxsize", 10) * 1024 * 1024;
    }

    s_sis_json_node *service = sis_json_cmp_child_node(server.config->node, "service");
    if (!service)
    {
        sis_out_error(1)("no find service define.\n");
        sis_conf_close(server.config);
        return NULL;
    }
    sis_strcpy(server.service_name, SIS_TABLE_MAXLEN, sis_json_get_str(service, "name"));
    // 设置文件版本号
    sis_json_object_add_int(service, "version", SIS_DB_FILE_VERSION);

    //-------- db start ----------//
    s_sis_json_handle *sdb_json = NULL;

    server.db = sis_db_create(server.service_name);

    // server.db->init_time = sis_json_get_int(service,"init-time",900);
    // server.db->init_status = SIS_INIT_WAIT;

    // s_sis_json_node *wtime = sis_json_cmp_child_node(node_, "work-time");
    // if (!wtime)
    // {
    //     sis_out_error(1)("no find work time define.\n");
    //     goto error;
    // }
    // s_sis_json_node *ptime = sis_json_cmp_child_node(wtime, "plans-work");
    // if (ptime)
    // {
    //     server.db->work_mode = SIS_WORK_MODE_PLANS;
    //     server.db->work_plan_first = false;
    //     int count = sis_json_get_size(ptime);
    //     for (int k = 0; k < count; k++)
    //     {
    //         uint16 min = sis_array_get_int(ptime, k, 0);
    //         if (min == 0) // 有0表示不开机就运行
    //         {
    //             server.db->work_plan_first = true;
    //             continue;
    //         }
    //         sis_struct_list_push(server.db->work_plans, &min);
    //     }
    // }
    // s_sis_json_node *atime = sis_json_cmp_child_node(wtime, "always-work");
    // if (atime)
    // {
    //     server.db->work_mode = SIS_WORK_MODE_GAPS;
    //     server.db->work_always.start = sis_json_get_int(atime, "start", 900);
    //     server.db->work_always.stop = sis_json_get_int(atime, "stop", 1530);
    //     server.db->work_always.delay = sis_json_get_int(atime, "delay", 3000);
    // }

    s_sis_json_node *ttime = sis_json_cmp_child_node(service, "trade-time");
    s_sis_time_pair pair;
    if (!ttime)
    {
        // sis_out_error(1)("no find trade time define.\n");
        // goto error;
        // 不存在就直接赋值一个
        pair.first = 0;
        pair.second = 2400;
        sis_struct_list_push(server.db->trade_time, &pair);
    }
    else
    {
        s_sis_json_node *next = sis_json_first_node(ttime);
        while (next)
        {
            pair.first = sis_json_get_int(next, "0", 930);
            pair.second = sis_json_get_int(next, "1", 1130);
            sis_struct_list_push(server.db->trade_time, &pair);
            // printf("trade time [%d, %d]\n",pair.begin,pair.end);
            next = next->next;
        }
    }
    if (server.db->trade_time->count < 1)
    {
        sis_out_error(1)("trade time < 1.\n");
        goto error;
    }

    server.db->save_mode = SIS_WORK_MODE_NONE;
    s_sis_json_node *stime = sis_json_cmp_child_node(service, "save-time");
    if (stime)
    {
        s_sis_json_node *ptime = sis_json_cmp_child_node(stime, "plans-work");
        if (ptime)
        {
            server.db->save_mode = SIS_WORK_MODE_PLANS;
            int count = sis_json_get_size(ptime);
            for (int k = 0; k < count; k++)
            {
                uint16 min = sis_array_get_int(ptime, k, 0);
                sis_struct_list_push(server.db->save_plans, &min);
            }
        }
        s_sis_json_node *atime = sis_json_cmp_child_node(stime, "always-work");
        if (atime)
        {
            server.db->save_mode = SIS_WORK_MODE_GAPS;
            server.db->save_always.start = sis_json_get_int(atime, "start", 900);
            server.db->save_always.stop = sis_json_get_int(atime, "stop", 1530);
            server.db->save_always.delay = sis_json_get_int(atime, "delay", 300);
        }
    }
    // server.db->save_type = SIS_WORK_MODE_NONE;

    // s_sis_json_node *stime = sis_json_cmp_child_node(service, "save-gaps");
    // if (stime)
    // {
    //     server.db->save_type = SIS_WORK_MODE_GAPS;
    //     // 默认15秒存盘一次，
    //     server.db->save_gaps = sis_json_get_int(service, "save-gaps", 30);
    //     if (server.db->save_gaps < 5)
    //     {
    //         server.db->save_gaps = 5;
    //     }
    // }

    // stime = sis_json_cmp_child_node(service, "save-plans");
    // if (stime)
    // {
    //     server.db->save_type = SIS_WORK_MODE_PLANS;
    //     int count = sis_json_get_size(stime);
    //     for (int k = 0; k < count; k++)
    //     {
    //         uint16 min = sis_array_get_int(stime, k, 930);
    //         sis_struct_list_push(server.db->save_plans, &min);
    //         // printf("save time [%d]\n",min);
    //     }
    // }

    server.db->save_format = sis_db_find_map_uid(server.db,
                                                 sis_json_get_str(service, "save-format"),
                                                 SIS_MAP_DEFINE_DATA_TYPE);

    // 启动存盘线程
    server.db->save_pid = 0;
    // 检查数据库文件有没有
    char sdb_json_name[SIS_PATH_LEN];
    sis_sprintf(sdb_json_name, SIS_PATH_LEN, SIS_DB_FILE_CONF, server.dbpath, server.service_name);

    if (!sis_file_exists(sdb_json_name))
    {
        s_sis_json_node *node = sis_json_cmp_child_node(service, "tables");
        // 加载conf
        size_t len = 0;
        char *str = sis_conf_to_json(service, &len);
        server.db->conf = sdsnewlen(str, len);
        sis_free(str);
        s_sis_json_node *info = sis_conf_first_node(node);
        while (info)
        {
            // s_sis_table *table =
            sis_table_create(server.db, info->key, info);
            info = info->next;
        }
    }
    else
    {
        sdb_json = sis_json_open(sdb_json_name);
        if (!sdb_json)
        {
            sis_out_error(1)("load sdb conf %s fail.\n", sdb_json_name);
            goto error;
        }
        // 检查版本号是否匹配
        if (SIS_DB_FILE_VERSION != sis_json_get_int(sdb_json->node, "version", 0))
        {
            sis_out_error(1)("file format ver error.\n");
            goto error;
        }
        // 创建数据表
        s_sis_json_node *node = sis_json_cmp_child_node(sdb_json->node, "tables");
        // 加载conf
        size_t len = 0;
        char *str = sis_conf_to_json(sdb_json->node, &len);
        server.db->conf = sdsnewlen(str, len);
        sis_free(str);

        s_sis_json_node *info = sis_conf_first_node(node);
        while (info)
        {
            sis_table_create(server.db, info->key, info);
            info = info->next;
        }
    }

    // 这里加载数据
    // 应该需要判断数据的版本号，如果不同，应该对磁盘上的数据进行数据字段重新匹配
    // 把老库中有的字段加载到新的库中，再存盘
    if (!sis_db_file_load(server.dbpath, server.db))
    {
        sis_out_error(1)("load sdb fail. exit!\n");
        goto error;
    }
    // 启动存盘线程
    if (server.db->save_mode != SIS_WORK_MODE_NONE)
    {
        if (!sis_thread_create(_thread_save_plan_task, server.db, &server.db->save_pid))
        {
            sis_out_error(1)("can't start save thread\n");
            goto error;
        }
    }

    server.status = SIS_SERVER_STATUS_INITED;
    sis_conf_close(server.config);

    return server.service_name;

error:
    if (sdb_json)
    {
        sis_json_close(sdb_json);
    }
    sis_db_destroy(server.db);
    sis_conf_close(server.config);
    return NULL;
}

void sisdb_close()
{
    if (server.status == SIS_SERVER_STATUS_CLOSE)
    {
        return;
    }
    server.status = SIS_SERVER_STATUS_CLOSE;

    sis_thread_wait_kill(&server.db->thread_wait);
    //???这里要好好测试一下，看看两个能不能一起退出来
    // sis_thread_join(server.db->init_pid);

    if (server.db && server.db->save_pid)
    {
        sis_thread_join(server.db->save_pid);
        printf("save_pid end.\n");
    }

    sis_db_destroy(server.db);
}
bool sisdb_save()
{
    if (server.status != SIS_SERVER_STATUS_INITED)
    {
        sis_out_error(3)("no init sisdb.\n");
        return false;
    }
    return sis_db_file_save(server.dbpath, server.db);
}
bool sisdb_saveto(const char *dt_, const char *db_)
{
    if (server.status != SIS_SERVER_STATUS_INITED)
    {
        sis_out_error(3)("no init sisdb.\n");
        return false;
    }

    int uid = sis_db_find_map_uid(server.db, dt_, SIS_MAP_DEFINE_DATA_TYPE);
    if (uid != SIS_DATA_CSV && uid != SIS_DATA_JSON && uid != SIS_DATA_ARRAY)
    {
        sis_out_error(3)("save data type error.\n");
        return false;
    }

    return sis_db_file_saveto(server.dbpath, server.db, uid, db_);
}
s_sis_sds sisdb_list_sds()
{
    if (server.status != SIS_SERVER_STATUS_INITED)
    {
        sis_out_error(3)("no init sisdb.\n");
        return NULL;
    }
    return sis_db_get_table_info_sds(server.db);
}

s_sis_sds sisdb_get_sds(const char *db_, const char *key_, const char *com_)
{
    if (server.status != SIS_SERVER_STATUS_INITED)
    {
        sis_out_error(3)("no init sisdb.\n");
        return NULL;
    }
    s_sis_table *table = sis_db_get_table(server.db, db_);
    if (!table)
    {
        sis_out_error(3)("no find %s db.\n", db_);
        return NULL;
    }
    if (!sis_strcasecmp(key_, "collects"))
    {
        return sis_table_get_collects_sds(table, com_);
    }
    else
    {
        return sis_table_get_sds(table, key_, com_);
    }
}

int sisdb_set_format(int format_, const char *db_, const char *key_, const char *val_, size_t len_)
{
    // if (sis_strcasecmp(key_,"SH600048")) return SIS_SERVER_REPLY_ERR;
    printf("----[%d] %s.%s  %ld\n", format_, key_, db_, len_);
    if (format_ == SIS_DATA_STRUCT)
    {
        // sis_out_binary("set", val_, len_);
    }
    else
    {
        printf("%s val : %s\n", __func__, val_);
    }
    s_sis_table *table = sis_db_get_table(server.db, db_);
    if (!table)
    {
        sis_out_error(3)("no find %s db.\n", db_);
        return SIS_SERVER_REPLY_ERR;
    }
    // 来源是结构体数据的，必须只能往结构体table写数据，
    int o = sis_table_update_mul(format_, table, key_, val_, len_);
    if (o)
    {
        sis_out_error(5)("set data ok,[%d].\n", o);
        return SIS_SERVER_REPLY_OK;
    }
    return SIS_SERVER_REPLY_ERR;
}

int sisdb_set(const char *dt_, const char *db_, const char *key_, const char *val_, size_t len_)
{
    if (server.status != SIS_SERVER_STATUS_INITED)
    {
        sis_out_error(3)("no init sisdb.\n");
        return SIS_SERVER_REPLY_ERR;
    }

    int uid = sis_db_find_map_uid(server.db, dt_, SIS_MAP_DEFINE_DATA_TYPE);
    if (uid != SIS_DATA_STRUCT && uid != SIS_DATA_JSON && uid != SIS_DATA_ARRAY)
    {
        sis_out_error(3)("set data type error.\n");
        return SIS_SERVER_REPLY_ERR;
    }

    // 如果保存aof失败就返回错误
    if (!sis_db_file_save_aof(server.dbpath, server.db, uid, db_, key_, val_, len_))
    {
        sis_out_error(3)("save aof error.\n");
        return SIS_SERVER_REPLY_ERR;
    }
    if (sis_mutex_trylock(&server.db->save_mutex))
    {
        // == 0 才是锁住
        sis_out_error(3)("saveing... set fail.\n");
        return SIS_SERVER_REPLY_OK;
    };
    int o = sisdb_set_format(uid, db_, key_, val_, len_);
    sis_mutex_unlock(&server.db->save_mutex);

    return o;
}
int sdsdb_delete_market(s_sis_table *tb_, const char *market_)
{
    int o = 0;
    s_sis_dict_entry *de;
    s_sis_dict_iter *di = sis_dict_get_iter(tb_->collect_map);
    while ((de = sis_dict_next(di)) != NULL)
    {
        s_sis_collect_unit *val = (s_sis_collect_unit *)sis_dict_getval(de);
        if (!sis_strncasecmp(sis_dict_getkey(de), market_, strlen(market_)))
        {
            sis_collect_unit_destroy(val);
            o++;
        }
        sis_dict_delete(tb_->collect_map, sis_dict_getkey(de));
    }
    sis_dict_iter_free(di);
    // printf("delete [%s] count=%d\n",tb_->name, o);
    //
    sis_map_buffer_clear(tb_->collect_map);

    return o;
}

int sisdb_init(const char *market_)
{
    int o = 0;
    s_sis_dict_entry *de;
    s_sis_dict_iter *di = sis_dict_get_iter(server.db->db);
    while ((de = sis_dict_next(di)) != NULL)
    {
        s_sis_table *val = (s_sis_table *)sis_dict_getval(de);
        if (val->control.isinit)
        {
            o += sdsdb_delete_market(val, market_);
        }
    }
    sis_dict_iter_free(di);
    return o;
}
