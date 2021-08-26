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
    {"create",    cmd_sisdb_create,     SIS_METHOD_ACCESS_RDWR,  NULL},   // 默认 json 格式
    {"get",       cmd_sisdb_get,        SIS_METHOD_ACCESS_READ,  NULL},   // 默认 json 格式
    {"set",       cmd_sisdb_set,        SIS_METHOD_ACCESS_RDWR,  NULL},   // 默认 json 格式
    {"bset",      cmd_sisdb_bset,       SIS_METHOD_ACCESS_RDWR,  NULL},   // 默认 二进制 格式
    {"del",       cmd_sisdb_del,        SIS_METHOD_ACCESS_RDWR,  NULL},   // 删除一个数据 数据区没有数据时 清理键值
    {"drop",      cmd_sisdb_drop,       SIS_METHOD_ACCESS_RDWR,  NULL},   // 删除一个表结构数据
    {"gets",      cmd_sisdb_gets,       SIS_METHOD_ACCESS_READ,  NULL},   // 默认 json 格式 get 多个key最后一条sdb数据 
    {"keys",      cmd_sisdb_keys,       SIS_METHOD_ACCESS_READ,  NULL},   // 获取 所有keys
    {"dels",      cmd_sisdb_dels,       SIS_METHOD_ACCESS_ADMIN, NULL},   // 删除多个数据
    {"sub",       cmd_sisdb_sub,        SIS_METHOD_ACCESS_READ,  NULL},   // 订阅数据 - 只发新写入的数据
    {"hsub",      cmd_sisdb_hsub,       SIS_METHOD_ACCESS_READ,  NULL},   // 订阅数据 - 头匹配 只发新写入的数据
    {"unsub",     cmd_sisdb_unsub,      SIS_METHOD_ACCESS_READ,  NULL},   // 取消订阅
    // 磁盘数据订阅
    {"psub",      cmd_sisdb_psub,       SIS_METHOD_ACCESS_READ,  NULL},   // 回放数据 需指定日期 支持模糊匹配 所有数据全部拿到按时间排序后一条一条返回 
    {"unpsub",    cmd_sisdb_unpsub,     SIS_METHOD_ACCESS_READ,  NULL},   // 取消回放
    {"read",      cmd_sisdb_read ,      SIS_METHOD_ACCESS_READ,  NULL},   // 从磁盘加载数据
    // 以下方法为数据集标配 即使没有最好也支持
    {"save",      cmd_sisdb_disk_save,  SIS_METHOD_ACCESS_ADMIN, NULL},   // 存盘
    {"pack",      cmd_sisdb_disk_pack,  SIS_METHOD_ACCESS_ADMIN, NULL},   // 合并整理数据
    {"open",      cmd_sisdb_open  ,     SIS_METHOD_ACCESS_NONET, NULL},   // 初始化信息
    {"close",     cmd_sisdb_close ,     SIS_METHOD_ACCESS_NONET, NULL},   // 关闭数据表
    {"rlog",      cmd_sisdb_rlog  ,     SIS_METHOD_ACCESS_NONET, NULL},   // 读入没有写盘的log信息
    {"wlog",      cmd_sisdb_wlog  ,     SIS_METHOD_ACCESS_NONET, NULL},   // 写入没有写盘的log信息
    {"clear",     cmd_sisdb_clear ,     SIS_METHOD_ACCESS_NONET, NULL},   // 停止某个客户的所有查询
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

