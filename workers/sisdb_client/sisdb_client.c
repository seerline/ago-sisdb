#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"

#include "sisdb_client.h"
#include "sisdb.h"
#include "sis_net.msg.h"
// #include "sisdb_server.h"
// #include "sisdb_collect.h"
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sisdb_client_methods[] = {
    {"setcb",            cmd_sisdb_client_setcb,        0, NULL},  // 
    {"status",           cmd_sisdb_client_status,       0, NULL},  // 0 不工作 1 工作中
    {"ask-chars-wait",   cmd_sisdb_client_chars_wait,   0, NULL},  // 堵塞直到数据返回
    {"ask-bytes-wait",   cmd_sisdb_client_bytes_wait,   0, NULL},  // 堵塞直到数据返回
    {"ask-chars-nowait", cmd_sisdb_client_chars_nowait, 0, NULL},  // 不堵塞
    {"ask-bytes-nowait", cmd_sisdb_client_bytes_nowait, 0, NULL},  // 不堵塞
};
// 通用文件存取接口
s_sis_modules sis_modules_sisdb_client = {
    sisdb_client_init,
    NULL,
    NULL,
    NULL,
    sisdb_client_uninit,
    sisdb_client_method_init,
    sisdb_client_method_uninit,
    sizeof(sisdb_client_methods) / sizeof(s_sis_method),
    sisdb_client_methods,
};

s_sisdb_client_ask *sisdb_client_ask_create(
    const char   *cmd_,            // 请求的参数
	const char   *key_,            // 请求的key
	void         *val_,            // 请求的参数
    size_t        vlen_,
	void         *cb_source_,      // 回调传送对象
	void         *cb_reply         // 回调的数据
)
{
    s_sisdb_client_ask *ask = SIS_MALLOC(s_sisdb_client_ask, ask);
    ask->cmd = cmd_ ? sis_sdsnew(cmd_) : NULL;
    ask->key = key_ ? sis_sdsnew(key_): NULL;
    ask->val = val_ ? sis_sdsnewlen(val_, vlen_): NULL;
    ask->issub = false;
    if (ask->cmd)
    {
        char service[128], cmd[128]; 
        sis_str_divide(ask->cmd, '.', service, cmd);
        if (sis_str_subcmp_strict(cmd, "sub,zsub,hsub", ',') >= 0)
        {
            ask->issub = true;
        }
    }
    ask->cb_source = cb_source_;
    ask->cb_reply = cb_reply;
    return ask;
}

void sisdb_client_ask_destroy(void *ask_)
{
    s_sisdb_client_ask *ask = (s_sisdb_client_ask *)ask_;
    sis_sdsfree(ask->cmd);
    sis_sdsfree(ask->key);
    sis_sdsfree(ask->val);
    sis_free(ask);
}

bool sisdb_client_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_sisdb_client_cxt *context = SIS_MALLOC(s_sisdb_client_cxt, context);
    worker->context = context;

    sis_url_set_ip4(&context->url_cli, sis_json_get_str(node, "ip"));
    context->url_cli.port = sis_json_get_int(node, "port", 7329);
    context->url_cli.io = SIS_NET_IO_CONNECT;
    context->url_cli.role = SIS_NET_ROLE_REQUEST;
    context->url_cli.version = 1;
    context->url_cli.protocol = SIS_NET_PROTOCOL_WS;
    context->url_cli.compress = 0;
    context->url_cli.crypt = 0;
    context->url_cli.dict = NULL;

    sis_strcpy(context->username, 32, sis_json_get_str(node, "username"));
    sis_strcpy(context->password, 32, sis_json_get_str(node, "password"));
    if(sis_strlen(context->username) > 0 && sis_strlen(context->password) > 0)
    {
        context->auth = true;
    }

    context->ask_sno = 0; 

    context->asks = sis_safe_map_create_v(sisdb_client_ask_destroy);

    context->status = SIS_CLI_STATUS_INIT;
    // printf("%s %p, %d\n", __func__, context, context->status);
    return true;
}
void sisdb_client_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_client_cxt *context = (s_sisdb_client_cxt *)worker->context;
    sis_safe_map_destroy(context->asks);
    sis_free(context);
}

void sisdb_client_send_ask(s_sisdb_client_cxt *context, s_sisdb_client_ask *ask)
{
    s_sis_net_message *netmsg = sis_net_message_create();
    netmsg->cid = context->cid;
    netmsg->name = sis_sdsnewlong(ask->sno);
    // cmd 可能有服务名
    sis_net_message_set_scmd(netmsg, ask->cmd);
    sis_net_message_set_subject(netmsg, ask->key, NULL);
    if  (ask->format == SIS_NET_FORMAT_CHARS)
    {
        sis_net_message_set_char(netmsg, ask->val, ask->val ? sis_sdslen(ask->val) : 0);
    }
    else
    {
        sis_net_message_set_byte(netmsg, ask->val, ask->val ? sis_sdslen(ask->val) : 0);
    }
    // SIS_NET_SHOW_MSG("client", netmsg);
    sis_net_class_send(context->session, netmsg);
    sis_net_message_destroy(netmsg);
}
        
