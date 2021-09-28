
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
    {"init",      cmd_snodb_init,   SIS_METHOD_ACCESS_NONET, NULL},  // 
    {"set",       cmd_snodb_set,    SIS_METHOD_ACCESS_RDWR, NULL},   // 
    {"start",     cmd_snodb_start,  SIS_METHOD_ACCESS_RDWR, NULL},   // 开始发送数据 
    {"stop",      cmd_snodb_stop,   SIS_METHOD_ACCESS_RDWR, NULL},   // 数据流发布完成 此时不再接收数据
    {"ipub",      cmd_snodb_ipub,   SIS_METHOD_ACCESS_NONET, NULL},  // 发布数据流 单条数据 kidx+sidx+data
    {"pub",       cmd_snodb_pub,    SIS_METHOD_ACCESS_RDWR, NULL},   // 发布数据流 单条数据 key sdb data
    {"zpub",      cmd_snodb_zpub,   SIS_METHOD_ACCESS_NONET, NULL},  // 发布数据流 只有完成的压缩数据块
    {"zsub",      cmd_snodb_zsub,   SIS_METHOD_ACCESS_READ, NULL},   // 订阅压缩数据流 
    {"sub",       cmd_snodb_sub,    SIS_METHOD_ACCESS_READ, NULL},   // 订阅数据流 
    {"unsub",     cmd_snodb_unsub,  SIS_METHOD_ACCESS_READ, NULL},   // 取消订阅数据流 
    {"get",       cmd_snodb_get,    SIS_METHOD_ACCESS_READ, NULL},   // 订阅数据流 
    {"clear",     cmd_snodb_clear,  SIS_METHOD_ACCESS_NONET, NULL},  // 清理数据流 
