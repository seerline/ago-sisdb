
#include <sts_db_io.h>


int call_stsdb_init(const char *conf_)
{
	if (!sts_file_exists(conf_))
	{
		sts_out_error(3)("conf file %s no finded.\n", conf_);
		return STS_MODULE_ERROR;
	}
	int o = stsdb_init(conf_);
	if (o == STS_MODULE_OK) {
		return STS_MODULE_OK;
	} 
	sts_out_error(3)("init stsdb error.\n");
	return STS_MODULE_ERROR;
}
int call_stsdb_start(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	sts_module_not_used(argc_);
	sts_module_not_used(argv_);

	int o = stsdb_start(ctx_);
	if (!o) {
		sts_module_reply_with_simple_string(ctx_, "OK");
		return STS_MODULE_OK;
	} 
	return sts_module_reply_with_error(ctx_, "start stsdb error.\n");
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

	int o;
	const char *key = sts_module_string_get(argv_[1], NULL);
	char db[32];
	char code[16];
	sts_str_substr(db, 32, key, '.', 1);
	sts_str_substr(code, 16, key, '.', 0);

	if (argc_ == 3)
	{
		o = stsdb_get(ctx_, db, code, sts_module_string_get(argv_[2], NULL));
	}
	else
	{
		o = stsdb_get(ctx_, db, code, "{\"format\":\"json\"}");
	}
	return o;
}
int call_stsdb_set(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ != 4)
	{
		return sts_module_wrong_arity(ctx_);
	}
	// printf("%s: %.90s\n", sts_module_string_get(argv_[1], NULL), sts_module_string_get(argv_[2], NULL));
	// sts_module_reply_with_simple_string(ctx_, "OK");
	const char * dt = sts_module_string_get(argv_[2],NULL);
 	if (sts_str_subcmp(dt ,"struct,json",',') < 0){
		return sts_module_reply_with_error(ctx_, "set data type error.\n");
	}
	int o;
	const char *key = sts_module_string_get(argv_[1], NULL);
	char db[32];
	char code[16];
	sts_str_substr(db, 32, key, '.', 1);
	sts_str_substr(code, 16, key, '.', 0);

	if (sts_strcasecmp(dt,"struct")) {
		o = stsdb_set_struct(ctx_, db, code, sts_module_string_get(argv_[3], NULL));
	} else {
		o = stsdb_set_json(ctx_, db, code, sts_module_string_get(argv_[3], NULL));
	}

	return o;
}

int sts_module_on_load(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (sts_module_init(ctx_, "stsdb", 1, STS_MODULE_VER) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	/* Log the list of parameters passing loading the module. */
	// for (int k = 0; k < argc_; k++)
	// {
	// 	const char *s = sts_module_string_get(argv_[k], NULL);
	// 	printf("module loaded with argv_[%d] = %s\n", k, s);
	// }
	int o;
	if (argc_ == 1)
	{
		o = call_stsdb_init(sts_module_string_get(argv_[0], NULL));
	}
	else
	{
		o = call_stsdb_init("../conf/stsdb.conf");
	}
	if (o != STS_MODULE_OK)
	{
		return STS_MODULE_ERROR;
	}

	if (sts_module_create_command(ctx_, "stsdb.start", call_stsdb_start,
								  "readonly",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	if (sts_module_create_command(ctx_, "stsdb.get", call_stsdb_get,
								  "readonly",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	if (sts_module_create_command(ctx_, "stsdb.set", call_stsdb_set,
								  "write deny-oom",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}

	return STS_MODULE_OK;
}

