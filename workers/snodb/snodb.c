
#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"
#include "sis_utils.h"

#include <snodb.h>
#include <sis_net.msg.h>

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////
// NULL 默认为读取
struct s_sis_method snodb_methods[] = {
// 网络结构体
    {"open",      cmd_snodb_open,   SIS_METHOD_ACCESS_NONET, NULL},  // 
    {"set",       cmd_snodb_set,    SIS_METHOD_ACCESS_RDWR, NULL},   // 
    {"start",     cmd_snodb_start,  SIS_METHOD_ACCESS_RDWR, NULL},   // 开始发送数据 
    {"stop",      cmd_snodb_stop,   SIS_METHOD_ACCESS_RDWR, NULL},   // 数据流发布完成 此时不再接收数据
    {"ipub",      cmd_snodb_ipub,   SIS_METHOD_ACCESS_NONET, NULL},  // 发布数据流 单条数据 kidx+sidx+data
    {"spub",      cmd_snodb_spub,   SIS_METHOD_ACCESS_RDWR, NULL},   // 发布数据流 单条数据 key sdb data
    {"zpub",      cmd_snodb_zpub,   SIS_METHOD_ACCESS_NONET, NULL},  // 发布数据流 只有完成的压缩数据块
    {"zsub",      cmd_snodb_zsub,   SIS_METHOD_ACCESS_READ, NULL},   // 订阅压缩数据流 
    {"sub",       cmd_snodb_sub,    SIS_METHOD_ACCESS_READ, NULL},   // 订阅数据流 
    {"unsub",     cmd_snodb_unsub,  SIS_METHOD_ACCESS_READ, NULL},   // 取消订阅数据流 
    {"get",       cmd_snodb_get,    SIS_METHOD_ACCESS_READ, NULL},   // 订阅数据流 
    {"clear",     cmd_snodb_clear,  SIS_METHOD_ACCESS_NONET, NULL},  // 清理数据流 
// 磁盘工具
    // {"wlog",      cmd_snodb_wlog,  0, NULL},  // 接收数据实时写盘
    {"rlog",      cmd_snodb_rlog,  0, NULL},  // 异常退出时加载磁盘数据
    // {"wsno",      cmd_snodb_wsno,  0, NULL},  // 盘后转压缩格式
    // {"rsno",      cmd_snodb_rsno,  0, NULL},  // 从历史数据中获取数据 
    {"getdb",     cmd_snodb_getdb ,   SIS_METHOD_ACCESS_NONET, NULL},   // 得到表
};
// 共享内存数据库
s_sis_modules sis_modules_snodb = {
    snodb_init,
    NULL,
    NULL,
    NULL,
    snodb_uninit,
    snodb_method_init,
    snodb_method_uninit,
    sizeof(snodb_methods) / sizeof(s_sis_method),
    snodb_methods,
};

//////////////////////////////////////////////////////////////////
//------------------------snodb --------------------------------//
//////////////////////////////////////////////////////////////////
// snodb 必须先收到 init 再收到 start 顺序不能错
///////////////////////////////////////////////////////////////////////////
//------------------------s_snodb_cxt --------------------------------//
///////////////////////////////////////////////////////////////////////////

static void cb_output_reader_send(s_snodb_reader *reader, s_snodb_compress *memory)
{
	// printf("cb_output_reader_send = %d : %d\n", reader->sub_whole, memory->init);
	if (reader->sub_whole)
	{
		// 订阅全部就直接发送
		if (reader->cb_sub_inctzip)
		{	
			reader->cb_sub_inctzip(reader, memory);
		}
	}
	else
	{
		// 订阅不是全部直接送到unzip中
		snodb_worker_unzip_set(reader->sub_unziper, memory);
	}
}
static int cb_output_reader(void *reader_, s_sis_object *in_)
{
	s_snodb_reader *reader = (s_snodb_reader *)reader_;
	s_snodb_compress *memory = MAP_SNODB_BITS(in_);

	s_snodb_cxt *snodb = ((s_sis_worker *)reader->snodb_worker)->context;
	// printf("cb_output_reader = %d\n",reader->isinit);
	if (!reader->isinit)
	{
		if (!memory->init)
		{
			return 0;
		}
		if (reader->ishead)
		{
			reader->isinit = true;
			cb_output_reader_send(reader, memory);
		}
		else
		{
			// 当前包是最新的起始包 就开始发送数据
			// printf("last = %p  %p %d | %d\n", snodb->last_object, in_, snodb->last_object == in_, snodb->outputs->work_queue->rnums);
			if (snodb->last_object == in_)
			{
				reader->isinit = true;
				cb_output_reader_send(reader, memory);
			}
		}		
	}
	else
	{
		cb_output_reader_send(reader, memory);
	}	
	return 0;
}