// 磁盘工具
    {"rlog",      cmd_snodb_rlog,  0, NULL},  // 异常退出时加载磁盘数据
    {"wlog",      cmd_snodb_wlog,  0, NULL},  // 异常退出时加载磁盘数据
};
// 共享内存数据库
s_sis_modules sis_modules_snodb = {
    snodb_init,
    NULL,
    NULL,
    NULL,
    snodb_uninit,
    NULL,
    NULL,
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

static int64 _zipnums = 0;
static int64 _innums = 0;
static size_t _insize = 0;

static int cb_input_reader(void *context_, s_sis_object *imem_)
{	
	s_snodb_cxt *context = (s_snodb_cxt *)context_;
	
	if ((_innums++) % 100000 == 0)
	{
		LOG(8)("recv innums = %d %zu %p\n", _innums, _insize, context->work_ziper);
	}
	return 0;
	if (!context->inited || !context->work_ziper || context->status == 1)
	{
		// 必须有这个判断 否则正在初始化时 会收到 in==NULL的数据包 
		LOG(5)("snodb no init. %d %d zip : %p imem : %p \n", context->inited, context->work_date, context->work_ziper, imem_);
		return -1;
	}
	if (!imem_) // 为空表示无数据超时
	{
		// 数据超时就强制回调一次 只是为了尽快返回数据 不是要强制分段
		sisdb_incr_zip_restart(context->work_ziper, 0);
	}
	else
	{
		s_sis_memory *memory = SIS_OBJ_MEMORY(imem_);
		_insize += sis_memory_get_size(memory);
		int kidx = sis_memory_get_byte(memory, 4);
		int sidx = sis_memory_get_byte(memory, 2);
		sisdb_incr_zip_set(context->work_ziper, kidx, sidx, sis_memory(memory), sis_memory_get_size(memory));
	}
	return 0;
}

static int cb_wlog_reader(void *reader_, s_sis_object *obj_)
{
	s_snodb_reader *reader = (s_snodb_reader *)reader_;
	s_snodb_cxt *snodb = (s_snodb_cxt *)reader->father;
	// 从第一个初始包开始存盘
	s_sis_db_incrzip zmem;
	zmem.data = (uint8 *)SIS_OBJ_GET_CHAR(obj_);
	zmem.size = SIS_OBJ_GET_SIZE(obj_);
	zmem.init = sis_incrzip_isinit(zmem.data, zmem.size);
	
	if ((_zipnums++) % 100 == 0)
	{
		LOG(8)("recv zipnums = %d %d %d\n", reader->isinit, zmem.init, snodb->wlog_init);
	}
	if (!reader->isinit)
	{
		if (!zmem.init)
		{
			return 0;
		}
		reader->isinit = true;
	}
	// 无论是否有历史数据，都从新写一份keys和sdbs读取的时候也要更新dict表 以免数据混乱
	if(snodb->wlog_init == 0)
	{
		snodb_wlog_start(snodb);
		snodb_wlog_save(snodb, SICDB_FILE_SIGN_KEYS, snodb->work_keys, sis_sdslen(snodb->work_keys));
		snodb_wlog_save(snodb, SICDB_FILE_SIGN_SDBS, snodb->work_sdbs, sis_sdslen(snodb->work_sdbs));
		snodb->wlog_init = 1;
	}
	snodb_wlog_save(snodb, SICDB_FILE_SIGN_ZPUB, (char *)zmem.data, zmem.size);
	return 0;
}

bool snodb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
	s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_snodb_cxt *context = SIS_MALLOC(s_snodb_cxt, context);
    worker->context = context;
	context->work_name = sis_sdsnew(node->key);
	const char *workpath = sis_json_get_str(node, "work-path");
	if (workpath)
	{
		context->work_path = sis_sdsnew(workpath);
	}
	else
	{
		context->work_path = sis_sdsnew("./");
	}
    context->work_date = sis_time_get_idate(0);
	context->wait_msec = sis_json_get_int(node, "wait-msec", 30);//45); // 21;

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
 
	context->outputs = sis_lock_list_create(context->catch_size);
    context->map_keys = sis_map_list_create(sis_sdsfree_call); 
	context->map_sdbs = sis_map_list_create(sis_dynamic_db_destroy);

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
	if (context->wlog_worker)
	{
		// 先从目录中获取wlog中数据 并加载到内存中
		// 0 表示加载当前目录下有的文件
		int isload = snodb_wlog_load(context);
		SIS_WAIT_LONG(context->wlog_load == 0);
		LOG(5)("load wlog ok. %d\n", context->wlog_load);

		// 加载成功后 wlog 文件会被重写 以此来保证数据前后的一致性
		// snodb_wlog_remove(context);
		//  如何保证磁盘的code索引和重启后索引保持一致 
		//  传入数据时不能清理 keys 和 sdbs 才能不出错
		// 然后启动一个读者 订阅 outputs 中数据 然后实时写盘
		context->wlog_reader = snodb_reader_create();
		context->wlog_reader->father = context;
		context->wlog_reader->isinit = 0;
		context->wlog_reader->ishead = isload ? 0 : 1;
		context->wlog_init = 0; 
		// 已经有了log文件就从头订阅 否则从尾部订阅
		context->wlog_reader->reader = sis_lock_reader_create(context->outputs, 
			context->wlog_reader->ishead ? SIS_UNLOCK_READER_HEAD : SIS_UNLOCK_READER_TAIL, 
			context->wlog_reader, cb_wlog_reader, NULL);
		sis_lock_reader_open(context->wlog_reader->reader);	
		 
	}
	context->status = 0;
    return true;
}
void snodb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
	context->status = 1;

    if (context->wlog_worker)
    {
		snodb_reader_destroy(context->wlog_reader);
        sis_worker_destroy(context->wlog_worker);
		context->wlog_worker = NULL;
    }
    if (context->wfile_worker)
    {
        sis_worker_destroy(context->wfile_worker);
		context->wfile_worker = NULL;
    }
	sis_sdsfree(context->work_keys);
	sis_sdsfree(context->work_sdbs);
	sis_sdsfree(context->init_keys);
	sis_sdsfree(context->init_sdbs);

    sis_map_list_destroy(context->map_keys); 
    sis_map_list_destroy(context->map_sdbs);

	if (context->inputs)
	{
		sis_fast_queue_destroy(context->inputs);
	}
	sis_pointer_list_destroy(context->readeres);

	sis_lock_list_destroy(context->outputs);

	sis_sdsfree(context->work_name);
	sis_sdsfree(context->work_path);
	sis_free(context);
}


