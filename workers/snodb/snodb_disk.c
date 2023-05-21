
#include "worker.h"
#include "sis_method.h"
#include "sis_disk.h"
#include <snodb.h>

//////////////////////////////////////////////////////////////////
//------------------------wlog function -----------------------//
//////////////////////////////////////////////////////////////////
void _snodb_write_start(s_snodb_cxt *context, int workdate);
bool _snodb_write_init(s_snodb_cxt *context);
// 传入的是原始数据
int _snodb_write_incrzip(void *context_, char *imem_, size_t isize_);

// static msec_t _speed_msec = 0;
// 从wlog文件中加载数据
static int cb_snodb_wlog_start(void *worker_, void *argv_)
{
    s_snodb_cxt *context = (s_snodb_cxt *)worker_;
	const char *sdate = (const char *)argv_;
	LOG(5)("load wlog start. %s\n", sdate);
	context->wlog_date = sis_atoll(sdate); 
	_snodb_write_start(context, context->wlog_date);
	return SIS_METHOD_OK;
}
static int cb_snodb_wlog_stop(void *worker_, void *argv_)
{
    // s_snodb_cxt *context = (s_snodb_cxt *)worker_;
	const char *sdate = (const char *)argv_;
	LOG(5)("load wlog stop. %s\n", sdate);
	// printf("load wlog cost : %lld\n", sis_time_get_now_msec()-_speed_msec);
	return SIS_METHOD_OK;
}
static int cb_snodb_wlog_load(void *worker_, void *argv_)
{
    s_snodb_cxt *context = (s_snodb_cxt *)worker_;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	
	switch (netmsg->tag)
	{
	case SIS_NET_TAG_SUB_KEY:
		{
			sis_sdsfree(context->work_keys);
			context->work_keys = sis_sdsdup(netmsg->info);
		}
		break;	
	case SIS_NET_TAG_SUB_SDB:
		{
			sis_sdsfree(context->work_sdbs);
			context->work_sdbs = sis_sdsdup(netmsg->info);
		}
		break;
	default:
		if (!sis_strcasecmp("zpub", netmsg->cmd))
		{
			if (!_snodb_write_init(context))
			{
				if (netmsg->info)
				{
					_snodb_write_incrzip(context, netmsg->info, sis_sdslen(netmsg->info));
				}
			}
		}
		break;
	}
    return SIS_METHOD_OK;
}

// 从磁盘中获取数据 0 没有wlog文件 ** 文件有错误直接删除 并返回0
//               1 有文件 正在读 需要设置状态 并等待数据读完
int snodb_wlog_load(s_snodb_cxt *snodb_)
{
	s_sis_message *msg = sis_message_create();

	sis_message_set_str(msg, "work-path", snodb_->work_path, sis_sdslen(snodb_->work_path));
	sis_message_set_str(msg, "work-name", snodb_->work_name, sis_sdslen(snodb_->work_name));
	sis_message_set_int(msg, "work-date", snodb_->work_date);

	sis_message_set(msg, "cb_source", snodb_, NULL);
	sis_message_set_method(msg, "cb_sub_start", cb_snodb_wlog_start);
	sis_message_set_method(msg, "cb_sub_stop", cb_snodb_wlog_stop);
	sis_message_set_method(msg, "cb_netmsg", cb_snodb_wlog_load);

	int o = sis_worker_command(snodb_->wlog_worker, "sub", msg);
	sis_message_destroy(msg);
	return o;	
}
// 把数据写入到wlog中
int snodb_wlog_save(s_snodb_cxt *snodb_, int sign_, char *imem_, size_t isize_)
{
	s_sis_net_message *netmsg = sis_net_message_create();
	switch (sign_) 
	{
	case SICDB_FILE_SIGN_KEYS:
    	sis_net_message_set_cmd(netmsg, "set");
		sis_net_message_set_tag(netmsg, SIS_NET_TAG_SUB_KEY);
		sis_net_message_set_char(netmsg, imem_, isize_);
		break;
	case SICDB_FILE_SIGN_SDBS:
    	sis_net_message_set_cmd(netmsg, "set");
		sis_net_message_set_tag(netmsg, SIS_NET_TAG_SUB_SDB);
		sis_net_message_set_char(netmsg, imem_, isize_);
		break;	
	default: // SICDB_FILE_SIGN_ZPUB
    	sis_net_message_set_cmd(netmsg, "zpub");
		sis_net_message_set_byte(netmsg, imem_, isize_);
		break;
	}
	snodb_->wlog_method->proc(snodb_->wlog_worker, netmsg);
	sis_net_message_destroy(netmsg);
	return 0;
}
int snodb_wlog_start(s_snodb_cxt *snodb_)
{
	s_sis_message *msg = sis_message_create();
	sis_message_set_str(msg, "work-path", snodb_->work_path, sis_sdslen(snodb_->work_path));
	sis_message_set_str(msg, "work-name", snodb_->work_name, sis_sdslen(snodb_->work_name));
	sis_message_set_int(msg, "work-date", snodb_->work_date);
	int o = sis_worker_command(snodb_->wlog_worker, "open", msg); 
	sis_message_destroy(msg);
	return o;
}
int snodb_wlog_stop(s_snodb_cxt *snodb_)
{
	return sis_worker_command(snodb_->wlog_worker, "close", NULL); 
}
int snodb_wlog_remove(s_snodb_cxt *snodb_)
{
	s_sis_message *msg = sis_message_create();
	sis_message_set_str(msg, "work-path", snodb_->work_path, sis_sdslen(snodb_->work_path));
	sis_message_set_str(msg, "work-name", snodb_->work_name, sis_sdslen(snodb_->work_name));
	sis_message_set_int(msg, "work-date", snodb_->work_date);
	int o = sis_worker_command(snodb_->wlog_worker, "remove", msg); 
	sis_message_destroy(msg);
	return o;
}