static int cb_output_realtime(void *reader_)
{
	s_snodb_reader *reader = (s_snodb_reader *)reader_;
	s_snodb_cxt *snodb = ((s_sis_worker *)reader->snodb_worker)->context;

	char sdate[32];
	sis_llutoa(snodb->work_date, sdate, 32, 10);
	if(!snodb->stoped)
	{
		if (reader->cb_sub_realtime)
		{
			reader->cb_sub_realtime(reader, sdate);
		}
	}
	else
	{
		if (reader->cb_sub_stop)
		{
			reader->cb_sub_stop(reader, sdate);
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

s_sis_object *_snodb_new_data(s_snodb_cxt *snodb, int isinit)
{
	size_t size = sizeof(s_snodb_compress) + snodb->zip_size + 1024; // 这里应该取最长的结构体长度
	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create_size(size));
	s_snodb_compress *inmem = MAP_SNODB_BITS(obj);
	if (isinit || snodb->cur_size > snodb->initsize)
	{
		inmem->init = 1;
		// printf("new last obj = %p\n", obj);
		snodb->last_object = obj;
		snodb->cur_size = size;
		sis_bits_struct_flush(snodb->cur_sbits);
		sis_bits_struct_link(snodb->cur_sbits, inmem->data, size);				
	}
	else
	{
		inmem->init = 0;
		snodb->cur_size += size;
		sis_bits_struct_link(snodb->cur_sbits, inmem->data, size);		
	}	
	inmem->size = 0;
	memset(inmem->data, 0, snodb->zip_size + 1024);
	return obj;
}

static int cb_input_reader(void *snodb_, s_sis_object *in_)
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
	s_snodb_cxt *snodb = (s_snodb_cxt *)snodb_;
	if (!snodb->inited)
	{
		// 必须有这个判断 否则正在初始化时 会收到 in==NULL的数据包 
		LOG(5)("snodb no init. %d %d %p\n", snodb->inited, snodb->work_date, in_);
		return -1;
	}
	if (!snodb->cur_object)
	{
		snodb->cur_object = _snodb_new_data(snodb, 1);
		LOG(5)("snodb new obj %d %d.\n", snodb->zip_size, snodb->cur_sbits->bit_maxsize);
	}
	s_sis_object *obj = snodb->cur_object; 
	s_snodb_compress *outmem = MAP_SNODB_BITS(obj);

	// printf("--input %d %d %p\n", snodb->inputs->rnums, outmem->size, in_);

	if (!in_) // 为空表示无数据超时
	{
		if (outmem->size > 1)
		{
			// printf("push 0 outmem->size = %d\n", outmem->size);
			sis_memory_set_size(SIS_OBJ_MEMORY(obj), sizeof(s_snodb_compress) + outmem->size);
			sis_lock_list_push(snodb->outputs, obj);
			// isnew = true;
			// _zip_nums++;
			sis_object_decr(snodb->cur_object);
			snodb->cur_object = _snodb_new_data(snodb, 0);
		}
	}
	else
	{
		s_sis_memory *inmem = SIS_OBJ_MEMORY(in_);
		size_t offset = sis_memory_getpos(inmem);

		int kidx = sis_memory_get_byte(inmem, 4);
		int sidx = sis_memory_get_byte(inmem, 2);
		// printf("%p %p %p |%d %d %d\n",snodb->cur_sbits, snodb->cur_object, snodb->last_object, snodb->cur_sbits->inited, kidx, sidx);
		int o = sis_bits_struct_encode(snodb->cur_sbits, kidx, sidx, sis_memory(inmem), sis_memory_get_size(inmem));
		if (o < 0)
		{
			LOG(5)("snodb zip fail %d.\n", o);
		}
		sis_memory_setpos(inmem, offset);
		outmem->size = sis_bits_struct_getsize(snodb->cur_sbits);
		//  数据如果超过一定数量就直接发送
		if ((int)outmem->size > snodb->zip_size)
		{
			sis_memory_set_size(SIS_OBJ_MEMORY(obj), sizeof(s_snodb_compress) + outmem->size);
			// printf("push 1 outmem->size  = %d num= %d\n", outmem->size, sis_bits_struct_get_bags(snodb->cur_sbits, false));
			sis_lock_list_push(snodb->outputs, obj);
			// isnew = true;
			sis_object_decr(snodb->cur_object);
			snodb->cur_object = _snodb_new_data(snodb, 0);
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
	s_snodb_reader *reader = (s_snodb_reader *)reader_;
	s_snodb_cxt *snodb = ((s_sis_worker *)reader->snodb_worker)->context;
	s_snodb_compress *memory = MAP_SNODB_BITS(in_);
	// 从第一个初始包开始存盘
	if ((snodb->zipnums++) % 100 == 0)
	{
		LOG(8)("recv nums = %d %d %d\n", reader->isinit, memory->init, snodb->wlog_init);
	}
	if (!reader->isinit)
	{
		if (!memory->init)
		{
			return 0;
		}
		reader->isinit = true;
	}
	// 无论是否有历史数据，都从新写一份keys和sdbs读取的时候也要更新dict表 以免数据混乱
	if(snodb->wlog_init == 0)
	{
		snodb_wlog_start(snodb);
		snodb_wlog_save(snodb, SDCDB_FILE_SIGN_KEYS, NULL);
		snodb_wlog_save(snodb, SDCDB_FILE_SIGN_SDBS, NULL);
		snodb->wlog_init = 1;
	}
	snodb_wlog_save(snodb, SDCDB_FILE_SIGN_ZPUB, memory);
	return 0;
}

bool snodb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
	s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_snodb_cxt *context = SIS_MALLOC(s_snodb_cxt, context);
    worker->context = context;
	context->dbname = sis_sdsnew(node->key);

    context->work_date = sis_time_get_idate(0);

	// context->initmsec = 600*1000;
	context->initsize = 4*1024*1024;  // 4M 大约20秒 单只股票一天所有数据也足够一次传完
	context->cur_size = 0; 

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

    s_sis_json_node *sdbsnode = sis_json_cmp_child_node(node, "sdbs");
    if (sdbsnode)
	{
		context->work_sdbs = sis_json_to_sds(sdbsnode, true);
		// printf("%s\n", context->work_sdbs);
	}
	// sis_mutex_init(&(context->write_lock), NULL);
 
	context->outputs = sis_lock_list_create(context->catch_size);
	// printf("context->outputs save = %d\n", context->outputs->save_mode);
	context->cur_sbits = sis_bits_stream_v0_create(NULL, 0);

    context->keys = sis_map_list_create(sis_sdsfree_call); 
	context->sdbs = sis_map_list_create(sis_dynamic_db_destroy);

	// 每个读者都自行压缩数据 并把压缩的数据回调出去
	context->readeres = sis_pointer_list_create();
	context->readeres->vfree = snodb_reader_destroy;

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
		context->rfile_config = sis_json_clone(rfilenode, 1);
		// s_sis_json_node *sonnode = sis_json_cmp_child_node(context->rfile_config, "work-path");
		// if (!sonnode)
		// {
		// 	sis_json_object_add_string(context->rfile_config, "work-path", "data/", context->dbname);
		// }
    }

	if (context->wlog_worker)
	{
		// 先从目录中获取wlog中数据 并加载到内存中
		// 0 表示加载当前目录下有的文件
		int isload = snodb_wlog_load(context);
		SIS_WAIT_LONG(context->wlog_load == 0);
		LOG(5)("load wlog ok. %d\n", context->wlog_load);

		// 加载成功后 wlog 文件会被重写 以此来保证数据前后的一致性
		// snodb_wlog_move(context);
		//  如何保证磁盘的code索引和重启后索引保持一致 
		//  传入数据时不能清理 keys 和 sdbs 才能不出错
		// 然后启动一个读者 订阅 outputs 中数据 然后实时写盘
		context->wlog_reader = snodb_reader_create();
		context->wlog_reader->snodb_worker = worker;
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
void snodb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *snodb = (s_snodb_cxt *)worker->context;

    if (snodb->wlog_worker)
    {
		snodb_reader_destroy(snodb->wlog_reader);
        sis_worker_destroy(snodb->wlog_worker);
		snodb->wlog_worker = NULL;
		sis_sdsfree(snodb->wlog_keys);
		sis_sdsfree(snodb->wlog_sdbs);
    }
    if (snodb->wfile_worker)
    {
        sis_worker_destroy(snodb->wfile_worker);
		snodb->wfile_worker = NULL;
    }
    if (snodb->rfile_config)
    {
		sis_json_delete_node(snodb->rfile_config);
		snodb->rfile_config = NULL;
    }
	sis_sdsfree(snodb->work_keys);
	sis_sdsfree(snodb->work_sdbs);

	sis_sdsfree(snodb->init_keys);
	sis_sdsfree(snodb->init_sdbs);

    sis_map_list_destroy(snodb->keys); 
    sis_map_list_destroy(snodb->sdbs);

	if (snodb->inputs)
	{
		sis_fast_queue_destroy(snodb->inputs);
	}
	if (snodb->cur_object)
	{
		sis_object_decr(snodb->cur_object);
	}
	sis_bits_stream_v0_destroy(snodb->cur_sbits);

	sis_pointer_list_destroy(snodb->readeres);
	
	sis_lock_list_destroy(snodb->outputs);

	sis_sdsfree(snodb->dbname);
	sis_free(snodb);
}

void snodb_method_init(void *worker_)
{
    // 加载数据
}
void snodb_method_uninit(void *worker_)
{
    // 释放数据
}

// 设置
bool _snodb_write_init(s_snodb_cxt *snodb_, int workdate_, s_sis_sds keys_, s_sis_sds sdbs_)
{
	snodb_->inited = false;
	snodb_->stoped = false;
	LOG(8)("_snodb_write_init : %d \n", workdate_);

	snodb_->work_date = workdate_;
	if (keys_)
	{
		sis_sdsfree(snodb_->work_keys); 
		snodb_->work_keys = sis_sdsdup(keys_);
	}
	if (sdbs_)
	{
		sis_sdsfree(snodb_->work_sdbs); 
		snodb_->work_sdbs = sis_sdsdup(sdbs_);
	}
	sis_map_list_clear(snodb_->keys);
	sis_map_list_clear(snodb_->sdbs);	
	// 压缩参数初始化完成 ///
	if (!snodb_->work_keys || sis_sdslen(snodb_->work_keys) < 3 
		|| !snodb_->work_sdbs || sis_sdslen(snodb_->work_sdbs) < 3)
	{
		return false;
	}
	// 这里为了保证二次进入 必须对压缩参数初始化
	if (snodb_->inputs)
	{
		LOG(5)("inputs: - %d %d\n", snodb_->inputs->rnums, snodb_->inputs->wnums);
		sis_fast_queue_clear(snodb_->inputs);
		LOG(5)("inputs: - %d %d\n", snodb_->inputs->rnums, snodb_->inputs->wnums);
	}
	snodb_->last_object = NULL;
	if (snodb_->cur_object)
	{
		sis_object_decr(snodb_->cur_object);
		snodb_->cur_object = NULL;
	}
	// sis_bits_stream_v0_clear(snodb_->cur_sbits);
	snodb_->cur_size = 0;

	// 清理输出缓存中可能存在的数据
	sis_lock_list_clear(snodb_->outputs);

	// 可能因为断线重连等原因 如果其他信息一致 就不需要执行以下代码
	// if (snodb_->work_date == workdate_ && 
	// 	snodb_->work_keys && snodb_->work_sdbs &&
	// 	!sis_strcasecmp(snodb_->work_keys, keys_) &&
	// 	!sis_strcasecmp(snodb_->work_sdbs, sdbs_)) 
	// {
	// 	return true;
	// }
	LOG(8)("_snodb_write_init : new. %d %d | %d\n", snodb_->work_date ,workdate_, snodb_->outputs->work_queue->rnums);
	sis_bits_stream_v0_clear(snodb_->cur_sbits);
	// 有一个信息不匹配就全部重新初始化
	{
		s_sis_string_list *klist = sis_string_list_create();
		sis_string_list_load(klist, snodb_->work_keys, sis_sdslen(snodb_->work_keys), ",");
		// 重新设置keys
		int count = sis_string_list_getsize(klist);
		if (count < 1)
		{
			return false;
		}
		for (int i = 0; i < count; i++)
		{
			s_sis_sds key = sis_sdsnew(sis_string_list_get(klist, i));
			sis_map_list_set(snodb_->keys, key, key);	
		}
		LOG(8)("sis_bits_struct_set_key . %d\n", count);
    	sis_bits_struct_set_key(snodb_->cur_sbits, count);
		sis_string_list_destroy(klist);
	}
	{
		s_sis_json_handle *injson = sis_json_load(snodb_->work_sdbs, sis_sdslen(snodb_->work_sdbs));
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
				sis_map_list_set(snodb_->sdbs, innode->key, sdb);
				sis_bits_struct_set_sdb(snodb_->cur_sbits, sdb);
			}
			innode = sis_json_next_node(innode);
		}
		sis_json_close(injson);
	}
	// 这里要设置
	
	// sis_bits_struct_set_zipcb(snodb_->cur_sbits, snodb_, cb_zip_complete);
	if (snodb_->wlog_reader && snodb_->wlog_date != workdate_)
	{
		// log日期不同就重新生成文件
		snodb_->wlog_reader->isinit = 0;
		snodb_->wlog_init = 0;
	}
	LOG(5)("old obj = %p.\n", snodb_->cur_object);
	snodb_->inited = true;
	return true;
}
int cmd_snodb_set(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

	if (!sis_strcasecmp(netmsg->key, "_keys_") && netmsg->ask)
	{
		sis_sdsfree(context->init_keys);
		context->init_keys = sis_sdsdup(netmsg->ask);
	}
	if (!sis_strcasecmp(netmsg->key, "_sdbs_") && netmsg->ask)
	{
		sis_sdsfree(context->init_sdbs);
		// 从外界过来的sdbs可能格式不对，需要转换
		s_sis_json_handle *injson = sis_json_load(netmsg->ask, sis_sdslen(netmsg->ask));
		if (!injson)
		{
			return SIS_METHOD_ERROR;
		}
		context->init_sdbs = sis_json_to_sds(injson->node, true);
		sis_json_close(injson);
	}
    return SIS_METHOD_OK;
}
// 因为可能发到中间时也会调用该函数 init 时需要保证环境和最初的环境一致
int cmd_snodb_open(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
    
	s_sis_message *msg = (s_sis_message *)argv_;
	context->cb_source = sis_message_get(msg, "cb_source");
	context->cb_net_message = sis_message_get_method(msg, "cb_net_message");
    return SIS_METHOD_OK;
}
int cmd_snodb_start(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	// 这里的keys和sdbs必须为真实值
	if (!netmsg->ask)
	{
		return SIS_METHOD_ERROR;
	}
	int work_date = sis_atoll(netmsg->ask);
    if (!_snodb_write_init(context, work_date, context->init_keys, context->init_sdbs))
	{
		LOG(5)("snodb init fail.\n");
		return SIS_METHOD_ERROR;
	}
	sis_sdsfree(context->init_keys);
	sis_sdsfree(context->init_sdbs);
	context->init_keys = NULL;
	context->init_sdbs = NULL;

	// printf("cmd_snodb_start : %d\n", context->inited);

	if (!context->inited)
	{
		LOG(5)("snodb no init.\n");
		return SIS_METHOD_ERROR;
	}
	// 初始化后首先向所有订阅者发送订阅开始信息
	for (int i = 0; i < context->readeres->count; i++)
	{
		s_snodb_reader *reader = (s_snodb_reader *)sis_pointer_list_get(context->readeres, i);
		if (reader->sub_disk)
		{
			continue;
		}
		snodb_reader_realtime_start(reader);
		LOG(5)("snodb start. [%d] cid : %d disk: %d workdate : %d = %s\n", 
			i,  reader->cid, reader->sub_disk, context->work_date, netmsg->ask);
	}
	return SIS_METHOD_OK;
}
int cmd_snodb_stop(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	// printf("cmd_snodb_stop, curmemory : %d\n", context->cur_object->size);
	
	LOG(5)("snodb stop. wlog : %d readers : %d workdate : %d = %s\n", 
		context->wlog_worker, context->readeres->count, context->work_date, netmsg->ask);
	for (int i = 0; i < context->readeres->count; i++)
	{
		s_snodb_reader *reader = (s_snodb_reader *)sis_pointer_list_get(context->readeres, i);
		if (reader->sub_disk)
		{
			continue;
		}
		LOG(5)("snodb stop. [%d] cid : %d workdate : %d = %s\n", 
			i,  reader->cid, context->work_date, netmsg->ask);
		snodb_reader_realtime_stop(reader);
	}
	if (context->wlog_worker)
	{
		// 停止写wlog文件
		snodb_wlog_stop(context);
		// 转格式
		if (context->wfile_worker)
		{
			if (snodb_wlog_save_snos(context))
			{
				// 等待数据存盘完毕
				SIS_WAIT_LONG(context->wfile_save == 0);
				// 存盘结束清理 wlog
				snodb_wlog_move(context);
			}
			// 数据转错 就不删除
		}
		else
		{
			snodb_wlog_move(context);
		}
	}
	context->stoped = true; // 停止表示当日数据已经落盘
	context->zipnums = 0;   // 重新计数
    return SIS_METHOD_OK;
}

