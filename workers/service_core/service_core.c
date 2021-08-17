#include "sis_modules.h"
#include "worker.h"

#include "service_core.h"
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method service_core_methods[] = {
  {"get",    service_core_get, 0, NULL},
  {"set",    service_core_set, 0, NULL},
  {"sub",    service_core_sub, 0, NULL},
  {"pub",    service_core_pub, 0, NULL},
  {"msub",   service_core_msub, 0, NULL},
  {"unsub",  service_core_unsub, 0, NULL},
};
// 共享内存数据库 不落盘
s_sis_modules sis_modules_service_core = {
  service_core_init,
  NULL,
  service_core_working,
  NULL,
  service_core_uninit,
  NULL,
  NULL,
  sizeof(service_core_methods)/sizeof(s_sis_method),
  service_core_methods,
};

s_service_core_reader *service_core_reader_create(s_sis_net_message *netmsg_)
{
    s_service_core_reader *o = SIS_MALLOC(s_service_core_reader, o);
    return o;
}
void service_core_reader_destroy(void *reader_)
{
    s_service_core_reader *reader = (s_service_core_reader *)reader_;
    sis_free(reader);
}

bool  service_core_init(void *worker_, void *node_)
{
    return true;
}
void  service_core_uninit(void *worker_)
{

}
void  service_core_working(void *worker_)
{

}

int service_core_get(void *worker_, void *argv_)
{
  // s_sis_worker *worker = (s_sis_worker *)worker_;
  printf("%s : %s\n", "method", __func__);
  return 0;
}
int service_core_set(void *worker_, void *argv_)
{
  printf("%s : %s\n", "method", __func__);
  return 0;
}
int service_core_sub(void *worker_, void *argv_)
{
  // s_sis_worker *worker = (s_sis_worker *)worker_;
  printf("%s : %s\n", "method", __func__);
  return 0;
}

int service_core_pub(void *worker_, void *argv_)
{
  printf("%s : %s\n", "method", __func__);
  return 0;
}
int service_core_msub(void *worker_, void *argv_)
{
  // s_sis_worker *worker = (s_sis_worker *)worker_;
  printf("%s : %s\n", "method", __func__);
  return 0;
}
int service_core_unsub(void *worker_, void *argv_)
{
  printf("%s : %s\n", "method", __func__);
  return 0;
}