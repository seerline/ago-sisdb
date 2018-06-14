
#include <server.h>
#include <signal.h>
static s_stock_server server = {
	.status = STS_SERVER_STATUS_NOINIT,
	.config = NULL,
	.work_mode = STOCK_WORK_MODE_NORMAL,
	.loglevel = 5,
	.logsize = 10
};

/////////////////////////////////////////
//
// thread function
//
/////////////////////////////////////////
void *_thread_proc_worker(void *argv_)
{
	s_sts_worker *worker = (s_sts_worker *)argv_;
	if (!worker)
	{
		return NULL;
	}
	sts_thread_wait_create(&worker->wait);

	sts_thread_wait_start(&worker->wait);
	if (worker->open)
	{
		worker->open(worker);
	}
	while (server.status != STS_SERVER_STATUS_CLOSE)
	{
		// 处理
		if (worker->work_mode == STS_WORK_MODE_GAPS)
		{
			if (sts_thread_wait_sleep(&worker->wait, worker->work_gaps) == STS_ETIMEDOUT)
			{
				if (worker->working)
				{
					worker->working(worker);
				}
			}
		}
		else
		{
			if (sts_thread_wait_sleep(&worker->wait, 30) == STS_ETIMEDOUT)
			// 30秒判断一次
			{
				int min = sts_time_get_iminute(0);
				for (int k = 0; k < worker->work_plans->count; k++)
				{
					uint16 *lm = sts_struct_list_get(worker->work_plans, k);
					if (min == *lm)
					{
						if (worker->working)
						{
							worker->working(worker);
						}
					}
				}
			}
		}
		// printf("server.status ... %d\n",server.status);
	}
	if (worker->close)
	{
		worker->close(worker); //这里做结束的工作
	}
	sts_thread_wait_stop(&worker->wait);
	printf("[%s] thread end.\n",worker->classname);

	return NULL;
}

s_sts_worker *stock_worker_create(s_sts_json_node *node_)
{
	s_sts_worker *worker = sts_worker_create(node_);
	if (worker)
	{
		if (worker->init&&!worker->init(worker, node_))
		{
			sts_out_error(1)("load worker config error\n");
			sts_worker_destroy(worker);
			return NULL;	
		}
		if (!sts_thread_create(_thread_proc_worker, worker, &worker->thread_id))
		{
			sts_out_error(1)("can't start worker thread\n");
			sts_worker_destroy(worker);
			return NULL;
		}
		printf("worker [%s] start.\n",worker->workname);
	}
	return worker;
}

int stock_server_open()
{
	s_sts_json_node *worker = sts_json_cmp_child_node(server.config->node, "workers");
	if (!worker)
	{
		return 0;
	}
	s_sts_json_node *next = sts_json_first_node(worker);
	if(server.work_mode&STOCK_WORK_MODE_SINGLE) 
	{
		while (next)
		{
			if (strlen(server.single_name)>0 && !sts_strcasecmp(next->key, server.single_name))
			{
				s_sts_worker *worker = stock_worker_create(next);
				if (worker)
				{
					sts_pointer_list_push(server.workers, worker);
				}	
			}
			next = next->next;
		}
	} 
	else 
	{
		while (next)
		{
			s_sts_worker *worker = stock_worker_create(next);
			if (worker)
			{
				sts_pointer_list_push(server.workers, worker);
			}
			next = next->next;
		}
	}
	return server.workers->count;
}

void stock_server_close()
{
	if (server.status == STS_SERVER_STATUS_CLOSE)
	{
		return;
	}
	server.status = STS_SERVER_STATUS_CLOSE;

}

void _sig_int(int sn)
{
	stock_server_close();
}

void _sig_kill(int sn)
{
	stock_server_close();
}

bool stock_server_load()
{
	if (!sts_file_exists(server.conf_name))
	{
		printf("conf file %s no finded.\n", server.conf_name);
		return false;
	}
	
	server.config = sts_conf_open(server.conf_name);
	if (!server.config)
	{
		printf("conf file %s format error.\n", server.conf_name);
		return false;
	}

	char conf_path[STS_PATH_LEN];
	sts_file_getpath(server.conf_name, conf_path, STS_PATH_LEN);

	s_sts_json_node *lognode = sts_json_cmp_child_node(server.config->node, "log");
	if (lognode)
	{
		sts_get_fixed_path(conf_path, sts_json_get_str(lognode, "path"),
						   server.logpath, STS_PATH_LEN);

		server.loglevel = sts_json_get_int(lognode, "level", 5);
		server.logsize = sts_json_get_int(lognode, "maxsize", 10) * 1024 * 1024;
	}
	else
	{
		sts_strcpy(server.logpath, STS_PATH_LEN, conf_path);
	}
	// 生成log文件
	char name[STS_PATH_LEN];
	sts_file_getname(server.conf_name, name, STS_PATH_LEN);
	size_t len = strlen(name);
	for (int i = (int)len - 1; i > 0; i--)
	{
		if (name[i] == '.')
		{
			name[i] = 0;
			break;
		}
	}
	sts_sprintf(server.logname, STS_PATH_LEN, "%s%s.log", server.logpath, name);

	return true;
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("command line format:\n");
		printf("		dsserver xxxx.conf \n");
		printf("		-d : debug mode run \n");
		printf("		-n xxxx : only run xxxx] \n");
		return 0;
	}

	safe_memory_start();

	int c = 1;
	while(c < argc)
	{
		if (argv[c][0] != '-') 
		{
			sts_strcpy(server.conf_name, STS_PATH_LEN, argv[c]);
		}
		else if (argv[c][0] == '-' && argv[c][1] == 'd') 
		{
			server.work_mode = server.work_mode | STOCK_WORK_MODE_DEBUG;
		}
		else if (argv[c][0] == '-' && argv[c][1] == 'n'&& argv[c+1]) 
		{
			server.work_mode = server.work_mode | STOCK_WORK_MODE_SINGLE;
			sts_strcpy(server.single_name, STS_NAME_LEN, argv[c+1]);
			c++;
		} 
		c++;
	}

	if (!stock_server_load())
	{
		printf("conf file %s load error.\n", server.conf_name);
		return 0;
	}

	sts_log_open(server.logname, server.loglevel, server.logsize);

	// bool debug = sts_conf_get_bool(server.config->node, "debug");
	if (!(server.work_mode&STOCK_WORK_MODE_DEBUG))
	{
		sts_fork_process();
	}
	sts_signal(SIGINT, _sig_int);
	sts_signal(SIGKILL, _sig_kill);
	sts_signal(SIGTERM, _sig_kill);
	sts_sigignore(SIGPIPE);

	server.workers = sts_pointer_list_create();
	int workers = stock_server_open();

	if (workers < 1)
	{
		printf("no active worker.\n");
		sts_pointer_list_destroy(server.workers);
		goto error;
	}

	printf("program start. workers = %d \n", workers);
	server.status = STS_SERVER_STATUS_INITED;

	while (server.status != STS_SERVER_STATUS_CLOSE)
	{
		sts_sleep(500);
	}

	for (int k = 0; k < server.workers->count; k++)
	{
		s_sts_worker *worker = (s_sts_worker *)sts_pointer_list_get(server.workers, k);
		// 通知所有线程各自结束自己
		sts_thread_wait_kill(&worker->wait);

		printf("worker [%s] kill.\n",worker->workname);

		sts_thread_wait_destroy(&worker->wait);

		sts_worker_destroy(worker);
	}
	printf("program exit.\n");

	sts_pointer_list_destroy(server.workers);

error:
	sts_conf_close(server.config);

	sts_log_close();

	safe_memory_stop();

	return 1;
}