int _snodb_write(s_snodb_cxt *snodb_, int kidx_, int sidx_, void *in_, size_t ilen_)
{
	if (!snodb_->inited || snodb_->stoped || kidx_ < 0 || sidx_ < 0)
	{
		return -1;
	}
	if (!snodb_->inputs)
	{
		snodb_->inputs = sis_fast_queue_create(snodb_, cb_input_reader, 30, snodb_->wait_msec);
	}
	s_sis_memory *memory = sis_memory_create();
	sis_memory_cat_byte(memory, kidx_, 4);
	sis_memory_cat_byte(memory, sidx_, 2);
	sis_memory_cat(memory, (char *)in_, ilen_);	
	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, memory);
	// printf("-- input -- kidx_= %d, sidx_= %d\n", kidx_, sidx_);
	sis_fast_queue_push(snodb_->inputs, obj);
	
	sis_object_destroy(obj);
	return 1;
}

int _snodb_write_bits(s_snodb_cxt *snodb_, s_snodb_compress *in_)
{
	if (!snodb_->inited || snodb_->stoped)
	{
		return -1;
	}
	// 直接写入
	size_t size = sizeof(s_snodb_compress) + in_->size;
	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create_size(size));
	s_snodb_compress *memory = MAP_SNODB_BITS(obj);
	memmove(memory, in_, size);
	sis_memory_set_size(SIS_OBJ_MEMORY(obj), size);
	// printf("_snodb_write_bits = %d %d | %d %d\n", snodb_->inited , snodb_->stoped, memory->init,memory->size);
	if (memory->init == 1)
	{
		snodb_->last_object = obj;
		// printf("set last obj = %p\n", obj);
	}
	// printf("push 3 outmem->size = %d\n", MAP_SNODB_BITS(obj)->size);
	sis_lock_list_push(snodb_->outputs, obj);
	if ((snodb_->zipnums++) % 100 == 0)
	{
		LOG(8)("zpub nums = %d %d\n", snodb_->zipnums, snodb_->outputs->users->count);
	}
	sis_object_destroy(obj);
	return 0;
}

