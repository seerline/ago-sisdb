
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
    {"ipub",      cmd_zipdb_ipub,    0, NULL}, // 发布数据流 单条数据 kidx+sidx+data
    {"spub",      cmd_zipdb_spub,   0, NULL},  // 发布数据流 单条数据 key sdb data
    {"zpub",      cmd_zipdb_zpub,   0, NULL},  // 发布数据流 只有完成的压缩数据块
    {"sub",       cmd_zipdb_sub,    0, NULL},  // 订阅数据流 
    {"unsub",     cmd_zipdb_unsub,  0, NULL},  // 取消订阅数据流 
    {"clear",     cmd_zipdb_clear,  0, NULL},  // 清理数据流 
// 磁盘工具
    {"wlog",     cmd_zipdb_wlog,  0, NULL},  // 接收数据写盘
    {"rlog",     cmd_zipdb_rlog,  0, NULL},  // 清理数据流 
    {"wsno",     cmd_zipdb_wsno,  0, NULL},  // 清理数据流 
    {"rsno",     cmd_zipdb_rsno,  0, NULL},  // 清理数据流 
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
s_zipdb_bits *_zipdb_new_data(s_zipdb_cxt *sno, msec_t agomsec)
{
	size_t size = sno->maxsize + 256; // 这里应该取最长的结构体长度
	s_zipdb_bits *memory = (s_zipdb_bits *)sis_malloc(sizeof(s_zipdb_bits) + size);
	msec_t nowmsec = sis_time_get_now_msec();
	if (nowmsec - agomsec > sno->initmsec)
	{
		memory->init = 1;
		sno->work_msec = nowmsec;
		sis_bits_struct_flush(sno->cur_bitzip);
		sis_bits_struct_link(sno->cur_bitzip, memory->data, size);		
	}
	else
	{
		memory->init = 0;
		sno->work_msec = agomsec;
		sis_bits_struct_link(sno->cur_bitzip, memory->data, size);		
	}
	memory->size = 0;
	memset(memory->data, 0, size);
	
	return memory;
}

s_zipdb_bits *_zipdb_get_data(s_zipdb_cxt *sno)
{
	if (!sno->cur_memory)
	{
		sno->cur_memory = _zipdb_new_data(sno, 0);
	}
	return sno->cur_memory;
}

// 把压缩流中数据按时间和大小 放入缓存 然后回调reader
static void *_thread_zipdb_write(void *argv_)
{
	sis_sleep(300);
    s_zipdb_cxt *sno = (s_zipdb_cxt *)argv_;

	sno->work_status = 0;
	
    sno->notice_wait = sis_wait_malloc();
    s_sis_wait *wait = sis_wait_get(sno->notice_wait);
	sis_thread_wait_start(wait);
    while (sno->work_status == 0)
    {
		bool surpass_waittime = false;
        if(sis_thread_wait_sleep_msec(wait, sno->gapmsec) == SIS_ETIMEDOUT)
        {
            // printf("timeout ..a.. %d \n",__kill);
			surpass_waittime = true;
        }
		if (sno->work_status)
		{
			break;
		}
		// 无论何种原因 只要有数据 就生成数据包
		sis_mutex_lock(&sno->write_lock);
		s_zipdb_bits *cur_memory = _zipdb_get_data(sno);
		if ( (surpass_waittime && cur_memory->size > 1)  || // 条件满足
		     (!surpass_waittime && cur_memory->size > sno->maxsize - 256))
		{
			// 把数据包发送给所有的reader
			for (int i = 0; i < sno->reader->count; i++)
			{
				s_zipdb_reader *reader = (s_zipdb_reader *)sis_pointer_list_get(sno->reader, i);
				if (reader->cb_bits)
				{	
					reader->cb_bits(reader, cur_memory);
				}
			}
			sis_pointer_list_push(sno->out_bitzips, cur_memory);
			sno->cur_memory = _zipdb_new_data(sno, sno->work_msec);
		}
		sis_mutex_unlock(&sno->write_lock);
    }
    sis_thread_wait_stop(wait);
    sis_wait_free(sno->notice_wait);
    sis_thread_finish(&sno->write_thread);
    sno->work_status = 2;
    return NULL;
}

