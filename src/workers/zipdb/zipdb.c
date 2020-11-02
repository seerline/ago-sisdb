
#include "sis_modules.h"
#include "worker.h"
#include "sis_method.h"

#include <zipdb.h>
#include <sis_net.io.h>

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////
// NULL 默认为读取
struct s_sis_method zipdb_methods[] = {
    {"init",      cmd_zipdb_init,   0, NULL},  // 初始化 workdate,keys,sdbs 
    {"start",     cmd_zipdb_start,  0, NULL},  // 开始发送数据 
    {"stop",      cmd_zipdb_stop,   0, NULL},  // 数据流发布完成 此时不再接收数据
    {"ipub",      cmd_zipdb_ipub,   0, NULL},  // 发布数据流 单条数据 kidx+sidx+data
    {"spub",      cmd_zipdb_spub,   0, NULL},  // 发布数据流 单条数据 key sdb data
    {"zpub",      cmd_zipdb_zpub,   0, NULL},  // 发布数据流 只有完成的压缩数据块
    {"sub",       cmd_zipdb_sub,    0, NULL},  // 订阅数据流 
    {"unsub",     cmd_zipdb_unsub,  0, NULL},  // 取消订阅数据流 
    {"clear",     cmd_zipdb_clear,  0, NULL},  // 清理数据流 
// 磁盘工具
    {"wlog",     cmd_zipdb_wlog,  0, NULL},  // 接收数据实时写盘
    {"rlog",     cmd_zipdb_rlog,  0, NULL},  // 异常退出时加载磁盘数据
    {"wsno",     cmd_zipdb_wsno,  0, NULL},  // 盘后转压缩格式
    {"rsno",     cmd_zipdb_rsno,  0, NULL},  // 从历史数据中获取数据 
};
// 共享内存数据库
s_sis_modules sis_modules_zipdb = {
    zipdb_init,
    NULL,
    NULL,
    NULL,
    zipdb_uninit,
    zipdb_method_init,
    zipdb_method_uninit,
    sizeof(zipdb_methods) / sizeof(s_sis_method),
    zipdb_methods,
};

//////////////////////////////////////////////////////////////////
//------------------------zipdb --------------------------------//
//////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
//------------------------s_zipdb_cxt --------------------------------//
///////////////////////////////////////////////////////////////////////////
s_sis_object *_zipdb_new_data(s_zipdb_cxt *zipdb, int isinit)
{
	size_t size = zipdb->zip_size + 256; // 这里应该取最长的结构体长度
	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create_size(size + sizeof(s_zipdb_bits)));

	s_zipdb_bits *memory = MAP_ZIPDB_BITS(obj);

	if (isinit || zipdb->calcsize > zipdb->initsize)
	{
		memory->init = 1;
		zipdb->last_object = obj;
		zipdb->calcsize = size;
		sis_bits_struct_flush(zipdb->cur_sbits);
		sis_bits_struct_link(zipdb->cur_sbits, memory->data, size);				
	}
	else
	{
		memory->init = 0;
		zipdb->calcsize += size;
		sis_bits_struct_link(zipdb->cur_sbits, memory->data, size);		
	}	
	// msec_t nowmsec = sis_time_get_now_msec();
	// if (nowmsec - agomsec > zipdb->initmsec)
	// {
	// 	memory->init = 1;
	// 	zipdb->last_object = obj;
	// 	zipdb->work_msec = nowmsec;
	// 	sis_bits_struct_flush(zipdb->cur_sbits);
	// 	sis_bits_struct_link(zipdb->cur_sbits, memory->data, size);		
	// }
	// else
	// {
	// 	memory->init = 0;
	// 	zipdb->work_msec = agomsec;
	// 	sis_bits_struct_link(zipdb->cur_sbits, memory->data, size);		
	// }
	memory->size = 0;
	memset(memory->data, 0, size);
	return obj;
}

s_sis_object *_zipdb_get_data(s_zipdb_cxt *zipdb)
{
	if (!zipdb->cur_object)
	{
		zipdb->cur_object = _zipdb_new_data(zipdb, 1);
	}
	return zipdb->cur_object;
}

