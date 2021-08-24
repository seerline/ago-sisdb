#include "sis_modules.h"
#include "worker.h"
#include "sis_method.h"

#include "sisdb.h"
#include "sisdb_io.h"
#include "sis_net.msg.h"
#include "sisdb_collect.h"

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////
// NULL 默认为读取
struct s_sis_method sisdb_methods[] = {
    {"show",      cmd_sisdb_show,       SIS_METHOD_ACCESS_READ,  NULL},   // 默认 json 格式
    {"init",      cmd_sisdb_open,       SIS_METHOD_ACCESS_NONET, NULL},   // 设置一些必要的参数
    {"get",       cmd_sisdb_get,        SIS_METHOD_ACCESS_READ,  NULL},   // 默认 json 格式
    {"set",       cmd_sisdb_set,        SIS_METHOD_ACCESS_RDWR,  NULL},   // 默认 json 格式
    {"bset",      cmd_sisdb_bset,       SIS_METHOD_ACCESS_RDWR,  NULL},   // 默认 二进制 格式
    {"del",       cmd_sisdb_del,        SIS_METHOD_ACCESS_RDWR,  NULL},   // 删除一个数据 数据区没有数据时 清理键值
    {"drop",      cmd_sisdb_drop,       SIS_METHOD_ACCESS_RDWR,  NULL},   // 删除一个表结构数据
    {"gets",      cmd_sisdb_gets,       SIS_METHOD_ACCESS_READ,  NULL},   // 默认 json 格式 get 多个key最后一条sdb数据 
    {"keys",      cmd_sisdb_keys,       SIS_METHOD_ACCESS_READ,  NULL},   // 获取 所有keys
    {"dels",      cmd_sisdb_dels,       SIS_METHOD_ACCESS_ADMIN, NULL},   // 删除多个数据
    {"sub",       cmd_sisdb_sub,        SIS_METHOD_ACCESS_READ,  NULL},   // 订阅数据 - 只发新写入的数据
    {"unsub",     cmd_sisdb_unsub,      SIS_METHOD_ACCESS_READ,  NULL},   // 取消订阅
    // 磁盘数据订阅
    {"psub",      cmd_sisdb_psub,       SIS_METHOD_ACCESS_READ,  NULL},   // 回放数据 需指定日期 支持模糊匹配 所有数据全部拿到按时间排序后一条一条返回 
    {"unpsub",    cmd_sisdb_unpsub,     SIS_METHOD_ACCESS_READ,  NULL},   // 取消回放
    // 存储方法
    {"save",      cmd_sisdb_disk_save  ,     SIS_METHOD_ACCESS_ADMIN, NULL},   // 存盘
    {"pack",      cmd_sisdb_disk_pack  ,     SIS_METHOD_ACCESS_ADMIN, NULL},   // 合并整理数据
    // 主要用于server调用
    {"rdisk",     cmd_sisdb_rdisk ,     SIS_METHOD_ACCESS_NONET, NULL},   // 从磁盘加载数据
    {"rlog",      cmd_sisdb_rlog  ,     SIS_METHOD_ACCESS_NONET, NULL},   // 加载没有写盘的log信息
    {"wlog",      cmd_sisdb_wlog  ,     SIS_METHOD_ACCESS_NONET, NULL},   // 写入没有写盘的log信息
    {"clear",     cmd_sisdb_clear ,     SIS_METHOD_ACCESS_NONET, NULL},   // 停止某个客户的所有查询
    {"getdb",     cmd_sisdb_getdb ,     SIS_METHOD_ACCESS_NONET, NULL},   // 得到表
};
// 共享内存数据库
s_sis_modules sis_modules_sisdb = {
    sisdb_init,
    NULL,
    sisdb_working,
    NULL,
    sisdb_uninit,
    sisdb_method_init,
    sisdb_method_uninit,
    sizeof(sisdb_methods) / sizeof(s_sis_method),
    sisdb_methods,
};