static void _cb_recv(void *source_, s_sis_net_message *msg_)
{
    s_sisdb_client_cxt *context = (s_sisdb_client_cxt *)source_;
    if (context->status == SIS_CLI_STATUS_WORK)
    {
        s_sisdb_client_ask *ask = sisdb_client_ask_get(context, msg_->name);
        // 清理上次返回的信息
        // SIS_NET_SHOW_MSG("_cb_recv ask", msg_);
        // printf("aaaaa: %d %p : %d\n", msg_->switchs.sw_tag, ask, msg_->tag);
        if (ask && msg_->switchs.sw_tag)
        {
            switch (msg_->tag)
            {
            case SIS_NET_TAG_SUB_START:
            case SIS_NET_TAG_SUB_WAIT:
            case SIS_NET_TAG_SUB_STOP:
                if(ask->cb_reply)
                {
                    ask->cb_reply(ask->cb_source, msg_->tag, NULL, msg_->info, msg_->info ? sis_sdslen(msg_->info) : 0);
                }
                break;
            case SIS_NET_TAG_OK:
                if(ask->cb_reply)
                {
                    if(ask->cb_reply)
                    {
                        ask->cb_reply(ask->cb_source, SIS_NET_TAG_OK, msg_->subject, msg_->info, msg_->info ? sis_sdslen(msg_->info) : 0);
                    }                                                      
                }        
                break;
            case SIS_NET_TAG_NIL:
                if(ask->cb_reply)
                {
                    // 其他返回NULL
                    ask->cb_reply(ask->cb_source, SIS_NET_TAG_NIL, NULL, NULL, 0);
                    // 返回 NULL 表示可不理会返回值 需要错误信息调用函数
                }        
                break;
            case SIS_NET_TAG_ERROR:
            default:
                if(ask->cb_reply)
                {
                    // 其他返回NULL
                    ask->cb_reply(ask->cb_source, SIS_NET_TAG_ERROR, msg_->subject, msg_->info, msg_->info ? sis_sdslen(msg_->info) : 0);
                    // 返回 NULL 表示可不理会返回值 需要错误信息调用函数
                }        
                break;
            }
            // 已经处理完数据 清理ask
            if (!ask->issub)
            {
                sisdb_client_ask_del(context, ask);
            }
        }
    }
    if (context->status == SIS_CLI_STATUS_AUTH)
    {
        // 判断是否验证通过
        if(msg_->switchs.sw_tag && msg_->tag == SIS_NET_TAG_OK)
        {        
            // 发送订阅信息
            sis_safe_map_lock(context->asks);
            s_sis_dict_entry *de;
            s_sis_dict_iter *di = sis_dict_get_iter(context->asks->map);
            while ((de = sis_dict_next(di)) != NULL)
            {
                s_sisdb_client_ask *ask = (s_sisdb_client_ask *)sis_dict_getval(de);
                if (ask->issub)
                {
                    sisdb_client_send_ask(context, ask);
                }
            }
            sis_dict_iter_free(di);
            sis_safe_map_unlock(context->asks);
            // 设置工作状态
            context->status = SIS_CLI_STATUS_WORK;
        }
        else
        {
            LOG(5)("auth fail. %d %s\n", msg_->tag, msg_->info ? msg_->info : "nil");
        }
    }    	
}

