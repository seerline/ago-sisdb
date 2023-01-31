
#include <worker.h>
#include <server.h>

/////////////////////////////////////////////////
//  worker thread
/////////////////////////////////////////////////
/**
 * @brief 根据任务类型及当前时间，判断当前任务是否允许执行
 * @param task_ 工作者线程对象
 * @return true 允许立即执行后续任务，false 不允许 
 */
bool sis_work_thread_wait(s_sis_work_thread *task_)
{
	if (task_->work_mode == SIS_WORK_MODE_ONCE)
	{
		if (!task_->isfirst)
		{
			task_->isfirst = true;
			return true;
		}	
        sis_wait_thread_close(task_->work_thread);        
	}
	else if (task_->work_mode == SIS_WORK_MODE_NOTICE)
	{
		int o = sis_wait_thread_wait(task_->work_thread, 30000);
		// printf("notice start... %d %d\n", SIS_ETIMEDOUT, o);
		if (o == SIS_WAIT_NOTICE)
		{
			// only notice return true.
			if (sis_wait_thread_noexit(task_->work_thread))
			{
				// printf("notice work...\n");
				return true;
			}
		}
	}
	else if (task_->work_mode == SIS_WORK_MODE_GAPS)
	{
		// printf("gap = [%d, %d , %d]\n", task_->work_gap.start ,task_->work_gap.stop, task_->work_gap.delay);
		int o = sis_wait_thread_wait(task_->work_thread, task_->work_gap.delay);
		// printf("notice start... %d %d\n", SIS_ETIMEDOUT, o);
		if (o != SIS_WAIT_NOTICE)
		{
			// printf("delay 1 = %d\n", task_->work_gap.delay);
			if (task_->work_gap.start == task_->work_gap.stop)
			{
				return true;
			}
			else
			{
				int min = sis_time_get_iminute(0);
				if ((task_->work_gap.start < task_->work_gap.stop && min > task_->work_gap.start && min < task_->work_gap.stop) ||
					(task_->work_gap.start > task_->work_gap.stop && (min > task_->work_gap.start || min < task_->work_gap.stop)))
				{
					return true;
				}
			}
			// printf("delay = %d\n", task_->work_gap.delay);
		}
	}
	else // SIS_WORK_MODE_PLANS
	{
		if (!task_->isfirst)
		{
			task_->isfirst = true;
			return true;
		}		
		else 
        {
            int o = sis_wait_thread_wait(task_->work_thread, 30000);
            // printf("notice start... %d %d\n", SIS_ETIMEDOUT, o);
            if (o != SIS_WAIT_NOTICE)
            {
                int min = sis_time_get_iminute(0);
                // printf("save plan ... -- -- -- %d \n", min);
                for (int k = 0; k < task_->work_plans->count; k++)
                {
                    uint16 *lm = sis_struct_list_get(task_->work_plans, k);
                    if (min == *lm)
                    {
                        return true;
                    }
                }
            }

        }
	}
    sis_sleep(300);
	return false;
}
s_sis_work_thread *sis_work_thread_create()
{
	s_sis_work_thread *o = sis_malloc(sizeof(s_sis_work_thread));
	memset(o, 0, sizeof(s_sis_work_thread));
	o->work_plans = sis_struct_list_create(sizeof(uint16));
	return o;
}

/**
 * @brief 创建线程
 * @param task_ 保存结果的s_sis_work_thread对象
 * @param func_ 线程将要执行的回调函数
 * @param worker_ 线程执行时传递给回调函数func_的参数
 * @return true 成功
 * @return false 出错
 */
bool sis_work_thread_open(s_sis_work_thread *task_, cb_thread_working func_, void *val_)
{
    // if (task_->work_mode == SIS_WORK_MODE_NONE)
    // {
    //     return false;
    // }
    // 默认30秒
    task_->work_thread = sis_wait_thread_create(30000);
	if (!sis_wait_thread_open(task_->work_thread, func_, val_))
	{
		return false;
	}
	return true;
}
void sis_work_thread_destroy(s_sis_work_thread *task_)
{
    if (task_->work_thread)
    {
        sis_wait_thread_destroy(task_->work_thread);
    }
	sis_struct_list_destroy(task_->work_plans);
	sis_free(task_);
	LOG(5)("work_thread end.\n");
}

