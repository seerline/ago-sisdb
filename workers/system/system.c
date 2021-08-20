#include "sis_modules.h"
#include "worker.h"

#include "system.h"
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////
// 所有方法只限于内部调用 不支持网络访问
struct s_sis_method system_methods[] = {
    {"login",  system_login,       SIS_METHOD_ACCESS_NONET, NULL},
    {"check",  system_check,       SIS_METHOD_ACCESS_NONET, NULL},
    {"get",    system_get_user,    SIS_METHOD_ACCESS_NONET, NULL},
    {"set",    system_set_user,    SIS_METHOD_ACCESS_NONET, NULL},
    {"open",   system_open_work,   SIS_METHOD_ACCESS_NONET, NULL},
    {"stop",   system_stop_work,   SIS_METHOD_ACCESS_NONET, NULL},
    {"save",   system_save,        SIS_METHOD_ACCESS_NONET, NULL},
    {"load",   system_load,        SIS_METHOD_ACCESS_NONET, NULL},
};
// 共享内存数据库 不落盘
s_sis_modules sis_modules_system = {
    system_init,
    NULL,
    NULL,
    NULL,
    system_uninit,
    NULL,
    NULL,
    sizeof(system_methods) / sizeof(s_sis_method),
    system_methods,
};

s_sis_userinfo *sis_userinfo_create(const char *username_, const char *password_, int access_)
{
    s_sis_userinfo *o = SIS_MALLOC(s_sis_userinfo, o);
    o->username = sis_sdsnew(username_);
    o->password = sis_sdsnew(password_);
    o->access = access_;
    return o;
}
void sis_userinfo_destroy(void *userinfo_)
{
    s_sis_userinfo *userinfo = (s_sis_userinfo *)userinfo_;
    sis_sdsfree(userinfo->username);
    sis_sdsfree(userinfo->password);
    sis_free(userinfo);
}
s_sis_workinfo *sis_workinfo_create(s_sis_json_node *incfg_)
{
    s_sis_workinfo *o = SIS_MALLOC(s_sis_workinfo, o);
    o->workname = sis_sdsnew(incfg_->key);
    o->config = sis_json_clone(incfg_, 1);
    o->status = 0;
    return o;
}

void sis_workinfo_destroy(void *workinfo_)
{
    s_sis_workinfo *workinfo = (s_sis_workinfo *)workinfo_;
    sis_sdsfree(workinfo->workname);
    if (workinfo->config)
    {
        sis_json_delete_node(workinfo->config);
    }
    if (workinfo->worker)
    {
        sis_worker_destroy(workinfo->worker);
    }
    sis_free(workinfo);
}

