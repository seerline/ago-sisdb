#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <api_dms.h>
#include "sis_list.h"
#include "sis_json.h"
#include "frwdb.h"
#include "snodb.h"
#include "worker.h"
#include "server.h"

static s_sis_map_list *_api_session_map = NULL;

typedef struct s_dms_service
{
	int                 used;
	int64               sno;      // 序列号
	s_sis_sds           wname;
    s_sis_worker       *worker;
	s_sis_map_kint     *replys;   // 回复暂存列表 s_sis_pointer_list -> s_service_reply 外部来释放
	s_service_callback *callback;
} s_dms_service;

s_dms_service *dms_service_create(const char *sname_, s_sis_json_node *snode_)
{
	s_dms_service *service = SIS_MALLOC(s_dms_service, service);
	service->used = 1;
	service->sno = 0;
	service->wname = sis_sdsnew(sname_);
	service->worker = sis_worker_create_of_name(NULL, service->wname, snode_);
	return service;
}
void dms_service_clear(s_dms_service *service_)
{
	s_dms_service *service = (s_dms_service *)service_;
	sis_worker_destroy(service->worker);
	service->worker = NULL;
	sis_sdsfree(service->wname);
	service->wname = NULL;
	service->used = 0;
}
void dms_service_destroy(void *service_)
{
	s_dms_service *service = (s_dms_service *)service_;
	if (service->worker)
	{
		sis_worker_destroy(service->worker);
		service->worker = NULL;
	}
	if (service->wname)
	{
		sis_sdsfree(service->wname);
		service->wname = NULL;
	}
	sis_free(service);
}

_API_DMS_DLLEXPORT_ int dms_service_open(const char *initinfo_)
{
	if (!_api_session_map)
	{
		_api_session_map = sis_map_list_create(dms_service_destroy);
		sis_server_init();
	}
	int index = -1;
	s_sis_json_handle *handle = sis_json_load(initinfo_, sis_strlen(initinfo_));
	if (handle)
	{
		const char *sname = sis_json_get_str(handle->node, "servicename");
		if (!sname)
		{
			goto _exit_;
		}
		s_sis_json_node *snode = sis_json_cmp_child_node(handle->node, "argv");
		
		s_dms_service *service = sis_map_list_get(_api_session_map, sname);
		if (!service)
		{
			service = dms_service_create(sname, snode);
			index = sis_map_list_set(_api_session_map, sname, service);
		}
		else
		{
			index = sis_map_list_get_index(_api_session_map, sname);
		}
_exit_:
		sis_json_close(handle);
	}
	return index;
}

_API_DMS_DLLEXPORT_ void dms_service_close(int handle_)
{
	if (_api_session_map)
	{
		s_dms_service *service = sis_map_list_geti(_api_session_map, handle_);
		if (service && service->used)
		{
			// 仅仅清理内存 和设置无用标记 不做其他工作
			dms_service_clear(service);
		}
		// 判断是否还有其他可用服务 没有就清理内存服务
		int count = sis_map_list_getsize(_api_session_map);
		int useds = 0;
		for (int i = 0; i < count; i++)
		{
			s_dms_service *srv = sis_map_list_geti(_api_session_map, i);
			if (srv->used)
			{
				useds++;
			}
		}
		if (useds <= 0)
		{
			// sis_map_list_clear(_api_session_map);
			sis_map_list_destroy(_api_session_map);
			_api_session_map = NULL;
			sis_server_uninit();
		}
	}	
}