// #include "stk_struct.v0.h"
int cmd_snodb_ipub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
    s_snodb_bytes *in = (s_snodb_bytes *)argv_;
	// if (in->size == 224)
	// {
	// 	s_v3_stk_snapshot *input = (s_v3_stk_snapshot *)in->data;
	// 	printf("cmd_snodb_ipub: %d %d %d time = %lld\n",in->kidx, in->sidx, in->size, sis_time_get_itime(input->time/1000));
	// }
	// printf("---1 %d %d\n", in->kidx, in->sidx);
    if (_snodb_write(context, in->kidx, in->sidx, in->data, in->size) >= 0)
    {
        return SIS_METHOD_OK;
    }
    return SIS_METHOD_ERROR;
}
int cmd_snodb_spub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	char kname[128]; 
	char sname[128]; 
	int  cmds = sis_str_divide(netmsg->key, '.', kname, sname);
	s_sis_sds inmem = sis_net_get_argvs(netmsg, 0);
	if (cmds == 2 && inmem)
	{
		int kidx = sis_map_list_get_index(context->keys, kname);
		int sidx = sis_map_list_get_index(context->sdbs, sname); 

		if (_snodb_write(context, kidx, sidx, inmem, sis_sdslen(inmem)) >= 0)
		{
			return SIS_METHOD_OK;
		}
	}
    return SIS_METHOD_ERROR;
}
// 从这里写入的数据为索引+二进制数据
int cmd_snodb_zpub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
    s_snodb_compress *in = (s_snodb_compress *)argv_;

    if (_snodb_write_bits(context, in) >= 0)
    {
        return SIS_METHOD_OK;
    }
    return SIS_METHOD_ERROR;
}

static int cb_sub_inctzip(void *source, void *argv);
static int cb_sub_chars(void *source, void *argv);
static int cb_sub_start(void *source, void *argv);
static int cb_sub_realtime(void *source, void *argv);
static int cb_sub_stop(void *source, void *argv);
static int cb_dict_keys(void *source, void *argv);
static int cb_dict_sdbs(void *source, void *argv);

int _snodb_sub(s_sis_worker *worker, s_sis_net_message *netmsg, bool iszip)
{
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;

	s_snodb_reader *reader = snodb_reader_create();
	reader->iszip = iszip;
	reader->cid = netmsg->cid;
	if (netmsg->name)
	{
		reader->serial = sis_sdsdup(netmsg->name);
	}
	reader->rfmt = SISDB_FORMAT_JSON;
    reader->snodb_worker = worker;
    reader->ishead = 0;
	reader->sub_date = context->work_date;
    if (netmsg->ask)
    {
        s_sis_json_handle *argnode = sis_json_load(netmsg->ask, sis_sdslen(netmsg->ask));
        if (argnode)
        {
			reader->rfmt = sis_db_get_format_from_node(argnode->node, reader->rfmt);
            reader->ishead = sis_json_get_int(argnode->node, "ishead", 0);
            reader->sub_date = sis_json_get_int(argnode->node, "date", context->work_date);
			// sis_json_get_int(argnode->node, "sub-date", context->work_date);
            sis_json_close(argnode);
        }
	}
	if (reader->iszip)
	{
		reader->cb_sub_inctzip = cb_sub_inctzip;
	}
	else
	{
		reader->cb_sub_chars 	= cb_sub_chars;
	}
	reader->cb_sub_start 		= cb_sub_start;
	reader->cb_sub_realtime 	= cb_sub_realtime;  // 当日
	reader->cb_sub_stop 		= cb_sub_stop;
	reader->cb_dict_keys 		= cb_dict_keys;
	reader->cb_dict_sdbs 		= cb_dict_sdbs;
	
    sis_str_divide_sds(netmsg->key, '.', &reader->sub_keys, &reader->sub_sdbs);
	if (!reader->sub_keys || !reader->sub_sdbs || 
		sis_sdslen(reader->sub_keys) < 1 || sis_sdslen(reader->sub_sdbs) < 1)
	{
		LOG(5)("format error. %s\n", netmsg->key);
		snodb_reader_destroy(reader);
		return SIS_METHOD_ERROR;
	}
	if (!sis_strcasecmp(reader->sub_keys, "*") && !sis_strcasecmp(reader->sub_sdbs, "*"))
	{
		reader->sub_whole = true;
	}
	else
	{
		reader->sub_whole = false;
	}
	printf("%s %d %d | %d %d\n", __func__, reader->sub_date ,context->work_date, context->stoped , context->inited);
	if (reader->sub_date < context->work_date ||  
		// (reader->sub_date == context->work_date && context->stoped && context->inited))
		(reader->sub_date == context->work_date && context->stoped))
	{
		reader->sub_disk = true;
		// 启动历史数据线程 并返回 
		// 如果断线要能及时中断文件读取 
		// 同一个用户 必须等待上一次读取中断后才能开始新的任务
		if (!snodb_reader_new_history(reader))
		{
			// 没有服务或其他原因
			if (reader->cb_sub_stop)
			{
				char sdate[32];
				sis_llutoa(reader->sub_date, sdate, 32, 10);
				reader->cb_sub_stop(reader, sdate);
			}
			snodb_reader_destroy(reader);
		}
	}
	else // 大于当前日期 或者等于但未停止 
	{		
		reader->sub_disk = false;
		// 一个 socket 只能订阅一次 后订阅的会冲洗掉前面一次
		snodb_reader_new_realtime(reader);
	}
    return SIS_METHOD_OK;
}
int cmd_snodb_zsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	return _snodb_sub(worker, netmsg, true);
}
int cmd_snodb_sub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	return _snodb_sub(worker, netmsg, false);
}
int cmd_snodb_unsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

	snodb_move_reader(context, netmsg->cid);

    return SIS_METHOD_OK;
}
s_sis_object *snodb_snos_read_get(s_sis_json_node *config_, s_snodb_reader *reader_)
{
	// printf("%s\n", __func__);
	s_sis_worker *worker = sis_worker_create(NULL, config_);
	if (!worker)
	{
		sis_free(worker);
		return NULL;
	}

	s_sis_message *msg = sis_message_create();
    char sdate[32];   
    sis_llutoa(reader_->sub_date, sdate, 32, 10);

	sis_message_set_str(msg, "get-date", sdate, sis_strlen(sdate));
	sis_message_set_str(msg, "get-keys", reader_->sub_keys, sis_sdslen(reader_->sub_keys));
	sis_message_set_str(msg, "get-sdbs", reader_->sub_sdbs, sis_sdslen(reader_->sub_sdbs));

	s_sis_object *obj = NULL;
	if (sis_worker_command(worker, "get", msg) == SIS_METHOD_OK)
	{
		obj = sis_message_get(msg, "omem");
		if (reader_->rfmt & SISDB_FORMAT_CHARS)
		{
			s_sis_dynamic_db *diskdb = sis_message_get(msg, "diskdb");
			if (diskdb)
			{
				// 这里未来可以做格式转换处理
				s_sis_sds omem = sis_db_format_sds(diskdb, NULL, reader_->rfmt, SIS_OBJ_GET_CHAR(obj),SIS_OBJ_GET_SIZE(obj), 0);
				obj = sis_object_create(SIS_OBJECT_SDS, omem);
			}
			else
			{
				obj = NULL;
			}			
		}
		else
		{
			sis_object_incr(obj);		
		}
	}
	sis_worker_destroy(worker);
	sis_message_destroy(msg);
	return obj;
}
// 只支持字符串数据获取 只支持单个结构单个品种
int _snodb_get(s_sis_worker *worker, s_sis_net_message *netmsg, bool iszip)
{
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;

	s_snodb_reader *reader = snodb_reader_create();
	reader->iszip = iszip;
	reader->cid = netmsg->cid;
	if (netmsg->name)
	{
		reader->serial = sis_sdsdup(netmsg->name);
	}
	reader->rfmt = SISDB_FORMAT_JSON;
    reader->snodb_worker = worker;
    reader->ishead = 1;
	reader->sub_date = context->work_date;
    if (netmsg->ask)
    {
        s_sis_json_handle *argnode = sis_json_load(netmsg->ask, sis_sdslen(netmsg->ask));
        if (argnode)
        {
			reader->rfmt = sis_db_get_format_from_node(argnode->node, reader->rfmt);
            reader->ishead = sis_json_get_int(argnode->node, "ishead", 1);
            reader->sub_date = sis_json_get_int(argnode->node, "date", context->work_date);
            sis_json_close(argnode);
        }
	}
	reader->sub_whole = false;
    sis_str_divide_sds(netmsg->key, '.', &reader->sub_keys, &reader->sub_sdbs);
	if (!reader->sub_keys || !reader->sub_sdbs || 
		sis_sdslen(reader->sub_keys) < 1 || sis_sdslen(reader->sub_sdbs) < 1)
	{
		snodb_reader_destroy(reader);
		return SIS_METHOD_ERROR;
	}

	s_sis_object *obj = NULL;
	// printf("%s %d %d\n", __func__, reader->sub_date ,context->work_date);
	if (reader->sub_date != context->work_date || (context->stoped && context->inited))
	{
		reader->sub_disk = true;
		// 请求历史数据 
		obj = snodb_snos_read_get(context->rfile_config, reader);
	}
	else
	{		
		reader->sub_disk = false;
		// 从实时数据中获取对应数据
	}	
	if (!obj)
	{
		snodb_reader_destroy(reader);
		return SIS_METHOD_NIL;
	}
	if (reader->rfmt & SISDB_FORMAT_BYTES)
	{
		if (reader->iszip)
		{
			// 数据压缩后再发送
		}
		else
		{
			sis_net_ans_with_bytes(netmsg, SIS_OBJ_GET_CHAR(obj),SIS_OBJ_GET_SIZE(obj));    
		}	
	}
	else
	{
		sis_net_ans_with_chars(netmsg, SIS_OBJ_GET_CHAR(obj),SIS_OBJ_GET_SIZE(obj));
	}
	snodb_reader_destroy(reader);
	sis_object_destroy(obj);
	// s_sis_net_message *newinfo = sis_net_message_create();
	// newinfo->serial = reader->serial ? sis_sdsdup(reader->serial) : NULL;
	// newinfo->cid = reader->cid;
	// sis_message_set_key(newinfo, reader->sub_keys, reader->sub_sdbs);
	// if (reader->rfmt & SISDB_FORMAT_BYTES)
	// {
	// 	if (reader->iszip)
	// 	{
	// 		// 数据压缩后再发送
	// 	}
	// 	else
	// 	{
	// 		sis_net_ans_with_bytes(newinfo, SIS_OBJ_GET_CHAR(obj),SIS_OBJ_GET_SIZE(obj));    
	// 	}	
	// }
	// else
	// {
	// 	sis_net_ans_with_chars(newinfo, SIS_OBJ_GET_CHAR(obj),SIS_OBJ_GET_SIZE(obj));
	// }
	// if (context->cb_net_message)
	// {
	// 	context->cb_net_message(context->cb_source, newinfo);
	// }
	// sis_net_message_destroy(newinfo); 	
	// snodb_reader_destroy(reader);
	// sis_object_destroy(obj);
	// sis_net_ans_with_noreply(netmsg); // 这一句功能是不二次回复

	return SIS_METHOD_OK;
}
 
