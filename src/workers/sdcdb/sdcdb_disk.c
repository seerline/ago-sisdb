
#include "sis_modules.h"
#include "worker.h"
#include "sis_method.h"
#include "sis_disk.h"

#include <sdcdb.h>

//////////////////////////////////////////////////////////////////
//------------------------wlog function -----------------------//
//////////////////////////////////////////////////////////////////
bool _sdcdb_write_init(s_sdcdb_cxt *sdcdb_, int workdate_, s_sis_sds keys_, s_sis_sds sdbs_);
int _sdcdb_write_bits(s_sdcdb_cxt *sdcdb_, s_sdcdb_compress *in_);

// 从wlog文件中加载数据
static int cb_sdcdb_wlog_start(void *worker_, void *argv_)
{
    s_sdcdb_cxt *context = (s_sdcdb_cxt *)worker_;
	const char *sdate = (const char *)argv_;
	LOG(5)("load wlog start. %s\n", sdate);
	context->wlog_date = sis_atoll(sdate); 
	return SIS_METHOD_OK;
}
static int cb_sdcdb_wlog_stop(void *worker_, void *argv_)
{
    s_sdcdb_cxt *context = (s_sdcdb_cxt *)worker_;
	const char *sdate = (const char *)argv_;
	context->wlog_load = 0;
	LOG(5)("load wlog stop. %s\n", sdate);
	return SIS_METHOD_OK;
}
static int cb_sdcdb_wlog_load(void *worker_, void *argv_)
{
    s_sdcdb_cxt *context = (s_sdcdb_cxt *)worker_;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	if (context->wlog_load != 1)
	{
		return SIS_METHOD_ERROR;
	}
    // printf("cb_sdcdb_wlog_load: %d %s \n%s \n%s \n%s \n", netmsg->style,
    //         netmsg->serial? netmsg->serial : "nil",
    //         netmsg->cmd ?   netmsg->cmd : "nil",
    //         netmsg->key?    netmsg->key : "nil",
    //         netmsg->val?    netmsg->val : "nil");   

	if (!sis_strcasecmp("zpub", netmsg->cmd))
	{
		for (int i = 0; i < netmsg->argvs->count; i++)
		{
			s_sis_object *obj = sis_pointer_list_get(netmsg->argvs, i);
			_sdcdb_write_bits(context, (s_sdcdb_compress *)SIS_OBJ_SDS(obj));
		}
	}
	else // "set"
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
			if(!_sdcdb_write_init(context, context->wlog_date, context->wlog_keys, context->wlog_sdbs))
			{
				// 数据错误 应该停止读取 直接返回错误
				context->wlog_load = 0;
			}	
		}
	}
    return SIS_METHOD_OK;
}

// 从磁盘中获取数据 0 没有wlog文件 ** 文件有错误直接删除 并返回0
//               1 有文件 正在读 需要设置状态 并等待数据读完
int sdcdb_wlog_load(s_sdcdb_cxt *sdcdb_)
{
	s_sis_message *msg = sis_message_create();

	sdcdb_->wlog_load = 1;
	sis_message_set_str(msg, "dbname", sdcdb_->dbname, sis_sdslen(sdcdb_->dbname));
	sis_message_set(msg, "source", sdcdb_, NULL);
	sis_message_set_method(msg, "cb_sub_start", cb_sdcdb_wlog_start);
	sis_message_set_method(msg, "cb_sub_stop", cb_sdcdb_wlog_stop);
	sis_message_set_method(msg, "cb_recv", cb_sdcdb_wlog_load);
	if (sis_worker_command(sdcdb_->wlog_worker, "read", msg) != SIS_METHOD_OK)
	{
		sdcdb_->wlog_load = 0;
		sis_message_destroy(msg);
		return SIS_METHOD_ERROR;
	}
	sis_message_destroy(msg);
	return SIS_METHOD_OK;	
}
// 把数据写入到wlog中
int sdcdb_wlog_save(s_sdcdb_cxt *sdcdb_, int sign_, s_sdcdb_compress *inmem_)
{
	s_sis_net_message *netmsg = sis_net_message_create();
	switch (sign_)
	{
	case SDCDB_FILE_SIGN_KEYS:
		sis_net_ask_with_chars(netmsg, "set", "_keys_", sdcdb_->work_keys, sis_sdslen(sdcdb_->work_keys));
		break;
	case SDCDB_FILE_SIGN_SDBS:
		sis_net_ask_with_chars(netmsg, "set", "_sdbs_", sdcdb_->work_sdbs, sis_sdslen(sdcdb_->work_sdbs));
		break;	
	default: // SDCDB_FILE_SIGN_ZPUB
		sis_net_ask_with_bytes(netmsg, "zpub", NULL, (char *)inmem_, sizeof(s_sdcdb_compress) + inmem_->size);
		break;
	}
	sdcdb_->wlog_method->proc(sdcdb_->wlog_worker, netmsg);
	sis_net_message_destroy(netmsg);
	return 0;
}
int sdcdb_wlog_start(s_sdcdb_cxt *sdcdb_)
{
	return sis_worker_command(sdcdb_->wlog_worker, "open", sdcdb_->dbname); 
}
int sdcdb_wlog_stop(s_sdcdb_cxt *sdcdb_)
{
	return sis_worker_command(sdcdb_->wlog_worker, "close", sdcdb_->dbname); 
}
int sdcdb_wlog_move(s_sdcdb_cxt *sdcdb_)
{
	return sis_worker_command(sdcdb_->wlog_worker, "move", sdcdb_->dbname); 
}

