
#include <sts_db_io.h>
#include <sts_comm.h>

int call_stsdb_list(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	sts_module_not_used(argc_);
	sts_module_not_used(argv_);

	s_sts_sds o = stsdb_list_sds();
	if (o)
	{
		sts_module_reply_with_simple_string(ctx_, o);
		sts_sdsfree(o);
		return STS_MODULE_OK;
	}
	return sts_module_reply_with_error(ctx_, "stsdb list table error.\n");
}
int call_stsdb_save(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	sts_module_not_used(argc_);
	sts_module_not_used(argv_);

	if (stsdb_save())
	{
		return sts_module_reply_with_simple_string(ctx_, "OK");
	}
	return sts_module_reply_with_error(ctx_, "stsdb save error.\n");
}
// 获取数据可以根据command中的format来确定是json或者是struct
// 可以单独取数据头定义，比如fields等的定义
// 但保存在内存中的数据一定是二进制struct的数据格式，仅仅在输出时做数据格式转换
// set数据时也可以是json或struct格式数据，获得数据后会自动转换成不压缩的struct数据格式
int call_stsdb_get(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ < 2)
	{
		return sts_module_wrong_arity(ctx_);
	}

	const char *key = sts_module_string_get(argv_[1], NULL);
	char db[STS_TABLE_MAXLEN];
	char code[STS_CODE_MAXLEN];
	int count = sts_str_substr_nums(key, '.');
	if (count == 1)
	{
		sts_strcpy(db, STS_TABLE_MAXLEN, key);
		code[0] = 0;
	}
	else if (count == 2)
	{
		sts_str_substr(db, STS_TABLE_MAXLEN, key, '.', 1);
		sts_str_substr(code, STS_CODE_MAXLEN, key, '.', 0);
	}
	else
	{
		sts_module_reply_with_error(ctx_, "set data key error.\n");
	}
	s_sts_sds o;
	if (argc_ == 3)
	{
		o = stsdb_get_sds(db, code, sts_module_string_get(argv_[2], NULL));
	}
	else
	{
		o = stsdb_get_sds(db, code, "{\"format\":\"json\"}");
	}
	if (o)
	{
		sts_module_reply_with_simple_string(ctx_, o);
		sts_sdsfree(o);
		return STS_MODULE_OK;
	}
	return sts_module_reply_with_error(ctx_, "stsdb get error.\n");
}
int call_stsdb_set(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ != 4)
	{
		return sts_module_wrong_arity(ctx_);
	}
	// printf("%s: %.90s\n", sts_module_string_get(argv_[1], NULL), sts_module_string_get(argv_[3], NULL));

	const char *key = sts_module_string_get(argv_[1], NULL);
	int count = sts_str_substr_nums(key, '.');
	if (count != 2)
	{
		return sts_module_reply_with_error(ctx_, "set data key error.\n");
	}
	char db[STS_TABLE_MAXLEN];
	char code[STS_CODE_MAXLEN];
	sts_str_substr(db, STS_TABLE_MAXLEN, key, '.', 1);
	sts_str_substr(code, STS_CODE_MAXLEN, key, '.', 0);

	int o;
	size_t len;
	const char *val = sts_module_string_get(argv_[3], &len);
	const char *dt = sts_module_string_get(argv_[2], NULL);

	o = stsdb_set(dt, db, code, val, len);

	if (!o)
	{
		return sts_module_reply_with_simple_string(ctx_, "OK");
	}
	return sts_module_reply_with_error(ctx_, "stsdb set error.\n");
}

char *call_stsdb_open(const char *conf_)
{
	if (!sts_file_exists(conf_))
	{
		sts_out_error(3)("conf file %s no finded.\n", conf_);
		return NULL;
	}
	return stsdb_open(conf_);
}
int call_stsdb_init(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ != 2)
	{
		return sts_module_wrong_arity(ctx_);
	}
	
	const char *market = sts_module_string_get(argv_[1], NULL);
	
	stsdb_init(market);
	
	return sts_module_reply_with_simple_string(ctx_, "OK");
}

int call_stsdb_close(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	sts_module_not_used(argc_);
	sts_module_not_used(argv_);
	stsdb_close();
	return sts_module_reply_with_simple_string(ctx_, "OK");
}

int sts_module_on_load(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	// 先取得服务名
	char *service;
	if (argc_ == 1)
	{
		// service = call_stsdb_open(sts_module_string_get(argv_[0], NULL));
		service = call_stsdb_open(((s_sts_object *)argv_[0])->ptr);
	}
	else
	{
		service = call_stsdb_open("../stsdb/conf/stsdb.conf");
	}
	if (!service || !*service)
	{
		sts_out_error(3)("init stsdb error.\n");
		return STS_MODULE_ERROR;
	}
	char servicename[64];
	sts_sprintf(servicename, 64, "%s", service);
	if (sts_module_init(ctx_, servicename, 1, STS_MODULE_VER) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	/* Log the list of parameters passing loading the module. */
	// for (int k = 0; k < argc_; k++)
	// {
	// 	const char *s = sts_module_string_get(argv_[k], NULL);
	// 	printf("module loaded with argv_[%d] = %s\n", k, s);
	// }

	sts_sprintf(servicename, 64, "%s.init", service);
	if (sts_module_create_command(ctx_, servicename, call_stsdb_init,
								  "write deny-oom",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	sts_sprintf(servicename, 64, "%s.close", service);
	if (sts_module_create_command(ctx_, servicename, call_stsdb_close,
								  "readonly",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	sts_sprintf(servicename, 64, "%s.list", service);
	if (sts_module_create_command(ctx_, servicename, call_stsdb_list,
								  "readonly",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	sts_sprintf(servicename, 64, "%s.get", service);
	if (sts_module_create_command(ctx_, servicename, call_stsdb_get,
								  "readonly",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	sts_sprintf(servicename, 64, "%s.set", service);
	if (sts_module_create_command(ctx_, servicename, call_stsdb_set,
								  "write deny-oom",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	sts_sprintf(servicename, 64, "%s.save", service);
	if (sts_module_create_command(ctx_, servicename, call_stsdb_save,
								  "write deny-oom",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	return STS_MODULE_OK;
}