// 因为可能发到中间时也会调用该函数 init 时需要保证环境和最初的环境一致
int cmd_snodb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
    
	s_sis_message *msg = (s_sis_message *)argv_;
	context->cb_source = sis_message_get(msg, "cb_source");
	context->cb_net_message = sis_message_get_method(msg, "cb_net_message");
    return SIS_METHOD_OK;
}
int cmd_snodb_set(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

	if (!sis_strcasecmp(netmsg->key, "_keys_") && netmsg->switchs.has_ask)
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

// 设置
bool _snodb_write_start(s_snodb_cxt *context, int workdate)
{
	context->inited = false;
	context->stoped = false;
	if (!context->init_keys || !context->init_sdbs)
	{
		return false;
	}
	LOG(8)("_snodb_write_init : %d \n", workdate);

	sis_sdsfree(context->work_keys); 
	context->work_keys = sis_sdsdup(context->init_keys);
	sis_sdsfree(context->work_sdbs); 
	context->work_sdbs = sis_sdsdup(context->init_sdbs);

	sis_sdsfree(context->init_keys); 
	context->init_keys = NULL;
	sis_sdsfree(context->init_sdbs); 
	context->init_sdbs = NULL;

	sis_get_map_keys(context->work_keys, context->map_keys);
	sis_get_map_sdbs(context->work_sdbs, context->map_sdbs);
	// 这里为了保证二次进入 必须对压缩参数初始化
	if (context->inputs)
	{
		sis_fast_queue_clear(context->inputs);
	}
	context->near_object = NULL;
	// 清理输出缓存中可能存在的数据
	sis_lock_list_clear(context->outputs);

	context->work_date = workdate;
	// 如果是断线重连 没有经过 stop 需要特殊处理 
	context->inited = true;
	return true;
}
int cmd_snodb_start(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

	int work_date = netmsg->ask ? sis_atoll(netmsg->ask) : sis_time_get_idate(0);

	if (!_snodb_write_start(context, work_date))
	{
		return SIS_METHOD_ERROR;
	}

	// 初始化 wlog 开始新的一天 wlog 写入
	// if (context->wlog_reader && context->wlog_date != workdate_)
	// {
	// 	// log日期不同就重新生成文件
	// 	context->wlog_reader->isinit = 0;
	// 	context->wlog_init = 0;
	// }	
	printf("cmd_snodb_start : %d\n", context->inited);

	// 初始化后首先向所有订阅者发送订阅开始信息
	for (int i = 0; i < context->readeres->count; i++)
	{
		s_snodb_reader *reader = (s_snodb_reader *)sis_pointer_list_get(context->readeres, i);
		if (!reader->sub_disk)
		{
			snodb_reader_realtime_start(reader);
			LOG(5)("snodb start. [%d] cid : %d disk: %d workdate : %d = %s\n", 
				i,  reader->cid, reader->sub_disk, context->work_date, netmsg->ask);
		}
	}
	_zipnums = 0;   // 重新计数
	return SIS_METHOD_OK;
}
int cmd_snodb_stop(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	// printf("cmd_snodb_stop, curmemory : %d\n", context->cur_object->size);
	if (!context->inited || context->stoped)
	{
		return SIS_METHOD_ERROR;
	}
	if (context->work_ziper)
	{
		// 如果有压缩组件 就停止 
		sisdb_incr_zip_stop(context->work_ziper);
		sisdb_incr_destroy(context->work_ziper);
		context->work_ziper = NULL;
	}
	LOG(5)("snodb stop. wlog : %d readers : %d workdate : %d = %s\n", 
		context->wlog_worker, context->readeres->count, context->work_date, netmsg->ask);
	for (int i = 0; i < context->readeres->count; i++)
	{
		s_snodb_reader *reader = (s_snodb_reader *)sis_pointer_list_get(context->readeres, i);
		if (!reader->sub_disk)
		{
			LOG(5)("snodb stop. [%d] cid : %d workdate : %d = %s\n", 
				i,  reader->cid, context->work_date, netmsg->ask);
			snodb_reader_realtime_stop(reader);
		}
	}
	// 处理wlog文件
	if (context->wlog_worker)
	{
		// 停止写wlog文件
		snodb_wlog_stop(context);
		// 转格式
		if (context->wfile_worker)
		{
			if (snodb_wlog_to_snos(context) == SIS_METHOD_OK)
			{
				// 等待数据存盘完毕
				SIS_WAIT_LONG(context->wfile_save == 0);
				// 存盘结束清理 wlog
				snodb_wlog_remove(context);
			}
			// 数据转错 就不删除
		}
		else
		{
			snodb_wlog_remove(context);
		}
	}
	context->stoped = true; // 停止表示当日数据已经落盘
    return SIS_METHOD_OK;
}

int _snodb_write_incrzip(void *context_, char *imem_, size_t isize_)
{
	s_snodb_cxt *context = (s_snodb_cxt *)context_;
	if ((_zipnums++) % 100 == 0)
	{
		LOG(8)("zpub nums = %d %d\n", _zipnums, context->outputs->users->count);
	}
	if (!context->inited || context->stoped)
	{
		return -1;
	}
	// 直接写入
	s_sis_memory *memory = sis_memory_create();
	sis_memory_cat(memory, imem_, isize_);
	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, memory);
	int isinit = sis_incrzip_isinit((uint8 *)imem_, isize_);
	if (isinit == 1)
	{
		context->near_object = obj;
	}
	sis_lock_list_push(context->outputs, obj);

	sis_object_destroy(obj);
	return 0;
}