/////////////////////////////////////////
// worker thread function
/////////////////////////////////////////

/**
 * @brief （1）调用工作者的module的work_init函数
 * （2）获取互斥锁,根据工作者的任务类别选择时机执行module的working函数
 * （3）等待任务完成，并回收资源
 * @param argv_ 工作者s_sis_worker
 * @return void* 永远NULL
 */
void *_service_work_thread(void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)argv_;
	if (!worker)
	{
		return NULL;
	}
    s_sis_work_thread *service_thread = worker->service_thread;
	if (worker->slots->work_init)
	{
		worker->slots->work_init(worker);
	}
    sis_wait_thread_start(service_thread->work_thread);
    while(!(worker->status & SIS_WORK_INIT_WORKER))
    {
        sis_sleep(10);
    }
    while (sis_wait_thread_noexit(service_thread->work_thread))
    {
        SIS_EXIT_SIGNAL;
        if (sis_work_thread_wait(service_thread))
        {
            if (sis_wait_thread_isexit(service_thread->work_thread))
            {
                break;
            }
            if (sis_wait_thread_iswork(service_thread->work_thread))
            {                
                // LOG(10)("work = %p mode = %d\n", worker, service_thread->work_mode);
                // --------user option--------- //
                if (worker->slots->working)
                {
                    worker->slots->working(worker);
                }
                // LOG(10)("work end. status = %d\n", worker->status);
            }
        }
    }	
	if (worker->slots->work_uninit)
	{
		worker->slots->work_uninit(worker);
	}	
    sis_wait_thread_stop(service_thread->work_thread);
	LOG(5)("work [%s] end.\n", worker->workername);
    return NULL;
}
// 获得完整的 workname
s_sis_sds _sis_worker_get_workname(s_sis_worker *worker_, const char *workname_)
{
    s_sis_sds workername = sis_sdsempty();
    if (worker_->father)
    {
        s_sis_worker *father = (s_sis_worker *)worker_->father;
        workername = sis_sdscatfmt(workername, "%S",  father->workername);
        workername = sis_sdscatfmt(workername, ".%s", workname_);
    }
    else
    {
        workername = sis_sdscat(workername, workname_);
    }
    return workername;
}
// s_sis_sds _sis_worker_get_workname(s_sis_worker *worker_, const char *workname_)
// {
//     s_sis_sds workername = sis_sdsempty();
//     if (worker_)
//     {
//         workername = sis_sdscatfmt(workername, "%S",  worker_->workername);
//         workername = sis_sdscatfmt(workername, ".%s", workname_);
//     }
//     else
//     {
//         workername = sis_sdscat(workername, workname_);
//     }
//     return workername;
// }

/**
 * @brief 根据json配置对工作者初始化，包括：
* (1) 根据工作者的classname获取对应的接插件module
* (2) 调用module的初始化方法init
* (3) 将module的methods添加到工作者的methods表中
* (4) 调用一遍module的method_init方法
 * @param worker_ 需要被初始化的工作者
 * @param node_ 工作者的json配置
 * @return true 成功
 * @return false 失败
 */
bool _sis_worker_init(s_sis_worker *worker_, s_sis_json_node *node_)
{
    worker_->slots = sis_get_worker_slot(worker_->classname);
    if (!worker_->slots)
    {
        LOG(3)("no find class : %s\n", worker_->classname);
        return false;
    }
    // init
    if (worker_->slots->init && !worker_->slots->init(worker_, node_))
    {
        LOG(1)("load worker [%s] config error.\n", worker_->classname);
        return false;
    }
    // init methods
    worker_->methods = sis_methods_create(NULL, 0);
	for (int i = 0; i < worker_->slots->mnums; i++)
	{
		s_sis_method *method = worker_->slots->methods + i;
		sis_methods_register(worker_->methods, method);
        // printf("---[%d]--- %s : %p %s\n", i, val->name, val->proc, val->style);
	} 
    if (sis_map_pointer_getsize(worker_->methods) > 0)
    {
        if (worker_->slots->method_init)
        {
            worker_->slots->method_init(worker_);
        }
    }
    return true;
}

