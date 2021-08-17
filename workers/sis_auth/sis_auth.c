#include "sis_modules.h"
#include "worker.h"

#include "sis_auth.h"
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sis_auth_methods[] = {
    {"get",    sis_auth_get,    0, NULL},
    {"set",    sis_auth_set,    0, NULL},
    {"login",  sis_auth_login,  0, NULL},
    {"access", sis_auth_access, 0, NULL},
};
// 共享内存数据库 不落盘
s_sis_modules sis_modules_sis_auth = {
    sis_auth_init,
    NULL,
    NULL,
    NULL,
    sis_auth_uninit,
    NULL,
    NULL,
    sizeof(sis_auth_methods) / sizeof(s_sis_method),
    sis_auth_methods,
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

bool sis_auth_init(void *worker_, void *node_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_json_node *node = (s_sis_json_node *)node_;

    s_sis_auth_cxt *context = SIS_MALLOC(s_sis_auth_cxt, context);
    worker->context = context;
    if (!node)
    {
        context->status = 0; 
        return true;  
    }
    context->users = sis_map_pointer_create_v(sis_userinfo_destroy);
    // 1.先加载node中信息 
    // 2.再加载磁盘信息 磁盘信息有就替换node的信息 如果都没有信息 默认无需校验任何请求
    // 3.设置status=1开始工作
    s_sis_json_node *next = sis_json_first_node(node);
    while(next)
    {
        const char *access = sis_json_get_str(next,"access");
        int iaccess = SIS_ACCESS_READ;
        if (!sis_strcasecmp(access, SIS_ACCESS_SWRITE))
        {
            iaccess = SIS_ACCESS_WRITE;
        }
        else if (!sis_strcasecmp(access, SIS_ACCESS_SPUBSUB))
        {
            iaccess = SIS_ACCESS_PUBSUB;
        }
        else if (!sis_strcasecmp(access, SIS_ACCESS_SADMIN))
        {
            iaccess= SIS_ACCESS_ADMIN;
        }  
        s_sis_userinfo *userinfo = sis_userinfo_create(
            sis_json_get_str(next,"username"), 
            sis_json_get_str(next,"password"), 
            iaccess);
        sis_map_pointer_set(context->users, userinfo->username, userinfo);
        next = next->next;
    }      
    context->status = sis_map_pointer_getsize(context->users) > 0 ? 1 : 2; 
    return true;
}
void sis_auth_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_sis_auth_cxt *context = (s_sis_auth_cxt *)worker->context;
    if (context->users)
    {
        sis_map_pointer_destroy(context->users);
    }
    sis_free(context);
}

int sis_auth_get(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_;
    s_sis_auth_cxt *context = (s_sis_auth_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;
    const char *username = (const char *)sis_message_get(msg, "username");
    s_sis_userinfo *userinfo = sis_map_pointer_get(context->users, username);
    if (userinfo)
    {
        sis_message_set(msg, "userinfo", userinfo, NULL);
        return SIS_METHOD_OK;
    }
    return SIS_METHOD_ERROR;
}
int sis_auth_set(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_;
    s_sis_auth_cxt *context = (s_sis_auth_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

    const char *username = (const char *)sis_message_get(msg, "username");
    s_sis_userinfo *userinfo = sis_map_pointer_get(context->users, username);
    if (!userinfo)
    {
        s_sis_userinfo *userinfo = sis_userinfo_create(
            (const char *)sis_message_get(msg, "username"),
            (const char *)sis_message_get(msg, "password"),
            sis_message_get_int(msg, "access"));
        sis_map_pointer_set(context->users, userinfo->username, userinfo);        
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

int sis_auth_access(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_;
    s_sis_auth_cxt *context = (s_sis_auth_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

    const char *username = (const char *)sis_message_get(msg, "username");
    s_sis_userinfo *userinfo = sis_map_pointer_get(context->users, username);
    if (!userinfo)
    {
        return SIS_METHOD_ERROR;   
    }
    sis_message_set_int(msg, "access", userinfo->access);
    return SIS_METHOD_OK;   
}
int sis_auth_login(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_;
    s_sis_auth_cxt *context = (s_sis_auth_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

    const char *username = (const char *)sis_message_get(msg, "username");
    s_sis_userinfo *userinfo = sis_map_pointer_get(context->users, username);
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
