
#include <server.h>
#include <signal.h>
#include <worker.h>

static s_sis_server _server = {
	.status = SIS_SERVER_STATUS_NOINIT,
	.config = NULL,
	.work_mode = SERVER_WORK_MODE_NORMAL,
	.log_screen = false,
	.log_level = 5,
	.log_size = 10,
};

extern s_sis_modules *__modules[];
extern const char *__modules_name[];
/////////////////////////////////
//  server
/////////////////////////////////
int _server_open_workers()
{
	s_sis_json_node *workers = sis_json_cmp_child_node(_server.config->node, "workers");
	if (!workers)
	{
		return 0;
	}
	s_sis_json_node *next = sis_json_first_node(workers);
	while (next)
	{
		if (!sis_map_pointer_get(_server.workers, next->key))
		{
			s_sis_worker *worker = sis_worker_create(NULL, next);
			if (worker)
			{
				sis_map_pointer_set(_server.workers, next->key, worker);
			}
		}
		next = next->next;
	}
	return sis_map_pointer_getsize(_server.workers);
}

void _server_close()
{
	if (_server.status == SIS_SERVER_STATUS_CLOSE)
	{
		return;
	}
	_server.status = SIS_SERVER_STATUS_CLOSE;
}

void _sig_int(int sn)
{
	_server_close();
}

void _sig_kill(int sn)
{
	_server_close();
}

bool _server_open()
{
	if (!sis_file_exists(_server.conf_name))
	{
		printf("conf file %s no finded.\n", _server.conf_name);
		return false;
	}

	_server.config = sis_conf_open(_server.conf_name);
	if (!_server.config)
	{
		printf("conf file %s format error.\n", _server.conf_name);
		return false;
	}

	char conf_path[SIS_PATH_LEN];
	sis_file_getpath(_server.conf_name, conf_path, SIS_PATH_LEN);

	s_sis_json_node *lognode = sis_json_cmp_child_node(_server.config->node, "log");
	if (lognode)
	{
		// printf("%s || %s\n",conf_path, _server.conf_name);
		sis_get_fixed_path(conf_path, sis_json_get_str(lognode, "path"),
						   _server.log_path, SIS_PATH_LEN);
		// printf("%s == %s\n",conf_path, _server.log_path);
		_server.log_screen = sis_json_get_bool(lognode, "screen", false);
		_server.log_level = sis_json_get_int(lognode, "level", 5);
		_server.log_size = sis_json_get_int(lognode, "maxsize", 10) * 1024 * 1024;
	}
	else
	{
		sis_strcpy(_server.log_path, SIS_PATH_LEN, conf_path);
	}
	// 生成log文件
	char name[SIS_PATH_LEN];
	sis_file_getname(_server.conf_name, name, SIS_PATH_LEN);
	size_t len = strlen(name);
	for (int i = (int)len - 1; i > 0; i--)
	{
		if (name[i] == '.')
		{
			name[i] = 0;
			break;
		}
	}
	sis_sprintf(_server.log_name, SIS_PATH_LEN, "%s%s.log", _server.log_path, name);

	return true;
}

int _server_load_modules()
{
	int i, count = 0;
    for (i = 0; __modules[i]; i++) 
	{
		count++;
		sis_map_pointer_set(_server.modules, __modules_name[i], __modules[i]);
		printf("load modules %s ok.\n", __modules_name[i]);
    }
	printf("load modules ok. count = %d \n", count);
	return sis_map_pointer_getsize(_server.modules);
}

bool sis_is_server(void *self_)
{
	if (self_ == &_server)
	{
		return true;
	}
	return false;
}

s_sis_server *sis_get_server()
{
	return &_server;
}

s_sis_modules *sis_get_worker_slot(const char *name_)
{
	return (s_sis_modules *)sis_map_pointer_get(_server.modules, name_);
}

void _server_help()
{
	printf("command format:\n");
	printf("		-f xxxx.conf : install custom conf. \n");
	printf("		-d           : debug mode run. \n");
	printf("		-h           : help. \n");
}
#if 0
int main(int argc, char *argv[])
{
	sis_sprintf(_server.conf_name, 255, "%s.conf", argv[0]);

	int c = 1;
	while (c < argc)
	{
		if (argv[c][0] == '-' && argv[c][1] == 'f' && argv[c + 1])
		{
			sis_strcpy(_server.conf_name, 255, argv[c + 1]);
			c++;
		}
		else if (argv[c][0] == '-' && argv[c][1] == 'd')
		{
			_server.work_mode = _server.work_mode | SERVER_WORK_MODE_DEBUG;
		}
		else if (argv[c][0] == '-' && argv[c][1] == 'h')
		{
			_server_help();
			return 0;
		}
		c++;
	}
	if (_server.work_mode & SERVER_WORK_MODE_DEBUG)
	{
		// 如果是debug模式就开启内存检查
		safe_memory_start();
	}
	if (!_server_open())
	{
		printf("conf file %s load error.\n", _server.conf_name);
		return 0;
	}
	// 加载模块
	_server.modules = sis_map_pointer_create();
	int modules = _server_load_modules();
	if (modules < 1)
	{
		printf("no active modules.\n");
		sis_map_pointer_destroy(_server.modules);
		return 0;
	}
	
	if (!(_server.work_mode & SERVER_WORK_MODE_DEBUG))
	{
		sis_fork_process();
	}	

	if (!_server.log_screen)
	{
		sis_log_open(_server.log_name, _server.log_level, _server.log_size);
	}
	else
	{
		sis_log_open(NULL, _server.log_level, _server.log_size);
	}
	sis_signal(SIGINT, _sig_int);
	sis_signal(SIGKILL, _sig_kill);
	sis_signal(SIGTERM, _sig_kill);
	sis_sigignore(SIGPIPE);

	//  创建工作者
	_server.workers = sis_map_pointer_create_v(sis_worker_destroy);
	int workers = _server_open_workers();

	if (workers > 0)
	{
		printf("program start. workers = %d.\n", workers);
		_server.status = SIS_SERVER_STATUS_INITED;

		while (_server.status != SIS_SERVER_STATUS_CLOSE)
		{
			sis_sleep(500);
		}
	}
	else
	{
		printf("no active worker.\n");		
	}
	sis_map_pointer_destroy(_server.workers);

	// 释放插件
	sis_map_pointer_destroy(_server.modules);

	LOG(3)("program exit.\n");

	sis_conf_close(_server.config);

	sis_log_close();

	if (_server.work_mode & SERVER_WORK_MODE_DEBUG)
	{
		safe_memory_stop();
	}

	return 0;
}
#endif