static void _cb_connected(void *source_, int sid)
{
    s_sisdb_client_cxt *context = (s_sisdb_client_cxt *)source_;

    sis_net_class_set_cb(context->session, sid, context, _cb_recv);

    // 连接后判断是否需要发送登录密码 
    if (context->status != SIS_CLI_STATUS_WORK && context->status != SIS_CLI_STATUS_EXIT)
    {
        if (context->auth)
        {
            // 发送验证信息
            context->status = SIS_CLI_STATUS_AUTH;
            context->cid = sid;
            s_sis_net_message *msg = sis_net_message_create();
            msg->cid = context->cid;
            char ask[128];
            sis_sprintf(ask, 128, "{\"username\":\"%s\",\"password\":\"%s\"}", context->username, context->password);
            sis_net_message_set_cmd(msg, "auth");
            sis_net_message_set_char(msg, ask, sis_strlen(ask));
            sis_net_class_send(context->session, msg);
            sis_net_message_destroy(msg);
        }
        else
        {
            context->status = SIS_CLI_STATUS_WORK;
        }
    }
    if (context->cb_connected)
    {
        context->cb_connected(context->cb_source, NULL);
    }
}
static void _cb_disconnect(void *source_, int sid)
{
    s_sisdb_client_cxt *context = (s_sisdb_client_cxt *)source_;
    // 断开连接 
    sis_net_class_set_cb(context->session, sid, context, NULL);
    context->status = SIS_CLI_STATUS_INIT;
    if (context->cb_disconnect)
    {
        context->cb_disconnect(context->cb_source, NULL);
    }
}
void sisdb_client_method_init(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_client_cxt *context = (s_sisdb_client_cxt *)worker->context;

    context->session = sis_net_class_create(&context->url_cli);

    if (context->session)
    {
        context->session->cb_source = context;
        context->session->cb_connected = _cb_connected;
        context->session->cb_disconnect = _cb_disconnect;
        sis_net_class_open(context->session);
    } 
    SIS_WAIT_TIME(context->status == SIS_CLI_STATUS_WORK, 5000);
}
void sisdb_client_method_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_client_cxt *context = (s_sisdb_client_cxt *)worker->context;
    context->status = SIS_CLI_STATUS_EXIT;
    if (context->session)
    {
        sis_net_class_destroy(context->session);
        context->session = NULL;
    }
}
s_sisdb_client_ask *sisdb_client_ask_cmd(s_sisdb_client_cxt *context, 
    const char   *cmd_,                // 请求的参数
	const char   *key_,                // 请求的key
	void         *val_,                // 请求的参数
    size_t        vlen_,
    void         *cb_source_,          // 回调传送对象
    void         *cb_reply)            // 回调的数据
{
    s_sisdb_client_ask *ask = sisdb_client_ask_create(cmd_, key_, val_, vlen_, cb_source_, cb_reply);
    ask->sno = (context->ask_sno++) % 0xFFFFFF;
    char serial[32];
    sis_llutoa(ask->sno, serial, 32, 10);
    sis_safe_map_set(context->asks, serial, ask);  
    return ask;    
}

void sisdb_client_ask_del(s_sisdb_client_cxt *context, s_sisdb_client_ask *ask_)
{
    if (ask_)
    {
        char serial[32];
        sis_llutoa(ask_->sno, serial, 32, 10);
        if (ask_)
        {
            // printf("del %d %s\n", (unsigned int)sis_thread_self(), serial);
            sis_safe_map_del(context->asks, serial);
        }
        else
        {
            sisdb_client_ask_destroy(ask_);
        }      
    }
}

s_sisdb_client_ask *sisdb_client_ask_get(
    s_sisdb_client_cxt *context, 	
    const char   *serial         // 来源信息
)
{
    return (s_sisdb_client_ask *)sis_safe_map_get(context->asks, serial);
}

void sisdb_client_ask_unsub(s_sisdb_client_cxt *context, s_sisdb_client_ask *ask)    
{
    if (!ask || !ask->issub)
    {
        return ;
    }
    char service[128], cmd[128]; 
    sis_str_divide(ask->cmd, '.', service, cmd);

    s_sis_net_message *msg = sis_net_message_create();
    msg->cid = context->cid;
    sis_net_message_set_cmd(msg, "unsub");
    sis_net_message_set_subject(msg, ask->key, NULL);
    sis_net_message_set_char(msg, ask->val, ask->val ? sis_sdslen(ask->val) : 0);
    sis_net_class_send(context->session, msg);
    sis_net_message_destroy(msg);
    // 订阅结束后 删除本地订阅信息
    sisdb_client_ask_del(context, ask);
}

bool sisdb_client_ask_sub_exists(
    s_sisdb_client_cxt *context, 	
    const char   *cmd_,         // 来源信息
    const char   *key_         // 来源信息
)
{
    bool exists = false;
    sis_safe_map_lock(context->asks);
    s_sis_dict_entry *de;
    s_sis_dict_iter *di = sis_dict_get_iter(context->asks->map);
    while ((de = sis_dict_next(di)) != NULL)
    {
        s_sisdb_client_ask *ask = (s_sisdb_client_ask *)sis_dict_getval(de);
        if (ask->issub && !sis_strcasecmp(ask->cmd, cmd_) && !sis_strcasecmp(ask->key, key_))
        {
            exists = true;
            break;
        }
    }
    sis_dict_iter_free(di);
    sis_safe_map_unlock(context->asks);
    return exists;
}

