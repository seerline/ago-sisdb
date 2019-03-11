
#include <siscs_load.h>

s_sis_sds siscs_send_local_sds(const char *command_, const char *key_, 
					     	   const char *val_, size_t ilen_)
{
	s_sis_module_context *ctx = sis_module_get_safe_context(NULL);
	if (!ctx)
	{
		return 0;
	}
	s_sis_module_call_reply *reply;
	if (!val_||!ilen_)
	{
		reply = sis_module_call(ctx, command_, "c!", key_);
	}
	else
	{
		reply = sis_module_call(ctx, command_, "cb!", key_, val_, ilen_);
	}

	s_sis_sds o = NULL;
	if (reply)
	{
		int style = sis_module_call_reply_type(reply);
		switch (style)
		{
			case SIS_MODULE_REPLY_STRING:
				{
					size_t len;
					const char *str = sis_module_call_reply_with_string(reply, &len);
					// printf("len=%lu\n", len);
					o = sis_sdsnewlen(str, len);
				}
				break;
			default:
				break;
		}
		sis_module_call_reply_free(reply);
	}
	sis_module_free_safe_context(ctx);
	return o;
}
////////////////////////////
//
///////////////////////////

int call_siscs_open(const char *conf_)
{
	if (!sis_file_exists(conf_))
	{
		sis_out_log(3)("conf file %s no finded.\n", conf_);
		return SIS_MODULE_ERROR;
	}
	return siscs_open(conf_);
}

int sis_module_on_unload()
{
	siscs_close();
	printf("--------------close-----------\n");
	safe_memory_stop();
	return SIS_MODULE_OK;
}

int sis_module_on_load(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	safe_memory_start();
	// 先取得服务名
	int cs = 0;
	if (argc_ == 1)
	{
		cs = call_siscs_open(((s_sis_object *)argv_[0])->ptr);
	}
	else
	{
		cs = call_siscs_open("../bin/siscs.conf");
	}
	if (cs != 0)
	{
		sis_out_log(3)("init siscs fail. [%d]\n", cs);
		return SIS_MODULE_ERROR;
	}

	if (sis_module_init(ctx_, "siscs", 1, SIS_MODULE_VER) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	// 不注册任何命令，这里仅仅是从自己的端口获取数据，然后把请求转发到server处理中心，

	/* Log the list of parameters passing loading the module. */
	// for (int k = 0; k < argc_; k++)
	// {
	// 	const char *s = sis_module_string_get(argv_[k], NULL);
	// 	printf("module loaded with argv_[%d] = %s\n", k, s);
	// }
	return SIS_MODULE_OK;
}
