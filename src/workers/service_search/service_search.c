#include "sis_modules.h"
#include "worker.h"

#include "service_search.h"
///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////

struct s_sis_method service_search_methods[] = {
  {"get",    service_search_get, 0, NULL},
  {"set",    service_search_set, 0, NULL},
  {"sub",    service_search_sub, 0, NULL},
  {"pub",    service_search_pub, 0, NULL},
  {"msub",   service_search_msub, 0, NULL},
  {"unsub",  service_search_unsub, 0, NULL},
};
// 共享内存数据库 不落盘
s_sis_modules sis_modules_service_search = {
  service_search_init,
  NULL,
  service_search_working,
  NULL,
  service_search_uninit,
  NULL,
  NULL,
  sizeof(service_search_methods)/sizeof(s_sis_method),
  service_search_methods,
};

s_service_search_reader *service_search_reader_create(s_sis_net_message *netmsg_)
{
    s_service_search_reader *o = SIS_MALLOC(s_service_search_reader, o);
    return o;
}
void service_search_reader_destroy(void *reader_)
{
    s_service_search_reader *reader = (s_service_search_reader *)reader_;
    sis_free(reader);
}

bool  service_search_init(void *worker_, void *node_)
{
    return true;
}
void  service_search_uninit(void *worker_)
{

}
void  service_search_working(void *worker_)
{

}

int service_search_get(void *worker_, void *argv_)
{
  // s_sis_worker *worker = (s_sis_worker *)worker_;
  printf("%s : %s\n", "method", __func__);
  return 0;
}
int service_search_set(void *worker_, void *argv_)
{
  printf("%s : %s\n", "method", __func__);
  return 0;
}
int service_search_sub(void *worker_, void *argv_)
{
  // s_sis_worker *worker = (s_sis_worker *)worker_;
  printf("%s : %s\n", "method", __func__);
  return 0;
}

int service_search_pub(void *worker_, void *argv_)
{
  printf("%s : %s\n", "method", __func__);
  return 0;
}
int service_search_msub(void *worker_, void *argv_)
{
  // s_sis_worker *worker = (s_sis_worker *)worker_;
  printf("%s : %s\n", "method", __func__);
  return 0;
}
int service_search_unsub(void *worker_, void *argv_)
{
  printf("%s : %s\n", "method", __func__);
  return 0;
}