int cmd_snodb_get(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	// printf("%s %d\n",__func__, sis_strsub(netmsg->key, "*"));
	if (sis_strsub(netmsg->key, "*") >= 0)
	{
		// 不支持多键值获取
		return SIS_METHOD_ERROR;
	}	
	return _snodb_get(worker, netmsg, false);
}
void _snodb_clear_data(s_snodb_cxt *context)
{
	context->last_object = NULL;
	if (context->cur_object)
	{
		sis_object_decr(context->cur_object);
		context->cur_object = NULL;
	}
	sis_lock_list_clear(context->outputs);
	// 清理数据可能造成数据压缩的不连续 需要处理
}
int cmd_snodb_clear(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

	const char *mode = sis_message_get_str(msg, "mode");
	if (!sis_strcasecmp(mode, "reader"))
	{
		int cid  = sis_message_get_int(msg, "cid");
		if (cid == -1)
		{
			sis_pointer_list_clear(context->readeres);
		}
		else
		{
			snodb_move_reader(context, cid);
		}		
	}
	if (!sis_strcasecmp(mode, "catch"))
	{
		_snodb_clear_data(context);	
	}	
    return SIS_METHOD_OK;
}
	
// int cmd_snodb_wlog(void *worker_, void *argv_)
// {
//     s_sis_worker *worker = (s_sis_worker *)worker_; 
//     s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
// 	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	
// 	if (!snodb_->wlog_reader)
// 	{
// 		// 非压缩数据 这里有问题
// 		context->wlog_method->proc(context->wlog_worker, netmsg);
// 	}
// 	return SIS_METHOD_OK;
// }
int cmd_snodb_rlog(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;

	if(snodb_wlog_load(context))
	{
		return SIS_METHOD_OK;
	}
	return SIS_METHOD_ERROR;
}
// int cmd_snodb_wsno(void *worker_, void *argv_)
// {
// 	// 从log中获取数据后直接写盘
// 	return SIS_METHOD_OK;
// }
// int cmd_snodb_rsno(void *worker_, void *argv_)
// {
// 	return SIS_METHOD_OK;
// }

int cmd_snodb_getdb(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

	const char *dbname = sis_message_get_str(msg, "dbname");
	s_sis_dynamic_db *db = (s_sis_dynamic_db *)sis_map_list_get(context->sdbs, dbname);
    sis_message_set(msg, "db", db, NULL);
    return SIS_METHOD_OK;
}

///////////////////////////////////////////////////////////////////////////
//------------------------s_snodb_reader callback -----------------------//
///////////////////////////////////////////////////////////////////////////

// // 直接返回一个数据块 压缩数据
#define MSERVER_DEBUG
#ifdef MSERVER_DEBUG
static int _bitzip_nums = 0;
static int64 _bitzip_size = 0;
static msec_t _bitzip_msec = 0;
#endif
static int cb_sub_inctzip(void *source, void *argv)
{
    // printf("%s\n", __func__);
    s_snodb_reader *reader = (s_snodb_reader *)source;
    s_sis_worker *snodb_worker = (s_sis_worker *)reader->snodb_worker;
    s_snodb_cxt *context = (s_snodb_cxt *)snodb_worker->context;
    s_snodb_compress *inmem = (s_snodb_compress *)argv;
    // 向对应的socket发送数据
    s_sis_net_message *newinfo = sis_net_message_create();
    newinfo->name = reader->serial ? sis_sdsdup(reader->serial) : NULL;
    newinfo->cid = reader->cid;
#ifdef MSERVER_DEBUG
    _bitzip_nums++;
    _bitzip_size += sizeof(s_snodb_compress) + inmem->size;
    if (_bitzip_nums % 100 == 0)
    {
        LOG(8)("server send zip : %d %lld(k) cost :%lld\n", _bitzip_nums, _bitzip_size/1000, sis_time_get_now_msec() - _bitzip_msec);
        _bitzip_msec = sis_time_get_now_msec();
    } 
#endif
    sis_net_ans_with_bytes(newinfo, (const char *)inmem, sizeof(s_snodb_compress) + inmem->size);    
    if (context->cb_net_message)
	{
		context->cb_net_message(context->cb_source, newinfo);
	}
    sis_net_message_destroy(newinfo); 
    return SIS_METHOD_OK;
}

