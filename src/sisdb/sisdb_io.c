
#include "sis_thread.h"
#include "sisdb_io.h"
#include "sisdb_file.h"
#include "sisdb_map.h"
#include "sisdb_fields.h"
/********************************/
// 一定要用static定义，不然内存混乱
static s_sisdb_server server = {
    .status = SIS_SERVER_STATUS_NOINIT,
    .db = NULL};
/********************************/

void *_thread_save_plan_task(void *argv_)
{
    s_sis_db *db = (s_sis_db *)argv_;
    s_sis_plan_task *task = db->save_task;

    while (sis_plan_task_working(task))
    {
        if (sis_plan_task_execute(task))
        {
            sis_mutex_lock(&task->mutex);
            // --------user option--------- //
            sisdb_file_save(&server);
            sis_mutex_unlock(&task->mutex);
        }
    }
    return NULL;
}

void *_thread_init_plan_task(void *argv_)
{
    s_sis_db *db = (s_sis_db *)argv_;
    s_sis_plan_task *task = db->init_task;

    while (sis_plan_task_working(task))
    {
        if (sis_plan_task_execute(task))
        {
            sisdb_market_work_init(db);
            // 需要检查所有市场的状态和初始化信息等信息
        }
    }
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

    server.db->init_task->work_mode = SIS_WORK_MODE_GAPS;
    server.db->init_task->work_gap.start = 0;
    server.db->init_task->work_gap.stop = 0;
    server.db->init_task->work_gap.delay = 30;

    server.db->save_task->work_mode = SIS_WORK_MODE_NONE;
    s_sis_json_node *stime = sis_json_cmp_child_node(service, "save-time");
    if (stime)
    {
        s_sis_json_node *ptime = sis_json_cmp_child_node(stime, "plans-work");
        if (ptime)
        {
            server.db->save_task->work_mode = SIS_WORK_MODE_PLANS;
            int count = sis_json_get_size(ptime);
            for (int k = 0; k < count; k++)
            {
                uint16 min = sis_array_get_int(ptime, k, 0);
                sis_struct_list_push(server.db->save_task->work_plans, &min);
            }
        }
        s_sis_json_node *atime = sis_json_cmp_child_node(stime, "always-work");
        if (atime)
        {
            server.db->save_task->work_mode = SIS_WORK_MODE_GAPS;
            server.db->save_task->work_gap.start = sis_json_get_int(atime, "start", 900);
            server.db->save_task->work_gap.stop = sis_json_get_int(atime, "stop", 1530);
            server.db->save_task->work_gap.delay = sis_json_get_int(atime, "delay", 300);
        }
    }

    s_sis_json_node *format = sis_json_cmp_child_node(service, "save-format");
    if (format)
    {
        server.db->save_format = sisdb_find_map_uid(server.db->map,
                                                    sis_json_get_str(service, "save-format"),
                                                    SIS_MAP_DEFINE_DATA_TYPE);
    }
    else
    {
        server.db->save_format = SIS_DATA_TYPE_STRUCT;
    }

    // 启动存盘线程
    // 检查数据库文件有没有
    char sdb_json_name[SIS_PATH_LEN];
    sis_sprintf(sdb_json_name, SIS_PATH_LEN, SIS_DB_FILE_CONF, server.dbpath, server.service_name);

    if (!sis_file_exists(sdb_json_name))
    {
        // 加载conf
        size_t len = 0;
        char *str = sis_conf_to_json(service, &len);
        server.db->conf = sdsnewlen(str, len);
        sis_free(str);

        s_sis_json_node *node = sis_json_cmp_child_node(service, "tables");
        s_sis_json_node *next = sis_conf_first_node(node);
        while (next)
        {
            sisdb_table_create(server.db, next->key, next);
            next = next->next;
        }
        // 仅仅没有save前取值
        // 加载默认变量
        node = sis_json_cmp_child_node(service, "values");
        next = sis_conf_first_node(node);
        while (next)
        {
            char *str = sis_conf_to_json_zip(next, &len);
            sisdb_set(SIS_DATA_TYPE_JSON, next->key, str, len);
            sis_free(str);
            next = next->next;
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

        s_sis_json_node *next = sis_conf_first_node(node);
        while (next)
        {
            sisdb_table_create(server.db, next->key, next);
            next = next->next;
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
    if (server.db->save_task->work_mode != SIS_WORK_MODE_NONE)
    {
        if (!sis_plan_task_start(server.db->save_task, _thread_save_plan_task, server.db))
        {
            sis_out_log(1)("can't start save thread\n");
            goto error;
        }
    }
    // 启动存盘线程
    if (server.db->init_task->work_mode != SIS_WORK_MODE_NONE)
    {
        if (!sis_plan_task_start(server.db->init_task,
                                 _thread_init_plan_task, server.db))
        {
            sis_out_log(1)("can't start init thread\n");
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
    sis_mutex_lock(&server.db->save_task->mutex);
    bool o = sisdb_file_save(&server);
    sis_mutex_unlock(&server.db->save_task->mutex);
    return o;
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
            list = sdscatprintf(list, "  %-10s : fields=%2d, len=%u\n",
                                val->name,
                                sis_string_list_getsize(val->field_name),
                                sisdb_table_get_fields_size(val));
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
    list = sdscatprintf(list, "  %-20s : len=%2d, count=%u\n",
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
    if (sis_mutex_trylock(&server.db->save_task->mutex))
    {
        // == 0 才是锁住
        sis_out_log(3)("saveing... set fail.\n");
        return SIS_SERVER_REPLY_ERR;
    };
    return SIS_SERVER_REPLY_OK;
}
void _sisdb_write_end()
{
    sis_mutex_unlock(&server.db->save_task->mutex);
}

void _sisdb_write_work_time(s_sisdb_collect *collect_)
{
    s_sisdb_cfg_exch *exch = collect_->cfg_exch;
    if (!exch)
    {
        return;
    }
    if (exch->status!=SIS_MARKET_STATUS_INITING) 
    {
        return ;
    }
    if (!collect_->db->control.isinit) 
    {
        // 来源数据并不需要初始化
        return ;
    }    
    s_sis_sds buffer = sisdb_collect_get_of_range_sds(collect_, 0, -1);
    if (!buffer)
    {
        return;
    }
    exch->init_time = sisdb_field_get_uint_from_key(collect_->db, "time", buffer);
    sis_free(buffer);
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

    _sisdb_write_work_time(collect);
    sisdb_write_config(server.db, key_, collect);

    if (!server.db->loading)
    {
        // 如果属于磁盘加载就不publish
        char code[SIS_MAXLEN_CODE];
        sis_str_substr(code, SIS_MAXLEN_CODE, key_, '.', 0);
        sisdb_collect_update_publish(collect, in, code);
    }
    sis_sdsfree(in);
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
        if (val->db->control.isinit && !val->db->control.issys &&
            !sis_strncasecmp(sis_dict_getkey(de), market_, strlen(market_)))
        {
            // 只是设置记录数为0
            sisdb_collect_clear(val);
            o++;
        }
    }
    sis_dict_iter_free(di);
    return o;
}

void _sisdb_market_set_status(s_sisdb_collect *collect_, const char *market, int status)
{
    // s_sisdb_cfg_exch *exch = sis_map_buffer_get(collect_->db->father->cfg_exchs, market);
    s_sisdb_cfg_exch *exch = collect_->cfg_exch;
    if (!exch)
    {
        return;
    }
    exch->status = status;
    s_sis_sds buffer = sisdb_collect_get_of_range_sds(collect_, 0, -1);
    if (!buffer)
    {
        return;
    }
    switch (status)
    {
    case SIS_MARKET_STATUS_INITING:
        exch->init_time = 0;
        // 即便是美国，获取的日期也是上一交易日的，
        exch->init_date = sisdb_field_get_uint_from_key(collect_->db, "trade-date", buffer);
        // 接收到now数据后就对new_time赋值，当发现new_time 日期大于交易日期就可以判定需要初始化
        break;
    case SIS_MARKET_STATUS_INITED:
        sisdb_field_set_uint_from_key(collect_->db, "version", buffer, sis_time_get_now());
        // sisdb_field_set_uint_from_key(collect_->db, "trade-date", buffer, sis_time_get_idate(0));
        sisdb_field_set_uint_from_key(collect_->db, "trade-date", buffer, sis_time_get_idate(exch->init_time));
        
        break;
    case SIS_MARKET_STATUS_CLOSE:
        // sisdb_field_set_uint_from_key(collect_->db, "close-date", buffer, sis_time_get_idate(0));
        sisdb_field_set_uint_from_key(collect_->db, "close-date", buffer, sis_time_get_idate(exch->init_time));
        break;
    default:
        break;
    }
    sisdb_field_set_uint_from_key(collect_->db, "status", buffer, status);
    sisdb_collect_update(collect_, buffer);

    sis_free(buffer);
}



bool _sisdb_market_start_init(s_sisdb_collect *collect_,  const char *market_)
{
    // s_sisdb_cfg_exch *exch = sis_map_buffer_get(db_->cfg_exchs, market);
    s_sisdb_cfg_exch *exch = collect_->cfg_exch;
    if (!exch)
    {
        return false;
    }
    if (exch->status != SIS_MARKET_STATUS_INITING) 
    {
        return false;
    }
    if (exch->init_time == 0)
    {
        return false;
    }
    int date = sis_time_get_idate(exch->init_time);
    if (date > exch->init_date) 
    {
        return true;
    }
    return false;
}

void sisdb_market_work_init(s_sis_db *db_)
{
    s_sisdb_table *tb = sisdb_get_table(db_, SIS_TABLE_EXCH);
    int count = sis_string_list_getsize(tb->collects);

    char key[SIS_MAXLEN_KEY];
    s_sisdb_cfg_exch exch;

    int min = sis_time_get_iminute(0);

    for (int k = 0; k < count; k++)
    {
        const char *market = sis_string_list_get(tb->collects, k);
        sis_sprintf(key, SIS_MAXLEN_KEY, "%s.%s", market, SIS_TABLE_EXCH);
        s_sisdb_collect *collect = sisdb_get_collect(db_, key);
        if (!collect)
            continue;

        if (!sisdb_collect_load_exch(collect, &exch))
            continue;

        int status = exch.status;
        if (exch.work_time.first == exch.work_time.second)
        {
            if (exch.work_time.first == min)
            {
                if (status != SIS_MARKET_STATUS_INITING)
                {
                    _sisdb_market_set_status(collect, market, SIS_MARKET_STATUS_CLOSE);
                    _sisdb_market_set_status(collect, market, SIS_MARKET_STATUS_INITING);
                }
                else
                {
                    // 需要等待第一个有行情的股票来了数据才初始化完成
                    if (_sisdb_market_start_init(collect, market))
                    {
                        sisdb_init(market);
                        _sisdb_market_set_status(collect, market, SIS_MARKET_STATUS_INITED);
                    }
                }
            }
        }
        else
        {
            if ((exch.work_time.first < exch.work_time.second && min > exch.work_time.first && min < exch.work_time.second) ||
                (exch.work_time.first > exch.work_time.second && (min > exch.work_time.first || min < exch.work_time.second)))
            {
                if (status == SIS_MARKET_STATUS_NOINIT || status == SIS_MARKET_STATUS_CLOSE)
                {
                    _sisdb_market_set_status(collect, market, SIS_MARKET_STATUS_INITING);
                }
                else if (status == SIS_MARKET_STATUS_INITING)
                {
                    // 需要等待第一个有行情的股票来了数据才初始化完成
                    if (_sisdb_market_start_init(collect, market))
                    {
                        sisdb_init(market);
                        _sisdb_market_set_status(collect, market, SIS_MARKET_STATUS_INITED);
                    }
                }
            }
            else
            {
                if (status == SIS_MARKET_STATUS_INITED)
                {
                    _sisdb_market_set_status(collect, market, SIS_MARKET_STATUS_CLOSE);
                }
            }
        }
    }
}

