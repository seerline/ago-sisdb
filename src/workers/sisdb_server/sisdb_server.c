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
    {"auth",  cmd_sisdb_server_auth, NULL, NULL},   // 用户登录
    {"show",  cmd_sisdb_server_show, NULL, NULL},   // 显示有多少数据集
    {"save",  cmd_sisdb_server_save, "write,admin", NULL},   // 手动存盘
    {"pack",  cmd_sisdb_server_pack, "write,admin", NULL},   // 手动清理磁盘旧的数据
    {"call",  cmd_sisdb_server_call, NULL, NULL},   // 用于不同数据表之间关联计算的用途，留出其他语言加载的接口
    {"wget",  cmd_sisdb_server_wget, NULL, NULL},   // get 后是否写log文件 方便查看信息
};
// 共享内存数据库
s_sis_modules sis_modules_sisdb_server = {
    sisdb_server_init,
    sisdb_server_work_init,
    sisdb_server_working,
    sisdb_server_work_uninit,
    sisdb_server_uninit,
    sisdb_server_method_init,
    sisdb_server_method_uninit,
    sizeof(sisdb_server_methods) / sizeof(s_sis_method),
    sisdb_server_methods,
};

static int cb_reader_recv(void *cls_, s_sis_object *in_);
// static int cb_reader_wlog(void *cls_, s_sis_object *in_);
static int cb_reader_convert(void *cls_, s_sis_object *in_);
static void cb_recv(void *worker_, s_sis_net_message *msg);

static void _cb_connected(void *worker_, int sid)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
		sis_net_class_set_cb(context->socket, sid, worker, cb_recv);
    LOG(5)("client connect ok. [%d]\n", sid);
}

static void _cb_disconnect(void *worker_, int sid)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
	sis_net_class_set_cb(context->socket, sid, worker, NULL);
    LOG(5)("client disconnect. [%d]\n", sid);	
}

bool sisdb_server_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_server_cxt *context = SIS_MALLOC(s_sisdb_server_cxt, context);
    worker->context = context;

    sis_mutex_init(&(context->wlog_lock), NULL);

    context->level = sis_json_get_int(node, "level", 10);

    context->recv_list = sis_share_list_create("system", 64*1000*1000);
    context->reader_recv = sis_share_reader_login(context->recv_list, 
        SIS_SHARE_FROM_HEAD, worker, cb_reader_recv);

    s_sis_json_node *wlognode = sis_json_cmp_child_node(node, "wlog-save");
    if (wlognode)
    {
        s_sis_worker *service = sis_worker_create(worker, wlognode);
        if (service)
        {
            context->wlog_save = service;  // 根据是否为空判断是订阅还是自动执行
            context->wlog_method = sis_worker_get_method(context->wlog_save, "write");
            // context->reader_wlog = sis_share_reader_login(context->recv_list, 
            //     SIS_SHARE_FROM_HEAD, worker, cb_reader_wlog);
        }     
    }

    s_sis_json_node *fastnode = sis_json_cmp_child_node(node, "fast-save");
    if (fastnode)
    {
        s_sis_worker *service = sis_worker_create(worker, fastnode);
        if (service)
        {
            context->fast_save = service;  // 根据是否为空判断是订阅还是自动执行
        }     
    }

    s_sis_json_node *authnode = sis_json_cmp_child_node(node, "auth");
    if (authnode)
    {
        context->user_auth = sis_map_pointer_create_v(sis_free_call);
        s_sis_json_node *next = sis_json_first_node(authnode);
        while(next)
        {
            s_sisdb_userinfo *userinfo = SIS_MALLOC(s_sisdb_userinfo, userinfo);
            userinfo->access = sis_json_get_int(next, "auth", 0xFF);
            sis_strcpy(userinfo->username, 32, sis_json_get_str(next,"username"));
            sis_strcpy(userinfo->password, 32, sis_json_get_str(next,"password"));
            if (!userinfo->username[0])
            {
                sis_strcpy(userinfo->username, 32, "guest");
                userinfo->access = 0;  // 可读
                // 如果不设置用户名就默认 guest 登录
            }
            sis_map_pointer_set(context->user_auth, userinfo->username, userinfo);
            next = next->next;
        } 
        context->logined = false; 
    }
    else
    {
        context->logined = true;
    }
    

    s_sis_json_node *dsnode = sis_json_cmp_child_node(node, "datasets");
    if (dsnode)
    {
        context->datasets = sis_map_list_create(sis_worker_destroy);
        s_sis_json_node *next = sis_json_first_node(dsnode);
        while(next)
        {
            // s_sis_worker *service = sis_worker_create(worker, next);
            s_sis_worker *service = sis_worker_create(NULL, next);
            if (service)
            {
                sis_map_list_set(context->datasets, next->key, service);
                // printf("add service : %s %d\n", next->key, 1);
            }
            next = next->next;
        }  
    }
    s_sis_json_node *cvnode = sis_json_cmp_child_node(node, "converts");
    if (cvnode)
    {
        context->converts = sis_map_pointer_create_v(sis_pointer_list_destroy);

        sisdb_convert_init(context, cvnode);

        context->reader_convert = sis_share_reader_login(context->recv_list, 
            SIS_SHARE_FROM_HEAD, worker, cb_reader_convert);
    }

    // context->message = sis_message_create();
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
    int count = sis_map_list_getsize(context->datasets);
    for (int i = 0; i < count; i++)
    {
        s_sis_worker *service = (s_sis_worker *)sis_map_list_geti(context->datasets, i);
        s_sis_message *msg = (s_sis_message *)sis_message_create();
        sis_message_set(msg, "socket", context->socket, NULL);
        sis_message_set_str(msg, "fastpath", "data/", 5);
        sis_worker_command(service, "init", msg);
        sis_message_destroy(msg);
    }
    // 加载数据
    if (_sisdb_server_load(context) != SIS_METHOD_OK)
    {
        sisdb_server_uninit(worker_);
        return false;
    }
    // 打开网络
    if (!sis_net_class_open(context->socket))
    {
        sisdb_server_uninit(worker_);
        return false;
    }

    return true;
}

