
#include "sis_modules.h"
#include "worker.h"
#include "server.h"
#include "sis_method.h"
#include "sis_utils.h"

#include "frwdb.h"
#include "sis_net.msg.h"

///////////////////////////////////////////////////
// *** s_sis_modules sis_modules_[dir name]  *** //
///////////////////////////////////////////////////
// NULL 默认为读取
struct s_sis_method frwdb_methods[] = {
// 写入操作
    {"create",    cmd_frwdb_create, SIS_METHOD_ACCESS_RDWR,  NULL},  // 默认 json 格式
    {"setdb",     cmd_frwdb_setdb,  SIS_METHOD_ACCESS_RDWR, NULL},   // 设置数据表 可叠加 
    {"start",     cmd_frwdb_start,  SIS_METHOD_ACCESS_RDWR, NULL},   // 开始发送数据 设置写入状态日期
    {"set",       cmd_frwdb_set,    SIS_METHOD_ACCESS_RDWR, NULL},   // 写入数据 json单条数据 key sdb data
    {"bset",      cmd_frwdb_bset,   SIS_METHOD_ACCESS_RDWR, NULL},   // 写入数据 二进制数据 key sdb data
    {"stop",      cmd_frwdb_stop,   SIS_METHOD_ACCESS_RDWR, NULL},   // 数据写入完成 存盘 并按规则 pack
    {"save",      cmd_frwdb_save,   SIS_METHOD_ACCESS_RDWR, NULL},   // 对完整数据存盘
    {"merge",     cmd_frwdb_merge,  SIS_METHOD_ACCESS_RDWR, NULL},   // 数据写入完成 存盘 并按规则 
// 读取操作
    {"sub",       cmd_frwdb_sub,    SIS_METHOD_ACCESS_READ, NULL},   // 订阅数据流 
    {"unsub",     cmd_frwdb_unsub,  SIS_METHOD_ACCESS_READ, NULL},   // 取消订阅数据流 
    {"get",       cmd_frwdb_get,    SIS_METHOD_ACCESS_READ, NULL},   // 读取单键单表数据
// 磁盘工具
    {"init",      cmd_frwdb_init,   SIS_METHOD_ACCESS_NONET, NULL},  // 
    {"rlog",      cmd_frwdb_rlog,   0, NULL},  // 异常退出时加载磁盘数据
    {"wlog",      cmd_frwdb_wlog,   0, NULL},  // 实时保存写入磁盘操作
};
// 共享内存数据库
s_sis_modules sis_modules_frwdb = {
    frwdb_init,
    NULL,
    frwdb_working,
    NULL,
    frwdb_uninit,
    NULL,
    NULL,
    sizeof(frwdb_methods) / sizeof(s_sis_method),
    frwdb_methods,
};

///////////////////////////////////////////////////////////////////////////
//------------------------s_frwdb_cxt --------------------------------//
///////////////////////////////////////////////////////////////////////////