static int cb_output_reader(void *reader_, s_sis_object *in_)
{
	s_zipdb_reader *reader = (s_zipdb_reader *)reader_;
	s_zipdb_cxt *zipdb = ((s_sis_worker *)reader->zipdb_worker)->context;
	s_zipdb_bits *memory = MAP_ZIPDB_BITS(in_);
	// printf("cb_output_reader %d\n", memory->size);
	if (!reader->isinit)
	{
		if (!memory->init)
		{
			return 0;
		}
		if (reader->ishead)
		{
			reader->isinit = true;
			if (reader->cb_zipbits)
			{	
				reader->cb_zipbits(reader, memory);
			}
		}
		else
		{
			// 当前包是最新的起始包 就开始发送数据
			if (zipdb->last_object == in_)
			{
				reader->isinit = true;
				if (reader->cb_zipbits)
				{	
					reader->cb_zipbits(reader, memory);
				}
			}
		}		
	}
	else
	{
		if (reader->cb_zipbits)
		{	
			reader->cb_zipbits(reader, memory);
		}
	}	
	return 0;
}

static int cb_output_realtime(void *reader_)
{
	s_zipdb_reader *reader = (s_zipdb_reader *)reader_;
	s_zipdb_cxt *zipdb = ((s_sis_worker *)reader->zipdb_worker)->context;

	char sub_date[32];
	sis_llutoa(zipdb->work_date, sub_date, 32, 10);
	if(!zipdb->stoped)
	{
		if (reader->cb_sub_realtime)
		{
			reader->cb_sub_realtime(reader, sub_date);
		}
	}
	else
	{
		if (reader->cb_sub_stop)
		{
			reader->cb_sub_stop(reader, sub_date);
		}		
	}
	
	return 0;
}
// int _zip_nums = 0;
// int _unzip_nums = 0;
// msec_t _zip_msec = 0;
// msec_t _unzip_msec = 0;
// 测试不压缩和压缩的速度一致
// 把队列数据写入压缩流中 堵塞

static int cb_input_reader(void *zipdb_, s_sis_object *in_)
{	
	// if (_unzip_nums <= 2000000)
	// {
	// 	if (_unzip_nums % 100000 == 0)
	// 	{
	// 		if (_unzip_nums > 0) printf("unzip cost : %d %d\n", _unzip_nums, sis_time_get_now_msec() - _unzip_msec);
	// 		_unzip_msec = sis_time_get_now_msec();
	// 	}
	// 	_unzip_nums++;
	// 	return 0;
	// }
	// if (_zip_nums <= 2000000)
	// {
	// 	if (_zip_nums % 100000 == 0)
	// 	{
	// 		if (_zip_nums > 0) printf("zip cost : %d %d\n", _zip_nums, sis_time_get_now_msec() - _zip_msec);
	// 		_zip_msec = sis_time_get_now_msec();
	// 	}
	// 	_zip_nums++;
	// }
	s_zipdb_cxt *zipdb = (s_zipdb_cxt *)zipdb_;
	
	s_sis_object *obj = _zipdb_get_data(zipdb);
	s_zipdb_bits *outmem = MAP_ZIPDB_BITS(obj);
	if (!in_) // 为空表示无数据超时
	{
		if (outmem->size > 1)
		{
			// printf("null ..... %lld\n", sis_time_get_now_msec());
			// printf("push 0 outmem->size = %d\n", outmem->size);
			sis_lock_list_push(zipdb->outputs, obj);
			sis_object_decr(zipdb->cur_object);
			zipdb->cur_object = _zipdb_new_data(zipdb, 0);
		}
	}
	else
	{
		// printf("in=%p\n", in_);
		// printf("inp=%p\n", in_->ptr);
		s_sis_memory *inmem = SIS_OBJ_MEMORY(in_);
		size_t offset = sis_memory_getpos(inmem);

		int kidx = sis_memory_get_byte(inmem, 4);
		int sidx = sis_memory_get_byte(inmem, 2);
		// sis_mutex_lock(&zipdb->write_lock);
		sis_bits_struct_encode(zipdb->cur_sbits, kidx, sidx, sis_memory(inmem), sis_memory_get_size(inmem));
		sis_memory_setpos(inmem, offset);
		// sis_mutex_unlock(&zipdb->write_lock);
		outmem->size = sis_bits_struct_getsize(zipdb->cur_sbits);
		//  数据如果超过一定数量就直接发送
		if (outmem->size > zipdb->zip_size - 256)
		{
			// printf("push 1 outmem->size  = %d num= %d\n", outmem->size, sis_bits_struct_get_bags(zipdb->cur_sbits, false));
			sis_lock_list_push(zipdb->outputs, obj);
			sis_object_decr(zipdb->cur_object);
			zipdb->cur_object = _zipdb_new_data(zipdb, 0);
		}
	}
	return 0;
}

