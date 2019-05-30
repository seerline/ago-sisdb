
#include <sis_net.v1.h>
#include <sis_net.cmd.h>
#include <sis_net.tcp.h>

s_sis_net_client *sis_net_client_create(int cid_)
{
	s_sis_net_client *client = SIS_MALLOC(s_sis_net_client, client);
	
	client->cid = cid_;

	client->message = NULL;
	
	client->sub_msgs = sis_map_pointer_create_v(sis_net_message_destroy);

	// client->send_buffer = sis_memory_create();
	client->recv_buffer = sis_memory_create();

	return client;
}
void sis_net_client_destroy(void *client_)
{
	s_sis_net_client *client = (s_sis_net_client *)client_;

	sis_map_pointer_destroy(client->sub_msgs);

	sis_net_message_destroy(client->message);

	// sis_memory_destroy(client_->send_buffer);
	sis_memory_destroy(client->recv_buffer);

	sis_free(client);
}


////////////////////////////////////////////////
//    s_sis_net_class define     
////////////////////////////////////////////////

void cb_after_recv(void *handle_, int cid_, const char* in_, size_t ilen_);

s_sis_net_class *sis_net_class_create(s_sis_url *url_)
{
	s_sis_net_class *sock = SIS_MALLOC(s_sis_net_class, sock);
	memmove(&sock->url, url_, sizeof(s_sis_url));

	sock->connect_method = SIS_NET_IO_CONNECT;
	if (!sis_strcasecmp(sock->url.io_connect,"listen"))
	{
		sock->connect_method = SIS_NET_IO_LISTEN;
	}
	sock->connect_role = SIS_NET_ROLE_CLIENT;
	if (!sis_strcasecmp(sock->url.io_role,"server"))
	{
		sock->connect_role = SIS_NET_ROLE_SERVER;
		sock->map_command = sis_net_command_create();
	}

	sis_mutex_rw_create(&sock->mutex_read);
	sis_mutex_rw_create(&sock->mutex_write);

	sock->sessions = sis_map_pointer_create_v(sis_net_client_destroy);	
	sock->sub_clients = sis_map_pointer_create_v(sis_struct_list_destroy);

	sock->connect_style = SIS_NET_PROTOCOL_TCP;
	if (!sis_strcasecmp(sock->url.protocol,"ws"))
	{
		sock->connect_style = SIS_NET_PROTOCOL_WS;
	}

	switch (sock->connect_style)
	{
	case SIS_NET_PROTOCOL_WS:
		/* code */
		break;
	default:
		sock->cb_connected = NULL;
		sock->cb_disconnect = NULL;
		sock->cb_pack_ask = sis_net_pack_tcp;
		sock->cb_unpack_ask = sis_net_unpack_tcp;
		sock->cb_pack_reply = sis_net_pack_reply_tcp;
		sock->cb_unpack_reply = sis_net_unpack_reply_tcp;
		sock->cb_recv = NULL;
		break;
	}
	sock->status = SIS_NET_INIT;
	return sock;
}

