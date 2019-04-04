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
    .config = NULL,
    .sysdb = NULL,
    .sisdbs = NULL,
    .workers = NULL};
/********************************/

// void *_thread_save_plan_task(void *argv_)
// {
//     s_sis_db *db = (s_sis_db *)argv_;
//     s_sis_plan_task *task = db->save_task;
//     sis_plan_task_wait_start(task);

//     while (sis_plan_task_working(task))
//     {
//         if (sis_plan_task_execute(task))
//         {
//             sis_mutex_lock(&task->mutex);
//             // --------user option--------- //
//             sisdb_file_save(&server);
//             sis_mutex_unlock(&task->mutex);
//         }
//     }
//     sis_plan_task_wait_stop(task);

//     return NULL;
// }

s_sis_db *sisdb_open_sisdb_one(s_sis_json_node *config_, const char *dbname_)
{
    s_sis_db *db = NULL;
    char cfgname[SIS_PATH_LEN];
    sis_sprintf(cfgname, SIS_PATH_LEN, SIS_DB_FILE_CONF, server.db_path, dbname_);

    if (!sis_file_exists(cfgname))
    {
        s_sis_json_node *service = sis_json_cmp_child_node(config_, dbname_);
        if (!service)
        {
            return NULL;
        }
        db = sisdb_create(dbname_);
        db->conf = sis_json_clone(service, 1);
        
        s_sis_json_node *sformat = sis_json_cmp_child_node(service, "save-format");
        if (sformat)
        {
            db->save_format = sisdb_find_map_uid(
                        server.maps,
                        sis_json_get_str(service, "save-format"),
                        SIS_MAP_DEFINE_DATA_TYPE);
        }
        else
        {
            db->save_format = SIS_DATA_TYPE_STRUCT;
        }
        s_sis_json_node *node = sis_json_cmp_child_node(service, "tables");
        if (node)
        {
            s_sis_json_node *next = sis_conf_first_node(node);
            while (next)
            {
                sisdb_table_create(db, next->key, next);
                next = next->next;
            }
        }
        // 加载默认变量
        node = sis_json_cmp_child_node(service, "values");
        if (node)
        {
            size_t len = 0;
            s_sis_json_node *next = sis_conf_first_node(node);
            while (next)
            {
                char *str = sis_conf_to_json_zip(next, &len);
                sisdb_set(db, SIS_DATA_TYPE_JSON, next->key, str, len);
                sis_free(str);
                next = next->next;
            }
        }
    }
    else
    {
        s_sis_json_handle *config= sis_json_open(cfgname);
        if (!config)
        {
            LOG(1)("load sdb conf %s fail.\n", cfgname);
            return NULL;
        }
        // 检查版本号是否匹配，如果用户数据库版本和程序版本不一致，就需要对磁盘的数据做数据格式转换
        // 或者单独运行一个数据升级程序来处理，升级前需要对数据进行备份，方便回滚
        if (SIS_DB_FILE_VERSION != sis_json_get_int(config->node, "version", SIS_DB_FILE_VERSION))
        {
            LOG(1)("file format ver error.\n");
            return NULL;
        }
        s_sis_json_node *service = config->node;
        db = sisdb_create(dbname_);
        db->conf = sis_json_clone(config->node, 1);

        s_sis_json_node *sformat = sis_json_cmp_child_node(service, "save-format");
        if (sformat)
        {
            db->save_format = sisdb_find_map_uid(
                        server.maps,
                        sis_json_get_str(service, "save-format"),
                        SIS_MAP_DEFINE_DATA_TYPE);
        }
        else
        {
            db->save_format = SIS_DATA_TYPE_STRUCT;
        }

        s_sis_json_node *node = sis_json_cmp_child_node(service, "tables");
        if (node)
        {
            s_sis_json_node *next = sis_conf_first_node(node);
            while (next)
            {
                sisdb_table_create(db, next->key, next);
                next = next->next;
            }
        }
        sis_json_close(config);
    }
    return db;

}
int sisdb_open_sisdbs()
{
    // 先检查是否配置了系统数据集
    server.sysdb = sisdb_open_sisdb_one(server.config->node, "system");
    if (server.sysdb)
    {
        sis_pointer_list_push(server.sisdbs, server.sysdb);
    }

    s_sis_json_node *sisdb = sis_json_cmp_child_node(server.config->node, "sisdb");
    if (sisdb)
    {
        s_sis_json_node *next = sis_conf_first_node(sisdb);
        while (next)
        {
            s_sis_db *db = sisdb_open_sisdb_one(sisdb, next->key);
            sis_pointer_list_push(server.sisdbs, db);
            next = next->next;
        }
    }
    return server.sisdbs->count;
}
int sisdb_open_workers()
{
    s_sis_json_node *workers = sis_json_cmp_child_node(server.config->node, "workers");
	if (workers)
	{
    	s_sis_json_node *next = sis_json_first_node(workers);
		while (next)
		{
            s_sisdb_worker *worker = sisdb_worker_create(next);
			if (worker)
			{
				sis_pointer_list_push(server.workers, worker);
			}
			next = next->next;
		}
	}
    return server.workers->count;
}
// 返回数据集合
s_sis_pointer_list *sisdb_open(const char *conf_)
{
    s_sis_conf_handle *config = sis_conf_open(conf_);
    if (!config)
    {
        printf("load conf %s fail.\n", conf_);
        return NULL;
    }
    server.config = config;
    // 加载可包含的配置文件，方便后面使用

    char current_path[SIS_PATH_LEN];
    //  获取conf文件目录，
    sis_file_getpath(conf_, current_path, SIS_PATH_LEN);

    sis_get_fixed_path(current_path, sis_json_get_str(server.config->node, "dbpath"),
                       server.db_path, SIS_PATH_LEN);

    s_sis_json_node *lognode = sis_json_cmp_child_node(server.config->node, "log");
    if (lognode)
    {
        sis_get_fixed_path(current_path, sis_json_get_str(lognode, "path"),
                           server.log_path, SIS_PATH_LEN);

        server.log_screen = sis_json_get_bool(lognode, "screen", true);
        server.log_level = sis_json_get_int(lognode, "level", 5);
        server.log_size = sis_json_get_int(lognode, "maxsize", 10) * 1024 * 1024;
    }
	// 生成log文件
	char name[SIS_PATH_LEN];
	sis_file_getname(conf_, name, SIS_PATH_LEN);
	size_t len = strlen(name);
	for (int i = (int)len - 1; i > 0; i--)
	{
		if (name[i] == '.')
		{
			name[i] = 0;
			break;
		}
	}
	sis_sprintf(server.logname, SIS_PATH_LEN, "%s%s.log", server.log_path, name);
	// 生成log文件结束

    s_sis_json_node *safenode = sis_json_cmp_child_node(server.config->node, "safe");
    if (safenode)
    {
        sis_get_fixed_path(current_path, sis_json_get_str(safenode, "path"),
                           server.safe_path, SIS_PATH_LEN);

        server.safe_deeply = sis_json_get_int(safenode, "deeply", 0);
        server.safe_lasts = sis_json_get_int(safenode, "last", 10);
    }
    // 基础数据读取完毕，下面启动数据表和服务程序
    // 打开log
	if (!server.log_screen)
	{
		sis_log_open(server.logname, server.log_level, server.log_size);
	}
	else
	{
		sis_log_open(NULL, server.log_level, server.log_size);
	}
    sis_mutex_init(&server.write_mutex, NULL);
	server.maps = sis_map_pointer_create();
	sisdb_init_map_define(server.maps);

	server.methods = sisdb_method_define_create();

	server.sys_infos = sis_pointer_list_create();
	server.sys_infos->free = sis_free_call;
	server.sys_exchs = sis_map_pointer_create();

	// 先建设数据库
	server.sisdbs = sis_pointer_list_create();
	int sdbs = sisdb_open_sisdbs();

    // 初始化和服务启动完毕，这里开始加载数据
    // 应该需要判断数据的版本号，如果不同，应该对磁盘上的数据进行数据字段重新匹配
    // 把老库中有的字段加载到新的库中，再存盘
    for (int i = 0; i < server.sisdbs->count; i++)
    {
        s_sis_db *db = (s_sis_db *)sis_pointer_list_get(server.sisdbs, i);
        if (!sisdb_file_load(db))
        {
            LOG(1)("load sdb fail. [%s]!\n", db->name);
        }        
    }
    // 最后再启动非必要的服务
	server.workers = sis_pointer_list_create();
	int workers = sisdb_open_workers();
    // 启动一些非必要的服务,到这里即便没有服务启动，也认为可以正常工作了，
    LOG(5)("workers = %d. sdbs = %d.\n", workers, sdbs);

    server.status = SIS_SERVER_STATUS_INITED;

    return server.sisdbs;

}