//////////////////////////////////////////////////////////////////
//------------------------wlog function -----------------------//
//////////////////////////////////////////////////////////////////
// 从wlog文件中加载数据

static int cb_snodb_wfile_start(void *worker_, void *argv_)
{
    s_snodb_cxt *context = (s_snodb_cxt *)worker_;
	LOG(5)("read wlog start. %s\n", (char *)argv_);
	// 先删除对应日期的目标文件

	if (context->wfile_cb_sub_start)
	{
		context->wfile_cb_sub_start(context->wfile_worker, (char *)argv_);
	}
	return SIS_METHOD_OK;
}
static int cb_snodb_wfile_stop(void *worker_, void *argv_)
{
    s_snodb_cxt *context = (s_snodb_cxt *)worker_;
	LOG(5)("read wlog stop. %s\n", (char *)argv_);
	if (context->wfile_cb_sub_stop)
	{
		context->wfile_cb_sub_stop(context->wfile_worker, (char *)argv_);
	}
	return SIS_METHOD_OK;
}

static int cb_snodb_wfile_load(void *worker_, void *argv_)
{
    s_snodb_cxt *context = (s_snodb_cxt *)worker_;
    s_sis_net_message *netmsg = (s_sis_net_message *)argv_;
	if (context->wfile_save != 1)
	{
		return SIS_METHOD_ERROR;
	}
	switch (netmsg->tag)
	{
	case SIS_NET_TAG_SUB_KEY:
		{
			LOG(5)("read wlog keys. \n");
			if (context->wfile_cb_dict_keys)
			{
				context->wfile_cb_dict_keys(context->wfile_worker, netmsg->info);
			}
		}
		break;	
	case SIS_NET_TAG_SUB_SDB:
		{
			LOG(5)("read wlog sdbs. \n");
			if (context->wfile_cb_dict_sdbs)
			{
				context->wfile_cb_dict_sdbs(context->wfile_worker, netmsg->info);
			}
		}
		break;
	default:
		if (!sis_strcasecmp("zpub", netmsg->cmd))
		{
			if (netmsg->info)
			{
				if (context->wfile_cb_sub_incrzip)
				{
					s_sis_db_incrzip zmem = {0};
					zmem.data = (uint8 *)netmsg->info;
					zmem.size = sis_sdslen(netmsg->info);
					zmem.init = sis_incrzip_isinit(zmem.data, zmem.size);
					context->wfile_cb_sub_incrzip(context->wfile_worker, &zmem);
				}
			}
		}
		break;
	}
    return SIS_METHOD_OK;
}
// 把wlog转为snos格式 
// 以目标为snappy格式 压缩到4.666G的数据
// 读取zbit压缩的wlog文件需要时间为 msec = 13602 ~ 22212
// 不用snappy压缩不写盘，只解压zbit数据需要时间为 msec = 153805 - 13602 = 140秒 约2分钟
// 测试硬盘顺序写速度为300M/秒 15秒应该差不多写完
// -- 以下测试为snappy压缩和写盘的 完整程序流程的速度 -- 
// --不用snappy压缩不写盘，走完所有函数功能 需要时间为 msec = 390161 - 153805 = 236秒 约4分钟
// --用snappy压缩不写盘，4.67G 需要时间为  msec = 512051 - 390161 = 122秒 约2分钟
// --用snappy压缩并且写盘，4.67G 需要时间为  msec = 879961 - 390161 = 490秒 约8分钟
// --不用snappy压缩但写盘，约20G 需要时间为 msec = 758862 - 390161 = 359秒 约6分钟
// --按一秒300M写盘速度 15秒写完
// 需要时间 153+236+122+369 = 880 大概14-15分钟
// ??? 走完所有函数话费4分钟有点多
// 修改按32K缓存写入 总需要时间 496 - 664 - 713秒 大概8-12分钟 
// 在机械硬盘上的表现为（磁盘速度为上面测试环境的 1/10）015056 - 024500 - 差不多1小时
// 在机械硬盘上的表现为（磁盘速度为上面测试环境的 1/10）累计32K写入 114805 - 115929 - 差不多11分钟 
// 在机械硬盘上的表现为（磁盘速度为上面测试环境的 1/10）有数据就写入 133226 - 134402 - 差不多12分钟 
// 速度快的原因是写入时用了缓存 而前面为了保证log文件的实时写入，每次都要求写到磁盘才返回
// 现在改为仅仅log文件才强制写盘 其他直接写入缓存就返回 只在关闭文件时才强制写入文件


