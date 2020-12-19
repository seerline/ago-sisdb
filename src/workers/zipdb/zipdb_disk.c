
#include "sis_modules.h"
#include "worker.h"
#include "sis_method.h"
#include "sis_disk.h"

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
void _zipdb_read_send_data(s_zipdb_reader *reader, int issend)
{
	s_zipdb_bits *zipmem = MAP_ZIPDB_BITS(reader->sub_ziper->zip_obj);
	zipmem->size = sis_bits_struct_getsize(reader->sub_ziper->cur_sbits);
	// printf("_zipdb_read_send_data :%d %d\n",zipmem->size, reader->sub_ziper->zip_size);
	if ((issend && zipmem->size > 0 ) || (int)zipmem->size > reader->sub_ziper->zip_size - 256)
	{
		if (reader->cb_zipbits)
		{	
			reader->cb_zipbits(reader, zipmem);
		}
		if (reader->sub_ziper->cur_size > reader->sub_ziper->initsize)
		{
			zipdb_worker_zip_flush(reader->sub_ziper, 1);
		}
		else
		{
			zipdb_worker_zip_flush(reader->sub_ziper, 0);
		}
	}
}
///////////////////////////////////////////
//  callback define begin
///////////////////////////////////////////
static void cb_begin(void *worker_, msec_t tt)
{
	s_zipdb_disk_worker *worker = (s_zipdb_disk_worker *)worker_;
	if (worker->zipdb_reader->cb_sub_start)
	{
        char sdate[32];
        sis_llutoa(tt, sdate, 32, 10);
		worker->zipdb_reader->cb_sub_start(worker->zipdb_reader, sdate);
	}
}
static void cb_end(void *worker_, msec_t tt)
{
	s_zipdb_disk_worker *worker = (s_zipdb_disk_worker *)worker_;
	// 检查是否有剩余数据
	_zipdb_read_send_data(worker->zipdb_reader, 1);
	if (worker->zipdb_reader->cb_sub_stop)
	{
        char sdate[32];
        sis_llutoa(tt, sdate, 32, 10);
		worker->zipdb_reader->cb_sub_stop(worker->zipdb_reader, sdate);
	}
}
static void cb_key(void *worker_, void *key_, size_t size) 
{
    // printf("%s : %s\n", __func__, (char *)key_);
	s_zipdb_disk_worker *worker = (s_zipdb_disk_worker *)worker_;
	s_sis_sds srckeys = sis_sdsnewlen((char *)key_, size);
	s_sis_sds keys = sis_match_key(worker->zipdb_reader->sub_keys, srckeys);
	zipdb_worker_set_keys(worker->zipdb_reader->sub_ziper, keys);
	if (worker->zipdb_reader->cb_dict_keys)
	{
		worker->zipdb_reader->cb_dict_keys(worker->zipdb_reader, keys);
	}
	sis_sdsfree(keys);
	sis_sdsfree(srckeys);
}

static void cb_sdb(void *worker_, void *sdb_, size_t size)  
{
	// printf("%s : %s\n", __func__, (char *)sdb_);
	s_zipdb_disk_worker *worker = (s_zipdb_disk_worker *)worker_;

	s_sis_sds srcsdbs = sis_sdsnewlen((char *)sdb_, size);
	s_sis_sds sdbs = sis_match_sdb_of_sds(worker->zipdb_reader->sub_sdbs, srcsdbs);
	zipdb_worker_set_sdbs(worker->zipdb_reader->sub_ziper, sdbs);
	if (worker->zipdb_reader->cb_dict_sdbs)
	{
		worker->zipdb_reader->cb_dict_sdbs(worker->zipdb_reader, sdbs);
	}
	sis_sdsfree(sdbs);
	sis_sdsfree(srcsdbs); 
}

static void cb_read(void *worker_, const char *key_, const char *sdb_, void *out_, size_t olen_)
{
    // printf("cb_read : %s %s\n", key_, sdb_);
	if (!key_ || !sdb_)
	{
		return ;
	}
	s_zipdb_disk_worker *worker = (s_zipdb_disk_worker *)worker_;

	int kidx = sis_map_list_get_index(worker->zipdb_reader->sub_ziper->keys, key_);
	int sidx = sis_map_list_get_index(worker->zipdb_reader->sub_ziper->sdbs, sdb_);
	// printf("cb_read :  %s %s | %d %d\n", key_, sdb_, kidx, sidx);
	if (kidx < 0 || sidx < 0)
	{
		return ;
	}
	zipdb_worker_zip_set(worker->zipdb_reader->sub_ziper, kidx, sidx, out_, olen_);

	_zipdb_read_send_data(worker->zipdb_reader, 0);

} 