bool frwdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
	s_sis_json_node *node = (s_sis_json_node *)argv_;

    s_frwdb_cxt *context = SIS_MALLOC(s_frwdb_cxt, context);
    worker->context = context;
	context->work_path = sis_sds_save_create(sis_json_get_str(node, "work-path"), "data");   
    context->work_name = sis_sds_save_create(sis_json_get_str(node, "work-name"), node->key);   
	context->save_time = sis_json_get_int(node, "save-time", 0);
	// 写入的数据信息
	context->map_keys = sis_map_list_create(sis_sdsfree_call); // 可能单条数据大
	context->map_sdbs = sis_map_list_create(sis_dynamic_db_destroy);
	context->map_data = sis_map_pointer_create_v(sis_node_list_destroy); // 可能单条数据大

	// 每个读者都自行压缩数据 并把压缩的数据回调出去
	context->map_reader_curr = sis_map_kint_create();
	context->map_reader_curr->type->vfree = frwdb_reader_destroy;
	context->map_reader_disk = sis_map_kint_create();
	context->map_reader_disk->type->vfree = frwdb_reader_destroy;
	// 是否写LOG
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
	// 写入的文件格式
	s_sis_sds work_path = sis_sds_save_get(context->work_path);
	s_sis_sds work_name = sis_sds_save_get(context->work_name);
    s_sis_json_node *wfilenode = sis_json_cmp_child_node(node, "wfile");
    if (wfilenode)
    {
		context->wfile_config = sis_json_clone(wfilenode, 1);
		s_sis_json_node *wpnode = sis_json_cmp_child_node(context->wfile_config, "work-path");
		if (!wpnode)
		{
			sis_json_object_add_string(context->wfile_config, "work-path", work_path, sis_sdslen(work_path));
		}
		s_sis_json_node *wnnode = sis_json_cmp_child_node(context->wfile_config, "work-name");
		if (!wnnode)
		{
			sis_json_object_add_string(context->wfile_config, "work-name", work_name, sis_sdslen(work_name));
		}
    }
	else
	{
		context->wfile_config = sis_json_create_object();
		context->wfile_config->key = sis_strdup("wfile", 5);
		sis_json_object_add_string(context->wfile_config, "work-path", work_path, sis_sdslen(work_path));
		sis_json_object_add_string(context->wfile_config, "work-name", work_name, sis_sdslen(work_name));
		sis_json_object_add_string(context->wfile_config, "classname", "sisdb_wsdb", 10);
	}
	// s_sis_sds winfo = sis_json_to_sds(context->wfile_config, 1);
	// printf("winfo =%s\n", winfo);
	// winfo = sis_json_to_sds(context->wfile_config, 1);
	// printf("winfo =%s\n", winfo);
	// 读取文件的格式
    s_sis_json_node *rfilenode = sis_json_cmp_child_node(node, "rfile");
    if (rfilenode)
    {
		context->rfile_config = sis_json_clone(rfilenode, 1);
		s_sis_json_node *wpnode = sis_json_cmp_child_node(context->rfile_config, "work-path");
		if (!wpnode)
		{
			sis_json_object_add_string(context->rfile_config, "work-path", work_path, sis_sdslen(work_path));
		}
		s_sis_json_node *wnnode = sis_json_cmp_child_node(context->rfile_config, "work-name");
		if (!wnnode)
		{
			sis_json_object_add_string(context->rfile_config, "work-name", work_name, sis_sdslen(work_name));
		}
    }
	else
	{
		context->rfile_config = sis_json_create_object();
		context->rfile_config->key = sis_strdup("rfile", 5);
		sis_json_object_add_string(context->rfile_config, "work-path", work_path, sis_sdslen(work_path));
		sis_json_object_add_string(context->rfile_config, "work-name", work_name, sis_sdslen(work_name));
		sis_json_object_add_string(context->rfile_config, "classname", "sisdb_rsdb", 10);
	}

	context->work_nodes = sis_net_mems_create();

	context->stoping = 0;
    return true;
}
void frwdb_uninit(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
	if (context->stoping)
	{
		return ;
	}
	context->stoping = 1;
	
	if (context->read_thread)
    {
        sis_wait_thread_destroy(context->read_thread);
        context->read_thread = NULL;
    }
	sis_net_mems_destroy(context->work_nodes);
	
	// 取消所有的实时读者
	sis_map_kint_destroy(context->map_reader_curr);
	// 取消所有磁盘数据的读者
	sis_map_kint_destroy(context->map_reader_disk);
	// 如果开始写就必须停止 并进行写盘操作
	if (context->status == FRWDB_STATUS_WRING)
	{
		// 设置状态为写入完成 此时不能
		context->status = FRWDB_STATUS_STOPW;

		frwdb_write_stop(context);

		context->work_date = 0;
		context->status = FRWDB_STATUS_NONE;
	}
	sis_map_list_destroy(context->map_keys);
	sis_map_list_destroy(context->map_sdbs);
	sis_map_pointer_destroy(context->map_data);
	// 停止wlog信息
    if (context->wlog_worker)
    {
        sis_worker_destroy(context->wlog_worker);
		context->wlog_worker = NULL;
    }
	// 清除写文件的插件
    if (context->wfile_config)
    {
        sis_json_delete_node(context->wfile_config);
		context->wfile_config = NULL;
    }
	// 清除读数据的配置
    if (context->rfile_config)
    {
		sis_json_delete_node(context->rfile_config);
		context->rfile_config = NULL;
    }
	// 释放其他配置信息
    sis_sds_save_destroy(context->work_path);
    sis_sds_save_destroy(context->work_name);
	sis_free(context);
}
int cmd_frwdb_create(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    printf("create ... %s\n", netmsg->subject);
    s_sis_json_handle *injson = sis_json_load(netmsg->info, netmsg->info ? sis_sdslen(netmsg->info) : 0); 
    if (!injson)
    {
        sis_net_msg_tag_error(netmsg, "no parse create info.", 0);
        return SIS_METHOD_OK;
    }
	injson->node->key = sis_strdup(netmsg->subject, sis_sdslen(netmsg->subject));
    s_sis_dynamic_db *db = sis_dynamic_db_create(injson->node);
    if(db)
    {
		// sis_dynamic_db_setname(db, netmsg->subject);
		sis_map_list_set(context->map_sdbs, netmsg->subject, db);
        sis_net_msg_tag_ok(netmsg);
    }
    else
    {
        sis_net_msg_tag_error(netmsg, "create sdb fail.", 0);
    }
    sis_json_close(injson);
    return SIS_METHOD_OK;
}

