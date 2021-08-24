#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"

#include "sis_message.h"
#include "sisdb_server.h"
#include "sisdb_collect.h"

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sisdb_server_methods[] = {
    {"auth",     cmd_sisdb_server_auth,    SIS_METHOD_ACCESS_READ,  NULL},   // 用户登录
    {"show",     cmd_sisdb_server_show,    SIS_METHOD_ACCESS_READ,  NULL},   // 显示有多少数据集

    {"setuser",  cmd_sisdb_server_setuser, SIS_METHOD_ACCESS_ADMIN, NULL}, // 设置用户信息

    {"open",     cmd_sisdb_server_open,    SIS_METHOD_ACCESS_ADMIN, NULL},   // 打开一个数据集
    {"close",    cmd_sisdb_server_close,   SIS_METHOD_ACCESS_ADMIN, NULL},   // 关闭一个数据集
    {"save",     cmd_sisdb_server_save,    SIS_METHOD_ACCESS_ADMIN, NULL},   // 手动存盘
    {"pack",     cmd_sisdb_server_pack,    SIS_METHOD_ACCESS_ADMIN, NULL},   // 手动清理磁盘旧的数据
    {"call",     cmd_sisdb_server_call,    SIS_METHOD_ACCESS_READ,  NULL},   // 直接调用系统定义好的扩展方法
};
// 共享内存数据库
s_sis_modules sis_modules_sisdb_server = {
    sisdb_server_init,
    NULL,
    NULL,
    NULL,
    sisdb_server_uninit,
    sisdb_server_method_init,
    sisdb_server_method_uninit,
    sizeof(sisdb_server_methods) / sizeof(s_sis_method),
    sisdb_server_methods,
};

static int  cb_reader_recv(void *worker_, s_sis_object *in_);
static void cb_socket_recv(void *worker_, s_sis_net_message *msg);

static void _cb_connect_open(void *worker_, int sid)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
	sis_net_class_set_cb(context->socket, sid, worker, cb_socket_recv);
    LOG(5)("client connect ok. [%d]\n", sid);
}
static void _cb_connect_close(void *worker_, int sid)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    // 清理客户信息
    if (context->user_access)
    {
        sis_map_kint_del(context->user_access, sid);
    }
    s_sis_message *msg = (s_sis_message *)sis_message_create();
    sis_message_set_str(msg, "mode", "reader", 6);
    sis_message_set_int(msg, "cid", sid);
    // 向所有工作进程发布信息 清理该客户
    int count = sis_map_list_getsize(context->works);
    for (int i = 0; i < count; i++)
    {
        s_sisdb_workinfo *workinfo = (s_sisdb_workinfo *)sis_map_list_geti(context->works, i);
        sis_worker_command(workinfo->worker, "clear", msg);
    }
    sis_message_destroy(msg);
    LOG(5)("client disconnect. [%d]\n", sid);	
}
// 所有子数据集通过网络主动返回的数据都在这里返回
static int cb_net_message(void *context_, void *argv_)
{
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)context_;
    sis_net_class_send(context->socket, (s_sis_net_message *)argv_);
    return 0;
}
// 从初始化文件中加载的网络包
// 未指定 service 且server 没有对应cmd 默认 service 为 sisdb
static int cb_sys_init_scripts(void *worker_, s_sis_json_node *node)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    // s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;

    s_sis_net_message *netmsg = sis_net_message_create();
    sis_json_to_netmsg(node, netmsg); // node 转 
    if (netmsg->service)
    {
        s_sis_worker *service = sis_worker_get(worker, netmsg->service);
        if (service)
        {
            sis_worker_command(service, netmsg->cmd, netmsg);
        }
    }
    else
    {
        sis_worker_command(worker, netmsg->cmd, netmsg);
    }
    sis_net_message_destroy(netmsg);
    return 0;
}