//////////////////////////////////////////////////////////////////
//------------------------wlog function -----------------------//
//////////////////////////////////////////////////////////////////
// 从wlog文件中加载数据
static int cb_sdcdb_wfile_start(void *worker_, void *argv_)
{
    s_sdcdb_cxt *context = (s_sdcdb_cxt *)worker_;
	LOG(5)("read wlog start. %s\n", (char *)argv_);
	if (context->wfile_cb_sub_start)
	{
		context->wfile_cb_sub_start(context->wfile_worker, (char *)argv_);
	}
	return SIS_METHOD_OK;
}
static int cb_sdcdb_wfile_stop(void *worker_, void *argv_)
{
    s_sdcdb_cxt *context = (s_sdcdb_cxt *)worker_;
	LOG(5)("read wlog stop. %s\n", (char *)argv_);
	if (context->wfile_cb_sub_stop)
	{
		context->wfile_cb_sub_stop(context->wfile_worker, (char *)argv_);
	}
	context->wfile_save = 0;
	return SIS_METHOD_OK;
}

static int cb_sdcdb_wfile_load(void *worker_, void *argv_)
{
    s_sdcdb_cxt *context = (s_sdcdb_cxt *)worker_;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	if (context->wfile_save != 1)
	{
		return SIS_METHOD_ERROR;
	}
    // printf("cb_sdcdb_wfile_load: %d %s \n%s \n%s \n%s \n", netmsg->style,
    //         netmsg->serial? netmsg->serial : "nil",
    //         netmsg->cmd ?   netmsg->cmd : "nil",
    //         netmsg->key?    netmsg->key : "nil",
    //         netmsg->val?    netmsg->val : "nil");   
	if (!sis_strcasecmp("zpub", netmsg->cmd))
	{
		// printf("%p %d\n", context->wfile_cb_sdcdb_compress, netmsg->argvs->count);
		for (int i = 0; i < netmsg->argvs->count; i++)
		{ 
			s_sis_object *obj = sis_pointer_list_get(netmsg->argvs, i);
			if (context->wfile_cb_sdcdb_compress)
			{
				context->wfile_cb_sdcdb_compress(context->wfile_worker, SIS_OBJ_SDS(obj));
			}
		}
	}
	else // "set"
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
int sdcdb_wlog_save_snos(s_sdcdb_cxt *sdcdb_)
{
	if (sis_worker_command(sdcdb_->wlog_worker, "exist", sdcdb_->dbname) != SIS_METHOD_OK)
	{
		// 文件不存在就返回
		return 0;
	}
	// 设置写文件回调
	{
		s_sis_message *msg = sis_message_create();
		if (sis_worker_command(sdcdb_->wfile_worker, "getcb", msg) != SIS_METHOD_OK)
		{
			sis_message_destroy(msg);
			return 0;
		}
		sdcdb_->wfile_cb_sub_start = sis_message_get_method(msg, "cb_sub_start");
		sdcdb_->wfile_cb_sub_stop  = sis_message_get_method(msg, "cb_sub_stop");
		sdcdb_->wfile_cb_dict_keys = sis_message_get_method(msg, "cb_dict_keys");
		sdcdb_->wfile_cb_dict_sdbs = sis_message_get_method(msg, "cb_dict_sdbs");
		sdcdb_->wfile_cb_sdcdb_compress = sis_message_get_method(msg, "cb_sdcdb_compress");
		sis_message_destroy(msg);
	}
	// 从wlog直接取数据
	{
		s_sis_message *msg = sis_message_create();
		sdcdb_->wfile_save = 1;
		sis_message_set_str(msg, "dbname", sdcdb_->dbname, sis_sdslen(sdcdb_->dbname));
		sis_message_set(msg, "source", sdcdb_, NULL);
		sis_message_set_method(msg, "cb_sub_start", cb_sdcdb_wfile_start);
		sis_message_set_method(msg, "cb_sub_stop", cb_sdcdb_wfile_stop);
		sis_message_set_method(msg, "cb_recv", cb_sdcdb_wfile_load);
		if (sis_worker_command(sdcdb_->wlog_worker, "read", msg) != SIS_METHOD_OK)
		{
			sdcdb_->wfile_save = 0;
			sis_message_destroy(msg);
			return SIS_METHOD_ERROR;
		}
		sis_message_destroy(msg);
	}
	return 1;
}

///////////////////////////////////////////
//  callback define begin
///////////////////////////////////////////

// 读取 snos 文件 snos 为sdcdb压缩的分块式顺序格式
s_sdcdb_disk_worker *sdcdb_snos_read_start(s_sis_json_node *config_, s_sdcdb_reader *reader_)
{
	// printf("%s\n", __func__);
	s_sdcdb_disk_worker *worker = SIS_MALLOC(s_sdcdb_disk_worker, worker);

	worker->rdisk_worker = sis_worker_create(NULL, config_);
	if (!worker->rdisk_worker)
	{
		sis_free(worker);
		return NULL;
	}
	// printf("%s\n", worker->rdisk_worker->workername);
	worker->rdisk_status = SDCDB_DISK_INIT;
	worker->sdcdb_reader = reader_;

	s_sis_message *msg = sis_message_create();
	sis_message_set_int(msg, "sub-date", worker->sdcdb_reader->sub_date);
	sis_message_set_str(msg, "sub-keys", worker->sdcdb_reader->sub_keys, sis_sdslen(worker->sdcdb_reader->sub_keys));
	sis_message_set_str(msg, "sub-sdbs", worker->sdcdb_reader->sub_sdbs, sis_sdslen(worker->sdcdb_reader->sub_sdbs));

    sis_message_set(msg, "source", worker->sdcdb_reader, NULL);
    sis_message_set_method(msg, "cb_sub_start" ,  worker->sdcdb_reader->cb_sub_start);
    sis_message_set_method(msg, "cb_sub_stop"  ,  worker->sdcdb_reader->cb_sub_stop );
    sis_message_set_method(msg, "cb_dict_sdbs" ,  worker->sdcdb_reader->cb_dict_sdbs);
    sis_message_set_method(msg, "cb_dict_keys" ,  worker->sdcdb_reader->cb_dict_keys);

	if (worker->sdcdb_reader->iszip)
	{
		s_sdcdb_cxt *sdcdb = ((s_sis_worker *)worker->sdcdb_reader->sdcdb_worker)->context;
		sis_message_set_int(msg, "zip-size" , sdcdb->zip_size);
		sis_message_set_int(msg, "init-size", sdcdb->initsize);
		sis_message_set_method(msg, "cb_sdcdb_compress", worker->sdcdb_reader->cb_sdcdb_compress);
	}
	else
	{
		sis_message_set_method(msg, "cb_sisdb_bytes", worker->sdcdb_reader->cb_sisdb_bytes);
	}

	sis_worker_command(worker->rdisk_worker, "sub", msg);
	sis_message_destroy(msg);

	return worker;
}

int sdcdb_snos_read_stop(s_sdcdb_disk_worker *worker_)
{
	sis_worker_command(worker_->rdisk_worker, "unsub", NULL);

	sis_worker_destroy(worker_->rdisk_worker);

	sis_free(worker_);
	return 0;
}



