
#include "sisdb_io.h"
#include "os_thread.h"

/********************************/
// 一定要用static定义，不然内存混乱
static s_sisdb_io server = {
    .status = SIS_SERVER_STATUS_NOINIT,
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
                // sis_mutex_lock(&server.db->save_mutex);
                sisdb_file_save(&server);
                // sis_mutex_unlock(&server.db->save_mutex);
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
                        // sis_mutex_lock(&server.db->save_mutex);
                        sisdb_file_save(&server);
                        // sis_mutex_unlock(&server.db->save_mutex);
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
    s_sis_conf_handle *config = sis_conf_open(conf_);
    if (!config)
    {
        sis_out_log(1)("load conf %s fail.\n", conf_);
        return NULL;
    }
    // 加载可包含的配置文件，方便后面使用

    char config_path[SIS_PATH_LEN];
    //  获取conf文件目录，
    sis_file_getpath(conf_, config_path, SIS_PATH_LEN);

    sis_get_fixed_path(config_path, sis_json_get_str(config->node, "dbpath"),
                       server.dbpath, SIS_PATH_LEN);
    // sis_path_complete(server.dbpath,SIS_PATH_LEN);

    s_sis_json_node *lognode = sis_json_cmp_child_node(config->node, "log");
    if (lognode)
    {
        sis_get_fixed_path(config_path, sis_json_get_str(lognode, "path"),
                           server.logpath, SIS_PATH_LEN);
        // sis_path_complete(server.logpath,SIS_PATH_LEN);

        server.loglevel = sis_json_get_int(lognode, "level", 5);
        server.logsize = sis_json_get_int(lognode, "maxsize", 10) * 1024 * 1024;
    }

    sis_strcpy(server.service_name, SIS_MAXLEN_TABLE, sis_json_get_str(config->node, "service-name"));

    s_sis_json_node *service = sis_json_cmp_child_node(config->node, server.service_name);
    if (!service)
    {
        sis_out_log(1)("service [%s] no define.\n", server.service_name);
        sis_conf_close(config);
        return NULL;
    }
    // 设置文件版本号
    sis_json_object_add_int(service, "version", SIS_DB_FILE_VERSION);

    //-------- db start ----------//
    s_sis_json_handle *sdb_json = NULL;

    server.db = sisdb_create(server.service_name);

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

    server.db->save_format = sisdb_find_map_uid(server.db,
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
            sisdb_table_create(server.db, info->key, info);
            info = info->next;
        }
    }
    else
    {
        sdb_json = sis_json_open(sdb_json_name);
        if (!sdb_json)
        {
            sis_out_log(1)("load sdb conf %s fail.\n", sdb_json_name);
            goto error;
        }
        // 检查版本号是否匹配
        if (SIS_DB_FILE_VERSION != sis_json_get_int(sdb_json->node, "version", 0))
        {
            sis_out_log(1)("file format ver error.\n");
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
            sisdb_table_create(server.db, info->key, info);
            info = info->next;
        }
    }

    // 这里加载数据
    // 应该需要判断数据的版本号，如果不同，应该对磁盘上的数据进行数据字段重新匹配
    // 把老库中有的字段加载到新的库中，再存盘
    if (!sisdb_file_load(&server))
    {
        sis_out_log(1)("load sdb fail. exit!\n");
        goto error;
    }
    // 启动存盘线程
    if (server.db->save_mode != SIS_WORK_MODE_NONE)
    {
        if (!sis_thread_create(_thread_save_plan_task, server.db, &server.db->save_pid))
        {
            sis_out_log(1)("can't start save thread\n");
            goto error;
        }
    }

    server.status = SIS_SERVER_STATUS_INITED;
    sis_conf_close(config);

    return server.service_name;