void sisdb_close()
{
    if (server.status == SIS_SERVER_STATUS_CLOSE)
    {
        return;
    }
    // 先停止服务
    server.status = SIS_SERVER_STATUS_CLOSE;
    // 再停止内存服务线程
    if(server.workers)
    {
        for(int i = 0; i < server.workers->count; i++)
        {
            s_sisdb_worker *worker = (s_sisdb_worker *)sis_pointer_list_get(server.workers, i);
            sisdb_worker_destroy(worker);
        }	
        sis_pointer_list_destroy(server.workers);
    }
    // 最后关闭数据集合
    if(server.sisdbs)
    {
        for (int i = 0; i < server.sisdbs->count; i++)
        {
            s_sis_db *db = (s_sis_db *)sis_pointer_list_get(server.sisdbs, i);
            sisdb_destroy(db);
        }
        sis_pointer_list_destroy(server.sisdbs);
    }

    sis_mutex_destroy(&server.write_mutex);
    // 清除其他内存
	// 下面释放股票信息
	sis_pointer_list_destroy(server.sys_infos);
	// 下面释放市场信息
	if (server.sys_exchs)
	{
		s_sis_dict_entry *de;
		s_sis_dict_iter *di = sis_dict_get_iter(server.sys_exchs);
		while ((de = sis_dict_next(di)) != NULL)
		{
			s_sisdb_sys_exch *val = (s_sisdb_sys_exch *)sis_dict_getval(de);
			sis_free(val);
		}
		sis_dict_iter_free(di);
	}
	sis_map_pointer_destroy(server.sys_exchs);

	sis_map_pointer_destroy(server.maps);
    sisdb_method_define_destroy(server.methods);

    sis_conf_close(server.config);
    // 关闭log
    sis_log_close();
    server.status = SIS_SERVER_STATUS_NOINIT;
}

