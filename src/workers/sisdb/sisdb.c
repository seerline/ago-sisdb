#include "sis_modules.h"
#include "worker.h"
#include "sis_method.h"

#include "sisdb.h"
#include "sisdb_io.h"
#include "sis_net.io.h"
#include "sisdb_collect.h"

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////
// NULL 默认为读取
struct s_sis_method sisdb_methods[] = {
    {"init",      cmd_sisdb_init, SIS_METHOD_ACCESS_READ, NULL},   // 设置一些必要的参数
    {"get",       cmd_sisdb_get,  SIS_METHOD_ACCESS_READ, NULL},   // 默认 json 格式
    {"set",       cmd_sisdb_set,  SIS_METHOD_ACCESS_WRITE, NULL},   // 默认 json 格式
    {"del",       cmd_sisdb_del,  SIS_METHOD_ACCESS_WRITE, NULL},  // 删除一个数据 数据区没有数据时 清理键值
    {"drop",      cmd_sisdb_drop, SIS_METHOD_ACCESS_WRITE, NULL},   // 删除一个表结构数据
    {"gets",      cmd_sisdb_gets, SIS_METHOD_ACCESS_READ, NULL},   // 默认 json 格式 get 多个key多个sdb数据 暂时不支持
    {"bset",      cmd_sisdb_bset, SIS_METHOD_ACCESS_WRITE, NULL},   // 默认 二进制 格式
    {"dels",      cmd_sisdb_dels, SIS_METHOD_ACCESS_ADMIN, NULL},   // 删除多个数据
    {"sub",       cmd_sisdb_sub,  SIS_METHOD_ACCESS_READ, NULL},   // 订阅数据
    {"unsub",     cmd_sisdb_unsub, SIS_METHOD_ACCESS_READ, NULL},   // 取消订阅
    {"stop",      cmd_sisdb_stop, SIS_METHOD_ACCESS_WRITE, NULL},   // sno 当日数据完毕
    {"subsno",    cmd_sisdb_subsno, SIS_METHOD_ACCESS_READ, NULL},   // 订阅sno数据 {"date":20201010}
    {"unsubsno",  cmd_sisdb_unsubsno, SIS_METHOD_ACCESS_READ, NULL},   // 取消订阅sno数据 
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

s_sisdb_table *sisdb_table_create(s_sis_json_node *node_)
{
    s_sis_dynamic_db *db = sis_dynamic_db_create(node_);
    if (!db)
    {
        return NULL;
    }
    s_sisdb_table *o = SIS_MALLOC(s_sisdb_table, o);
    o->db = db;
    o->fields = sis_string_list_create();
    for (int i = 0; i < sis_map_list_getsize(db->fields); i++)
    {
        s_sis_dynamic_field *field = (s_sis_dynamic_field *)sis_map_list_geti(db->fields, i);
        sis_string_list_push(o->fields, (const char *)field->name, sis_sdslen(field->name));
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

s_sis_json_node *sis_sisdb_make_sdb_node(s_sisdb_cxt *context_)
{
    s_sis_json_node *jone = sis_json_create_object();
    s_sis_json_node *jsno = NULL;
    s_sis_json_node *jdbs = NULL;
    int count = sis_map_list_getsize(context_->sdbs);
    for (int i = 0; i < count; i++)
    {
        s_sisdb_table *table = sis_map_list_geti(context_->sdbs, i);
        if (table->style == SISDB_TB_STYLE_SNO)
        {
            if (!jsno)
            {
                jsno = sis_json_create_object();
            }
            sis_json_object_add_node(jsno, table->db->name, sis_dynamic_dbinfo_to_json(table->db));
        }
        if (table->style == SISDB_TB_STYLE_SDB)
        {
            if (!jdbs)
            {
                jdbs = sis_json_create_object();
            }
            sis_json_object_add_node(jdbs, table->db->name, sis_dynamic_dbinfo_to_json(table->db));
        }
    }
    if (jsno)
    {
        sis_json_object_add_node(jone, "snos", jsno);
    }
    if (jdbs)
    {
        sis_json_object_add_node(jone, "sdbs", jdbs);
    }
    return jone;
}
int is_single_sub(const char *key, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (key[i] == '*'||key[i] == ',')
        // if (key[i] == '*')
        {
            return false;
        }
    }  
    return true;
}
uint8 _set_sub_info_style(s_sisdb_sub_info *info, const char *in, size_t ilen)
{
    if (sis_strcasecmp(in, "*"))
    {
        return SISDB_SUB_ONE_ALL;
    }
    if (sis_strcasecmp(in, "*.*"))
    {
        return SISDB_SUB_TABLE_ALL;
    }
    s_sis_sds keyn = NULL; s_sis_sds sdbn = NULL; 
    int cmds = sis_str_divide_sds(in, '.', &keyn, &sdbn);
    if (cmds == 2)
    {
        if (sis_strcasecmp(keyn, "*"))
        {
            info->sdbs = sdbn;
            sis_sdsfree(keyn);
            return SISDB_SUB_TABLE_SDB;
        }
        if (sis_strcasecmp(sdbn, "*"))
        {
            info->keys = keyn;
            sis_sdsfree(sdbn);
            return SISDB_SUB_TABLE_KEY;
        }

        s_sis_string_list *klists = sis_string_list_create();
        sis_string_list_load(klists, keyn, sis_sdslen(keyn), ",");  
        s_sis_string_list *slists = sis_string_list_create();
        sis_string_list_load(slists, sdbn, sis_sdslen(sdbn), ",");  
        int kcount = sis_string_list_getsize(klists);
        int scount = sis_string_list_getsize(slists);
        info->keys = sis_sdsempty();
        int count = 0;
        for (int k = 0; k < kcount; k++)
        {
            for (int i = 0; i < scount; i++)
            {
                if (count > 0)
                {
                    info->keys = sis_sdscat(info->keys, ",");
                }
                count++;
                info->keys = sis_sdscatfmt(info->keys, "%s.%s", sis_string_list_get(klists, k), sis_string_list_get(slists, i));
            }         
        }
        sis_string_list_destroy(klists);
        sis_string_list_destroy(slists);

        sis_sdsfree(keyn);    sis_sdsfree(sdbn);
        return SISDB_SUB_TABLE_MUL;
    }
    info->keys = sis_sdsnewlen(in, ilen);
    
    sis_sdsfree(keyn);    sis_sdsfree(sdbn);
    return SISDB_SUB_ONE_MUL;
} 
s_sisdb_sub_info *sisdb_sub_info_create(s_sis_net_message *netmsg_)
{
    if (!netmsg_||!netmsg_->key)
    {
        return NULL;
    }
    s_sisdb_sub_info *o = SIS_MALLOC(s_sisdb_sub_info, o);
    o->subtype = _set_sub_info_style(o, netmsg_->key, sis_sdslen(netmsg_->key));
    o->netmsgs = sis_pointer_list_create();
    o->netmsgs->vfree = sis_net_message_decr;
    return o;
}

void sisdb_sub_info_destroy(void *info_)
{
    s_sisdb_sub_info *info = (s_sisdb_sub_info *)info_;
    if(info->keys)
    {
        sis_sdsfree(info->keys);
    }
    if(info->sdbs)
    {
        sis_sdsfree(info->sdbs);
    }
    sis_pointer_list_destroy(info->netmsgs);
    sis_free(info);
}

s_sisdb_subsno_info *sisdb_subsno_info_create(s_sisdb_cxt *sisdb_,s_sis_net_message *netmsg_)
{
    if (!netmsg_||!netmsg_->key)
    {
        return NULL;
    }
    s_sisdb_subsno_info *o = SIS_MALLOC(s_sisdb_subsno_info, o);
    o->sisdb = sisdb_;
    sis_net_message_incr(netmsg_);
    o->netmsg = netmsg_;
    // 设置请求的格式
    o->netmsg->rfmt = sisdb_get_format(o->netmsg->val);
    return o;
}
void sisdb_subsno_info_destroy(void *info_)
{
    s_sisdb_subsno_info *info = (s_sisdb_subsno_info *)info_;
    sis_net_message_decr(info->netmsg);
    sis_free(info);
}

bool sisdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_cxt *context = SIS_MALLOC(s_sisdb_cxt, context);
    worker->context = context;

	context->name = sis_sdsnew(node->key);
    // context->work_date = 0;
    context->work_date = sis_time_get_idate(0);

	context->sdbs = sis_map_list_create(sisdb_table_destroy);
    {
        s_sis_json_node *snnode = sis_json_cmp_child_node(node, "snos");
        if (snnode)
        {
            s_sis_json_node *next = sis_conf_first_node(snnode);
            while (next)
            {
                s_sisdb_table *table = sisdb_table_create(next);
                table->style = SISDB_TB_STYLE_SNO;
                sis_map_list_set(context->sdbs, next->key, table);
                next = next->next;
            }
        }
        s_sis_json_node *tbnode = sis_json_cmp_child_node(node, "sdbs");
        if (tbnode)
        {
            s_sis_json_node *next = sis_conf_first_node(tbnode);
            while (next)
            {
                s_sisdb_table *table = sisdb_table_create(next);
                table->style = SISDB_TB_STYLE_SDB;
                sis_map_list_set(context->sdbs, next->key, table);
                next = next->next;
            }
        }
    }
    // 设置为 4M 容量
    context->series = sis_node_list_create(4000000, sizeof(s_sisdb_collect_sno));
    context->collects = sis_map_pointer_create_v(sisdb_collect_destroy);

    context->sub_multiple = sis_map_pointer_create_v(sisdb_sub_info_destroy);

    context->sub_single = sis_map_pointer_create_v(sisdb_sub_info_destroy);
    
    context->subsno_worker = sis_map_pointer_create_v(sisdb_subsno_info_destroy);
    return true;
}
void sisdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;