bool zipdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
	s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_zipdb_cxt *context = SIS_MALLOC(s_zipdb_cxt, context);
    worker->context = context;

    context->work_date = sis_time_get_idate(0);

	// context->initmsec = 600*1000;
	context->initsize = 4*1024*1024;  // 4M 大约20秒 单只股票一天所有数据也足够一次传完
	context->calcsize = 0; 

	context->wait_msec = sis_json_get_int(node, "wait-msec", 30);//45); // 21;
	context->zip_size = ZIPMEM_MAXSIZE;

    s_sis_json_node *catchnode = sis_json_cmp_child_node(node, "catch-size");
    if (catchnode)
    {
        context->catch_size = sis_json_get_int(node,"catch-size", 1);
        if (context->catch_size > 0)
        {
            context->catch_size *= 1024*1024;
        }
    }
    else
    {
        context->catch_size = 1; // 不保留
    }

	// sis_mutex_init(&(context->write_lock), NULL);
 
	context->outputs = sis_lock_list_create(context->catch_size);
	printf("context->outputs = %p\n", context->outputs);
	context->cur_sbits = sis_bits_stream_create(NULL, 0);

	// 最大数据不超过 256M
	context->inputs = sis_lock_list_create(32*1024*1024);	
	context->in_reader = sis_lock_reader_create(context->inputs, 
		SIS_UNLOCK_READER_HEAD | SIS_UNLOCK_READER_ZERO, 
		context, cb_input_reader, NULL);
	sis_lock_reader_zero(context->in_reader, context->wait_msec);
	sis_lock_reader_open(context->in_reader);
	printf("context->in_reader = %p\n", context->in_reader);

    context->keys = sis_map_list_create(sis_sdsfree_call); 
	context->sdbs = sis_map_list_create(sis_dynamic_db_destroy);

	// 每个读者都自行压缩数据 并把压缩的数据回调出去
	context->readeres = sis_pointer_list_create();
	context->readeres->vfree = zipdb_reader_destroy;

    s_sis_json_node *wlognode = sis_json_cmp_child_node(node, "wlog");
    if (wlognode)
    {
        s_sis_worker *service = sis_worker_create(worker, wlognode);
        if (service)
        {
            context->service_wlog = service; 
			context->wlog_method = sis_worker_get_method(context->service_wlog, "write");
        }     
    }
    s_sis_json_node *wfilenode = sis_json_cmp_child_node(node, "wfile");
    if (wfilenode)
    {
        s_sis_worker *service = sis_worker_create(worker, wfilenode);
        if (service)
        {
            context->service_wfile = service; 
        }     
    }
    s_sis_json_node *rfilenode = sis_json_cmp_child_node(node, "rfile");
    if (rfilenode)
    {
        s_sis_worker *service = sis_worker_create(worker, rfilenode);
        if (service)
        {
            context->service_rfile = service; 
        }     
    }
    return true;
}
void zipdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *zipdb = (s_zipdb_cxt *)worker->context;

    if (zipdb->service_wlog)
    {
        sis_worker_destroy(zipdb->service_wlog);
		zipdb->service_wlog = NULL;
    }
    if (zipdb->service_wfile)
    {
        sis_worker_destroy(zipdb->service_wfile);
		zipdb->service_wfile = NULL;
    }
    if (zipdb->service_rfile)
    {
        sis_worker_destroy(zipdb->service_rfile);
		zipdb->service_rfile = NULL;
    }
	sis_sdsfree(zipdb->work_keys);
	sis_sdsfree(zipdb->work_sdbs);
    sis_map_list_destroy(zipdb->keys); 
    sis_map_list_destroy(zipdb->sdbs);

	sis_lock_list_destroy(zipdb->inputs);

	sis_object_decr(zipdb->cur_object);
	sis_bits_stream_destroy(zipdb->cur_sbits);

	sis_pointer_list_destroy(zipdb->readeres);
	sis_lock_list_destroy(zipdb->outputs);

	sis_free(zipdb);
}

