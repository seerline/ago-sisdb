#include "sis_modules.h"
#include "worker.h"
#include "sis_method.h"

#include "sisdb.h"
#include "sisdb_io.h"
#include "sisdb_collect.h"

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////
// NULL 默认为读取
struct s_sis_method sisdb_methods[] = {
    {"get",   cmd_sisdb_get, NULL, NULL},   // 默认 json 格式
    {"set",   cmd_sisdb_set, "write", NULL},   // 默认 json 格式
    {"del",   cmd_sisdb_del, "write", NULL},   // 删除一个数据 数据区没有数据时 清理键值
    {"drop",  cmd_sisdb_drop, "write", NULL},   // 删除一个表结构数据
    {"gets",  cmd_sisdb_gets, NULL, NULL},   // 默认 json 格式 get 多个key多个sdb数据 暂时不支持
    {"bset",  cmd_sisdb_bset, "write", NULL},   // 默认 二进制 格式
    {"dels",  cmd_sisdb_dels, "write", NULL},   // 删除多个数据
    {"sub",   cmd_sisdb_sub, "subscribe", NULL},   // 订阅数据
    {"unsub", cmd_sisdb_unsub, "unsubscribe", NULL},   // 订阅数据
    {"subs",  cmd_sisdb_subs, "subscribe", NULL},   // 多个订阅数据 *.* 
};
// 共享内存数据库
s_sis_modules sis_modules_sisdb = {
    sisdb_init,
    NULL,
    NULL,
    NULL,
    sisdb_uninit,
    sisdb_method_init,
    sisdb_method_uninit,
    sizeof(sisdb_methods) / sizeof(s_sis_method),
    sisdb_methods,
};

s_sisdb_kv *sisdb_kv_create(uint16 format_, s_sis_sds in_)
{
    s_sisdb_kv *o = SIS_MALLOC(s_sisdb_kv, o);
    o->format = format_;
    o->value = sis_sdsdup(in_);
    return o;
}
void sisdb_kv_destroy(void *info_)
{
    s_sisdb_kv *info = (s_sisdb_kv *)info_;
    sis_sdsfree(info->value);
    sis_free(info);
}

s_sisdb_table *sisdb_table_create(s_sis_json_node *node_)
{
    s_sis_dynamic_db *db = sis_dynamic_db_create(node_);
    if (!db)
    {
        return NULL;
    }
    s_sisdb_table *o = SIS_MALLOC(s_sisdb_table, o);
    o->db = db;
    o->fields = sis_string_list_create_w();
    for (int i = 0; i < sis_map_list_getsize(db->fields); i++)
    {
        s_sis_dynamic_field *field = (s_sis_dynamic_field *)sis_map_list_geti(db->fields, i);
        sis_string_list_push(db->fields, field->name, sis_sdslen(field->name));
    }
    return o;
}
void sisdb_table_destroy(void *table_)
{
    s_sisdb_table *table = (s_sisdb_table *)table_;
    sis_string_list_destroy(table->fields);
    sis_dynamic_db_destroy(table->db);
    sis_free(table);
}

bool sisdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_cxt *context = SIS_MALLOC(s_sisdb_cxt, context);
    worker->context = context;

	context->name = sis_sdsnew(node->key);

    context->kvs = sis_map_pointer_create_v(sisdb_kv_destroy);

	context->sdbs = sis_map_list_create(sisdb_table_destroy);
    {
        s_sis_json_node *node = sis_json_cmp_child_node(node, "tables");
        if (node)
        {
            s_sis_json_node *next = sis_conf_first_node(node);
            while (next)
            {
                s_sisdb_table *table = sisdb_table_create(next);
                sis_map_list_set(context->sdbs, next->key, table);
                next = next->next;
            }
        }
    }

    context->collects = sis_map_pointer_create_v(sisdb_collect_destroy);

    return true;
}
void sisdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;

    sis_map_pointer_destroy(context->collects);

    sis_map_pointer_destroy(context->kvs);
    sis_map_list_destroy(context->sdbs);

    sis_sdsfree(context->name);
    sis_free(context);
}

void sisdb_method_init(void *worker_)
{
    // 加载数据
}
void sisdb_method_uninit(void *worker_)
{
    // 释放数据
}

