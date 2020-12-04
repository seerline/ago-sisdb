
#include "sis_modules.h"
#include "worker.h"
#include "sis_method.h"

#include <zipdb.h>

//////////////////////////////////////////////////////////////////
//------------------------wlog function -----------------------//
//////////////////////////////////////////////////////////////////
bool _zipdb_write_init(s_zipdb_cxt *zipdb_, int workdate_, s_sis_sds keys_, s_sis_sds sdbs_);
int _zipdb_write_bits(s_zipdb_cxt *zipdb_, s_zipdb_bits *in_);

// 从wlog文件中加载数据
static int cb_zipdb_wlog_start(void *worker_, void *argv_)
{
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker_;
	const char *sdate = (const char *)argv_;
	LOG(5)("load wlog start. %s\n", sdate);
	context->wlog_date = sis_atoll(sdate);
	return SIS_METHOD_OK;
}
static int cb_zipdb_wlog_stop(void *worker_, void *argv_)
{
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker_;
	const char *sdate = (const char *)argv_;
	context->wlog_load = 0;
	LOG(5)("load wlog stop. %s\n", sdate);
	return SIS_METHOD_OK;
}
static int cb_zipdb_wlog_load(void *worker_, void *argv_)
{
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker_;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	if (context->wlog_load != 1)
	{
		return SIS_METHOD_ERROR;
	}
    // printf("cb_zipdb_wlog_load: %d %s \n%s \n%s \n%s \n", netmsg->style,
    //         netmsg->source? netmsg->source : "nil",
    //         netmsg->cmd ?   netmsg->cmd : "nil",
    //         netmsg->key?    netmsg->key : "nil",
    //         netmsg->val?    netmsg->val : "nil");   

	if (!sis_strcasecmp("zdb.zpub", netmsg->cmd))
	{
		for (int i = 0; i < netmsg->argvs->count; i++)
		{
			s_sis_object *obj = sis_pointer_list_get(netmsg->argvs, i);
			_zipdb_write_bits(context, (s_zipdb_bits *)SIS_OBJ_SDS(obj));
		}
	}
	else // "zdb.set"
	{
		if (!sis_strcasecmp("_keys_", netmsg->key))
		{
			sis_sdsfree(context->wlog_keys);
			context->wlog_keys = sis_sdsdup(netmsg->val);
		}
		if (!sis_strcasecmp("_sdbs_", netmsg->key))
		{
			sis_sdsfree(context->wlog_sdbs);
			context->wlog_sdbs = sis_sdsdup(netmsg->val);			
		}
		if (context->wlog_keys && context->wlog_sdbs)
		{
			if(!_zipdb_write_init(context, context->wlog_date, context->wlog_keys, context->wlog_sdbs))
			{
				// 数据错误 应该停止读取 直接返回错误
				context->wlog_load = 0;
			}	
		}
		sis_sdsfree(context->wlog_keys); context->wlog_keys = NULL;
		sis_sdsfree(context->wlog_sdbs); context->wlog_sdbs = NULL;
	}
    return SIS_METHOD_OK;
}

// 从磁盘中获取数据 0 没有wlog文件 ** 文件有错误直接删除 并返回0
//               1 有文件 正在读 需要设置状态 并等待数据读完
int zipdb_wlog_load(s_zipdb_cxt *zipdb_)
{
	s_sis_message *msg = sis_message_create();

	zipdb_->wlog_load = 1;
	sis_message_set_str(msg, "dbname", "zdb", 3);
	sis_message_set(msg, "source", zipdb_, NULL);
	sis_message_set_method(msg, "cb_sub_start", cb_zipdb_wlog_start);
	sis_message_set_method(msg, "cb_sub_stop", cb_zipdb_wlog_stop);
	sis_message_set_method(msg, "cb_recv", cb_zipdb_wlog_load);
	if (sis_worker_command(zipdb_->wlog_worker, "read", msg) != SIS_METHOD_OK)
	{
		zipdb_->wlog_load = 0;
		sis_message_destroy(msg);
		return SIS_METHOD_ERROR;
	}
	sis_message_destroy(msg);
	return SIS_METHOD_OK;	
}
// 把数据写入到wlog中
int zipdb_wlog_save(s_zipdb_cxt *zipdb_, int sign_, s_zipdb_bits *inmem_)
{
	s_sis_net_message *netmsg = sis_net_message_create();

	switch (sign_)
	{
	case ZIPDB_FILE_SIGN_KEYS:
		sis_net_ask_with_chars(netmsg, "zdb.set", "_keys_", zipdb_->work_keys, sis_sdslen(zipdb_->work_keys));
		break;
	case ZIPDB_FILE_SIGN_SDBS:
		sis_net_ask_with_chars(netmsg, "zdb.set", "_sdbs_", zipdb_->work_sdbs, sis_sdslen(zipdb_->work_sdbs));
		break;	
	default: // ZIPDB_FILE_SIGN_ZPUB
		sis_net_ask_with_bytes(netmsg, "zdb.zpub", NULL, (char *)inmem_, sizeof(s_zipdb_bits) + inmem_->size);
		break;
	}
	zipdb_->wlog_method->proc(zipdb_->wlog_worker, netmsg);
	sis_net_message_destroy(netmsg);
	return 0;
}
int zipdb_wlog_move(s_zipdb_cxt *zipdb_)
{
	return sis_worker_command(zipdb_->wlog_worker, "clear", "zdb"); 
}