static int cb_sub_chars(void *source, void *argv)
{
    // printf("%s %d\n", __func__, reader->rfmt);
    s_snodb_reader *reader = (s_snodb_reader *)source;
    s_sis_worker *snodb_worker = (s_sis_worker *)reader->snodb_worker;
    s_snodb_cxt *context = (s_snodb_cxt *)snodb_worker->context;
    s_sis_db_chars *inmem = (s_sis_db_chars *)argv;
    // 向对应的socket发送数据

    // printf("%s %d %s\n", __func__, reader->rfmt, inmem->sname);

	s_sis_net_message *newinfo = sis_net_message_create();
	newinfo->name = reader->serial ? sis_sdsdup(reader->serial) : NULL;
	newinfo->cid = reader->cid;
	sis_message_set_key(newinfo, inmem->kname, inmem->sname);
	if (reader->rfmt & SISDB_FORMAT_BYTES)
	{
		sis_net_ans_with_bytes(newinfo, (const char *)inmem->data, inmem->size);    
	}
	else
	{
		s_sis_dynamic_db *db = sis_map_list_get(reader->sub_unziper->sdbs, inmem->sname);
		s_sis_sds omem = sis_db_format_sds(db, NULL, reader->rfmt, (const char *)inmem->data, inmem->size, 0);
		// printf("%s %d : %s\n",db->name, reader->rfmt, omem);
		sis_net_ans_with_chars(newinfo, omem, sis_sdslen(omem));
		sis_sdsfree(omem); 
	}
	if (context->cb_net_message)
	{
		context->cb_net_message(context->cb_source, newinfo);
	}
	sis_net_message_destroy(newinfo); 
    return SIS_METHOD_OK;
}
static int cb_sub_start(void *source, void *argv)
{
    s_snodb_reader *reader = (s_snodb_reader *)source;
    s_sis_worker *snodb_worker = (s_sis_worker *)reader->snodb_worker;
    s_snodb_cxt *context = (s_snodb_cxt *)snodb_worker->context;
    const char *workdate = (const char *)argv;
    // 向对应的socket发送数据
    s_sis_net_message *newinfo = sis_net_message_create();
    newinfo->cid = reader->cid;
    newinfo->name = reader->serial ? sis_sdsdup(reader->serial) : NULL;
    sis_net_ans_with_sub_start(newinfo, workdate);
    if (context->cb_net_message)
	{
		context->cb_net_message(context->cb_source, newinfo);
	}
    sis_net_message_destroy(newinfo); 
    return 0;
}
static int cb_sub_realtime(void *source, void *argv)
{
    s_snodb_reader *reader = (s_snodb_reader *)source;
    s_sis_worker *snodb_worker = (s_sis_worker *)reader->snodb_worker;
    s_snodb_cxt *context = (s_snodb_cxt *)snodb_worker->context;
    const char *workdate = (const char *)argv;
    // 向对应的socket发送数据
    s_sis_net_message *newinfo = sis_net_message_create();
    newinfo->cid = reader->cid;
    newinfo->name = reader->serial ? sis_sdsdup(reader->serial) : NULL;
    sis_net_ans_with_sub_wait(newinfo, workdate);
    if (context->cb_net_message)
	{
		context->cb_net_message(context->cb_source, newinfo);
	}
    sis_net_message_destroy(newinfo); 
    return 0;
}

static int cb_sub_stop(void *source, void *argv)
{ 
    s_snodb_reader *reader = (s_snodb_reader *)source;
    s_sis_worker *snodb_worker = (s_sis_worker *)reader->snodb_worker;
    s_snodb_cxt *context = (s_snodb_cxt *)snodb_worker->context;
    const char *workdate = (const char *)argv;
    LOG(5)("server send sub stop. %s [%d]\n", (char *)argv, reader->cid);
    // 向对应的socket发送数据
    s_sis_net_message *newinfo = sis_net_message_create();
    newinfo->cid = reader->cid;
    newinfo->name = reader->serial ? sis_sdsdup(reader->serial) : NULL;
    sis_net_ans_with_sub_stop(newinfo, workdate);
    if (context->cb_net_message)
	{
		context->cb_net_message(context->cb_source, newinfo);
	}
    sis_net_message_destroy(newinfo); 
    return 0;
}
static int cb_dict_keys(void *source, void *argv)
{
    s_snodb_reader *reader = (s_snodb_reader *)source;
    s_sis_worker *snodb_worker = (s_sis_worker *)reader->snodb_worker;
    s_snodb_cxt *context = (s_snodb_cxt *)snodb_worker->context;
    const char *keys = (const char *)argv;

    // 向对应的socket发送数据
    s_sis_net_message *newinfo = sis_net_message_create();
    newinfo->cid = reader->cid;
    newinfo->name = reader->serial ? sis_sdsdup(reader->serial) : NULL;
    newinfo->key = sis_sdsnew("_keys_");
    sis_net_ans_with_chars(newinfo, keys, sis_strlen(keys));
	if (context->cb_net_message)
	{
		context->cb_net_message(context->cb_source, newinfo);
	}
    sis_net_message_destroy(newinfo); 
    return 0;
}
static int cb_dict_sdbs(void *source, void *argv)
{
    s_snodb_reader *reader = (s_snodb_reader *)source;
    s_sis_worker *snodb_worker = (s_sis_worker *)reader->snodb_worker;
    s_snodb_cxt *context = (s_snodb_cxt *)snodb_worker->context;
	if (reader->sub_disk)
	{
		// 如果是磁盘读取 需要传递sdb表
		if (!reader->sub_unziper)
		{
			reader->sub_unziper = snodb_worker_create();
			// snodb_worker_unzip_init(reader->sub_unziper, reader, cb_unzip_reply);
		}
		else
		{
			snodb_worker_clear(reader->sub_unziper);
		}	
        // snodb_worker_set_keys(reader->sub_unziper, keys);
        snodb_worker_set_sdbs(reader->sub_unziper, (s_sis_sds)argv);
	}
	const char *sdbs = (const char *)argv;
    // 向对应的socket发送数据
    s_sis_net_message *newinfo = sis_net_message_create();
    newinfo->cid = reader->cid;
    newinfo->name = reader->serial ? sis_sdsdup(reader->serial) : NULL;
    newinfo->key = sis_sdsnew("_sdbs_");
    sis_net_ans_with_chars(newinfo, sdbs, sis_strlen(sdbs));
	if (context->cb_net_message)
	{
		context->cb_net_message(context->cb_source, newinfo);
	}
    sis_net_message_destroy(newinfo); 
    return 0;
}
///////////////////////////////////////////////////////////////////////////
//------------------------s_snodb_reader --------------------------------//
///////////////////////////////////////////////////////////////////////////


