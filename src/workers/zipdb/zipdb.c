
#include "sis_modules.h"
#include "worker.h"
#include "server.h"
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
    {"wlog",      cmd_zipdb_wlog,  0, NULL},  // 接收数据实时写盘
    {"rlog",      cmd_zipdb_rlog,  0, NULL},  // 异常退出时加载磁盘数据
    {"wsno",      cmd_zipdb_wsno,  0, NULL},  // 盘后转压缩格式
    {"rsno",      cmd_zipdb_rsno,  0, NULL},  // 从历史数据中获取数据 
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
// zipdb 必须先收到 init 再收到 start 顺序不能错
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
		// printf("new last obj = %p\n", obj);
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

	// printf("cb_output_reader %d %d | %d %d | %p %p\n", reader->isinit, reader->ishead, 
	// 	memory->init, memory->size,
	// 	zipdb->last_object , in_);
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
	// printf("cb_output_reader %d %d | %d %d | %p %p\n", reader->isinit, reader->ishead, 
	// 	memory->init, memory->size,
	// 	zipdb->last_object , in_);
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

	// bool isnew = false;
	s_zipdb_cxt *zipdb = (s_zipdb_cxt *)zipdb_;
	
	s_sis_object *obj = _zipdb_get_data(zipdb);
	s_zipdb_bits *outmem = MAP_ZIPDB_BITS(obj);

	// printf("--input %d %d %p\n", zipdb->inputs->rnums, outmem->size, in_);

	if (!in_) // 为空表示无数据超时
	{
		if (outmem->size > 1)
		{
			// printf("push 0 outmem->size = %d\n", outmem->size);
			sis_memory_set_size(SIS_OBJ_MEMORY(obj), sizeof(s_zipdb_bits) + outmem->size);
			sis_lock_list_push(zipdb->outputs, obj);
			// isnew = true;
			// _zip_nums++;
			sis_object_decr(zipdb->cur_object);
			zipdb->cur_object = _zipdb_new_data(zipdb, 0);
		}
	}
	else
	{
		s_sis_memory *inmem = SIS_OBJ_MEMORY(in_);
		size_t offset = sis_memory_getpos(inmem);

		int kidx = sis_memory_get_byte(inmem, 4);
		int sidx = sis_memory_get_byte(inmem, 2);
		// printf("%p %p %p |%d %d %d\n",zipdb->cur_sbits, zipdb->cur_object, zipdb->last_object, zipdb->cur_sbits->inited, kidx, sidx);
		sis_bits_struct_encode(zipdb->cur_sbits, kidx, sidx, sis_memory(inmem), sis_memory_get_size(inmem));
		sis_memory_setpos(inmem, offset);
		outmem->size = sis_bits_struct_getsize(zipdb->cur_sbits);

		//  数据如果超过一定数量就直接发送
		if (outmem->size > zipdb->zip_size - 256)
		{
			sis_memory_set_size(SIS_OBJ_MEMORY(obj), sizeof(s_zipdb_bits) + outmem->size);
			// printf("push 1 outmem->size  = %d num= %d\n", outmem->size, sis_bits_struct_get_bags(zipdb->cur_sbits, false));
			sis_lock_list_push(zipdb->outputs, obj);
			// isnew = true;
			sis_object_decr(zipdb->cur_object);
			zipdb->cur_object = _zipdb_new_data(zipdb, 0);
		}
	}
	// if (isnew)
	// {
	// 	if (_zip_nums == 0)
	// 	{
	// 		_zip_msec = sis_time_get_now_msec();
	// 	}
	// 	_zip_nums++;
	// 	if (_zip_nums % 1000 == 0)
	// 	{
	// 		printf("zip cost : %lld recs: %d\n", sis_time_get_now_msec() - _zip_msec, _zip_nums);
	// 		_zip_msec = sis_time_get_now_msec();
	// 	}
	// }
	return 0;
}

