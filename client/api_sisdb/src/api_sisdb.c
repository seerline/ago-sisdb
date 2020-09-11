#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <api_sisdb.h>
#include "sis_list.h"
#include "sisdb_client.h"
#include "worker.h"

static s_sis_pointer_list *_api_session_list = NULL;       

s_sis_worker *worker_api_sisdb_create(
	const char *ip_,
	int         port_,
	const char *username_, 
	const char *password_)
{
	s_sis_worker *worker = SIS_MALLOC(s_sis_worker, worker);

	if (sisdb_client_init(worker, NULL))
	{
		// 设置worker的信息
		s_sisdb_client_cxt *context = worker->context;
		sis_url_set_ip4(&context->url_cli, ip_);
		context->url_cli.port = port_;
		sis_strcpy(context->username, 32, username_);
		sis_strcpy(context->password, 32, password_);
		// 连接服务器
		sisdb_client_method_init(worker);
		int wait = 5000;
		while(wait > 0)
		{
			sis_sleep(50);
			wait -= 50;
			if (context->status == SIS_CLI_STATUS_WORK)
			{
				return worker;
			}
		}
	}
	return NULL;
}
void worker_api_sisdb_destroy(void *client_)
{
	s_sis_worker *worker = (s_sis_worker *)client_;
	// 注销服务器
	sisdb_client_method_uninit(worker);
	sisdb_client_uninit(worker);
	sis_free(worker);
}

_API_SISDB_DLLEXPORT_ int api_sisdb_client_create(	
	const char *ip_,
	int         port_,
	const char *username_, 
	const char *password_)
{
	if (!_api_session_list)
	{
		_api_session_list = sis_pointer_list_create();
		_api_session_list->vfree = worker_api_sisdb_destroy;
	}
	s_sis_worker *worker = worker_api_sisdb_create(ip_, port_, username_, password_);	
	return sis_pointer_list_push(_api_session_list, worker) - 1;
}

_API_SISDB_DLLEXPORT_ void api_sisdb_client_destroy(int id_)
{
	if (_api_session_list)
	{
		sis_pointer_list_delete(_api_session_list, id_, 1);
		if (_api_session_list->count < 1)
		{
			sis_pointer_list_destroy(_api_session_list);
			_api_session_list = NULL;
		}
	}
}

_API_SISDB_DLLEXPORT_ int api_sisdb_command_ask(
	int           id_,             // 句柄
	const char   *cmd_,            // 请求的参数
	const char   *key_,            // 请求的key
	const char   *val_,            // 请求的参数
	void         *cb_reply         // 回调的数据
)
{
	s_sis_worker* worker = sis_pointer_list_get(_api_session_list, id_);
	if (!worker)
	{
		return API_REPLY_ERR;
	}
	char ds[128], cmd[128]; 
	sis_str_divide(cmd_, '.', ds, cmd);
	if (sis_str_subcmp_strict(cmd, "sub,subsno,unsub,unsubsno", ',') >= 0)
	{
		return API_REPLY_ERR;
	}

	// 调取sisdb_client对应的函数
	s_sisdb_client_cxt *context = worker->context;
	s_sisdb_client_ask *ask = sisdb_client_ask_new( 
		context,
		cmd_, 
		key_, 
		(void *)val_, 
		sis_strlen(val_),
		NULL, 
		NULL,
		NULL,
		NULL,
		cb_reply,
		false);
	
	if (cmd_sisdb_client_send_cb(worker, ask) == SIS_METHOD_OK)
	{
		return API_REPLY_OK;
	}
	// 不是订阅的收到响应就自动删除
	return API_REPLY_ERR;
}

