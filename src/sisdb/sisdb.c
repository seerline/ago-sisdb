
#include "sisdb.h"
#include "sisdb_map.h"
#include "sisdb_fields.h"
#include "sisdb_table.h"
#include "sisdb_collect.h"
#include "sisdb_call.h"
#include "sisdb_method.h"
#include "sisdb_io.h"
#include "sisdb_sys.h"
#include "sisws.h"

s_sis_db *sisdb_create(const char *name_) //数据库的名称，为空建立一个sys的数据库名
{
	s_sis_db *db = sis_malloc(sizeof(s_sis_db));
	memset(db, 0, sizeof(s_sis_db));
	db->name = sis_sdsnew(name_);

	db->dbs = sis_map_pointer_create();

	sis_mutex_init(&db->write_mutex, NULL);

	db->collects = sis_map_pointer_create();

	return db;
}

void sisdb_destroy(s_sis_db *db_) //关闭一个数据库
{
	if (!db_)
	{
		return;
	}
	sis_mutex_destroy(&db_->write_mutex);
	// // if (db_->special)
	// {
	// 	// 普通数据库没有init线程
	// 	sis_plan_task_destroy(db_->init_task);
	// }
	// sis_plan_task_destroy(db_->save_task);
	// 遍历字典中table，手动释放实际的table
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(db_->dbs);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sisdb_table *val = (s_sisdb_table *)sis_dict_getval(de);
		sisdb_table_destroy(val);
	}
	sis_dict_iter_free(di);

	// 如果没有sis_map_pointer_destroy就必须加下面这句
	// sis_map_buffer_clear(db_->db);
	// 下面仅仅释放key
	sis_map_pointer_destroy(db_->dbs);

	// 遍历字典中collects，手动释放实际的 collects
	if (db_->collects)
	{
		s_sis_dict_entry *de;
		s_sis_dict_iter *di = sis_dict_get_iter(db_->collects);
		while ((de = sis_dict_next(di)) != NULL)
		{
			s_sisdb_collect *val = (s_sisdb_collect *)sis_dict_getval(de);
			sisdb_collect_destroy(val);
		}
		sis_dict_iter_free(di);
	}
	// 下面仅仅释放key
	sis_map_pointer_destroy(db_->collects);

	sis_sdsfree(db_->name);
	sis_json_delete_node(db_->conf);

	sis_free(db_);
}

///////////////////////////////////////
//
///////////////////////////////////////
// 处理工作时间等公共的配置
void sisdb_worker_init(s_sisdb_worker *worker_, s_sis_json_node *node_)
{
	worker_->task->work_mode = SIS_WORK_MODE_ONCE;
	s_sis_json_node *wtime = sis_json_cmp_child_node(node_, "work-time");
	if (wtime)
	{
		s_sis_json_node *ptime = sis_json_cmp_child_node(wtime, "plans-work");
		if (ptime)
		{
			worker_->task->work_mode = SIS_WORK_MODE_PLANS;
			int count = sis_json_get_size(ptime);
			for (int k = 0; k < count; k++)
			{
				uint16 min = sis_array_get_int(ptime, k, 0);
				sis_struct_list_push(worker_->task->work_plans, &min);
			}
		}
		s_sis_json_node *atime = sis_json_cmp_child_node(wtime, "always-work");
		if (atime)
		{
			worker_->task->work_mode = SIS_WORK_MODE_GAPS;
			worker_->task->work_gap.start = sis_json_get_int(atime, "start", 900);
			worker_->task->work_gap.stop = sis_json_get_int(atime, "stop", 1530);
			worker_->task->work_gap.delay = sis_json_get_int(atime, "delay", 300);
		}
	}
}

void *_thread_proc_worker(void *argv_)
{
	s_sisdb_worker *worker = (s_sisdb_worker *)argv_;
	if (!worker)
	{
		return NULL;
	}
	s_sis_plan_task *task = worker->task;
	sis_plan_task_wait_start(task);
	while (sis_plan_task_working(task))
	{
		if (sis_plan_task_execute(task))
		{
			// printf("work = %p mode = %d\n", worker, task->work_mode);
			LOG(5)
			("work = %p mode = %d\n", worker, task->work_mode);
			// sis_mutex_lock(&task->mutex);
			// --------user option--------- //
			if (worker->working)
			{
				worker->working(worker);
			}
			// printf("work end.%d\n", sis_plan_task_execute(task));
			// sis_mutex_unlock(&task->mutex);
		}
	}
	sis_plan_task_wait_stop(task);
	return NULL;
}

s_sisdb_worker *sisdb_worker_create(s_sis_json_node *node_)
{
	s_sisdb_worker *worker = (s_sisdb_worker *)sis_malloc(sizeof(s_sisdb_worker));
	memset(worker, 0, sizeof(s_sisdb_worker));

	worker->status = SIS_SERVER_STATUS_NOINIT;

	sis_strcpy(worker->classname, SIS_NAME_LEN, node_->key);

	if (!sis_strcasecmp(worker->classname, "save-class"))
	{
		worker->open = worker_save_open;
		worker->close = worker_save_close;
		worker->working = worker_save_working;
	}
	else if (!sis_strcasecmp(worker->classname, "init-class"))
	{
		worker->open = worker_init_open;
		worker->close = worker_init_close;
		worker->working = worker_init_working;
	}
	else if (!sis_strcasecmp(worker->classname, "ws-class"))
	{
		worker->open = worker_ws_open;
		worker->close = worker_ws_close;
		worker->working = worker_ws_working;
	}
	else
	{
		LOG(3)
		("no find class : %s\n", worker->classname);
		sis_free(worker);
		return NULL;
	}
	////////////////////////////////////////
	worker->task = sis_plan_task_create();

	if (worker->open && !worker->open(worker, node_))
	{
		LOG(3)
		("load worker config error.\n");
		sisdb_worker_destroy(worker);
		return NULL;
	}
	if (worker->task->work_mode != SIS_WORK_MODE_NONE)
	{
		if (!sis_plan_task_start(worker->task, _thread_proc_worker, worker))
		{
			LOG(3)
			("can't start worker thread.\n");
			sisdb_worker_destroy(worker);
			return NULL;
		}
	}
	LOG(5)
	("worker [%s] start.\n", worker->classname);

	return worker;
}

void sisdb_worker_destroy(s_sisdb_worker *worker_)
{
	sis_plan_task_destroy(worker_->task);
	LOG(5)
	("worker [%s] kill.\n", worker_->classname);
	if (worker_->close)
	{
		worker_->close(worker_);
	}
	sis_free(worker_);
}

///////////////////////////
//  save worker define   //
///////////////////////////
bool worker_save_open(s_sisdb_worker *worker_, s_sis_json_node *node_)
{
	sisdb_worker_init(worker_, node_);

	worker_->other = sis_malloc(sizeof(s_sisdb_save_config));

	s_sisdb_save_config *self = (s_sisdb_save_config *)worker_->other;
	memset(self, 0, sizeof(s_sisdb_save_config));

	self->status = 0;
	return true;
}
void worker_save_close(s_sisdb_worker *worker_)
{
	sis_free(worker_->other);
}

void worker_save_working(s_sisdb_worker *worker_)
{
	// 这里放存盘的代码
	sisdb_save(NULL);
}