static int cb_wlog_reader(void *reader_, s_sis_object *in_)
{
	s_zipdb_reader *reader = (s_zipdb_reader *)reader_;
	s_zipdb_cxt *zipdb = ((s_sis_worker *)reader->zipdb_worker)->context;
	s_zipdb_bits *memory = MAP_ZIPDB_BITS(in_);
	// 从第一个初始包开始存盘
	if (!reader->isinit)
	{
		if (!memory->init)
		{
			return 0;
		}
		reader->isinit = true;
	}
	// 无论是否有历史数据，都从新写一份keys和sdbs读取的时候也要更新dict表 以免数据混乱
	if(zipdb->wlog_init == 0)
	{
		zipdb_wlog_save(zipdb, ZIPDB_FILE_SIGN_KEYS, NULL);
		zipdb_wlog_save(zipdb, ZIPDB_FILE_SIGN_SDBS, NULL);
		zipdb->wlog_init = 1;
	}
	zipdb_wlog_save(zipdb, ZIPDB_FILE_SIGN_ZPUB, memory);
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
        context->catch_size = sis_json_get_int(node,"catch-size", 0);
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
	printf("context->outputs save = %d\n", context->outputs->save_mode);
	context->cur_sbits = sis_bits_stream_create(NULL, 0);

    context->keys = sis_map_list_create(sis_sdsfree_call); 
	context->sdbs = sis_map_list_create(sis_dynamic_db_destroy);

	// 每个读者都自行压缩数据 并把压缩的数据回调出去
	context->readeres = sis_pointer_list_create();
	context->readeres->vfree = zipdb_reader_destroy;

    s_sis_json_node *wlognode = sis_json_cmp_child_node(node, "wlog");
    if (wlognode)
    {
		// 表示需要加载wlog的数据
        s_sis_worker *service = sis_worker_create(worker, wlognode);
        if (service)
        {
            context->wlog_worker = service; 
			context->wlog_method = sis_worker_get_method(context->wlog_worker, "write");
        }     
    }
    s_sis_json_node *wfilenode = sis_json_cmp_child_node(node, "wfile");
    if (wfilenode)
    {
		// 表示收到stop时需要从wlog中获取数据并转格式后存盘
        s_sis_worker *service = sis_worker_create(worker, wfilenode);
        if (service)
        {
            context->wfile_worker = service; 
        }     
    }
    s_sis_json_node *rfilenode = sis_json_cmp_child_node(node, "rfile");
    if (rfilenode)
    {
		// 表示支持历史数据的读取
        s_sis_worker *service = sis_worker_create(worker, rfilenode);
        if (service)
        {
            context->rfile_worker = service; 
        }     
    }
	if (context->wlog_worker)
	{
		// 先从目录中获取wlog中数据 并加载到内存中
		// 0 表示加载当前目录下有的文件
		int isload = zipdb_wlog_load(context);
		SIS_WAIT_LONG(context->wlog_load == 0);
		LOG(5)("load wlog ok. %d\n", context->wlog_load);
		// 加载成功后 wlog 文件会被重写 以此来保证数据前后的一致性
		// zipdb_wlog_move(context);
		//  如何保证磁盘的code索引和重启后索引保持一致 
		//  传入数据时不能清理 keys 和 sdbs 才能不出错
		// 然后启动一个读者 订阅 outputs 中数据 然后实时写盘
		context->wlog_reader = zipdb_reader_create();
		context->wlog_reader->zipdb_worker = worker;
		context->wlog_reader->isinit = 0;
		context->wlog_reader->ishead = isload ? 0 : 1;
		context->wlog_init = 0; 
		// 已经有了log文件就从头订阅 否则从尾部订阅
		context->wlog_reader->reader = sis_lock_reader_create(context->outputs, 
			context->wlog_reader->ishead ? SIS_UNLOCK_READER_HEAD : SIS_UNLOCK_READER_TAIL, 
			context->wlog_reader, cb_wlog_reader, NULL);
		sis_lock_reader_open(context->wlog_reader->reader);	
		 
	}
    return true;
}
void zipdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *zipdb = (s_zipdb_cxt *)worker->context;

    if (zipdb->wlog_worker)
    {
		zipdb_reader_destroy(zipdb->wlog_reader);
        sis_worker_destroy(zipdb->wlog_worker);
		zipdb->wlog_worker = NULL;
		sis_sdsfree(zipdb->wlog_keys);
		sis_sdsfree(zipdb->wlog_sdbs);
    }
    if (zipdb->wfile_worker)
    {
        sis_worker_destroy(zipdb->wfile_worker);
		zipdb->wfile_worker = NULL;
    }
    if (zipdb->rfile_worker)
    {
        sis_worker_destroy(zipdb->rfile_worker);
		zipdb->rfile_worker = NULL;
    }
	sis_sdsfree(zipdb->work_keys);
	sis_sdsfree(zipdb->work_sdbs);
    sis_map_list_destroy(zipdb->keys); 
    sis_map_list_destroy(zipdb->sdbs);

	if (zipdb->inputs)
	{
		sis_fast_queue_destroy(zipdb->inputs);
	}
	if (zipdb->cur_object)
	{
		sis_object_decr(zipdb->cur_object);
	}
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
bool _zipdb_write_init(s_zipdb_cxt *zipdb_, int workdate_, s_sis_sds keys_, s_sis_sds sdbs_)
{
	if (!keys_ || sis_sdslen(keys_) < 3 || !sdbs_ || sis_sdslen(sdbs_) < 3)
	{
		return false;
	}
	LOG(8)("_zipdb_write_init : %d %d %d\n", workdate_, (int)sis_sdslen(keys_), (int)sis_sdslen(sdbs_));
	// 这里为了保证二次进入 必须对压缩参数初始化
	if (zipdb_->inputs)
	{
		sis_fast_queue_clear(zipdb_->inputs);
	}
	zipdb_->last_object = NULL;
	if (zipdb_->cur_object)
	{
		sis_object_decr(zipdb_->cur_object);
		zipdb_->cur_object = NULL;
	}
	sis_bits_stream_clear(zipdb_->cur_sbits);

	zipdb_->inited = true;
	zipdb_->stoped = false;

	// 可能因为断线重连等原因 如果其他信息一致 就不需要执行以下代码
	if (zipdb_->work_date == workdate_ && 
		zipdb_->work_keys && zipdb_->work_sdbs &&
		!sis_strcasecmp(zipdb_->work_keys, keys_) &&
		!sis_strcasecmp(zipdb_->work_sdbs, sdbs_)) 
	{
		return true;
	}
	LOG(8)("_zipdb_write_init : new. %d %d\n", zipdb_->work_date ,workdate_);

	// 有一个信息不匹配就全部重新初始化
	zipdb_->work_date = workdate_;
	sis_sdsfree(zipdb_->work_keys); 
	zipdb_->work_keys = sis_sdsdup(keys_);
	sis_sdsfree(zipdb_->work_sdbs); 
	zipdb_->work_sdbs = sis_sdsdup(sdbs_);
	sis_map_list_clear(zipdb_->keys);
	sis_map_list_clear(zipdb_->sdbs);	
	// 压缩参数初始化完成 ///
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
		sis_json_close(injson);
	}
	if (zipdb_->wlog_reader)
	{
		zipdb_->wlog_reader->isinit = 0;
		zipdb_->wlog_init = 0;
	}
	return true;
}
// 因为可能发到中间时也会调用该函数 init 时需要保证环境一致
int cmd_zipdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

	int work_date = sis_message_get_int(msg, "work_date");
	s_sis_sds keys = sis_message_get_str(msg, "keys");
	s_sis_sds sdbs = sis_message_get_str(msg, "sdbs");
	
	// 从外界过来的sdbs可能格式不对，需要转换
	s_sis_json_handle *injson = sis_json_load(sdbs, sis_sdslen(sdbs));
	if (!injson)
	{
		return SIS_METHOD_ERROR;
	}
	sdbs = sis_json_to_sds(injson->node, true);
	sis_json_close(injson);

    if (_zipdb_write_init(context, work_date, keys, sdbs))
    {
		sis_sdsfree(sdbs);
        return SIS_METHOD_OK;
    }
	sis_sdsfree(sdbs);
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
	if (context->wlog_worker)
	{
		int o = zipdb_wlog_save_snos(context);
		if (o)
		{
			// 等待数据存盘完毕
			SIS_WAIT_LONG(context->wfile_save == 0);
			// 存盘结束清理wlog
			zipdb_wlog_move(context);
		}
	}
    return SIS_METHOD_OK;
}