// 所有系统级别的开关设置都在这里
int sisdb_set_switch(const char *key_)
{
    if (!sis_strcasecmp(key_, "output"))
    {
        server.switch_output = !server.switch_output;
        return server.switch_output;
    }
    else if (!sis_strcasecmp(key_, "super"))
    {
        server.switch_super = !server.switch_super;
        return server.switch_super;
    }
    return -1;
}

s_sisdb_server *sisdb_get_server()
{
    return &server;
}
int sisdb_get_server_status()
{
    return server.status;
}
int sisdb_write_lock(s_sis_db *db_)
{
    int o = 0;
    if(db_)
    {
        o = sis_mutex_lock(&db_->write_mutex);
    }
    else
    {
        o = sis_mutex_lock(&server.write_mutex);
    }
    return o;
}
void sisdb_write_unlock(s_sis_db *db_)
{
    if(db_)
    {
        sis_mutex_unlock(&db_->write_mutex);
    }
    else
    {
        sis_mutex_unlock(&server.write_mutex);
    }    
}
//////////-----------/////////////////
bool sisdb_save(const char *dbname_)
{
    bool o = TRUE;
    for (int i = 0; i < server.sisdbs->count; i++)
    {
        s_sis_db *db = sis_pointer_list_get(server.sisdbs, i);
        if (dbname_)
        {
            if (!sis_strcasecmp(db->name, dbname_))
            {
                sisdb_write_lock(db);
                o = sisdb_file_save(db);
                sisdb_write_unlock(db);    
                break;      
            }
        }
        else
        {
            sisdb_write_lock(db);
            o &= sisdb_file_save(db);
            sisdb_write_unlock(db);    
        }
    }
    return o;
}
int sisdb_get_dbs_collects(s_sis_db *db_, const char *com_)
{
    int o = 0;
    if (!com_)
    {
        return o;
    }

    char table[SIS_MAXLEN_TABLE];
    s_sis_dict_entry *de;
    s_sis_dict_iter *di = sis_dict_get_iter(db_->collects);
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
s_sis_sds sisdb_show_dbs_sds(s_sis_db *db_, const char *com_)
{
    s_sis_sds list = sis_sdsempty();
    list = sdscatprintf(list, "%s : \n", db_->name);
    s_sis_dict_entry *de;
    s_sis_dict_iter *di = sis_dict_get_iter(db_->dbs);

    while ((de = sis_dict_next(di)) != NULL)
    {
        s_sisdb_table *val = (s_sisdb_table *)sis_dict_getval(de);
        list = sdscatprintf(list, "\t%-15s : fields=%-3d, len=%-4u, collects=%d\n",
                            val->name,
                            sis_string_list_getsize(val->field_name),
                            sisdb_table_get_fields_size(val),
                            sisdb_get_dbs_collects(db_, val->name));
    }
    return list;
}
s_sis_sds sisdb_show_all_sds()
{
    s_sis_sds list = NULL;
    for (int i = 0; i < server.sisdbs->count; i++)
    {
        s_sis_db *db = sis_pointer_list_get(server.sisdbs, i);
        if (!list)
        {
            list = sis_sdsempty();
        }
        list = sdscatprintf(list, "%s : \n", db->name);
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(db->dbs);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_table *val = (s_sisdb_table *)sis_dict_getval(de);
            list = sdscatprintf(list, "\t%-15s : fields=%-3d, len=%-4u, collects=%d\n",
                                val->name,
                                sis_string_list_getsize(val->field_name),
                                sisdb_table_get_fields_size(val),
                                sisdb_get_dbs_collects(db, val->name));
        }
    }
    return list;
}

