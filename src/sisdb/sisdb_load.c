
#include <sis_comm.h>
#include <sisdb_io.h>

#define CHECK_SERVER_STATUS  { if (sisdb_get_server_status()!= SIS_SERVER_STATUS_INITED)  \
								return sis_module_reply_with_error(ctx_, "server starting..."); }

// 入口初始化，根据配置文件获取初始化所有信息，并返回s_sisdb_server指针，
// 一个实例只有一个server，但是可以有多个
char *load_sisdb_open(const char *conf_)
{		
	if (!sis_file_exists(conf_))
	{
		sis_out_log(3)("conf file %s no finded.\n", conf_);
		return NULL;
	}
	return sisdb_open(conf_);
}

int load_sisdb_close(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	sis_module_not_used(argc_);
	sis_module_not_used(argv_);
	sisdb_close();
	return sis_module_reply_with_simple_string(ctx_, "OK");
}

// 获取数据可以根据 command中的 format来确定是json或者是struct
// 可以单独取数据头定义，比如fields等的定义
// 但保存在内存中的数据一定是二进制struct的数据格式，仅仅在输出时做数据格式转换
// set数据时也可以是json或struct格式数据，获得数据后会自动转换成不压缩的struct数据格式
int load_sisdb_get(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	CHECK_SERVER_STATUS;

	if (argc_ < 2)
	{
		return sis_module_wrong_arity(ctx_);
	}

	s_sis_sds o;
	const char *key = sis_module_string_get(argv_[1], NULL);
	if (argc_ == 3)
	{
		o = sisdb_get_sds(key, sis_module_string_get(argv_[2], NULL));
	}
	else
	{
		o = sisdb_fast_get_sds(key);
	}
	if (o)
	{
		// sis_out_binary("get out", o, 30);
		// printf("get out ...%lu\n", sis_sdslen(o));
		sis_module_reply_with_buffer(ctx_, o, sis_sdslen(o));
		sis_sdsfree(o);
		return SIS_MODULE_OK;
	}
	return sis_module_reply_with_null(ctx_);
}
// 设置系统级别的一些默认操作，通过用于一些危险操作的授权，例如：
	// output   查询结果写入磁盘，
    // super  允许删除表格，系统的del只能清除表格内容，不能删除表结构
int load_sisdb_cfg(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	CHECK_SERVER_STATUS;

	if (argc_ < 2)
	{
		return sis_module_wrong_arity(ctx_);
	}

	int o = sisdb_cfg_option(sis_module_string_get(argv_[1], NULL));
	
	if (o >=0 )
	{
		return sis_module_reply_with_int64(ctx_, o);
	} 
	return sis_module_reply_with_null(ctx_);

}

// 显示表信息和其他系统级别的信息
int load_sisdb_show(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	CHECK_SERVER_STATUS;

	const char *key = NULL;
	const char *com = NULL;

	if (argc_ >= 2)
	{
		key = sis_module_string_get(argv_[1], NULL);
		if (argc_ >= 3) com = sis_module_string_get(argv_[2], NULL);
	}

	s_sis_sds o = sisdb_show_sds(key, com);

	if (o)
	{
		sis_module_reply_with_simple_string(ctx_, o);
		sis_sdsfree(o);
		return SIS_MODULE_OK;
	}
	return sis_module_reply_with_null(ctx_);
}

// sisdb.call [procname] [command]
// 提供更高级的查询语句，仅仅返回特定格式数据
int load_sisdb_call(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	CHECK_SERVER_STATUS;
	if (argc_ < 2)
	{
		return sis_module_wrong_arity(ctx_);
	}
	// 如果有返回值值，一律为json格式
	s_sis_sds o;
	const char *key = sis_module_string_get(argv_[1], NULL);
	if (argc_ >= 3)
	{
		o = sisdb_call_sds(key, sis_module_string_get(argv_[2], NULL));
	}
	else
	{
		o = sisdb_call_sds(key, NULL);
	}
	if (o)
	{
		// sis_out_binary("call out", o, 30);
		// printf("call out ...%lu\n", sis_sdslen(o));
		sis_module_reply_with_buffer(ctx_, o, sis_sdslen(o));
		// sis_module_reply_with_simple_string(ctx_, o);
		// sis_module_reply_with_buffer(ctx_, o, sis_sdslen(o));
		sis_sdsfree(o);
		return SIS_MODULE_OK;
	}
	return sis_module_reply_with_null(ctx_);
}
int load_sisdb_del(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	CHECK_SERVER_STATUS;
	if (argc_ < 2)
	{
		return sis_module_wrong_arity(ctx_);
	}
	int o = 0;
	const char *key = sis_module_string_get(argv_[1], NULL);
	if (argc_ == 3)
	{
		size_t len = 0;
		const char *com = sis_module_string_get(argv_[2], &len);
		o = sisdb_del(key, com, len);
	}
	else
	{
		o = sisdb_del(key, NULL, 0);
	}
	if (o > 0)
	{
		return sis_module_reply_with_int64(ctx_, o);
	}
	return sis_module_reply_with_null(ctx_);
}
int load_sisdb_set(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	CHECK_SERVER_STATUS;
	if (argc_ != 3)
	{
		return sis_module_wrong_arity(ctx_);
	}
	// printf("%s: %.90s\n", sis_module_string_get(argv_[1], NULL), sis_module_string_get(argv_[3], NULL));

	size_t len;
	const char *key = sis_module_string_get(argv_[1], NULL);
	const char *val = sis_module_string_get(argv_[2], &len);

	int o = sisdb_set_json(key, val, len);

	if (!o)
	{
		return sis_module_reply_with_simple_string(ctx_, "OK");
	}
	return sis_module_reply_with_null(ctx_);
}

