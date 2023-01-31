
#include "worker.h"
#include "server.h"
#include "sis_method.h"
#include "sis_utils.h"
#include <frwdb.h>

static int cb_sub_incrzip(void *source, void *argv);
static int cb_sub_chars(void *source, void *argv);
static int cb_sub_start(void *source, void *argv);
static int cb_sub_realtime(void *source, void *argv);
static int cb_sub_stop(void *source, void *argv);
static int cb_dict_keys(void *source, void *argv);
static int cb_dict_sdbs(void *source, void *argv);

int frwdb_register_reader(s_frwdb_cxt *context_, s_sis_message *netmsg)
{
    s_frwdb_cxt *context = (s_frwdb_cxt *)context_;
	
	s_frwdb_reader *reader = frwdb_reader_create();
	reader->cid = netmsg->cid;
	reader->status = SIS_SUB_STATUS_INIT;
	if (netmsg->name)
	{
		reader->cname = sis_sdsdup(netmsg->name);
	}
	reader->iszip = true;
	reader->rfmt = SISDB_FORMAT_BITZIP;
    reader->father = context;
    reader->ishead = 0;
	reader->sub_date   = context->work_date;
	reader->start_date = context->work_date;
	reader->stop_date  = context->work_date;
    if (netmsg->info)
    {
        s_sis_json_handle *argnode = sis_json_load(netmsg->info, sis_sdslen(netmsg->info));
        if (argnode)
        {
			reader->rfmt = sis_db_get_format_from_node(argnode->node, reader->rfmt);
            reader->ishead = sis_json_get_int(argnode->node, "sub-head", 0);
            reader->sub_date = sis_json_get_int(argnode->node, "sub-date", context->work_date);
            reader->start_date = sis_json_get_int(argnode->node, "start-date", reader->sub_date);
            reader->stop_date = sis_json_get_int(argnode->node, "stop-date", reader->sub_date);
            sis_json_close(argnode);
        }
	}
	// 默认压缩传输
	if (reader->rfmt != SISDB_FORMAT_BITZIP)
	{
		reader->iszip = false;
	}
	if (reader->iszip)
	{
		reader->cb_sub_incrzip  = cb_sub_incrzip;
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
	
    sis_str_divide_sds(netmsg->subject, '.', &reader->sub_keys, &reader->sub_sdbs);
	if (!reader->sub_keys || !reader->sub_sdbs || 
		sis_sdslen(reader->sub_keys) < 1 || sis_sdslen(reader->sub_sdbs) < 1)
	{
		LOG(5)("format error. %s\n", netmsg->subject);
		frwdb_reader_destroy(reader);
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
	printf("%s %d %d | %d\n", __func__, reader->sub_date, context->work_date, context->status);
	if (reader->sub_date > 0 && reader->sub_date != context->work_date)
	{
		reader->sub_disk = true;
		// 启动历史数据线程 并返回 
		// 如果断线要能及时中断文件读取 
		// 同一个用户 必须等待上一次读取中断后才能开始新的任务
		if (!frwdb_reader_new_history(reader))
		{
			frwdb_reader_destroy(reader);
		}
	}
	else // 大于当前日期 或者等于但未停止 
	{		
		reader->sub_disk = false;
		// 一个 socket 只能订阅一次 后订阅的会冲洗掉前面一次
		frwdb_reader_new_realtime(reader);
	}
    return SIS_METHOD_OK;
}

s_sis_object *frwdb_read_get_obj(s_frwdb_reader *reader_)
{
	s_frwdb_cxt *snodb = (s_frwdb_cxt *)reader_->father;
	if (!snodb->rfile_config)
	{
		return NULL;
	}
	reader_->sub_disker = sis_worker_create(NULL, snodb->rfile_config);
	s_sis_message *msg = sis_message_create();
	sis_message_set_int(msg, "sub-date",   reader_->sub_date);
	sis_message_set_int(msg, "start-date", reader_->start_date);
	sis_message_set_int(msg, "stop-date",  reader_->stop_date);
	sis_message_set_str(msg, "sub-keys",   reader_->sub_keys, sis_sdslen(reader_->sub_keys));
	sis_message_set_str(msg, "sub-sdbs",   reader_->sub_sdbs, sis_sdslen(reader_->sub_sdbs));
	sis_message_set_int(msg, "format",     reader_->rfmt);
	// 只能得到单表单键的数据
	s_sis_object *obj = NULL;
	if (sis_worker_command(reader_->sub_disker, "get", msg) == SIS_METHOD_OK)
	{
		obj = sis_message_get(msg, "info");
		if (obj)
		{
			sis_object_incr(obj);
			// sis_out_binary("..2..", SIS_OBJ_GET_CHAR(obj), SIS_OBJ_GET_SIZE(obj));
		}
	}
	sis_message_destroy(msg);
	sis_worker_destroy(reader_->sub_disker);
	reader_->sub_disker = NULL;
	return obj;
}
// 只支持字符串数据获取 只支持单个结构单个品种
int frwdb_read(s_frwdb_cxt *context_, s_sis_message *netmsg)
{
    s_frwdb_cxt *context = (s_frwdb_cxt *)context_;

	if (sis_is_multiple_sub(netmsg->subject, sis_sdslen(netmsg->subject)))
	{
		// 不支持多键值获取
		return SIS_METHOD_NIL;
	}
	s_frwdb_reader *reader = frwdb_reader_create();
	reader->iszip = false;
	reader->cid = netmsg->cid;
	if (netmsg->name)
	{
		reader->cname = sis_sdsdup(netmsg->name);
	}
	reader->rfmt = SISDB_FORMAT_JSON;
    reader->father = context;
    reader->ishead = 1;
	reader->sub_date   = context->work_date;
	reader->start_date = context->work_date;
	reader->stop_date  = context->work_date;
    if (netmsg->info)
    {
        s_sis_json_handle *argnode = sis_json_load(netmsg->info, sis_sdslen(netmsg->info));
        if (argnode)
        {
			reader->rfmt = sis_db_get_format_from_node(argnode->node, reader->rfmt);
            reader->ishead = sis_json_get_int(argnode->node, "sub-head", 1);
            reader->sub_date = sis_json_get_int(argnode->node, "sub-date", context->work_date);
            reader->start_date = sis_json_get_int(argnode->node, "start-date", reader->sub_date);
            reader->stop_date = sis_json_get_int(argnode->node, "stop-date", reader->sub_date);
            sis_json_close(argnode);
        }
	}
	if (reader->rfmt == SISDB_FORMAT_BITZIP)
	{
		reader->iszip = true;
	}
	reader->sub_whole = false;
    sis_str_divide_sds(netmsg->subject, '.', &reader->sub_keys, &reader->sub_sdbs);
	if (!reader->sub_keys || !reader->sub_sdbs || 
		sis_sdslen(reader->sub_keys) < 1 || sis_sdslen(reader->sub_sdbs) < 1)
	{
		frwdb_reader_destroy(reader);
		return SIS_METHOD_ERROR;
	}
	
	s_sis_object *obj = NULL;
	printf("%s %d %d %d | %s %s\n", __func__, reader->sub_date ,context->work_date, context->status, reader->sub_keys, reader->sub_sdbs);
	if (reader->sub_date != context->work_date || 
		(context->status == SIS_SUB_STATUS_STOP) || 
		(context->status == SIS_SUB_STATUS_NONE))
	{
		reader->sub_disk = true;
		// 请求历史数据 
		obj = frwdb_read_get_obj(reader);
	}
	else
	{		
		// printf("===1.2==\n");	
		reader->sub_disk = false;
		// 从实时数据中获取对应数据
		s_sis_memory *imem = sis_memory_create_size(16 * 1024 * 1024);
		s_sis_node_list *nodes = sis_map_pointer_get(context->map_data, netmsg->subject);
		// printf("nodes = %d %s\n", sis_map_pointer_getsize(context->map_data), netmsg->subject);
		if (frwdb_wfile_nodes_to_memory(nodes, imem))
		{
			obj = sis_object_create(SIS_OBJECT_MEMORY, imem);
		}
		else
		{
			sis_memory_destroy(imem);
		}
	}	
	if (!obj)
	{
		frwdb_reader_destroy(reader);
		return SIS_METHOD_NIL;
	}
	if (reader->rfmt & SISDB_FORMAT_BYTES)
	{
		if (reader->iszip)
		{
			// 数据压缩后再发送
			LOG(5)("not zip.\n");
		}
		else
		{
		    sis_net_message_set_byte(netmsg, SIS_OBJ_GET_CHAR(obj), SIS_OBJ_GET_SIZE(obj));  
		}	
	}
	else
	{
		sis_net_message_set_char(netmsg, SIS_OBJ_GET_CHAR(obj), SIS_OBJ_GET_SIZE(obj));  
	}
	frwdb_reader_destroy(reader);
	sis_object_destroy(obj);

	return SIS_METHOD_OK;
}

///////////////////////////////////////////////////////////////////////////
//------------------------s_frwdb_reader callback -----------------------//
///////////////////////////////////////////////////////////////////////////

// 直接返回一个数据块 压缩数据
// #define MSERVER_DEBUG
#ifdef MSERVER_DEBUG
static int _bitzip_nums = 0;
static int64 _bitzip_size = 0;
static msec_t _bitzip_msec = 0;
#endif
static int cb_sub_incrzip(void *source, void *argv)
{
    // printf("%s\n", __func__);
    s_frwdb_reader *reader = (s_frwdb_reader *)source;
    s_frwdb_cxt *context = (s_frwdb_cxt *)reader->father;
    s_sis_db_incrzip *inmem = (s_sis_db_incrzip *)argv;
    // 向对应的socket发送数据
    s_sis_message *newinfo = sis_net_message_create();
    newinfo->name = reader->cname ? sis_sdsdup(reader->cname) : NULL;
    newinfo->cid = reader->cid;
#ifdef MSERVER_DEBUG
    _bitzip_nums++;
    _bitzip_size += sizeof(s_frwdb_compress) + inmem->size;
    if (_bitzip_nums % 100 == 0)
    {
        LOG(8)("server send zip : %d %lld(k) cost :%lld\n", _bitzip_nums, _bitzip_size/1000, sis_time_get_now_msec() - _bitzip_msec);
        _bitzip_msec = sis_time_get_now_msec();
    } 
#endif
    sis_net_message_set_cmd(newinfo, "zpub");
	sis_net_message_set_byte(newinfo, (char *)inmem->data, inmem->size); 
	// SIS_NET_SHOW_MSG("send", newinfo);
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
    s_frwdb_reader *reader = (s_frwdb_reader *)source;
    s_frwdb_cxt *context = (s_frwdb_cxt *)reader->father;
    s_sis_db_chars *inmem = (s_sis_db_chars *)argv;
    // 向对应的socket发送数据

    // printf("%s %d %s\n", __func__, reader->rfmt, inmem->sname);
	s_sis_message *newinfo = sis_net_message_create();
	newinfo->name = reader->cname ? sis_sdsdup(reader->cname) : NULL;
	newinfo->cid = reader->cid;
	sis_net_message_set_cmd(newinfo, "pub");
	sis_net_message_set_subject(newinfo, inmem->kname, inmem->sname);
	if (reader->rfmt & SISDB_FORMAT_BYTES)
	{
		sis_net_message_set_byte(newinfo, inmem->data, inmem->size); 
	}
	else
	{
		s_sis_dynamic_db *db = sis_map_list_get(context->map_sdbs, inmem->sname);
		s_sis_sds omem = sis_db_format_sds(db, NULL, reader->rfmt, (const char *)inmem->data, inmem->size, 0);
		// printf("%s %d : %s\n",db->name, reader->rfmt, omem);
		sis_net_message_set_char(newinfo, omem, sis_sdslen(omem));
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
    s_frwdb_reader *reader = (s_frwdb_reader *)source;
    s_frwdb_cxt *context = (s_frwdb_cxt *)reader->father;
    const char *workdate = (const char *)argv;
    // 向对应的socket发送数据
    s_sis_message *newinfo = sis_net_message_create();
    newinfo->cid = reader->cid;
    newinfo->name = reader->cname ? sis_sdsdup(reader->cname) : NULL;
    sis_net_msg_tag_sub_start(newinfo, workdate);
    if (context->cb_net_message)
	{
		context->cb_net_message(context->cb_source, newinfo);
	}
    sis_net_message_destroy(newinfo); 
    return 0;
}
static int cb_sub_realtime(void *source, void *argv)
{
    s_frwdb_reader *reader = (s_frwdb_reader *)source;
    s_frwdb_cxt *context = (s_frwdb_cxt *)reader->father;
    const char *workdate = (const char *)argv;
    // 向对应的socket发送数据
    s_sis_message *newinfo = sis_net_message_create();
    newinfo->cid = reader->cid;
    newinfo->name = reader->cname ? sis_sdsdup(reader->cname) : NULL;
    sis_net_msg_tag_sub_wait(newinfo, workdate);
    if (context->cb_net_message)
	{
		context->cb_net_message(context->cb_source, newinfo);
	}
    sis_net_message_destroy(newinfo); 
    return 0;
}

static int cb_sub_stop(void *source, void *argv)
{ 
    s_frwdb_reader *reader = (s_frwdb_reader *)source;
    s_frwdb_cxt *context = (s_frwdb_cxt *)reader->father;
    const char *workdate = (const char *)argv;
    LOG(5)("server send sub stop. %s [%d]\n", (char *)argv, reader->cid);
    // 向对应的socket发送数据
    s_sis_message *newinfo = sis_net_message_create();
    newinfo->cid = reader->cid;
    newinfo->name = reader->cname ? sis_sdsdup(reader->cname) : NULL;
    sis_net_msg_tag_sub_stop(newinfo, workdate);
    if (context->cb_net_message)
	{
		context->cb_net_message(context->cb_source, newinfo);
	}
    sis_net_message_destroy(newinfo); 
	// 如果是磁盘 订阅完毕就销毁 不能要否则会陷入回调死锁
	// if (reader->sub_disk)
	// {
	// 	frwdb_remove_reader(context, reader->cid);
	// }
    return 0;
}
static int cb_dict_keys(void *source, void *argv)
{
    s_frwdb_reader *reader = (s_frwdb_reader *)source;
    s_frwdb_cxt *context = (s_frwdb_cxt *)reader->father;
    char *keys = (char *)argv;
    // 向对应的socket发送数据
    s_sis_message *newinfo = sis_net_message_create();
    newinfo->cid = reader->cid;
    newinfo->name = reader->cname ? sis_sdsdup(reader->cname) : NULL;
	
	sis_net_message_set_cmd(newinfo, "set");
	sis_net_message_set_tag(newinfo, SIS_NET_TAG_SUB_KEY);
    sis_net_message_set_char(newinfo, keys, sis_strlen(keys));
	if (context->cb_net_message)
	{
		context->cb_net_message(context->cb_source, newinfo);
	}
    sis_net_message_destroy(newinfo); 
    return 0;
}
static int cb_dict_sdbs(void *source, void *argv)
{
    s_frwdb_reader *reader = (s_frwdb_reader *)source;
    s_frwdb_cxt *context = (s_frwdb_cxt *)reader->father;
	char *sdbs = (char *)argv;
    // 向对应的socket发送数据
    s_sis_message *newinfo = sis_net_message_create();
    newinfo->cid = reader->cid;
    newinfo->name = reader->cname ? sis_sdsdup(reader->cname) : NULL;
	sis_net_message_set_cmd(newinfo, "set");
	sis_net_message_set_tag(newinfo, SIS_NET_TAG_SUB_SDB);
	sis_net_message_set_char(newinfo, sdbs, sis_strlen(sdbs));
	if (context->cb_net_message)
	{
		context->cb_net_message(context->cb_source, newinfo);
	}
    sis_net_message_destroy(newinfo); 
    return 0;
}
///////////////////////////////////////////////////////////////////////////
//------------------------s_frwdb_reader --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_frwdb_reader *frwdb_reader_create()
{
	s_frwdb_reader *o = SIS_MALLOC(s_frwdb_reader, o);
	return o;
}
void frwdb_reader_destroy(void *reader_)
{
	s_frwdb_reader *reader = (s_frwdb_reader *)reader_;
	// 必须先关闭读句柄 数据才会不再写入 再关闭其他
	// LOG(8)("reader close. cid = %d %d %p %p | %p %p\n", reader->cid, reader->sub_disk, 
	// 	reader->reader, reader->sub_disker,
	// 	reader->sub_unziper, reader->sub_ziper);
	if (reader->reader)
	{
		sis_lock_reader_close(reader->reader);	
	}
	if (reader->sub_disk)
	{
		frwdb_reader_history_stop(reader);
	}

	if (reader->sub_unziper)
	{
		sisdb_incr_unzip_stop(reader->sub_unziper);
		sisdb_incr_destroy(reader->sub_unziper);	
		reader->sub_unziper = NULL;
	}
	if (reader->sub_ziper)
	{
		// 次级退出 上级会报异常 onebyone = 1
		sisdb_incr_zip_stop(reader->sub_ziper);
		sisdb_incr_destroy(reader->sub_ziper);
		reader->sub_ziper = NULL;	
	}
	sis_sdsfree(reader->sub_keys);
	sis_sdsfree(reader->sub_sdbs);
	sis_sdsfree(reader->cname);
	LOG(8)("reader close ok. cid = %d\n", reader->cid);
	sis_free(reader);
}

int frwdb_reader_new_history(s_frwdb_reader *reader_)
{
    s_frwdb_cxt *snodb = (s_frwdb_cxt *)reader_->father;
	// 清除该端口其他的订阅
	s_frwdb_reader *reader = sis_map_kint_get(snodb->map_reader_disk, reader_->cid);
	if (reader)
	{
		frwdb_remove_reader(snodb, reader_->cid);
	}
	frwdb_reader_history_start(reader_);
	if (reader_->sub_disker)
	{
		// 成功创建磁盘工作者 表示有文件并开始工作
		sis_map_kint_set(snodb->map_reader_disk, reader_->cid, reader_);
		return 1;
	}		
	return 0;
}
int frwdb_reader_new_realtime(s_frwdb_reader *reader_)
{
    s_frwdb_cxt *snodb = (s_frwdb_cxt *)reader_->father; 

	s_frwdb_reader *reader = sis_map_kint_get(snodb->map_reader_curr, reader_->cid);
	if (!reader)
	{
		sis_map_kint_set(snodb->map_reader_curr, reader_->cid, reader_);
		return 1;
	}
	return 0;
}

int frwdb_remove_reader(s_frwdb_cxt *frwdb_, int cid_)
{
	{
		// 检查实时
		s_frwdb_reader *reader = sis_map_kint_get(frwdb_->map_reader_curr, cid_);
		if (reader)
		{
			// 实时订阅下面这句会异常
			sis_map_kint_del(frwdb_->map_reader_curr, cid_);
			frwdb_->curr_readers--;
			frwdb_->curr_readers = sis_max(frwdb_->curr_readers, 0);
		}
	}
	{
		// 检查历史 
		s_frwdb_reader *reader = sis_map_kint_get(frwdb_->map_reader_disk, cid_);
		if (reader)
		{
			sis_map_kint_del(frwdb_->map_reader_disk, cid_);
		}
	}
	return sis_map_kint_getsize(frwdb_->map_reader_curr) + sis_map_kint_getsize(frwdb_->map_reader_disk);
}


// static void cb_output_reader_send(s_frwdb_reader *reader, s_sis_db_incrzip *imem)
// {
// 	// printf("cb_output_reader_send = %d : %d\n", reader->sub_whole, imem->init);
// 	if (reader->sub_whole)
// 	{
// 		// printf("cb_output_reader_send = %p : %d\n", reader->cb_sub_incrzip, imem->init);
// 		// 订阅全部就直接发送
// 		if (reader->cb_sub_incrzip)
// 		{	
// 			reader->cb_sub_incrzip(reader, imem);
// 		}
// 	}
// 	else
// 	{
// 		// 订阅不是全部直接送到unzip中
// 		sisdb_incr_unzip_set(reader->sub_unziper, imem);
// 	}
// }
// static int cb_output_reader(void *reader_, s_sis_object *obj_)
// {
// 	s_frwdb_reader *reader = (s_frwdb_reader *)reader_;
// 	s_frwdb_cxt *snodb = (s_frwdb_cxt *)reader->father;
	
// 	s_sis_db_incrzip zmem = {0};
// 	zmem.data = (uint8 *)SIS_OBJ_GET_CHAR(obj_);
// 	zmem.size = SIS_OBJ_GET_SIZE(obj_);
// 	zmem.init = sis_incrzip_isinit(zmem.data, zmem.size);
// 	// printf("cb_output_reader = %d %d\n",reader->isinit, reader->ishead);
// 	if (!reader->isinit)
// 	{
// 		if (!zmem.init)
// 		{
// 			return 0;
// 		}
// 		if (reader->ishead)
// 		{
// 			reader->isinit = true;
// 			cb_output_reader_send(reader, &zmem);
// 		}
// 		else
// 		{
// 			// 当前包是最新的起始包 就开始发送数据
// 			printf("near_obj = %p  %p %d | %d\n", snodb->near_object, obj_, snodb->near_object == obj_, snodb->outputs->work_queue->rnums);
// 			if (snodb->near_object == obj_)
// 			{
// 				reader->isinit = true;
// 				cb_output_reader_send(reader, &zmem);
// 			}
// 		}		
// 	}
// 	else
// 	{
// 		cb_output_reader_send(reader, &zmem);
// 	}	
// 	return 0;
// }

// static int cb_output_realtime(void *reader_)
// {
// 	s_frwdb_reader *reader = (s_frwdb_reader *)reader_;
// 	s_frwdb_cxt *snodb = (s_frwdb_cxt *)reader->father;

// 	// printf("========cb_output_realtime \n");
// 	char sdate[32];
// 	sis_llutoa(snodb->work_date, sdate, 32, 10);
// 	if(snodb->status != SIS_SUB_STATUS_STOP)
// 	{
// 		if (reader->cb_sub_realtime)
// 		{
// 			reader->cb_sub_realtime(reader, sdate);
// 		}
// 	}
// 	else
// 	{
// 		if (reader->cb_sub_stop)
// 		{
// 			reader->cb_sub_stop(reader, sdate);
// 		}		
// 	}
// 	return 0;
// }

// static int cb_encode(void *context_, char *in_, size_t ilen_)
// {
// 	s_frwdb_reader *reader = (s_frwdb_reader *)context_;
// 	// s_frwdb_cxt *snodb = (s_frwdb_cxt *)reader->father;
// 	// printf("cb_encode %p %p\n", reader->sub_ziper, reader->cb_sub_incrzip);
// 	if (reader->cb_sub_incrzip)
// 	{
//         s_sis_db_incrzip zmem = {0};
//         zmem.data = (uint8 *)in_;
//         zmem.size = ilen_;
//         zmem.init = sis_incrzip_isinit(zmem.data, zmem.size);
// 		reader->cb_sub_incrzip(reader, &zmem);
// 	}
//     return 0;
// } 
// static int cb_decode(void *context_, int kidx_, int sidx_, char *in_, size_t ilen_)
// {
// 	s_frwdb_reader *reader = (s_frwdb_reader *)context_;
// 	// s_frwdb_cxt *snodb = (s_frwdb_cxt *)reader->father;
// 	// return; // 只解压什么也不干
// 	if (reader->cb_sub_chars)
// 	{
// 		if (!in_)
// 		{
// 			// 数据包结束
// 			return 0;
// 		}
// 		const char *kname = sisdb_incr_get_kname(reader->sub_unziper, kidx_);
// 		const char *sname = sisdb_incr_get_sname(reader->sub_unziper, sidx_);

// 		if (!sname || !kname)
// 		{
// 			return 0;
// 		}
// 		int kidx = sisdb_incr_get_kidx(reader->sub_ziper, kname);
// 		int sidx = sisdb_incr_get_kidx(reader->sub_ziper, sname);
// 		if (kidx < 0 || sidx < 0)
// 		{
// 			// 通过此判断是否是需要的key和sdb
// 			return 0;
// 		}
// 		s_sis_db_chars inmem = {0};
// 		inmem.kname = kname;
// 		inmem.sname = sname;
// 		inmem.data = in_;
// 		inmem.size = ilen_;
// 		reader->cb_sub_chars(reader, &inmem);
// 	}
// 	if (reader->cb_sub_incrzip)
// 	{
// 		if (!in_)
// 		{
// 			// if (ilen_ > 0 ) LOG(5)("==== curr_size = %d\n", sisdb_incr_getsize(reader->sub_ziper));
// 			sisdb_incr_zip_restart(reader->sub_ziper);	
// 			return 0;
// 		}
// 		// 先从大表中得到实际名称
// 		const char *kname = sisdb_incr_get_kname(reader->sub_unziper, kidx_);
// 		const char *sname = sisdb_incr_get_sname(reader->sub_unziper, sidx_);
// 		// printf("cb_decode %p %d %d | %d %d | %d %d\n", in_, kidx_, sidx_, 
// 		// 	sis_map_list_getsize(reader->sub_unziper->work_keys), 
// 		// 	sis_map_list_getsize(reader->sub_unziper->work_sdbs),
// 		// 	sis_map_list_getsize(reader->sub_ziper->work_keys),
// 		// 	sis_map_list_getsize(reader->sub_ziper->work_sdbs));

// 		if (!sname || !kname)
// 		{
// 			return 0;
// 		}
// 		int kidx = sisdb_incr_get_kidx(reader->sub_ziper, kname);
// 		int sidx = sisdb_incr_get_sidx(reader->sub_ziper, sname);
// 		if (kidx < 0 || sidx < 0)
// 		{
// 			// LOG(5)("fail = %s %s -> %d  %d | %d  %d\n", kname, sname, kidx, sidx, kidx_, sidx_);
// 			// 通过此判断是否是需要的key和sdb
// 			return 0;
// 		}

// 		sisdb_incr_zip_set(reader->sub_ziper, kidx, sidx, in_, ilen_);
// 		// LOG(5)("==== cb_unzip_reply = %s %s -> %d  %d | %d  %d | === %d\n", kname, sname, kidx, sidx, kidx_, sidx_, sisdb_incr_getsize(reader->sub_ziper));

// 	}
//     return 0;
// }
// 只能在确定 start 时处理 定制解压相关类
int frwdb_reader_realtime_start(s_frwdb_reader *reader_)
{
    // s_frwdb_cxt *snodb = (s_frwdb_cxt *)reader_->father; 
    
	// LOG(5)("sub start : date = %d - %d\n", snodb->work_date ,reader_->sub_date);
	
	// s_sis_sds work_keys =  NULL;
	// s_sis_sds work_sdbs =  NULL;

	// if (!reader_->sub_whole)
	// {
	// 	if (!reader_->sub_ziper)
	// 	{
	// 		reader_->sub_ziper = sisdb_incr_create();
	// 	}
	// 	else
	// 	{
	// 		sisdb_incr_clear(reader_->sub_ziper);			
	// 	}  

	// 	work_keys = sis_match_key(reader_->sub_keys, snodb->work_keys);
	// 	if (!work_keys)
	// 	{
	// 		work_keys =  sis_sdsdup(snodb->work_keys);
	// 	}
	// 	work_sdbs = sis_match_sdb_of_sds(reader_->sub_sdbs, snodb->work_sdbs);
	// 	if (!work_sdbs)
	// 	{
	// 		work_sdbs =  sis_sdsdup(snodb->work_sdbs);
	// 	}
	// 	sisdb_incr_set_keys(reader_->sub_ziper, work_keys);
	// 	sisdb_incr_set_sdbs(reader_->sub_ziper, work_sdbs);
	// 	sisdb_incr_zip_start(reader_->sub_ziper, reader_, cb_encode);

	// 	if (!reader_->sub_unziper)
	// 	{
	// 		reader_->sub_unziper = sisdb_incr_create();
	// 	}
	// 	else
	// 	{
	// 		sisdb_incr_clear(reader_->sub_unziper);
	// 	}	
    //     sisdb_incr_set_keys(reader_->sub_unziper, snodb->work_keys);
    //     sisdb_incr_set_sdbs(reader_->sub_unziper, snodb->work_sdbs);
	// 	sisdb_incr_unzip_start(reader_->sub_unziper, reader_, cb_decode);
	// }
	// else
	// {
	// 	work_keys =  snodb->work_keys;
	// 	work_sdbs =  snodb->work_sdbs;
	// }

	// if (reader_->cb_sub_start)
	// {
	// 	char sdate[32];
	// 	sis_llutoa(snodb->work_date, sdate, 32, 10);
	// 	reader_->cb_sub_start(reader_, sdate);
	// }
	// printf("frwdb_reader_realtime_start count = %d status = %d \n", snodb->outputs->work_queue->rnums, snodb->status);
	// if (reader_->cb_dict_keys)
	// {
	// 	reader_->cb_dict_keys(reader_, work_keys);
	// }
	// if (reader_->cb_dict_sdbs)
	// {
	// 	reader_->cb_dict_sdbs(reader_, work_sdbs);
	// }

	// if (!reader_->sub_whole)
	// {
	// 	sis_sdsfree(work_keys);
	// 	sis_sdsfree(work_sdbs);
	// }
	
	// reader_->isinit = false;
	// // 新加入的订阅者 先发送订阅开始 再发送最后一个初始块后续所有数据 
	// if (!reader_->reader)
	// {
	// 	reader_->reader = sis_lock_reader_create(snodb->outputs, 
	// 		// reader_->ishead ? SIS_UNLOCK_READER_HEAD : SIS_UNLOCK_READER_TAIL, 
	// 		SIS_UNLOCK_READER_HEAD,
	// 		reader_, cb_output_reader, cb_output_realtime);
	// 	sis_lock_reader_open(reader_->reader);	
	// }
    return 0;
}

int frwdb_reader_realtime_stop(s_frwdb_reader *reader_)
{    
    // s_frwdb_cxt *snodb = (s_frwdb_cxt *)reader_->father; 
    
	// LOG(5)("sub stop : date = %d - %d\n", snodb->work_date ,reader_->sub_date);
	// if (reader_->cb_sub_stop)
	// {
	// 	char sdate[32];
	// 	sis_llutoa(snodb->work_date, sdate, 32, 10);
	// 	reader_->cb_sub_stop(reader_, sdate);
	// }
	// if (!reader_->sub_whole)
	// {
	// 	if (reader_->sub_ziper)
	// 	{
	// 		sisdb_incr_zip_stop(reader_->sub_ziper);
	// 		sisdb_incr_destroy(reader_->sub_ziper);
	// 		reader_->sub_ziper = NULL;
	// 	}
	// 	if (reader_->sub_unziper)
	// 	{
	// 		sisdb_incr_zip_stop(reader_->sub_unziper);
	// 		sisdb_incr_destroy(reader_->sub_unziper);
	// 		reader_->sub_unziper = NULL;
	// 	}
	// }
	// if (reader_->reader)
	// {
	// 	sis_lock_reader_close(reader_->reader);	
	// 	reader_->reader = NULL;
	// }
	return 0;
}

void frwdb_reader_curr_init(s_frwdb_cxt *context)
{
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(context->map_reader_curr);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_frwdb_reader *reader = (s_frwdb_reader *)sis_dict_getval(de);
		reader->status = SIS_SUB_STATUS_INIT;
	}
	sis_dict_iter_free(di);
	context->curr_readers = 0;
}

void frwdb_reader_curr_start(s_frwdb_cxt *context)
{
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(context->map_reader_curr);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_frwdb_reader *reader = (s_frwdb_reader *)sis_dict_getval(de);
        LOG(5)("send sub start.[%d] %d %d %d\n", reader->cid, reader->status, context->work_date, context->curr_readers);
		if (reader->status != SIS_SUB_STATUS_INIT)
        {
            continue;
        }
		// frwdb_reader_realtime_start(reader);
		reader->status = SIS_SUB_STATUS_WORK;
		context->curr_readers ++;
	}
}
void frwdb_reader_curr_write(s_frwdb_cxt *context, s_sis_db_chars *chars)
{
	// 这里开始发送数据
	// s_sis_dict_entry *de;
	// s_sis_dict_iter *di = sis_dict_get_iter(context->map_reader_curr);
	// while ((de = sis_dict_next(di)) != NULL)
	// {
	// 	s_frwdb_reader *reader = (s_frwdb_reader *)sis_dict_getval(de);
	// }
	// sis_dict_iter_free(di);
}
void frwdb_reader_curr_stop(s_frwdb_cxt *context)
{
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(context->map_reader_curr);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_frwdb_reader *reader = (s_frwdb_reader *)sis_dict_getval(de);
		if (reader->status != SIS_SUB_STATUS_STOP)
		{
			LOG(5)("snodb stop. cid : %d workdate : %d\n", reader->cid, context->work_date);
			// frwdb_reader_realtime_stop(reader);
			reader->status = SIS_SUB_STATUS_STOP;
		}
	}
	sis_dict_iter_free(di);
	context->curr_readers = 0;
}