uint8 _set_sub_info_style(s_sisdb_reader *info, const char *in, size_t ilen)
{
    if (sis_strcasecmp(in, "*"))
    {
        return SISDB_SUB_ONE_ALL;
    }
    if (sis_strcasecmp(in, "*.*"))
    {
        return SISDB_SUB_TABLE_ALL;
    }
    s_sis_sds keys = NULL; s_sis_sds sdbs = NULL; 
    int cmds = sis_str_divide_sds(in, '.', &keys, &sdbs);
    if (cmds == 2)
    {
        if (sis_strcasecmp(keys, "*"))
        {
            info->sub_sdbs = sdbs;
            sis_sdsfree(keys);
            return SISDB_SUB_TABLE_SDB;
        }
        if (sis_strcasecmp(sdbs, "*"))
        {
            info->sub_keys = keys;
            sis_sdsfree(sdbs);
            return SISDB_SUB_TABLE_KEY;
        }
        s_sis_string_list *klists = sis_string_list_create();
        sis_string_list_load(klists, keys, sis_sdslen(keys), ",");  
        s_sis_string_list *slists = sis_string_list_create();
        sis_string_list_load(slists, sdbs, sis_sdslen(sdbs), ",");  
        int kcount = sis_string_list_getsize(klists);
        int scount = sis_string_list_getsize(slists);
        info->sub_keys = sis_sdsempty();
        int count = 0;
        for (int k = 0; k < kcount; k++)
        {
            for (int i = 0; i < scount; i++)
            {
                if (count > 0)
                {
                    info->sub_keys = sis_sdscat(info->sub_keys, ",");
                }
                count++;
                info->sub_keys = sis_sdscatfmt(info->sub_keys, "%s.%s", sis_string_list_get(klists, k), sis_string_list_get(slists, i));
            }         
        }
        sis_string_list_destroy(klists);
        sis_string_list_destroy(slists);

        sis_sdsfree(keys);    sis_sdsfree(sdbs);
        return SISDB_SUB_TABLE_MUL;
    }
    info->sub_keys = sis_sdsnewlen(in, ilen);  
    sis_sdsfree(keys);    sis_sdsfree(sdbs);
    return SISDB_SUB_ONE_MUL;
} 

s_sisdb_reader *sisdb_reader_create(s_sis_net_message *netmsg_)
{
    if (!netmsg_||!netmsg_->key)
    {
        return NULL;
    }
    s_sisdb_reader *o = SIS_MALLOC(s_sisdb_reader, o);
    o->sub_type = _set_sub_info_style(o, netmsg_->key, sis_sdslen(netmsg_->key));
    o->netmsgs = sis_pointer_list_create();
    o->netmsgs->vfree = sis_net_message_decr;
    return o;
}

void sisdb_reader_destroy(void *reader_)
{
    s_sisdb_reader *o = (s_sisdb_reader *)reader_;
    sis_sdsfree(o->sub_keys);
    sis_sdsfree(o->sub_sdbs);
    sis_pointer_list_destroy(o->netmsgs);
    sis_free(o);
}


