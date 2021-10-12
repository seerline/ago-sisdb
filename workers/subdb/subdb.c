#include "sis_modules.h"
#include "worker.h"

#include "subdb.h"
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method subdb_methods[] = {
    {"init",   cmd_subdb_init,  SIS_METHOD_ACCESS_NONET, NULL},
    {"set",    cmd_subdb_set,   SIS_METHOD_ACCESS_RDWR, NULL},
    {"start",  cmd_subdb_start, SIS_METHOD_ACCESS_RDWR, NULL},
    {"stop",   cmd_subdb_stop,  SIS_METHOD_ACCESS_RDWR, NULL},
    {"pub",    cmd_subdb_pub,   SIS_METHOD_ACCESS_RDWR, NULL},
    {"sub",    cmd_subdb_sub,   SIS_METHOD_ACCESS_READ, NULL},
    {"unsub",  cmd_subdb_unsub, SIS_METHOD_ACCESS_READ, NULL},
    {"wlog",   cmd_subdb_wlog,  0, NULL},
};
// 共享内存数据库 不落盘
s_sis_modules sis_modules_subdb = {
  subdb_init,
  NULL,
  NULL,
  NULL,
  subdb_uninit,
  NULL,
  NULL,
  sizeof(subdb_methods)/sizeof(s_sis_method),
  subdb_methods,
};

int subdb_userinfo_incr(s_subdb_cxt *context, int cid)
{
    s_subdb_userinfo *o = sis_map_kint_get(context->map_users, cid);
    if (!o)
    {
        o = SIS_MALLOC(s_subdb_userinfo, o);
        o->cid = cid;
        o->status = SIS_SUB_STATUS_INIT;
        sis_map_kint_set(context->map_users, cid, o);
        return 1;
    }
    return 0;
}
void subdb_userinfo_decr(s_subdb_cxt *context, int cid)
{
    printf("subdb_userinfo_decr: %d %d\n", cid, context->work_readers); 
    if (cid == -1)
    {
        sis_map_kint_clear(context->map_users); 
        context->work_readers = 0;
    }
    else
    {
        sis_map_kint_del(context->map_users, cid);
        context->work_readers--;  
        context->work_readers = sis_max(context->work_readers, 0);
    }
}

bool  subdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_subdb_cxt *context = SIS_MALLOC(s_subdb_cxt, context);
    worker->context = context;

	const char *workname = sis_json_get_str(node, "work-name");
	if (workname)
	{
		context->work_name = sis_sdsnew(workname);
	}
	else
	{
		context->work_name = sis_sdsnew(node->key);
	}
	const char *workpath = sis_json_get_str(node, "work-path");
	if (workpath)
	{
		context->work_path = sis_sdsnew(workpath);
	}
	else
	{
		context->work_path = sis_sdsnew("./");
	}
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
    context->work_sub_cxt = sisdb_sub_cxt_create();
    context->map_users = sis_map_kint_create();
    context->map_users->type->vfree = sis_free_call;


    return true;
}
void  subdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_subdb_cxt *context = (s_subdb_cxt *)worker->context;

    if (context->wlog_worker)
    {
        sis_worker_destroy(context->wlog_worker);
		context->wlog_worker = NULL;
    }
    sis_map_kint_destroy(context->map_users);
    sisdb_sub_cxt_destroy(context->work_sub_cxt);
    sis_sdsfree(context->work_keys);
    sis_sdsfree(context->work_sdbs);
    sis_sdsfree(context->work_path);
    sis_sdsfree(context->work_name);
    sis_free(context);
}
int cmd_subdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_subdb_cxt *context = (s_subdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    
	context->cb_source = sis_message_get(msg, "cb_source");
	context->cb_net_message = sis_message_get_method(msg, "cb_net_message");

    sisdb_sub_cxt_init(context->work_sub_cxt, context->cb_source, context->cb_net_message);
    // 初始化用户信息
    // s_sis_dict_entry *de;
    // s_sis_dict_iter *di = sis_dict_get_iter(context->map_users);
    // while ((de = sis_dict_next(di)) != NULL)
    // {
    //     s_subdb_userinfo *userinfo = (s_subdb_userinfo *)sis_dict_getval(de);
    //     userinfo->status = SIS_SUB_STATUS_INIT;
    // }
    // sis_dict_iter_free(di);
    // context->work_readers = 0;
    return SIS_METHOD_OK;
}

