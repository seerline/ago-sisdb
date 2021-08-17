#include "sis_modules.h"
#include "worker.h"

#include "memdb.h"
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method memdb_methods[] = {
  {"get",    memdb_get,   0, NULL},
  {"set",    memdb_set,   0, NULL},
  {"sub",    memdb_sub,   0, NULL},
  {"pub",    memdb_pub,   0, NULL},
  {"msub",   memdb_msub,  0, NULL},  // 默认头匹配多键值查询
  {"unsub",  memdb_unsub, 0, NULL},
};
// 共享内存数据库 不落盘
s_sis_modules sis_modules_memdb = {
  memdb_init,
  NULL,
  memdb_working,
  NULL,
  memdb_uninit,
  NULL,
  NULL,
  sizeof(memdb_methods)/sizeof(s_sis_method),
  memdb_methods,
};



bool  memdb_init(void *worker_, void *node_)
{
    return true;
}
void  memdb_uninit(void *worker_)
{

}
void  memdb_working(void *worker_)
{

}

int memdb_get(void *worker_, void *argv_)
{
  // s_sis_worker *worker = (s_sis_worker *)worker_;
  printf("%s : %s\n", "method", __func__);
  return 0;
}
int memdb_set(void *worker_, void *argv_)
{
  printf("%s : %s\n", "method", __func__);
  return 0;
}
int memdb_sub(void *worker_, void *argv_)
{
  // s_sis_worker *worker = (s_sis_worker *)worker_;
  printf("%s : %s\n", "method", __func__);
  return 0;
}

int memdb_pub(void *worker_, void *argv_)
{
  printf("%s : %s\n", "method", __func__);
  return 0;
}
int memdb_msub(void *worker_, void *argv_)
{
  // s_sis_worker *worker = (s_sis_worker *)worker_;
  printf("%s : %s\n", "method", __func__);
  return 0;
}
int memdb_unsub(void *worker_, void *argv_)
{
  printf("%s : %s\n", "method", __func__);
  return 0;
}