int cmd_frwdb_setdb(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
    s_sis_message *netmsg = (s_sis_message *)argv_;

	s_sis_sds sinfo = netmsg->info;
	if (sinfo)
	{
		s_sis_json_handle *injson = sis_json_load(sinfo, sis_sdslen(sinfo));
		if (!injson)
		{
			return SIS_METHOD_ERROR;
		}
		s_sis_json_node *innode = sis_json_first_node(injson->node); 
		while (innode)
		{
			s_sis_dynamic_db *db = sis_dynamic_db_create(innode);
			sis_map_list_set(context->map_sdbs, innode->key, db);
			innode = sis_json_next_node(innode);
		}
		sis_json_close(injson);
	}
    return SIS_METHOD_OK;
}

int cmd_frwdb_start(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
	s_sis_message *netmsg = (s_sis_message *)argv_;

	int curr_date = sis_net_msg_info_as_date(netmsg); 
	printf("frw start: %s\n", netmsg->info);
	if (context->status == FRWDB_STATUS_WRING)
	{
		// 已经开始写入 什么也不做
		if (curr_date == context->work_date)
		{
			return SIS_METHOD_ERROR;
		}
		else if (context->work_date > 0)
		{
			// 关闭前日的数据
			context->status = FRWDB_STATUS_STOPW;
			frwdb_write_stop(context);
			context->status = FRWDB_STATUS_NONE;
		}
	}
	if (context->status != FRWDB_STATUS_NONE)
	{
		return SIS_METHOD_ERROR;
	}
	printf("frw start: :: %d %d %d\n", curr_date, context->work_date, context->status);
	frwdb_write_start(context, curr_date);

	context->status = FRWDB_STATUS_WRING;

	return SIS_METHOD_OK;
}
int frwdb_wfile_merge(s_frwdb_cxt *context, int iyear)
{
	// 这里开始对曾经写入的历史日线数据进行自动合并
	// 默认情况下 写入数据时日线上的数据是一天一条数据
	s_sis_worker *wfile = sis_worker_create(NULL, context->wfile_config);
	if (!wfile)
	{
		return 0;
	}
	s_sis_message *msg = sis_message_create();
	sis_net_message_set_info_i(msg, iyear * 10000 + 1231);
	sis_worker_command(wfile, "merge", msg);
	sis_message_destroy(msg);

	sis_worker_destroy(wfile);
	return 0;
}
int cmd_frwdb_merge(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
	if (context->status != FRWDB_STATUS_NONE)
	{
		// 没有写入动作 什么也不做
		return SIS_METHOD_ERROR;
	}
	s_sis_message *netmsg = (s_sis_message *)argv_;
	int curr_year = sis_net_msg_info_as_date(netmsg) / 10000;
	// merge 当天的所有数据
	printf("frw merge: %s\n", netmsg->info);

	frwdb_wfile_merge(context, curr_year);

	return SIS_METHOD_OK;
}
int cmd_frwdb_stop(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
	if (context->status != FRWDB_STATUS_WRING)
	{
		// 没有写入动作 什么也不做
		return SIS_METHOD_ERROR;
	}	
	s_sis_message *netmsg = (s_sis_message *)argv_;
	int curr_date = sis_net_msg_info_as_date(netmsg); 
	int curr_year = curr_date / 10000;

	printf("frw stop: %s\n", netmsg->info);
	// 设置状态为写入完成 此时不能
	context->status = FRWDB_STATUS_STOPW;

	frwdb_write_stop(context);

	if (context->work_year > 0 && curr_year > context->work_year)
	{
		// 切换年时合并日上数据
		// merge 用于合并数据 stop 时会自动判断是否是否需要清理冗余数据
		frwdb_wfile_merge(context, context->work_year);
	}

	context->work_year = curr_year;
	context->work_date = 0;
	context->status = FRWDB_STATUS_NONE;

    return SIS_METHOD_OK;
}
int frwdb_wfile_save(s_frwdb_cxt *context)
{
	int count = sis_map_pointer_getsize(context->map_data);
	if (count < 1)
	{
		LOG(8)("no data.\n");
		return 0;
	}
	// 从内存数据写入到数据库
	s_sis_worker *wfile = sis_worker_create(NULL, context->wfile_config);
	if (!wfile)
	{
		LOG(8)("no create wfile.\n");
		return 0;
	}
	s_sis_message *msg = sis_message_create();
	s_sis_sds keys = sis_map_as_keys(context->map_keys);
	sis_message_set_str(msg, "work-keys", keys, sis_sdslen(keys));
	sis_sdsfree(keys);

	s_sis_sds sdbs = sis_map_as_sdbs(context->map_sdbs);
	sis_message_set_str(msg, "work-sdbs", sdbs, sis_sdslen(sdbs));
	sis_sdsfree(sdbs);

	sis_worker_command(wfile, "start", msg); 

	char kname[128];
	char sname[128];
	s_sis_db_chars chars;
	chars.kname = kname;
	chars.sname = sname;
	sis_message_set(msg, "chars", &chars, NULL);

	s_sis_memory *imem = sis_memory_create_size(16 * 1024 * 1024);
	
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(context->map_data);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sis_node_list *nodes = (s_sis_node_list *)sis_dict_getval(de);
		sis_str_divide(sis_dict_getkey(de), '.', kname, sname);
		// 转数据到 memory
		if (frwdb_wfile_nodes_to_memory(nodes, imem))
		{
			chars.data = sis_memory(imem);
			chars.size = sis_memory_get_size(imem);
			sis_worker_command(wfile, "write", msg);			
		}
	}
	sis_dict_iter_free(di);

	sis_memory_destroy(imem);

	sis_worker_command(wfile, "stop", msg); 
	sis_message_destroy(msg);
	sis_worker_destroy(wfile);
	return 1;
}
int cmd_frwdb_save(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
	printf("save status = %d\n", context->status);
	if (context->status == FRWDB_STATUS_NONE)
	{
		if (!frwdb_wfile_save(context))
		{
			LOG(5)("frwdb wfile fail.\n");
		}
	}	
    return SIS_METHOD_OK;
}
int cmd_frwdb_set(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
	if (context->status != FRWDB_STATUS_WRING)
	{
		return SIS_METHOD_ERROR;
	}
	s_sis_message *netmsg = (s_sis_message *)argv_;
	s_sis_sds imem = netmsg->info;
    if (!netmsg->subject || !imem || sis_sdslen(imem) < 1 || (imem[0] != '{' && imem[0] != '['))
    {
		return SIS_METHOD_ERROR;
	}
	char kname[128];
	char sname[128];
	sis_str_divide(netmsg->subject, '.', kname, sname);
	s_sis_dynamic_db *curdb = sis_map_list_get(context->map_sdbs, sname);
	if(!curdb)
	{
		return SIS_METHOD_ERROR;
	}
	s_sis_sds omem = NULL;
	if (imem[0] == '{')
	{
		omem = sis_json_to_struct_sds(curdb, imem, NULL);
	}
	if (imem[0] == '[')
	{
		omem = sis_array_to_struct_sds(curdb, imem);
	}
	if (!omem)
	{
		return SIS_METHOD_ERROR;
	}		

	int kidx = sis_map_list_get_index(context->map_keys, kname);
	// 这里检查键值是否有增加
	if (kidx < 0)
	{
		sis_map_list_set(context->map_keys, kname, sis_sdsnew(kname));
	}
	int sidx = sis_map_list_get_index(context->map_sdbs, sname);
	sis_net_mems_push_kv(context->work_nodes, kidx, sidx, omem, sis_sdslen(omem));

	sis_sdsfree(omem);

    return SIS_METHOD_OK;
}