// 处理工作时间等公共的配置
void _sis_load_work_time(s_sis_worker *worker_, s_sis_json_node *node_)
{
    worker_->service_thread->work_mode = SIS_WORK_MODE_NONE;  
    // 没有 work-time 就不启动线程
    s_sis_json_node *wtime = sis_json_cmp_child_node(node_, "work-time");
    if (wtime)
    {
        worker_->service_thread->work_mode = SIS_WORK_MODE_ONCE;
        if (wtime->type == SIS_JSON_STRING)
        {
            if (!sis_strcasecmp(wtime->value, "notice"))
            {
                worker_->service_thread->work_mode = SIS_WORK_MODE_NOTICE;
            }
        }
        else
        {
            s_sis_json_node *ptime = sis_json_cmp_child_node(wtime, "plans-work");
            if (ptime)
            {
                worker_->service_thread->work_mode = SIS_WORK_MODE_PLANS;
                int count = sis_json_get_size(ptime);
                for (int k = 0; k < count; k++)
                {
                    uint16 min = sis_array_get_int(ptime, k, 0);
                    sis_struct_list_push(worker_->service_thread->work_plans, &min);
                }
            }
            s_sis_json_node *atime = sis_json_cmp_child_node(wtime, "always-work");
            if (atime)
            {
                worker_->service_thread->work_mode = SIS_WORK_MODE_GAPS;
                worker_->service_thread->work_gap.start = sis_json_get_int(atime, "start", 900);
                worker_->service_thread->work_gap.stop = sis_json_get_int(atime, "stop", 1530);
                worker_->service_thread->work_gap.delay = sis_json_get_int(atime, "delay", 300);
            }            
        }
    }
}
/////////////////////////////////
//  worker
/////////////////////////////////
/**
 * @brief 根据work名称及其json配置文件，创建工作者，并根据json配置对工作者初始化，包括：
* (1) 根据工作者的classname获取对应的接插件module
* (2) 调用module的初始化方法init
* (3) 将module的methods添加到工作者的methods表中
* (4) 调用一遍module的method_init方法
* (5) 启动独立线程运行module的working函数
 * @param father_ 父工作者
 * @param name_ 父工作者
 * @param node_ 工作者的json配置，如果为NULL，则自动创建空白配置
 * @return s_sis_worker* 生成的工作者对象指针
 */
