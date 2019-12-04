
#include <sis_message.h>

/////////////////////////////////
//  message
/////////////////////////////////

// s_sis_worker *sis_worker_create(s_sis_json_node *node_)
// {
//     s_sis_worker *worker = (s_sis_worker *)sis_malloc(sizeof(s_sis_worker));
//     memset(worker, 0, sizeof(s_sis_worker));

//     worker->father = sis_get_server();

//     worker->status = SIS_SERVER_STATUS_NOINIT;

//     const char *classname = sis_json_get_str(node_, "classname");
//     sis_strcpy(worker->workname, SIS_NAME_LEN, node_->key); // 必须唯一
//     sis_strcpy(worker->classname, SIS_NAME_LEN, classname); // 多个工作者可以调用同一个类

//     worker->slots = sis_get_worker_slot(worker->classname);
//     if (!worker->slots)
//     {
//         LOG(3)("no find class : %s\n", classname);
//         sis_free(worker);
//         return NULL;
//     }
//     worker->task = sis_plan_task_create();
//     if (worker->slots->init && !worker->slots->init(worker, node_))
//     {
//         LOG(1)("load worker config error.\n");
//         sis_worker_destroy(worker);
//         return NULL;
//     }
//     printf("worker_create. %d\n", worker->task->work_mode);
//     if(worker->task->work_mode != SIS_WORK_MODE_NONE)
//     {
//         if (!sis_plan_task_start(worker->task, _thread_proc_worker, worker))
//         {
//             LOG(1)("can't start worker thread\n");
//             sis_worker_destroy(worker);
//             return NULL;
//         }			
//     }
//     LOG(5)("worker [%s] start.\n", worker->workname);
// 	return worker;
// }

// void sis_worker_destroy(s_sis_worker *worker_)
// {   
//     sis_plan_task_destroy(worker_->task);
//     LOG(5)("worker [%s] kill.\n", worker_->workname);
// 	if (worker_->slots->close)
// 	{
// 		worker_->slots->close(worker_);
// 	}	
// 	if (worker_->slots->uninit)
// 	{
// 		worker_->slots->uninit(worker_);
// 	}	
//     sis_free(worker_);
// }