void _frwdb_make_data_chr(s_frwdb_cxt *context, char *kname, s_sis_dynamic_db *curdb, char *imem, size_t isize)
{
	char subject[128];
	sis_sprintf(subject, 128, "%s.%s", kname, curdb->name);
	s_sis_node_list *curdata = sis_map_pointer_get(context->map_data, subject);
	if (!curdata)
	{
		curdata = sis_node_list_create(1024, curdb->size);
		sis_map_pointer_set(context->map_data, subject, curdata);
	}
	// 数据只能增加 不判断重复数据
	int count = isize / curdb->size;
	for (int i = 0; i < count; i++)
	{
		sis_node_list_push(curdata, imem + i * curdb->size);
	}
	// 这里处理实时数据订阅 或者其他密集型操作
	s_sis_db_chars chars;
	chars.kname = kname;
	chars.sname = curdb->name;
	chars.size  = curdb->size;
	for (int i = 0; i < count; i++)
	{
		chars.data = imem + i * curdb->size;
		frwdb_reader_curr_write(context, &chars);
	}
}
int cmd_frwdb_bset(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
	s_sis_message *netmsg = (s_sis_message *)argv_;
	s_sis_sds omem = netmsg->info;
	if (!netmsg->subject || !omem || sis_sdslen(omem) < 1)
	{
		return SIS_METHOD_ERROR;
	}
	if (context->status == FRWDB_STATUS_WRING)
	{
		// 这里按天写入数据
		char kname[128];
		char sname[128];
		sis_str_divide(netmsg->subject, '.', kname, sname);
		s_sis_dynamic_db *curdb = sis_map_list_get(context->map_sdbs, sname);
		if(!curdb)
		{
			return SIS_METHOD_ERROR;
		}
		int kidx = sis_map_list_get_index(context->map_keys, kname);
		// 这里检查键值是否有增加
		if (kidx < 0)
		{
			sis_map_list_set(context->map_keys, kname, sis_sdsnew(kname));
		}
		int sidx = sis_map_list_get_index(context->map_sdbs, sname);
		sis_net_mems_push_kv(context->work_nodes, kidx, sidx, omem, sis_sdslen(omem));

		return SIS_METHOD_OK;
	}
	if (context->status == FRWDB_STATUS_NONE)
	{
		// 这里直接整块数据写入
		char kname[128];
		char sname[128];
		sis_str_divide(netmsg->subject, '.', kname, sname);
		s_sis_dynamic_db *curdb = sis_map_list_get(context->map_sdbs, sname);
		if(!curdb)
		{
			return SIS_METHOD_ERROR;
		}
		int kidx = sis_map_list_get_index(context->map_keys, kname);
		// 这里检查键值是否有增加
		if (kidx < 0)
		{
			sis_map_list_set(context->map_keys, kname, sis_sdsnew(kname));
		}
		_frwdb_make_data_chr(context, kname, curdb, omem, sis_sdslen(omem));
		return SIS_METHOD_OK;
	}
	return SIS_METHOD_ERROR;
}