s_sis_worker *sis_worker_create_of_name(s_sis_worker *father_, const char *name_, s_sis_json_node *node_)
{
    if (!name_ || sis_str_substr_nums(name_, sis_strlen(name_), '.') > 1)
    {
        LOG(3)("workname [%s] cannot '.' !\n", name_ ? name_ : "");  
        return NULL;
    }
    s_sis_json_node *node = NULL;
    if (!node_)
    {
        node = sis_json_create_object();
        sis_json_object_add_string(node, "classname", name_, sis_strlen(name_));
    }
    else
    {
        node = node_;
    }
    // size_t iii = 1;
    // printf("%s\n", sis_json_output(node, &iii));
    s_sis_worker *worker = SIS_MALLOC(s_sis_worker, worker);
    worker->father = father_; 
    worker->status = SIS_WORK_INIT_NONE;
    s_sis_json_node *classname_node = sis_json_cmp_child_node(node, "classname");
    if (classname_node)
    {
        worker->classname = sis_sdsnew(sis_json_get_str(node, "classname"));
    }
    else
    {
        worker->classname = sis_sdsnew(name_);
    }
    // 需要遍历前导字符
    worker->workername = _sis_worker_get_workname(worker, name_);

    // 必须在 _sis_worker_init 之前初始化workers
    worker->workers = sis_map_pointer_create();

    // 调用公共初始化配置
    if (!node->key)
    {
        node->key = sis_strdup(name_, sis_strlen(name_));
    }
    if (!_sis_worker_init(worker, node))
    {
        LOG(3)("init worker [%s] error.\n", worker->classname);
        sis_worker_destroy(worker);
        worker = NULL;
        goto work_exit;
    }
    worker->status |= SIS_WORK_INIT_METHOD;
    // 如果有 working 表示有独立线程需要启动
    if (worker->slots->working) 
    {
        worker->service_thread = sis_work_thread_create();
        // 设置工作模式
        _sis_load_work_time(worker, node);

        if (worker->service_thread->work_mode != SIS_WORK_MODE_NONE)
        {
            LOG(5)("worker service_thread create. %d\n", worker->service_thread->work_mode);
            if (!sis_work_thread_open(worker->service_thread, _service_work_thread, worker))
            {
                LOG(3)("can't start service_thread.\n");
                sis_worker_destroy(worker);
                worker = NULL;
                goto work_exit;
            }
        }
    }
    // worker->status |= SIS_WORK_INIT_WORKER;
    // 这里已经初始化完成
    // 只有父节点不为空 才会写入workers 用于检索
    if (worker->father)
    {
        s_sis_worker *father = (s_sis_worker *)worker->father;
        if (!sis_map_pointer_find(father->workers, worker->workername))
        {
            sis_map_pointer_set(father->workers, worker->workername, worker);
        }
        else
        {
            // 一样的就跳过
            LOG(5)("workname = %s already exists.\n", worker->workername);
        }       
    }
    else
    {
        // 不能够被检索的工作线程，由用户自行管理
    }
    LOG(5)("worker start. [%s] workname = %s status: %d sub-workers = %d \n", 
        worker->classname, worker->workername, 
        worker->status, (int)sis_map_pointer_getsize(worker->workers));
    worker->status |= SIS_WORK_INIT_WORKER;
work_exit:
    if (!node_)
    {
        sis_json_delete_node(node);
    }
	return worker;
}

s_sis_worker *sis_worker_create_of_conf(s_sis_worker *father_, const char *name_, const char *config_)
{
    s_sis_worker *worker = NULL;
    s_sis_conf_handle *handle = sis_conf_load(config_, sis_strlen(config_));
    if (handle)
    {
        worker = sis_worker_create_of_name(father_, name_, handle->node);
        sis_conf_close(handle);
    }
    else
    {
        worker = sis_worker_create_of_name(father_, name_, NULL);
    }
    return worker;
}
/**
 * @brief 根据JSON配置文件创建工作者，初始化，包括：
* (1) 根据工作者的classname获取对应的接插件module
* (2) 调用module的初始化方法init
* (3) 将module的methods添加到工作者的methods表中
* (4) 调用一遍module的method_init方法
* (5) 启动独立线程运行module的working函数
 * @param father_ 父工作者
 * @param node_ 工作者的json配置，如果为NULL，则直接返回NULL
 * @return s_sis_worker* 生成的工作者对象指针，配置为空时返回NULL
 */
s_sis_worker *sis_worker_create(s_sis_worker *father_, s_sis_json_node *node_)
{
    if (!node_)
    {
        return NULL;
    }
    return sis_worker_create_of_name(father_, node_->key, node_);
}

void sis_worker_destroy(void *worker_)
{   
    s_sis_worker *worker =(s_sis_worker *)worker_;

    LOG(5)("worker kill. [%s] %s \n", worker->classname, worker->workername);
    // 应该先释放 notice 再释放其他的
    worker->status = SIS_WORK_INIT_NONE;
    if (worker->service_thread)
    {
        sis_work_thread_destroy(worker->service_thread);
    }
    if (worker->workers)
    {
        sis_map_pointer_destroy(worker->workers);
    }
    // 禁止所有服务
    if (worker->methods)
    {
        if (sis_map_pointer_getsize(worker->methods) > 0)
        {
            if (worker->slots && worker->slots->method_uninit)
            {
                worker->slots->method_uninit(worker);
            }
        }
        sis_methods_destroy(worker->methods);
    }
    if (worker->slots && worker->slots->uninit)
    {
        worker->slots->uninit(worker);
    }
    sis_sdsfree(worker->workername);
    sis_sdsfree(worker->classname);
    sis_free(worker);
}