s_snodb_reader *snodb_reader_create()
{
	s_snodb_reader *o = SIS_MALLOC(s_snodb_reader, o);
	return o;
}
void snodb_reader_destroy(void *reader_)
{
	s_snodb_reader *reader = (s_snodb_reader *)reader_;
	// 必须先关闭读句柄 数据才会不再写入 再关闭其他
	LOG(8)("reader close. cid = %d\n", reader->cid);
	if (reader->reader)
	{
		sis_lock_reader_close(reader->reader);	
	}
	if (reader->sub_disker)
	{
		snodb_snos_read_stop(reader->sub_disker);
	}
	if (reader->sub_unziper)
	{
		snodb_worker_destroy(reader->sub_unziper);	
	}
	if (reader->sub_ziper)
	{
		// 次级退出 上级会报异常 onebyone = 1
		snodb_worker_destroy(reader->sub_ziper);	
	}
	sis_sdsfree(reader->sub_keys);
	sis_sdsfree(reader->sub_sdbs);
	sis_sdsfree(reader->serial);
	LOG(8)("reader close ok. cid = %d\n", reader->cid);
	sis_free(reader);
}
// static int cb_zip_complete_worker(void *reader_, size_t size_)
// {
// 	if (size_ < 1)
// 	{
// 		return 1;
// 	}
// 	s_snodb_reader *reader = (s_snodb_reader *)reader_;
// 	s_snodb_compress *zipmem = reader->sub_ziper->zip_bits;
// 	zipmem->size = sis_bits_struct_getsize(reader->sub_ziper->cur_sbits);
// 	reader->cb_sub_inctzip(reader, zipmem);
// 	if (reader->sub_ziper->cur_size > reader->sub_ziper->initsize)
// 	{
// 		snodb_worker_zip_flush(reader->sub_ziper, 1);
// 	}
// 	else
// 	{
// 		snodb_worker_zip_flush(reader->sub_ziper, 0);
// 	}
// 	return 0;
// }
static int cb_unzip_reply(void *source_, int kidx_, int sidx_, char *in_, size_t ilen_)
{
	s_snodb_reader *reader = (s_snodb_reader *)source_;
	// return; // 只解压什么也不干
	if (reader->cb_sub_chars)
	{
		if (!in_)
		{
			// 数据包结束
			return 0;
		}
		s_sis_sds kname = sis_map_list_geti(reader->sub_unziper->keys, kidx_);
		s_sis_dynamic_db *db = sis_map_list_geti(reader->sub_unziper->sdbs, sidx_);
		if (!db || !kname)
		{
			return 0;
		}
		int kidx = sis_map_list_get_index(reader->sub_ziper->keys, kname);
		int sidx = sis_map_list_get_index(reader->sub_ziper->sdbs, db->name);
		if (kidx < 0 || sidx < 0)
		{
			// 通过此判断是否是需要的key和sdb
			return 0;
		}
		s_sis_db_chars inmem = {0};
		inmem.kname = kname;
		inmem.sname = db->name;
		inmem.data = in_;
		inmem.size = ilen_;
		reader->cb_sub_chars(reader, &inmem);
	}
	if (reader->cb_sub_inctzip)
	{
		if (!in_)
		{
			// 表示一个包解析完成 如果数据区有数据就发送
			s_snodb_compress *zipmem = reader->sub_ziper->zip_bits;
			zipmem->size = sis_bits_struct_getsize(reader->sub_ziper->cur_sbits);
			// printf("reader->sub_ziper = %p %d %d\n", reader->sub_ziper->zip_bits, reader->sub_ziper->zip_size, zipmem->size);
			// sis_memory_set_size(SIS_OBJ_MEMORY(worker->zip_bits), sizeof(s_snodb_bytes) + zipmem->size);

			if (zipmem->size > 0)
			{
				reader->cb_sub_inctzip(reader, zipmem);
				if (reader->sub_ziper->cur_size > reader->sub_ziper->initsize)
				{
					snodb_worker_zip_flush(reader->sub_ziper, 1);
				}
				else
				{
					snodb_worker_zip_flush(reader->sub_ziper, 0);
				}
			}			
			return 0;
		}
		// 先从大表中得到实际名称
		s_sis_sds kname = sis_map_list_geti(reader->sub_unziper->keys, kidx_);
		s_sis_dynamic_db *db = sis_map_list_geti(reader->sub_unziper->sdbs, sidx_);
		if (!db || !kname)
		{
			return 0;
		}
		// 再获取订阅的索引
		int kidx = sis_map_list_get_index(reader->sub_ziper->keys, kname);
		int sidx = sis_map_list_get_index(reader->sub_ziper->sdbs, db->name);
		if (kidx < 0 || sidx < 0)
		{
			return 0;
		}

		LOG(5)("cb_unzip_reply = %s %s -> %d  %d | %d  %d\n", kname, db->name, kidx, sidx, kidx_, sidx_);
		snodb_worker_zip_set(reader->sub_ziper, kidx, sidx, in_, ilen_);
		// if (reader->sub_ziper->zip_bits->size > reader->sub_ziper->zip_size)
		// printf("zipbit size= %d\n", reader->sub_ziper->zip_bits->size);
		s_snodb_compress *zipmem = reader->sub_ziper->zip_bits;
		zipmem->size = reader->sub_ziper->zip_bits->size;
		// 这里必须要判断 否则可能因为品种不同造成单包尺寸越界
		// ??? 应该根据压缩的返回值 来判断是否要处理
		if (zipmem->size > reader->sub_ziper->zip_size)
		{
			printf("zipbit size= %d\n", reader->sub_ziper->zip_bits->size);
			reader->cb_sub_inctzip(reader, zipmem);
			if (reader->sub_ziper->cur_size > reader->sub_ziper->initsize)
			{
				snodb_worker_zip_flush(reader->sub_ziper, 1);
			}
			else
			{
				snodb_worker_zip_flush(reader->sub_ziper, 0);
			}
		}
	}
    return 0;
}


int snodb_reader_new_history(s_snodb_reader *reader_)
{
	// printf("%s\n", __func__);
    s_sis_worker *worker = (s_sis_worker *)reader_->snodb_worker; 
    s_snodb_cxt *snodb = (s_snodb_cxt *)worker->context;
	printf("snodb_reader_new_history %p\n", snodb->rfile_config);
	// 清除该端口其他的订阅
	snodb_move_reader(snodb, reader_->cid);
	reader_->sub_disker = snodb_snos_read_start(snodb->rfile_config, reader_);
	if (reader_->sub_disker)
	{
		sis_pointer_list_push(snodb->readeres, reader_);
		return 1;
	}
	printf("sub histroy error.\n");
	return 0;
}
int snodb_reader_new_realtime(s_snodb_reader *reader_)
{
    s_sis_worker *worker = (s_sis_worker *)reader_->snodb_worker; 
    s_snodb_cxt *snodb = (s_snodb_cxt *)worker->context;

	snodb_move_reader(snodb, reader_->cid);
	// 
	if (snodb->inited && snodb->work_date == reader_->sub_date)  
	// 如果已经初始化就启动发送数据 否则就等待系统通知后再发送
	{
		snodb_reader_realtime_start(reader_);
	}
	sis_pointer_list_push(snodb->readeres, reader_);
	return 1;
}
int snodb_move_reader(s_snodb_cxt *snodb_,int cid_)
{
	int count = 0;
	for (int i = 0; i < snodb_->readeres->count; )
	{
		s_snodb_reader *reader = (s_snodb_reader *)sis_pointer_list_get(snodb_->readeres, i);
		if (reader->cid == cid_)
		{
			sis_pointer_list_delete(snodb_->readeres, i, 1);
			count++;
		}
		else
		{
			i++;
		}
	}	
	return count;
}