bool sisdb_server_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_server_cxt *context = SIS_MALLOC(s_sisdb_server_cxt, context);
    worker->context = context;

    context->level = sis_json_get_int(node, "level", 10);

    context->socket_recv = sis_lock_list_create(16 * 1024 * 1024);
    context->reader_recv = sis_lock_reader_create(context->socket_recv, 
        SIS_UNLOCK_READER_HEAD, worker, cb_reader_recv, NULL);
    sis_lock_reader_open(context->reader_recv);

    context->users = sis_map_list_create(sis_userinfo_destroy);
    context->works = sis_map_list_create(sis_workinfo_destroy);
    
    s_sis_message *msg = sis_message_create();
    if (sisdb_server_sysinfo_load(worker))
    {
        // 没有初始化 需要加载脚本
        const char *initname = sis_json_get_str(node, "init-scripts");
        if (initname && sis_file_exists(initname))
        {
            sis_conf_sub(initname, worker, cb_sys_init_scripts);
        }
    }
    if (sis_map_list_getsize(context->works) < 1)
    {
        // 最差也要启动一个空的数据集
        s_sis_net_message *netmsg = sis_net_message_create();
        netmsg->switchs.is_inside = 1;  // 仅仅内部传递
        netmsg->key = sis_sdsnew("sisdb");
        netmsg->ask = sis_sdsnew("{\"save-time\":40000,\"classname\":\"sisdb\"}");
        cmd_sisdb_server_open(worker, netmsg);
        sis_net_message_destroy(netmsg);
    }
    if (sis_map_list_getsize(context->works) < 1)
    {
        LOG(5)("no worker.\n");
        sis_message_destroy(msg);
        return false;
    }
    sis_message_destroy(msg);
    // 数据集合打开时需要加载 当日log数据 各数据集自行管理数据       

    // 到这里判断是否需要验证用户
    if (sis_map_list_getsize(context->users) > 0)
    {
        context->user_access = sis_map_kint_create();
    }

    s_sis_json_node *srvnode = sis_json_cmp_child_node(node, "server");
    s_sis_url  url;
    memset(&url, 0, sizeof(s_sis_url));
    if (sis_url_load(srvnode, &url))
    {
        url.io = SIS_NET_IO_WAITCNT;
        url.role = SIS_NET_ROLE_ANSWER;
        context->socket = sis_net_class_create(&url);
        context->socket->cb_source = worker;
        context->socket->cb_connected = _cb_connect_open;
        context->socket->cb_disconnect = _cb_connect_close;
    }
    // 打开网络
    if (!sis_net_class_open(context->socket))
    {
        return false;
    }
    
    context->status = SISDB_STATUS_WORK;

    return true;
}

void sisdb_server_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;

    context->status = SISDB_STATUS_EXIT;

    if (context->socket)
    {
        sis_net_class_destroy(context->socket);
    }
    if (context->user_access)
    {
        sis_map_kint_destroy(context->user_access);
    }
	sis_lock_reader_close(context->reader_recv);
    sis_lock_list_destroy(context->socket_recv);
    if (context->users)
    {
    	sis_worker_destroy(context->users);
    }
    if (context->works)
    {
    	sis_worker_destroy(context->works);
    }

    sis_free(context);
    worker->context = NULL;
}