    sis_map_pointer_destroy(context->sub_multiple);
    sis_map_pointer_destroy(context->sub_single);

    sis_map_pointer_destroy(context->subsno_worker);

    sis_map_pointer_destroy(context->collects);
    sis_node_list_destroy(context->series);
    
    sis_map_list_destroy(context->sdbs);

    if (context->fast_path)
    {
        sis_sdsfree(context->fast_path);
    }

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

int cmd_sisdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    context->socket = sis_message_get(msg, "socket");
    context->fast_path = sis_sdsdup(sis_message_get_str(msg, "fastpath"));
    // printf("init %p \n", argv_);
    return SIS_METHOD_OK;
}


int cmd_sisdb_get(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    s_sis_sds keyn = NULL; s_sis_sds sdbn = NULL; 
    int cmds = sis_str_divide_sds(netmsg->key, '.', &keyn, &sdbn);
    s_sis_sds o = NULL;

    uint16 format = SISDB_FORMAT_CHARS;
    if (cmds == 1)
    {
        // 单
        o = sisdb_one_get_sds(context, keyn, &format, netmsg->val);
    }
    else
    {
        s_sisdb_table *tb = sis_map_list_get(context->sdbs, sdbn);
        if (tb)
        {
            printf("tb->style = %s %d\n", sdbn, tb->style);
            if (tb->style == SISDB_TB_STYLE_SNO)
            {
                o = sisdb_get_sno_sds(context, netmsg->key, &format, netmsg->val);
            }
            else
            {
                o = sisdb_get_sds(context, netmsg->key, &format, netmsg->val);
            }
        }
    }
    sis_sdsfree(keyn);    sis_sdsfree(sdbn);
	if (o)
	{
        if(format == SISDB_FORMAT_CHARS)
        {
            sis_net_ans_with_chars(netmsg, o, sis_sdslen(o));
        }
        else
        {
            sis_net_ans_with_bytes(netmsg, o, sis_sdslen(o));
        } 
        sis_sdsfree(o);      
		return SIS_METHOD_OK;
	}
	return SIS_METHOD_NULL;
}
int cmd_sisdb_set(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    if (!netmsg->val && sis_sdslen(netmsg->val) > 0)
    {
        return SIS_METHOD_ERROR;
    }
    int o = -1;

    s_sis_sds keyn = NULL; s_sis_sds sdbn = NULL; 
    int cmds = sis_str_divide_sds(netmsg->key, '.', &keyn, &sdbn);
    // printf("cmd_sisdb_set: %d %s %s %s\n", cmds, netmsg->key, keyn, sdbn);
    if (cmds == 1)
    {
        // 单
        o = sisdb_one_set(context, keyn, SISDB_COLLECT_TYPE_CHARS, netmsg->val);
    }
    else
    {
        o = sisdb_set_chars(context, netmsg->key, netmsg->val);
    }
    sis_sdsfree(keyn);    sis_sdsfree(sdbn);
	if (!o)
	{
        sis_net_ans_with_ok(netmsg);
		return SIS_METHOD_OK;
	}
    // printf("set rtn : %d\n", o);
	return SIS_METHOD_ERROR;
}