bool sisdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_cxt *context = SIS_MALLOC(s_sisdb_cxt, context);
    worker->context = context;

    context->work_date = sis_time_get_idate(0); 
	context->work_name = sis_sdsnew(node->key);

    // 默认4点存盘
    context->save_time = sis_json_get_int(node, "save-time", 40000);

	context->work_sdbs = sis_map_list_create(sisdb_table_destroy);
    s_sis_json_node *tbnode = sis_json_cmp_child_node(node, "sdbs");
    if (tbnode)
    {
        s_sis_json_node *next = sis_conf_first_node(tbnode);
        while (next)
        {
            s_sisdb_table *table = sisdb_table_create(next);
            sis_map_list_set(context->work_sdbs, next->key, table);
            next = next->next;
        }
    }
    context->work_sub_cxt = sisdb_sub_cxt_create();

    // 数据集合
    context->work_keys = sis_map_pointer_create_v(sisdb_collect_destroy);

    sis_mutex_init(&(context->wlog_lock), NULL);
    s_sis_json_node *wlognode = sis_json_cmp_child_node(node, "wlog");
    if (wlognode)
    {
		// 表示需要加载wlog的数据
        s_sis_worker *service = sis_worker_create(worker, wlognode);
        if (service)
        {
            context->wlog_worker = service; 
			context->wlog_method = sis_worker_get_method(context->wlog_worker, "write");
        }     
    }
    s_sis_json_node *wfilenode = sis_json_cmp_child_node(node, "wfile");
    if (wfilenode)
    {
		// 表示收到stop时需要从wlog中获取数据并转格式后存盘
        s_sis_worker *service = sis_worker_create(worker, wfilenode);
        if (service)
        {
            context->wfile_worker = service; 
        }     
    }
    s_sis_json_node *rfilenode = sis_json_cmp_child_node(node, "rfile");
    if (rfilenode)
    {
        s_sis_worker *service = sis_worker_create(worker, rfilenode);
        if (service)
        {
            context->rfile_worker = service; 
        }       }
    return true;
}
void sisdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;

    if (context->wlog_worker)
    {
        if (context->wlog_open)
        {
            sis_worker_command(context->wlog_worker, "close", context->work_name);
            context->wlog_open = 0;
        }
        sis_worker_destroy(context->wlog_worker);
		context->wlog_worker = NULL;
    }
    if (context->wfile_worker)
    {
        sis_worker_destroy(context->wfile_worker);
		context->wfile_worker = NULL;
    }
    if (context->rfile_worker)
    {
		sis_worker_destroy(context->rfile_worker);
		context->rfile_worker = NULL;
    }
    if (context->work_sub_cxt)
    {
        sisdb_sub_cxt_destroy(context->work_sub_cxt);
        context->work_sub_cxt = NULL;
    }
    sis_map_pointer_destroy(context->work_keys); 
    sis_map_list_destroy(context->work_sdbs);
    sis_sdsfree(context->work_path);
    sis_sdsfree(context->work_name);
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

void sisdb_working(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;

    if (!context->wfile_worker)
    {
        return;
    }
    int idate = sis_time_get_idate(0);
    if (idate <= context->work_date)
    {
        return;
    }
    int minute = sis_time_get_iminute(0);
    if (minute > context->save_time)
    {
        sis_mutex_lock(&context->wlog_lock);
        if (context->wlog_open)
        {
            sis_worker_command(context->wlog_worker, "close", context->work_name);
            context->wlog_open = 0;
        }

        // 这里要判断是否新的一天 如果是就存盘
        if (sisdb_disk_save(context) == SIS_METHOD_OK)
        {
            sis_worker_command(context->wlog_worker, "move", context->work_name);
        }
        int week = sis_time_get_week_ofday(context->work_date);
        // 存盘后如果检测到是周五 就执行pack工作
        if (week == 5)
        {
            if (sisdb_disk_pack(context) == SIS_METHOD_OK)
            {

            }
        }   
        context->work_date = idate;   
        LOG(5)("new workdate = %d\n", context->work_date);  
        sis_mutex_unlock(&context->wlog_lock);
    }
}
s_sis_sds sis_sisdb_make_sdbs(s_sisdb_cxt *context_)
{
    int count = sis_map_list_getsize(context_->work_sdbs);
    if (count < 1)
    {
        return NULL;
    }
    s_sis_json_node *jone = sis_json_create_object();
    s_sis_json_node *jdbs = sis_json_create_object();
    for (int i = 0; i < count; i++)
    {
        s_sisdb_table *table = sis_map_list_geti(context_->work_sdbs, i);
		sis_json_object_add_node(jdbs, table->name, sis_dynamic_dbinfo_to_json(table));
    }
    sis_json_object_add_node(jone, context_->work_name, jdbs);
    s_sis_sds o = sis_json_to_sds(jone, 1);
	sis_json_delete_node(jone);
	return o;
}
int cmd_sisdb_show(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    s_sis_sds sdbs = sis_sisdb_make_sdbs(context);
    if (!sdbs)
    {
        return SIS_METHOD_NULL;
    }
    sis_net_ans_with_chars(netmsg, sdbs, sis_sdslen(sdbs));
    sis_sdsfree(sdbs);
    return SIS_METHOD_OK;
}
int cmd_sisdb_open(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    
	context->cb_source = sis_message_get(msg, "cb_source");
	context->cb_net_message = sis_message_get_method(msg, "cb_net_message");

    // 多值订阅者
    sisdb_sub_cxt_init(context->work_sub_cxt, context->cb_source, context->cb_net_message);
    
    return SIS_METHOD_OK;
}