//////////////////////////////////////////////////////////////////
//------------------------wlog function -----------------------//
//////////////////////////////////////////////////////////////////
// 从wlog文件中加载数据
static int cb_zipdb_wfile_start(void *worker_, void *argv_)
{
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker_;
	LOG(5)("read wlog start. %s\n", (char *)argv_);
	if (context->wfile_cb_sub_start)
	{
		context->wfile_cb_sub_start(context->wfile_worker, (char *)argv_);
	}
	return SIS_METHOD_OK;
}
static int cb_zipdb_wfile_stop(void *worker_, void *argv_)
{
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker_;
	LOG(5)("read wlog stop. %s\n", (char *)argv_);
	if (context->wfile_cb_sub_stop)
	{
		context->wfile_cb_sub_stop(context->wfile_worker, (char *)argv_);
	}
	context->wfile_save = 0;
	return SIS_METHOD_OK;
}

static int cb_zipdb_wfile_load(void *worker_, void *argv_)
{
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker_;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	if (context->wfile_save != 1)
	{
		return SIS_METHOD_ERROR;
	}
    // printf("cb_zipdb_wlog_load: %d %s \n%s \n%s \n%s \n", netmsg->style,
    //         netmsg->source? netmsg->source : "nil",
    //         netmsg->cmd ?   netmsg->cmd : "nil",
    //         netmsg->key?    netmsg->key : "nil",
    //         netmsg->val?    netmsg->val : "nil");   

	if (!sis_strcasecmp("zdb.zpub", netmsg->cmd))
	{
		for (int i = 0; i < netmsg->argvs->count; i++)
		{
			s_sis_object *obj = sis_pointer_list_get(netmsg->argvs, i);
			if (context->wfile_cb_zip_bytes)
			{
				context->wfile_cb_zip_bytes(context->wfile_worker, SIS_OBJ_SDS(obj));
			}
		}
	}
	else // "zdb.set"
	{
		if (!sis_strcasecmp("_keys_", netmsg->key))
		{
			if (context->wfile_cb_dict_keys)
			{
				context->wfile_cb_dict_keys(context->wfile_worker, netmsg->val);
			}
		}
		if (!sis_strcasecmp("_sdbs_", netmsg->key))
		{
			if (context->wfile_cb_dict_sdbs)
			{
				context->wfile_cb_dict_sdbs(context->wfile_worker, netmsg->val);
			}
		}
	}
    return SIS_METHOD_OK;
}
// 把wlog转为snos格式 
int zipdb_wlog_save_snos(s_zipdb_cxt *zipdb_)
{
	if (sis_worker_command(zipdb_->wlog_worker, "exist", "zdb") != SIS_METHOD_OK)
	{
		// 文件不存在就返回
		return 0;
	}
	// 设置写文件回调
	{
		s_sis_message *msg = sis_message_create();
		if (sis_worker_command(zipdb_->wfile_worker, "getcb", msg) != SIS_METHOD_OK)
		{
			sis_message_destroy(msg);
			return 0;
		}
		zipdb_->wfile_cb_sub_start = sis_message_get_method(msg, "cb_sub_start");
		zipdb_->wfile_cb_sub_stop = sis_message_get_method(msg, "cb_sub_stop");
		zipdb_->wfile_cb_dict_keys = sis_message_get_method(msg, "cb_dict_keys");
		zipdb_->wfile_cb_dict_sdbs = sis_message_get_method(msg, "cb_dict_sdbs");
		zipdb_->wfile_cb_zip_bytes = sis_message_get_method(msg, "cb_any_bytes");
		sis_message_destroy(msg);
	}
	// 从wlog直接取数据
	{
		s_sis_message *msg = sis_message_create();
		zipdb_->wfile_save = 1;
		sis_message_set_str(msg, "dbname", "zdb", 3);
		sis_message_set(msg, "source", zipdb_, NULL);
		sis_message_set_method(msg, "cb_sub_start", cb_zipdb_wfile_start);
		sis_message_set_method(msg, "cb_sub_stop", cb_zipdb_wfile_stop);
		sis_message_set_method(msg, "cb_recv", cb_zipdb_wfile_load);
		if (sis_worker_command(zipdb_->wlog_worker, "read", msg) != SIS_METHOD_OK)
		{
			zipdb_->wfile_save = 0;
			sis_message_destroy(msg);
			return SIS_METHOD_ERROR;
		}
		sis_message_destroy(msg);
	}
	return 1;
}

// 读取 snos 文件 snos 为zipdb压缩的分块式顺序格式
int zipdb_snos_read(s_zipdb_cxt *zipdb_)
{
    // s_sis_message *msg = sis_message_create();
    // int count = sis_map_list_getsize(context->datasets);
    // for (int i = 0; i < count; i++)
    // {
    //     s_sisdb_cxt *sisdb = (s_sisdb_cxt *)((s_sis_worker *)sis_map_list_geti(context->datasets, i))->context;
    //     sis_message_set(msg, "sisdb", sisdb, NULL);
    //     sis_message_set(msg, "config", &context->catch_cfg, NULL);
    //     if (sis_worker_command(context->fast_save, "load", msg) != SIS_METHOD_OK)
    //     {
    //         sis_message_destroy(msg);
    //         return SIS_METHOD_ERROR;
    //     }
    // }
	return SIS_METHOD_OK;
}