void zipdb_method_init(void *worker_)
{
    // 加载数据
}
void zipdb_method_uninit(void *worker_)
{
    // 释放数据
}

// 设置
bool _zipdb_new(s_zipdb_cxt *zipdb_, int workdate_, s_sis_sds keys_, s_sis_sds sdbs_)
{
	zipdb_->work_date = 0;
	if (!keys_ || sis_sdslen(keys_) < 3 || !sdbs_ || sis_sdslen(sdbs_) < 3)
	{
		return false;
	}
	printf("------ _zipdb_new : %d %d %d\n", workdate_, (int)sis_sdslen(keys_), (int)sis_sdslen(sdbs_));
	zipdb_->work_date = workdate_;

	sis_sdsfree(zipdb_->work_keys); zipdb_->work_keys = NULL;
	sis_sdsfree(zipdb_->work_sdbs); zipdb_->work_sdbs = NULL;

	sis_map_list_clear(zipdb_->keys);
	sis_map_list_clear(zipdb_->sdbs);	
	{
		s_sis_string_list *klist = sis_string_list_create();
		sis_string_list_load(klist, keys_, sis_sdslen(keys_), ",");
		// 重新设置keys
		int count = sis_string_list_getsize(klist);
		if (count < 1)
		{
			return false;
		}
		for (int i = 0; i < count; i++)
		{
			s_sis_sds key = sis_sdsnew(sis_string_list_get(klist, i));
			sis_map_list_set(zipdb_->keys, key, key);	
		}
    	sis_bits_struct_set_key(zipdb_->cur_sbits, count);
		sis_string_list_destroy(klist);
		zipdb_->work_keys = sis_sdsnew(keys_);
	}
	{
		s_sis_json_handle *injson = sis_json_load(sdbs_, sis_sdslen(sdbs_));
		if (!injson)
		{
			return false;
		}
		s_sis_json_node *innode = sis_json_first_node(injson->node);
		while (innode)
		{
			s_sis_dynamic_db *sdb = sis_dynamic_db_create(innode);
			if (sdb)
			{
				sis_map_list_set(zipdb_->sdbs, innode->key, sdb);
				sis_bits_struct_set_sdb(zipdb_->cur_sbits, sdb);
			}
			innode = sis_json_next_node(innode);
		}
		zipdb_->work_sdbs = sis_sdsnew(sdbs_);
		sis_json_close(injson);
	}
	return true;
}

int cmd_zipdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

	int work_date = sis_message_get_int(msg, "work_date");
	s_sis_sds keys = sis_message_get_str(msg, "keys");
	s_sis_sds sdbs = sis_message_get_str(msg, "sdbs");

    if (_zipdb_new(context, work_date, keys, sdbs))
    {
		context->inited = true;
		context->stoped = false;
        return SIS_METHOD_OK;
    }
    return SIS_METHOD_ERROR;
}
int cmd_zipdb_start(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;

	if (!context->inited)
	{
		LOG(5)("zipdb no init.\n");
		return SIS_METHOD_ERROR;
	}
	// 初始化后首先向所有订阅者发送订阅开始信息
	for (int i = 0; i < context->readeres->count; i++)
	{
		s_zipdb_reader *reader = (s_zipdb_reader *)sis_pointer_list_get(context->readeres, i);
		zipdb_sub_start(reader);
	}
	return SIS_METHOD_OK;
}
int cmd_zipdb_stop(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
	// printf("cmd_zipdb_stop, curmemory : %d\n", context->cur_object->size);
	context->stoped = true;
	for (int i = 0; i < context->readeres->count; i++)
	{
		s_zipdb_reader *reader = (s_zipdb_reader *)sis_pointer_list_get(context->readeres, i);
		if (reader->cb_sub_stop)
		{
			char sub_date[32];
        	sis_llutoa(context->work_date, sub_date, 32, 10);
			reader->cb_sub_stop(reader, sub_date);
		}
		zipdb_sub_stop(reader);
	}
    return SIS_METHOD_OK;
}