int cmd_sisdb_get(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    s_sis_sds kname = NULL; s_sis_sds sname = NULL; 
    int cmds = sis_str_divide_sds(netmsg->key, '.', &kname, &sname);
    s_sis_sds o = NULL;
    uint16 format = SISDB_FORMAT_CHARS;
    if (cmds == 1)
    {
        // 单
        o = sisdb_one_get_sds(context, kname, &format, netmsg->ask);
    }
    else
    {
        s_sisdb_table *table = sis_map_list_get(context->work_sdbs, sname);
        if (table)
        {
            o = sisdb_get_sds(context, netmsg->key, &format, netmsg->ask);
        }
    }
    sis_sdsfree(kname);    sis_sdsfree(sname);
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
    
    if (!netmsg->switchs.has_ask)
    {
        return SIS_METHOD_ERROR;
    }
    int o = -1;

    s_sis_sds kname = NULL; s_sis_sds sname = NULL; 
    int cmds = sis_str_divide_sds(netmsg->key, '.', &kname, &sname);
    // printf("cmd_sisdb_set: %d %s %s %s\n", cmds, netmsg->key, kname, sname);
    if (cmds == 1)
    {
        // 单
        o = sisdb_one_set(context, kname, SISDB_COLLECT_TYPE_CHARS, netmsg->ask);
    }
    else
    {
        o = sisdb_set_chars(context, netmsg->key, netmsg->ask);
    }
    sis_sdsfree(kname);    sis_sdsfree(sname);

    // 这里处理订阅
    sisdb_sub_cxt_pub(context->work_sub_cxt, netmsg);

	if (!o)
	{
        sis_net_ans_with_ok(netmsg);
		return SIS_METHOD_OK;
	}
    // printf("set rtn : %d\n", o);
	return SIS_METHOD_ERROR;
}
int cmd_sisdb_bset(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    // 得到二进制数据
    if(!netmsg->switchs.has_argvs)
    {
        return SIS_METHOD_ERROR;
    }
    s_sis_sds imem = sis_net_get_argvs(netmsg, 0);
    s_sis_sds kname = NULL; s_sis_sds sname = NULL; 
    int cmds = sis_str_divide_sds(netmsg->key, '.', &kname, &sname);
    // printf("cmd_sisdb_bset: %d %s %s \n", cmds, kname, sname);
    int o = -1;
    if (cmds == 1)
    {
        // 单
        o = sisdb_one_set(context, kname, SISDB_COLLECT_TYPE_BYTES, imem);
    }
    else
    {
        o = sisdb_set_bytes(context, netmsg->key, imem);
    }
    sis_sdsfree(kname);    sis_sdsfree(sname);

    // 这里处理订阅
    sisdb_sub_cxt_pub(context->work_sub_cxt, netmsg);

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
    
    s_sis_sds kname = NULL; s_sis_sds sname = NULL; 
    int cmds = sis_str_divide_sds(netmsg->key, '.', &kname, &sname);
    int o = 0;
    if (cmds == 1)
    {
        o = sisdb_one_del(context, kname, netmsg->ask);
    }
    else
    {
        o = sisdb_del(context, netmsg->key, netmsg->ask);
    }
    sis_sdsfree(kname);    sis_sdsfree(sname);
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
    
    s_sis_sds o = NULL;
    s_sis_sds kname = NULL; s_sis_sds sname = NULL; 
    int cmds = sis_str_divide_sds(netmsg->key, '.', &kname, &sname);
    // printf("cmd_sisdb_gets: %d %s %s \n", cmds, kname, sname);
    if (cmds == 1)
    {
        o = sisdb_one_gets_sds(context, kname, netmsg->ask);
    }
    else
    {
        o = sisdb_gets_sds(context, kname, sname, netmsg->ask);
    }
    sis_sdsfree(kname);    sis_sdsfree(sname);
    
	if (o)
	{
        sis_net_ans_with_chars(netmsg, o, sis_sdslen(o));
        sis_sdsfree(o);
		return SIS_METHOD_OK;
	}
	return SIS_METHOD_NULL;
}
int cmd_sisdb_keys(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    s_sis_sds o = NULL;
    
    if (netmsg->key && !sis_strcasecmp(netmsg->key, "*"))
    {
        // 求单键
        o = sisdb_one_keys_sds(context, netmsg->key);
    }
    else
    {
        o = sisdb_keys_sds(context, netmsg->key, netmsg->ask);        
    }
    
	if (o)
	{
        sis_net_ans_with_chars(netmsg, o, sis_sdslen(o));
        sis_sdsfree(o);
		return SIS_METHOD_OK;
	}
	return SIS_METHOD_NULL;
}


