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

bool sisdb_server_init(void *worker_, void *argv_)
{
    return false;
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
