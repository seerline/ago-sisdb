#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"

#include "sisdb.h"

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method sisdb_methods[] = {
  {"get",    cmd_sisdb_get,   NULL, NULL},  // json 格式
  {"set",    cmd_sisdb_set,   NULL, NULL},  // json 格式
  {"bget",   cmd_sisdb_bget,  NULL, NULL},  // 二进制格式
  {"bset",   cmd_sisdb_bset,  NULL, NULL},  // 二进制格式
  {"del",    cmd_sisdb_del,   NULL, NULL},  // 删除数据
};
// 共享内存数据库
s_sis_modules sis_modules_sisdb = {
	sisdb_init,
  sisdb_work_init,
  sisdb_working,
  sisdb_work_uninit,
  sisdb_uninit,
	sisdb_method_init,
	sisdb_method_uninit,
  sizeof(sisdb_methods)/sizeof(s_sis_method),
  sisdb_methods,
};

bool  sisdb_init(void *worker_, void *argv_)
{
  return false;
}
void  sisdb_work_init(void *worker_)
{
}
void  sisdb_working(void *worker_)
{
}
void  sisdb_work_uninit(void *worker_)
{
}
void  sisdb_uninit(void *worker_)
{
}
void  sisdb_method_init(void *worker_)
{
}
void  sisdb_method_uninit(void *worker_)
{
}

int cmd_sisdb_get(void *worker_, void *argv_)
{
  return 0;
}
int cmd_sisdb_set(void *worker_, void *argv_)
{
  return 0;
}
int cmd_sisdb_bget(void *worker_, void *argv_)
{
  return 0;
}
int cmd_sisdb_bset(void *worker_, void *argv_)
{
  return 0;
}
int cmd_sisdb_del(void *worker_, void *argv_)
{
  return 0;
}
