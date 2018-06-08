
#include <digger_io.h>
#include <os_thread.h>
#include <sts_comm.h>


int call_digger_start(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ != 3)
	{
		return sts_module_wrong_arity(ctx_);
	}
	s_sts_sds o = digger_start_sds(sts_module_string_get(argv_[1], NULL),  // name : me
						 sts_module_string_get(argv_[2], NULL)); // command: json
	if (o)
	{
		sts_module_reply_with_simple_string(ctx_, o);
		sts_sdsfree(o);
		return STS_MODULE_OK;
	}
	return sts_module_reply_with_error(ctx_, "digger start error.\n");
}

int call_digger_stop(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ != 2)
	{
		return sts_module_wrong_arity(ctx_);
	}
	const char *key = sts_module_string_get(argv_[1], NULL);
	
	int o = digger_stop(key); // id : me000012

	if (o >= 0) {
		sts_module_reply_with_simple_string(ctx_, "OK");
		return STS_MODULE_OK;
	}
	return sts_module_reply_with_error(ctx_, "no find id.\n");
}

int call_digger_get(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ != 3)
	{
		return sts_module_wrong_arity(ctx_);
	}
	// s_sts_sds o = digger_get_sds(sts_module_string_get(argv_[1], NULL), 
	// 							  sts_module_string_get(argv_[2], NULL));
	// if (o)
	// {
	// 	sts_module_reply_with_simple_string(ctx_, o);
	// 	sts_sdsfree(o);
	// 	return STS_MODULE_OK;
	// }
	return sts_module_reply_with_error(ctx_, "get error.\n");
}

int call_digger_set(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ != 3)
	{
		return sts_module_wrong_arity(ctx_);
	}
	// int o = digger_set("order",
	// 				   sts_module_string_get(argv_[1], NULL),  // id : me000012
	// 				   sts_module_string_get(argv_[2], NULL)); // command: json
	// return o;
	sts_module_reply_with_simple_string(ctx_, "OK");
	return STS_MODULE_OK;
}

int _digger_block_sub_reply(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
    sts_module_not_used(argv_);
    sts_module_not_used(argc_);
    s_sts_module_string *s = sts_module_block_getdata(ctx_);
    sts_module_reply_with_string(ctx_, s);
    // sts_module_string_free(ctx_, s);
    return STS_MODULE_OK;
}

int _digger_block_sub_timeout(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
    sts_module_not_used(argv_);
    sts_module_not_used(argc_);
    return sts_module_reply_with_simple_string(ctx_, "sub timedout\n");
}
void _digger_block_sub_free(s_sts_module_context *ctx_, void *buffer_)
{
	REDISMODULE_NOT_USED(ctx_);
    sts_module_free(buffer_);
}

void *_digger_block_sub_thread(void *argv_)
{
    void **argv = argv_;
    s_sts_module_block_client *bc = argv[0];
    s_sts_module_string *code = argv[1];
    // sts_module_free(argv);

    s_sts_module_context *ctx = sts_module_get_safe_context(bc);

    while (1)
    {
        s_sts_module_key *key = sts_module_key_open(ctx, code,
                                                    STS_MODULE_READ | STS_MODULE_WRITE);
        s_sts_module_string *e = sts_module_list_pop(key, STS_MODULE_LIST_HEAD);
        if (e)
        {
            // sts_module_reply_with_string(ctx, e);
            // sts_module_string_free(ctx, e);
            sts_module_key_close(key);
            sts_module_free_safe_context(ctx);
            sts_module_free(argv);
            sts_module_unblock_client(bc, e);
            break;
        }
        else
        {
            sts_module_key_close(key);
            sts_sleep(1000);
        }
    }

    return NULL;
}

int call_digger_sub(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ != 2)
	{
		return sts_module_wrong_arity(ctx_);
	}
	const char *db_ = "order";
	const char *key_ = sts_module_string_get(argv_[1], NULL);

    printf("%s.%s\n", db_, key_);

    s_sts_thread_id_t pid;
    s_sts_module_block_client *bc = sts_module_block_client(
        ctx_,
        _digger_block_sub_reply,
        _digger_block_sub_timeout,
        _digger_block_sub_free,
        0);
    void **argv = sts_module_alloc(sizeof(void *) * 2);
    argv[0] = bc;
    argv[1] = sts_module_string_create_printf(ctx_, "%s.%s", db_, key_);

	if (!sts_thread_create(_digger_block_sub_thread, argv, &pid))
    {
        sts_module_block_abort(bc);
        return sts_module_reply_with_error(ctx_, "can't start thread\n");
    }
    return 0;
}
int call_digger_pub(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ != 3)
	{
		return sts_module_wrong_arity(ctx_);
	}
	const char *db_  = "order";
	const char *key_ = sts_module_string_get(argv_[1], NULL); // id : me000012
	const char *com_ = sts_module_string_get(argv_[2], NULL); // command: json

    printf("%s.%s\n", db_, key_);

    s_sts_module_string *code = sts_module_string_create_printf(ctx_, "%s.%s", db_, key_);
    s_sts_module_key *key = sts_module_key_open(ctx_, code,
                                                STS_MODULE_READ | STS_MODULE_WRITE);

    s_sts_module_string *info = sts_module_string_create(ctx_, com_, strlen(com_));
    sts_module_list_push(key, STS_MODULE_LIST_TAIL, info);
    size_t newlen = sts_module_value_len(key);
    sts_module_key_close(key);
    sts_module_reply_with_int64(ctx_, newlen);

    return STS_MODULE_OK;
}

