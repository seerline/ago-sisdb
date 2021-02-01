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
    {"auth",     cmd_sisdb_server_auth, SIS_METHOD_ACCESS_READ,  NULL},   // 用户登录
    {"show",     cmd_sisdb_server_show, SIS_METHOD_ACCESS_READ,  NULL},   // 显示有多少数据集
    {"save",     cmd_sisdb_server_save, SIS_METHOD_ACCESS_ADMIN, NULL},   // 手动存盘
    {"pack",     cmd_sisdb_server_pack, SIS_METHOD_ACCESS_ADMIN, NULL},   // 手动清理磁盘旧的数据
    {"wget",     cmd_sisdb_server_wget, SIS_METHOD_ACCESS_READ,  NULL},   // get 后是否写log文件 方便查看信息
    {"call",     cmd_sisdb_server_call, SIS_METHOD_ACCESS_READ,  NULL},   // 用于不同数据表之间关联计算的用途，留出其他语言加载的接口
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
static int  cb_reader_convert(void *worker_, s_sis_object *in_);
static void cb_socket_recv(void *worker_, s_sis_net_message *msg);

static void _cb_connected(void *worker_, int sid)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
	sis_net_class_set_cb(context->socket, sid, worker, cb_socket_recv);
    LOG(5)("client connect ok. [%d]\n", sid);
}
static void _cb_disconnect(void *worker_, int sid)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
	// sis_net_class_set_cb(context->socket, sid, worker, NULL);
    // 清理客户信息
    sisdb_server_decr_auth(context, sid);
    
    _sisdb_server_stop(context, sid);
    // 向所有表发布信息 停止工作
    LOG(5)("client disconnect. [%d]\n", sid);	
}
// 所有子数据集通过网络主动返回的数据都在这里返回
static int cb_net_message(void *context_, void *argv_)
{
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)context_;
    sis_net_class_send(context->socket, (s_sis_net_message *)argv_);
    return 0;
}
bool sisdb_server_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_server_cxt *context = SIS_MALLOC(s_sisdb_server_cxt, context);
    worker->context = context;

    context->level = sis_json_get_int(node, "level", 10);

    context->recv_list = sis_lock_list_create(16 * 1024 * 1024);
    context->reader_recv = sis_lock_reader_create(context->recv_list, 
        SIS_UNLOCK_READER_HEAD, worker, cb_reader_recv, NULL);
    sis_lock_reader_open(context->reader_recv);

    s_sis_json_node *authnode = sis_json_cmp_child_node(node, "auth");
    if (authnode)
    {
        context->user_auth = sis_map_pointer_create_v(sis_free_call);
        context->user_access = sis_map_pointer_create(); // 不用释放
        s_sis_json_node *next = sis_json_first_node(authnode);
        while(next)
        {
            s_sisdb_userinfo *userinfo = SIS_MALLOC(s_sisdb_userinfo, userinfo);
            const char *access = sis_json_get_str(next,"access");
            userinfo->access = SIS_METHOD_ACCESS_READ;
            if (!sis_strcasecmp(access, "write"))
            {
                userinfo->access = SIS_METHOD_ACCESS_RDWR;
            }
            if (!sis_strcasecmp(access, "admin"))
            {
                userinfo->access = SIS_METHOD_ACCESS_ADMIN;
            }
            sis_strcpy(userinfo->username, 32, sis_json_get_str(next,"username"));
            sis_strcpy(userinfo->password, 32, sis_json_get_str(next,"password"));
            if (!userinfo->username[0])
            {
                sis_strcpy(userinfo->username, 32, "guest");
                sis_strcpy(userinfo->password, 32, "guest1234");
                // 如果不设置用户名就默认 guest 登录
            }
            sis_map_pointer_set(context->user_auth, userinfo->username, userinfo);
            next = next->next;
        } 
    }    

    s_sis_json_node *dsnode = sis_json_cmp_child_node(node, "datasets");
    if (dsnode)
    {
        context->datasets = sis_map_list_create(sis_worker_destroy);
        s_sis_json_node *next = sis_json_first_node(dsnode);
        while(next)
        {
            s_sis_worker *service = sis_worker_create(NULL, next);
            if (service)
            {
                s_sis_message *msg = sis_message_create();
                sis_message_set(msg, "cb_source", context, NULL);
                sis_message_set_method(msg, "cb_net_message", cb_net_message);
                sis_worker_command(service, "init", msg);
                sis_message_destroy(msg);
                sis_map_list_set(context->datasets, next->key, service);
            }
            next = next->next;
        }  
    }
    s_sis_json_node *cvnode = sis_json_cmp_child_node(node, "converts");
    if (cvnode)
    {
        context->converts = sis_map_pointer_create_v(sis_pointer_list_destroy);
        sisdb_convert_init(context, cvnode);
        context->reader_convert = sis_lock_reader_create(context->recv_list, 
            SIS_UNLOCK_READER_HEAD, worker, cb_reader_convert, NULL);
        sis_lock_reader_open(context->reader_convert);
    }

    // 数据集初始化和服务启动完毕，这里开始加载数据
    // 应该需要判断数据的版本号，如果不同，应该对磁盘上的数据进行数据字段重新匹配
    s_sis_json_node *catchcfg = sis_json_cmp_child_node(node, "catchcfg");
    if (catchcfg)
    {
        context->catch_cfg.last_day = sis_json_get_int(catchcfg,"last_day", 0);
        context->catch_cfg.last_min = sis_json_get_int(catchcfg,"last_min", 0);
        context->catch_cfg.last_sec = sis_json_get_int(catchcfg,"last_sec", 0);
        context->catch_cfg.last_sno = sis_json_get_int(catchcfg,"last_sno", 0);
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
        context->socket->cb_connected = _cb_connected;
        context->socket->cb_disconnect = _cb_disconnect;
    }

    // 加载log中的当日数据
    if (_sisdb_server_load(context) != SIS_METHOD_OK)
    {
        // printf("...load fail\n");
        return false;
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
    if (context->datasets)
    {
        sis_map_list_destroy(context->datasets);
    }
    if (context->user_auth)
    {
        sis_map_pointer_destroy(context->user_access);
        sis_map_pointer_destroy(context->user_auth);
    }
    if (context->converts)
    {
    	sis_lock_reader_close(context->reader_convert);
        sis_map_pointer_destroy(context->converts);
    }
	sis_lock_reader_close(context->reader_recv);
    sis_lock_list_destroy(context->recv_list);

    sis_free(context);
    worker->context = NULL;
}

