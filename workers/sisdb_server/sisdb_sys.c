#include "worker.h"
#include "sis_conf.h"
#include "sisdb_sys.h"
// 创建用户对象，其中用户名密码等均采用sds字符串
s_sisdb_userinfo *sis_userinfo_create(const char *username_, const char *password_, int access_)
{
    s_sisdb_userinfo *o = SIS_MALLOC(s_sisdb_userinfo, o);
    o->username = sis_sdsnew(username_);
    o->password = sis_sdsnew(password_);
    printf("%s %s | %s %s\n", o->username, o->password, username_, password_);
    o->access = access_;
    return o;
}
void sis_userinfo_destroy(void *userinfo_)
{
    s_sisdb_userinfo *userinfo = (s_sisdb_userinfo *)userinfo_;
    sis_sdsfree(userinfo->username);
    sis_sdsfree(userinfo->password);
    sis_free(userinfo);
}
s_sisdb_workinfo *sis_workinfo_create_of_json(s_sis_json_node *incfg_)
{
    s_sisdb_workinfo *o = SIS_MALLOC(s_sisdb_workinfo, o);
    o->workname = sis_sdsnew(incfg_->key);
    o->config = sis_json_clone(incfg_, 1);
    o->work_status = 0;
    return o;
}

s_sisdb_workinfo *sis_workinfo_create(const char *name_, const char *config_)
{
    s_sisdb_workinfo *o = SIS_MALLOC(s_sisdb_workinfo, o);
    o->workname = sis_sdsnew(name_);
    s_sis_json_handle *handle = NULL;
    if (config_)
    {
        handle = sis_json_load(config_, sis_strlen(config_));
    }
    if (handle)
    {
        o->config = sis_json_clone(handle->node, 1);
        sis_json_close(handle);
    }
    else
    {
        o->config = sis_json_create_object();
        sis_json_object_add_string(o->config, "classname", name_, sis_strlen(name_));
    }
    o->work_status = 0;
    return o;
}

void sis_workinfo_destroy(void *workinfo_)
{
    s_sisdb_workinfo *workinfo = (s_sisdb_workinfo *)workinfo_;
    sis_sdsfree(workinfo->workname);
    // printf("===free %p %p\n", workinfo, workinfo->config);
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

const char *sis_sys_access_itoa(int access)
{
    if (access == SIS_ACCESS_ADMIN)
    {
        return SIS_ACCESS_SADMIN;
    }
    else  if (access == SIS_ACCESS_WORKER)
    {
        return SIS_ACCESS_SWORKER;
    }
    else  if (access == SIS_ACCESS_WRITE)
    {
        return SIS_ACCESS_SWRITE;
    }
    else
    {
        return SIS_ACCESS_SREAD;
    } 
}
int sis_sys_access_atoi(const char * access)
{
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
    return iaccess;
}

bool sis_sys_access_method(s_sis_method *method, int access)
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