void sis_net_class_destroy(s_sis_net_class *sock_)
{
	if (!sock_) {return;}
	s_sis_net_class *sock = (s_sis_net_class *)sock_;

	sis_net_class_close(sock);

	sis_mutex_rw_destroy(&sock->mutex_read);
	sis_mutex_rw_destroy(&sock->mutex_write);

	sis_map_pointer_destroy(sock->sub_clients);
	sis_map_pointer_destroy(sock->sessions);

	if (sock->connect_role == SIS_NET_ROLE_SERVER)
	{
		sis_net_command_destroy(sock->map_command);
	}

	sis_free(sock);
}
s_sis_net_client *_get_net_client(s_sis_net_class *sock_, int cid_)
{
	s_sis_net_client *client = NULL;
	if (sock_->connect_method == SIS_NET_IO_LISTEN)
	{
		char key[16];
		sis_sprintf(key, 16, "%d", cid_);
		client = sis_map_pointer_get(sock_->sessions, key);
	}	
	else
	{
		client = sis_map_pointer_get(sock_->sessions, "0");
	}
	return client;
}
void _sis_net_class_client_remove(s_sis_net_class *sock_, int cid_)
{
	// 清理掉sub信息
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(sock_->sub_clients);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sis_struct_list *cids = (s_sis_struct_list *)sis_dict_getval(de);
		for (int i = 0; i < cids->count; i++)
		{
			int cid = *(int *)sis_struct_list_get(cids, i);
			if (cid_ == cid) 
			{
				sis_struct_list_delete(cids, i, 1);
				break;
			}
		}
	}
	sis_dict_iter_free(di);	
	// 删除客户对话
	char key[16];
	sis_sprintf(key, 16, "%d", cid_);
	sis_map_pointer_del(sock_->sessions, key);
}
void sis_net_class_close(s_sis_net_class *sock_)
{
	if (sock_->status == SIS_NET_EXITING || sock_->status == SIS_NET_EXIT)
	{
		return;
	}
	sock_->status = SIS_NET_EXITING;
	// 等线程结束,必须加在这里，不能换顺序，否则退出时会有异常
	if (sock_->connect_method == SIS_NET_IO_LISTEN)
	{
		sis_socket_server_close(sock_->server);		
		sis_socket_server_destroy(sock_->server);
	}
	else
	{
		sis_plan_task_destroy(sock_->client_thread_connect);
		sock_->client_thread_connect = NULL;
		sis_socket_client_close(sock_->client);		
		sis_socket_client_destroy(sock_->client);
	}
	sis_map_pointer_clear(sock_->sessions);
	sis_map_pointer_clear(sock_->sub_clients);

	sock_->status = SIS_NET_EXIT;
	LOG(5)("net close ... \n");
}
void *_client_thread_connect(void *argv_)
{
	s_sis_net_class *socket = (s_sis_net_class *)argv_;
	if (!socket)
	{
		return NULL;
	}
	s_sis_plan_task *task = socket->client_thread_connect;
	sis_plan_task_wait_start(task);
	while (sis_plan_task_working(task))
	{
		if (sis_plan_task_execute(task))
		{
			if (socket->status == SIS_NET_WAITINIT)
			{
				socket->status = SIS_NET_INITING;
				sis_socket_client_close(socket->client);		
				sis_socket_client_open(socket->client);
				// printf("client working .1. %d \n", client->status == SIS_UV_CONNECT_FINISH);
			}
		}
	}
	sis_plan_task_wait_stop(task);
	return NULL;
}
void _sis_net_class_client_append(s_sis_net_class *sock_, int cid_)
{
	char key[16];
	sis_sprintf(key, 16, "%d", cid_);
	s_sis_net_client *client = sis_net_client_create(cid_);
	sis_map_pointer_set(sock_->sessions, key, client);
}
void cb_node_connect(void *handle_, int cid_)
{
	printf("%s\n", __func__);		
	s_sis_net_class *socket = (s_sis_net_class *)handle_;
	sis_socket_server_set_recv_cb(socket->server, cid_, cb_after_recv);
	// 这里创建客户端的缓存数据 订阅等信息
	_sis_net_class_client_append(socket, cid_);
	if (socket->cb_connected)
	{
		socket->cb_connected(socket, cid_);
	}
}

void cb_node_disconnect(void *handle_, int cid_)
{
	printf("%s\n", __func__);	
	//  删除cid对应的缓存数据
	s_sis_net_class *socket = (s_sis_net_class *)handle_;
	_sis_net_class_client_remove(socket, cid_);
	if (socket->cb_disconnect)
	{
		socket->cb_disconnect(socket, cid_);
	}
}
////////////////////////////////////////////////////////////////
//  -- client --
////////////////////////////////////////////////////////////////