int _zipdb_write(s_zipdb_cxt *zipdb_, int kidx_, int sidx_, void *in_, size_t ilen_)
{
	if (!zipdb_->inited || zipdb_->stoped || kidx_ < 0 || sidx_ < 0)
	{
		return -1;
	}
	s_sis_memory *memory = sis_memory_create();
	sis_memory_cat_byte(memory, kidx_, 4);
	sis_memory_cat_byte(memory, sidx_, 2);
	sis_memory_cat(memory, (char *)in_, ilen_);	
	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, memory);

	sis_lock_list_push(zipdb_->inputs, obj);
	
	sis_object_destroy(obj);
	return 1;
}

int _zipdb_write_bits(s_zipdb_cxt *zipdb_, s_zipdb_bits *in_)
{
	if (!zipdb_->inited || zipdb_->stoped)
	{
		return -1;
	}
	// 直接写入
	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create_size(sizeof(s_zipdb_bits) + in_->size));
	memmove(MAP_ZIPDB_BITS(obj), in_, sizeof(s_zipdb_bits) + in_->size);
	// printf("push 3 outmem->size = %d\n", MAP_ZIPDB_BITS(obj)->size);
	sis_lock_list_push(zipdb_->outputs, obj);
	sis_object_destroy(obj);
	return 0;
}
// #include "stk_struct.v0.h"
int cmd_zipdb_ipub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
    s_zipdb_bytes *in = (s_zipdb_bytes *)argv_;
	// if (in->size == 224)
	// {
	// 	s_v3_stk_snapshot *input = (s_v3_stk_snapshot *)in->data;
	// 	printf("cmd_zipdb_ipub: %d %d %d time = %lld\n",in->kidx, in->sidx, in->size, sis_time_get_itime(input->time/1000));
	// }
	// printf("---1 %d %d\n", in->kidx, in->sidx);
    if (_zipdb_write(context, in->kidx, in->sidx, in->data, in->size) >= 0)
    {
        return SIS_METHOD_OK;
    }
    return SIS_METHOD_ERROR;
}
int cmd_zipdb_spub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
    s_zipdb_chars *in = (s_zipdb_chars *)argv_;

	int kidx = sis_map_list_get_index(context->keys, in->keyn);
	int sidx = sis_map_list_get_index(context->sdbs, in->sdbn);

    if (_zipdb_write(context, kidx, sidx, in->data, in->size) >= 0)
    {
        return SIS_METHOD_OK;
    }
    return SIS_METHOD_ERROR;
}
// 从这里写入的数据为索引+二进制数据
int cmd_zipdb_zpub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
    s_zipdb_bits *in = (s_zipdb_bits *)argv_;

    if (_zipdb_write_bits(context, in) >= 0)
    {
        return SIS_METHOD_OK;
    }
    return SIS_METHOD_ERROR;
}

int cmd_zipdb_sub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    // s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

    s_zipdb_reader *reader = zipdb_reader_create();
	reader->cid = sis_message_get_int(msg, "cid");
	s_sis_sds source = sis_message_get_str(msg, "source");
	if (source)
	{
		reader->source = sis_sdsdup(source);
	}
    reader->zipdb_worker = worker;
    reader->ishead = sis_message_get_int(msg, "ishead");
	printf("reader->ishead = %d\n", reader->ishead);
    reader->cb_zipbits     = sis_message_get_method(msg, "cb_zipbits");

    reader->cb_sub_start = sis_message_get_method(msg, "cb_sub_start");
    reader->cb_sub_realtime = sis_message_get_method(msg, "cb_sub_realtime");
    reader->cb_sub_stop = sis_message_get_method(msg, "cb_sub_stop");

    reader->cb_dict_keys = sis_message_get_method(msg, "cb_dict_keys");
    reader->cb_dict_sdbs = sis_message_get_method(msg, "cb_dict_sdbs");

    // 一个 socket 只能订阅一次 后订阅的会冲洗掉前面一次
    zipdb_add_reader(reader);

    return SIS_METHOD_OK;

}