// 把队列数据写入压缩流中 可堵塞
static int cb_zipdb_reader(void *zipdb_, s_sis_object *in_)
{
	s_zipdb_cxt *sno = (s_zipdb_cxt *)zipdb_;
	
	int size = 0;
	sis_mutex_lock(&sno->write_lock);

	s_sis_memory *memory = SIS_OBJ_MEMORY(in_);
	int kidx = sis_memory_get_byte(memory, 4);
	int sidx = sis_memory_get_byte(memory, 2);
	// 如果订阅者不需要压缩 就直接返回压入的数据
	for (int i = 0; i < sno->reader->count; i++)
	{
		s_zipdb_reader *reader = (s_zipdb_reader *)sis_pointer_list_get(sno->reader, i);
		if (reader->cb_onebyone)
		{
			reader->cb_onebyone(reader, kidx, sidx, sis_memory(memory), sis_memory_get_size(memory));
		}
	}
	//  向memory中压入数据
	s_zipdb_bits *cur_memory = _zipdb_get_data(sno);

	sis_bits_struct_encode(sno->cur_bitzip, kidx, sidx, sis_memory(memory), sis_memory_get_size(memory));
	size = sis_bits_stream_getbytes(sno->cur_bitzip);
	cur_memory->size = size;
	// sis_mutex_unlock(&sno->write_lock);
	//  数据如果超过一定数量就直接发送
	if (size > sno->maxsize - 256)
	{
		sis_thread_wait_notice(sis_wait_get(sno->notice_wait));
	}
	sis_mutex_unlock(&sno->write_lock);
	return 0;
}

bool zipdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
	s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_zipdb_cxt *context = SIS_MALLOC(s_zipdb_cxt, context);
    worker->context = context;

    context->work_date = sis_time_get_idate(0);

	context->initmsec = 600*1000;
	context->gapmsec = 20;
	context->maxsize = ZIPMEM_MAXSIZE;

	sis_mutex_init(&(context->write_lock), NULL);
    if (!sis_thread_create(_thread_zipdb_write, context, &context->write_thread))
    {
        LOG(1)("can't start zipdb_write thread.\n");
		sis_free(context);
        return NULL;
    }
	const char *wpath = sis_json_get_str(node,"work-path");
	if (wpath)
	{
		context->work_path = sis_sdsnew(wpath);  // 根据是否为空判断是否处理log
	}
	context->page_size = sis_json_get_int(node, "page-size", 500) * 1000000;

	context->out_bitzips = sis_pointer_list_create();
	context->out_bitzips->vfree = sis_memory_destroy;
	context->cur_bitzip = sis_bits_stream_create(NULL, 0);

	// 最大数据不超过 256M
	context->inputs = sis_share_list_create("sno-stream", 16*1024*1024);
	context->in_reader = sis_share_reader_login(context->inputs, SIS_SHARE_FROM_HEAD, context, cb_zipdb_reader);

    context->keys = sis_map_list_create(sis_sdsfree_call); 
	context->sdbs = sis_map_list_create(sis_dynamic_db_destroy);

	// 每个读者都自行压缩数据 并把压缩的数据回调出去
	context->reader = sis_pointer_list_create();
	context->reader->vfree = zipdb_reader_destroy;

    return true;
}
void zipdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *sno = (s_zipdb_cxt *)worker->context;

	if (sno->cur_memory)
	{
		sis_free(sno->cur_memory);
	}

	sis_sdsfree(sno->work_path);
	sis_sdsfree(sno->work_keys);
	sis_sdsfree(sno->work_sdbs);

	sno->work_status = 1;
	// 通知退出
	sis_thread_wait_notice(sis_wait_get(sno->notice_wait));
	while (sno->work_status != 2)
	{
		sis_sleep(10);
	}

	sis_pointer_list_destroy(sno->reader);

    sis_map_list_destroy(sno->keys); 
    sis_map_list_destroy(sno->sdbs);

	sis_share_reader_logout(sno->inputs, sno->in_reader);
	sis_share_list_destroy(sno->inputs);

	sis_bits_stream_destroy(sno->cur_bitzip);
	sis_pointer_list_destroy(sno->out_bitzips);

	sis_free(sno);
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
	zipdb_->inited = false;
	zipdb_->stoped = false;
	zipdb_->work_date = 0;
	printf("------ _zipdb_new : %d %d %d\n", workdate_, sis_sdslen(keys_), sis_sdslen(sdbs_));
	if (!keys_ || sis_sdslen(keys_) < 3 || !sdbs_ || sis_sdslen(sdbs_) < 3)
	{
		return false;
	}
	zipdb_->work_date = workdate_;

	sis_sdsfree(zipdb_->work_keys); zipdb_->work_keys = NULL;
	sis_sdsfree(zipdb_->work_sdbs); zipdb_->work_sdbs = NULL;

	sis_map_list_clear(zipdb_->keys);
	sis_map_list_clear(zipdb_->sdbs);	
	{
		s_sis_string_list *klist = sis_string_list_create_w();
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
    	sis_bits_struct_set_key(zipdb_->cur_bitzip, count);
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
				sis_bits_struct_set_sdb(zipdb_->cur_bitzip, sdb);
			}
			innode = sis_json_next_node(innode);
		}
		zipdb_->work_sdbs = sis_sdsnew(sdbs_);
		sis_json_close(injson);
	}
	zipdb_->inited = true;
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
        return SIS_METHOD_OK;
    }
    return SIS_METHOD_ERROR;
}
int cmd_zipdb_start(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;

	sis_mutex_lock(&context->write_lock);
	// 初始化后首先向所有订阅者发送订阅开始信息
	for (int i = 0; i < context->reader->count; i++)
	{
		s_zipdb_reader *reader = (s_zipdb_reader *)sis_pointer_list_get(context->reader, i);
		zipdb_sub_start(reader);
	}
	sis_mutex_unlock(&context->write_lock);
	return SIS_METHOD_OK;
}
int cmd_zipdb_stop(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
	// printf("cmd_zipdb_stop, curmemory : %d\n", context->cur_memory->size);
	context->stoped = true;
	sis_mutex_lock(&context->write_lock);
	for (int i = 0; i < context->reader->count; i++)
	{
		s_zipdb_reader *reader = (s_zipdb_reader *)sis_pointer_list_get(context->reader, i);
		// 停止前把临时的数据区发送一下
		if (context->cur_memory->size > 0)
		{
			if (reader->cb_bits)
			{	
				reader->cb_bits(reader, context->cur_memory);
			}
		}
		if (reader->cb_sub_stop)
		{
			reader->cb_sub_stop(reader, context->work_date);
		}
	}
	sis_mutex_unlock(&context->write_lock);
    return SIS_METHOD_OK;
}