// 只能在确定start时处理 定制解压相关类
int snodb_reader_realtime_start(s_snodb_reader *reader_)
{
    s_sis_worker *worker = (s_sis_worker *)reader_->snodb_worker; 
    s_snodb_cxt *snodb = (s_snodb_cxt *)worker->context;
    
	LOG(5)("sub start : date = %d - %d\n", snodb->work_date ,reader_->sub_date);
	
	s_sis_sds work_keys =  NULL;
	s_sis_sds work_sdbs =  NULL;

	if (!reader_->sub_whole)
	{
		if (!reader_->sub_ziper)
		{
			reader_->sub_ziper = snodb_worker_create();
		}
		else
		{
			snodb_worker_clear(reader_->sub_ziper);			
		}	
		snodb_worker_zip_init(reader_->sub_ziper, snodb->zip_size, 1024 * 1024);
		// printf("%d:%s \n %d:%s \n", sis_sdslen(reader_->sub_keys), reader_->sub_keys,
		// 	sis_sdslen(snodb->work_keys), snodb->work_keys);
		work_keys = sis_match_key(reader_->sub_keys, snodb->work_keys);
		if (!work_keys)
		{
			work_keys =  sis_sdsdup(snodb->work_keys);
		}
		work_sdbs = sis_match_sdb_of_map(reader_->sub_sdbs, snodb->sdbs);
		if (!work_sdbs)
		{
			work_sdbs =  sis_sdsdup(snodb->work_sdbs);
		}
		snodb_worker_set_keys(reader_->sub_ziper, work_keys);
		snodb_worker_set_sdbs(reader_->sub_ziper, work_sdbs);

		if (!reader_->sub_unziper)
		{
			reader_->sub_unziper = snodb_worker_create();
			snodb_worker_unzip_init(reader_->sub_unziper, reader_, cb_unzip_reply);
		}
		else
		{
			snodb_worker_clear(reader_->sub_unziper);
		}	
        snodb_worker_set_keys(reader_->sub_unziper, snodb->work_keys);
        snodb_worker_set_sdbs(reader_->sub_unziper, snodb->work_sdbs);
	}
	else
	{
		work_keys =  snodb->work_keys;
		work_sdbs =  snodb->work_sdbs;
	}

	if (reader_->cb_sub_start)
	{
		char sdate[32];
		sis_llutoa(snodb->work_date, sdate, 32, 10);
		reader_->cb_sub_start(reader_, sdate);
	}
	LOG(5)("count = %d inited = %d stoped = %d\n", snodb->outputs->work_queue->rnums, snodb->inited, snodb->stoped);
	if (reader_->cb_dict_keys)
	{
		reader_->cb_dict_keys(reader_, work_keys);
	}
	if (reader_->cb_dict_sdbs)
	{
		reader_->cb_dict_sdbs(reader_, work_sdbs);
	}

	if (!reader_->sub_whole)
	{
		sis_sdsfree(work_keys);
		sis_sdsfree(work_sdbs);
	}
	
	reader_->isinit = false;
	// 新加入的订阅者 先发送订阅开始 再发送最后一个初始块后续所有数据 
	if (!reader_->reader)
	{
		reader_->reader = sis_lock_reader_create(snodb->outputs, 
			// reader_->ishead ? SIS_UNLOCK_READER_HEAD : SIS_UNLOCK_READER_TAIL, 
			SIS_UNLOCK_READER_HEAD,
			reader_, cb_output_reader, cb_output_realtime);
		sis_lock_reader_open(reader_->reader);	
	}
    return 0;
}

int snodb_reader_realtime_stop(s_snodb_reader *reader_)
{    
    s_sis_worker *worker = (s_sis_worker *)reader_->snodb_worker; 
    s_snodb_cxt *snodb = (s_snodb_cxt *)worker->context;
    
	LOG(5)("sub stop : date = %d - %d\n", snodb->work_date ,reader_->sub_date);
	if (reader_->cb_sub_stop)
	{
		char sdate[32];
		sis_llutoa(snodb->work_date, sdate, 32, 10);
		reader_->cb_sub_stop(reader_, sdate);
	}
	if (reader_->reader)
	{
		sis_lock_reader_close(reader_->reader);	
		reader_->reader = NULL;
	}
	return 0;
}
// // 从磁盘中获取数据
// s_sis_sds sisdb_disk_get_sno_sds(s_sisdb_cxt *sisdb_, const char *key_, int iformat_, int workdate_, s_sis_json_node *node_)
// {
//     char fn[32];
//     sis_llutoa(workdate_, fn, 32, 10);
//     char work_path[512];
//     sis_sprintf(work_path, 512, "%s%s/", sisdb_->fast_path, sisdb_->name);
//     s_sis_disk_v1_class *read_class = sis_disk_v1_class_create(); 
//     if (sis_disk_v1_class_init(read_class, SIS_DISK_TYPE_SNO, work_path, fn) 
//         || sis_disk_v1_file_read_start(read_class))
//     {
//         sis_disk_v1_class_destroy(read_class);
//         return NULL;
//     }
//     s_sis_sds kname = NULL; s_sis_sds sname = NULL; 
//     int cmds = sis_str_divide_sds(key_, '.', &kname, &sname);
//     if (cmds < 2)
//     {
//         sis_disk_v1_class_destroy(read_class);
//         return NULL;
//     }
//     s_sis_dynamic_db *tb = sis_map_list_get(sisdb_->sdbs, sname);

//     s_sis_disk_v1_reader *reader = sis_disk_v1_reader_create(NULL);
//     // reader->issub = 0;
//     // printf("%s| %s | %s\n", context->read_cb->sub_keys, context->work_sdbs, context->read_cb->sub_sdbs ? context->read_cb->sub_sdbs : "nil");
//     sis_disk_v1_reader_set_sdb(reader, sname);
//     sis_disk_v1_reader_set_key(reader, kname); 
//     sis_sdsfree(kname); sis_sdsfree(sname);

//     // sub 是一条一条的输出
//     // get 是所有符合条件的一次性输出
//     s_sis_object *obj = sis_disk_v1_file_read_get_obj(read_class, reader);
//     printf("obj = %p\n",obj);
//     sis_disk_v1_reader_destroy(reader);

//     sis_disk_v1_file_read_stop(read_class);
//     sis_disk_v1_class_destroy(read_class); 

//     if (obj)
//     {
//         const char *fields = NULL;
//         if (node_ && sis_json_cmp_child_node(node_, "fields"))
//         {
//             fields = sis_json_get_str(node_, "fields");
//         }
//         // printf(" %s iformat_ = %x %d\n", key_, iformat_, obj->style);
//         // sis_out_binary("sno", SIS_OBJ_GET_CHAR(obj),SIS_OBJ_GET_SIZE(obj));
//         if (iformat_ == SISDB_FORMAT_BYTES)
//         {
//             bool iswhole = sisdb_field_is_whole(fields);
//             if (iswhole)
//             {
//                 s_sis_sds o = sis_sdsnewlen(SIS_OBJ_GET_CHAR(obj),SIS_OBJ_GET_SIZE(obj));
//                 sis_object_destroy(obj);
//                 return o;
//             }
//             s_sis_string_list *field_list = sis_string_list_create();
//             sis_string_list_load(field_list, fields, sis_strlen(fields), ",");
//             s_sis_sds other = sisdb_collect_struct_to_sds(tb->db, SIS_OBJ_GET_CHAR(obj),SIS_OBJ_GET_SIZE(obj), field_list);
//             sis_string_list_destroy(field_list);
//             return other;
//         }
//         s_sis_sds o = sisdb_get_chars_format_sds(tb, key_, iformat_, SIS_OBJ_GET_CHAR(obj),SIS_OBJ_GET_SIZE(obj), fields);
//         sis_object_destroy(obj);
//         return o;
//     }
//     return NULL;
// }

// // 获取sno的数据
// s_sis_sds sisdb_get_sno_sds(s_sisdb_cxt *sisdb_, const char *key_, uint16 *format_, s_sis_sds argv_)
// {
//     s_sis_sds o = NULL;    
//     s_sisdb_collect *collect = sisdb_get_collect(sisdb_, key_);  
//     if (!argv_)
//     {
//         *format_ = SISDB_FORMAT_CHARS;
//         o = sisdb_collect_fastget_sds(collect, key_, SISDB_FORMAT_CHARS);
//     }
//     else
//     {
//         s_sis_json_handle *handle = sis_json_load(argv_, sis_sdslen(argv_));
//         if (handle)
//         {
//             // 带参数 
//             int iformat = sis_db_get_format_from_node(handle->node, SISDB_FORMAT_JSON);
//             *format_ = iformat & SISDB_FORMAT_CHARS ? SISDB_FORMAT_CHARS : SISDB_FORMAT_BYTES;
//             // 找 date 字段  
//             date_t workdate = sis_json_get_int(handle->node, "date", sisdb_->work_date);
//             printf("----- %d %d\n", workdate, sisdb_->work_date);
//             if (workdate < sisdb_->work_date)
//             {
//                 // 从磁盘中获取数据
//                 o = sisdb_disk_get_sno_sds(sisdb_, key_, iformat, workdate, handle->node); 
//             }
//             else
//             {
//                 o = sisdb_collect_get_sds(collect, key_, iformat, handle->node);                          
//             }
//             sis_json_close(handle);
//         }
//         else
//         {
//             *format_ = SISDB_FORMAT_CHARS;
//             o = sisdb_collect_fastget_sds(collect, key_, SISDB_FORMAT_CHARS);
//         }        
//     } 
//     return o;
// } 

//////////////////////////////////////////////////////////////////