int cmd_zipdb_unsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

	int cid = sis_message_get_int(msg, "cid");

	zipdb_move_reader(context, cid);

    return SIS_METHOD_OK;
}
void _zipdb_clear_data(s_zipdb_cxt *context)
{
	context->last_object = NULL;
	sis_object_decr(context->cur_object);
	context->cur_object = NULL;
	sis_lock_list_clear(context->outputs);
}
int cmd_zipdb_clear(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

	int mode = sis_message_get_int(msg, "mode");
	switch (mode)
	{
	case ZIPDB_CLEAR_MODE_DATA:
		_zipdb_clear_data(context);
		break;
	case ZIPDB_CLEAR_MODE_READER:
		sis_pointer_list_clear(context->readeres);
		break;	
	default:
		sis_pointer_list_clear(context->readeres);
		_zipdb_clear_data(context);
		break;
	}
    return SIS_METHOD_OK;
}
	
int cmd_zipdb_wlog(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	
	context->wlog_method->proc(context->service_wlog, netmsg);

	return SIS_METHOD_OK;
}
int cmd_zipdb_rlog(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
	// s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	return SIS_METHOD_OK;
	s_sis_message *msg = sis_message_create();
	sis_message_set(msg, "zipdb", worker, NULL);
	sis_message_set(msg, "source", context, NULL);
	// sis_message_set_method(msg, "cb_recv", cb_zipdb_wlog_load);
	if (sis_worker_command(context->service_wlog, "rlog", msg) != SIS_METHOD_OK)
	{
		sis_message_destroy(msg);
		return SIS_METHOD_ERROR;
	}
	sis_message_destroy(msg);
	return SIS_METHOD_OK;
}
int cmd_zipdb_wsno(void *worker_, void *argv_)
{
	// 从log中获取数据后直接写盘
	return SIS_METHOD_OK;
}
int cmd_zipdb_rsno(void *worker_, void *argv_)
{
	return SIS_METHOD_OK;
}
//////////////////////////////////////////////////////////////////
//------------------------s_unzipdb_reader -----------------------//
//////////////////////////////////////////////////////////////////
s_unzipdb_reader *unzipdb_reader_create(void *cb_source_, cb_sis_struct_decode *cb_read_)
{
	s_unzipdb_reader *o = SIS_MALLOC(s_unzipdb_reader, o);
	o->cb_source = cb_source_;
	o->cb_read = cb_read_;
	o->cur_sbits = sis_bits_stream_create(NULL, 0);
	o->keys = sis_map_list_create(sis_sdsfree_call);
	o->sdbs = sis_map_list_create(sis_dynamic_db_destroy);
	return o;
}
void unzipdb_reader_destroy(s_unzipdb_reader *unzipdb_)
{
	sis_map_list_destroy(unzipdb_->keys);
	sis_map_list_destroy(unzipdb_->sdbs);
	sis_bits_stream_destroy(unzipdb_->cur_sbits);
	sis_free(unzipdb_);
}

void unzipdb_reader_set_keys(s_unzipdb_reader *unzipdb_, s_sis_sds in_)
{
	// printf("%s\n",in_);
	s_sis_string_list *klist = sis_string_list_create();
	sis_string_list_load(klist, in_, sis_sdslen(in_), ",");
	// 重新设置keys
	int count = sis_string_list_getsize(klist);
	for (int i = 0; i < count; i++)
	{
		s_sis_sds key = sis_sdsnew(sis_string_list_get(klist, i));
		sis_map_list_set(unzipdb_->keys, key, key);	
	}
	sis_string_list_destroy(klist);
	sis_bits_struct_set_key(unzipdb_->cur_sbits, count);
}
void unzipdb_reader_set_sdbs(s_unzipdb_reader *unzipdb_, s_sis_sds in_)
{
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
			sis_map_list_set(unzipdb_->sdbs, innode->key, sdb);
			sis_bits_struct_set_sdb(unzipdb_->cur_sbits, sdb);
		}
		innode = sis_json_next_node(innode);
	}
	sis_json_close(injson);
}