s_sis_sds sisdb_show_keys_sds(s_sis_db *db_, const char *com_)
{
    int count = 0;
    s_sis_sds list = NULL;
    s_sis_dict_entry *de;
    s_sis_dict_iter *di = sis_dict_get_iter(db_->collects);
    while ((de = sis_dict_next(di)) != NULL)
    {
        if (!com_ || (com_ && strstr(sis_dict_getkey(de), com_)))
        {
            if (!list)
            {
                list = sis_sdsempty();
            }
            s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
            count++;
            list = sdscatprintf(list, "\t%-15s : len=%-4d, count=%u\n",
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

s_sis_db *sisdb_get_db(const char *dbname_)
{
    s_sis_db *o = NULL;
    for (int i = 0; i < server.sisdbs->count; i++)
    {
        s_sis_db *db = sis_pointer_list_get(server.sisdbs, i);
        if (!sis_strcasecmp(db->name, dbname_))
        {
            o = db;
            break;
        }
    }
    return o;
}
s_sis_db *sisdb_get_db_from_table(const char *tbname_)
{
    s_sis_db *o = NULL;
	for(int i = 0; i < server.sisdbs->count; i++)
	{
		s_sis_db *db = sis_pointer_list_get(server.sisdbs, i);
		s_sisdb_table *tb = sis_map_pointer_get(db->dbs, tbname_);
		if (tb)
		{
            o = db;
            break;
		}
	}    
    return o;
}

s_sis_sds sisdb_show_sds(const char *dbname_, const char *keyname_)
{
    s_sis_sds o = NULL;
    if (!dbname_&&!keyname_)
    {
       o = sisdb_show_all_sds(NULL); 
    }
    else
    {
        if (dbname_)
        {
            s_sis_db *db = sisdb_get_db(dbname_);
            if (db)
            {
                if (keyname_)
                {
                    o = sisdb_show_keys_sds(db, keyname_);
                }
                else
                {
                    o = sisdb_show_dbs_sds(db, keyname_);
                }
            }
        }
    }
    return o;
}

s_sis_sds sisdb_call_sds(const char *key_, const char *com_)
{
    // 先去找本地调用，再去找远程调用，
    s_sis_method *method = sis_method_map_find(server.methods, key_, SISDB_CALL_STYLE_SYSTEM);
    if (method)
    {
        return (s_sis_sds)method->proc(&server, (void *)com_);
    }
    else
    {
        LOG(3)("no find proc %s.\n", key_);
        return NULL;
    }
    return NULL;
}

// 为保证最快速度，尽量不加参数
// 默认返回最后不超过 16K 的数据，以json格式
s_sis_sds sisdb_fast_get_sds(s_sis_db *db_, const char *key_)
{
    return sisdb_collect_fastget_sds(db_, key_);
}
// 带参数的查询，解析参数，会返回相对完整的数据，适用与
// com 中携带的为返回格式和search针对时间定位的查询语句，需要解析
s_sis_sds sisdb_get_sds(s_sis_db *db_, const char *key_, const char *com_)
{
    // printf("-----get %s \n", key_);
    if (key_[0] == '*' && key_[1] == '.') // 要改成从 com 中方法调用
    {
        char tbname[SIS_MAXLEN_TABLE];
        sis_str_substr(tbname, SIS_MAXLEN_TABLE, key_, '.', 1);
        // 获得多只股票某类数据的最后一条记录
        // 根据codes字段来判断都需要哪些股票
        return sisdb_collects_get_last_sds(db_, tbname, com_);
    }
    return sisdb_collect_get_sds(db_, key_, com_);
}

int sisdb_delete_collect_of_table(s_sis_db *db_,const char *tbname_, const char *com_, size_t len_)
{
    s_sis_dict_entry *de;
    int o = 0;
    s_sis_dict_iter *di = sis_dict_get_iter(db_->collects);
    while ((de = sis_dict_next(di)) != NULL)
    {
        s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
        if (collect && !sis_strcasecmp(collect->db->name, tbname_))
        {
            sis_dict_delete(db_->collects, sis_dict_getkey(de));
            sisdb_collect_destroy(collect);
            o++;
        }
    }
    sis_dict_iter_free(di);
    return o;
}

int sisdb_delete_collect_of_code(s_sis_db *db_,const char *code_, const char *com_, size_t len_)
{
    s_sis_dict_entry *de;
    int o = 0;
    s_sis_dict_iter *di = sis_dict_get_iter(db_->collects);
    while ((de = sis_dict_next(di)) != NULL)
    {
        char code[SIS_MAXLEN_CODE];
        sis_str_substr(code, SIS_MAXLEN_CODE, sis_dict_getkey(de), '.', 0);
        if (!sis_strcasecmp(code_, code))
        {
            s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
            sis_dict_delete(db_->collects, sis_dict_getkey(de));
            sisdb_collect_destroy(collect);
            o++;
        }
    }
    sis_dict_iter_free(di);
    return o;
}
int sisdb_delete(s_sis_db *db_, const char *key_, const char *com_, size_t len_)
{
    int o = 0;
    char tbname[SIS_MAXLEN_TABLE];
    char code[SIS_MAXLEN_TABLE];
    sis_str_substr(tbname, SIS_MAXLEN_TABLE, key_, '.', 1);
    sis_str_substr(code, SIS_MAXLEN_TABLE, key_, '.', 0);

    if (code[0] == '*' && !code[1])
    {
        o = sisdb_delete_collect_of_table(db_, tbname, com_, len_);
    }
    else if (tbname[0] == '*' && !tbname[1])
    {
        o = sisdb_delete_collect_of_code(db_, code, com_, len_);
    }
    else
    {
        s_sisdb_collect *collect = sisdb_get_collect(db_, key_);
        if (collect)
        {
            s_sis_sds key = sis_sdsnew(key_);
            sis_dict_delete(db_->collects, key);
            sis_sdsfree(key);
            sisdb_collect_destroy(collect);
            o = 1;
        }
        // s_sisdb_collect *ddd = sisdb_get_collect(db_, key_);
        // printf("del %s %p\n", key_, ddd);
    }

    return o;
}

int sisdb_delete_link(s_sis_db *db_, const char *key_, const char *com_, size_t len_)
{
    if (sisdb_write_begin(db_, SIS_AOF_TYPE_DEL, key_, com_, len_) == SIS_SERVER_REPLY_ERR)
    {
        return SIS_SERVER_REPLY_ERR;
    }
    int o = sisdb_delete(db_, key_, com_, len_);

    sisdb_write_end(db_);

    return o;
}

// 直接拷贝
int sisdb_set(s_sis_db *db_, int fmt_, const char *key_, const char *val_, size_t len_)
{
    printf("-----set %s\n", key_);
    // 如果表不存在就新建一个表格，仅仅对json格式，表格字段按第一次发送的结构
    if (fmt_ == SIS_DATA_TYPE_JSON)
    {
        // 如果是json结构，需要支持如果没有表，自动生成表的功能；
        char tbname[SIS_MAXLEN_TABLE];
        sis_str_substr(tbname, SIS_MAXLEN_TABLE, key_, '.', 1);
        s_sisdb_table *tb = sisdb_get_table(db_, tbname);
        printf("fmt = %d db= %s tb=%p\n",fmt_, tbname, tb);
        if (!tb)
        {
            s_sis_json_node *node = sisdb_table_new_config(val_, len_);
            if (!node)
            {
                LOG(5)("set format error,[%s].\n", val_);
                return SIS_SERVER_REPLY_ERR;
            }
            tb = sisdb_table_create(db_, tbname, node);
            if (tb)
            {
                sisdb_table_set_conf(db_, tbname, node);
            }
            sis_json_delete_node(node);
        }
    }
    
    // 1 . 先把来源数据，转换为 srcdb 的二进制结构数据集合
    s_sisdb_collect *collect = sisdb_get_collect(db_, key_);
    if (!collect)
    {
        collect = sisdb_collect_create(db_, key_);
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
    // sis_out_binary("update 0 ", in, 60);
    // printf("----len=%ld:%d\n", sis_sdslen(in), collect->value->len);

    int o = sisdb_collect_update(collect, in, false);
   
    // printf("----len=%ld:%d\n", sis_sdslen(in), collect->value->len);

    // 这里应该对所有相应订阅者发送数据过去，方便订阅者得到最新的数据
    sisdb_sys_flush_work_time(collect);
    sisdb_sys_check_write(db_, key_, collect);
    
    // 如果是aof加载就需要广播
    if (!db_->loading) // loading = true --> 从磁盘中加载数据
    {
        for(int i = 0; i < db_->subscribes->count; i++)
        {
            // ... 
        }
        // 如果属于磁盘加载就不publish
        char code[SIS_MAXLEN_CODE];
        sis_str_substr(code, SIS_MAXLEN_CODE, key_, '.', 0);
        sisdb_collect_update_publish(collect, in, code);
    }
    sis_sdsfree(in);
    if (o)
    {
        // LOG(5)("set data ok,[%d].\n", o);
        return SIS_SERVER_REPLY_OK;
    }
    return SIS_SERVER_REPLY_ERR;
}

int sisdb_set_json(s_sis_db *db_,const char *key_, const char *val_, size_t len_)
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
        LOG(3)("set data format error.\n");
        return SIS_SERVER_REPLY_ERR;
    }

    if (sisdb_write_begin(db_, type, key_, val_, len_) == SIS_SERVER_REPLY_ERR)
    {
        return SIS_SERVER_REPLY_ERR;
    }

    // 开始写数据
    int o = sisdb_set(db_, fmt, key_, val_, len_);

    sisdb_write_end(db_);

    return o;
}
int sisdb_set_struct(s_sis_db *db_,const char *key_, const char *val_, size_t len_)
{
    if (sisdb_write_begin(db_, SIS_DATA_TYPE_STRUCT, key_, val_, len_) == SIS_SERVER_REPLY_ERR)
    {
        return SIS_SERVER_REPLY_ERR;
    }
    int o = sisdb_set(db_, SIS_DATA_TYPE_STRUCT, key_, val_, len_);
    sisdb_write_end(db_);
    return o;
}

// int sisdb_new(const char *table_, const char *attrs_, size_t len_)
// {
//     s_sis_conf_handle *config = sis_conf_load(attrs_, len_);
//     if (!config)
//     {
//         LOG(1)("parse attrs_fail.\n");
//         return SIS_SERVER_REPLY_ERR;
//     }

//     if (sisdb_write_begin(SIS_AOF_TYPE_CREATE, table_, attrs_, len_) == SIS_SERVER_REPLY_ERR)
//     {
//         return SIS_SERVER_REPLY_ERR;
//     }
//     // 如果保存aof失败就返回错误
//     s_sisdb_table *tb = sisdb_table_create(server.db, table_, config->node);
//     if (tb)
//     {
//         // 更新配置信息，保存时才会有改表的配置
//         sisdb_table_set_conf(server.db, table_, config->node);
//     }

//     sisdb_write_end();

//     sis_conf_close(config);

//     if (!tb)
//         return SIS_SERVER_REPLY_ERR;
//     return SIS_SERVER_REPLY_OK;
// }

// table.scale  1
// table.fields []
//
// int sisdb_update(const char *key_, const char *val_, size_t len_)
// {

//     char db[SIS_MAXLEN_TABLE];
//     sis_str_substr(db, SIS_MAXLEN_TABLE, key_, '.', 0);
//     char attr[SIS_MAXLEN_STRING];
//     sis_str_substr(attr, SIS_MAXLEN_STRING, key_, '.', 1);
//     if (strlen(attr) < 1)
//     {
//         LOG(5)("no attr %s.\n", key_);
//         return SIS_SERVER_REPLY_ERR;
//     }
//     s_sisdb_table *tb = sisdb_get_table(server.db, db);
//     if (!tb)
//     {
//         LOG(5)("no find table %s.\n", db);
//         return SIS_SERVER_REPLY_ERR;
//     }
//     // 只要修改了就会重新save所以不需要写aof文件
//     sis_mutex_lock(&server.db->save_task->mutex);
//     // 修改表结构一定要重新存盘，
//     int o = sisdb_table_update(server.db, tb, attr, val_, len_);
//     // -1 返回错误 0 什么也没有修改 1 表示修改了表结构，需要重新存盘
//     if (o == 1)
//     {
//         sisdb_file_save(&server);
//     }
//     sis_mutex_unlock(&server.db->save_task->mutex);

//     return o;
// }

// void _printf_info()
// {

//     char key[SIS_MAXLEN_KEY];

//     sis_sprintf(key, SIS_MAXLEN_KEY, "SH.%s", SIS_TABLE_EXCH);
//     s_sisdb_collect *collect = sisdb_get_collect(server.db, key);
//     if (!collect)
//     {
//         printf("no find %s\n",key);
//     }
//     printf("SH === %p  %s\n", collect->sys_exch, collect->sys_exch->market);

//     sis_sprintf(key, SIS_MAXLEN_KEY, "SZ.%s", SIS_TABLE_EXCH);
//     collect = sisdb_get_collect(server.db, key);
//     if (!collect)
//     {
//         printf("no find %s\n",key);
//     }
//     printf("SZ === %p  %s\n", collect->sys_exch, collect->sys_exch->market);

// 		s_sis_dict_entry *de;
// 		s_sis_dict_iter *di = sis_dict_get_iter(server.db->sys_exchs);
// 		while ((de = sis_dict_next(di)) != NULL)
// 		{
// 			s_sisdb_sys_exch *val = (s_sisdb_sys_exch *)sis_dict_getval(de);
//             printf("%s === %p  %s\n", val->market, val, (char *)sis_dict_getkey(de));

// 		}
// 		sis_dict_iter_free(di);
// }
// char ddd_key[255];
// int ddd = 0;
int sisdb_write_begin(s_sis_db *db_, int type_, const char *key_, const char *val_, size_t len_)
{
    if (!sisdb_file_save_aof(db_, type_, key_, val_, len_))
    {
        LOG(3)("save aof error.\n");
        return SIS_SERVER_REPLY_ERR;
    }
    // int o = sisdb_write_lock(NULL);
    // db的锁有问题
    printf("lock : %p\n",db_);
    int o = sisdb_write_lock(db_);

    // int o = sis_mutex_trylock(&db_->write_mutex);
    // int o = sis_mutex_trylock(&server.db->save_task->mutex);
    // sis_mutex_trylock 有问题，
    if (o)
    {
        // EAGAIN EINVAL EBUSY
        // == 0 才是锁住
        // sis_mutex_unlock(&server.db->save_task->mutex);
        LOG(3)("saveing... set fail.[%s] %d\n", key_, o);
        return SIS_SERVER_REPLY_ERR;
    };

    return SIS_SERVER_REPLY_OK;
}
void sisdb_write_end(s_sis_db *db_)
{
    // LOG(8)("..... unlock.\n");
    // sis_mutex_unlock(&db_->write_mutex);
    printf("unlock : %p\n",db_);
    sisdb_write_unlock(db_);
    // sisdb_write_unlock(NULL);
    // ddd--;
}