// 在 worker_ 下根据 node 配置生成多个子 worker, worker_ 为空表示依赖于 server
int sis_worker_init_multiple(s_sis_worker *worker_, s_sis_json_node *node_)
{
	s_sis_json_node *next = sis_json_first_node(node_);
	while (next)
	{
        s_sis_sds wname = _sis_worker_get_workname(worker_, next->key);
		if (!sis_map_pointer_find(worker_->workers, wname))
		{
			s_sis_worker *worker = sis_worker_create(worker_, next);
			if (worker)
			{
				sis_map_pointer_set(worker_->workers, (const char *)wname, worker);
			}
		}
        sis_sdsfree(wname);
		next = next->next;
	}
	return sis_map_pointer_getsize(worker_->workers);
}

/////////////////////////////////
//  worker tools
/////////////////////////////////

int sis_worker_command(s_sis_worker *worker_, const char *cmd_, void *msg_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_;
    if (!worker)
    {
        return SIS_METHOD_NOWORK;
    }
    // s_sis_dict_entry *de;
    // s_sis_dict_iter *di = sis_dict_get_iter(worker_->methods);
    // while ((de = sis_dict_next(di)) != NULL)
    // {
    //     s_sis_method *val = (s_sis_method *)sis_dict_getval(de);
    //     printf("---- %s : %p %s\n", val->name, val->proc, val->style);
    // }
    // sis_dict_iter_free(di);
    // 必须等方法启动完毕
    SIS_WAIT_LONG(worker->status & SIS_WORK_INIT_METHOD);

    s_sis_method *method = sis_methods_get(worker->methods, cmd_);
    if (method&&method->proc)
    {
        // printf("---- %s : %p %s\n", method->name, method->proc, method->style);
        return method->proc(worker_, msg_);
    }
    return SIS_METHOD_NOCMD;
}

s_sis_method *sis_worker_get_method(s_sis_worker *worker_, const char *cmd_)
{
    s_sis_method *method = sis_methods_get(worker_->methods, cmd_);
    if (method&&method->proc)
    {
        // printf("---- %s : %p %s\n", method->name, method->proc, method->style);
        return method;
    }
    return NULL;   
}

// 通知worker去执行 working 
void sis_work_thread_notice(s_sis_worker *worker_)
{
    // printf("====== %p\n", worker_->service_thread);
    // if (worker_ && worker_->service_thread && worker_->service_thread->work_mode == SIS_WORK_MODE_NOTICE)
    if (worker_ && worker_->service_thread && worker_->service_thread->work_thread)
    {
        sis_wait_thread_notice(worker_->service_thread->work_thread);
    }
}



// 仅仅找当前 worker->workers 下的 workname_ = aaa
s_sis_worker *sis_worker_get(s_sis_worker *worker_, const char *workname_)
{
    if (!worker_) // server 的 worker
    {
        s_sis_server *server = sis_get_server();
        return sis_map_pointer_get(server->workers, workname_);
    }
    else
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(worker_->workers);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sis_worker *work = (s_sis_worker *)sis_dict_getval(de);
            printf("get ---- %s : [%s] %s\n", work->classname, (char *)sis_dict_getkey(de), work->workername);
        }
        sis_dict_iter_free(di);

        s_sis_sds wname = sis_sdsdup(worker_->workername);
        wname = sis_sdscatfmt(wname, ".%s", workname_);
        printf("get worker : %s %s %s %s\n", worker_->classname, worker_->workername, wname, workname_);
        s_sis_worker *worker = sis_map_pointer_find(worker_->workers, wname);
        sis_sdsfree(wname);
        return worker;
    }
}