void *_thread_snos_read_sub(void *argv_)
{
    s_zipdb_disk_worker *worker = (s_zipdb_disk_worker *)argv_;
    if (!worker->zipdb_reader->sub_disk)
    {
        return NULL;
    }
    worker->rdisk_status = ZIPDB_DISK_WORK;
	// 开始读盘
    s_sis_disk_callback *callback = SIS_MALLOC(s_sis_disk_callback, callback);

    callback->source = worker;
    callback->cb_begin = cb_begin;
    callback->cb_key = cb_key;
    callback->cb_sdb = cb_sdb;
    callback->cb_read = cb_read;
    callback->cb_end = cb_end;

    s_sis_disk_reader *reader = sis_disk_reader_create(callback);
    // printf("%s| %s | %s\n", context->write_cb->sub_keys, context->work_sdbs, context->write_cb->sub_sdbs ? context->write_cb->sub_sdbs : "nil");
	if (worker->zipdb_reader->sub_whole)
	{
		sis_disk_reader_set_sdb(reader, "*");  
		sis_disk_reader_set_key(reader, "*");
	}
	else
	{
		// 获取真实的代码
		s_sis_sds keys = sis_disk_file_get_keys(worker->rdisk_worker, false);
		s_sis_sds sub_keys = sis_match_key(worker->zipdb_reader->sub_keys, keys);
		sis_disk_reader_set_key(reader, sub_keys);
		sis_sdsfree(sub_keys);
		sis_sdsfree(keys);

		// 获取真实的表名
		s_sis_sds sdbs = sis_disk_file_get_sdbs(worker->rdisk_worker, false);
		s_sis_sds sub_sdbs = sis_match_sdb(worker->zipdb_reader->sub_sdbs, sdbs);
		sis_disk_reader_set_sdb(reader, worker->zipdb_reader->sub_sdbs);  
		sis_sdsfree(sub_sdbs);
		sis_sdsfree(sdbs);
	}

    // sub 是一条一条的输出
    sis_disk_file_read_sub(worker->rdisk_worker, reader);
    // get 是所有符合条件的一次性输出
    // sis_disk_file_read_get(ctrl->disk_reader, reader);

    sis_disk_reader_destroy(reader);
    sis_free(callback);
    sis_disk_file_read_stop(worker->rdisk_worker);
	printf("sub end..\n");
    worker->rdisk_status = ZIPDB_DISK_INIT;
	// 订阅结束 终止线程
    return NULL;
}

// 读取 snos 文件 snos 为zipdb压缩的分块式顺序格式
s_zipdb_disk_worker *zipdb_snos_read_start(const char *workpath_, s_zipdb_reader *reader_)
{
	if (!workpath_)
	{
		return NULL;
	}
	s_zipdb_disk_worker *worker = SIS_MALLOC(s_zipdb_disk_worker, worker);
	worker->work_path = sis_sdsnew(workpath_);
	worker->rdisk_status = ZIPDB_DISK_INIT;
	worker->zipdb_reader = reader_;

	worker->rdisk_worker = sis_disk_class_create(); 
	worker->rdisk_worker->isstop = false;

    char sdate[32];   
    sis_llutoa(worker->zipdb_reader->sub_date, sdate, 32, 10);
    
    if (sis_disk_class_init(worker->rdisk_worker, SIS_DISK_TYPE_SNO, worker->work_path, sdate)
    	|| sis_disk_file_read_start(worker->rdisk_worker))
    {
        if (worker->zipdb_reader->cb_sub_stop)
        {
            worker->zipdb_reader->cb_sub_stop(worker->zipdb_reader, sdate);
        }
		zipdb_snos_read_stop(worker);
        return NULL;
    }
	// 设置压缩对象
	if (!worker->zipdb_reader->sub_ziper)
	{
		worker->zipdb_reader->sub_ziper = zipdb_worker_create();
		// 超过 1M 就
		s_zipdb_cxt *zipdb = ((s_sis_worker *)worker->zipdb_reader->zipdb_worker)->context;
		zipdb_worker_zip_init(worker->zipdb_reader->sub_ziper, zipdb->zip_size, zipdb->initsize);
	}
	else
	{
		zipdb_worker_clear(worker->zipdb_reader->sub_ziper);
	}
	// 启动订阅线程 到这里已经保证了文件存在
	sis_thread_create(_thread_snos_read_sub, worker, &worker->rdisk_thread);

	return worker;
}

int zipdb_snos_read_stop(s_zipdb_disk_worker *worker_)
{
	if (!worker_)
	{
		return 1;
	}
	if (worker_->rdisk_status == ZIPDB_DISK_WORK)
	{
		worker_->rdisk_worker->isstop = true;
		while (worker_->rdisk_status != ZIPDB_DISK_INIT)
		{
			// 等待读盘退出
			sis_sleep(100);
		}
	}
	sis_disk_class_destroy(worker_->rdisk_worker);

	sis_sdsfree(worker_->work_path);
	
	sis_free(worker_);
	return 0;
}