static void cb_socket_recv(void *worker_, s_sis_net_message *msg)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
	
    // printf("recv query: [%d] %d : %s %s %s %s\n %s\n", msg->cid, msg->style, 
    //     msg->serial ? msg->serial : "nil",
    //     msg->cmd ? msg->cmd : "nil",
    //     msg->key ? msg->key : "nil",
    //     msg->val ? msg->val : "nil",
    //     msg->rval ? msg->rval : "nil");

    sis_net_message_incr(msg);
    s_sis_object *obj = sis_object_create(SIS_OBJECT_NETMSG, msg);
    sis_lock_list_push(context->socket_recv, obj);
	sis_object_destroy(obj);
	
}
void sisdb_server_reply_error(s_sisdb_server_cxt *context, s_sis_net_message *netmsg)
{
    s_sis_sds reply = sis_sdsnew("method call fail ");
    if (netmsg->cmd)
    {
        reply = sis_sdscatfmt(reply, "[%s].", netmsg->cmd);
    }
    sis_net_ans_with_error(netmsg, reply, sis_sdslen(reply));
    sis_net_class_send(context->socket, netmsg);
    sis_sdsfree(reply);
}
void sisdb_server_reply_null(s_sisdb_server_cxt *context, s_sis_net_message *netmsg, const char *name)
{
    s_sis_sds reply = sis_sdsnew("null of ");
    if (name)
    {
        reply = sis_sdscatfmt(reply, "[%s].", name);
    }    
    sis_net_ans_with_error(netmsg, reply, sis_sdslen(reply));
    sis_net_class_send(context->socket, netmsg);
    sis_sdsfree(reply);
}
void sisdb_server_reply_no_method(s_sisdb_server_cxt *context, s_sis_net_message *netmsg, const char *name)
{
    s_sis_sds reply = sis_sdsnew("no find method ");
    if (name)
    {
        reply = sis_sdscatfmt(reply, "[%s].", name);
    }
    sis_net_ans_with_error(netmsg, reply, sis_sdslen(reply));
    sis_net_class_send(context->socket, netmsg);
    sis_sdsfree(reply);
}
void sisdb_server_reply_no_service(s_sisdb_server_cxt *context, s_sis_net_message *netmsg, const char *name)
{
    s_sis_sds reply = sis_sdsnew("no find service ");
    if (name)
    {
        reply = sis_sdscatfmt(reply, "[%s].", name);
    }    
    sis_net_ans_with_error(netmsg, reply, sis_sdslen(reply));
    sis_net_class_send(context->socket, netmsg);
    sis_sdsfree(reply);
}
void sisdb_server_send_service(s_sis_worker *worker, const char *workname, const char *cmdname, s_sis_net_message *netmsg)
{
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;

    if (!workname)
    {
        int o = sis_worker_command(worker, cmdname, netmsg);
        if (o == SIS_METHOD_OK)
        {
            sis_net_class_send(context->socket, netmsg);
        }  
        else if (o == SIS_METHOD_NULL)
        {
            sisdb_server_reply_null(context, netmsg, cmdname);
        }
        else if (o == SIS_METHOD_NOCMD)
        {
            sisdb_server_reply_no_method(context, netmsg, cmdname);
        }
        else // if (o == SIS_METHOD_ERROR)
        {
            sisdb_server_reply_error(context, netmsg);
        }
    }
    else
    {
        s_sisdb_workinfo *workinfo = sis_map_list_get(context->works, workname);
        if (workinfo)
        {
            int o = sis_worker_command(workinfo->worker, cmdname, netmsg);
            if (o == SIS_METHOD_OK)
            {
                sis_net_class_send(context->socket, netmsg);
            } 
            else if (o == SIS_METHOD_NULL)
            {
                sisdb_server_reply_null(context, netmsg, netmsg->key);
            }
            else if (o == SIS_METHOD_NOCMD)
            {
                sisdb_server_reply_no_method(context, netmsg, cmdname);
            }
            else // if (o == SIS_METHOD_ERROR)
            {
                sisdb_server_reply_error(context, netmsg);
            }
        } 
        else
        {
            sisdb_server_reply_no_service(context, netmsg, workname);
        }               
    }  
}

// 返回值 -1 为没有权限
int sisdb_server_get_access(s_sisdb_server_cxt *context, s_sis_net_message *netmsg)
{
    if (!context->user_access)
    {
        return SIS_METHOD_ACCESS_ADMIN;
    }
    s_sisdb_userinfo *info = sis_map_kint_get(context->user_access, netmsg->cid);
    if (!info)
    {
        return -1;
    }
    return info->access;
}

static int cb_reader_recv(void *worker_, s_sis_object *in_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_net_message *netmsg = SIS_OBJ_NETMSG(in_);

    sis_net_message_incr(netmsg);
    
    int access = sisdb_server_get_access(context, netmsg);
    if ( access < 0 && sis_strcasecmp("auth", netmsg->cmd))
    {
        sis_net_ans_with_error(netmsg, "no auth.", 8);
        sis_net_class_send(context->socket, netmsg);
    }
    else
    {
        // 先写log再处理
        bool ismake = true; 
        if (netmsg->service)
        {
            s_sisdb_workinfo *workinfo = sis_map_list_get(context->works, netmsg->service);
            if (workinfo)
            {
                s_sis_method *method = sis_worker_get_method(workinfo->worker, netmsg->cmd);
                if (!method)
                {
                    ismake = false;
                }
                else
                {
                    if (!sis_sys_access_method(method, access))
                    {
                        // 当前用户是否有权限
                        // 从网络过来的数据包 只能调用支持网络数据包的方法
                        ismake = false;
                    }
                    else
                    {
                        // 指令是否为写入
                        if (method->access & SIS_METHOD_ACCESS_WRITE)
                        {
                            // printf("wlog: %s %s\n", netmsg->key, netmsg->cmd);
                            // 只记录写盘的数据
                            sis_worker_command(workinfo->worker, "wlog", netmsg);
                        }
                    }
                }
            }
            else
            {
                ismake = false;
            }
        }
        else
        {
            s_sis_method *method = sis_worker_get_method(worker, netmsg->cmd);
            if (!method || !sis_sys_access_method(method, access))
            {
                ismake = false;
            }
            // printf("1 access = %d %d| %d %s\n", access, method->access, make, netmsg->cmd);
        }
        
        if (ismake)
        {
            // 此时已经写完log 再开始处理
            sisdb_server_send_service(worker, netmsg->service, netmsg->cmd, netmsg);
        }
        else
        {
            sisdb_server_reply_no_method(context, netmsg, netmsg->cmd);
            sis_net_class_send(context->socket, netmsg);
        }
    }
	sis_net_message_decr(netmsg);
	return 0;
}