int snodb_wlog_to_snos(s_snodb_cxt *snodb_)
{
	
	if (sis_disk_control_exist(snodb_->work_path, snodb_->work_name, SIS_DISK_TYPE_LOG, snodb_->work_date) != 1)
	{
		// 文件不存在就返回
		return SIS_METHOD_ERROR;
	}
	if (sis_disk_control_exist(snodb_->work_path, snodb_->work_name, SIS_DISK_TYPE_SNO, snodb_->work_date))
	{
		// 文件不存在就返回
		LOG(5)("sno already exist. %s %s %d\n", snodb_->work_path, snodb_->work_name, snodb_->work_date);
		sis_disk_control_remove(snodb_->work_path, snodb_->work_name, SIS_DISK_TYPE_SNO, snodb_->work_date);
		// return SIS_METHOD_ERROR; 
	}
	// 设置写文件回调
	{
		s_sis_message *msg = sis_message_create();
		sis_message_set_str(msg, "work-path", snodb_->work_path, sis_sdslen(snodb_->work_path));
		sis_message_set_str(msg, "work-name", snodb_->work_name, sis_sdslen(snodb_->work_name));
		if (sis_worker_command(snodb_->wfile_worker, "getcb", msg) != SIS_METHOD_OK)
		{
			sis_message_destroy(msg);
			return SIS_METHOD_ERROR;
		}
		snodb_->wfile_cb_sub_start   = sis_message_get_method(msg, "cb_sub_start");
		snodb_->wfile_cb_sub_stop    = sis_message_get_method(msg, "cb_sub_stop");
		snodb_->wfile_cb_dict_keys   = sis_message_get_method(msg, "cb_dict_keys");
		snodb_->wfile_cb_dict_sdbs   = sis_message_get_method(msg, "cb_dict_sdbs");
		snodb_->wfile_cb_sub_incrzip = sis_message_get_method(msg, "cb_sub_incrzip");
		sis_message_destroy(msg);
	}
	// 从wlog直接取数据
	int  o = SIS_METHOD_OK;
	{
		s_sis_message *msg = sis_message_create();
		snodb_->wfile_save = 1;
		sis_message_set_str(msg, "work-path", snodb_->work_path, sis_sdslen(snodb_->work_path));
		sis_message_set_str(msg, "work-name", snodb_->work_name, sis_sdslen(snodb_->work_name));
		sis_message_set_int(msg, "work-date", snodb_->work_date);
		sis_message_set(msg, "cb_source", snodb_, NULL);
		sis_message_set_method(msg, "cb_sub_start", cb_snodb_wfile_start);
		sis_message_set_method(msg, "cb_sub_stop", cb_snodb_wfile_stop);
		sis_message_set_method(msg, "cb_netmsg", cb_snodb_wfile_load);
		o = sis_worker_command(snodb_->wlog_worker, "sub", msg);
		snodb_->wfile_save = 0;
		sis_message_destroy(msg);
	}
	// 这里来判断文件是否生成好 如果失败
	return o;
}

///////////////////////////////////////////
//  callback define begin
///////////////////////////////////////////

int snodb_reader_history_start(s_snodb_reader *reader_)
{

	s_snodb_cxt *snodb = (s_snodb_cxt *)reader_->father;
	if (!snodb->rfile_config)
	{
		return 0;
	}
	reader_->sub_disker = sis_worker_create(NULL, snodb->rfile_config);

	s_sis_message *msg = sis_message_create();
	sis_message_set_int(msg, "sub-date", reader_->sub_date);
	sis_message_set_str(msg, "sub-keys", reader_->sub_keys, sis_sdslen(reader_->sub_keys));
	sis_message_set_str(msg, "sub-sdbs", reader_->sub_sdbs, sis_sdslen(reader_->sub_sdbs));

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

int snodb_reader_history_stop(s_snodb_reader *reader_)
{
	printf("snodb_reader_history_stop \n");
	if (reader_->sub_disker)
	{
		// sis_worker_command(reader_->sub_disker, "unsub", NULL);
		sis_worker_destroy(reader_->sub_disker);
		reader_->sub_disker = NULL;
	}
	printf("snodb_reader_history_stop ok\n");
	return 0;
}