_API_SISDB_DLLEXPORT_ int api_sisdb_command_sub(
	int           id_,             // 句柄
	const char   *cmd_,            // 请求的参数
	const char   *key_,            // 请求的key
	const char   *val_,            // 请求的参数
	//////////////////
	void   *cb_source_,          // 回调传送对象
    void   *cb_sub_start,        // 回调开始
	void   *cb_sub_realtime,     // 订阅进入实时状态
	void   *cb_sub_stop,         // 订阅结束 自动取消订阅
	void   *cb_reply             // 回调的数据
)
{
	s_sis_worker* worker = sis_pointer_list_get(_api_session_list, id_);
	if (!worker)
	{
		return API_REPLY_ERR;
	}

	// 调取sisdb_client对应的函数
	s_sisdb_client_cxt *context = worker->context;

	char ds[128], cmd[128]; 
	int cmds = sis_str_divide(cmd_, '.', ds, cmd);
	if (cmds != 2 || sis_str_subcmp_strict(cmd, "sub,subsno,unsub,unsubsno", ',') < 0)
	{
		return API_REPLY_ERR;
	}

	if (sis_str_subcmp_strict(cmd, "unsub,unsubsno", ',') >= 0)
	{
		// 删除本地的订阅信息
		sisdb_client_ask_unsub(context, cmd_, key_);
		s_sisdb_client_ask *ask = sisdb_client_ask_new(
			context,
			cmd_, 
			key_, 
			(void *)val_, 
			sis_strlen(val_),
			cb_source_, 
			cb_sub_start, 
			cb_sub_realtime,
			cb_sub_stop,
			cb_reply,
			false);
		// 不是订阅的收到响应就自动删除 ask
		// 发给server 取消订阅
		if (cmd_sisdb_client_send_cb(worker, ask) == SIS_METHOD_OK)
		{			
			return API_REPLY_OK;
		}
	}
	else
	{
		if (sisdb_client_ask_sub_exists(context, cmd_, key_))
		{
			return API_REPLY_ERR;
		}
		s_sisdb_client_ask *ask = sisdb_client_ask_new(
			context,
			cmd_, 
			key_, 
			(void *)val_, 
			sis_strlen(val_),
			cb_source_, 
			cb_sub_start, 
			cb_sub_realtime,
			cb_sub_stop,
			cb_reply,
			true);
		if (cmd_sisdb_client_send_cb(worker, ask) == SIS_METHOD_OK)
		{
			return API_REPLY_OK;
		}		
	}
	return API_REPLY_ERR;
}

#if 1

int cb_read1(void *source, void *argv)
{
	printf("%s %d\n", argv ? (char *)argv : "nil", strlen((char *)argv + 1));

	s_sis_json_handle *h = sis_json_load((char *)argv + 1,strlen((char *)argv) -1);
	if (!h) {return -1;}

	int iii=1;
	sis_json_show(h->node,&iii);
	sis_json_close(h);

	return 0;
}

int ask_sisdb(int id_)
{
	s_sis_worker* worker = sis_pointer_list_get(_api_session_list, id_);
	if (!worker)
	{
		return API_REPLY_ERR;
	}
	s_sis_message *msg = sis_message_create();
	sis_message_set(msg, "cmd", "sdb.set", NULL);
	sis_message_set(msg, "key", "key1", NULL);
	sis_message_set(msg, "val", "my is dzd.", NULL);
	sis_message_set_int(msg, "vlen", 10);

	int o = cmd_sisdb_client_ask_chars(worker, msg);

	if (o == SIS_METHOD_OK)
	{
		printf("OK : %s\n", sis_message_get_str(msg,"reply"));
	}
	else
	{
		printf("NO : %s\n",sis_message_get_str(msg,"info"));
	}
	// 不是订阅的收到响应就自动删除
	sis_message_destroy(msg);
	return API_REPLY_OK;
}
int main()
{
	// int no = api_sisdb_client_create("woan2007.ticp.io", 7329, "", ""); 
	int no = api_sisdb_client_create("192.168.3.118", 7329, "aaa", "aaa"); 

	// api_sisdb_command_ask(no, "show", NULL, NULL, cb_read1);
	// api_sisdb_command_ask(no, "sdb.get", "sh600601.stk_snapshot", "{\"date\":20200204,\"format\":\"chars\"}", cb_read1);
	// 订阅有问题
	api_sisdb_command_sub(no, "sdb.subsno", "sh600601.stk_snapshot", "{\"date\":20200204,\"format\":\"chars\"}", 
		NULL, cb_read1, cb_read1, cb_read1, cb_read1);
	// ask_sisdb(no);


	while(1)
	{
		sis_sleep(1000);
	}

	api_sisdb_client_destroy(no);  
}
#endif