int load_sisdb_sset(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	CHECK_SERVER_STATUS;
	if (argc_ != 3)
	{
		return sis_module_wrong_arity(ctx_);
	}
	// printf("%s: %.90s\n", sis_module_string_get(argv_[1], NULL), sis_module_string_get(argv_[3], NULL));

	size_t len;
	const char *key = sis_module_string_get(argv_[1], NULL);
	const char *val = sis_module_string_get(argv_[2], &len);

	int o = sisdb_set_struct(key, val, len);

	if (!o)
	{
		return sis_module_reply_with_simple_string(ctx_, "OK");
	}
	return sis_module_reply_with_null(ctx_);
}
int load_sisdb_save(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	CHECK_SERVER_STATUS;
	sis_module_not_used(argc_);
	sis_module_not_used(argv_);

	if (sisdb_save())
	{
		return sis_module_reply_with_simple_string(ctx_, "OK");
	}
	return sis_module_reply_with_null(ctx_);
}

int load_sisdb_new(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	CHECK_SERVER_STATUS;
	if (argc_ != 3)
	{
		return sis_module_wrong_arity(ctx_);
	}
	// printf("%s: %.90s\n", sis_module_string_get(argv_[1], NULL), sis_module_string_get(argv_[3], NULL));

	size_t len;
	const char *table = sis_module_string_get(argv_[1], NULL);
	const char *attrs = sis_module_string_get(argv_[2], &len);

	int o = sisdb_new(table, attrs, len);

	if (!o)
	{
		return sis_module_reply_with_simple_string(ctx_, "OK");
	}
	return sis_module_reply_with_null(ctx_);
}

int load_sisdb_update(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	CHECK_SERVER_STATUS;
	if (argc_ != 3)
	{
		return sis_module_wrong_arity(ctx_);
	}
	// printf("%s: %.90s\n", sis_module_string_get(argv_[1], NULL), sis_module_string_get(argv_[3], NULL));

	size_t len;
	const char *key = sis_module_string_get(argv_[1], NULL);
	const char *val = sis_module_string_get(argv_[2], &len);

	int o = sisdb_update(key, val, len);

	if (!o)
	{
		return sis_module_reply_with_simple_string(ctx_, "OK");
	}
	return sis_module_reply_with_null(ctx_);
}

int sis_module_on_unload()
{
	sisdb_close();
	sis_out_log(3)("close sisdb.\n");
	safe_memory_stop();
	return SIS_MODULE_OK;
}
int sis_module_on_load(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	safe_memory_start();
	// 先取得服务名
	char *service_name;
	if (argc_ == 1)
	{
		service_name = load_sisdb_open(((s_sis_object *)argv_[0])->ptr);
	}
	else
	{
		service_name = load_sisdb_open("../sisdb/bin/sisdb.conf");
	}
	if (!service_name || !*service_name)
	{
		sis_out_log(3)("init sisdb error.\n");
		return SIS_MODULE_ERROR;
	}
	if (sis_module_init(ctx_, service_name, 1, SIS_MODULE_VER) == SIS_MODULE_ERROR)
		return SIS_MODULE_ERROR;

	/* Log the list of parameters passing loading the module. */
	// for (int k = 0; k < argc_; k++)
	// {
	// 	const char *s = sis_module_string_get(argv_[k], NULL);
	// 	printf("module loaded with argv_[%d] = %s\n", k, s);
	// }
	// printf("====1.2===\n");

	char command[SIS_MAXLEN_COMMAND];
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.close", service_name);
	if (sis_module_create_command(ctx_, command, load_sisdb_close,
								  "readonly",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.get", service_name);
	if (sis_module_create_command(ctx_, command, load_sisdb_get,
								  "readonly",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.cfg", service_name);
	if (sis_module_create_command(ctx_, command, load_sisdb_cfg,
								  "readonly",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.show", service_name);
	if (sis_module_create_command(ctx_, command, load_sisdb_show,
								  "readonly",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	// 去掉call，直接在get中实现，$+方法名
	// sisdb.get $xxxxx {"argv":{....}}  当key值为$开头就是方法调用
	// show 方法也注入到这种方式中，简化访问接口，一个get全部搞定
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.call", service_name);
	if (sis_module_create_command(ctx_, command, load_sisdb_call,
								  "write deny-oom",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.del", service_name);
	if (sis_module_create_command(ctx_, command, load_sisdb_del,
								  "write deny-oom",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.set", service_name);
	if (sis_module_create_command(ctx_, command, load_sisdb_set,
								  "write deny-oom",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(command, SIS_MAXLEN_KEY, "%s.sset", service_name);
	if (sis_module_create_command(ctx_, command, load_sisdb_sset,
								  "write deny-oom",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.save", service_name);
	if (sis_module_create_command(ctx_, command, load_sisdb_save,
								  "write deny-oom",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	// 创建数据表，后面跟各种属性和字段定义
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.new", service_name);
	if (sis_module_create_command(ctx_, command, load_sisdb_new,
								  "write deny-oom",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	// 修改数据表，可以是字段，可以是其他属性，只在设置字段时会锁住进程，处理完后再恢复访问
	// 情况太复杂，牵涉到scale修改导致的time字段修改和数据是否合并等功能
	// 可能只支持字段增加和删除，不能支持字段修改，
	// sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.update", service_name);
	// if (sis_module_create_command(ctx_, command, load_sisdb_update,
	// 							  "write deny-oom",
	// 							  0, 0, 0) == SIS_MODULE_ERROR)
	// {
	// 	return SIS_MODULE_ERROR;
	// }	
	
	return SIS_MODULE_OK;
}
