#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"

#include "sisdb_server.h"

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sisdb_server_methods[] = {
    {"auth", cmd_sisdb_server_auth, NULL, NULL},
    {"get", cmd_sisdb_server_get, NULL, NULL},   // json 格式
    {"set", cmd_sisdb_server_set, NULL, NULL},   // json 格式
    {"getb", cmd_sisdb_server_getb, NULL, NULL}, // 二进制 格式
    {"setb", cmd_sisdb_server_setb, NULL, NULL}, // 二进制 格式
    {"del", cmd_sisdb_server_del, NULL, NULL},
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

static int cb_reader_recv(void *cls_, s_sis_object *in_);
static int cb_reader_fast(void *cls_, s_sis_object *in_);
static int cb_reader_catch(void *cls_, s_sis_object *in_);
static void cb_recv(void *worker_, s_sis_net_message *msg);

static void _cb_connected(void *worker_, int sid)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
		sis_net_class_set_cb(context->server, sid, worker, cb_recv);
    LOG(5)("client connect ok. [%d]\n", sid);
}

static void _cb_disconnect(void *worker_, int sid)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
	sis_net_class_set_cb(context->server, sid, worker, NULL);
    LOG(5)("client disconnect. [%d]\n", sid);	
}

bool sisdb_server_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_server_cxt *context = SIS_MALLOC(s_sisdb_server_cxt, context);
    worker->context = context;

    context->level = sis_json_get_int(node, "level", 10);

    context->recv_list = sis_share_list_create("system", 32*1000*1000);
    context->reader_recv = sis_share_reader_login(context->recv_list, 
        SIS_SHARE_FROM_HEAD, worker, cb_reader_recv);
    context->reader_fast = sis_share_reader_login(context->recv_list, 
        SIS_SHARE_FROM_HEAD, worker, cb_reader_fast);
    context->reader_catch = sis_share_reader_login(context->recv_list, 
        SIS_SHARE_FROM_HEAD, worker, cb_reader_catch);

    s_sis_json_node *catchnode = sis_json_cmp_child_node(node, "catch-save");
    if (catchnode)
    {
        s_sis_worker *service = sis_worker_create(worker, catchnode);
        if (service)
        {
            context->catch_save = service;  // 根据是否为空判断是订阅还是自动执行
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
            userinfo->authority = sis_json_get_int(next, "auth", 0xFF);
            sis_strcpy(userinfo->username, 32, sis_json_get_str(next,"username"));
            sis_strcpy(userinfo->password, 32, sis_json_get_str(next,"password"));
            if (!userinfo->username[0])
            {
                sis_strcpy(userinfo->username, 32, "guest");
                userinfo->authority = 0;  // 可读
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
            s_sis_worker *service = sis_worker_create(worker, next);
            if (service)
            {
                sis_map_list_set(context->datasets, next->key, service);
                // printf("add service : %s\n", next->key);
            }
            next = next->next;
        }  
    }
    context->message = sis_message_create();

    int isopen = 0;
    s_sis_json_node *srvnode = sis_json_cmp_child_node(node, "server");
    s_sis_url  url;
    memset(&url, 0, sizeof(s_sis_url));
    if (sis_url_load(srvnode, &url))
    {
        url.io = SIS_NET_IO_WAITCNT;
        url.role = SIS_NET_ROLE_ANSWER;
        context->server = sis_net_class_create(&url);
        context->server->cb_source = worker;
        context->server->cb_connected = _cb_connected;
        context->server->cb_disconnect = _cb_disconnect;
        if (sis_net_class_open(context->server))
        {
            isopen = 1;
        }
    }
    if (!isopen)
    {
        return false;
    }
    return true;
}
// void  sisdb_server_work_init(void *worker_)
// {
// }
// void  sisdb_server_working(void *worker_)
// {
// }
// void  sisdb_server_work_uninit(void *worker_)
// {
// }
void sisdb_server_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;

    if (context->server)
    {
        sis_net_class_destroy(context->server);
    }
    sis_message_destroy(context->message);
    if (context->datasets)
    {
        sis_map_list_destroy(context->datasets);
    }
    if (context->user_auth)
    {
        sis_map_pointer_destroy(context->user_auth);
    }

	sis_share_reader_logout(context->recv_list, context->reader_fast);
    if (context->fast_save)
    {
        sis_worker_destroy(context->fast_save);
    }
	sis_share_reader_logout(context->recv_list, context->reader_catch);
    if (context->catch_save)
    {
        sis_worker_destroy(context->catch_save);
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
    sis_share_list_publish(context->recv_list, obj);
	sis_object_destroy(obj);

    // if (msg->style & SIS_NET_ASK_MSG)
	// {
	// 	printf("recv query: [%d] %d : %s %s %s\n", msg->cid, 1, 
	// 		msg->cmd ? msg->cmd : "null",
	// 		msg->key ? msg->key : "null",
	// 		msg->val ? msg->val : "null");
	// 	s_sis_sds reply = sis_sdsempty();
	// 	reply = sis_sdscatfmt(reply, "%S %S ok.", msg->cmd, msg->key);
	// 	sis_net_ans_reply(socket, msg->cid, reply, sis_sdslen(reply));
	// 	sis_sdsfree(reply);
	// }	
}
void sisdb_server_reply_error(s_sisdb_server_cxt *context, int handle, char *name)
{
    s_sis_sds reply = sis_sdsempty();
    reply = sis_sdscatfmt(reply, "method [%s] fail.", name);
    sis_net_ans_error(context->server, handle, reply, sis_sdslen(reply));
    sis_sdsfree(reply);
}
void sisdb_server_reply_no_method(s_sisdb_server_cxt *context, int handle, char *name)
{
    s_sis_sds reply = sis_sdsempty();
    reply = sis_sdscatfmt(reply, "no find method [%s].", name);
    sis_net_ans_error(context->server, handle, reply, sis_sdslen(reply));
    sis_sdsfree(reply);
}
void sisdb_server_reply_no_service(s_sisdb_server_cxt *context, int handle, char *name)
{
    s_sis_sds reply = sis_sdsempty();
    reply = sis_sdscatfmt(reply, "no find service [%s].", name);
    sis_net_ans_error(context->server, handle, reply, sis_sdslen(reply));
    sis_sdsfree(reply);
}
static int cb_reader_recv(void *worker_, s_sis_object *in_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;

    sis_object_incr(in_);
    s_sis_net_message *netmsg = SIS_OBJ_NETMSG(in_);

    char argv[2][32]; 
    int count = sis_str_divide(netmsg->cmd, '.', argv[0], argv[1]);
    printf("cb_reader_recv: %d %s %s \n", count, argv[0], argv[1]);
    if (count == 1)
    {
        int noservice = 1;
        int count = sis_map_list_getsize(context->datasets);
        for (int i = 0; i < count; i++)
        {
            s_sis_worker *service = sis_map_list_geti(context->datasets, i);
            int o = sis_worker_command(service, netmsg->cmd, netmsg);
            if ( o < SIS_METHOD_ERROR)
            {
                continue;
            }
            noservice = 0;
            if ( o == SIS_METHOD_OK)
            {
                sis_net_class_send(context->server, netmsg);
            }
            else
            {
                sisdb_server_reply_error(context, netmsg->cid, netmsg->cmd);
            }
            break;
        }
        if (noservice)
        {
            sisdb_server_reply_no_method(context, netmsg->cid, netmsg->cmd);
        }
    }
    else
    {
        s_sis_worker *service = sis_map_list_get(context->datasets, argv[0]);
        if (service)
        {
            // sis_message_set(context->message, "message", netmsg, NULL);
            // if (sis_worker_command(service, argv[1], context->message) == SIS_METHOD_OK)
            int o = sis_worker_command(service, argv[1], netmsg);
            if (o == SIS_METHOD_OK)
            {
                sis_net_class_send(context->server, netmsg);
            }
            else if (o == SIS_METHOD_ERROR)
            {
                sisdb_server_reply_error(context, netmsg->cid, netmsg->cmd);
            }
            else
            {
                sisdb_server_reply_no_method(context, netmsg->cid, argv[1]);
            }
        } 
        else
        {
            sisdb_server_reply_no_service(context, netmsg->cid, argv[0]);
        }
               
    }
	sis_object_decr(in_);
	return 0;
}

static int cb_reader_fast(void *worker_, s_sis_object *in_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
	return 0;
}

static int cb_reader_catch(void *worker_, s_sis_object *in_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_server_cxt *context = (s_sisdb_server_cxt *)worker->context;
	return 0;
}

void sisdb_server_method_init(void *worker_)
{
}
void sisdb_server_method_uninit(void *worker_)
{
}
int cmd_sisdb_server_auth(void *worker_, void *argv_)
{
    return 0;
}
int cmd_sisdb_server_get(void *worker_, void *argv_)
{
    return 0;
}
int cmd_sisdb_server_set(void *worker_, void *argv_)
{
    return 0;
}

int cmd_sisdb_server_getb(void *worker_, void *argv_)
{
    return 0;
}
int cmd_sisdb_server_setb(void *worker_, void *argv_)
{
    return 0;
}
int cmd_sisdb_server_del(void *worker_, void *argv_)
{
    return 0;
}
