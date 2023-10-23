
#include <server.h>
#include <signal.h>
#include <worker.h>

static s_sis_server _server = {
	.status = SERVER_STATUS_NONE,
	.conf_h = NULL,
	.json_h = NULL,
	.work_mode = SERVER_WORK_MODE_NORMAL,
	.log_level = 5,
	.log_size = 10,
};

extern s_sis_modules *__modules[];
extern const char *__modules_name[];

/**
 * @brief 根据JSON配置文件创建工作者并初始化，包括：
* (1) 根据工作者的classname获取对应的接插件module
* (2) 调用module的初始化方法init
* (3) 将module的methods添加到工作者的methods表中
* (4) 调用一遍module的method_init方法
* (5) 启动独立线程运行module的working函数
 * @return int 工作者列表的数量
 */
int _server_open_workers()
{
	// 从conf配置中读取workers的json设置文件
	s_sis_json_node *workers = sis_json_cmp_child_node(_server.cfgnode, "workers");
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

void _sig_kill(int sn)
{
	// 最简单的方法
	if (sis_get_signal() == SIS_SIGNAL_EXIT)
	{
		printf("force exit!\n");  
		exit(0); 
	}
	// 通知其他循环工作终止
	sis_set_signal(SIS_SIGNAL_EXIT);
	// 通知等待server停止的循环终止
	_server.status = SERVER_STATUS_EXIT;
	// 这里清除所有的worker 当workers列表为空时表示所有work都已经关闭
	// printf("exit! %d\n", sis_map_pointer_getsize(_server.workers));  
	sis_map_pointer_clear(_server.workers);
}
void _fmt_trans(const char *confname, const char *jsonname)
{
	LOG(8)("loading config file [%s]\n", _server.conf_name);
	if (!sis_file_exists(_server.conf_name))
	{
		printf("conf file %s no finded.\n", _server.conf_name);
		return ;
	}
	_server.conf_h = sis_conf_open(_server.conf_name);
	if (!_server.conf_h)
	{
		printf("config file %s format error.\n", _server.conf_name);
		return ;
	}
	size_t ilen = 0;
	char *str = sis_json_output(_server.conf_h->node, &ilen);
	if (str && ilen > 0)
	{
		sis_writefile(_server.json_name, str, ilen);
		sis_free(str);
	}
	sis_conf_close(_server.conf_h);
}
// 读取配置文件
bool _server_open(const char *cmdinfo)
{
	char config[1024];
	if (_server.load_mode )
	{
		sis_strcpy(config, 1024, _server.json_name);
	}
	else
	{
		sis_strcpy(config, 1024, _server.conf_name);
	}
	
	printf("loading config file [%s]\n", config);
	if (!sis_file_exists(config))
	{
		printf("conf file %s no finded.\n", config);
		return false;
	}
	if (_server.load_mode)
	{
		_server.json_h = sis_json_open(config);
		if (!_server.json_h)
		{
			printf("config file %s format error.\n", config);
			return false;
		}
		_server.cfgnode = _server.json_h->node;	
	}
	else
	{
		_server.conf_h = sis_conf_open(config);
		if (!_server.conf_h)
		{
			printf("config file %s format error.\n", config);
			return false;
		}
		_server.cfgnode = _server.conf_h->node;	
	}

	if (sis_strlen(cmdinfo) > 0)
	{
		s_sis_json_handle *hcmd = sis_json_open(cmdinfo);
		if (!hcmd)
		{
			printf("merge file %s no open.\n", cmdinfo);
		}
		else
		{
			// int ii = 1;
			// sis_json_show(_server.cfgnode, &ii);
			sis_json_object_merge(_server.cfgnode, hcmd->node);

			// char *str = NULL;
			// size_t olen;
			// str = sis_json_output(_server.cfgnode, &olen);
			// printf(str);

			sis_json_close(hcmd);
		}
	}

	char conf_path[SIS_PATH_LEN];
	sis_file_getpath(config, conf_path, SIS_PATH_LEN);

	s_sis_json_node *lognode = sis_json_cmp_child_node(_server.cfgnode, "log");
	if (lognode)
	{
		// printf("%s || %s\n",conf_path, config);
		sis_cat_fixed_path(conf_path, sis_json_get_str(lognode, "path"),
						   _server.log_path, SIS_PATH_LEN);
		// printf("%s == %s\n",conf_path, _server.log_path);
		_server.log_level = sis_json_get_int(lognode, "level", 5);
		_server.log_size = sis_json_get_int(lognode, "maxsize", 10) * 1024 * 1024;
	}
	else
	{
		sis_strcpy(_server.log_path, SIS_PATH_LEN, conf_path);
	}
	// 生成log文件
	char name[SIS_PATH_LEN];
	if (sis_strlen(cmdinfo) > 0)
	{
		sis_file_getname(cmdinfo, name, SIS_PATH_LEN);
	}
	else
	{
		sis_file_getname(config, name, SIS_PATH_LEN);
	}
	size_t len = strlen(name);
	for (int i = (int)len - 1; i > 0; i--)
	{
		if (name[i] == '.')
		{
			name[i] = 0;
			break;
		}
	}
	if (_server.work_mode & SERVER_WORK_MODE_DATE)
	{
		int idate = sis_time_get_idate(0);
		sis_sprintf(_server.log_name, SIS_PATH_LEN, "%s%d.%s.log", _server.log_path, idate, name);
	}
	else
	{
		sis_sprintf(_server.log_name, SIS_PATH_LEN, "%s%s.log", _server.log_path, name);
	}

	return true;
}

void _server_merge_cmd(const char *cmdinfo)
{
	s_sis_json_handle *handle = sis_json_open(cmdinfo);
	if (!handle)
	{
		printf("merge file %s no open.\n", cmdinfo);
		return ;
	}
	

	sis_json_close(handle);
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

/**
 * @brief 根据工作者的classname，从模块HASH表中查找对应的接插件方法并返回
 * @param name_ 工作者的classname
 * @return s_sis_modules* 找到的s_sis_modules指针
 */
s_sis_modules *sis_get_worker_slot(const char *name_)
{
	return (s_sis_modules *)sis_map_pointer_get(_server.modules, name_);
}

void _server_help()
{
	printf("command format:\n");
	printf("		-f xxxx.conf : install custom conf. \n");
	printf("		-j xxxx.json : install custom json. \n");
	printf("		-t xxxx.conf xxxx.json : trans conf to json. \n");
	printf("		-d           : debug mode run. \n");
	printf("		-w workinfo.json : install workinfo config. \n");
	printf("		-h           : help. \n");
}
/**
 * @brief 加载所有模块至_server.modules
 * @return int 
 */
int sis_server_init()
{
	// 加载模块
	_server.modules = sis_map_pointer_create();
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
void sis_server_uninit()
{
	sis_map_pointer_destroy(_server.modules);
}

 #ifndef TEST_DEBUG 
 
int main(int argc, char *argv[])
{
	sis_sprintf(_server.conf_name, 1024, "%s.conf", argv[0]);
	sis_sprintf(_server.json_name, 1024, "%s.json", argv[0]);
	int c = 1;
	char cmdinfo[255]; 
	cmdinfo[0] = 0;
	while (c < argc)
	{
		if (argv[c][0] == '-' && argv[c][1] == 't' && argv[c + 1])
		{
			_server.fmt_trans = 1;
			sis_strcpy(_server.conf_name, 1024, argv[c + 1]);
			c++;
			if (argv[c + 1])
			{
				sis_strcpy(_server.json_name, 1024, argv[c + 1]);
			}
			c++;
		}
		else if (argv[c][0] == '-' && argv[c][1] == 'f' && argv[c + 1])
		{
			sis_strcpy(_server.conf_name, 1024, argv[c + 1]);
			c++;
			_server.load_mode = 0;
		}
		else if (argv[c][0] == '-' && argv[c][1] == 'j' && argv[c + 1])
		{
			sis_strcpy(_server.json_name, 1024, argv[c + 1]);
			c++;
			_server.load_mode = 1;
		}
		else if (argv[c][0] == '-' && argv[c][1] == 'w' && argv[c + 1])
		{
			sis_strcpy(cmdinfo, 255, argv[c + 1]);
			c++;
		}
		else if (argv[c][0] == '-' && argv[c][1] == 'p')
		{
			_server.work_mode = _server.work_mode | SERVER_WORK_MODE_DEBUG;
		}
		else if (argv[c][0] == '-' && argv[c][1] == 'd')
		{
			_server.work_mode = _server.work_mode | SERVER_WORK_MODE_FACE | SERVER_WORK_MODE_DATE;
		}
		else if (argv[c][0] == '-' && argv[c][1] == 'h')
		{
			_server_help();
			return 0;
		}
		c++;
	}
	if (_server.fmt_trans > 0)
	{
		_fmt_trans(_server.conf_name, _server.json_name);
		exit(0);
	}

	safe_memory_start();

	if (!_server_open(cmdinfo))
	{
		printf("config file %s load error.\n", _server.load_mode ? _server.json_name : _server.conf_name);
		return 0;
	}
	if (sis_server_init() < 1)
	{
		printf("no active modules.\n");
		sis_map_pointer_destroy(_server.modules);
		return 0;
	}
	
	if (!(_server.work_mode & SERVER_WORK_MODE_DEBUG) && !(_server.work_mode & SERVER_WORK_MODE_FACE))
	{
		sis_fork_process();
	}	

	if (!(_server.work_mode & SERVER_WORK_MODE_DEBUG))
	{
		sis_log_open(_server.log_name, _server.log_level, _server.log_size);
	}
	else
	{
		sis_log_open(NULL, _server.log_level, _server.log_size);
	}
	sis_signal(SIGINT, _sig_kill);
	sis_signal(SIGKILL, _sig_kill);
	sis_signal(SIGTERM, _sig_kill);
	sis_sigignore(SIGPIPE);

	sis_set_signal(SIS_SIGNAL_WORK);

	//  为工作者HASH表分配内存
	_server.workers = sis_map_pointer_create_v(sis_worker_destroy);

	// 创建工作者并初始化，调用对应module的初始化方法init和method_init
	int workers = _server_open_workers();

	// 循环等待所有work都退出
	if (workers > 0)
	{
		printf("program start. workers = %d.\n", workers);
		_server.status = SERVER_STATUS_WORK;
		while (_server.status != SERVER_STATUS_NONE)
		{
			if (sis_map_pointer_getsize(_server.workers) < 1)
			{
				// 所有work结束就退出
				_server.status = SERVER_STATUS_NONE;
				break; 
			}
			sis_sleep(500);
		}
	}
	else
	{
		printf("no active worker.\n");		
	}
	sis_map_pointer_destroy(_server.workers);
	// 释放插件
	sis_server_uninit();

	LOG(3)("program exit.\n");

	if (_server.load_mode)
	{
		sis_json_close(_server.json_h);
	}
	else
	{
		sis_conf_close(_server.conf_h);
	}
	sis_log_close();

	safe_memory_stop();

	return 0;
}
#endif