void _system_load(s_sis_worker *worker, s_sis_json_node *node)
{
    s_system_cxt *context = (s_system_cxt *)worker->context;
    s_sis_json_node *userinfos = sis_json_cmp_child_node(node, "userinfos");
    if(userinfos) 
    {
        s_sis_json_node *next = sis_json_first_node(userinfos);
        while(next)
        {
            const char *access = sis_json_get_str(next,"access");
            int iaccess = SIS_ACCESS_READ;
            if (!sis_strcasecmp(access, SIS_ACCESS_SWRITE))
            {
                iaccess = SIS_ACCESS_WRITE;
            }
            else if (!sis_strcasecmp(access, SIS_ACCESS_SWORKER))
            {
                iaccess = SIS_ACCESS_WORKER;
            }
            else if (!sis_strcasecmp(access, SIS_ACCESS_SADMIN))
            {
                iaccess= SIS_ACCESS_ADMIN;
            }  
            s_sis_userinfo *userinfo = sis_userinfo_create(
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
            s_sis_workinfo *workinfo = sis_workinfo_create(next);
            sis_map_list_set(context->works, next->key, workinfo);
            next = next->next;
        }
    }   
}
void _system_save(s_sis_worker *worker)
{
    s_system_cxt *context = (s_system_cxt *)worker->context;
	s_sis_json_node *jone = sis_json_create_object();
	s_sis_json_node *jusers = sis_json_create_object();
    s_sis_json_node *jworks = sis_json_create_object();

    {
        // 打开所有工作
        int count = sis_map_list_getsize(context->users);
        for (int i = 0; i < count; i++)
        {
            s_sis_userinfo *userinfo = sis_map_list_geti(context->users, i);
            s_sis_json_node *juser = sis_json_create_object();
            sis_json_object_add_string(juser, "password", userinfo->password, sis_sdslen(userinfo->password));
            if (userinfo->access == SIS_ACCESS_ADMIN)
            {
                sis_json_object_add_string(juser, "access", SIS_ACCESS_SADMIN, sis_strlen(SIS_ACCESS_SADMIN));
            }
            else  if (userinfo->access == SIS_ACCESS_WORKER)
            {
                sis_json_object_add_string(juser, "access", SIS_ACCESS_SWORKER, sis_strlen(SIS_ACCESS_SWORKER));
            }
            else  if (userinfo->access == SIS_ACCESS_WRITE)
            {
                sis_json_object_add_string(juser, "access", SIS_ACCESS_SWRITE, sis_strlen(SIS_ACCESS_SWRITE));
            }
            else
            {
                sis_json_object_add_string(juser, "access", SIS_ACCESS_SREAD, sis_strlen(SIS_ACCESS_SREAD));
            }
            sis_json_object_add_node(jusers, userinfo->username, juser);
        }  
    }
	sis_json_object_add_node(jone, "userinfos", jusers); 
    {
        // 打开所有工作
        int count = sis_map_list_getsize(context->works);
        for (int i = 0; i < count; i++)
        {
            s_sis_workinfo *workinfo = sis_map_list_geti(context->works, i);
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
bool system_init(void *worker_, void *node_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)node_;

    s_system_cxt *context = SIS_MALLOC(s_system_cxt, context);
    worker->context = context;
    if (!node)
    {
        context->status = 0; 
        return true;  
    }
    context->users = sis_map_list_create(sis_userinfo_destroy);
    context->works = sis_map_list_create(sis_workinfo_destroy);

    _system_load(worker, node);

    return true;
}
void system_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_system_cxt *context = (s_system_cxt *)worker->context;
    if (context->users)
    {
        sis_map_list_destroy(context->users);
    }
    if (context->works)
    {
        sis_map_list_destroy(context->works);
    }
    sis_free(context);
}

int system_get_user(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_;
    s_system_cxt *context = (s_system_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    const char *username = (const char *)sis_message_get(msg, "username");
    s_sis_userinfo *userinfo = sis_map_list_get(context->users, username);
    if (userinfo)
    {
        sis_message_set(msg, "userinfo", userinfo, NULL);
        return SIS_METHOD_OK;
    }
    return SIS_METHOD_ERROR;
}
int system_set_user(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_;
    s_system_cxt *context = (s_system_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

    const char *username = (const char *)sis_message_get(msg, "username");
    s_sis_userinfo *userinfo = sis_map_list_get(context->users, username);
    if (!userinfo)
    {
        s_sis_userinfo *userinfo = sis_userinfo_create(
            (const char *)sis_message_get(msg, "username"),
            (const char *)sis_message_get(msg, "password"),
            sis_message_get_int(msg, "access"));
        sis_map_list_set(context->users, userinfo->username, userinfo);        
    }
    else
    {
        if (sis_message_exist(msg, "password"))
        {
            sis_sdsfree(userinfo->password);
            userinfo->password = sis_sdsnew((const char *)sis_message_get(msg, "password"));
        }
        if (sis_message_exist(msg, "access"))
        {
            userinfo->access = sis_message_get_int(msg, "access");
        }
    }
    return SIS_METHOD_OK;
}

int system_login(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_;
    s_system_cxt *context = (s_system_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

    const char *username = (const char *)sis_message_get(msg, "username");
    s_sis_userinfo *userinfo = sis_map_list_get(context->users, username);
    if (!userinfo)
    {
        return SIS_METHOD_ERROR;   
    }
    const char *password = (const char *)sis_message_get(msg, "password");
    if (sis_strcasecmp(userinfo->password, password))
    {
        return SIS_METHOD_ERROR;   
    }
    return SIS_METHOD_OK;
}

int system_check(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_;
    s_system_cxt *context = (s_system_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    if (context->status == 0)
    {
        // 没有权限设置 全部返回真
        return SIS_METHOD_OK;
    }
    const char *username = (const char *)sis_message_get(msg, "username");
    s_sis_userinfo *userinfo = sis_map_list_get(context->users, username);
    if (!userinfo)
    {
        return SIS_METHOD_NIL;   
    }
    int method_access = sis_message_get_int(msg, "access");
    if (!method_access)
    {
        // 方法未做任何限制
        return SIS_METHOD_OK;
    }
    if (method_access & SIS_METHOD_ACCESS_NONET)
    {
        // 方法只能内部调用
        return SIS_METHOD_ERROR;
    }
    if (userinfo->access == SIS_METHOD_ACCESS_ADMIN)
    {
        // 用户权限高 但也不能使用内部调用方法
        return SIS_METHOD_OK;
    }
    if (userinfo->access == SIS_METHOD_ACCESS_WRITE && 
        method_access & SIS_METHOD_ACCESS_DEL)
    {
        // WRITE 只能写 但方法需要删除 需要 SIS_METHOD_ACCESS_WORKER 权限
        return SIS_METHOD_ERROR;
    }
    if (userinfo->access == SIS_METHOD_ACCESS_READ &&
        (method_access & SIS_METHOD_ACCESS_DEL ||
         method_access & SIS_METHOD_ACCESS_WRITE))
    {
        return SIS_METHOD_ERROR;
    }
    return SIS_METHOD_OK;
}
int system_open_work(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_;
    s_system_cxt *context = (s_system_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

    const char *workname = (const char *)sis_message_get(msg, "workname");
    if (!workname)
    {
        // 打开所有工作
        int count = sis_map_list_getsize(context->works);
        for (int i = 0; i < count; i++)
        {
            s_sis_workinfo *workinfo = sis_map_list_geti(context->works, i);
            if (workinfo->status == 0)
            {
                sis_worker_command(workinfo->worker, "open", msg);
                workinfo->status = 1;
            }
        }  
    }
    else
    {
        s_sis_workinfo *workinfo = sis_map_list_get(context->works, workname);
        if (!workinfo)
        {
            s_sis_worker *worker = NULL;
            s_sis_json_node *cfgnode = NULL;
            const char *config = (const char *)sis_message_get(msg, "config");
            s_sis_json_handle *handle = sis_json_load(config, sis_strlen(config));
            if (handle)
            {
                worker = sis_worker_create(worker->father, handle->node);
                cfgnode = sis_json_clone(handle->node, 1);
                sis_json_close(handle);
            }
            else
            {
                worker = sis_worker_create(worker->father, NULL);
            }
            if (worker)
            {
                s_sis_workinfo *workinfo = SIS_MALLOC(s_sis_workinfo, workinfo);
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
        }
        if (workinfo->status == 0)
        {
            sis_worker_command(workinfo->worker, "open", msg);
            workinfo->status = 1;
        }
        sis_message_set(msg, "workinfo", workinfo, NULL);
    }
    return SIS_METHOD_OK;
}

int system_stop_work(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_;
    s_system_cxt *context = (s_system_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

    const char *workname = (const char *)sis_message_get(msg, "workname");
    if (!workname)
    {
        // 停止所有工作
        // 打开所有工作
        int count = sis_map_list_getsize(context->works);
        for (int i = 0; i < count; i++)
        {
            s_sis_workinfo *workinfo = sis_map_list_geti(context->works, i);
            if (workinfo->status == 0)
            {
                sis_worker_command(workinfo->worker, "stop", msg);
                workinfo->status = 1;
            }
        }  
    }
    else
    {
        s_sis_workinfo *workinfo = sis_map_list_get(context->works, workname);
        if (workinfo)
        {
            if (workinfo->status == 1)
            {
                sis_worker_command(workinfo->worker, "stop", msg);
                workinfo->status = 0;
            }
        }
    }
    return SIS_METHOD_OK;
}

int system_save(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_;
    
    // 保存到当前目录下的 .sisdb.json 文件中
    _system_save(worker);

    return SIS_METHOD_OK;
}

int system_load(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_;
    s_system_cxt *context = (s_system_cxt *)worker->context;

    s_sis_json_handle *handle = sis_json_open("./.sissys.json");
    if (!handle)
    {
        context->status = 0;
        return SIS_METHOD_ERROR;
    }    
    _system_load(worker, handle->node);
    sis_json_close(handle);
    context->status = 1;
    return SIS_METHOD_OK;
}