static void cb_socket_recv(void *worker_, s_sis_net_message *msg)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
	
    printf("recv query: [%d] %d : %s %s %s %s\n %s\n", msg->cid, msg->style, 
        msg->serial ? msg->serial : "nil",
        msg->cmd ? msg->cmd : "nil",
        msg->key ? msg->key : "nil",
        msg->val ? msg->val : "nil",
        msg->rval ? msg->rval : "nil");

    sis_net_message_incr(msg);
    s_sis_object *obj = sis_object_create(SIS_OBJECT_NETMSG, msg);
    sis_lock_list_push(context->recv_list, obj);
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
void sisdb_server_send_service(s_sis_worker *worker, const char *sdbname, const char *cmdname, s_sis_net_message *netmsg)
{
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;

    if (!sdbname)
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
        s_sis_worker *service = sis_map_list_get(context->datasets, sdbname);
        if (service)
        {
            int o = sis_worker_command(service, cmdname, netmsg);
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
            sisdb_server_reply_no_service(context, netmsg, sdbname);
        }               
    }  
}

// 返回值 -1 为没有权限
int sisdb_server_get_access(s_sisdb_server_cxt *context, s_sis_net_message *netmsg)
{
    if (!context->user_auth)
    {
        return SIS_METHOD_ACCESS_ADMIN;
    }
    char userid[16];   
    sis_lldtoa(netmsg->cid, userid, 16, 10);
    s_sisdb_userinfo *info = sis_map_pointer_get(context->user_access, userid);
    return info->access;
}
bool sisdb_check_method_access(s_sis_method *method, int access)
{
    if (!method->access)
    {
        return true;
    }
    if (method->access & SIS_METHOD_ACCESS_NONET)
    {
        return false;
    }
    if (access == SIS_METHOD_ACCESS_ADMIN)
    {
        return true;
    }
    if (access == SIS_METHOD_ACCESS_WRITE && 
        method->access & SIS_METHOD_ACCESS_DEL)
    {
        return false;
    }
    if (access == SIS_METHOD_ACCESS_READ &&
        (method->access & SIS_METHOD_ACCESS_DEL ||
         method->access & SIS_METHOD_ACCESS_WRITE))
    {
        return false;
    }
    return true;
}
static int cb_reader_recv(void *worker_, s_sis_object *in_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    sis_object_incr(in_);
    s_sis_net_message *netmsg = SIS_OBJ_NETMSG(in_);

    int access = sisdb_server_get_access(context, netmsg);
    if ( access < 0 && sis_strcasecmp("auth", netmsg->cmd))
    {
        sis_net_ans_with_error(netmsg, "no auth.", 8);
        sis_net_class_send(context->socket, netmsg);
    }
    else
    {
        // 先写log再处理
        bool make = true; 
        char sdbname[128]; 
        char cmdname[128]; 
        int cmds = sis_str_divide(netmsg->cmd, '.', sdbname, cmdname);
        if (cmds == 2)
        {
            // printf("2 %s %s\n", sdbname, cmdname);
            s_sis_worker *service = sis_map_list_get(context->datasets, sdbname);
            if (service)
            {
                s_sis_method *method = sis_worker_get_method(service, cmdname);
                if (!method)
                {
                    make = false;
                }
                else
                {
                    if (!sisdb_check_method_access(method, access))
                    {
                        // 当前用户是否有权限
                        // 从网络过来的数据包 只能调用支持网络数据包的方法
                        make = false;
                    }
                    else
                    {
                        // 指令是否为写入
                        if (method->access & SIS_METHOD_ACCESS_WRITE)
                        {
                            // printf("wlog: %s %s\n", netmsg->key, netmsg->cmd);
                            // 只记录写盘的数据
                            sis_worker_command(service, "wlog", netmsg);
                        }
                    }
                }
            }
            else
            {
                make = false;
            }
        }
        else
        {
            s_sis_method *method = sis_worker_get_method(worker, netmsg->cmd);
            if (!method || !sisdb_check_method_access(method, access))
            {
                make = false;
            }
            // printf("1 access = %d %d| %d %s\n", access, method->access, make, netmsg->cmd);
        }
        
        if (make)
        {
            // 此时已经写完log 再开始处理
            sisdb_server_send_service(worker, cmds == 1 ? NULL : sdbname, cmds == 1 ? sdbname : cmdname, netmsg);
        }
        else
        {
            sisdb_server_reply_no_method(context, netmsg, netmsg->cmd);
            sis_net_class_send(context->socket, netmsg);
        }
    }
	sis_object_decr(in_);
	return 0;
}