void subdb_wlog_save(s_subdb_cxt *context, s_sis_net_message *netmsg)
{
    if (context->wlog_init == 1)
    {
        context->wlog_method->proc(context->wlog_worker, netmsg);
    }
}
void subdb_wlog_stop(s_subdb_cxt *context)
{
    if (context->wlog_init == 1)
    {
        sis_worker_command(context->wlog_worker, "close", NULL); 
        context->wlog_init = 0;
    }
}
void subdb_wlog_start(s_subdb_cxt *context)
{
    if (context->wlog_init == 1)
    {
        subdb_wlog_stop(context);
    }
	s_sis_message *msg = sis_message_create();
	sis_message_set_str(msg, "work-path", context->work_path, sis_sdslen(context->work_path));
	sis_message_set_str(msg, "work-name", context->work_name, sis_sdslen(context->work_name));
	sis_message_set_int(msg, "work-date", context->work_date);
	sis_worker_command(context->wlog_worker, "open", msg); 
	sis_message_destroy(msg);
    context->wlog_init = 1;
}


int cmd_subdb_set(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_subdb_cxt *context = (s_subdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    if (context->wlog_worker)
    {
        subdb_wlog_save(context, netmsg);
    }

	if (!sis_strcasecmp(netmsg->key, "_keys_") && netmsg->switchs.has_ask)
	{
		sis_sdsfree(context->work_keys);
		context->work_keys = sis_sdsdup(netmsg->ask);
	}
	if (!sis_strcasecmp(netmsg->key, "_sdbs_") && netmsg->ask)
	{
		sis_sdsfree(context->work_sdbs);
		// 从外界过来的sdbs可能格式不对，需要转换
		s_sis_json_handle *injson = sis_json_load(netmsg->ask, sis_sdslen(netmsg->ask));
		if (!injson)
		{
			return SIS_METHOD_ERROR;
		}
		context->work_sdbs = sis_json_to_sds(injson->node, true);
		sis_json_close(injson);
	}
    return SIS_METHOD_OK;
}

int cmd_subdb_start(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_subdb_cxt *context = (s_subdb_cxt *)worker->context;
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

	SIS_NET_SHOW_MSG("start", netmsg);
    s_sis_sds sdate = sis_net_get_val(netmsg);
	int workdate = sdate ? sis_atoll(sdate) : sis_time_get_idate(0);

    {
        context->work_date = workdate;
        // 初始化用户信息
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(context->map_users);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_subdb_userinfo *userinfo = (s_subdb_userinfo *)sis_dict_getval(de);
            userinfo->status = SIS_SUB_STATUS_INIT;
        }
        sis_dict_iter_free(di);
        context->work_readers = 0;
        context->status = SIS_SUB_STATUS_INIT;
    }

    if (context->wlog_worker)
    {
        subdb_wlog_start(context);
        subdb_wlog_save(context, netmsg);
    }
	LOG(5)("subdb init ok : %d %d\n", context->status, context->work_date);
	return SIS_METHOD_OK;
}