int cmd_frwdb_sub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
	s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
	s_sis_message *msg = (s_sis_message *)argv_;

	frwdb_register_reader(context, msg);
	
	return SIS_METHOD_OK;
}
int cmd_frwdb_unsub(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
	s_sis_message *msg = (s_sis_message *)argv_;
	
	frwdb_remove_reader(context, msg->cid);
    
	return SIS_METHOD_OK;
}

int cmd_frwdb_get(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
	s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
	s_sis_message *msg = (s_sis_message *)argv_;
	// 默认读取数据不压缩
	return frwdb_read(context, msg);
}

// 因为可能发到中间时也会调用该函数 init 时需要保证环境和最初的环境一致
int cmd_frwdb_init(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
    
	s_sis_message *msg = (s_sis_message *)argv_;

	context->cb_source = sis_message_get(msg, "cb_source");
	context->cb_net_message = sis_message_get_method(msg, "cb_net_message");

	// 加载 上次未正常退出 log 的数据
	if (context->wlog_worker)
	{
		LOG(5)("init wlog start.\n");
		frwdb_wlog_load(worker);
		LOG(5)("init wlog stop. %d\n");
	}

    return SIS_METHOD_OK;
}

int cmd_frwdb_rlog(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
	
	if (context->wlog_worker)
	{
		LOG(5)("load wlog start.\n");
		frwdb_wlog_load(worker);
		LOG(5)("load wlog stop. %d\n");
	}
	return SIS_METHOD_OK;
}