int _zipdb_write(s_zipdb_cxt *zipdb_, int kidx_, int sidx_, void *in_, size_t ilen_)
{
	if (!zipdb_->inited || zipdb_->stoped)
	{
		return -1;
	}
	if (kidx_ < 0) {
		return -2;
	}
	if (sidx_ < 0) {
		return -3;
	}
	s_sis_memory *memory = sis_memory_create();
	sis_memory_cat_byte(memory, kidx_, 4);
	sis_memory_cat_byte(memory, sidx_, 2);
	sis_memory_cat(memory, (char *)in_, ilen_);	
	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, memory);

	int index = sis_share_list_push(zipdb_->inputs, obj);

	sis_object_destroy(obj);
	return index;
}

int _zipdb_write_bits(s_zipdb_cxt *zipdb_, s_zipdb_bits *in_)
{
	if (!zipdb_->inited || zipdb_->stoped)
	{
		return -1;
	}
	// 直接写入到数据流中
	sis_mutex_lock(&zipdb_->write_lock);
	s_zipdb_bits *cur_memory = in_;
	for (int i = 0; i < zipdb_->reader->count; i++)
	{
		s_zipdb_reader *reader = (s_zipdb_reader *)sis_pointer_list_get(zipdb_->reader, i);
		if (reader->cb_bits)
		{	
			reader->cb_bits(reader, cur_memory);
		}
	}
	sis_pointer_list_push(zipdb_->out_bitzips, cur_memory);
	sis_mutex_unlock(&zipdb_->write_lock);
	return 0;
}
int cmd_zipdb_ipub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
    s_zipdb_bytes *in = (s_zipdb_bytes *)argv_;
	printf("cmd_zipdb_ipub: %d %d %d\n",in->kidx, in->sidx, in->size);
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

    if (!_zipdb_write(context, kidx, sidx, in->data, in->size) >= 0)
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
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

    s_zipdb_reader *reader = zipdb_reader_create();
	reader->cid = sis_message_get_int(msg, "cid");
	s_sis_sds source = sis_message_get_str(msg, "source");
	if (source)
	{
		reader->source = sis_sdsdup(source);
	}
    reader->zipdb_worker = worker;
    reader->ishead = sis_message_get_bool(msg, "head");
    reader->cb_bits = (cb_zipdb_bits *)sis_message_get(msg, "cb_bits");
    reader->cb_onebyone = (cb_zipdb_onebyone *)sis_message_get(msg, "cb_onebyone");

    reader->cb_sub_start = (cb_zipdb_sub_info *)sis_message_get(msg, "cb_sub_start");
    reader->cb_sub_realtime = (cb_zipdb_sub_info *)sis_message_get(msg, "cb_sub_realtime");
    reader->cb_sub_stop = (cb_zipdb_sub_info *)sis_message_get(msg, "cb_sub_stop");

    reader->cb_init_keys = (cb_zipdb_init_info *)sis_message_get(msg, "cb_init_keys");
    reader->cb_init_sdbs = (cb_zipdb_init_info *)sis_message_get(msg, "cb_init_sdbs");

    // 一个socket只能订阅一次 后订阅的会冲洗掉前面一次
    zipdb_add_reader(context, reader);

    return SIS_METHOD_OK;

}

