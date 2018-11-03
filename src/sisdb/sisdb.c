
#include <sis_db_io.h>
#include <sis_comm.h>

int call_sisdb_list(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	sis_module_not_used(argc_);
	sis_module_not_used(argv_);

	s_sis_sds o = sisdb_list_sds();
	if (o)
	{
		sis_module_reply_with_simple_string(ctx_, o);
		sis_sdsfree(o);
		return SIS_MODULE_OK;
	}
	return sis_module_reply_with_error(ctx_, "sisdb list table error.\n");
}
int call_sisdb_save(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	sis_module_not_used(argc_);
	sis_module_not_used(argv_);

	if (sisdb_save())
	{
		return sis_module_reply_with_simple_string(ctx_, "OK");
	}
	return sis_module_reply_with_error(ctx_, "sisdb save error.\n");
}
int call_sisdb_saveto(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	if (argc_ < 1)
	{
		return sis_module_wrong_arity(ctx_);
	}
	bool o;
	if (argc_ == 2) {
		o = sisdb_saveto(sis_module_string_get(argv_[1], NULL), NULL);
	} else {
		o = sisdb_saveto(
				sis_module_string_get(argv_[1], NULL),
				sis_module_string_get(argv_[2], NULL));
	}
	if (o)
	{
		return sis_module_reply_with_simple_string(ctx_, "OK");
	}
	return sis_module_reply_with_error(ctx_, "sisdb saveto error.\n");
}
// 获取数据可以根据command中的format来确定是json或者是struct
// 可以单独取数据头定义，比如fields等的定义
// 但保存在内存中的数据一定是二进制struct的数据格式，仅仅在输出时做数据格式转换
// set数据时也可以是json或struct格式数据，获得数据后会自动转换成不压缩的struct数据格式
int call_sisdb_get(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	if (argc_ < 2)
	{
		return sis_module_wrong_arity(ctx_);
	}

	const char *key = sis_module_string_get(argv_[1], NULL);
	char db[SIS_TABLE_MAXLEN];
	char code[SIS_CODE_MAXLEN];
	int count = sis_str_substr_nums(key, '.');
	if (count == 1)
	{
		sis_strcpy(db, SIS_TABLE_MAXLEN, key);
		code[0] = 0;
	}
	else if (count == 2)
	{
		sis_str_substr(db, SIS_TABLE_MAXLEN, key, '.', 1);
		sis_str_substr(code, SIS_CODE_MAXLEN, key, '.', 0);
	}
	else
	{
		sis_module_reply_with_error(ctx_, "set data key error.\n");
	}
	s_sis_sds o;
	if (argc_ == 3)
	{
		o = sisdb_get_sds(db, code, sis_module_string_get(argv_[2], NULL));
	}
	else
	{
		o = sisdb_get_sds(db, code, "{\"format\":\"json\"}");
	}
	if (o)
	{
		sis_out_binary("get out",o, 30);
		printf("get out ...%lu\n",sis_sdslen(o));

		sis_module_reply_with_buffer(ctx_, o, sis_sdslen(o));
		sis_sdsfree(o);
		return SIS_MODULE_OK;
	}
	return sis_module_reply_with_error(ctx_, "sisdb get error.\n");
}
int call_sisdb_set(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	if (argc_ != 4)
	{
		return sis_module_wrong_arity(ctx_);
	}
	// printf("%s: %.90s\n", sis_module_string_get(argv_[1], NULL), sis_module_string_get(argv_[3], NULL));

	const char *key = sis_module_string_get(argv_[1], NULL);
	int count = sis_str_substr_nums(key, '.');
	if (count != 2)
	{
		return sis_module_reply_with_error(ctx_, "set data key error.\n");
	}
	char db[SIS_TABLE_MAXLEN];
	char code[SIS_CODE_MAXLEN];
	sis_str_substr(db, SIS_TABLE_MAXLEN, key, '.', 1);
	sis_str_substr(code, SIS_CODE_MAXLEN, key, '.', 0);

	int o;
	size_t len;
	const char *val = sis_module_string_get(argv_[3], &len);
	const char *dt = sis_module_string_get(argv_[2], NULL);

	o = sisdb_set(dt, db, code, val, len);

	if (!o)
	{
		return sis_module_reply_with_simple_string(ctx_, "OK");
	}
	return sis_module_reply_with_error(ctx_, "sisdb set error.\n");
}

char *call_sisdb_open(const char *conf_)
{
	if (!sis_file_exists(conf_))
	{
		sis_out_error(3)("conf file %s no finded.\n", conf_);
		return NULL;
	}
	return sisdb_open(conf_);
}
int call_sisdb_init(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	if (argc_ != 2)
	{
		return sis_module_wrong_arity(ctx_);
	}
	
	const char *market = sis_module_string_get(argv_[1], NULL);
	
	sisdb_init(market);
	
	return sis_module_reply_with_simple_string(ctx_, "OK");
}

int call_sisdb_close(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	sis_module_not_used(argc_);
	sis_module_not_used(argv_);
	sisdb_close();
	return sis_module_reply_with_simple_string(ctx_, "OK");
}
int sis_module_on_unload()
{
	printf("--------------close-----------\n");
	sisdb_close();
	safe_memory_stop();
	return SIS_MODULE_OK;
}
int sis_module_on_load(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	safe_memory_start();
	// 先取得服务名
	char *service;
	if (argc_ == 1)
	{
		// service = call_sisdb_open(sis_module_string_get(argv_[0], NULL));
		service = call_sisdb_open(((s_sis_object *)argv_[0])->ptr);
	}
	else
	{
		service = call_sisdb_open("../sisdb/conf/sisdb.conf");
	}
	if (!service || !*service)
	{
		sis_out_error(3)("init sisdb error.\n");
		return SIS_MODULE_ERROR;
	}
	char servicename[64];
	sis_sprintf(servicename, 64, "%s", service);
	if (sis_module_init(ctx_, servicename, 1, SIS_MODULE_VER) == SIS_MODULE_ERROR)
		return SIS_MODULE_ERROR;

	/* Log the list of parameters passing loading the module. */
	// for (int k = 0; k < argc_; k++)
	// {
	// 	const char *s = sis_module_string_get(argv_[k], NULL);
	// 	printf("module loaded with argv_[%d] = %s\n", k, s);
	// }

	sis_sprintf(servicename, 64, "%s.init", service);
	if (sis_module_create_command(ctx_, servicename, call_sisdb_init,
								  "write deny-oom",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(servicename, 64, "%s.close", service);
	if (sis_module_create_command(ctx_, servicename, call_sisdb_close,
								  "readonly",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(servicename, 64, "%s.list", service);
	if (sis_module_create_command(ctx_, servicename, call_sisdb_list,
								  "readonly",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(servicename, 64, "%s.get", service);
	if (sis_module_create_command(ctx_, servicename, call_sisdb_get,
								  "readonly",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(servicename, 64, "%s.set", service);
	if (sis_module_create_command(ctx_, servicename, call_sisdb_set,
								  "write deny-oom",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(servicename, 64, "%s.save", service);
	if (sis_module_create_command(ctx_, servicename, call_sisdb_save,
								  "write deny-oom",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(servicename, 64, "%s.saveto", service);
	if (sis_module_create_command(ctx_, servicename, call_sisdb_saveto,
								  "write deny-oom",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	return SIS_MODULE_OK;
}