static void cb_connected(void *handle_)
{
	s_sis_net_class *socket = (s_sis_net_class *)handle_;
	sis_socket_client_set_recv_cb(socket->client, cb_after_recv);
	printf("client connect ok.\n");	
	socket->client->status = SIS_NET_INITED;
	_sis_net_class_client_append(socket, 0);
	if (socket->cb_connected)
	{
		socket->cb_connected(socket, 0);
	}
}
static void cb_disconnected(void *handle_)
{
	s_sis_net_class *socket = (s_sis_net_class *)handle_;
	if (socket->status == SIS_NET_EXITING || socket->status == SIS_NET_EXIT)
	{
		return;
	}
	_sis_net_class_client_remove(socket, 0);
	socket->client->status = SIS_NET_WAITINIT;
	printf("client disconnect.\n");	
	if (socket->cb_connected)
	{
		socket->cb_connected(socket, 0);
	}
}
void sis_net_class_open(s_sis_net_class *sock_)
{
	if (sock_->status != SIS_NET_INIT && sock_->status != SIS_NET_WAITINIT)
	{
		return;
	}
	sock_->status = SIS_NET_INITING;

	if (sock_->connect_method == SIS_NET_IO_LISTEN)
	{
		sock_->server = sis_socket_server_create();
		sock_->server->port = sock_->url.port;
		sis_strcpy(sock_->server->ip, 128, sock_->url.ip);
		sock_->server->data = sock_;
		sis_socket_server_set_cb(sock_->server, cb_node_connect, cb_node_disconnect);
		sis_socket_server_open(sock_->server);		
		sock_->status = SIS_NET_INITED;
	}
	else
	{
		sock_->client = sis_socket_client_create();	
		sock_->client_thread_connect = sis_plan_task_create();
		sock_->client_thread_connect->work_mode = SIS_WORK_MODE_GAPS;
		sock_->client_thread_connect->work_gap.delay = 5; // 间隔5秒检查一次
		if (!sis_plan_task_start(sock_->client_thread_connect, _client_thread_connect, sock_))
		{
			LOG(3)("can't start client thread.\n");
			sis_plan_task_destroy(sock_->client_thread_connect);
			sis_socket_client_destroy(sock_->client);
			sock_->status = SIS_NET_INIT;
			return ;
		}
		sock_->client->port = sock_->url.port;
		sis_strcpy(sock_->client->ip, 128, sock_->url.ip);
		sock_->client->data = sock_;
		sis_socket_client_set_cb(sock_->client, cb_connected, cb_disconnected);
		sis_socket_client_open(sock_->client);
	}
	LOG(5)("net working ... \n");
}

void sis_net_class_set_recv_cb(s_sis_net_class *sock_, callback_net_recv cb_)
{
	sock_->cb_recv = cb_; //  公共接收回调
}