int cmd_zipdb_unsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

	int cid = sis_message_get_int(msg, "cid");

 	sis_mutex_lock(&context->write_lock);
	zipdb_del_reader(context, cid);
	sis_mutex_unlock(&context->write_lock);

    return SIS_METHOD_OK;

}
int cmd_zipdb_clear(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_zipdb_cxt *context = (s_zipdb_cxt *)worker->context;
    s_sis_message *msg = (s_sis_message *)argv_;

	int mode = sis_message_get_int(msg, "mode");
	sis_mutex_lock(&context->write_lock);
	switch (mode)
	{
	case ZIPDB_CLEAR_MODE_DATA:
		context->cur_memory = NULL;
		sis_pointer_list_clear(context->out_bitzips);
		break;
	case ZIPDB_CLEAR_MODE_READER:
		sis_pointer_list_clear(context->reader);
		break;	
	default:
		sis_pointer_list_clear(context->reader);
		context->cur_memory = NULL;
		sis_pointer_list_clear(context->out_bitzips);
		break;
	}
	sis_mutex_unlock(&context->write_lock);
    return SIS_METHOD_OK;
}
	
int cmd_zipdb_wlog(void *worker_, void *argv_)
{
	return SIS_METHOD_OK;
}
int cmd_zipdb_rlog(void *worker_, void *argv_)
{
	return SIS_METHOD_OK;
}
int cmd_zipdb_wsno(void *worker_, void *argv_)
{
	return SIS_METHOD_OK;
}
int cmd_zipdb_rsno(void *worker_, void *argv_)
{
	return SIS_METHOD_OK;
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
	sis_sdsfree(reader->source);
	sis_free(reader);
}

int zipdb_add_reader(s_zipdb_cxt *zipdb_,s_zipdb_reader *reader_)
{
	sis_mutex_lock(&zipdb_->write_lock);
	zipdb_del_reader(zipdb_, reader_->cid);
	// 新加入的订阅者 先发送订阅开始 再发送最后一个初始块后续所有数据 
	zipdb_sub_start(reader_);
	sis_pointer_list_push(zipdb_->reader, reader_);
	sis_mutex_unlock(&zipdb_->write_lock);
	return 1;
}
int zipdb_del_reader(s_zipdb_cxt *zipdb_,int cid_)
{
	int count = 0;
	for (int i = 0; i < zipdb_->reader->count; )
	{
		s_zipdb_reader *reader = (s_zipdb_reader *)sis_pointer_list_get(zipdb_->reader, i);
		if (reader->cid == cid_)
		{
			sis_pointer_list_delete(zipdb_->reader, i, 1);
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
    s_zipdb_cxt *sno = (s_zipdb_cxt *)worker->context;
    
	if (reader->cb_sub_start)
	{
		reader->cb_sub_start(reader, sno->work_date);
	}
	printf("-------count %d: inited %d stoped %d\n",sno->out_bitzips->count, sno->inited, sno->stoped);
	if(sno->inited)
	{
		if (reader->cb_init_keys)
		{
			reader->cb_init_keys(reader, sno->work_keys, sis_sdslen(sno->work_keys));
		}
		if (reader->cb_init_sdbs)
		{
			reader->cb_init_sdbs(reader, sno->work_sdbs, sis_sdslen(sno->work_sdbs));
		}
		if (reader->cb_bits)
		{		
			int start = -1;
			for (int i = sno->out_bitzips->count - 1; i >= 0; i--)
			{
				s_zipdb_bits *memory = (s_zipdb_bits *)sis_pointer_list_get(sno->out_bitzips, i);
				if (memory->init)
				{
					start = i;
					break;
				}
			}
			if (start >= 0)
			{
				if (reader->ishead)
				{
					start = 0;
				} 
				for (int i = start; i < sno->out_bitzips->count; i++)
				{
					s_zipdb_bits *memory = (s_zipdb_bits *)sis_pointer_list_get(sno->out_bitzips, i);
					reader->cb_bits(reader, memory);
				}
			}	
		}
		else
		{
			// 如果不是压缩数据就不发送了
		}
	}

	if(sno->stoped)
	{
		if (reader->cb_sub_stop)
		{
			reader->cb_sub_stop(reader, sno->work_date);
		}
	}
	else
	{
		if (reader->cb_sub_realtime)
		{
			reader->cb_sub_realtime(reader, sno->work_date);
		}
	}
    return 0;
}