// 数据转换好后直接写入队列 当成用户数据输入 保证单线程运行
static int cb_reader_convert(void *worker_, s_sis_object *in_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    if (context->status != SISDB_STATUS_WORK)
    {
        // 数据库重新加载时不工作
        return 0;
    }
    s_sis_net_message *netmsg = SIS_OBJ_NETMSG(in_);    
    if(netmsg->style & SIS_NET_INSIDE)
    {
        // 不嵌套播报数据 根据style来判断
        return 0;
    }
    sis_object_incr(in_);
    int access = sisdb_server_get_access(context, netmsg);
    if (access == SIS_METHOD_ACCESS_ADMIN)   
    // 只有超级用户权限  发出的指令才能进行数据转移
    {
        // 把返回值重新写入队列 供大家使用
        sisdb_convert_working(context, netmsg);
    }
	sis_object_decr(in_);
	return 0;
}

void sisdb_server_method_init(void *worker_)
{
}
void sisdb_server_method_uninit(void *worker_)
{
}
//////////////////////////////////
bool sisdb_server_check_auth(s_sisdb_server_cxt *context, s_sis_net_message *netmsg)
{
    if (!context->user_auth)
    {
        return true;
    }
    char userid[16];   
    sis_lldtoa(netmsg->cid, userid, 16, 10);
    s_sisdb_userinfo *info = sis_map_pointer_get(context->user_access, userid);
    if (!info)
    {
        return false;
    }
    return true;
}
void sisdb_server_decr_auth(s_sisdb_server_cxt *context, int cid_)
{
    if (context->user_auth)
    {
        char userid[16];   
        sis_lldtoa(cid_, userid, 16, 10);
        sis_map_pointer_del(context->user_access, userid);
    }
}
void sisdb_server_incr_auth(s_sisdb_server_cxt *context, int cid, s_sisdb_userinfo *info)
{
    if (context->user_auth)
    {
        char userid[16];   
        sis_lldtoa(cid, userid, 16, 10);
        sis_map_pointer_set(context->user_access, userid, info);
    }
}
int cmd_sisdb_server_auth(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    if (!context->user_auth)
    {
        sis_net_ans_with_ok(netmsg);
        return SIS_METHOD_OK;
    }
    s_sisdb_userinfo *uinfo = (s_sisdb_userinfo *)sis_map_pointer_get(context->user_auth, netmsg->key);
    if (!uinfo || sis_strcasecmp(uinfo->password, netmsg->val))
    {
        sis_net_ans_with_error(netmsg, "auth fail.", 10);
    }
    else
    {
        // 登录成功
        sisdb_server_incr_auth(context, netmsg->cid, uinfo);
        sis_net_ans_with_ok(netmsg);
    }
    return SIS_METHOD_OK;
}

