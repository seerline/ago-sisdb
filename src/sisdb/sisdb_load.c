
#include <sis_comm.h>
#include <sisdb_io.h>
#include "sis_net.obj.h"

#define CHECK_SERVER_STATUS                                                 \
	{                                                                       \
		if (sisdb_get_server_status() != SIS_SERVER_STATUS_INITED)          \
			return sis_module_reply_with_error(ctx_, "server starting..."); \
	}


s_sis_object *siscs_send_local_obj(const char *command_, const char *key_, 
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

	s_sis_object *o = NULL;
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
					s_sis_sds val = sis_sdsnewlen(str, len);
					o = sis_object_create(SIS_OBJECT_SDS, val);
					sis_object_incr(o);
				}
				break;
			default:
				break;
		}
		sis_module_call_reply_free(reply);
	}
	sis_module_free_safe_context(ctx);
	sis_object_decr(o);
	return o;
}

// 入口初始化，根据配置文件获取初始化所有信息，并返回s_sisdb_server指针，
// 一个实例只有一个server，但是可以有多个
s_sis_pointer_list *load_sisdb_open(const char *conf_)
{
	if (!sis_file_exists(conf_))
	{
		printf("conf file %s no finded.\n", conf_);
		return NULL;
	}
	return sisdb_open(conf_);
}

// 设置系统级别的一些默认操作，通过用于一些危险操作的授权，例如：
// output   查询结果写入磁盘，
// super  允许删除表格，系统的del只能清除表格内容，不能删除表结构
int load_sisdb_switch(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	CHECK_SERVER_STATUS;
	if (argc_ < 2)
	{
		return sis_module_wrong_arity(ctx_);
	}

	int o = sisdb_set_switch(sis_module_string_get(argv_[1], NULL));

	if (o >= 0)
	{
		return sis_module_reply_with_int64(ctx_, o);
	}
	return sis_module_reply_with_null(ctx_);
}

int load_sisdb_close(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	sis_module_not_used(argc_);
	sis_module_not_used(argv_);
	sisdb_close();
	return sis_module_reply_with_simple_string(ctx_, "OK");
}

