#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"

#include "sisdb_catch.h"

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sisdb_catch_methods[] = {
    {"get", cmd_sisdb_catch_get, NULL, NULL},   // json 格式
    {"set", cmd_sisdb_catch_set, NULL, NULL},   // json 格式
    {"bget", cmd_sisdb_catch_bget, NULL, NULL}, // 二进制格式
    {"bset", cmd_sisdb_catch_bset, NULL, NULL}, // 二进制格式
    {"del", cmd_sisdb_catch_del, NULL, NULL},   // 删除数据
};
// 共享内存数据库
s_sis_modules sis_modules_sisdb_catch = {
    sisdb_catch_init,
    sisdb_catch_work_init,
    sisdb_catch_working,
    sisdb_catch_work_uninit,
    sisdb_catch_uninit,
    sisdb_catch_method_init,
    sisdb_catch_method_uninit,
    sizeof(sisdb_catch_methods) / sizeof(s_sis_method),
    sisdb_catch_methods,
};

bool sisdb_catch_init(void *worker_, void *argv_)
{
    return false;
}
void sisdb_catch_work_init(void *worker_)
{
}
void sisdb_catch_working(void *worker_)
{
}
void sisdb_catch_work_uninit(void *worker_)
{
}
void sisdb_catch_uninit(void *worker_)
{
}
void sisdb_catch_method_init(void *worker_)
{
}
void sisdb_catch_method_uninit(void *worker_)
{
}

int cmd_sisdb_catch_get(void *worker_, void *argv_)
{
    printf("%s\n", __func__);
    return 0;
}
int cmd_sisdb_catch_set(void *worker_, void *argv_)
{
    return 0;
}
int cmd_sisdb_catch_bget(void *worker_, void *argv_)
{
    return 0;
}
int cmd_sisdb_catch_bset(void *worker_, void *argv_)
{
    return 0;
}
int cmd_sisdb_catch_del(void *worker_, void *argv_)
{
    return 0;
}