void sisdb_server_method_init(void *worker_)
{
}
void sisdb_server_method_uninit(void *worker_)
{
}
//////////////////////////////////

int cmd_sisdb_server_auth(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    if (!context->user_access)
    {
        sis_net_ans_with_ok(netmsg);
        return SIS_METHOD_OK;
    }
    s_sis_json_handle *handle = sis_json_load(netmsg->ask, sis_sdslen(netmsg->ask));
    if (!handle)
    {
        sis_net_ans_with_error(netmsg, "auth ask error.", 15);
    }
    else
    {
        int ok = 1;
        const char *username = sis_json_get_str(handle->node, "username");
        const char *password = sis_json_get_str(handle->node, "password");
        s_sisdb_userinfo *userinfo = sis_map_list_get(context->users, username);
        if (!userinfo)
        {
            ok = 0;
        }
        else if (sis_strcasecmp(userinfo->password, password))
        {
            ok = 0;
        }
        else
        {
            sis_map_kint_set(context->user_access, netmsg->cid, userinfo);
        }
        if (ok)
        {
            sis_net_ans_with_ok(netmsg);
        }
        else
        {
            sis_net_ans_with_error(netmsg, "auth fail.", 10);
        }
    }
    return SIS_METHOD_OK;
}

int cmd_sisdb_server_show(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
        
    s_sis_sds sdbs = NULL;

    int count = sis_map_list_getsize(context->works);
    for (int i = 0; i < count; i++)
    {
        s_sisdb_workinfo *workinfo = sis_map_list_geti(context->works, i);
        if (!sdbs)
        {
            sdbs = sis_sdsdup(workinfo->workname);
        }
        else
        {
            sdbs = sis_sdscatfmt(sdbs, ",%S", workinfo->workname);
        }
    }  
    sis_net_ans_with_chars(netmsg, sdbs, sis_sdslen(sdbs));
    sis_sdsfree(sdbs);
    return SIS_METHOD_OK;
}

// 增加一个用户
int cmd_sisdb_server_setuser(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    s_sis_json_handle *handle = sis_json_load(netmsg->ask, sis_sdslen(netmsg->ask));
    if (!handle)
    {
        sis_net_ans_with_error(netmsg, "auth ask error.", 15);
    }
    else
    {
        int ok = 1;
        const char *username = sis_json_get_str(handle->node, "username");
        const char *password = sis_json_get_str(handle->node, "password");
        int iaccess = sis_sys_access_atoi(sis_json_get_str(handle->node, "access"));
        if (!context->user_access)
        {
            context->user_access = sis_map_kint_create();
        }
        s_sisdb_userinfo *userinfo = sis_map_list_get(context->users, username);
        if (!userinfo)
        {
            s_sisdb_userinfo *userinfo = sis_userinfo_create(
                username, password, iaccess);
            sis_map_list_set(context->users, username, userinfo); 
            sisdb_server_sysinfo_save(worker);
        }
        else if (sis_strcasecmp(userinfo->password, password))
        {
            ok = 0;
        }
        else
        {
            if (userinfo->access != iaccess)
            {
                userinfo->access = iaccess;
                sisdb_server_sysinfo_save(worker);
            }
            sis_map_kint_set(context->user_access, netmsg->cid, userinfo);
        }
        if (ok)
        {
            sis_net_ans_with_ok(netmsg);
        }
        else
        {
            sis_net_ans_with_error(netmsg, "auth fail.", 10);
        }
    }
    return SIS_METHOD_OK;
}