int _snodb_write_bytes(s_snodb_cxt *context, int kidx, int sidx, void *imem, size_t isize)
{
	if (!context->inited || context->stoped || kidx < 0 || sidx < 0)
	{
		return -1;
	}
	// 初始化 压缩
	if (!context->work_ziper)
	{
		context->work_ziper = sisdb_incr_create();
		sisdb_incr_set_keys(context->work_ziper, context->work_keys);
		sisdb_incr_set_sdbs(context->work_ziper, context->work_sdbs);
		sisdb_incr_zip_start(context->work_ziper, context, _snodb_write_incrzip);
		LOG(5)("no.... %d\n", context->work_ziper->incrzip->status);
	}
	if (!context->inputs)
	{
		context->inputs = sis_fast_queue_create(context, cb_input_reader, 30, context->wait_msec);
	}
	s_sis_memory *memory = sis_memory_create();
	sis_memory_cat_byte(memory, kidx, 4);
	sis_memory_cat_byte(memory, sidx, 2);
	sis_memory_cat(memory, (char *)imem, isize);	
	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, memory);
	// printf("-- input -- kidx_= %d, sidx_= %d isize = %zu\n", kidx, sidx, isize);
	sis_fast_queue_push(context->inputs, obj);
	
	sis_object_destroy(obj);
	return 1;
}

// #include "stk_struct.v0.h"
int cmd_snodb_ipub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
    s_sis_db_bytes *in = (s_sis_db_bytes *)argv_;
	_snodb_write_bytes(context, in->kidx, in->sidx, in->data, in->size);
    // sisdb_incr_zip_set(context->work_ziper, in->kidx, in->sidx, in->data, in->size);
    return SIS_METHOD_OK;
}
int cmd_snodb_pub(void *worker_, void *argv_)
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
		int kidx = sis_map_list_get_index(context->map_keys, kname);
		int sidx = sis_map_list_get_index(context->map_sdbs, sname); 
		_snodb_write_bytes(context, kidx, sidx, inmem, sis_sdslen(inmem));
		// sisdb_incr_zip_set(context->work_ziper, kidx, sidx, inmem, sis_sdslen(inmem));
	}
    return SIS_METHOD_OK;
}
// 从这里写入的数据为索引+二进制数据
int cmd_snodb_zpub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
    s_sis_db_incrzip *zmem = (s_sis_db_incrzip *)argv_;
    if (_snodb_write_incrzip(context, (char *)zmem->data, zmem->size) >= 0)
    {
        return SIS_METHOD_OK;
    }
    return SIS_METHOD_ERROR;
}

int cmd_snodb_zsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
	s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	return snodb_start_reader(context, netmsg, true);
}
int cmd_snodb_sub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
	s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	return snodb_start_reader(context, netmsg, false);
}
int cmd_snodb_unsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;

	snodb_remove_reader(context, netmsg->cid);

    return SIS_METHOD_OK;
}

int cmd_snodb_get(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
	s_snodb_cxt *context = (s_snodb_cxt *)worker->context;
	s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	// printf("%s %d\n",__func__, sis_strsub(netmsg->key, "*"));
	if (sis_is_multiple_sub(netmsg->key, sis_sdslen(netmsg->key)))
	{
		// 不支持多键值获取
		return SIS_METHOD_ERROR;
	}	
	return snodb_read(context, netmsg, false);
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
			snodb_remove_reader(context, cid);
		}		
	}
	if (!sis_strcasecmp(mode, "catch"))
	{
		context->near_object = NULL;
		sis_lock_list_clear(context->outputs);
		// 清理数据可能造成数据压缩的不连续 需要处理
	}	
    return SIS_METHOD_OK;
}
int cmd_snodb_wlog(void *worker_, void *argv_)
{
	return SIS_METHOD_OK;
}	
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

