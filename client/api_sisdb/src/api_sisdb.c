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
	int                   id;
	int                   isinit;   // worker 是否初始化
	s_sis_message        *usermsg;  // 用户保存信息
	s_sis_worker         *worker;

	s_api_sisdb_cb_sys   *callback;
} s_api_sisdb_class;

s_api_sisdb_class *worker_api_sisdb_create(s_api_sisdb_cb_sys *callback_)
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

_API_SISDB_DLLEXPORT_ int api_sisdb_client_create(s_api_sisdb_cb_sys *callback_)
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
static int _cb_connected(void *source_, void *argv_)
{
	s_api_sisdb_class* mclass = (s_api_sisdb_class* )source_;
	if (mclass->callback && mclass->callback->cb_connected)
	{
		mclass->callback->cb_connected(mclass->id);
	}	
	return SIS_METHOD_OK;
}
static int _cb_disconnect(void *source_, void *argv_)
{
	s_api_sisdb_class* mclass = (s_api_sisdb_class* )source_;
	if (mclass->callback && mclass->callback->cb_disconnect)
	{
		mclass->callback->cb_disconnect(mclass->id);
	}
	return SIS_METHOD_OK;
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
	s_sisdb_client_cxt *context = mclass->worker->context;
	context->cb_source = mclass;
	context->cb_connected = _cb_connected;;
	context->cb_disconnect = _cb_disconnect;
	
	// 连接服务器
	sisdb_client_method_init(mclass->worker);
	
	if (context->status == SIS_CLI_STATUS_WORK)
	{
		// 已经连接服务器 
		return API_REPLY_OK;
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

_API_SISDB_DLLEXPORT_ int api_sisdb_client_cmd(
	int                 id_,             // 句柄
	const char         *cmd_,            // 请求的参数
	const char         *key_,            // 请求的key
	const char         *value_,            // 请求的参数
	size_t              vsize_,
	void               *cb_source_,
	void               *cb_reply_)
{
	s_api_sisdb_class* mclass = sis_pointer_list_get(_api_session_list, id_);
	if (!mclass || !mclass->isinit)
	{
		return API_REPLY_NIL; 
	}
	s_sis_message *msg = sis_message_create();
	sis_message_set(msg, "cmd", cmd_, NULL);
	sis_message_set(msg, "key", key_, NULL);
	sis_message_set(msg, "val", value_, NULL);
	sis_message_set_int(msg, "vlen", vsize_ == 0 ? sis_strlen(value_) : vsize_);
	
	if (cb_source_ && cb_reply_)
	{
		sis_message_set(msg, "cb_source", cb_source_, NULL);
		sis_message_set(msg, "cb_reply", cb_reply_, NULL);
	}
	cmd_sisdb_client_bytes_nowait(mclass->worker, msg);

	sis_message_destroy(msg);
	// 不是订阅的收到响应就自动删除
	return API_REPLY_OK;
}

_API_SISDB_DLLEXPORT_ int api_sisdb_client_cmd_wait(
	int                 id_,             // 句柄
	const char         *cmd_,            // 请求的参数
	const char         *key_,            // 请求的key
	const char         *value_,            // 请求的参数
	size_t              vsize_,
	void               *cb_source_,
	void               *cb_reply_)
{
	s_api_sisdb_class* mclass = sis_pointer_list_get(_api_session_list, id_);
	if (!mclass || !mclass->isinit)
	{
		return API_REPLY_NIL; 
	}
	s_sis_message *msg = sis_message_create();
	sis_message_set(msg, "cmd", cmd_, NULL);
	sis_message_set(msg, "key", key_, NULL);
	sis_message_set(msg, "val", value_, NULL);
	sis_message_set_int(msg, "vlen", vsize_ == 0 ? sis_strlen(value_) : vsize_);
	
	int o = cmd_sisdb_client_bytes_wait(mclass->worker, msg);

	if (o == SIS_METHOD_NOANS) 
	{
		sis_sdsfree(msg->rmsg);
		msg->rmsg = sis_sdsnew("server too slow.");
		msg->rans = API_REPLY_NOANS;
	}
	else if (o == API_REPLY_NONET)
	{
		sis_sdsfree(msg->rmsg);
		msg->rmsg = sis_sdsnew("api no work. wait...");
		msg->rans = API_REPLY_NOWORK;
	}
	if (cb_reply_)
	{
		cb_api_sisdb_ask *cb_reply = (cb_api_sisdb_ask *)cb_reply_;
		cb_reply(cb_source_, msg->rans, key_, msg->rmsg, msg->rmsg ? sis_sdslen(msg->rmsg) : 0);
	}

	sis_message_destroy(msg);
	// 不是订阅的收到响应就自动删除
	return API_REPLY_OK;
}
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

#if 0

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

int cb_reply1(void *source, int rsno, void *key, void *val)
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
		return API_REPLY_ERROR;
	}
	s_sis_message *msg = sis_message_create();
	sis_message_set(msg, "cmd", "sdb.set", NULL);
	sis_message_set(msg, "key", "key1", NULL);
	sis_message_set(msg, "val", "my is dzd.", NULL);
	sis_message_set_int(msg, "vlen", 10);

	int o = cmd_sisdb_client_chars_wait(worker, msg);

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
	s_api_sisdb_cb_sys cb = {0};
    // cb.OnConnect         = cb_connect;
    // cb.OnDisconnect      = cb_disconnect1;

    // cb.OnRtnDayBegin     = cb_sub_info;
    // cb.OnRtnDayEnd       = cb_sub_stop1;
    // cb.OnRtnTimeless     = cb_sub_info;
    // cb.OnRtnTradingCode  = cb_sub_code;
    // cb.OnRtnMarket       = cb_sub_market;
    // cb.OnRtnIndex        = cb_sub_index;
    // cb.OnRtnTransaction  = cb_sub_trans;
    // cb.OnRtnOrder        = cb_sub_order;
 
 	int no = api_sisdb_client_create(&cb); 
	
	int o = api_sisdb_client_open(no, "{\"ip\":\"192.168.3.118\",\"port\":10001,\"username\":\"guest\",\"password\":\"guest1234\"}");
	// int no = api_sisdb_client_create("woan2007.ticp.io", 7329, "", ""); 
	// int no = api_sisdb_client_create("192.168.3.118", 7329, "aaa", "aaa"); 

	api_sisdb_client_command(no, "show", NULL, NULL, cb_reply1);
	// api_sisdb_client_command(no, "mdb.get", "sh600601.stk_right", "{\"format\":\"struct\"}", cb_reply1);
	// api_sisdb_client_command(no, "mdb.get", "BK000000.bkinfo", "{\"format\":\"struct\"}", cb_reply1);
	// api_sisdb_client_command(no, "mdb.get", "BK600001.bkcodes", "{\"format\":\"struct\"}", cb_reply1);
	// api_sisdb_client_command(no, "mdb.get", "SH600600.bkinside", "{\"format\":\"struct\"}", cb_reply1);
	
	// // 获取二进制数据
	// api_sisdb_client_command(no, "nowdb.get", "sh600601.stk_snapshot", "{\"date\":20200204,\"format\":\"struct\"}", cb_reply1);
	// 订阅二进制数据
	// api_sisdb_client_command(no, "nowdb.sub", "sh600601.stk_snapshot", "{\"date\":20200204,\"format\":\"struct\"}", 
	// 	NULL, cb_read1, cb_read1, cb_read1, cb_reply1);
	// ask_sisdb(no);


	while(1)
	{
		sis_sleep(1000);
	}
	api_sisdb_client_close(no);
	api_sisdb_client_destroy(no);  
	return 0;
}
#endif