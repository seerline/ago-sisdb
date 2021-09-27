#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <api_sisdb.h>
#include "sis_list.h"
#include "sisdb_client.h"
#include "worker.h"

static s_sis_pointer_list *_api_session_list = NULL;       

typedef struct s_api_sisdb_class
{
	int                          id;
	int                          isinit;   // worker 是否初始化
	int                          isopen;   // 是否打开
	s_sis_message               *usermsg;  // 用户保存信息
	s_sis_worker                *worker;

	s_api_sisdb_client_callback *callback;
} s_api_sisdb_class;

s_api_sisdb_class *worker_api_sisdb_create(s_api_sisdb_client_callback *callback_)
{
	s_api_sisdb_class *mclass = SIS_MALLOC(s_api_sisdb_class, mclass);
	mclass->worker = SIS_MALLOC(s_sis_worker, mclass->worker);
	mclass->callback = callback_;
	mclass->usermsg = sis_message_create();
	return mclass;
}
void worker_api_sisdb_destroy(void *mclass_)
{
	s_api_sisdb_class *mclass = (s_api_sisdb_class *)mclass_;
	if (mclass->worker && mclass->isinit)
	{
		sisdb_client_method_uninit(mclass->worker);
		sisdb_client_uninit(mclass->worker);
	}
	sis_free(mclass->worker);
	sis_message_destroy(mclass->usermsg);
	sis_free(mclass);
}

_API_SISDB_DLLEXPORT_ int api_sisdb_client_create(s_api_sisdb_client_callback *callback_)
{
	if (!_api_session_list)
	{
		sis_socket_init();
		_api_session_list = sis_pointer_list_create();
		_api_session_list->vfree = worker_api_sisdb_destroy;
	}
	s_api_sisdb_class *mclass = worker_api_sisdb_create(callback_);	
	mclass->id = _api_session_list->count;
	sis_pointer_list_push(_api_session_list, mclass);
	return mclass->id;
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
			sis_socket_uninit();
		}
	}
}

// { "ip" : "127.0.0.1",
//   "port" : 10000,
//   "username" : "guest",
//   "password" : "guest1234",
// }
_API_SISDB_DLLEXPORT_ int api_sisdb_client_open(int id_, const char *conf_)
{
	s_api_sisdb_class* mclass = sis_pointer_list_get(_api_session_list, id_);
	if (!mclass)
	{
		return API_REPLY_NIL;
	}
	s_sis_json_handle *handle = sis_json_load(conf_, sis_strlen(conf_));
	if (!handle)
	{
		return API_REPLY_INERR;
	}
	// 初始化
	sisdb_client_init(mclass->worker, handle->node);
	sis_json_close(handle);
	mclass->isinit = 1;
	// 连接服务器
	sisdb_client_method_init(mclass->worker);
	
	s_sisdb_client_cxt *context = mclass->worker->context;
	int wait = 10000;
	while(wait > 0)
	{
		sis_sleep(50);
		wait -= 50;
		if (context->status == SIS_CLI_STATUS_WORK)
		{
			mclass->isopen = 1;
			// 已经连接服务器 
			return API_REPLY_OK;
		}
	}
	return API_REPLY_NONET;
}
// 关闭接口
_API_SISDB_DLLEXPORT_ void api_sisdb_client_close(int id_)
{
	s_api_sisdb_class* mclass = sis_pointer_list_get(_api_session_list, id_);
	if (mclass && mclass->isinit)
	{
		sisdb_client_method_uninit(mclass->worker);
		sisdb_client_uninit(mclass->worker);
		mclass->isinit = 0;
	}
}
_API_SISDB_DLLEXPORT_ int api_sisdb_client_command(
	int           id_,             // 句柄
	const char   *cmd_,            // 请求的参数
	const char   *key_,            // 请求的key
	const char   *ask_,            // 请求的参数
)
{
	s_api_sisdb_class* mclass = sis_pointer_list_get(_api_session_list, id_);
	if (!mclass || !mclass->isinit)
	{
		return API_REPLY_NIL; 
	}

	// s_sis_worker* worker = sis_pointer_list_get(_api_session_list, id_);
	// if (!worker)
	// {
	// 	return API_REPLY_ERR;
	// }
	// char ds[128], cmd[128]; 
	// sis_str_divide(cmd_, '.', ds, cmd);
	// if (sis_str_subcmp_strict(cmd, "sub,zsub,unsub", ',') >= 0)
	// {
	// 	return API_REPLY_ERR;
	// }

	// // 调取sisdb_client对应的函数
	// s_sisdb_client_cxt *context = worker->context;
	// s_sisdb_client_ask *ask = sisdb_client_ask_new( 
	// 	context,
	// 	cmd_, 
	// 	key_, 
	// 	(void *)val_, 
	// 	sis_strlen(val_),
	// 	NULL, 
	// 	NULL,
	// 	NULL,
	// 	NULL,
	// 	cb_reply,
	// 	false);
	
	// if (cmd_sisdb_client_send_cb(worker, ask) == SIS_METHOD_OK)
	// {
	// 	return API_REPLY_OK;
	// }
	// // 不是订阅的收到响应就自动删除
	// return API_REPLY_ERR;
}


// {
// 	s_sis_worker* worker = sis_pointer_list_get(_api_session_list, id_);
// 	if (!worker)
// 	{
// 		return API_REPLY_ERR;
// 	}

// 	// 调取sisdb_client对应的函数
// 	s_sisdb_client_cxt *context = worker->context;

// 	char ds[128], cmd[128]; 
// 	int cmds = sis_str_divide(cmd_, '.', ds, cmd);
// 	if (cmds != 2 || sis_str_subcmp_strict(cmd, "sub,zsub,unsub", ',') < 0)
// 	{
// 		return API_REPLY_ERR;
// 	}