void unzipdb_reader_set_bits(s_unzipdb_reader *unzipdb_, s_zipdb_bits *in_)
{
	if (in_->init == 1)
	{
		// 这里memset时报过错
		sis_bits_struct_flush(unzipdb_->cur_sbits);
		sis_bits_struct_link(unzipdb_->cur_sbits, in_->data, in_->size);	
	}
	else
	{
		sis_bits_struct_link(unzipdb_->cur_sbits, in_->data, in_->size);
	}
	// 开始解压 并回调
	if(sis_bits_struct_decode(unzipdb_->cur_sbits, unzipdb_->cb_source, unzipdb_->cb_read) == 0)
	{
		LOG(5)("unzip fail.\n");
	}
	// printf("---\n");
}

///////////////////////////////////////////////////////////////////////////
//------------------------s_zipdb_reader --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_zipdb_reader *zipdb_reader_create()
{
	s_zipdb_reader *o = SIS_MALLOC(s_zipdb_reader, o);
	return o;
}
void zipdb_reader_destroy(void *reader_)
{
	s_zipdb_reader *reader = (s_zipdb_reader *)reader_;
	
	if (reader->unzip_reader)
	{
		unzipdb_reader_destroy(reader->unzip_reader);	
	}
	if (reader->reader)
	{
		sis_lock_reader_close(reader->reader);	
	}
	sis_sdsfree(reader->sub_keys);
	sis_sdsfree(reader->sub_sdbs);
	sis_sdsfree(reader->source);
	sis_free(reader);
}

int zipdb_add_reader(s_zipdb_reader *reader_)
{
    s_sis_worker *worker = (s_sis_worker *)reader_->zipdb_worker; 
    s_zipdb_cxt *zipdb = (s_zipdb_cxt *)worker->context;

	zipdb_move_reader(zipdb, reader_->cid);
	if (zipdb->inited)  // 如果已经初始化就启动发送数据 否则就等待系统通知后再发送
	{
		zipdb_sub_start(reader_);
	}
	sis_pointer_list_push(zipdb->readeres, reader_);
	return 1;
}
int zipdb_move_reader(s_zipdb_cxt *zipdb_,int cid_)
{
	int count = 0;
	for (int i = 0; i < zipdb_->readeres->count; )
	{
		s_zipdb_reader *reader = (s_zipdb_reader *)sis_pointer_list_get(zipdb_->readeres, i);
		if (reader->cid == cid_)
		{
			sis_pointer_list_delete(zipdb_->readeres, i, 1);
			count++;
		}
		else
		{
			i++;
		}
	}	
	return count;
}

int zipdb_sub_start(s_zipdb_reader *reader)
{
    s_sis_worker *worker = (s_sis_worker *)reader->zipdb_worker; 
    s_zipdb_cxt *zipdb = (s_zipdb_cxt *)worker->context;
    
	if (reader->cb_sub_start)
	{
		char sub_date[32];
		sis_llutoa(zipdb->work_date, sub_date, 32, 10);
		reader->cb_sub_start(reader, sub_date);
	}
	LOG(5)("sub start : count = %d inited = %d stoped = %d\n", zipdb->outputs->work_queue->rnums, zipdb->inited, zipdb->stoped);
	if (reader->cb_dict_keys)
	{
		reader->cb_dict_keys(reader, zipdb->work_keys);
	}
	if (reader->cb_dict_sdbs)
	{
		reader->cb_dict_sdbs(reader, zipdb->work_sdbs);
	}
	// 新加入的订阅者 先发送订阅开始 再发送最后一个初始块后续所有数据 
	reader->isinit = false;
	if (!reader->reader)
	{
		reader->reader = sis_lock_reader_create(zipdb->outputs, 
			SIS_UNLOCK_READER_HEAD, reader, cb_output_reader, cb_output_realtime);
		sis_lock_reader_open(reader->reader);	
	}
    return 0;
}

int zipdb_sub_stop(s_zipdb_reader *reader)
{    
	if (reader->reader)
	{
		sis_lock_reader_close(reader->reader);	
		reader->reader = NULL;
	}
	return 0;
}