int cmd_sisdb_server_show(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
        
    s_sis_sds sdbs = NULL;
    s_sis_dict_entry *de;
    s_sis_dict_iter *di = sis_dict_get_iter(context->datasets->map);
    while ((de = sis_dict_next(di)) != NULL)
    {
        if (!sdbs)
        {
            sdbs = sis_sdsnew((char *)sis_dict_getkey(de));
        }
        else
        {
            sdbs = sis_sdscatfmt(sdbs, ",%s", (char *)sis_dict_getkey(de));
        }
    }
    sis_dict_iter_free(di);

    sis_net_ans_with_chars(netmsg, sdbs, sis_sdslen(sdbs));

    sis_sdsfree(sdbs);
    return SIS_METHOD_OK;
}

int _sisdb_server_stop(s_sisdb_server_cxt *context, int sid)
{
    // 停止某个客户端的所有未完成工作
    // -1 表示停止所有工作
    s_sis_message *msg = (s_sis_message *)sis_message_create();
    sis_message_set_str(msg, "mode", "reader", 6);
    sis_message_set_int(msg, "cid", sid);
    int count = sis_map_list_getsize(context->datasets);
    for (int i = 0; i < count; i++)
    {
        s_sis_worker *service = (s_sis_worker *)sis_map_list_geti(context->datasets, i);
        sis_worker_command(service, "clear", msg);
    }
    sis_message_destroy(msg);
    return SIS_METHOD_OK;
}

// ??? 这里如果文件没有返回错误的情况下 初始就启动不了server 
// 读取sno文件时，如果客户端中断 服务端会偶尔崩溃 怀疑是数据未清理 检查一下
// 订阅sno时，会有几秒等待时间 查一下
int _sisdb_server_load(s_sisdb_server_cxt *context)
{
    // 先根据规则加载磁盘数据
    s_sis_message *msg = sis_message_create();
    int count = sis_map_list_getsize(context->datasets);
    for (int i = 0; i < count; i++)
    {
        s_sis_worker *service = (s_sis_worker *)sis_map_list_geti(context->datasets, i);
        sis_message_set(msg, "config", &context->catch_cfg, NULL);
        sis_worker_command(service, "rdisk", msg);
    }
    // 再加载wlog中的数据
    for (int i = 0; i < count; i++)
    {
        s_sis_worker *service = (s_sis_worker *)sis_map_list_geti(context->datasets, i);
        if (sis_worker_command(service, "rlog", msg) != SIS_METHOD_OK)
        {
        }
    }
    sis_message_destroy(msg);
    return SIS_METHOD_OK;
}
int _sisdb_server_save(s_sisdb_server_cxt *context)
{
    // 需要把内存数据保存到磁盘中 并清理 wlog 等工作
    int oks = 0;
    s_sis_message *msg = sis_message_create();
    int count = sis_map_list_getsize(context->datasets);
    for (int i = 0; i < count; i++)
    {
        s_sis_worker *service = (s_sis_worker *)sis_map_list_geti(context->datasets, i);
        if (sis_worker_command(service, "save", msg) == SIS_METHOD_OK)
        {
            oks++;
        }
    }
    sis_message_destroy(msg);
    return oks;
}
int _sisdb_server_pack(s_sisdb_server_cxt *context)
{
    int oks = 0;
    s_sis_message *msg = sis_message_create();
    int count = sis_map_list_getsize(context->datasets);
    for (int i = 0; i < count; i++)
    {
        s_sis_worker *service = (s_sis_worker *)sis_map_list_geti(context->datasets, i);
        if (sis_worker_command(service, "pack", msg) == SIS_METHOD_OK)
        {
            oks++;
        }
    }
    sis_message_destroy(msg);
    return oks;
}
int cmd_sisdb_server_save(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    int count = _sisdb_server_save(context);

    sis_net_ans_with_int(netmsg, count); 
    return SIS_METHOD_OK;
}

int cmd_sisdb_server_pack(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    int count = _sisdb_server_pack(context);

    sis_net_ans_with_int(netmsg, count); 

    return SIS_METHOD_OK;
}
int cmd_sisdb_server_wget(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    context->switch_wget = !context->switch_wget;
    return SIS_METHOD_OK;
}
int cmd_sisdb_server_call(void *worker_, void *argv_)
{
    // 调用其他扩展方法
    return SIS_METHOD_ERROR;
}