void _sis_net_class_recv_cb(
	s_sis_net_class *sock_, 
	s_sis_net_client *client_,
	s_sis_net_message *msg_)
{
	if (sock_->connect_role == SIS_NET_ROLE_CLIENT)
	{
		// 客户端来说，收到的永远是应答
		// 先去对应发送请求，对应上就回调
		// 如果回来消息为订阅，就去调用订阅对应回调
		// 如果是其他，表示有请求过来，调用默认回调
		// if (!msg_->reply)
		// {
		// 	return ;
		// }
		// s_sis_net_reply *reply = (s_sis_net_reply *)msg_->reply;
		// if (reply->type == SIS_NET_REPLY_ARRAY && reply->count > 1)
		// {
		// 	s_sis_net_reply *subkey = reply->nodes[0];
		// 	if (subkey->type == SIS_NET_REPLY_STRING) //SIS_NET_REPLY_SUB)
		// 	{
		// 		s_sis_net_message *submsg = sis_map_pointer_get(client_->sub_msgs, subkey->str);
		// 		if (submsg)
		// 		{
		// 			callback_net_recv cb_recv = (callback_net_recv)submsg->reply;
		// 			cb_recv(sock_, msg_);	
		// 			return ;			
		// 		}
		// 		else
		// 		{
		// 			// 没订阅直接调默认
		// 		}
		// 	}
		// }	
		// if (client_->message)
		// {
		// 	if (client_->message->reply)
		// 	{
		// 		callback_net_recv cb_recv = (callback_net_recv)client_->message->reply;
		// 		cb_recv(sock_, msg_);
		// 	}
		// 	sis_net_message_destroy(client_->message);
		// 	client_->message = NULL;
		// 	return ;
		// }
		// else
		// {
		// 	if (sock_->cb_recv)
		// 	{
		// 		sock_->cb_recv(sock_, msg_);
		// 	}
		// }
	}
	else // SIS_NET_ROLE_SERVER
	{
		// 服务端来说，收到的永远是请求
		msg_->cid = client_->cid;
		s_sis_method *method = sis_method_map_find(sock_->map_command, msg_->command, "system");
		if (method)
		{
			method->proc(sock_, msg_);
		}
		else
		{
			if (sock_->cb_recv)
			{
				sock_->cb_recv(sock_, msg_);
			}
		}
	}
}
void cb_after_recv(void *handle_, int cid_, const char* in_, size_t ilen_)
{
	printf("recv [%d]:%zu [%s]\n", cid_, ilen_, in_);
	s_sis_net_class *socket = (s_sis_net_class *)handle_;

	if (socket->status != SIS_NET_INITED)
	{
		return ;
	}
	if (!socket->cb_unpack_ask)
	{
		return ;
	}
	s_sis_net_client *client = _get_net_client(socket, cid_);
	if (!client)
	{
		return;
	}
	sis_memory_cat(client->recv_buffer, (char *)in_, ilen_);
	while(1)
	{
		s_sis_net_message *msg = sis_net_message_create();
		size_t bytes = socket->cb_unpack_ask(socket, msg, 
			sis_memory(client->recv_buffer), 
			sis_memory_get_size(client->recv_buffer));
		
		if (!bytes)
		{
			sis_net_message_destroy(msg);
			break;
		}
		msg->cid = cid_;
		// 先去对应发送请求，对应上就回调
		// 如果回来消息为订阅，就去调用订阅对应回调
		// 如果是其他，表示有请求过来，调用默认回调
		_sis_net_class_recv_cb(socket, client, msg);

		sis_net_message_destroy(msg);
		// 下一条信息
		sis_memory_move(client->recv_buffer, bytes);
	}
}
bool _sis_net_class_sendto(s_sis_net_class *sock_, int cid_, const char* in_, size_t ilen_)
{
	if (cid_ == 0)
	{
		return  sis_socket_client_send(sock_->client, in_, ilen_);
	}
	return sis_socket_server_send(sock_->server, cid_, in_, ilen_);
}

int _sis_net_class_send(s_sis_net_class *sock_, int style_, s_sis_net_message *mess_, callback_net_recv cb_)
{
	if (sock_->status != SIS_NET_INITED)
	{
		return -8;
	}
	
	if (!sock_->cb_pack_ask)
	{
		return -1;
	}

	// 把发送信息保存到 message_list 中，必须在发送数据前
	s_sis_net_client *client = _get_net_client(sock_, mess_->cid);
	if (!client)
	{
		return -2;
	}
	s_sis_sds out = sock_->cb_pack_ask(sock_, mess_);
	if (!out)
	{
		return -3;
	}

	switch (style_)
	{
	case SIS_NET_FLAG_REQ:
		client->message = sis_net_message_clone(mess_);
		client->message->cb_reply = cb_; 
		break;
	case SIS_NET_FLAG_SUB:
		{
			// 注册订阅的回调
			s_sis_net_message *subkey = (s_sis_net_message *)sis_net_message_clone(mess_);
			subkey->cb_reply = cb_; 
			sis_map_pointer_set(client->sub_msgs, mess_->key, subkey); 
		}
		break;
	default:
		break;
	}

	if (!_sis_net_class_sendto(sock_, mess_->cid, out, sis_sdslen(out)))
	{
		// 发送失败就返回错误
		if (client->message)
		{
			sis_net_message_destroy(client->message);
			client->message = NULL;
		}
		sis_sdsfree(out);
		return -4;
	}
	sis_sdsfree(out);
	return 1; // 发送成功
}
int sis_net_class_send(s_sis_net_class *sock_, s_sis_net_message *mess_, callback_net_recv cb_)
{
	return _sis_net_class_send(sock_, SIS_NET_FLAG_REQ, mess_, cb_);
}
// 发送应答数据
int sis_net_class_reply(s_sis_net_class *sock_, s_sis_net_message *mess_)
{
	// 修改对应状态
}

// int sis_net_class_sub(s_sis_net_class *sock_, s_sis_net_message *mess_, callback_net_recv cb_)
// {
// 	return _sis_net_class_send(sock_, SIS_NET_FLAG_SUB, mess_, cb_);
// }