///////////////////////////////////////////
//  callback define begin
///////////////////////////////////////////

int frwdb_reader_history_start(s_frwdb_reader *reader_)
{

	s_frwdb_cxt *snodb = (s_frwdb_cxt *)reader_->father;
	if (!snodb->rfile_config)
	{
		return 0;
	}
	reader_->sub_disker = sis_worker_create(NULL, snodb->rfile_config);

	s_sis_message *msg = sis_message_create();
	sis_message_set_int(msg, "sub-date",   reader_->sub_date);
	sis_message_set_int(msg, "start-date", reader_->start_date);
	sis_message_set_int(msg, "stop-date",  reader_->stop_date);
	sis_message_set_str(msg, "sub-keys",   reader_->sub_keys, sis_sdslen(reader_->sub_keys));
	sis_message_set_str(msg, "sub-sdbs",   reader_->sub_sdbs, sis_sdslen(reader_->sub_sdbs));

    sis_message_set(msg, "cb_source", reader_, NULL);
    sis_message_set_method(msg, "cb_sub_start" ,  reader_->cb_sub_start);
    sis_message_set_method(msg, "cb_sub_stop"  ,  reader_->cb_sub_stop );
    sis_message_set_method(msg, "cb_dict_sdbs" ,  reader_->cb_dict_sdbs);
    sis_message_set_method(msg, "cb_dict_keys" ,  reader_->cb_dict_keys);
	if (reader_->iszip)
	{
		sis_message_set_method(msg, "cb_sub_incrzip", reader_->cb_sub_incrzip);
	}
	else
	{
		sis_message_set_method(msg, "cb_sub_chars", reader_->cb_sub_chars);
	}

	if(sis_worker_command(reader_->sub_disker, "sub", msg) != SIS_METHOD_OK)
	{
		// 订阅
		sis_worker_destroy(reader_->sub_disker);
		reader_->sub_disker = NULL;
	}
	sis_message_destroy(msg);

	return 1;
}

int frwdb_reader_history_stop(s_frwdb_reader *reader_)
{
	printf("frwdb_reader_history_stop \n");
	if (reader_->sub_disker)
	{
		// sis_worker_command(reader_->sub_disker, "unsub", NULL);
		sis_worker_destroy(reader_->sub_disker);
		reader_->sub_disker = NULL;
	}
	printf("frwdb_reader_history_stop ok\n");
	return 0;
}

