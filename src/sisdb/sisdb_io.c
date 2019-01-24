//******************************************************
// Copyright (C) 2018, coollyer <seerlinecoin@gmail.com>
//*******************************************************

#include "sisdb_io.h"
#include "sisdb_file.h"
#include "sisdb_map.h"
#include "sisdb_fields.h"
#include "sisdb_call.h"
#include "sisdb_sys.h"
#include "sisdb_method.h"
/********************************/
// 一定要用static定义，不然内存混乱
static s_sisdb_server server = {
    .switch_output = false,
    .switch_super = false,
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
                       server.db_path, SIS_PATH_LEN);
    // sis_path_complete(server.db_path,SIS_PATH_LEN);

    s_sis_json_node *lognode = sis_json_cmp_child_node(config->node, "log");
    if (lognode)
    {
        sis_get_fixed_path(config_path, sis_json_get_str(lognode, "path"),
                           server.log_path, SIS_PATH_LEN);

        server.log_level = sis_json_get_int(lognode, "level", 5);
        server.log_size = sis_json_get_int(lognode, "maxsize", 10) * 1024 * 1024;
    }

    s_sis_json_node *safenode = sis_json_cmp_child_node(config->node, "safe");
    if (safenode)
    {
        sis_get_fixed_path(config_path, sis_json_get_str(safenode, "path"),
                           server.safe_path, SIS_PATH_LEN);

        server.safe_deeply = sis_json_get_int(safenode, "deeply", 0);
        server.safe_lasts = sis_json_get_int(safenode, "last", 10);
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
    sis_sprintf(sdb_json_name, SIS_PATH_LEN, SIS_DB_FILE_CONF, server.db_path, server.service_name);

    if (!sis_file_exists(sdb_json_name))
    {
        // 备份conf
        server.db->conf = sis_json_clone(service, 1);
        server.db->special = false;
        s_sis_json_node *node = sis_json_cmp_child_node(service, "system");
        if (node) 
        {
            server.db->special = true;
            s_sis_json_node *next = sis_conf_first_node(node);
            while (next)
            {
                s_sisdb_table *tb = sisdb_table_create(server.db, next->key, next);
                tb->control.issys = 1;
                sisdb_sys_load_default(server.db, next);
                next = next->next;
            }
        }
        node = sis_json_cmp_child_node(service, "tables");
        if (node) 
        {
            s_sis_json_node *next = sis_conf_first_node(node);
            while (next)
            {
                sisdb_table_create(server.db, next->key, next);
                next = next->next;
            }
        }
        // 仅仅没有save前取值
        // 加载默认变量
        node = sis_json_cmp_child_node(service, "values");
        if (node) 
        {
            size_t len = 0;
            s_sis_json_node *next = sis_conf_first_node(node);
            while (next)
            {
                char *str = sis_conf_to_json_zip(next, &len);
                sisdb_set(SIS_DATA_TYPE_JSON, next->key, str, len);
                sis_free(str);
                next = next->next;
            }
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
        // 检查版本号是否匹配，如果用户数据库版本和程序版本不一致，就需要对磁盘的数据做数据格式转换
        // 或者单独运行一个数据升级程序来处理，升级前需要对数据进行备份，方便回滚
        if (SIS_DB_FILE_VERSION != sis_json_get_int(sdb_json->node, "version", 0))
        {
            sis_out_log(1)("file format ver error.\n");
            goto error;
        }
        // 加载conf
        server.db->conf = sis_json_clone(sdb_json->node, 1);
        // 创建数据表
        server.db->special = false;
        s_sis_json_node *node = sis_json_cmp_child_node(sdb_json->node, "system");
        if (node)
        {
            server.db->special = true;
            s_sis_json_node *next = sis_conf_first_node(node);
            while (next)
            {
                s_sisdb_table *tb = sisdb_table_create(server.db, next->key, next);
                tb->control.issys = 1;
                sisdb_sys_load_default(server.db, next);
                next = next->next;
            }
        }
        /////
        node = sis_json_cmp_child_node(sdb_json->node, "tables");
        if (node)
        {
            s_sis_json_node *next = sis_conf_first_node(node);
            while (next)
            {
                sisdb_table_create(server.db, next->key, next);
                next = next->next;
            }
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
    if(server.db->special)
    {
        server.db->init_task->work_mode = SIS_WORK_MODE_GAPS;
        server.db->init_task->work_gap.start = 0;
        server.db->init_task->work_gap.stop = 0;
        server.db->init_task->work_gap.delay = 10;
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

s_sisdb_server *sisdb_get_server()
{
    return &server;
}
int sisdb_get_server_status()
{
    return server.status;
}
bool sisdb_save()
{
    sis_mutex_lock(&server.db->save_task->mutex);
    bool o = sisdb_file_save(&server);
    sis_mutex_unlock(&server.db->save_task->mutex);
    return o;
}
int sisdb_get_dbs_collects(const char *com_)
{
    int o= 0;
    if (!com_) return o;
 
    char table[SIS_MAXLEN_TABLE];
    s_sis_dict_entry *de;
    s_sis_dict_iter *di = sis_dict_get_iter(server.db->collects);
    while ((de = sis_dict_next(di)) != NULL)
    {
        sis_str_substr(table, SIS_MAXLEN_TABLE, sis_dict_getkey(de), '.', 1);
        if (!sis_strcasecmp(com_, table))
        {
            o++;
        }
    }
    sis_dict_iter_free(di);
    return o;
}
s_sis_sds sisdb_show_dbs_sds(const char *com_)
{
    s_sis_sds list = NULL;
    if (server.db->dbs)
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(server.db->dbs);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_table *val = (s_sisdb_table *)sis_dict_getval(de);
            if (!list) list = sis_sdsempty();
            if (val->control.issys) 
            {
                list = sdscat(list, "[sys] ");
            } else {
                list = sdscat(list, "[usr] ");
            }
            list = sdscatprintf(list, "%-15s : fields=%3d, len=%3u, collects=%d\n",
                                val->name,
                                sis_string_list_getsize(val->field_name),
                                sisdb_table_get_fields_size(val),
                                sisdb_get_dbs_collects(val->name));
        }
    }
    return list;
}
s_sis_sds sisdb_show_keys_sds(const char *com_)
{
    int count = 0;
    s_sis_sds list = NULL;
    s_sis_dict_entry *de;
    s_sis_dict_iter *di = sis_dict_get_iter(server.db->collects);
    while ((de = sis_dict_next(di)) != NULL)
    {
        if (!com_||(com_&&strstr(sis_dict_getkey(de), com_)))
        {
            if (!list) list = sis_sdsempty();
            s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
            count++;
            list = sdscatprintf(list, " %-15s : len=%2d, count=%u\n",
                        (char *)sis_dict_getkey(de),
                        collect->value->len,
                        sisdb_collect_recs(collect));
            
        }
        if (count >= SISDB_MAX_ONE_OUTS) 
        {
            list = sdscat(list, " ...(more)...");
            break;
        }
    }
    sis_dict_iter_free(di);
    return list;       
}
s_sis_sds sisdb_show_sds(const char *key_, const char *com_)
{
    s_sis_sds o = NULL;
    if (!sis_strcasecmp(key_,"keys"))
    {
        o = sisdb_show_keys_sds(com_);
    } else  if (!sis_strcasecmp(key_, "dbs"))
    {
        o = sisdb_show_dbs_sds(com_);
    }
	else 
    {
		o = sisdb_show_dbs_sds(NULL);
	}  
    return o;  
}

s_sis_sds sisdb_call_sds(const char *key_, const char *com_)
{
    // 先去找本地调用，再去找远程调用，
    s_sis_method *method = sis_method_map_find(server.db->methods, key_, SISDB_CALL_STYLE_SYSTEM);
    if (method)
    {
        return (s_sis_sds)method->proc(server.db, (void *)com_);
    }
    else 
    {
        sis_out_log(3)("no find %s proc.\n", key_);
        return NULL;       
    }
    return NULL;
}

s_sis_sds sisdb_fast_get_sds(const char *key_)
{
    return sisdb_collect_fastget_sds(server.db, key_);
}
// 为保证最快速度，尽量不加参数
// 默认返回最后不超过8K的数据，以json格式
// com 中携带的为返回格式和search针对时间定位的查询语句，需要解析
s_sis_sds sisdb_get_sds(const char *key_, const char *com_)
{
    if (key_[0] == '*'&&key_[1] == '.') // ??? 要改成从com中方法调用
    {
        char db[SIS_MAXLEN_TABLE];
        sis_str_substr(db, SIS_MAXLEN_TABLE, key_, '.', 1);
        // 获得多只股票某类数据的最后一条记录
        // 根据codes字段来判断都需要哪些股票
        return sisdb_collects_get_last_sds(server.db, db, com_);
    }
    return sisdb_collect_get_sds(server.db, key_, com_);
}
// 所有系统级别的开关设置都在这里
int sisdb_cfg_option(const char *key_)
{
    if (!sis_strcasecmp(key_,"output"))
    {
        server.switch_output = !server.switch_output;
        return server.switch_output;
    } else  if (!sis_strcasecmp(key_, "super"))
    {
        server.switch_super = !server.switch_super;
        return server.switch_super;
    }
    return -1;
}

int sisdb_delete_collect_of_table(const char *dbname_, const char *com_, size_t len_)
{
    s_sis_dict_entry *de;
    int o = 0 ;
    s_sis_dict_iter *di = sis_dict_get_iter(server.db->collects);
    while ((de = sis_dict_next(di)) != NULL)
    {
        s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
        if (collect&&!sis_strcasecmp(collect->db->name, dbname_))
        {
            sisdb_collect_destroy(collect);
            sis_dict_delete(server.db->collects, sis_dict_getkey(de));
            o ++;
        }
    }
    sis_dict_iter_free(di);
    return o;
}

int sisdb_delete_collect_of_code(const char *code_, const char *com_, size_t len_)
{
    s_sis_dict_entry *de;
    int o = 0 ;
    s_sis_dict_iter *di = sis_dict_get_iter(server.db->collects);
    while ((de = sis_dict_next(di)) != NULL)
    {
        char code[SIS_MAXLEN_TABLE];
        sis_str_substr(code, SIS_MAXLEN_TABLE, sis_dict_getkey(de), '.', 0);
        if (!sis_strcasecmp(code_, code))
        {
            s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
            sisdb_collect_destroy(collect);
            sis_dict_delete(server.db->collects, sis_dict_getkey(de));
            o++;
        }
    }
    sis_dict_iter_free(di);
    return o;
}
int sisdb_delete(const char *key_, const char *com_, size_t len_)
{
    int o = 0 ;
    char db[SIS_MAXLEN_TABLE];
    char code[SIS_MAXLEN_TABLE];
    sis_str_substr(db, SIS_MAXLEN_TABLE, key_, '.', 1);
    sis_str_substr(code, SIS_MAXLEN_TABLE, key_, '.', 0);

    if (code[0] == '*'&&!code[1])
    {
        o = sisdb_delete_collect_of_table(db, com_, len_);
    } else if (db[0] == '*'&&!db[1])
    {
        o = sisdb_delete_collect_of_code(code, com_, len_);
    } else{
        s_sisdb_collect *collect = sisdb_get_collect(server.db, key_);
        if (collect)
        {
            sisdb_collect_destroy(collect);
            sis_dict_delete(server.db->collects, key_);
            o = 1;
        }        
    }

    return o;
}

int sisdb_del(const char *key_, const char *com_, size_t len_)
{

    if (sisdb_write_begin(SIS_AOF_TYPE_DEL, key_, com_, len_) == SIS_SERVER_REPLY_ERR)
    {
        return SIS_SERVER_REPLY_ERR;
    }
    int o = sisdb_delete(key_, com_, len_);

    sisdb_write_end();

    return o;
}

// 直接拷贝
int sisdb_set(int fmt_, const char *key_, const char *val_, size_t len_)
{
    // 如果表不存在就新建一个表格，仅仅对json格式，表格字段按第一次发送的结构
    if (fmt_ == SIS_DATA_TYPE_JSON)
    {       
        // 如果是json结构，需要支持如果没有表，自动生成表的功能；
        char db[SIS_MAXLEN_TABLE];
        sis_str_substr(db, SIS_MAXLEN_TABLE, key_, '.', 1);
        s_sisdb_table *tb = sisdb_get_table(server.db, db);
        if (!tb)
        { 
            // printf("fmt = %d db= %s \n",fmt_, db);
            s_sis_json_node *node = sisdb_table_new_config(val_, len_);
            if(!node) 
            {
                sis_out_log(5)("set format error,[%s].\n", val_);
                return SIS_SERVER_REPLY_ERR;
            }
            tb = sisdb_table_create(server.db, db, node);
            if(tb)
            {
                sisdb_table_set_conf(server.db, db, node);
            }
            sis_json_delete_node(node); 
        }
    }
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
    // printf("----len=%ld:%d\n", sis_sdslen(in), collect->value->len);

    int o = sisdb_collect_update(collect, in, false);

    sisdb_sys_flush_work_time(collect);
    sisdb_sys_check_write(server.db, key_, collect);

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
        // sis_out_log(5)("set data ok,[%d].\n", o);
        return SIS_SERVER_REPLY_OK;
    }
    return SIS_SERVER_REPLY_ERR;
}

int sisdb_set_json(const char *key_, const char *val_, size_t len_)
{

    int fmt = SIS_DATA_TYPE_JSON;
    int type = SIS_AOF_TYPE_JSET;
    // 先判断是json or array
    if (val_[0] == '{')
    {
        fmt = SIS_DATA_TYPE_JSON;
        type = SIS_AOF_TYPE_JSET;
    }
    else if (val_[0] == '[')
    {
        fmt = SIS_DATA_TYPE_ARRAY;
        type = SIS_AOF_TYPE_ASET;
    }
    else
    {
        sis_out_log(3)("set data format error.\n");
        return SIS_SERVER_REPLY_ERR;
    }

    if (sisdb_write_begin(type, key_, val_, len_) == SIS_SERVER_REPLY_ERR)
    {
        return SIS_SERVER_REPLY_ERR;
    }

    // 开始写数据
    int o = sisdb_set(fmt, key_, val_, len_);

    sisdb_write_end();

    return o;
}
int sisdb_set_struct(const char *key_, const char *val_, size_t len_)
{
    if (sisdb_write_begin(SIS_AOF_TYPE_SSET, key_, val_, len_) == SIS_SERVER_REPLY_ERR)
    {
        return SIS_SERVER_REPLY_ERR;
    }
    // 如果保存aof失败就返回错误
    int o = sisdb_set(SIS_DATA_TYPE_STRUCT, key_, val_, len_);

    sisdb_write_end();

    return o;
}

int sisdb_new(const char *table_, const char *attrs_, size_t len_)
{
    s_sis_conf_handle *config = sis_conf_load(attrs_, len_);
    if (!config)
    {
        sis_out_log(1)("parse attrs_fail.\n");
        return SIS_SERVER_REPLY_ERR;
    }    

    if (sisdb_write_begin(SIS_AOF_TYPE_CREATE, table_, attrs_, len_) == SIS_SERVER_REPLY_ERR)
    {
        return SIS_SERVER_REPLY_ERR;
    }
    // 如果保存aof失败就返回错误
    s_sisdb_table *tb = sisdb_table_create(server.db, table_, config->node);
    if (tb)
    {
        // 更新配置信息，保存时才会有改表的配置
        sisdb_table_set_conf(server.db, table_, config->node);
    }

    sisdb_write_end();
    
    sis_conf_close(config);

    if (!tb) return SIS_SERVER_REPLY_ERR;
    return SIS_SERVER_REPLY_OK;

}

// table.scale  1
// table.fields []
// 
int sisdb_update(const char *key_, const char *val_, size_t len_)
{

    char db[SIS_MAXLEN_TABLE];
    sis_str_substr(db, SIS_MAXLEN_TABLE, key_, '.', 0);
    char attr[SIS_MAXLEN_STRING];
    sis_str_substr(attr, SIS_MAXLEN_STRING, key_, '.', 1);
    if (strlen(attr) < 1)
    {
        sis_out_log(5)("no attr %s.\n", key_);
        return SIS_SERVER_REPLY_ERR;          
    }
    s_sisdb_table *tb = sisdb_get_table(server.db, db);
    if (!tb)
    {
        sis_out_log(5)("no find table %s.\n", db);
        return SIS_SERVER_REPLY_ERR;        
    }
    // 只要修改了就会重新save所以不需要写aof文件
    sis_mutex_lock(&server.db->save_task->mutex);
    // 修改表结构一定要重新存盘，
    int o = sisdb_table_update(server.db, tb, attr, val_, len_);
    // -1 返回错误 0 什么也没有修改 1 表示修改了表结构，需要重新存盘 
    if( o == 1)
    {
        sisdb_file_save(&server);
    }
    sis_mutex_unlock(&server.db->save_task->mutex);
    
    return o;
}

// void _printf_info()
// {

//     char key[SIS_MAXLEN_KEY];

//     sis_sprintf(key, SIS_MAXLEN_KEY, "SH.%s", SIS_TABLE_EXCH);
//     s_sisdb_collect *collect = sisdb_get_collect(server.db, key);
//     if (!collect)
//     {
//         printf("no find %s\n",key);
//     }    
//     printf("SH === %p  %s\n", collect->spec_exch, collect->spec_exch->market);

//     sis_sprintf(key, SIS_MAXLEN_KEY, "SZ.%s", SIS_TABLE_EXCH);
//     collect = sisdb_get_collect(server.db, key);
//     if (!collect)
//     {
//         printf("no find %s\n",key);
//     }    
//     printf("SZ === %p  %s\n", collect->spec_exch, collect->spec_exch->market);

// 		s_sis_dict_entry *de;
// 		s_sis_dict_iter *di = sis_dict_get_iter(server.db->sys_exchs);
// 		while ((de = sis_dict_next(di)) != NULL)
// 		{
// 			s_sisdb_sys_exch *val = (s_sisdb_sys_exch *)sis_dict_getval(de);
//             printf("%s === %p  %s\n", val->market, val, (char *)sis_dict_getkey(de));
			
// 		}
// 		sis_dict_iter_free(di);
// }

int sisdb_write_begin(int type_, const char *key_, const char *val_, size_t len_)
{
    if (!sisdb_file_save_aof(&server, type_, key_, val_, len_))
    {
        sis_out_log(3)("save aof error.\n");
        return SIS_SERVER_REPLY_ERR;
    }
    if (sis_mutex_trylock(&server.db->save_task->mutex))
    {
        // == 0 才是锁住
        sis_out_log(3)("saveing... set fail.[%s]\n", key_);
        return SIS_SERVER_REPLY_ERR;
    };
    return SIS_SERVER_REPLY_OK;
}
void sisdb_write_end()
{
    sis_mutex_unlock(&server.db->save_task->mutex);
}