bool sisdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_cxt *context = SIS_MALLOC(s_sisdb_cxt, context);
    worker->context = context;

    context->work_date = sis_time_get_idate(0); 
	context->work_name = sis_sdsnew(node->key);

    s_sis_json_node *sonnode = sis_json_cmp_child_node(node, "work-path");
    if (sonnode)
    {
        context->work_path = sis_sdsnew(sonnode->value);
    }
    // 默认4点存盘
    context->save_time = sis_json_get_int(node, "save-time", 40000);

    context->work_sub_cxt = sisdb_sub_cxt_create();

    // 数据集合
    context->work_keys = sis_map_pointer_create_v(sisdb_collect_destroy);

    // 加载本地的所有数据结构
    context->work_sdbs = sis_map_list_create(sisdb_table_destroy);

    sis_mutex_init(&(context->wlog_lock), NULL);

    context->status = 0;
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
    int itime = sis_time_get_itime(0);
    if (itime > context->save_time)
    {
        sis_mutex_lock(&context->wlog_lock);
        sisdb_wlog_close(context);
        sisdb_wlog_move(context);
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

int cmd_sisdb_create(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    s_sisdb_table *table = sis_map_list_get(context->work_sdbs, netmsg->key);
    if (!table)
    {
        s_sis_json_handle *handle = sis_json_open(netmsg->ask);
        if (handle)
        {
            s_sisdb_table *table = sisdb_table_create(handle->node);
            sis_map_list_set(context->work_sdbs, netmsg->key, table);
        }
        else
        {
            return SIS_METHOD_ERROR;
        }
    }
   sis_net_ans_with_ok(netmsg);
    return SIS_METHOD_OK;
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
int cmd_sisdb_hsub(void *worker_, void *argv_)
{
    // 只订阅最后一条记录 不开线程 
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;  
    int o = sisdb_sub_cxt_hsub(context->work_sub_cxt, netmsg);
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
int cmd_sisdb_read(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    s_sis_object *obj = sisdb_read_disk(context, netmsg);
    
    sis_net_ans_with_object(netmsg, obj);
    
    sis_object_destroy(obj);

    return SIS_METHOD_OK;
}

int cmd_sisdb_disk_save(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    sisdb_wlog_close(context);

    int o = sisdb_disk_save(context);  
    if (o == SIS_METHOD_OK)
    {
        // 存盘成功 可以清理老的log
        sisdb_wlog_move(context);
        sis_net_ans_with_ok(netmsg);
    }
    else
    {
        sis_net_ans_with_error(netmsg, "save fail.", 0);
    }
    return o;
}

int cmd_sisdb_disk_pack(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    int o = sisdb_disk_pack(context);
    if (o == SIS_METHOD_OK)
    {
        sis_net_ans_with_ok(netmsg);
    }
    else
    {
        sis_net_ans_with_error(netmsg, "pack fail.", 0);
    }
    return o;
}
int cmd_sisdb_open(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    
	context->cb_source = sis_message_get(msg, "cb_source");
	context->cb_net_message = sis_message_get_method(msg, "cb_net_message");

    {
        s_sis_sds str = sis_message_get_str(msg, "work-path");
        if (str)
        {
            sis_sdsfree(context->work_path);
            context->work_path = sis_sdsdup(str);
        }
    }
    {
        s_sis_sds str = sis_message_get_str(msg, "work-name");
        if (str)
        {
            sis_sdsfree(context->work_name);
            context->work_name = sis_sdsdup(str);
        }
    }
    if (sis_message_exist(msg, "work-date"))
    {
        context->work_date = sis_message_get_int(msg, "work-date");
    }
    // 先加载所有数据结构
    sisdb_read_sdbs(context);

    // 再加载 log 的数据 加载 log数据会读取对应磁盘的数据
    sisdb_rlog_read(worker);

    // 这里初始化 log 的写入服务
    s_sis_worker *service = sis_worker_create_of_conf(worker, context->work_name, "{classname:sisdb_flog}");
    if (service)
    {
        context->wlog_worker = service; 
        context->wlog_write = sis_worker_get_method(context->wlog_worker, "write");
    }     
    // 初始化订阅者
    sisdb_sub_cxt_init(context->work_sub_cxt, context->cb_source, context->cb_net_message);
    
    return SIS_METHOD_OK;
}
int cmd_sisdb_close(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    // 取消所有订阅
    sisdb_sub_cxt_unsub(context->work_sub_cxt, -1);
    // 关闭wlog
    sisdb_wlog_close(context);
    // 数据不清理 等到销毁时清理
    return SIS_METHOD_OK;
}

int cmd_sisdb_rlog(void *worker_, void *argv_)
{   
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    int o = sisdb_rlog_read(worker);
	return o;
}
int cmd_sisdb_wlog(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;

    sis_mutex_lock(&context->wlog_lock);
    if (!context->wlog_open)
    {
        sisdb_wlog_open(context);
    }
    int o = context->wlog_write->proc(context->wlog_worker, argv_);
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
        // 清理订阅的信息
        if (sis_message_exist(msg, "cid"))
        {
            sisdb_sub_cxt_unsub(context->work_sub_cxt, sis_message_get_int(msg, "cid"));
        }
	}
    return SIS_METHOD_OK;
}