static int cb_reply(void *worker_, int rans_, const char *key_, const char *val_, size_t vsize_)
{
    s_sis_message *msg = (s_sis_message *)worker_;
    
    sis_net_message_set_subject(msg, key_, NULL);
    sis_net_message_set_info(msg, (void *)val_, vsize_);
    sis_net_message_set_tag(msg, rans_);

    return 0; 
}
int cmd_sisdb_client_setcb(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_client_cxt *context = (s_sisdb_client_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    context->cb_source     = sis_message_get(msg, "cb_source");
    context->cb_connected  = sis_message_get_method(msg, "cb_connected" );
    context->cb_disconnect = sis_message_get_method(msg, "cb_disconnect");
    return SIS_METHOD_OK;
}
int cmd_sisdb_client_status(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_client_cxt *context = (s_sisdb_client_cxt *)worker->context;
    return context->status == SIS_CLI_STATUS_WORK;
}
int cmd_sisdb_client_chars_wait(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_client_cxt *context = (s_sisdb_client_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    if (!msg || context->status != SIS_CLI_STATUS_WORK)
    {
        return SIS_METHOD_NOWORK;
    }
    s_sisdb_client_ask *ask = sisdb_client_ask_cmd( 
		context,
		(const char *)sis_message_get(msg, "cmd"), 
		(const char *)sis_message_get(msg, "key"), 
		sis_message_get(msg, "val"), 
        sis_message_get_int(msg, "vlen"), 
		msg, 
		cb_reply);

    ask->format = SIS_NET_FORMAT_CHARS;
    msg->switchs.sw_tag = 0;

    sisdb_client_send_ask(context, ask);

    while(!msg->switchs.sw_tag && !SIS_SERVER_EXIT)
    {
        sis_sleep(1);
    }
    // printf("start 1 : %u\n", (unsigned int)sis_thread_self());
    // sis_wait_start(&context->wait);
    if (msg->switchs.sw_tag)
    {
        if (msg->tag < SIS_NET_TAG_OK)
        {
            return SIS_METHOD_ERROR; 
        }
        else
        {
            return SIS_METHOD_OK;    
        }
    }
    return SIS_METHOD_NOANS; 
}

int cmd_sisdb_client_bytes_wait(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_client_cxt *context = (s_sisdb_client_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    if (!msg || context->status != SIS_CLI_STATUS_WORK)
    {
        return SIS_METHOD_NOWORK;
    }

    s_sisdb_client_ask *ask = sisdb_client_ask_cmd( 
		context,
		(const char *)sis_message_get(msg, "cmd"), 
		(const char *)sis_message_get(msg, "key"), 
		sis_message_get(msg, "val"), 
        sis_message_get_int(msg, "vlen"), 
		msg, 
		cb_reply);

    ask->format = SIS_NET_FORMAT_BYTES;
    msg->switchs.sw_tag = 0;

    sisdb_client_send_ask(context, ask);

    // 这里如果时间太久会应答错误 以后再处理
    // int waitmsec = 10000;
    // while(!msg->switchs.is_reply && waitmsec > 0)
    // {
    //     sis_sleep(1);
    //     waitmsec--;
    // }
    while(!msg->switchs.sw_tag && !SIS_SERVER_EXIT)
    {
        sis_sleep(1);
    }
    // printf("start 1 : %d\n", (unsigned int)sis_thread_self());
    // sis_wait_start(&context->wait);
    if (msg->switchs.sw_tag)
    {
        if (msg->tag < SIS_NET_TAG_OK)
        {
            return SIS_METHOD_ERROR; 
        }
        else
        {
            return SIS_METHOD_OK;    
        }
    }
    return SIS_METHOD_NOANS;
}

int cmd_sisdb_client_chars_nowait(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_client_cxt *context = (s_sisdb_client_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    if (!msg || context->status != SIS_CLI_STATUS_WORK)
    {
        return SIS_METHOD_NOWORK;
    }
    s_sisdb_client_ask *ask = sisdb_client_ask_cmd( 
		context,
		(const char *)sis_message_get(msg, "cmd"), 
		(const char *)sis_message_get(msg, "key"), 
		sis_message_get(msg, "val"), 
        sis_message_get_int(msg, "vlen"),
        sis_message_get(msg, "cb_source"), 
        sis_message_get(msg, "cb_reply"));

    ask->format = SIS_NET_FORMAT_CHARS;
    
    sisdb_client_send_ask(context, ask);

    return SIS_METHOD_OK;    
}

int cmd_sisdb_client_bytes_nowait(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sisdb_client_cxt *context = (s_sisdb_client_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    if (!msg || context->status != SIS_CLI_STATUS_WORK)
    {
        return SIS_METHOD_NOWORK;
    }

    s_sisdb_client_ask *ask = sisdb_client_ask_cmd( 
		context,
		(const char *)sis_message_get(msg, "cmd"), 
		(const char *)sis_message_get(msg, "key"), 
		sis_message_get(msg, "val"), 
        sis_message_get_int(msg, "vlen"),
        sis_message_get(msg, "cb_source"), 
        sis_message_get(msg, "cb_reply"));

    ask->format = SIS_NET_FORMAT_BYTES;

    sisdb_client_send_ask(context, ask);

    return SIS_METHOD_OK;    
}