// 函数调用
// handle_ 服务的句柄
// command_ 调用服务的命令名
// subject_ 操作的主体名
// in_ 传入的数据缓存
// isize_ 传入数据的大小
// callback_ 有值就按照回调方式返回数据 否则就同步方式返回数据
//    异步返回数据也会返回 s_service_reply 如果成功执行 style == SIS_TAG_OK 
_API_DMS_DLLEXPORT_ s_service_reply *dms_service_command(
	int handle_,           // 句柄
    const char *command_,  // 指令
    const char *subject_,  // 对象
    void *info_,           // 传入数据的指针
    size_t isize_,         // 传入数据的尺寸
	void *callback_)      
{
	s_service_reply *reply = NULL;
	s_dms_service *service = sis_map_list_geti(_api_session_map, handle_);
	if (service && service->worker)
	{
		s_sis_message *msg = sis_message_create();
		if (subject_)
		{
			msg->subject = sis_sdsnew(subject_);
		}
		if (info_ && isize_ > 0)
		{
			msg->info = sis_sdsnewlen(info_, isize_);
		}
		if (callback_)
		{
			sis_message_set(msg, "callback", callback_, NULL);
		}
		reply = SIS_MALLOC(s_service_reply, reply);
		if (sis_worker_command(service->worker, command_, msg) == SIS_METHOD_OK)
		{
			if (!callback_)
			{
				reply->style = msg->tag;
				switch (reply->style)
				{
				case SIS_NET_TAG_INT:
					reply->integer = sis_net_msg_info_as_int(msg);
					break;
				default:
					reply->isize = sis_sdslen(msg->info);
					reply->inmem = sis_sdsdup(msg->info);
					break;
				}
			}
			else
			{
				reply->style = SIS_NET_TAG_OK;
			}
		}
		else
		{
			reply->style = SIS_NET_TAG_ERROR;
			if (msg->info)
			{
				reply->isize = sis_sdslen(msg->info);
				reply->inmem = sis_sdsdup(msg->info);
			}
		}
		sis_message_destroy(msg);
	}
	return reply;
}

_API_DMS_DLLEXPORT_ void dms_service_free_reply(s_service_reply *reply_)
{
	if (reply_)
	{
		if (reply_->inmem)
		{
			sis_sdsfree(reply_->inmem);
			reply_->inmem = NULL;
		}
		if (reply_->reply)
		{
			dms_service_free_reply(reply_->reply);
		}
		sis_free(reply_);
	}
}


#if 1

static void cb_sub_chars(int id_, const char *key_, const char *sdb_, void *in_, size_t isize_)
{
	printf("%s : %s %s %zu\n", __func__, key_, sdb_, isize_);
}
static void cb_sub_info(int id_, const char *keys_, size_t ksize_)
{
	printf("%s : %zu\n", __func__, ksize_);
}
static void cb_sub_ctrl(int id_, long long subdate_)
{
	printf("%s : %lld\n", __func__, subdate_);
}
static void cb_reply(int id_, s_service_reply *reply)
{
	printf("%s : %p\n", __func__, reply);
}

int main()
{
	int no = dms_service_open("{\"version\" : 1, \"servicename\": \"sisdb_rsno_v1\", \
	   \"argv\":{\"work-path\":\"../../data/\",\"work-name\":\"snodb\",\"classname\":\"sisdb_rsno\"}}");
    /* \"ip\": \"192.168.3.118\", \"port\":50008,\"username\":\"guest\", \"password\":\"guest1234\"}"); */
	
	char info[1024];
	s_service_callback cb = {0};
	// cb.cb_net_open  = 
	// cb.cb_net_stop  = 
	cb.cb_sub_open  = cb_sub_ctrl;
	cb.cb_sub_start = cb_sub_ctrl;
	cb.cb_sub_wait  = cb_sub_ctrl;
	cb.cb_sub_stop  = cb_sub_ctrl;
	cb.cb_sub_break = cb_sub_ctrl;
	cb.cb_sub_close = cb_sub_ctrl;
	cb.cb_sub_dict_keys = cb_sub_info;
	cb.cb_sub_dict_sdbs = cb_sub_info;
	cb.cb_sub_chars = cb_sub_chars;
	cb.cb_reply = cb_reply;

	{
		sis_sprintf(info, 1024, "{\"start-date\": %d, \"stop-date\": %d }", 20220301, 20220301);
		s_service_reply *reply = dms_service_command(no, "get", "SH600600.stk_snapshot", 
			info, sis_strlen(info), NULL);

		if (reply)
		{
			printf("ayns : %d %zu\n", reply->style, reply->isize);
			dms_service_free_reply(reply);
		}
	}

	{
		sis_sprintf(info, 1024, "{\"start-date\": %d, \"stop-date\": %d, \"sub-date\": %d,  }", 20220301, 20220301);
		s_service_reply *reply = dms_service_command(no, "sub", "SH600600.stk_snapshot", 
			info, sis_strlen(info), &cb);

		if (reply)
		{
			printf("async: %d %zu\n", reply->style, reply->isize);
			dms_service_free_reply(reply);
		}
	}

	while(1)
	{
		sis_sleep(1000);
	}

	dms_service_close(no);  
}
#endif