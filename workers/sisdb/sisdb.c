#include "sis_modules.h"
#include "worker.h"
#include "sis_method.h"

#include "sisdb.h"
#include "sisdb_io.h"
#include "sis_net.msg.h"
#include "sisdb_fmap.h"

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
    {"keys",      cmd_sisdb_keys,       SIS_METHOD_ACCESS_READ,  NULL},   // 获取 所有 keys 信息
    {"dels",      cmd_sisdb_dels,       SIS_METHOD_ACCESS_ADMIN, NULL},   // 删除多个数据

    {"sub",       cmd_sisdb_sub,        SIS_METHOD_ACCESS_READ,  NULL},   // 订阅数据 - 只发新写入的数据
    {"hsub",      cmd_sisdb_hsub,       SIS_METHOD_ACCESS_READ,  NULL},   // 订阅数据 - 头匹配 只发新写入的数据
    {"unsub",     cmd_sisdb_unsub,      SIS_METHOD_ACCESS_READ,  NULL},   // 取消订阅
    // 磁盘数据订阅
    {"psub",      cmd_sisdb_psub,       SIS_METHOD_ACCESS_READ,  NULL},   // 回放数据 需指定日期 支持头匹配 所有数据全部拿到按时间排序后一条一条返回 
    {"unpsub",    cmd_sisdb_unpsub,     SIS_METHOD_ACCESS_READ,  NULL},   // 取消回放
    {"read",      cmd_sisdb_read ,      SIS_METHOD_ACCESS_READ,  NULL},   // 从磁盘加载数据
    // 以下方法为数据集标配 即使没有最好也支持
    {"save",      cmd_sisdb_save,       SIS_METHOD_ACCESS_ADMIN|SIS_METHOD_ACCESS_NOLOG, NULL},   // 存盘
    {"pack",      cmd_sisdb_pack,       SIS_METHOD_ACCESS_ADMIN|SIS_METHOD_ACCESS_NOLOG, NULL},   // 合并整理数据
    {"init",      cmd_sisdb_init,       SIS_METHOD_ACCESS_NONET, NULL},
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
    // 默认4点存盘
    context->save_time = sis_json_get_int(node, "save-time", 40000);

    context->work_sub_cxt = sisdb_sub_cxt_create();

    // 数据集合
    context->work_path = sis_sds_save_create(sis_json_get_str(node, "work-path"), "data");   
    context->work_name = sis_sds_save_create(sis_json_get_str(node, "work-name"), node->key);   

    {
        s_sis_json_node *sonnode = sis_json_cmp_child_node(node, "safe-path");
        if (sonnode)
        {
            context->safe_path = sis_sdsnew(sonnode->value);
        }
        else
        {
            context->safe_path = sis_sdsnew("safe");
        }
    }
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
            sisdb_wlog_close(context);
        }
        sis_worker_destroy(context->wlog_worker);
		context->wlog_worker = NULL;
    }
    if (context->work_sub_cxt)
    {
        sisdb_sub_cxt_destroy(context->work_sub_cxt);
        context->work_sub_cxt = NULL;
    }
    if (context->work_fmap_cxt)
    {
        // int count = sis_map_list_getsize(context->work_fmap_cxt->work_sdbs);
        // printf("=== close dbs %d\n", count);
        // for (int i = 0; i < count; i++)
        // {
        //     s_sis_dynamic_db *db = (s_sis_dynamic_db *)sis_map_list_geti(context->work_fmap_cxt->work_sdbs, i);
        //     printf("=== close db %s\n", db->name);
        // }       
        sisdb_fmap_cxt_destroy(context->work_fmap_cxt);
        context->work_fmap_cxt =  NULL;
    }
    sis_sds_save_destroy(context->work_path);
    sis_sds_save_destroy(context->work_name);
    sis_sdsfree(context->safe_path);
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

    int idate = sis_time_get_idate(0);
    if (idate <= context->work_date)
    {
        return;
    }
    int itime = sis_time_get_itime(0);
    if (itime > context->save_time)
    {
        sisdb_disk_save_start(context);  
        // 这里要判断是否新的一天 如果是就存盘
        if (sisdb_disk_save(context) == SIS_METHOD_OK)
        {
        }
        sisdb_disk_save_stop(context);

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
    }
}