// // ??? 这里如果文件没有返回错误的情况下 初始就启动不了server 
// // 读取sno文件时，如果客户端中断 服务端会偶尔崩溃 怀疑是数据未清理 检查一下
// // 订阅sno时，会有几秒等待时间 查一下
// int _sisdb_server_load(s_sisdb_server_cxt *context)
// {
//     // 先根据规则加载磁盘数据
//     s_sis_message *msg = sis_message_create();
//     int count = sis_map_list_getsize(context->datasets);
//     for (int i = 0; i < count; i++)
//     {
//         s_sis_worker *service = (s_sis_worker *)sis_map_list_geti(context->datasets, i);
//         sis_message_set(msg, "config", &context->catch_cfg, NULL);
//         sis_worker_command(service, "rdisk", msg);
//     }
//     // 再加载wlog中的数据
//     for (int i = 0; i < count; i++)
//     {
//         s_sis_worker *service = (s_sis_worker *)sis_map_list_geti(context->datasets, i);
//         if (sis_worker_command(service, "rlog", msg) != SIS_METHOD_OK)
//         {
//         }
//     }
//     sis_message_destroy(msg);
//     return SIS_METHOD_OK;
// }
int cmd_sisdb_server_open(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    sis_message_set(netmsg, "cb_source", context, NULL);
    sis_message_set_method(netmsg, "cb_net_message", cb_net_message);

    int oks = 0;
    const char *workname = (const char *)netmsg->key;
    if (!workname)
    {
        // 打开所有工作
        int count = sis_map_list_getsize(context->works);
        for (int i = 0; i < count; i++)
        {
            s_sisdb_workinfo *workinfo = sis_map_list_geti(context->works, i);
            if (workinfo->work_status == 0)
            {
                sis_worker_command(workinfo->worker, "open", netmsg);
                workinfo->work_status = 1;
            }
            oks++;
        }  
    }
    else
    {
        s_sisdb_workinfo *workinfo = sis_map_list_get(context->works, workname);
        if (!workinfo)
        {
            s_sis_worker *worker = NULL;
            s_sis_json_node *cfgnode = NULL;
            s_sis_json_handle *handle = sis_json_load(netmsg->ask, sis_sdslen(netmsg->ask));
            if (handle)
            {
                worker = sis_worker_create_of_name(worker, workname, handle->node);
                cfgnode = sis_json_clone(handle->node, 1);
                sis_json_close(handle);
            }
            else
            {
                worker = sis_worker_create_of_name(worker, workname, NULL);
            }

            if (worker)
            {
                s_sisdb_workinfo *workinfo = SIS_MALLOC(s_sisdb_workinfo, workinfo);
                workinfo->config = cfgnode;
                workinfo->worker = worker;
                workinfo->workname = sis_sdsnew(workname);
                sis_map_list_set(context->works, workname, workinfo);

            }
            else
            {
                sis_json_delete_node(cfgnode);
                return SIS_METHOD_ERROR;
            }
            sisdb_server_sysinfo_save(worker);
            
        }
        if (workinfo->work_status == 0)
        {
            sis_worker_command(workinfo->worker, "open", netmsg);
            workinfo->work_status = 1;
        }
        oks++;
    }
    sis_net_ans_with_int(netmsg, oks); 
    return SIS_METHOD_OK;
}

int cmd_sisdb_server_close(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    
    int oks = 0;
    const char *workname = (const char *)netmsg->key;
    if (!workname)
    {
        // 停止所有工作
        int count = sis_map_list_getsize(context->works);
        for (int i = 0; i < count; i++)
        {
            s_sisdb_workinfo *workinfo = sis_map_list_geti(context->works, i);
            if (workinfo->work_status == 1)
            {
                sis_worker_command(workinfo->worker, "close", netmsg);
                workinfo->work_status = 0;
            }
            oks++;
        }  
    }
    else
    {
        s_sisdb_workinfo *workinfo = sis_map_list_get(context->works, workname);
        if (workinfo)
        {
            if (workinfo->work_status == 1)
            {
                sis_worker_command(workinfo->worker, "close", netmsg);
                workinfo->work_status = 0;
            }
            oks++;
        }
    }
    sis_net_ans_with_int(netmsg, oks); 
    return SIS_METHOD_OK;
}

int cmd_sisdb_server_save(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    int oks = 0;
    s_sis_message *msg = sis_message_create();
    int count = sis_map_list_getsize(context->works);
    for (int i = 0; i < count; i++)
    {
        s_sisdb_workinfo *workinfo = sis_map_list_geti(context->works, i);
        if (sis_worker_command(workinfo->worker, "save", msg) == SIS_METHOD_OK)
        {
            oks++;
        }
    }
    sis_message_destroy(msg);
    sis_net_ans_with_int(netmsg, oks); 
    return SIS_METHOD_OK;
} 

