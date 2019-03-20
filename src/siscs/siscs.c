#include "siscs.h"
#include "sis_ws.h"
#include "sis_thread.h"

void *_thread_proc_service(void *argv_)
{
	s_siscs_service *service = (s_siscs_service *)argv_;
	if (!service)
	{
		return NULL;
	}
	// sis_thread_wait_create(&worker->wait);
  if (sis_ws_server_start(service->port)) 
  {
    return NULL;
  }


	// sis_thread_wait_stop(&worker->wait);
	LOG(5)("[%s:%d] service end. \n", service->protocol, service->port);

	return NULL;
}

int siscs_service_start(s_siscs_service *service_)
{
  if (!sis_thread_create(_thread_proc_service, service_, &service_->thread_id))
  {
    LOG(1)("can't start service.[%s:%d]\n", service_->protocol, service_->port);
    siscs_service_stop(service_);
    return 1;
  }
  return 0;
}
void siscs_service_stop(s_siscs_service *service_)
{

}