void sisdb_server_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;

    if (context->socket)
    {
        sis_net_class_destroy(context->socket);
    }
    // sis_message_destroy(context->message);
    if (context->datasets)
    {
        sis_map_list_destroy(context->datasets);
    }
    if (context->user_auth)
    {
        sis_map_pointer_destroy(context->user_auth);
    }

    if (context->fast_save)
    {
        sis_worker_destroy(context->fast_save);
    }

    if (context->wlog_save)
    {
    	// sis_share_reader_logout(context->recv_list, context->reader_wlog);
        sis_worker_destroy(context->wlog_save);
    }

    if (context->converts)
    {
    	sis_share_reader_logout(context->recv_list, context->reader_convert);
        sis_map_pointer_destroy(context->converts);
    }

	sis_share_reader_logout(context->recv_list, context->reader_recv);
    sis_share_list_destroy(context->recv_list);

    sis_free(context);
    worker->context = NULL;
}

static void cb_recv(void *worker_, s_sis_net_message *msg)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
	
    printf("recv query: [%d] %d : %s %s %s\n %s\n", msg->cid, msg->style, 
        msg->cmd ? msg->cmd : "null",
        msg->key ? msg->key : "null",
        msg->val ? msg->val : "null",
        msg->rval ? msg->rval : "null");

    sis_net_message_incr(msg);
    s_sis_object *obj = sis_object_create(SIS_OBJECT_NETMSG, msg);
    sis_share_list_push(context->recv_list, obj);
	sis_object_destroy(obj);
	
}
void sisdb_server_reply_error(s_sisdb_server_cxt *context, s_sis_net_message *netmsg)
{
    s_sis_sds reply = sis_sdsempty();
    reply = sis_sdscatfmt(reply, "method [%s] fail.", netmsg->cmd);
    sis_net_ans_with_error(netmsg, reply, sis_sdslen(reply));
    sis_net_class_send(context->socket, netmsg);
    sis_sdsfree(reply);
}