int load_sisdb_show(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	const char *key = NULL;
	const char *com = NULL;

	if (argc_ >= 2)
	{
		key = sis_module_string_get(argv_[1], NULL);
		if (argc_ >= 3)
		{
			com = sis_module_string_get(argv_[2], NULL);
		}
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

int load_sisdb_save(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	CHECK_SERVER_STATUS;
	const char *dbname = NULL;

	if (argc_ >= 2)
	{
		dbname = sis_module_string_get(argv_[1], NULL);
	}
	if (sisdb_save(dbname))
	{
		return sis_module_reply_with_simple_string(ctx_, "OK");
	}
	return sis_module_reply_with_null(ctx_);
}
// sisdb.call [procname] [command]
// 提供更高级的查询语句，仅仅返回特定格式数据, 
// 和get的区别是，get在本数据集合中获取数据，call不限于数据集，进行统计和运算，并返回结果，
// 支持其他脚本语言的动态加载，放到指定目录，文件名即为函数名，
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
		// 这里要判断返回的数据是否为字符串
		// sis_out_binary("call out", o, 30);
		// printf("call out ...%lu\n", sis_sdslen(o));
		if (!strcasecmp(key, "list"))
		{
			// 字符串返回
			sis_module_reply_with_simple_string(ctx_, o);
		}
		else
		{
			// 二进制返回
			sis_module_reply_with_buffer(ctx_, o, sis_sdslen(o));
		}
		sis_sdsfree(o);
		return SIS_MODULE_OK;
	}
	return sis_module_reply_with_null(ctx_);
}
//////////////////////////////////////////////////////////////////////////////
// 下面是单独某个db使用的命令集合
//////////////////////////////////////////////////////////////////////////////
s_sis_db *_load_sisdb_getdb(s_sis_module_context *ctx_, const char *key_)
{
	s_sis_db *db = (s_sis_db *)sis_module_get_userdata(ctx_);
	if (db)
	{	
		return db;
	}	
	// 表示需要自己去找 table 的归属db 速度会稍慢，默认找到
	char tbname[SIS_MAXLEN_TABLE];
	sis_str_substr(tbname, SIS_MAXLEN_TABLE, key_, '.', 1);
	s_sisdb_server *server = sisdb_get_server();
	for(int i = 0; i < server->sisdbs->count; i++)
	{
		db = sis_pointer_list_get(server->sisdbs, i);
		s_sisdb_table *tb = sis_map_pointer_get(db->dbs, tbname);
		if (tb)
		{
			// 如果找到名字一样的数据表，按顺序直接返回，要返回后面的同名数据表，就必须写前缀
			return db;
		}
	}
	return NULL;
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
	s_sis_sds o = NULL;
	const char *key = sis_module_string_get(argv_[1], NULL);
	s_sis_db *db = _load_sisdb_getdb(ctx_, key);
	if (!db)
	{	
		return sis_module_reply_with_null(ctx_);
	}
	if (argc_ == 3)
	{
		o = sisdb_get_sds(db, key, sis_module_string_get(argv_[2], NULL));
	}
	else
	{
		// fast不写盘不判断，专用于快速返回给用户数据
		o = sisdb_fast_get_sds(db, key);
	}
	if (o)
	{
		sis_module_reply_with_buffer(ctx_, o, sis_sdslen(o));
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
	s_sis_db *db = _load_sisdb_getdb(ctx_, key);
	if (!db)
	{	
		return sis_module_reply_with_null(ctx_);
	}
	if (argc_ == 3)
	{
		size_t len = 0;
		const char *com = sis_module_string_get(argv_[2], &len);
		o = sisdb_delete_link(db, key, com, len);
	}
	else
	{
		o = sisdb_delete_link(db, key, NULL, 0);
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
	s_sis_db *db = _load_sisdb_getdb(ctx_, key);
	if (!db)
	{	
		return sis_module_reply_with_null(ctx_);
	}
	// printf("set %s\n", db->name);
	if (!val || len < 1)
	{
		return sis_module_reply_with_null(ctx_);
	}
	int o = sisdb_set_json(db, key, val, len);

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
	s_sis_db *db = _load_sisdb_getdb(ctx_, key);
	if (!db)
	{	
		return sis_module_reply_with_null(ctx_);
	}
	if (!val || len < 1)
	{
		return sis_module_reply_with_null(ctx_);
	}
	int o = sisdb_set_struct(db, key, val, len);

	if (!o)
	{
		return sis_module_reply_with_simple_string(ctx_, "OK");
	}
	return sis_module_reply_with_null(ctx_);
}

// int load_sisdb_new(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
// {
// 	CHECK_SERVER_STATUS;
// 	if (argc_ != 3)
// 	{
// 		return sis_module_wrong_arity(ctx_);
// 	}
// 	// printf("%s: %.90s\n", sis_module_string_get(argv_[1], NULL), sis_module_string_get(argv_[3], NULL));
// 	s_sis_db *db = (s_sis_db *)sis_module_get_userdata(ctx_);
// 	if (!db)
// 	{
// 		return sis_module_reply_with_null(ctx_);
// 	}
// 	size_t len;
// 	const char *key = sis_module_string_get(argv_[1], NULL);
// 	const char *val = sis_module_string_get(argv_[2], &len);

// 	if (!val || len < 1)
// 	{
// 		return sis_module_reply_with_null(ctx_);
// 	}
// 	int o = sisdb_new(db, key, val, len);
// 	if (!o)
// 	{
// 		return sis_module_reply_with_simple_string(ctx_, "OK");
// 	}
// 	return sis_module_reply_with_null(ctx_);
// }

// int load_sisdb_update(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
// {
// 	CHECK_SERVER_STATUS;
// 	if (argc_ != 3)
// 	{
// 		return sis_module_wrong_arity(ctx_);
// 	}
// 	// printf("%s: %.90s\n", sis_module_string_get(argv_[1], NULL), sis_module_string_get(argv_[3], NULL));
// 	s_sis_db *db = (s_sis_db *)sis_module_get_userdata(ctx_);
// 	if (!db)
// 	{
// 		return sis_module_reply_with_null(ctx_);
// 	}
// 	size_t len;
// 	const char *key = sis_module_string_get(argv_[1], NULL);
// 	const char *val = sis_module_string_get(argv_[2], &len);
// 	if (!val || len < 1)
// 	{
// 		return sis_module_reply_with_null(ctx_);
// 	}
// 	int o = sisdb_update(db, key, val, len);

// 	if (!o)
// 	{
// 		return sis_module_reply_with_simple_string(ctx_, "OK");
// 	}
// 	return sis_module_reply_with_null(ctx_);
// }

int sis_module_on_unload()
{
	sisdb_close();
	printf("close sisdb.\n");
	safe_memory_stop();
	return SIS_MODULE_OK;
}
int sis_module_on_load(s_sis_module_context *ctx_, s_sis_module_string **argv_, int argc_)
{
	safe_memory_start();
	// 先取得服务名
	s_sis_pointer_list *sisdbs;
	if (argc_ == 1)
	{
		sisdbs = load_sisdb_open(((s_sis_object_r *)argv_[0])->ptr);
		// 为什么不用 sis_module_string_get(argv_[0], NULL);
	}
	else
	{
		sisdbs = load_sisdb_open("./sisdb.conf");
	}
	if (!sisdbs || sisdbs->count < 1)
	{
		printf("init sisdb error.\n");
		return SIS_MODULE_ERROR;
	}
	const char *service = "sisdb";
	if (sis_module_init(ctx_, service, 1, SIS_MODULE_VER) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}

	/* Log the list of parameters passing loading the module. */
	// for (int k = 0; k < argc_; k++)
	// {
	// 	const char *s = sis_module_string_get(argv_[k], NULL);
	// 	printf("module loaded with argv_[%d] = %s\n", k, s);
	// }

	char command[SIS_MAXLEN_COMMAND];
	for (int i = 0; i < sisdbs->count; i++)
	{
		s_sis_db *db = (s_sis_db *)sis_pointer_list_get(sisdbs, i);
		sis_module_set_userdata(ctx_, db);

		sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.get", db->name);
		if (sis_module_create_command(ctx_, command, load_sisdb_get,
									  "readonly",
									  0, 0, 0) == SIS_MODULE_ERROR)
		{
			return SIS_MODULE_ERROR;
		}
		sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.set", db->name);
		if (sis_module_create_command(ctx_, command, load_sisdb_set,
									  "write deny-oom",
									  0, 0, 0) == SIS_MODULE_ERROR)
		{
			return SIS_MODULE_ERROR;
		}
		sis_sprintf(command, SIS_MAXLEN_KEY, "%s.sset", db->name);
		if (sis_module_create_command(ctx_, command, load_sisdb_sset,
									  "write deny-oom",
									  0, 0, 0) == SIS_MODULE_ERROR)
		{
			return SIS_MODULE_ERROR;
		}
		sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.del", db->name);
		if (sis_module_create_command(ctx_, command, load_sisdb_del,
									  "write deny-oom",
									  0, 0, 0) == SIS_MODULE_ERROR)
		{
			return SIS_MODULE_ERROR;
		}
		// 创建数据表，后面跟各种属性和字段定义
		// sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.new", db->name);
		// if (sis_module_create_command(ctx_, command, load_sisdb_new,
		// 							  "write deny-oom",
		// 							  0, 0, 0) == SIS_MODULE_ERROR)
		// {
		// 	return SIS_MODULE_ERROR;
		// }
		// sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.save", db->name);
		// if (sis_module_create_command(ctx_, command, load_sisdb_save,
		// 								"write deny-oom",
		// 								0, 0, 0) == SIS_MODULE_ERROR)
		// {
		// 	return SIS_MODULE_ERROR;
		// }
	}
	// 空表示
	sis_module_set_userdata(ctx_, NULL);
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.get", service);
	if (sis_module_create_command(ctx_, command, load_sisdb_get,
									"readonly",
									0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.set", service);
	if (sis_module_create_command(ctx_, command, load_sisdb_set,
									"write deny-oom",
									0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(command, SIS_MAXLEN_KEY, "%s.sset", service);
	if (sis_module_create_command(ctx_, command, load_sisdb_sset,
									"write deny-oom",
									0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.del", service);
	if (sis_module_create_command(ctx_, command, load_sisdb_del,
									"write deny-oom",
									0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	// 权限管理，不影响实际数据库数据
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.switch", service);
	if (sis_module_create_command(ctx_, command, load_sisdb_switch,
								  "readonly",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.close", service);
	if (sis_module_create_command(ctx_, command, load_sisdb_close,
								  "readonly",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.show", service);
	if (sis_module_create_command(ctx_, command, load_sisdb_show,
								  "readonly",
								  0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.save", service);
	if (sis_module_create_command(ctx_, command, load_sisdb_save,
									"write deny-oom",
									0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	// call 专门用于不同数据表之间关联计算的用途，留出其他语言加载的接口
	sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.call", service);
	if (sis_module_create_command(ctx_, command, load_sisdb_call,
									"write deny-oom",
									0, 0, 0) == SIS_MODULE_ERROR)
	{
		return SIS_MODULE_ERROR;
	}
	// 修改数据表，可以是字段，可以是其他属性，只在设置字段时会锁住进程，处理完后再恢复访问
	// 情况太复杂，牵涉到scale修改导致的time字段修改和数据是否合并等功能
	// 可能只支持字段增加和删除，不能支持字段修改，
	// sis_sprintf(command, SIS_MAXLEN_COMMAND, "%s.update", db->name);
	// if (sis_module_create_command(ctx_, command, load_sisdb_update,
	// 							  "write deny-oom",
	// 							  0, 0, 0) == SIS_MODULE_ERROR)
	// {
	// 	return SIS_MODULE_ERROR;
	// }

	return SIS_MODULE_OK;
}