int cmd_sisdb_dels(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    s_sis_sds kname = NULL; s_sis_sds sname = NULL; 
    int cmds = sis_str_divide_sds(netmsg->key, '.', &kname, &sname);
    // printf("cmd_sisdb_dels: %d %s %s \n", cmds, kname, sname);
    int o = 0;
    if (cmds == 1)
    {
        o = sisdb_one_dels(context, kname, netmsg->ask);
    }
    else
    {
        o = sisdb_dels(context, kname, sname, netmsg->ask);
    }
    sis_sdsfree(kname);    sis_sdsfree(sname);
    sis_net_ans_with_int(netmsg, o);
    return SIS_METHOD_OK;
}

int cmd_sisdb_sub(void *worker_, void *argv_)
{
    // 只订阅最后一条记录 不开线程 
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;  
    int o = sisdb_sub_cxt_sub(context->work_sub_cxt, netmsg);
    sis_net_ans_with_int(netmsg, o);
    return SIS_METHOD_OK;
}
int cmd_sisdb_unsub(void *worker_, void *argv_)
{
    // 只订阅最后一条记录 不开线程 
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    int o = sisdb_sub_cxt_unsub(context->work_sub_cxt, netmsg->cid);
    sis_net_ans_with_int(netmsg, o);
    return SIS_METHOD_OK;
}
int cmd_sisdb_psub(void *worker_, void *argv_)
{
    // 只订阅最后一条记录 不开线程 
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    int o = 0;
    // if (netmsg->key && sis_sdslen(netmsg->key) > 0)
    // {
    //     if (sis_is_multiple_sub(netmsg->key, sis_sdslen(netmsg->key)))
    //     {
    //         o = sisdb_multiple_sub(context, netmsg);
    //     }
    //     else
    //     {
    //         o = sisdb_one_sub(context, netmsg);        
    //     }
    // }
    sis_net_ans_with_int(netmsg, o);
    return SIS_METHOD_OK;
}
int cmd_sisdb_unpsub(void *worker_, void *argv_)
{
    // 只订阅最后一条记录 不开线程 
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    int o = 0;
    // if (netmsg->key && sis_sdslen(netmsg->key) > 0)
    // {
    //     if (!sis_strcasecmp(netmsg->key, "*")||!sis_strcasecmp(netmsg->key, "*.*"))
    //     {
    //         o = sisdb_unsub_whole(context, netmsg->cid);
    //     }    
    //     else if (sis_is_multiple_sub(netmsg->key, sis_sdslen(netmsg->key)))
    //     {
    //         o = sisdb_multiple_unsub(context, netmsg);
    //     }
    //     else
    //     {
    //         o = sisdb_one_unsub(context, netmsg);
    //     }
    // }
    // else
    // {
    //     // 没有键值就取消所有订阅
    //     o = sisdb_unsub_whole(context, netmsg->cid);
    // }
    sis_net_ans_with_int(netmsg, o);
    return SIS_METHOD_OK;
}

