#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"

#include "sisdb_wlog.h"

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sisdb_wlog_methods[] = {
    {"read",  cmd_sisdb_wlog_read, NULL, NULL},   
    {"write", cmd_sisdb_wlog_write, NULL, NULL},  
    {"clear", cmd_sisdb_wlog_clear, NULL, NULL},  
};
// 共享内存数据库
s_sis_modules sis_modules_sisdb_wlog = {
    sisdb_wlog_init,
    sisdb_wlog_work_init,
    sisdb_wlog_working,
    sisdb_wlog_work_uninit,
    sisdb_wlog_uninit,
    sisdb_wlog_method_init,
    sisdb_wlog_method_uninit,
    sizeof(sisdb_wlog_methods) / sizeof(s_sis_method),
    sisdb_wlog_methods,
};

bool sisdb_wlog_init(void *worker_, void *argv_)
{
    return false;
}
void sisdb_wlog_work_init(void *worker_)
{
}
void sisdb_wlog_working(void *worker_)
{
}
void sisdb_wlog_work_uninit(void *worker_)
{
}
void sisdb_wlog_uninit(void *worker_)
{
}
void sisdb_wlog_method_init(void *worker_)
{
}
void sisdb_wlog_method_uninit(void *worker_)
{
}
int cmd_sisdb_wlog_read(void *worker_, void *argv_)
{
    printf("%s\n", __func__);
    return 0;
}

int cmd_sisdb_wlog_write(void *worker_, void *argv_)
{
    printf("%s\n", __func__);
    return 0;
}

int cmd_sisdb_wlog_clear(void *worker_, void *argv_)
{
    printf("%s\n", __func__);
    return 0;
}