int cmd_subdb_stop(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_subdb_cxt *context = (s_subdb_cxt *)worker->context;
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	if (context->status == SIS_SUB_STATUS_STOP || context->status == SIS_SUB_STATUS_NONE)
	{
		return SIS_METHOD_ERROR;
	}
    {
        char sdate[32];
        sis_llutoa(context->work_date, sdate, 32, 10);
        s_sis_net_message *netmsg = sis_net_message_create();
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(context->map_users);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_subdb_userinfo *userinfo = (s_subdb_userinfo *)sis_dict_getval(de);
            if (userinfo->status != SIS_SUB_STATUS_STOP)
            {
                LOG(5)("send sub stop.[%d] %s\n", userinfo->cid, sdate);
                netmsg->cid = userinfo->cid;
                // netmsg->name = userinfo->serial ? sis_sdsdup(userinfo->serial) : NULL;
                sis_net_ans_with_sub_stop(netmsg, sdate);
                context->cb_net_message(context->cb_source, netmsg);
                userinfo->status = SIS_SUB_STATUS_STOP;
            }
        }
        sis_dict_iter_free(di);
        sis_net_message_destroy(netmsg);
        context->work_readers = 0;
        context->status = SIS_SUB_STATUS_STOP; // 停止表示当日数据已经落盘
    }
    if (context->wlog_worker)
    {
        subdb_wlog_save(context, netmsg);
        subdb_wlog_stop(context);
    }
    return SIS_METHOD_OK;
}
bool _subdb_reader_init(s_subdb_cxt *context)
{
    if (context->status == SIS_SUB_STATUS_STOP || context->status == SIS_SUB_STATUS_NONE)
    {
        return false;
    }
    if (context->status == SIS_SUB_STATUS_INIT)
    {
        if (!context->work_keys || !context->work_sdbs)
        {
            return false;
        }
        context->status = SIS_SUB_STATUS_WORK;
    }
    int count = sis_map_kint_getsize(context->map_users);
    if (count <= context->work_readers)
    {
        return true;
    }
    LOG(5)("subdb start. %d readers : %d : %d\n", context->work_date, count, context->work_readers);
    char sdate[32];
    sis_llutoa(context->work_date, sdate, 32, 10);
    s_sis_net_message *netmsg = sis_net_message_create();
    s_sis_dict_entry *de;
    s_sis_dict_iter *di = sis_dict_get_iter(context->map_users);
    while ((de = sis_dict_next(di)) != NULL)
    {
        s_subdb_userinfo *userinfo = (s_subdb_userinfo *)sis_dict_getval(de);
        LOG(5)("send sub start.[%d] %d %s\n", userinfo->cid, userinfo->status, sdate);
        if (userinfo->status != SIS_SUB_STATUS_INIT)
        {
            continue;
        }
        // netmsg->name = userinfo->serial ? sis_sdsdup(userinfo->serial) : NULL;
        netmsg->cid = userinfo->cid;
        sis_net_ans_with_sub_start(netmsg, sdate);
        context->cb_net_message(context->cb_source, netmsg);
        sis_net_message_clear(netmsg);
        netmsg->cid = userinfo->cid;
        sis_message_set_key(netmsg, "_keys_", NULL);
        sis_net_ans_with_chars(netmsg, context->work_keys, sis_sdslen(context->work_keys));
        context->cb_net_message(context->cb_source, netmsg);
        sis_net_message_clear(netmsg);
        netmsg->cid = userinfo->cid;
        sis_message_set_key(netmsg, "_sdbs_", NULL);
        sis_net_ans_with_chars(netmsg, context->work_sdbs, sis_sdslen(context->work_sdbs));
        context->cb_net_message(context->cb_source, netmsg);
        sis_net_message_clear(netmsg);
        netmsg->cid = userinfo->cid;
        sis_net_ans_with_sub_wait(netmsg, sdate);
        context->cb_net_message(context->cb_source, netmsg);
        userinfo->status = SIS_SUB_STATUS_WORK;
        context->work_readers ++;
    }
    sis_dict_iter_free(di);
    sis_net_message_destroy(netmsg);
    return true;
}
int cmd_subdb_pub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_subdb_cxt *context = (s_subdb_cxt *)worker->context;
    if (!_subdb_reader_init(context))
    {
        return SIS_METHOD_ERROR;
    }
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    if (context->wlog_worker)
    {
        subdb_wlog_save(context, netmsg);
    }
    sisdb_sub_cxt_pub(context->work_sub_cxt, netmsg);
    // SIS_NET_SHOW_MSG("subdb pub", netmsg);
    sis_net_ans_with_noreply(netmsg);
    return SIS_METHOD_OK;
}

int cmd_subdb_sub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_subdb_cxt *context = (s_subdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    subdb_userinfo_incr(context, netmsg->cid);
    int o = sisdb_sub_cxt_sub(context->work_sub_cxt, netmsg);
    sis_net_ans_with_int(netmsg, o);
    return SIS_METHOD_OK;
}

int cmd_subdb_unsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_subdb_cxt *context = (s_subdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    subdb_userinfo_decr(context, netmsg->cid);
    int o = sisdb_sub_cxt_unsub(context->work_sub_cxt, netmsg->cid);
    sis_net_ans_with_int(netmsg, o);
     return SIS_METHOD_OK;
}

int cmd_subdb_wlog(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_subdb_cxt *context = (s_subdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    if (context->wlog_worker)
    {
        subdb_wlog_save(context, netmsg);
    }
    return SIS_METHOD_OK;
}