error:
    if (sdb_json)
    {
        sis_json_close(sdb_json);
    }
    sisdb_destroy(server.db);
    sis_conf_close(config);
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
    //这里要好好测试一下，看看两个以上任务时能不能一起退出来
    // sis_thread_join(server.db->init_pid);

    if (server.db && server.db->save_pid)
    {
        sis_thread_join(server.db->save_pid);
        printf("save_pid end.\n");
    }

    sisdb_destroy(server.db);
}
bool sisdb_save()
{
    if (server.status != SIS_SERVER_STATUS_INITED)
    {
        sis_out_log(3)("no init sisdb.\n");
        return false;
    }
    // 存为struct格式
    return sisdb_file_save(&server);
}
bool sisdb_out(const char *key_, const char *com_)
{
    if (server.status != SIS_SERVER_STATUS_INITED)
    {
        sis_out_log(3)("no init sisdb.\n");
        return false;
    }
    return sisdb_file_out(&server, key_, com_);
}
s_sis_sds sisdb_show_db_info_sds(s_sis_db *db_)
{
    s_sis_sds list = sis_sdsempty();
    if (db_->dbs)
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(db_->dbs);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_table *val = (s_sisdb_table *)sis_dict_getval(de);
            list = sdscatprintf(list, "  %-10s : fields=%2d, len=%lu\n",
                                val->name,
                                sis_string_list_getsize(val->field_name),
                                val->len);
        }
    }
    return list;
}
// 某个股票有多少条记录
s_sis_sds sisdb_show_collect_info_sds(s_sis_db *db_, const char *key_)
{
    s_sisdb_collect *val = sisdb_get_collect(db_, key_);
    if (!val)
    {
        sis_out_log(3)("no find %s key.\n", key_);
        return NULL;
    }
    s_sis_sds list = sis_sdsempty();
    list = sdscatprintf(list, "  %-20s : len=%2d, count=%lu\n",
                        key_,
                        val->value->len,
                        sisdb_collect_recs(val));
    return list;
}
s_sis_sds sisdb_show_sds(const char *key_)
{
    if (server.status != SIS_SERVER_STATUS_INITED)
    {
        sis_out_log(3)("no init sisdb.\n");
        return NULL;
    }
    if (!key_)
    {
        return sisdb_show_db_info_sds(server.db);
    }
    else
    {
        // 这里根据key格式不同，可以有以下功能
        return sisdb_show_collect_info_sds(server.db, key_);
    }
}

s_sis_sds sisdb_get_sds(const char *key_, const char *com_)
{
    if (server.status != SIS_SERVER_STATUS_INITED)
    {
        sis_out_log(3)("no init sisdb.\n");
        return NULL;
    }
    // *.DAY -- 取所有股票符合条件的1条数据
    if (key_[0] == '*')
    {
        char db[SIS_MAXLEN_TABLE];
        sis_str_substr(db, SIS_MAXLEN_TABLE, key_, '.', 1);
        if (key_[1] == '*')
        {
            // 获得某个数据表所有的key键 用 sis_str_match 来判断比较
            return sisdb_collects_get_code_sds(server.db, db, com_);
        }
        else
        {
            // 获得多只股票某类数据的一条记录
            return sisdb_collects_get_last_sds(server.db, db, com_);
        }
    }

    return sisdb_collect_get_sds(server.db, key_, com_);
}

int _sisdb_write_begin(int fmt_, const char *key_, const char *val_, size_t len_)
{
    if (!sisdb_file_save_aof(&server, fmt_, key_, val_, len_))
    {
        sis_out_log(3)("save aof error.\n");
        return SIS_SERVER_REPLY_ERR;
    }
    if (sis_mutex_trylock(&server.db->save_mutex))
    {
        // == 0 才是锁住
        sis_out_log(3)("saveing... set fail.\n");
        return SIS_SERVER_REPLY_ERR;
    };
    return SIS_SERVER_REPLY_OK;
}
void _sisdb_write_end()
{
    sis_mutex_unlock(&server.db->save_mutex);
}

