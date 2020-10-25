
#include <worker.h>
#include <server.h>

/////////////////////////////////////////
// worker thread function
/////////////////////////////////////////

void *_service_work_thread(void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)argv_;
	if (!worker)
	{
		return NULL;
	}
    s_sis_service_thread *service_thread = worker->service_thread;
	if (worker->slots->work_init)
	{
		worker->slots->work_init(worker);
	}
    sis_service_thread_wait_start(service_thread);  
    worker->status = SIS_SERVER_STATUS_INITED;
    while (sis_service_thread_working(service_thread))
    {
        // printf(".....%d \n", worker->status);
        if (sis_service_thread_execute(service_thread) && worker->status != SIS_SERVER_STATUS_CLOSE)
        {
			// printf("work = %p mode = %d\n", worker, service_thread->work_mode);
			LOG(10)("work = %p mode = %d\n", worker, service_thread->work_mode);
            // --------user option--------- //
			if (worker->slots->working)
			{
				worker->slots->working(worker);
			}
			LOG(10)("work end. status = %d\n", worker->status);
        }
    }	
    sis_service_thread_wait_stop(service_thread);
    // 这里总执行不到，需想其他办法
	// if (worker->slots->work_uninit)
	// {
	// 	worker->slots->work_uninit(worker);
	// }	
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
bool _sis_service_init(s_sis_worker *worker_, s_sis_json_node *node_)
{
    worker_->service_thread->work_mode = SIS_WORK_MODE_NONE;  // 没有 work-time 就不启动线程
    s_sis_json_node *wtime = sis_json_cmp_child_node(node_, "work-time");
    if (wtime)
    {
        worker_->service_thread->work_mode = SIS_WORK_MODE_ONCE;
        if (wtime->type == SIS_JSON_STRING)
        {
            if (!sis_strcasecmp(wtime->value, "notice"))
            {
                worker_->service_thread->work_mode = SIS_WORK_MODE_NOTICE;
                return true;            
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
                return true;
            }
            s_sis_json_node *atime = sis_json_cmp_child_node(wtime, "always-work");
            if (atime)
            {
                worker_->service_thread->work_mode = SIS_WORK_MODE_GAPS;
                worker_->service_thread->work_gap.start = sis_json_get_int(atime, "start", 900);
                worker_->service_thread->work_gap.stop = sis_json_get_int(atime, "stop", 1530);
                worker_->service_thread->work_gap.delay = sis_json_get_int(atime, "delay", 300);
                return true;
            }            
        }

    }
    return true;
}
/////////////////////////////////
//  worker
/////////////////////////////////

s_sis_worker *sis_worker_create(s_sis_worker *father_, s_sis_json_node *node_)
{
    s_sis_worker *worker = SIS_MALLOC(s_sis_worker, worker);

    worker->father = father_; 

    worker->status = SIS_SERVER_STATUS_NOINIT;

    s_sis_json_node *classname_node = sis_json_cmp_child_node(node_, "classname");
    if (classname_node)
    {
        worker->classname = sis_sdsnew(sis_json_get_str(node_, "classname"));
    }
    else
    {
        worker->classname = sis_sdsnew(node_->key);
    }
    if (sis_str_substr_nums(node_->key, '.') > 1)
    {
        LOG(3)("workname [%s] cannot '.' !\n", node_->key);  
        sis_worker_destroy(worker);
        return NULL;
    }
    // 需要遍历前导字符
    worker->workername = _sis_worker_get_workname(worker, node_->key);

    // 必须在 _sis_worker_init 之前初始化workers
    worker->workers = sis_map_pointer_create();

    // 调用公共初始化配置
    if (!_sis_worker_init(worker, node_))
    {
        LOG(3)("init worker [%s] error.\n", worker->classname);
        sis_worker_destroy(worker);
        return NULL;
    }
    worker->status |= SIS_SERVER_STATUS_MAIN_INIT;
    // 如果有 working 表示有独立线程需要启动
    if (worker->slots->working) 
    {
        worker->service_thread = sis_service_thread_create();

        if (!_sis_service_init(worker, node_))
        {
            LOG(3)("init service_thread [%s] error.\n", worker->classname);
            sis_worker_destroy(worker);
            return NULL;
        }
        LOG(5)("worker service_thread create. %d\n", worker->service_thread->work_mode);
        if (!sis_service_thread_start(worker->service_thread, _service_work_thread, worker))
        {
            LOG(3)("can't start service_thread\n");
            sis_worker_destroy(worker);
            return NULL;
        }
    }
    worker->status |= SIS_SERVER_STATUS_WORK_INIT;
    // 这里已经初始化完成
    // worker->status = SIS_SERVER_STATUS_INITED;
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
            // sis_get_server()->work_series++;
            // worker->workername = sis_sdscatfmt(worker->workername, "%i", sis_get_server()->work_series);
            // LOG(5)("new workname = %s .\n", worker->workername);
        }       
    }
    else
    {
        // 不能够被检索的工作线程，由用户自行管理
    }
    LOG(5)("worker [%s] start. workname = %s status: %d sub-workers = %d \n", 
        worker->classname, worker->workername, 
        worker->status, (int)sis_map_pointer_getsize(worker->workers));
	return worker;
}

void sis_worker_destroy(void *worker_)
{   
    s_sis_worker *worker =(s_sis_worker *)worker_;

    LOG(5)("worker [%s] kill.\n", worker->workername);
    // 应该先释放 notice 再释放其他的
    if (worker->workers)
    {
        sis_map_pointer_destroy(worker->workers);
    }
    if(worker->status & SIS_SERVER_STATUS_WORK_INIT)
    {
        if (worker->service_thread)
        {
            sis_service_thread_destroy(worker->service_thread);
            // 应该在线程退出后执行，但有问题，所以先放这里
            if (worker->slots->work_uninit)
            {
                worker->slots->work_uninit(worker);
            }
        }
    }
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
    if(worker->status & SIS_SERVER_STATUS_MAIN_INIT)
    {
        if (worker->slots && worker->slots->uninit)
        {
            worker->slots->uninit(worker);
        }
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
    SIS_WAIT(worker->status == SIS_SERVER_STATUS_INITED);

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

// 仅仅找当前 worker->workers 下的 workname_ = aaa
s_sis_worker *sis_worker_get(s_sis_worker *worker_, const char *workname_)
{
    if (!worker_) // server 的 worker
    {
        s_sis_server *server = sis_get_server();
        return sis_map_pointer_get(server->workers, workname_);
    }
    // else
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(worker_->workers);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sis_worker *work = (s_sis_worker *)sis_dict_getval(de);
            printf("---- %s : [%s] %s\n", work->classname, (char *)sis_dict_getkey(de), work->workername);
        }
        sis_dict_iter_free(di);

        s_sis_sds wname = _sis_worker_get_workname(worker_, workname_);
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
    int count = sis_str_substr_nums(workname_, '.') - 1;
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

// 通知worker去执行 working 
void sis_worker_notice(s_sis_worker *worker_)
{
    // printf("====== %p\n", worker_->service_thread);
    // if (worker_ && worker_->service_thread && worker_->service_thread->work_mode == SIS_WORK_MODE_NOTICE)
    if (worker_ && worker_->service_thread)
    {
        sis_service_thread_wait_notice(worker_->service_thread);
    }
}