int cmd_sisdb_stop(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sisdb_collect_sno sno;
    sno.collect = NULL;
    sno.recno = -1;
    sis_node_list_push(context->series, &sno);
    return SIS_METHOD_OK;
}

int cmd_sisdb_del(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    s_sis_sds keyn = NULL; s_sis_sds sdbn = NULL; 
    int cmds = sis_str_divide_sds(netmsg->key, '.', &keyn, &sdbn);
    int o = 0;
    if (cmds == 1)
    {
        o = sisdb_one_del(context, keyn, netmsg->val);
    }
    else
    {
        o = sisdb_del(context, netmsg->key, netmsg->val);
    }
    sis_sdsfree(keyn);    sis_sdsfree(sdbn);
    sis_net_ans_with_int(netmsg, o);
    return SIS_METHOD_OK;
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
    
    s_sis_sds keyn = NULL; s_sis_sds sdbn = NULL; 
    int cmds = sis_str_divide_sds(netmsg->key, '.', &keyn, &sdbn);
    // printf("cmd_sisdb_gets: %d %s %s \n", cmds, keyn, sdbn);
    s_sis_sds o = NULL;
    if (cmds == 1)
    {
        o = sisdb_one_gets_sds(context, keyn, netmsg->val);
    }
    else
    {
        o = sisdb_gets_sds(context, keyn, sdbn, netmsg->val);
    }
    sis_sdsfree(keyn);    sis_sdsfree(sdbn);
	if (o)
	{
        sis_net_ans_with_chars(netmsg, o, sis_sdslen(o));
        sis_sdsfree(o);
		return SIS_METHOD_OK;
	}
	return SIS_METHOD_NULL;
}
int cmd_sisdb_bset(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    s_sis_sds keyn = NULL; s_sis_sds sdbn = NULL; 
    int cmds = sis_str_divide_sds(netmsg->key, '.', &keyn, &sdbn);
    // printf("cmd_sisdb_bset: %d %s %s \n", cmds, keyn, sdbn);
    if (!netmsg->val && sis_sdslen(netmsg->val) > 0)
    {
        return SIS_METHOD_ERROR;
    }
    int o = -1;
    if (cmds == 1)
    {
        // 单
        o = sisdb_one_set(context, keyn, SISDB_COLLECT_TYPE_BYTES, netmsg->val);
    }
    else
    {
        o = sisdb_set_bytes(context, netmsg->key, netmsg->val);
    }
    sis_sdsfree(keyn);    sis_sdsfree(sdbn);
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
    
    s_sis_sds keyn = NULL; s_sis_sds sdbn = NULL; 
    int cmds = sis_str_divide_sds(netmsg->key, '.', &keyn, &sdbn);
    // printf("cmd_sisdb_dels: %d %s %s \n", cmds, keyn, sdbn);
    int o = 0;
    if (cmds == 1)
    {
        o = sisdb_one_dels(context, keyn, netmsg->val);
    }
    else
    {
        o = sisdb_dels(context, keyn, sdbn, netmsg->val);
    }
    sis_sdsfree(keyn);    sis_sdsfree(sdbn);
    sis_net_ans_with_int(netmsg, o);
    return SIS_METHOD_OK;
}