void sisdb_server_reply_null(s_sisdb_server_cxt *context, s_sis_net_message *netmsg, char *name)
{
    s_sis_sds reply = sis_sdsempty();
    reply = sis_sdscatfmt(reply, "no key [%s].", name);
    sis_net_ans_with_error(netmsg, reply, sis_sdslen(reply));
    sis_net_class_send(context->socket, netmsg);
    sis_sdsfree(reply);
}
void sisdb_server_reply_no_method(s_sisdb_server_cxt *context, s_sis_net_message *netmsg, char *name)
{
    s_sis_sds reply = sis_sdsempty();
    reply = sis_sdscatfmt(reply, "no find method [%s].", name);
    sis_net_ans_with_error(netmsg, reply, sis_sdslen(reply));
    sis_net_class_send(context->socket, netmsg);
    sis_sdsfree(reply);
}
void sisdb_server_reply_no_service(s_sisdb_server_cxt *context, s_sis_net_message *netmsg, char *name)
{
    s_sis_sds reply = sis_sdsempty();
    reply = sis_sdscatfmt(reply, "no find service [%s].", name);
    sis_net_ans_with_error(netmsg, reply, sis_sdslen(reply));
    sis_net_class_send(context->socket, netmsg);
    sis_sdsfree(reply);
}
void sisdb_server_send_service(s_sis_worker *worker, s_sis_net_message *netmsg)
{
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;

    char argv[2][128]; 
    int cmds = sis_str_divide(netmsg->cmd, '.', argv[0], argv[1]);
    // printf("cb_reader_recv: %d %s %s %s\n", cmds, argv[0], argv[1], netmsg->key);

    if (cmds == 1)
    {
        int o = sis_worker_command(worker, netmsg->cmd, netmsg);
        if (o == SIS_METHOD_OK)
        {
            sis_net_class_send(context->socket, netmsg);
        }  
        else if (o == SIS_METHOD_NULL)
        {
            sisdb_server_reply_null(context, netmsg, netmsg->cmd);
        }
        else if (o == SIS_METHOD_NOCMD)
        {
            sisdb_server_reply_no_method(context, netmsg, netmsg->cmd);
        }
        else // if (o == SIS_METHOD_ERROR)
        {
            sisdb_server_reply_error(context, netmsg);
        }
    }
    else
    {
        s_sis_worker *service = sis_map_list_get(context->datasets, argv[0]);
        if (service)
        {
            int o = sis_worker_command(service, argv[1], netmsg);
            if (o == SIS_METHOD_OK)
            {
                sis_net_class_send(context->socket, netmsg);
            } 
            else if (o == SIS_METHOD_NULL)
            {
                sisdb_server_reply_null(context, netmsg, argv[1]);
            }
            else if (o == SIS_METHOD_NOCMD)
            {
                sisdb_server_reply_no_method(context, netmsg, argv[1]);
            }
            else // if (o == SIS_METHOD_ERROR)
            {
                sisdb_server_reply_error(context, netmsg);
            }
        } 
        else
        {
            sisdb_server_reply_no_service(context, netmsg, argv[0]);
        }               
    }  
}

// static int cb_reader_wlog(void *worker_, s_sis_object *in_)
// {
//     s_sis_worker *worker = (s_sis_worker *)worker_; 
//     s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
//     if (!context->wlog_method || // 没有方法就不执行下面这句
//         (context->user_auth && !context->logined))  // 没有登录成功
//     {
//         return 0;
//     }
//     s_sis_net_message *netmsg = SIS_OBJ_NETMSG(in_);
//     if(netmsg->style & SIS_NET_INSIDE)
//     {
//         return 0;
//     }
//     sis_object_incr(in_);

//     char argv[2][128]; 
//     // int cmds = 
//     sis_str_divide(netmsg->cmd, '.', argv[0], argv[1]);

//     s_sis_worker *service = sis_map_list_get(context->datasets, argv[0]);
//     if (service)
//     {
//         s_sis_method *method = sis_worker_get_method(service, argv[1]);
//         if (method && sis_str_subcmp_strict("write",  method->access, ',') >= 0)
//         {
//             printf("wlog: %s %s\n", netmsg->key, netmsg->cmd);

//             // 只记录写盘的数据
//             sis_mutex_lock(&context->wlog_lock);
//             context->wlog_method->proc(context->wlog_save, netmsg);
//             sis_mutex_unlock(&context->wlog_lock);
//         }
//     }
// 	sis_object_decr(in_);
// 	return 0;
// }
static int cb_reader_recv(void *worker_, s_sis_object *in_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    sis_object_incr(in_);
    s_sis_net_message *netmsg = SIS_OBJ_NETMSG(in_);

    if (context->user_auth && !context->logined && sis_strcasecmp("auth", netmsg->cmd))
    {
        sis_net_ans_with_error(netmsg, "no auth.", 8);
        sis_net_class_send(context->socket, netmsg);
    }
    else
    {
        // 先写log再处理
        char argv[2][128]; 
        int cmds = sis_str_divide(netmsg->cmd, '.', argv[0], argv[1]);
        if (cmds == 2)
        {
            s_sis_worker *service = sis_map_list_get(context->datasets, argv[0]);
            if (service)
            {
                s_sis_method *method = sis_worker_get_method(service, argv[1]);
                if (method && sis_str_subcmp_strict("write",  method->access, ',') >= 0)
                {
                    // printf("wlog: %s %s\n", netmsg->key, netmsg->cmd);
                    // 只记录写盘的数据
                    sis_mutex_lock(&context->wlog_lock);
                    context->wlog_method->proc(context->wlog_save, netmsg);
                    sis_mutex_unlock(&context->wlog_lock);
                }
            }
        }
        // 写完log开始处理
        sisdb_server_send_service(worker, netmsg);
    }

	sis_object_decr(in_);
	return 0;
}