int cmd_sisdb_server_pack(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    int oks = 0;
    s_sis_message *msg = sis_message_create();
    int count = sis_map_list_getsize(context->works);
    for (int i = 0; i < count; i++)
    {
        s_sisdb_workinfo *workinfo = sis_map_list_geti(context->works, i);
        if (sis_worker_command(workinfo->worker, "pack", msg) == SIS_METHOD_OK)
        {
            oks++;
        }
    }
    sis_message_destroy(msg);
    sis_net_ans_with_int(netmsg, oks); 

    return SIS_METHOD_OK;
}

int cmd_sisdb_server_call(void *worker_, void *argv_)
{
    // 调用其他扩展方法 只能返回json格式
    // s_sis_worker *worker = (s_sis_worker *)worker_; 
    // s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    // s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
        
    // s_sis_sds o = NULL;
    // s_sis_dict_entry *de;
    // s_sis_dict_iter *di = sis_dict_get_iter(context->datasets->map);
    // while ((de = sis_dict_next(di)) != NULL)
    // {
    //     if (!sdbs)
    //     {
    //         sdbs = sis_sdsnew((char *)sis_dict_getkey(de));
    //     }
    //     else
    //     {
    //         sdbs = sis_sdscatfmt(sdbs, ",%s", (char *)sis_dict_getkey(de));
    //     }
    // }
    // sis_dict_iter_free(di);

    // sis_net_ans_with_chars(netmsg, sdbs, sis_sdslen(sdbs));

    // sis_sdsfree(sdbs);
    return SIS_METHOD_OK;
}


void sisdb_server_sysinfo_save(s_sis_worker *worker)
{
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
	s_sis_json_node *jone = sis_json_create_object();
	s_sis_json_node *jusers = sis_json_create_object();
    s_sis_json_node *jworks = sis_json_create_object();
    {
        int count = sis_map_list_getsize(context->users);
        for (int i = 0; i < count; i++)
        {
            s_sisdb_userinfo *userinfo = sis_map_list_geti(context->users, i);
            s_sis_json_node *juser = sis_json_create_object();
            sis_json_object_add_string(juser, "password", userinfo->password, sis_sdslen(userinfo->password));
            const char *access = sis_sys_access_itoa(userinfo->access);
            sis_json_object_add_string(juser, "access", access, sis_strlen(access));
            sis_json_object_add_node(jusers, userinfo->username, juser);
        }  
    }
	sis_json_object_add_node(jone, "userinfos", jusers); 
    {
        int count = sis_map_list_getsize(context->works);
        for (int i = 0; i < count; i++)
        {
            s_sisdb_workinfo *workinfo = sis_map_list_geti(context->works, i);
            if (workinfo->config)
            {
                sis_json_object_add_node(jworks, workinfo->workname, workinfo->config);
            }
        }  
    }
    sis_json_object_add_node(jone, "workinfos", jworks);
    
    sis_json_save(jone, "./.sissys.json");

	sis_json_delete_node(jone);	

}
int sisdb_server_sysinfo_load(s_sis_worker *worker)
{
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_json_handle *handle = sis_json_open("./.sissys.json");
    if (!handle)
    {
        // 没有配置
        return -1;
    }
    s_sis_json_node *node = handle->node;
    s_sis_json_node *userinfos = sis_json_cmp_child_node(node, "userinfos");
    if(userinfos) 
    {
        s_sis_json_node *next = sis_json_first_node(userinfos);
        while(next)
        {
            int iaccess = sis_sys_access_atoi(sis_json_get_str(next,"access"));
            s_sisdb_userinfo *userinfo = sis_userinfo_create(
                next->key, 
                sis_json_get_str(next,"password"), 
                iaccess);
            sis_map_list_set(context->users, userinfo->username, userinfo);
            next = next->next;
        }
    } 
    s_sis_json_node *workinfos = sis_json_cmp_child_node(node, "workinfos");
    if(workinfos) 
    {
        s_sis_json_node *next = sis_json_first_node(workinfos);
        while(next)
        {
            s_sisdb_workinfo *workinfo = sis_workinfo_create(next);
            sis_map_list_set(context->works, next->key, workinfo);
            next = next->next;
        }
    }  
    sis_json_close(handle);
    return 0; 
}