// 从根部的 worker->workers 找起 workname_ = aaa.bbb.ccc
s_sis_worker *sis_worker_search(const char *workname_)
{
    char firstname[128];
    sis_str_substr(firstname, 128, workname_, '.', 0);
    s_sis_worker *worker = sis_worker_get(NULL, firstname);
    if (!worker)
    {
        return NULL;
    }
    int count = sis_str_substr_nums(workname_, sis_strlen(workname_), '.') - 1;
    int index = 1;
    while(count > 0)
    {
        sis_str_substr(firstname, 128, workname_, '.', index);
        worker = sis_worker_get(worker, firstname);
        if (!worker)
        {
            return NULL;
        }
        count--;
    }
    return worker;
}


#if 0
#include <signal.h>
#include <stdio.h>

s_sis_wait __thread_wait__; 
s_sis_wait_handle __wait;
int __count = 0;

int sis_thread_wait_sleep1(s_sis_wait *wait_, int delay_) // 秒
{
	struct timeval tv;
	struct timespec ts;
	sis_time_get_day(&tv, NULL);
	ts.tv_sec = tv.tv_sec + delay_;
	ts.tv_nsec = tv.tv_usec * 1000;
	return pthread_cond_timedwait(&wait_->cond, &wait_->mutex, &ts);
	// 返回 SIS_ETIMEDOUT 就正常处理
}

void *_plan_task_example(void *argv_)
{
	s_sis_work_thread *task = (s_sis_work_thread *)argv_;

	// sis_thread_wait_create(&task->wait);
	// sis_thread_wait_start(&task->wait);
	sis_thread_wait_create(&__thread_wait__);
	sis_thread_wait_start(&__thread_wait__);
	// sis_out_binary("alone: ",(const char *)&__thread_wait__,sizeof(s_sis_wait));

	// sis_thread_wait_create(&test_wait->wait);
	// sis_thread_wait_start(&test_wait->wait);
	// sis_out_binary("new: ",&test_wait->wait,sizeof(s_sis_wait));

	s_sis_wait *wait = sis_wait_get(__wait);
	sis_mutex_lock(&wait->mutex);

	while (task->working)
	{
		printf(" test ..1.. %p \n", task);
		sis_sleep(1000);
		// if(sis_thread_wait_sleep(&task->wait, 3) == SIS_ETIMEDOUT)
		// if(sis_thread_wait_sleep(&task->wait, 3) == SIS_ETIMEDOUT)
		// if(sis_thread_wait_sleep_msec(&task->wait, 1000) == SIS_ETIMEDOUT)
		// if(sis_thread_wait_sleep_msec(&__thread_wait__, 1300) == SIS_ETIMEDOUT)
		if(sis_thread_wait_sleep1(wait, 1) == SIS_ETIMEDOUT)
		{
			__count++;
			printf(" test ... [%d]\n", __count);
		}
		else
		{
			printf(" no timeout ... \n");
		}
		
		// if (sis_work_thread_working(task))
		{
			// printf(" run ... \n");
		}
	}
	printf("end . \n");
	// sis_work_thread_wait_stop(task);
	// sis_thread_wait_stop(&task->wait);
	// sis_thread_wait_stop(&__thread_wait__);
	// sis_thread_wait_stop(&test_wait->wait);
	sis_mutex_unlock(&wait->mutex);
	return NULL;
}

int __exit = 0;
s_sis_work_thread *__plan_task;

void exithandle(int sig)
{
	printf("exit .1. \n");
	sis_work_thread_destroy(__plan_task);
	printf("exit .ok . \n");
	__exit = 1;
}

int main()
{
	__wait = sis_wait_malloc();

	__plan_task = sis_work_thread_create();
	__plan_task->work_mode = SIS_WORK_MODE_GAPS;
	__plan_task->work_gap.start = 0;
	__plan_task->work_gap.stop = 2359;
	__plan_task->work_gap.delay = 5;

	printf(" test ..0.. %p wait = %lld\n", __plan_task, sizeof(__plan_task->wait));
	sis_work_thread_open(__plan_task, _plan_task_example, __plan_task);

	signal(SIGINT, exithandle);

	printf("working ... \n");
	while (!__exit)
	{
		sis_sleep(300);
	}
}
#endif