// 数据转换好后直接写入队列 当成用户数据输入 保证单线程运行
static int cb_reader_convert(void *worker_, s_sis_object *in_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    if (context->status != SISDB_STATUS_WORKING)
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
    if (!context->user_auth || context->logined)
    {
        // 判断是否需要转数据需要就传数据 并取得返回值
        // 把返回值重新写入队列 供大家使用
        sisdb_convert_working(context, netmsg);
    }
	sis_object_decr(in_);
	return 0;
}

void sisdb_server_work_init(void *worker_)
{
}
void sisdb_server_working(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    // 这里要判断是否新的一天 如果是就存盘
    // 存盘后如果检测到是周五 就执行pack工作
    // if (context->work_date == 0)
    // {
    //     context->work_date = sis_time_get_idate(0);
    // }
    // else
    {
        int minute = sis_time_get_iminute(0);
        // 最后一分钟就开始处理数据
        if (minute >= 2359)
        {
            sis_mutex_lock(&context->wlog_lock);
            _sisdb_server_save(context, context->work_date);
            int week = sis_time_get_week_ofday(context->work_date);
            if (week == 5)
            {
                _sisdb_server_pack(context);
            }
            // 等待新的一天到来
            while(sis_time_get_idate(0) <= context->work_date)
            {
                sis_sleep(1000);
            }
            context->work_date = sis_time_get_idate(0);
            // 这步开始时 wlog 已经被清空 
            _sisdb_server_load(context);

            sis_mutex_unlock(&context->wlog_lock);
        }
    }  
}
void sisdb_server_work_uninit(void *worker_)
{
}
void sisdb_server_method_init(void *worker_)
{
}
void sisdb_server_method_uninit(void *worker_)
{
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
        context->logined = true;
        if (sis_map_list_getsize(context->datasets) > 0)
        {
            sis_net_ans_with_ok(netmsg);
        }
        else
        {
            sis_net_ans_with_error(netmsg, "auth ok. but no dataset.", 24);
        }
    }
    return SIS_METHOD_OK;
}

int cmd_sisdb_server_show(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
        
    s_sis_json_node *jone = sis_json_create_object();
    s_sis_json_node *jdbs = sis_json_create_object();
    sis_json_object_add_node(jone, "datasets", jdbs);
    int count = sis_map_list_getsize(context->datasets);
    for (int i = 0; i < count; i++)
    {
        s_sis_worker *service = (s_sis_worker *)sis_map_list_geti(context->datasets, i);
        
        s_sisdb_cxt *cxt = service->context;
        s_sis_json_node *jdb = sis_sisdb_make_sdb_node(cxt);
        sis_json_object_add_node(jdbs, service->workername, jdb);
    }
    size_t len;
    char *str = sis_json_output_zip(jone, &len);

    sis_net_ans_with_string(netmsg, str, len);

    sis_free(str);
    sis_json_delete_node(jone);

    return SIS_METHOD_OK;

}
int cb_sisdb_wlog_load(void *worker_, void *argv_)
{
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker_;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    printf(":::%d %s \n%s \n%s \n%s \n", netmsg->style,
            netmsg->source? netmsg->source : "nil",
            netmsg->cmd ?   netmsg->cmd : "nil",
            netmsg->key?    netmsg->key : "nil",
            netmsg->val?    netmsg->val : "nil");   

    char argv[2][128]; 
    int cmds = sis_str_divide(netmsg->cmd, '.', argv[0], argv[1]);
    if (cmds == 2)
    {
        s_sis_worker *service = sis_map_list_get(context->datasets, argv[0]);
        if (service)
        {
            sis_worker_command(service, argv[1], netmsg);
        }              
    }  
    return 0;
}