// 	if (sis_str_subcmp_strict(cmd, "unsub", ',') >= 0)
// 	{
// 		// 删除本地的订阅信息
// 		sisdb_client_ask_unsub(context, cmd_, key_);
// 		s_sisdb_client_ask *ask = sisdb_client_ask_new(
// 			context,
// 			cmd_, 
// 			key_, 
// 			(void *)val_, 
// 			sis_strlen(val_),
// 			cb_source_, 
// 			cb_sub_start, 
// 			cb_sub_realtime,
// 			cb_sub_stop,
// 			cb_reply,
// 			false);
// 		// 不是订阅的收到响应就自动删除 ask
// 		// 发给server 取消订阅
// 		if (cmd_sisdb_client_send_cb(worker, ask) == SIS_METHOD_OK)
// 		{			
// 			return API_REPLY_OK;
// 		}
// 	}
// 	else
// 	{
// 		if (sisdb_client_ask_sub_exists(context, cmd_, key_))
// 		{
// 			return API_REPLY_ERR;
// 		}
// 		s_sisdb_client_ask *ask = sisdb_client_ask_new(
// 			context,
// 			cmd_, 
// 			key_, 
// 			(void *)val_, 
// 			sis_strlen(val_),
// 			cb_source_, 
// 			cb_sub_start, 
// 			cb_sub_realtime,
// 			cb_sub_stop,
// 			cb_reply,
// 			true);
// 		if (cmd_sisdb_client_send_cb(worker, ask) == SIS_METHOD_OK)
// 		{
// 			return API_REPLY_OK;
// 		}		
// 	}
// 	return API_REPLY_ERR;
// }


// 可以传递用户的数据，这样回调函数返回时 通过下面的函数就知道调用主体是谁
_API_SISDB_DLLEXPORT_ void *api_sisdb_client_get_userdata(int id_, const char *name_)
{
	s_api_sisdb_class* mclass = sis_pointer_list_get(_api_session_list, id_);
	if (!mclass)
	{
		return NULL;
	}
	return sis_message_get(mclass->usermsg, name_);
}
_API_SISDB_DLLEXPORT_ void  api_sisdb_client_set_userdata(int id_, const char *name_, void *data_)
{
	s_api_sisdb_class* mclass = sis_pointer_list_get(_api_session_list, id_);
	if (!mclass)
	{
		return ;
	}
	return sis_message_set(mclass->usermsg, name_, data_, NULL);
}

#if 1

int cb_read1(void *source, void *argv)
{
	printf("%s %d\n", argv ? (char *)argv : "nil", strlen((char *)argv));

	// s_sis_json_handle *h = sis_json_load((char *)argv,strlen((char *)argv));
	// if (!h) {return -1;}

	// int iii=1;
	// sis_json_show(h->node,&iii);
	// sis_json_close(h);

	return 0;
}

int cb_reply1(void *source, int rid, void *key, void *val)
{
	printf("%d : %s %d, vsize = %d\n", rid, 
		key ? (char *)key : "nil", 
		key ? sis_sdslen(key) : 0,
		val ? sis_sdslen(val) : 0);
	if (val) sis_out_binary("val", val, 16);
	// printf("%d : %s %d, vsize = %d\n", rid, 
	// 	key ? (char *)key : "nil", 
	// 	key ? sis_sdslen(key) : 0,
	// 	val ? sis_sdslen(val) : 0);

	// s_sis_json_handle *h = sis_json_load((char *)argv,strlen((char *)argv));
	// if (!h) {return -1;}

	// int iii=1;
	// sis_json_show(h->node,&iii);
	// sis_json_close(h);

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

	int rid = sis_message_get_int(msg,"rid");
	char *key = sis_message_get_str(msg,"key");
	char *val = sis_message_get_str(msg,"val");
	if (o == SIS_METHOD_OK)
	{
		printf("OK : %d %s %s\n", rid, key, val);
	}
	else
	{
		printf("NO : %d %s %s\n", rid, key, val);
	}
	// 不是订阅的收到响应就自动删除
	sis_message_destroy(msg);
	return API_REPLY_OK;
}

int main()
{
	// int no = api_sisdb_client_create("woan2007.ticp.io", 7329, "", ""); 
	// int no = api_sisdb_client_create("192.168.3.118", 7329, "aaa", "aaa"); 
	int no = api_sisdb_client_create("192.168.1.202", 7329, "guest", "guest1234"); 

	// api_sisdb_command_ask(no, "show", NULL, NULL, cb_reply1);
	api_sisdb_command_ask(no, "mdb.get", "sh600601.stk_right", "{\"format\":\"struct\"}", cb_reply1);
	api_sisdb_command_ask(no, "mdb.get", "BK000000.bkinfo", "{\"format\":\"struct\"}", cb_reply1);
	api_sisdb_command_ask(no, "mdb.get", "BK600001.bkcodes", "{\"format\":\"struct\"}", cb_reply1);
	api_sisdb_command_ask(no, "mdb.get", "SH600600.bkinside", "{\"format\":\"struct\"}", cb_reply1);
	
	// 获取二进制数据
	api_sisdb_command_ask(no, "nowdb.get", "sh600601.stk_snapshot", "{\"date\":20200204,\"format\":\"struct\"}", cb_reply1);
	// 订阅二进制数据
	// api_sisdb_command_sub(no, "nowdb.sub", "sh600601.stk_snapshot", "{\"date\":20200204,\"format\":\"struct\"}", 
	// 	NULL, cb_read1, cb_read1, cb_read1, cb_reply1);
	// ask_sisdb(no);


	while(1)
	{
		sis_sleep(1000);
	}

	api_sisdb_client_destroy(no);  
}
#endif