int _zipdb_write(s_zipdb_cxt *zipdb_, int kidx_, int sidx_, void *in_, size_t ilen_)
{
	if (!zipdb_->inited || zipdb_->stoped || kidx_ < 0 || sidx_ < 0)
	{
		return -1;
	}
	if (zipdb_->isinput == 0)
	{
		zipdb_->inputs = sis_fast_queue_create(zipdb_, cb_input_reader, 30, zipdb_->wait_msec);
		zipdb_->isinput = 1;
	}
	s_sis_memory *memory = sis_memory_create();
	sis_memory_cat_byte(memory, kidx_, 4);
	sis_memory_cat_byte(memory, sidx_, 2);
	sis_memory_cat(memory, (char *)in_, ilen_);	
	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, memory);
	// printf("-- input -- kidx_= %d, sidx_= %d\n", kidx_, sidx_);
	sis_fast_queue_push(zipdb_->inputs, obj);
	
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
	size_t size = sizeof(s_zipdb_bits) + in_->size;
	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create_size(size));
	s_zipdb_bits *memory = MAP_ZIPDB_BITS(obj);
	memmove(memory, in_, size);
	sis_memory_set_size(SIS_OBJ_MEMORY(obj), size);
	// printf("_zipdb_write_bits = %d %d | %d %d\n", zipdb_->inited , zipdb_->stoped, memory->init,memory->size);
	if (memory->init == 1)
	{
		zipdb_->last_object = obj;
		// printf("set last obj = %p\n", obj);
	}
	// printf("push 3 outmem->size = %d\n", MAP_ZIPDB_BITS(obj)->size);
	sis_lock_list_push(zipdb_->outputs, obj);
	if ((zipdb_->zipnums++) % 100 == 0)
	{
		LOG(8)("zpub nums = %d\n", zipdb_->zipnums);
	}
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

	zipdb_reader_move(context, cid);

    return SIS_METHOD_OK;
}
void _zipdb_clear_data(s_zipdb_cxt *context)
{
	context->last_object = NULL;
	if (context->cur_object)
	{
		sis_object_decr(context->cur_object);
		context->cur_object = NULL;
	}
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
	
	context->wlog_method->proc(context->wlog_worker, netmsg);

	return SIS_METHOD_OK;
}
int cmd_zipdb_rlog(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;

	if(zipdb_wlog_load(context))
	{
		return SIS_METHOD_OK;
	}
	return SIS_METHOD_ERROR;
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

void unzipdb_reader_clear(s_unzipdb_reader *unzipdb_)
{
	sis_map_list_clear(unzipdb_->keys);
	sis_map_list_clear(unzipdb_->sdbs);
	sis_bits_stream_clear(unzipdb_->cur_sbits);
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
int _unzip_nums = 0;
msec_t _unzip_msec = 0;
int _unzip_recs = 0;
msec_t _unzip_usec = 0;

void unzipdb_reader_set_bits(s_unzipdb_reader *unzipdb_, s_zipdb_bits *in_)
{
	if (in_->init == 1)
	{
		printf("unzip init = 1 : %d\n", unzipdb_->cur_sbits->inited);
		// 这里memset时报过错
		sis_bits_struct_flush(unzipdb_->cur_sbits);
		sis_bits_struct_link(unzipdb_->cur_sbits, in_->data, in_->size);	
	}
	else
	{
		sis_bits_struct_link(unzipdb_->cur_sbits, in_->data, in_->size);
	}
	msec_t _start_usec = sis_time_get_now_usec();
	// 开始解压 并回调
	// int nums = sis_bits_struct_decode(unzipdb_->cur_sbits, NULL, NULL);
	int nums = sis_bits_struct_decode(unzipdb_->cur_sbits, unzipdb_->cb_source, unzipdb_->cb_read);
	if (nums == 0)
	{
		LOG(5)("unzip fail.\n");
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
	
	// if (reader->unzip_reader)
	// {
	// 	unzipdb_reader_destroy(reader->unzip_reader);	
	// }
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

	zipdb_reader_move(zipdb, reader_->cid);
	if (zipdb->inited)  // 如果已经初始化就启动发送数据 否则就等待系统通知后再发送
	{
		zipdb_sub_start(reader_);
	}
	sis_pointer_list_push(zipdb->readeres, reader_);
	return 1;
}
int zipdb_reader_move(s_zipdb_cxt *zipdb_,int cid_)
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
			reader->ishead ? SIS_UNLOCK_READER_HEAD : SIS_UNLOCK_READER_TAIL, 
			reader, cb_output_reader, cb_output_realtime);
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

//////////////////////////////////////////////////////////////////