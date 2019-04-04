#include "sisws.h"
#include "sis_ws.h"
#include "sis_thread.h"
#include "sisdb_io.h"

bool worker_ws_open(s_sisdb_worker *worker_, s_sis_json_node *node_)
{
	sisdb_worker_init(worker_, node_);

	worker_->other = sis_malloc(sizeof(s_sisdb_ws_config));

	s_sisdb_ws_config *self = (s_sisdb_ws_config *)worker_->other;
	memset(self, 0, sizeof(s_sisdb_ws_config));

  self->port = sis_json_get_int(node_, "port", 7329);
	return true;
}
void worker_ws_close(s_sisdb_worker *worker_)
{
	sis_free(worker_->other);
}

void worker_ws_working(s_sisdb_worker *worker_)
{
  s_sisdb_ws_config *self = (s_sisdb_ws_config *)worker_->other;
  if (sis_ws_server_start(self->port)) 
  {
    LOG(1)("ws service no start.[%d]",self->port);
  }
}

