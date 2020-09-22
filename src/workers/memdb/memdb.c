#include "sis_modules.h"
#include "worker.h"

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

int memdb_min(void *worker_, void *argv_);
int memdb_max(void *worker_, void *argv_);

struct s_sis_method memdb_methods[] = {
  {"min",    memdb_min, 0, NULL},
  {"max",    memdb_max, 0, NULL},
};
// 共享内存数据库 不落盘
s_sis_modules sis_modules_memdb = {
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  sizeof(memdb_methods)/sizeof(s_sis_method),
  memdb_methods,
};

int memdb_min(void *worker_, void *argv_)
{
  // s_sis_worker *worker = (s_sis_worker *)worker_;
  printf("%s : %s\n", "method", __func__);
  return 0;
}
int memdb_max(void *worker_, void *argv_)
{
  printf("%s : %s\n", "method", __func__);
  return 0;
}