int cmd_sisdb_get(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    char argv[2][128]; 
    int cmds = sis_str_divide(netmsg->key, '.', argv[0], argv[1]);
    printf("cmd_sisdb_get: %d %s %s \n", cmds, argv[0], argv[1]);
    s_sis_sds o = NULL;

    uint8 format = SISDB_FORMAT_CHARS;
    if (cmds == 1)
    {
        // 单
        o = sisdb_single_get_sds(context, argv[0], &format, netmsg->val);
    }
    else
    {
        o = sisdb_get_sds(context, netmsg->key, &format, netmsg->val);
    }
	if (o)
	{
        if(format == SISDB_FORMAT_CHARS)
        {
            sis_net_ans_with_string(netmsg, o);
        }
        else
        {
            sis_net_ans_with_bytes(netmsg, o);
        }
        
		return SIS_METHOD_OK;
	}
	return SIS_METHOD_ERROR;
}
int cmd_sisdb_set(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    char argv[2][128]; 
    int cmds = sis_str_divide(netmsg->key, '.', argv[0], argv[1]);
    printf("cmd_sisdb_set: %d %s %s \n", cmds, argv[0], argv[1]);
    if (!netmsg->val && sis_sdslen(netmsg->val) > 0)
    {
        return SIS_METHOD_ERROR;
    }
    int o = -1;
    if (cmds == 1)
    {
        // 单
        o = sisdb_single_set(context, argv[0], SISDB_FORMAT_CHARS, netmsg->val);
    }
    else
    {
        o = sisdb_set_chars(context, netmsg->key, netmsg->val);
    }
	if (!o)
	{
        sis_net_ans_with_ok(netmsg);
		return SIS_METHOD_OK;
	}
	return SIS_METHOD_ERROR;
}

int cmd_sisdb_del(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    char argv[2][128]; 
    int cmds = sis_str_divide(netmsg->key, '.', argv[0], argv[1]);
    printf("cmd_sisdb_del: %d %s %s \n", cmds, argv[0], argv[1]);
    int o = 0;
    if (cmds == 1)
    {
        o = sisdb_single_del(context, argv[0], netmsg->val);
    }
    else
    {
        o = sisdb_del(context, netmsg->key, netmsg->val);
    }
	if (!o)
	{
        sis_net_ans_with_ok(netmsg);
		return SIS_METHOD_OK;
	}
	return SIS_METHOD_ERROR;
}
int cmd_sisdb_drop(void *worker_, void *argv_)
{
    return SIS_METHOD_ERROR;
}
// 获取多个key数据 只返回字符串数据
// 并且对数据表仅仅返回最后一条记录
int cmd_sisdb_gets(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    char argv[2][128]; 
    int cmds = sis_str_divide(netmsg->key, '.', argv[0], argv[1]);
    printf("cmd_sisdb_gets: %d %s %s \n", cmds, argv[0], argv[1]);
    s_sis_sds o = NULL;
    if (cmds == 1)
    {
        o = sisdb_single_gets_sds(context, argv[0], netmsg->val);
    }
    else
    {
        o = sisdb_gets_sds(context, argv[0], argv[1], netmsg->val);
    }
	if (o)
	{
        sis_net_ans_with_string(netmsg, o);
		return SIS_METHOD_OK;
	}
	return SIS_METHOD_ERROR;
}
int cmd_sisdb_bset(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    char argv[2][128]; 
    int cmds = sis_str_divide(netmsg->key, '.', argv[0], argv[1]);
    printf("cmd_sisdb_bset: %d %s %s \n", cmds, argv[0], argv[1]);
    if (!netmsg->val && sis_sdslen(netmsg->val) > 0)
    {
        return SIS_METHOD_ERROR;
    }
    int o = -1;
    if (cmds == 1)
    {
        // 单
        o = sisdb_single_set(context, argv[0], SISDB_FORMAT_BYTES, netmsg->val);
    }
    else
    {
        o = sisdb_set_bytes(context, netmsg->key, netmsg->val);
    }
	if (!o)
	{
        sis_net_ans_with_ok(netmsg);
		return SIS_METHOD_OK;
	}
	return SIS_METHOD_ERROR;
}

int cmd_sisdb_dels(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    char argv[2][128]; 
    int cmds = sis_str_divide(netmsg->key, '.', argv[0], argv[1]);
    printf("cmd_sisdb_dels: %d %s %s \n", cmds, argv[0], argv[1]);
    int o = 0;
    if (cmds == 1)
    {
        o = sisdb_single_dels(context, argv[0], netmsg->val);
    }
    else
    {
        o = sisdb_dels(context, argv[0], argv[1], netmsg->val);
    }
	if (!o)
	{
        sis_net_ans_with_ok(netmsg);
		return SIS_METHOD_OK;
	}
	return SIS_METHOD_ERROR;
}

int cmd_sisdb_sub(void *worker_, void *argv_)
{
    return 0;
}
int cmd_sisdb_unsub(void *worker_, void *argv_)
{
    return 0;
}
int cmd_sisdb_subs(void *worker_, void *argv_)
{
    return 0;
}