int cmd_sisdb_show(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    s_sis_sds sdbs = sisdb_fmap_cxt_get_sdbs(context->work_fmap_cxt, 1, 0);
    if (!sdbs)
    {
        return SIS_METHOD_NULL;
    }
    sis_net_message_set_char(netmsg, sdbs, sis_sdslen(sdbs));
    sis_sdsfree(sdbs);
    return SIS_METHOD_OK;
}

int cmd_sisdb_create(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    printf("create ... %s\n", netmsg->subject);
    s_sis_json_handle *argvs = sis_json_load(netmsg->info, netmsg->info ? sis_sdslen(netmsg->info) : 0); 
    if (!argvs)
    {
        sis_net_msg_tag_error(netmsg, "no create info.", 0);
        return SIS_METHOD_OK;
    }
    int  o = sisdb_io_create(context, netmsg->subject, argvs->node);
    sis_json_close(argvs);
    if(o == 0)
    {
        sis_net_msg_tag_ok(netmsg);
    }
    else if(o == 1)
    {
        sis_net_msg_tag_error(netmsg, "sdb already.", 0);
    }
    else if(o == -1)
    {
        sis_net_msg_tag_error(netmsg, "add sdb fail.", 0);
    }
    return SIS_METHOD_OK;
}


int cmd_sisdb_get(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    s_sis_sds  o = NULL;
    int rfmt = SISDB_FORMAT_ARRAY;
    SIS_NET_SHOW_MSG("get==", netmsg);
    s_sis_json_handle *argvs = sis_json_load(netmsg->info, netmsg->info ? sis_sdslen(netmsg->info) : 0);
    if (!argvs)
    {
        o = sisdb_io_get_chars_sds(context, netmsg->subject, rfmt, NULL);  
    }
    else
    {
        rfmt = sis_db_get_format_from_node(argvs->node, SISDB_FORMAT_ARRAY);
        if (rfmt & SISDB_FORMAT_BYTES)
        {
            o = sisdb_io_get_sds(context, netmsg->subject, argvs->node);    
        }
        else
        {
            o = sisdb_io_get_chars_sds(context, netmsg->subject, rfmt, argvs->node);    
        }
        sis_json_close(argvs);
    }
    // 发送数据
	if (o)
	{
        if(rfmt & SISDB_FORMAT_CHARS)
        {
            sis_net_message_set_char(netmsg, o, sis_sdslen(o));
        }
        else
        {
            sis_net_message_set_byte(netmsg, o, sis_sdslen(o));
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
    
    // 写入数据
    if (netmsg->subject)
    {
        int o = 0;
        if (sis_str_exist_ch(netmsg->subject, sis_sdslen(netmsg->subject), ".", 1))
        {
            o = sisdb_io_set_chars(context, netmsg->subject, netmsg->info);
        }
        else
        {
            o = sisdb_io_set_one_chars(context, netmsg->subject, netmsg->info);
        }
        // 这里处理订阅
        sisdb_sub_cxt_pub(context->work_sub_cxt, netmsg);
        sis_net_msg_tag_int(netmsg, o);
        return SIS_METHOD_OK;
    }
	return SIS_METHOD_ERROR;
}

int cmd_sisdb_bset(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    if (netmsg->subject)
    {
        int o = 0;
        if (sis_str_exist_ch(netmsg->subject, sis_sdslen(netmsg->subject), ".", 1))
        {
            o = sisdb_io_update(context, netmsg->subject, netmsg->info);
        }
        else
        {
            // 单键值需要转把二进制格式转字符型 暂时不支持
            // o = sisdb_io_set_one_bytes(context, netmsg->subject, netmsg->info);
        }
        // 这里处理订阅
        sisdb_sub_cxt_pub(context->work_sub_cxt, netmsg);
        sis_net_msg_tag_int(netmsg, o);
        return SIS_METHOD_OK;
    }
	return SIS_METHOD_ERROR;
}
int cmd_sisdb_del(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    s_sis_json_handle *argvs = sis_json_load(netmsg->info, sis_sdslen(netmsg->info));
    if (!argvs)
    {
        sis_net_msg_tag_error(netmsg, "no del info.", 0);
        return SIS_METHOD_OK;
    }
    int o = 0;
    if (sis_str_exist_ch(netmsg->subject, sis_sdslen(netmsg->subject), ".", 1))
    {
        o = sisdb_io_del(context, netmsg->subject, argvs->node);
    }
    else
    {
        // 单键值需要转把二进制格式转字符型 暂时不支持
        o = sisdb_io_del_one(context, netmsg->subject, argvs->node);
    }
    sis_json_close(argvs);
    sis_net_msg_tag_int(netmsg, o);
    return SIS_METHOD_OK;
}
int cmd_sisdb_drop(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    int  o = sisdb_io_drop(context, netmsg->subject);
    if(o == 0)
    {
        sis_net_msg_tag_int(netmsg, 1);
    }
    else if(o == -1)
    {
        sis_net_msg_tag_error(netmsg, "table is used.", 0);
    }
    else if(o == -2)
    {
        sis_net_msg_tag_error(netmsg, "no find table.", 0);
    }
    return SIS_METHOD_OK;
}
// 获取多个key数据 只返回字符串数据
int cmd_sisdb_gets(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    // 特指结构化数据 并且只返回最后一条记录
    if (netmsg->subject)
    {
        s_sis_sds o = NULL;
        if (sis_str_exist_ch(netmsg->subject, sis_sdslen(netmsg->subject), ".", 1))
        {
            s_sis_json_handle *argvs = sis_json_load(netmsg->info, sis_sdslen(netmsg->info));
            if (!argvs)
            {
                o = sisdb_io_gets_sds(context, netmsg->subject, NULL);  
            }
            else
            {
                o = sisdb_io_gets_sds(context, netmsg->subject, argvs->node);    
                sis_json_close(argvs);
            }
        }
        else
        {
            // 求单键
            o = sisdb_io_gets_one_sds(context, netmsg->subject);
        }     
        if (o)
        {
            sis_net_message_set_char(netmsg, o, sis_sdslen(o));
            sis_sdsfree(o);
            return SIS_METHOD_OK;
        }
    }
	return SIS_METHOD_NULL;
}
// 使用头匹配
int cmd_sisdb_keys(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    if (netmsg->subject)
    {
        s_sis_sds o = NULL;
        if (sis_str_exist_ch(netmsg->subject, sis_sdslen(netmsg->subject), ".", 1))
        {
            s_sis_json_handle *argvs = sis_json_load(netmsg->info, sis_sdslen(netmsg->info));
            if (!argvs)
            {
                o = sisdb_io_keys_sds(context, netmsg->subject, NULL);  
            }
            else
            {
                o = sisdb_io_keys_sds(context, netmsg->subject, argvs->node);    
                sis_json_close(argvs);
            }
        }
        else
        {
            // 求单键
            o = sisdb_io_keys_one_sds(context, netmsg->subject);
        }     
        if (o)
        {
            sis_net_message_set_char(netmsg, o, sis_sdslen(o));
            sis_sdsfree(o);
            return SIS_METHOD_OK;
        }
    }
	return SIS_METHOD_NULL;
}

// 模糊匹配 删除
int cmd_sisdb_dels(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    s_sis_json_handle *argvs = sis_json_load(netmsg->subject, sis_sdslen(netmsg->subject));
    if (!argvs)
    {
        sis_net_msg_tag_error(netmsg, "no dels info.", 0);
        return SIS_METHOD_OK;
    }
    int  o = 0;
    if (sis_str_exist_ch(netmsg->subject, sis_sdslen(netmsg->subject), ".", 1))
    {
        sisdb_io_dels(context, netmsg->subject, argvs->node);
    }
    else
    {
        sisdb_io_dels_one(context, netmsg->subject);
    }
    sis_json_close(argvs);
    sis_net_msg_tag_int(netmsg, o);
    return SIS_METHOD_OK;
}

int cmd_sisdb_sub(void *worker_, void *argv_)
{
    // 只订阅最后一条记录 不开线程 
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;  
    int o = sisdb_sub_cxt_sub(context->work_sub_cxt, netmsg);
    sis_net_msg_tag_int(netmsg, o);
    return SIS_METHOD_OK;
}
int cmd_sisdb_hsub(void *worker_, void *argv_)
{
    // 只订阅最后一条记录 不开线程 
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;  
    int o = sisdb_sub_cxt_hsub(context->work_sub_cxt, netmsg);
    sis_net_msg_tag_int(netmsg, o);
    return SIS_METHOD_OK;
}
int cmd_sisdb_unsub(void *worker_, void *argv_)
{
    // 只订阅最后一条记录 不开线程 
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    int o = sisdb_sub_cxt_unsub(context->work_sub_cxt, netmsg->cid);
    sis_net_msg_tag_int(netmsg, o);
    return SIS_METHOD_OK;
}
// 从磁盘中订阅 用线程
// 启动一个线程 新建一个订阅类 然后从磁盘读取数据 然后按时序排序后一条条发送给客户
// 适合回放磁盘中大数据 模拟历史真实环境 
int cmd_sisdb_psub(void *worker_, void *argv_)
{
    // s_sis_worker *worker = (s_sis_worker *)worker_; 
    // s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
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
    sis_net_msg_tag_int(netmsg, o);
    return SIS_METHOD_OK;
}
// 取消磁盘订阅
int cmd_sisdb_unpsub(void *worker_, void *argv_)
{
    // 只订阅最后一条记录 不开线程 
    // s_sis_worker *worker = (s_sis_worker *)worker_; 
    // s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
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
    sis_net_msg_tag_int(netmsg, o);
    return SIS_METHOD_OK;
}
// 直接从磁盘读取数据
int cmd_sisdb_read(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    s_sis_object *obj = sisdb_disk_read(context, netmsg);
    // sis_net_message_set_tag(netmsg, 0);
    // sis_net_message_set_object(netmsg, obj, 0);
    
    sis_object_destroy(obj);

    return SIS_METHOD_OK;
}

int cmd_sisdb_save(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    // 先关闭 log 然后转移log文件 然后再打开新的log 
    // 并设置标记 此时只接收数据 等待save结束
    // printf("====%d\n", context->work_fmap_cxt->sdbs_writed);
    sisdb_disk_save_start(context);   
    int o = sisdb_disk_save(context);  
    if (o == SIS_METHOD_OK)
    {
        // 存盘成功 可以清理老的log
        sis_net_msg_tag_ok(netmsg);
    }
    else
    {
        sis_net_msg_tag_error(netmsg, "save fail.", 0);
    }
    sisdb_disk_save_stop(context);    
    return o;
}

int cmd_sisdb_pack(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    int o = sisdb_disk_pack(context);
    if (o == SIS_METHOD_OK)
    {
        sis_net_msg_tag_ok(netmsg);
    }
    else
    {
        sis_net_msg_tag_error(netmsg, "pack fail.", 0);
    }
    return o;
}
int cmd_sisdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    if (context->work_fmap_cxt)
    {
        sisdb_fmap_cxt_destroy(context->work_fmap_cxt);
    }
    sis_sds_save_set(context->work_path, sis_message_get_str(msg, "work-path"));
    printf("init ====== sisdb\n");
    context->work_fmap_cxt = sisdb_fmap_cxt_create(
        sis_sds_save_get(context->work_path), 
        sis_sds_save_get(context->work_name));

	context->cb_source = sis_message_get(msg, "cb_source");
	context->cb_net_message = sis_message_get_method(msg, "cb_net_message");

    // 先加载所有数据结构
    sisdb_fmap_cxt_init(context->work_fmap_cxt);

    // 再加载 log 的数据 加载 log数据会读取对应磁盘的数据
    sisdb_rlog_read(worker);

    // 这里初始化 log 的写入服务
    s_sis_worker *service = sis_worker_create_of_conf(worker, 
        sis_sds_save_get(context->work_name), "{classname:sisdb_flog}");
    if (service)
    {
        context->wlog_worker = service; 
        context->wlog_write = sis_worker_get_method(context->wlog_worker, "write");
    }     
    // 初始化订阅者
    sisdb_sub_cxt_init(context->work_sub_cxt, context->cb_source, context->cb_net_message);

    return SIS_METHOD_OK;
}
int cmd_sisdb_open(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_cxt *context = (s_sisdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;    
    if (sis_message_exist(msg, "work-date"))
    {
        context->work_date = sis_message_get_int(msg, "work-date");
    }
    
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
    // printf("---%p %p\n", context->wlog_worker, context->wlog_write);
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
    if (sis_strcasecmp(mode, "memory"))
	{
        // 清理所有内存数据
	}
    return SIS_METHOD_OK;
}


