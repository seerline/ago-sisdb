#include "incrdb.h"
//////////////////////////////////////////////////////////////////
//------------------------s_incrdb_worker -----------------------//
//////////////////////////////////////////////////////////////////
s_incrdb_worker *incrdb_worker_create()
{
	s_incrdb_worker *o = SIS_MALLOC(s_incrdb_worker, o);
	o->cur_sbits = sis_bits_stream_v0_create(NULL, 0);
	o->keys = sis_map_list_create(sis_sdsfree_call);
	o->sdbs = sis_map_list_create(sis_dynamic_db_destroy);
	o->zip_bits = NULL;
	return o;
}

void incrdb_worker_destroy(s_incrdb_worker *worker)
{
	sis_map_list_destroy(worker->keys);
	sis_map_list_destroy(worker->sdbs);

	if (worker->zip_bits)
	{
		sis_free(worker->zip_bits);
	}
	sis_bits_stream_v0_destroy(worker->cur_sbits);
	sis_free(worker);
}

void incrdb_worker_unzip_init(s_incrdb_worker *worker, void *cb_source_, cb_sis_struct_decode *cb_read_)
{
	worker->cb_source = cb_source_;
	worker->cb_decode = cb_read_;
}
// 很重要
void incrdb_worker_zip_init(s_incrdb_worker *worker, int zipsize, int initsize) //, void *cb_source_, cb_sis_struct_encode *cb_read_)
{
	worker->initsize = initsize;
	worker->zip_size = zipsize;

	if (worker->zip_bits) 
	{
		sis_free(worker->zip_bits);
	}
	int size = sizeof(s_incrdb_compress) + worker->zip_size + 1024;
	worker->zip_bits = sis_malloc(size);
	incrdb_worker_zip_flush(worker, 1);
}
void incrdb_worker_clear(s_incrdb_worker *worker)
{
	LOG(5)("clear unzip reader\n");
	sis_map_list_clear(worker->keys);
	sis_map_list_clear(worker->sdbs);
	sis_bits_stream_v0_clear(worker->cur_sbits);
	// 如果是压缩 清理后必须记得 init
	// incrdb_worker_zip_init(worker, worker->zip_size, worker->initsize);
}
void incrdb_worker_zip_flush(s_incrdb_worker *worker, int isinit)
{
	s_incrdb_compress *zipmem = worker->zip_bits;
	if (isinit)
	{
		zipmem->init = 1;
		sis_bits_struct_flush(worker->cur_sbits);
		worker->cur_size = 0;
	}
	else
	{
		zipmem->init = 0;
		worker->cur_size += zipmem->size;
	}
	int size = worker->zip_size + 1024;
	sis_bits_struct_link(worker->cur_sbits, zipmem->data, size);	
	zipmem->size = 0;
	memset(zipmem->data, 0, size);			
}
void incrdb_worker_zip_set(s_incrdb_worker *worker, int kidx, int sidx, char *in_, size_t ilen_)
{
	// printf("zip cur_sbits : %p %d %d\n", worker, worker->cur_sbits->max_keynum, worker->cur_sbits->units->count);
	int o = sis_bits_struct_encode(worker->cur_sbits, kidx, sidx, in_, ilen_);
	if (o < 0)
	{
		LOG(5)("worker zip fail. %d \n", o);
	}
	worker->zip_bits->size = sis_bits_struct_getsize(worker->cur_sbits);
}

void incrdb_worker_set_keys(s_incrdb_worker *worker, s_sis_sds in_)
{
	// printf("%s\n",in_);
	s_sis_string_list *klist = sis_string_list_create();
	sis_string_list_load(klist, in_, sis_sdslen(in_), ",");
	// 重新设置keys
	int count = sis_string_list_getsize(klist);
	LOG(5)("set unzip keys %d\n", count);
	for (int i = 0; i < count; i++)
	{
		s_sis_sds key = sis_sdsnew(sis_string_list_get(klist, i));
		sis_map_list_set(worker->keys, key, key);	
	}
	sis_string_list_destroy(klist);
	sis_bits_struct_set_key(worker->cur_sbits, count);
}
void incrdb_worker_set_sdbs(s_incrdb_worker *worker, s_sis_sds in_)
{
	LOG(5)("set unzip sdbs\n");
	// printf("%s %d\n",in_, sis_sdslen(in_));
	s_sis_json_handle *injson = sis_json_load(in_, sis_sdslen(in_));
	if (!injson)
	{
		return ;
	}
	s_sis_json_node *innode = sis_json_first_node(injson->node);
	while (innode)
	{
		s_sis_dynamic_db *sdb = sis_dynamic_db_create(innode);
		if (sdb)
		{
			sis_map_list_set(worker->sdbs, sdb->name, sdb);
			sis_bits_struct_set_sdb(worker->cur_sbits, sdb);
		}
		innode = sis_json_next_node(innode);
	}
	sis_json_close(injson);
}

static int _unzip_nums = 0;
static msec_t _unzip_msec = 0;
static int _unzip_recs = 0;
static msec_t _unzip_usec = 0;

void incrdb_worker_unzip_set(s_incrdb_worker *worker, s_incrdb_compress *in_)
{
	// printf("incrdb_worker_unzip_set %s %d\n",in_, sis_sdslen(in_));
	if (in_->init == 1)
	{ 
		LOG(5)("unzip init = %d : %d\n", in_->init, worker->cur_sbits->inited);
		// 这里memset时报过错
		sis_bits_struct_flush(worker->cur_sbits);
		sis_bits_struct_link(worker->cur_sbits, in_->data, in_->size);	
	}
	else
	{
		sis_bits_struct_link(worker->cur_sbits, in_->data, in_->size);
	}
	msec_t _start_usec = sis_time_get_now_usec();
	// 开始解压 并回调
	// int nums = sis_bits_struct_decode(worker->cur_sbits, NULL, NULL);
	// printf("unzip cur_sbits : %p %d %d\n", worker, worker->cur_sbits->max_keynum, worker->cur_sbits->units->count);
	int nums = sis_bits_struct_decode(worker->cur_sbits, worker->cb_source, worker->cb_decode);
	if (nums == 0)
	{
		LOG(5)("unzip fail.\n");
	}
	if (worker->cb_decode)
	{
		// 当前包处理完毕
		worker->cb_decode(worker->cb_source, -1, -1, NULL, 0);
	}
	if (_unzip_nums == 0)
	{
		_unzip_msec = sis_time_get_now_msec();
	}
	_unzip_recs+=nums;
	_unzip_nums++;
	_unzip_usec+= (sis_time_get_now_usec() - _start_usec);
	if (_unzip_nums % 1000 == 0)
	{
		printf("unzip cost : %lld. [%d] msec : %lld recs : %d\n", _unzip_usec, _unzip_nums, 
			sis_time_get_now_msec() - _unzip_msec, _unzip_recs);
		_unzip_msec = sis_time_get_now_msec();
		_unzip_usec = 0;
	}
}