// 函数名 call getnow {"codes":"sh600600"}
//       返回格式为array， {"sh600600":33.45,"sh600601":44.55} 
// 函数名 call getright {"codes":"sh600600", "argv":[{"time":20180101,"close":33.44,"vol":1000}]}
int call_digger_call(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ < 3)
	{
		return sts_module_wrong_arity(ctx_);
	}

	s_sts_sds o = digger_call_sds(ctx_,
								  sts_module_string_get(argv_[1], NULL), 
								  sts_module_string_get(argv_[2], NULL));
	if (o)
	{
		sts_module_reply_with_simple_string(ctx_, o);
		sts_sdsfree(o);
		return STS_MODULE_OK;
	}
	return sts_module_reply_with_error(ctx_, "digger call error.\n");
}
s_sts_sds get_stsdb_info_sds(void *server_, const char *key_, const char *com_)
{
	s_digger_server *server = (s_digger_server *)server_;
	// printf("key = %s  %p\n",key_, server->context);
	s_sts_module_context *cxt = (s_sts_module_context *)(server->context);
	if (!cxt) {
		return NULL;
	}
    s_sts_module_call_reply *reply;
	char command[64];
	sts_snprintf(command, 64, "%s.get", server->source_name);

	// printf("%s\n",command);
	if(!com_) {
		reply = sts_module_call(cxt,command,"c!", key_);
	} else {
		reply = sts_module_call(cxt,command,"cc!", key_, com_);
	}
	if(reply){
		if(sts_module_call_reply_type(reply) == STS_MODULE_REPLY_STRING) {
			size_t len;
			const char *str=sts_module_call_reply_with_string(reply, &len);
			return sts_sdsnewlen(str, len);
		} else {
			sts_out_error(3)("get_stsdb_info reply type error.\n");
		}
	}
	return NULL;
}
int call_digger_source(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
    s_sts_module_call_reply *reply;

	reply = sts_module_call(ctx_,"stsdb.list","!");
	if(reply){
		sts_module_reply_with_reply(ctx_,reply);
	    sts_module_call_reply_free(reply);
		return STS_MODULE_OK;
	}
	return sts_module_reply_with_error(ctx_, "digger source error.\n");
}
char *call_digger_open(const char *conf_)
{
	if (!sts_file_exists(conf_))
	{
		sts_out_error(3)("conf file %s no finded.\n", conf_);
		return NULL;
	}
	return digger_open(conf_);
}
int call_digger_close(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	sts_module_not_used(argc_);
	sts_module_not_used(argv_);
	digger_close();
	return sts_module_reply_with_simple_string(ctx_, "OK");
}
int sts_module_on_unload()
{
	printf("--------------close-----------\n");
	digger_close();
	safe_memory_stop();
	return STS_MODULE_OK;
}

int sts_module_on_load(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	safe_memory_start();
	// 先取得服务名
	char *service;
	if (argc_ == 1)
	{
		// service = call_digger_open(sts_module_string_get(argv_[0], NULL));
		service = call_digger_open(((s_sts_object *)argv_[0])->ptr);
	}
	else
	{
		service = call_digger_open("../stsdb/conf/digger.conf");
	}
	if (!service || !*service)
	{
		sts_out_error(3)("init digger error.\n");
		return STS_MODULE_ERROR;
	}
	char servicename[64];
	sts_sprintf(servicename, 64, "%s", service);
	if (sts_module_init(ctx_, servicename, 1, STS_MODULE_VER) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}	

	/* Log the list of parameters passing loading the module. */
	// for (int k = 0; k < argc_; k++)
	// {
	// 	const char *s = sts_module_string_get(argv_[k], NULL);
	// 	printf("module loaded with argv_[%d] = %s\n", k, s);
	// }
	sts_sprintf(servicename, 64, "%s.close", service);
	if (sts_module_create_command(ctx_, servicename, call_digger_close,
								  "readonly",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}

	sts_sprintf(servicename, 64, "%s.start", service);
	if (sts_module_create_command(ctx_, servicename, call_digger_start,
								  "write deny-oom",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	sts_sprintf(servicename, 64, "%s.stop", service);
	if (sts_module_create_command(ctx_, servicename, call_digger_stop,
								  "write deny-oom",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	sts_sprintf(servicename, 64, "%s.get", service);
	if (sts_module_create_command(ctx_, servicename, call_digger_get,
								  "readonly",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	sts_sprintf(servicename, 64, "%s.set", service);
	if (sts_module_create_command(ctx_, servicename, call_digger_set,
								  "write deny-oom", // deny-oom
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	sts_sprintf(servicename, 64, "%s.sub", service);
	if (sts_module_create_command(ctx_, servicename, call_digger_sub,
								  "readonly", // deny-oom
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	sts_sprintf(servicename, 64, "%s.pub", service);
	if (sts_module_create_command(ctx_, servicename, call_digger_pub,
								  "write deny-oom", //
								  1, 1, 1) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	sts_sprintf(servicename, 64, "%s.call", service);
	if (sts_module_create_command(ctx_, servicename, call_digger_call,
								  "readonly",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	sts_sprintf(servicename, 64, "%s.source", service);
	if (sts_module_create_command(ctx_, servicename, call_digger_source,
								  "readonly",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	return STS_MODULE_OK;
}

