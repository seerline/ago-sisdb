#include "sisdb_incr.h"
//////////////////////////////////////////////////////////////////
//------------------------s_sisdb_si c -----------------------//
//////////////////////////////////////////////////////////////////
s_sisdb_incr *sisdb_incr_create()
{
	s_sisdb_incr *o = SIS_MALLOC(s_sisdb_incr, o);
	o->work_keys = sis_map_list_create(sis_sdsfree_call);
	o->work_sdbs = sis_map_list_create(sis_dynamic_db_destroy);
	o->page_size = SISDB_ZIP_PAGE_SIZE;
	o->part_size = SISDB_ZIP_PART_SIZE;

	return o;
}

void sisdb_incr_destroy(s_sisdb_incr *worker)
{
	// printf("=== free %p\n", worker->work_keys);
	sis_map_list_destroy(worker->work_keys);
	sis_map_list_destroy(worker->work_sdbs);
	sis_free(worker);
}

void sisdb_incr_clear(s_sisdb_incr *worker)
{
	if (worker->incrzip)
	{
		sis_incrzip_class_destroy(worker->incrzip);
		worker->incrzip = NULL;
	}
	worker->cb_source = NULL;
	worker->cb_encode = NULL;
	worker->cb_decode = NULL;
	sis_map_list_clear(worker->work_keys);
	sis_map_list_clear(worker->work_sdbs);
}

void sisdb_incr_set_keys(s_sisdb_incr *worker, s_sis_sds in_)
{
	// printf("%s\n",in_);
	s_sis_string_list *klist = sis_string_list_create();
	sis_string_list_load(klist, in_, sis_sdslen(in_), ",");
	// 重新设置keys
	int count = sis_string_list_getsize(klist);
	// LOG(5)("set unzip keys %d\n", count);
	for (int i = 0; i < count; i++)
	{
		s_sis_sds key = sis_sdsnew(sis_string_list_get(klist, i));
		sis_map_list_set(worker->work_keys, key, key);	
	}
	sis_string_list_destroy(klist);
	LOG(5)("set unzip keys %d\n", sis_map_list_getsize(worker->work_keys));
}
void sisdb_incr_set_sdbs(s_sisdb_incr *worker, s_sis_sds in_)
{
	// LOG(5)("set unzip sdbs : %p\n", worker);
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
			sis_map_list_set(worker->work_sdbs, sdb->name, sdb);
		}
		innode = sis_json_next_node(innode);
	}
	sis_json_close(injson);
}

int sisdb_incr_get_kidx(s_sisdb_incr *worker, const char *kname_)
{
    return sis_map_list_get_index(worker->work_keys, kname_);
}
int sisdb_incr_get_sidx(s_sisdb_incr *worker, const char *sname_)
{
    return sis_map_list_get_index(worker->work_sdbs, sname_);
}
const char *sisdb_incr_get_kname(s_sisdb_incr *worker, int kidx_)
{
	return sis_map_list_geti(worker->work_keys, kidx_);
}
const char *sisdb_incr_get_sname(s_sisdb_incr *worker, int sidx_)
{
	s_sis_dynamic_db *db = sis_map_list_geti(worker->work_sdbs, sidx_);
	return db->name;
}
void _init_dict(s_sisdb_incr *worker)
{
	sis_incrzip_set_key(worker->incrzip, sis_map_list_getsize(worker->work_keys));
	int count = sis_map_list_getsize(worker->work_sdbs);
	for (int i = 0; i < count; i++)
	{
		s_sis_dynamic_db *db = sis_map_list_geti(worker->work_sdbs, i);
		sis_incrzip_set_sdb(worker->incrzip, db);
	}
}
// 
void sisdb_incr_unzip_start(s_sisdb_incr *worker, void *cb_source_, cb_incrzip_decode *cb_decode_)
{
	worker->incrzip = sis_incrzip_class_create();
	worker->cb_source = cb_source_;
	worker->cb_decode = cb_decode_;
	_init_dict(worker);
	sis_incrzip_uncompress_start(worker->incrzip, cb_source_, cb_decode_);
}

void sisdb_incr_unzip_set(s_sisdb_incr *worker, s_sis_db_incrzip *in_)
{
	sis_incrzip_uncompress_step(worker->incrzip, (char *)in_->data, in_->size);
	if (worker->cb_decode)
	{
		// 当前包处理完毕
		worker->cb_decode(worker->cb_source, -1, -1, NULL, 0);
	}
}
void sisdb_incr_unzip_stop(s_sisdb_incr *worker)
{
	if (worker->incrzip)
	{
		sis_incrzip_uncompress_stop(worker->incrzip);
		sis_incrzip_class_destroy(worker->incrzip);
		worker->incrzip = NULL;
	}
}
int sisdb_incr_getsize(s_sisdb_incr *worker)
{
	return (int)sis_incrzip_getsize(worker->incrzip);
}

static int cb_worker_encode(void *context_, char *in_, size_t ilen_)
{
    s_sisdb_incr *worker = (s_sisdb_incr *)context_;
	// printf("cb_worker_encode %p %p\n", worker, worker->cb_encode);
    if (worker->cb_encode)
    {
        worker->cb_encode(worker->cb_source, in_, ilen_);
    }
	return 0;
} 
// 很重要
void sisdb_incr_zip_start(s_sisdb_incr *worker, void *cb_source, cb_incrzip_encode *cb_encode) 
{
	worker->incrzip = sis_incrzip_class_create();
	worker->cb_source = cb_source;
	worker->cb_encode = cb_encode;
	worker->summ_size = 0;
	// printf("cb_encode ===1=== %p %p \n", worker, worker->cb_encode);
	_init_dict(worker);
	sis_incrzip_compress_start(worker->incrzip, worker->part_size, worker, cb_worker_encode);
}
void sisdb_incr_zip_set(s_sisdb_incr *worker, int kidx, int sidx, char *in_, size_t ilen_)
{
	s_sis_dynamic_db *sdb = sis_map_list_geti(worker->work_sdbs, sidx);
	if (!sdb)
	{
		LOG(5)("no find sdb. %d : %d \n", sidx, sis_map_list_getsize(worker->work_sdbs));
		return ;
	}
	// int curr_size = sis_incrzip_getsize(worker->incrzip);
	worker->summ_size += ilen_;
	// LOG(5)("----- compress . %d %d\n", worker->summ_size, worker->page_size);
    if ((worker->summ_size) > worker->page_size)
    {
		LOG(5)("restart compress . %d %d\n", worker->summ_size, worker->page_size);
        sis_incrzip_compress_restart(worker->incrzip, 1);
		worker->summ_size = 0;
    }
    int count = ilen_ / sdb->size;
    for (int i = 0; i < count; i++)
    {
        int o = sis_incrzip_compress_step(worker->incrzip, kidx, sidx, (char *)in_ + i * sdb->size, sdb->size);
		if (o < 0)
		{
			LOG(5)("compress error : %d %d\n", o, worker->incrzip->status);
		}
    }
}
void sisdb_incr_zip_restart(s_sisdb_incr *worker)
{
	sis_incrzip_compress_restart(worker->incrzip, 0);
} 

void sisdb_incr_zip_stop(s_sisdb_incr *worker)
{
	if (worker->incrzip)
	{
		sis_incrzip_compress_stop(worker->incrzip);
		sis_incrzip_class_destroy(worker->incrzip);
		worker->incrzip = NULL;
	}
}