int cmd_frwdb_wlog(void *worker_, void *argv_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
	if (context->wlog_worker)
	{
		if (!context->wlog_opened)
		{
			context->wlog_mark = sis_time_get_idate(0);
			if (frwdb_wlog_start(context) == SIS_METHOD_OK)
			{
				context->wlog_opened = 1;
			}
		}
		if (context->wlog_opened && context->wlog_method)
		{
			// 保存记录
			s_sis_message *netmsg = (s_sis_message *)argv_;
			context->wlog_method->proc(context->wlog_worker, netmsg);
		}
	}
	return SIS_METHOD_OK;
}	

void frwdb_working(void *worker_)
{
    s_sis_worker *worker = (s_sis_worker *)worker_; 
    s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
	// 自动存盘
	if (context->save_time > 0 && context->status == FRWDB_STATUS_WRING && 
		context->work_date > 0 && context->stoping == 0)
	{
		int idate = sis_time_get_idate(0);
		int itime = sis_time_get_itime(0);
		if (itime > context->save_time && idate > context->work_date)
		{
			// 设置状态为写入完成 此时不能
			context->status = FRWDB_STATUS_STOPW;

			frwdb_write_stop(context);

			context->work_date = 0;
			context->status = FRWDB_STATUS_NONE;
		}
	}
}


//////////////////////////////////////////////////////////////////
//------------------------wlog function -----------------------//
//////////////////////////////////////////////////////////////////