int _sisdb_server_load(s_sisdb_server_cxt *context)
{
    // 先根据规则加载磁盘数据
    s_sis_message *msg = sis_message_create();
    int count = sis_map_list_getsize(context->datasets);
    for (int i = 0; i < count; i++)
    {
        s_sisdb_cxt *sisdb = (s_sisdb_cxt *)((s_sis_worker *)sis_map_list_geti(context->datasets, i))->context;
        sis_message_set(msg, "sisdb", sisdb, NULL);
        sis_message_set(msg, "config", &context->catch_cfg, NULL);
        if (sis_worker_command(context->fast_save, "load", msg) != SIS_METHOD_OK)
        {
            sis_message_destroy(msg);
            return SIS_METHOD_ERROR;
        }
    }
    // 再加载wlog中的数据
    for (int i = 0; i < count; i++)
    {
        s_sisdb_cxt *sisdb = (s_sisdb_cxt *)((s_sis_worker *)sis_map_list_geti(context->datasets, i))->context;
        sis_message_set(msg, "sisdb", sisdb, NULL);
        sis_message_set(msg, "source", context, NULL);
        sis_message_set_method(msg, "cb_recv", cb_sisdb_wlog_load);
        if (sis_worker_command(context->wlog_save, "read", msg) != SIS_METHOD_OK)
        {
            sis_message_destroy(msg);
            return SIS_METHOD_ERROR;
        }
    }
    sis_message_destroy(msg);
    return SIS_METHOD_OK;
}
int _sisdb_server_save(s_sisdb_server_cxt *context, int workdate)
{
    // 需要把内存数据保存到磁盘中 并清理 wlog 等工作
    int oks = 0;
    s_sis_message *msg = sis_message_create();
    int count = sis_map_list_getsize(context->datasets);
    for (int i = 0; i < count; i++)
    {
        s_sisdb_cxt *sisdb = (s_sisdb_cxt *)((s_sis_worker *)sis_map_list_geti(context->datasets, i))->context;  
        printf("save [%s] fail. workdate = %d .\n", sisdb->name, workdate);
      
        if (sis_worker_command(context->wlog_save, "check", sisdb->name) != SIS_METHOD_OK)
        {
            continue;
        }
        sis_message_set(msg, "sisdb", sisdb, NULL);
        sis_message_set_int(msg, "workdate", workdate);       
        if (sis_worker_command(context->fast_save, "save", msg) == SIS_METHOD_OK)
        {
            LOG(5)("save ok. start clear wlog [%s] ...\n", sisdb->name);
            // 数据已经保存 删除wlog
            sis_worker_command(context->wlog_save, "clear", sisdb->name);
            oks++;
        }
        else
        {
            LOG(5)("save [%s] fail. workdate = %d .\n", sisdb->name, workdate);
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

    int workdate = context->work_date;
    if (netmsg->key)
    {
        workdate = atoll(netmsg->key);
    }

    sis_mutex_lock(&context->wlog_lock);
    int count = _sisdb_server_save(context, workdate);
    sis_mutex_unlock(&context->wlog_lock);

    sis_net_ans_with_int(netmsg, count); 
    return SIS_METHOD_OK;
}
int _sisdb_server_pack(s_sisdb_server_cxt *context)
{
    int oks = 0;
    s_sis_message *msg = sis_message_create();
    int count = sis_map_list_getsize(context->datasets);
    for (int i = 0; i < count; i++)
    {
        s_sisdb_cxt *sisdb = (s_sisdb_cxt *)((s_sis_worker *)sis_map_list_geti(context->datasets, i))->context;
        sis_message_set(msg, "sisdb", sisdb, NULL);
        if (sis_worker_command(context->fast_save, "pack", msg) == SIS_METHOD_OK)
        {
            oks++;
        }
    }
    return oks;
}
int cmd_sisdb_server_pack(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

    sis_mutex_lock(&context->wlog_lock);
    int count = _sisdb_server_pack(context);
    sis_mutex_unlock(&context->wlog_lock);

    sis_net_ans_with_int(netmsg, count); 

    return SIS_METHOD_OK;
}
int cmd_sisdb_server_call(void *worker_, void *argv_)
{
    // 调用其他扩展方法
    return SIS_METHOD_ERROR;
}
int cmd_sisdb_server_wget(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
    context->switch_wget = !context->switch_wget;
    return SIS_METHOD_OK;
}
