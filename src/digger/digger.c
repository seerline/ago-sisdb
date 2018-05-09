
#include <call.h>
#include <stdbool.h>


bool _inited = false;
char _source[255];

int call_sts_db_init(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	int o = sts_db_create(ctx_, _source);
	if (o <= 0)
	{
		printf("create db [%s] fail. code = [%d]\n", _source, o);
	}
	else
	{
		printf("create db ok. count = [%d]\n", o);
	}
	_inited = true;
    sts_module_reply_with_simple_string(ctx_,"OK");
	return o;	
}
// _module 开头的函数仅仅在该文件应用，该文件仅仅处理逻辑问题，具体函数实现在call文件中
int call_sts_db_get(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (!_inited) {
		call_sts_db_init(ctx_, NULL, 0);	
	}
	// sts_module_not_used(argv_);
	// sts_module_not_used(argc_);
	if (argc_ < 3)
	{
		return sts_module_wrong_arity(ctx_);
	}

	int out;
	if (argc_ == 4)
	{
		out = sts_db_get(ctx_,
						 sts_module_string_get(argv_[1], NULL),  // dbname : day
						 sts_module_string_get(argv_[2], NULL),  // key : sh600600
						 sts_module_string_get(argv_[3], NULL)); // command: json
	}
	else
	{
		out = sts_db_get(ctx_,
						 sts_module_string_get(argv_[1], NULL), // dbname : day
						 sts_module_string_get(argv_[2], NULL), // key : sh600600
						 NULL);
	}
	return out;
}
int sts_module_on_load(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (sts_module_init(ctx_, "stsdb", 1, STS_MODULE_VER) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	/* Log the list of parameters passing loading the module. */
	for (int k = 0; k < argc_; k++)
	{
		const char *s = sts_module_string_get(argv_[k], NULL);
		printf("module loaded with argv_[%d] = %s\n", k, s);
	}
	if (argc_ == 1)
	{
		sprintf(_source,"%s", sts_module_string_get(argv_[0], NULL));
	}
	else
	{
		_source[0] = 0;
	}

	example();

	if (sts_module_create_command(ctx_, "stsdb.init",call_sts_db_init, 
		"write",
		0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	if (sts_module_create_command(ctx_, "stsdb.get",call_sts_db_get, 
		"readonly",
		0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}

	return STS_MODULE_OK;
}