static int cb_frwdb_wlog_load(void *worker_, void *argv_)
{
	s_sis_worker *worker = (s_sis_worker *)worker_;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
    sis_worker_command(worker, netmsg->cmd, netmsg);
	return 0;
}
// 从磁盘中获取数据 0 没有wlog文件 ** 文件有错误直接删除 并返回0
//               1 有文件 正在读 需要设置状态 并等待数据读完
int frwdb_wlog_load(s_sis_worker *worker)
{
    s_frwdb_cxt *context = (s_frwdb_cxt *)worker->context;
	s_sis_message *msg = sis_message_create();

    s_sis_sds work_path = sis_sds_save_get(context->work_path);
    s_sis_sds work_name = sis_sds_save_get(context->work_name);
    sis_message_set_str(msg, "work-path", work_path, sis_sdslen(work_path));
    sis_message_set_str(msg, "work-name", work_name, sis_sdslen(work_name));
	sis_message_set_int(msg, "work-date", context->wlog_mark);

	sis_message_set(msg, "cb_source", worker, NULL);
	sis_message_set_method(msg, "cb_netmsg", cb_frwdb_wlog_load);

	int o = sis_worker_command(context->wlog_worker, "sub", msg);
	sis_message_destroy(msg);
	return o;	
}

int frwdb_wlog_start(s_frwdb_cxt *context)
{
	s_sis_message *msg = sis_message_create();
	s_sis_sds work_path = sis_sds_save_get(context->work_path);
	s_sis_sds work_name = sis_sds_save_get(context->work_name);
	sis_message_set_str(msg, "work-path", work_path, sis_sdslen(work_path));
	sis_message_set_str(msg, "work-name", work_name, sis_sdslen(work_name));
	sis_message_set_int(msg, "work-date", context->wlog_mark);
	int o = sis_worker_command(context->wlog_worker, "open", msg); 
	sis_message_destroy(msg);
	return o;
}
int frwdb_wlog_stop(s_frwdb_cxt *context)
{
	return sis_worker_command(context->wlog_worker, "close", NULL); 
}
int frwdb_wlog_remove(s_frwdb_cxt *context)
{
	s_sis_message *msg = sis_message_create();
	s_sis_sds work_path = sis_sds_save_get(context->work_path);
	s_sis_sds work_name = sis_sds_save_get(context->work_name);
	sis_message_set_str(msg, "work-path", work_path, sis_sdslen(work_path));
	sis_message_set_str(msg, "work-name", work_name, sis_sdslen(work_name));
	sis_message_set_int(msg, "work-date", context->wlog_mark);
	int o = sis_worker_command(context->wlog_worker, "remove", msg); 
	sis_message_destroy(msg);
	return o;
}

//////////////////////////////////////////////////////////////////
//------------------------frwdb function -----------------------//
//////////////////////////////////////////////////////////////////
int frwdb_wfile_nodes_to_memory(s_sis_node_list *nodes, s_sis_memory *imem)
{
	if (!nodes)
	{
		return 0;
	}
	sis_memory_clear(imem);
	for (int i = 0; i < nodes->nodes->count; i++)
	{
		s_sis_struct_list *node = (s_sis_struct_list *)sis_pointer_list_get(nodes->nodes, i);
		sis_memory_cat(imem, sis_struct_list_get(node, 0), nodes->node_size * node->count);
	}
	return sis_memory_get_size(imem);
}


void frwdb_write_stop(s_frwdb_cxt *context)
{
	// 停止读数线程
	if (context->read_thread)
	{
		// 发个通知把数据刷到数据区
		sis_wait_thread_destroy(context->read_thread);
		context->read_thread = NULL;
	}	
	// 停止订阅实时的读者
	frwdb_reader_curr_stop(context);
	// 停止写wlog文件
	if (context->wlog_worker)
	{ 
		if (context->wlog_opened)
		{
			frwdb_wlog_stop(context);
			context->wlog_opened = 0;
		}
	}
	// 把内存的数据写入磁盘
	if (!frwdb_wfile_save(context))
	{
		LOG(5)("frwdb wfile fail.\n");
	}
	// 清理wlog文件
	if (context->wlog_worker)
	{ 
		frwdb_wlog_remove(context);
	}

	sis_map_pointer_clear(context->map_data);
	// 不清理表结构 下次直接用
	// sis_map_list_clear(context->map_keys); 
	// sis_map_list_clear(context->map_sdbs);
}