// 直接拷贝
int sisdb_set(int fmt_, const char *key_, const char *val_, size_t len_)
{
    // 1 . 先把来源数据，转换为 srcdb 的二进制结构数据集合
    s_sisdb_collect *collect = sisdb_get_collect(server.db, key_);
    if (!collect)
    {
        collect = sisdb_collect_create(server.db, key_);
        if (!collect)
        {
            return SIS_SERVER_REPLY_ERR;
        }
        // 进行其他的处理
    }

    s_sis_sds in = NULL;

    switch (fmt_)
    {
    case SIS_DATA_TYPE_JSON:
        in = sisdb_collect_json_to_struct_sds(collect, val_, len_);
        break;
    case SIS_DATA_TYPE_ARRAY:
        in = sisdb_collect_array_to_struct_sds(collect, val_, len_);
        break;
    default:
        // 这里应该不用申请新的内存
        in = sis_sdsnewlen(val_, len_);
    }
    // sis_out_binary("update 0 ", in_, ilen_);

    int o = sisdb_collect_update(collect, in);

    if (!server.db->loading)
    {
        // 如果属于磁盘加载就不publish
        char code[SIS_MAXLEN_CODE];
        sis_str_substr(code, SIS_MAXLEN_TABLE, key_, '.', 0);
        sisdb_collect_update_publish(collect, in, code);
    }

    if (o)
    {
        sis_out_log(5)("set data ok,[%d].\n", o);
        return SIS_SERVER_REPLY_OK;
    }
    return SIS_SERVER_REPLY_ERR;
}

int sisdb_set_json(const char *key_, const char *val_, size_t len_)
{
    if (server.status != SIS_SERVER_STATUS_INITED)
    {
        sis_out_log(3)("no init sisdb.\n");
        return SIS_SERVER_REPLY_ERR;
    }
    int fmt = SIS_DATA_TYPE_JSON;
    // 先判断是json or array
    if (val_[0] == '{')
    {
        fmt = SIS_DATA_TYPE_JSON;
    }
    else if (val_[0] == '[')
    {
        fmt = SIS_DATA_TYPE_ARRAY;
    }
    else
    {
        sis_out_log(3)("set data format error.\n");
        return SIS_SERVER_REPLY_ERR;
    }

    if (_sisdb_write_begin(fmt, key_, val_, len_) == SIS_SERVER_REPLY_ERR)
    {
        return SIS_SERVER_REPLY_ERR;
    }
    // 如果保存aof失败就返回错误
    int o = sisdb_set(fmt, key_, val_, len_);

    _sisdb_write_end();

    return o;
}
int sisdb_set_struct(const char *key_, const char *val_, size_t len_)
{
    if (server.status != SIS_SERVER_STATUS_INITED)
    {
        sis_out_log(3)("no init sisdb.\n");
        return SIS_SERVER_REPLY_ERR;
    }

    if (_sisdb_write_begin(SIS_DATA_TYPE_STRUCT, key_, val_, len_) == SIS_SERVER_REPLY_ERR)
    {
        return SIS_SERVER_REPLY_ERR;
    }
    // 如果保存aof失败就返回错误
    int o = sisdb_set(SIS_DATA_TYPE_STRUCT, key_, val_, len_);

    _sisdb_write_end();

    return o;
}

int sisdb_init(const char *market_)
{
    int o = 0;
    s_sis_dict_entry *de;
    s_sis_dict_iter *di = sis_dict_get_iter(server.db->collects);
    while ((de = sis_dict_next(di)) != NULL)
    {
        s_sisdb_collect *val = (s_sisdb_collect *)sis_dict_getval(de);
        if (val->db->control.isinit && !sis_strncasecmp(sis_dict_getkey(de), market_, strlen(market_)))
        {
            // 只是设置记录数为0
            sisdb_collect_clear(val);
            o++;
        }
    }
    sis_dict_iter_free(di);
    return o;
}