int cmd_sisdb_disk_save(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    
    int o = sisdb_disk_save(context);  
    if (o == SIS_METHOD_OK)
    {
        sis_worker_command(context->wlog_worker, "move", context->work_name);
        s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
        sis_net_ans_with_ok(netmsg);
    }
    return o;
}

int cmd_sisdb_disk_pack(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    
    int o = sisdb_disk_pack(context);

    if (o == SIS_METHOD_OK)
    {
        s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
        sis_net_ans_with_ok(netmsg);
    }
    return o;
}
int cmd_sisdb_rdisk(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    sis_message_set(msg, "sisdb", context, NULL);
    // 如果自己有就用自己的 否则就继承
    // sis_message_set(msg, "config", &context->catch_cfg, NULL);
    // 直接转移过去
    // sis_mutex_lock(&context->wlog_lock);
    sis_worker_command(context->rfile_worker, "load", msg);
    // sis_mutex_unlock(&context->wlog_lock);

    return SIS_METHOD_OK;
}

static int cb_rlog_recv(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    printf("cb_rlog_recv: %d %s \n%s \n%s \n%s \n", netmsg->switchs.is_reply,
            netmsg->name? netmsg->name : "nil",
            netmsg->cmd ?   netmsg->cmd : "nil",
            netmsg->key?    netmsg->key : "nil",
            netmsg->ask?    netmsg->ask : "nil");   

    char argv[2][128]; 
    int cmds = sis_str_divide(netmsg->cmd, '.', argv[0], argv[1]);
    if (cmds == 2)
    {
        sis_worker_command(worker, argv[1], netmsg);
    }  
    return 0;
}
int cmd_sisdb_rlog(void *worker_, void *argv_)
{   
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

    sis_mutex_lock(&context->wlog_lock);
    sis_message_set_str(msg, "dbname", context->work_name, sis_sdslen(context->work_name));
    sis_message_set(msg, "source", worker, NULL);
    sis_message_set_method(msg, "cb_recv", cb_rlog_recv);
    int o = sis_worker_command(context->wlog_worker, "read", msg);
    sis_mutex_unlock(&context->wlog_lock);
	return o;
}
int cmd_sisdb_wlog(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;

    sis_mutex_lock(&context->wlog_lock);
    if (!context->wlog_open)
    {
        s_sis_message *msg = sis_message_create();
        sis_message_set_str(msg, "log-name", context->work_name, sis_sdslen(context->work_name));
        sis_message_set_int(msg, "log-date", context->work_date);
        sis_worker_command(context->wlog_worker, "open", msg);
    	sis_message_destroy(msg);
        context->wlog_open = 1;
    }
    int o = context->wlog_method->proc(context->wlog_worker, argv_);
    sis_mutex_unlock(&context->wlog_lock);

    return o;
}

int cmd_sisdb_clear(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

	const char *mode = sis_message_get_str(msg, "mode");
	if (sis_strcasecmp(mode, "reader"))
	{
        if (sis_message_exist(msg, "cid"))
        {
            sisdb_sub_cxt_unsub(context->work_sub_cxt, sis_message_get_int(msg, "cid"));
        }
	}
    return SIS_METHOD_OK;
}

int cmd_sisdb_getdb(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

	const char *dbname = sis_message_get_str(msg, "dbname");
    sis_message_set(msg, "db", NULL, NULL);
    s_sisdb_table *table = sisdb_get_table(context, dbname);
    if (table)
    {
        sis_message_set(msg, "db", table, NULL);
    }
    return SIS_METHOD_OK;
}