void _frwdb_make_data_idx(s_frwdb_cxt *context, int kidx, int sidx, char *imem, size_t isize)
{
	s_sis_dynamic_db *curdb = sis_map_list_geti(context->map_sdbs, sidx);
	s_sis_sds kname = sis_map_list_geti(context->map_keys, kidx);
	if (!curdb || !kname)
	{
		return ;
	}
	char subject[128];
	sis_sprintf(subject, 128, "%s.%s", kname, curdb->name);
	s_sis_node_list *curdata = sis_map_pointer_get(context->map_data, subject);
	if (!curdata)
	{
		curdata = sis_node_list_create(1024, curdb->size);
		sis_map_pointer_set(context->map_data, subject, curdata);
	}
	// 数据只能增加 不判断重复数据
	int count = isize / curdb->size;
	for (int i = 0; i < count; i++)
	{
		sis_node_list_push(curdata, imem + i * curdb->size);
	}
	// printf("nodes = %d\n", sis_node_list_getsize(curdata));
	// 这里处理实时数据订阅 或者其他密集型操作
	s_sis_db_chars chars;
	chars.kname = kname;
	chars.sname = curdb->name;
	chars.size  = curdb->size;
	for (int i = 0; i < count; i++)
	{
		chars.data = imem + i * curdb->size;
		frwdb_reader_curr_write(context, &chars);
	}
}
void _frwdb_read_data(s_frwdb_cxt *context)
{
	int count = sis_net_mems_read(context->work_nodes, 0);
	// printf("read %d %d %d\n",count, context->work_nodes->rnums, context->work_nodes->wnums);
	if (count > 0)
	{
		s_sis_net_mem *mem = sis_net_mems_pop(context->work_nodes);
		while(mem)
		{
			int *kidx = (int *)(mem->data + 0);
			int *sidx = (int *)(mem->data + 4);
			_frwdb_make_data_idx(context, *kidx, *sidx, mem->data + 8, mem->size - 8);
			mem = sis_net_mems_pop(context->work_nodes);
			// sis_sleep(1);
		}
		sis_net_mems_free_read(context->work_nodes);
		// LOG(5)("free %d %d %d\n", count, context->work_nodes->rnums, context->work_nodes->wnums);
	}

}
static void *_thread_reader(void *argv_)
{
    s_frwdb_cxt *context = (s_frwdb_cxt *)argv_;
    sis_wait_thread_start(context->read_thread);
	// printf("read == %d %d\n", context->work_nodes->rnums, context->work_nodes->wnums);
    while (sis_wait_thread_noexit(context->read_thread))    
    {
		// printf("read %d %d\n", context->work_nodes->rnums, context->work_nodes->wnums);
		_frwdb_read_data(context);
       	sis_wait_thread_wait(context->read_thread, context->read_thread->wait_msec);
    }
	_frwdb_read_data(context);
    sis_wait_thread_stop(context->read_thread);
	context->read_thread = NULL;
    return NULL;
}

void frwdb_write_start(s_frwdb_cxt *context, int workdate)
{
	// 设置日期
	context->work_date = workdate;
	// 如果是断线重连 没有经过 stop 需要特殊处理
	if (!context->read_thread)
	{
		context->read_thread = sis_wait_thread_create(10);
        if (!sis_wait_thread_open(context->read_thread, _thread_reader, context))
        {
            sis_wait_thread_destroy(context->read_thread);
            context->read_thread = NULL;
            LOG(1)("can't start write thread.\n");
        }    
	} 
	// 初始化用户信息
	frwdb_reader_curr_init(context);
}