int is_multiple_sub(const char *key, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (key[i] == '*'||key[i] == ',')
        // if (key[i] == '*')
        {
            return true;
        }
    }  
    return false;
}

int cmd_sisdb_sub(void *worker_, void *argv_)
{
    // 只订阅最后一条记录 不开线程 
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    int o = 0;
    if (netmsg->key && sis_sdslen(netmsg->key) > 0)
    {
        if (is_multiple_sub(netmsg->key, sis_sdslen(netmsg->key)))
        {
            o = sisdb_multiple_sub(context, netmsg, false);
        }
        else
        {
            o = sisdb_one_sub(context, netmsg, false);        
        }
    }
    sis_net_ans_with_int(netmsg, o);
    return SIS_METHOD_OK;
}
int cmd_sisdb_unsub(void *worker_, void *argv_)
{
    // 只订阅最后一条记录 不开线程 
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    int o = 0;
    if (netmsg->key && sis_sdslen(netmsg->key) > 0)
    {
        if (!sis_strcasecmp(netmsg->key, "*")||!sis_strcasecmp(netmsg->key, "*.*"))
        {
            o = sisdb_unsub_whole(context, netmsg->cid, false);
        }    
        else if (is_multiple_sub(netmsg->key, sis_sdslen(netmsg->key)))
        {
            o = sisdb_multiple_unsub(context, netmsg, false);
        }
        else
        {
            o = sisdb_one_unsub(context, netmsg, false);
        }
    }
    sis_net_ans_with_int(netmsg, o);
    return SIS_METHOD_OK;
}

int cmd_sisdb_subsno(void *worker_, void *argv_)
{
    // 开启线程来处理
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    int o = 0;
    if (netmsg->key && strchr(netmsg->key, '.') && sis_sdslen(netmsg->key) > 0)
    {
        if (is_multiple_sub(netmsg->key, sis_sdslen(netmsg->key)))
        {
            o = sisdb_multiple_subsno(context, netmsg);
        }
        else
        {
            o = sisdb_one_subsno(context, netmsg);        
        }
        sis_net_ans_with_int(netmsg, o);
        return SIS_METHOD_OK;
    }
    return SIS_METHOD_ERROR;
}
int cmd_sisdb_unsubsno(void *worker_, void *argv_)
{
    // 开启线程来处理
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
        
    int o = 0;
    if (netmsg->key && strchr(netmsg->key, '.') && sis_sdslen(netmsg->key) > 0)
    {
        if (!sis_strcasecmp(netmsg->key, "*.*"))
        {
            o = sisdb_unsubsno_whole(context, netmsg->cid);
        }    
        else if (is_multiple_sub(netmsg->key, sis_sdslen(netmsg->key)))
        {
            o = sisdb_multiple_unsubsno(context, netmsg);
        }
        else
        {
            o = sisdb_one_unsubsno(context, netmsg);
        }
        sis_net_ans_with_int(netmsg, o);
        return SIS_METHOD_OK;
    }
    return SIS_METHOD_ERROR;
}
