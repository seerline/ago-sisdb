
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

void _cb_after_recv(void *handle_, int cid_, const char* in_, size_t ilen_);

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

	sock->cb_connected = NULL;
	sock->cb_disconnect = NULL;
	sock->cb_auth = NULL;
	sock->cb_recv = NULL;
	switch (sock->connect_style)
	{
	case SIS_NET_PROTOCOL_WS:
		/* code */
		break;
	default:
		sock->cb_pack = sis_net_pack_tcp;
		sock->cb_unpack = sis_net_unpack_tcp;
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
void sis_net_class_close(s_sis_net_class *sock_)
{
	if (sock_->status == SIS_NET_EXITING || sock_->status == SIS_NET_EXIT)
	{
		return;
	}
	sock_->status = SIS_NET_EXITING;
	// prevent repeated close
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
void _sis_net_class_client_remove(s_sis_net_class *sock_, int cid_)
{
	// clear sub info
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
	// delete client session
	char key[16];
	sis_sprintf(key, 16, "%d", cid_);
	sis_map_pointer_del(sock_->sessions, key);
}
void _sis_net_class_client_append(s_sis_net_class *sock_, int cid_)
{
	char key[16];
	sis_sprintf(key, 16, "%d", cid_);
	s_sis_net_client *client = sis_net_client_create(cid_);
	sis_map_pointer_set(sock_->sessions, key, client);
}

void _cb_node_connect(void *handle_, int cid_)
{
	printf("%s -- %d\n", __func__, cid_);		
	s_sis_net_class *socket = (s_sis_net_class *)handle_;
	sis_socket_server_set_recv_cb(socket->server, cid_, _cb_after_recv);
	// 这里创建客户端的缓存数据 订阅等信息
	_sis_net_class_client_append(socket, cid_);
	if (socket->cb_connected)
	{
		socket->cb_connected(socket, cid_);
	}
}

void _cb_node_disconnect(void *handle_, int cid_)
{
	printf("%s -- %d\n", __func__, cid_);	
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

static void _cb_connected(void *handle_)
{
	s_sis_net_class *socket = (s_sis_net_class *)handle_;
	sis_socket_client_set_recv_cb(socket->client, _cb_after_recv);
	LOG(5)("client connect ok.\n");	
	socket->status = SIS_NET_INITED;
	_sis_net_class_client_append(socket, 0);
	if (socket->cb_connected)
	{
		socket->cb_connected(socket, 0);
	}
}
static void _cb_disconnect(void *handle_)
{
	s_sis_net_class *socket = (s_sis_net_class *)handle_;
	if (socket->status == SIS_NET_EXITING || socket->status == SIS_NET_EXIT)
	{
		return;
	}
	_sis_net_class_client_remove(socket, 0);
	socket->status = SIS_NET_WAITINIT;
	// socket->client->cb_recv = NULL;
	LOG(5)("client disconnect.\n");	
	if (socket->cb_disconnect)
	{
		socket->cb_disconnect(socket, 0);
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
		sis_socket_server_set_cb(sock_->server, _cb_node_connect, _cb_node_disconnect);
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
		sis_socket_client_set_cb(sock_->client, _cb_connected, _cb_disconnect);
		sis_socket_client_open(sock_->client);
	}
	LOG(5)("net working ... \n");
}

// void sis_net_class_set_recv_cb(s_sis_net_class *sock_, callback_net_recv cb_)
// {
// 	sock_->cb_recv = cb_; // 公共接收回调, 可能是请求也可能是应答
// }

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

		if (msg_->subpub)
		{
			// msg_->key must 
			s_sis_net_message *submsg = sis_map_pointer_get(client_->sub_msgs, msg_->key);
			if (submsg && submsg->cb_reply)
			{
				callback_net_recv cb_recv = (callback_net_recv)submsg->cb_reply;
				cb_recv(sock_, msg_);	
				return ;			
			}
			else
			{
				// 没订阅直接调默认
			}
		}	
		if (client_->message)
		{
			if (client_->message->cb_reply)
			{
				callback_net_recv cb_recv = (callback_net_recv)client_->message->cb_reply;
				cb_recv(sock_, msg_);
			}
			sis_net_message_destroy(client_->message);
			client_->message = NULL;
			return ;
		}
		else
		{
			if (sock_->cb_recv)
			{
				sock_->cb_recv(sock_, msg_);
			}
		}
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
			if(sis_socket_check_auth(sock_, msg_->cid))
			{
				if (sock_->cb_recv)
				{
					sock_->cb_recv(sock_, msg_);
				}
			}
		}
	}
}
void _cb_after_recv(void *handle_, int cid_, const char* in_, size_t ilen_)
{
	printf("recv [%d] [%s]\n", cid_, in_);
	// sis_out_binary("recv:",in_, ilen_);
	s_sis_net_class *socket = (s_sis_net_class *)handle_;

	if (socket->status != SIS_NET_INITED)
	{
		return ;
	}
	if (!socket->cb_unpack)
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
		if (socket->connect_role == SIS_NET_ROLE_CLIENT)
		{
			msg->style = SIS_NET_REPLY_MESSAGE;
		}
		else
		{
			msg->style = SIS_NET_ASK_MESSAGE;
		}
		
		// int c = socket->cb_unpack(socket, msg, client->recv_buffer); 
			// sis_memory(client->recv_buffer), 
			// sis_memory_get_size(client->recv_buffer));
		
		if (!socket->cb_unpack(socket, msg, client->recv_buffer))
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
		// sis_memory_move(client->recv_buffer, bytes);
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

int sis_net_class_send_ask(s_sis_net_class *sock_, s_sis_net_message *mess_, callback_net_recv cb_)
{
	if (sock_->status != SIS_NET_INITED)
	{
		return -8;
	}
	if (!mess_ || mess_->style > SIS_NET_ASK_MESSAGE)
	{
		return -7;
	}
	if (!sock_->cb_pack)
	{
		return -1;
	}
	// int style = SIS_NET_FLAG_REQ;
	// if (!sis_strcasecmp(mess_->command, "sub"))
	// {
	// 	style |= SIS_NET_FLAG_SUB;
	// }
	int style = SIS_NET_FLAG_REQ;
	if (mess_->subpub)
	{
		style |= SIS_NET_FLAG_SUB;
		if(!cb_)
		{
			// subscribe mode must have cb_
			return -6;
		}
	}
	s_sis_net_client *client = _get_net_client(sock_, mess_->cid);
	if (!client)
	{
		return -2;
	}
	s_sis_sds out = sock_->cb_pack(sock_, mess_);
	if (!out)
	{
		return -3;
	}
	if (style & SIS_NET_FLAG_REQ)
	{
		client->message = sis_net_message_clone(mess_);
		client->message->cb_reply = cb_; 
	}
	if (style & SIS_NET_FLAG_SUB)
	{
		s_sis_net_message *subkey = (s_sis_net_message *)sis_net_message_clone(mess_);
		subkey->cb_reply = cb_; 
		sis_map_pointer_set(client->sub_msgs, mess_->key, subkey); 
	}

	if (!_sis_net_class_sendto(sock_, mess_->cid, out, sis_sdslen(out)))
	{
		if (style & SIS_NET_FLAG_REQ)
		{
			sis_net_message_destroy(client->message);
			client->message = NULL;
		}
		if (style & SIS_NET_FLAG_SUB)
		{
			sis_map_pointer_del(client->sub_msgs, mess_->key); 
		}
		sis_sdsfree(out);
		return -4;
	}
	sis_sdsfree(out);
	return 0; // ok
}
// send reply
int sis_net_class_send_ans(s_sis_net_class *sock_, s_sis_net_message *mess_)
{
	if (sock_->status != SIS_NET_INITED)
	{
		return -8;
	}
	if (!mess_ || mess_->style < SIS_NET_REPLY_MESSAGE)
	{
		return -7;
	}
	if (!sock_->cb_pack)
	{
		return -1;
	}
	s_sis_net_client *client = _get_net_client(sock_, mess_->cid);
	if (!client)
	{
		return -2;
	}
	s_sis_sds out = sock_->cb_pack(sock_, mess_);
	if (!out)
	{
		return -3;
	}
	if (!_sis_net_class_sendto(sock_, mess_->cid, out, sis_sdslen(out)))
	{
		sis_sdsfree(out);
		return -4;
	}
	sis_sdsfree(out);
	return 0; // ok
}

#if 0
// #define TEST_PORT 7328
#define TEST_PORT 7777
#define TEST_IP "127.0.0.1"
int __id = 0; 
s_sis_net_class *session = NULL;
int __exit = 0;
bool issub = false;
bool ispub = false;
bool isget = false;
bool isset =false;
bool iswork = false;

void exithandle(int sig)
{
	printf("exit .1. \n");	
	if (session)
	{
		sis_net_class_close(session);
		sis_net_class_destroy(session);
	}
	printf("exit .ok . \n");
	__exit = 1;
}
void cb_recv_sub(struct s_sis_net_class *sock_,s_sis_net_message *msg)
{
	printf("recv sub msg: %s [%d:%d]\n", msg->key, msg->style, msg->cid);
	printf(":::: %s\n", msg->rval);
}
void cb_recv_get(struct s_sis_net_class *sock_,s_sis_net_message *msg)
{
	printf("recv get msg: [%d:%d]\n", msg->style, msg->cid);

}
void cb_recv(struct s_sis_net_class *sock_,s_sis_net_message *msg)
{
	printf("recv msg: [%d:%d]\n", msg->style, msg->cid);
}
void cb_connect(struct s_sis_net_class *sock_, int cid_)
{
	printf("open connect [%d]\n", cid_);
	iswork = true;
	if (sock_->connect_role == SIS_NET_ROLE_CLIENT)
	{
		{
			s_sis_net_message *msg = sis_net_message_create();
			msg->cid = cid_;
			msg->style = SIS_NET_ASK_ARRAY;
			msg->command = sdsnew("auth");
			msg->key = sdsnew("clxx11101234");
			sis_net_class_send_ask(sock_, msg, cb_recv_get);
			sis_net_message_destroy(msg);
		}
	}
}
void cb_disconnect(struct s_sis_net_class *sock_, int cid_)
{
	printf("close connect [%d]\n", cid_);
	iswork = false;
}
// s -- server
// c -- client
// a -- connect
// l -- listen
int main(int argc, const char **argv)
{
	if (argc < 3)
	{
		return 0;
	}
	safe_memory_start();

	signal(SIGINT, exithandle);

	s_sis_url url;
	url.port = TEST_PORT;
	if (argv[1][0] == 's')
	{	
		sis_strcpy(url.io_role, 128, "server"); 
	}
	else
	{
		sis_strcpy(url.io_role, 128, "client"); 
	}
	if (argv[2][0] == 'l')
	{
		sis_strcpy(url.ip, 128, "0.0.0.0"); 
		sis_strcpy(url.io_connect, 128, "listen"); 
	}
	else
	{
		sis_strcpy(url.ip, 128, TEST_IP); 
		sis_strcpy(url.io_connect, 128, "connect"); 
	}
	if (argc == 4)
	{
		if (!sis_strcasecmp(argv[3], "sub"))
		{
			issub = true;
		}
		if (!sis_strcasecmp(argv[3], "pub"))
		{
			ispub = true;
		}
		if (!sis_strcasecmp(argv[3], "set"))
		{
			isset = true;
		}
		if (!sis_strcasecmp(argv[3], "get"))
		{
			isget = true;
		}
	}

	session = sis_net_class_create(&url);


	printf("[%d] %d %d\n",argc, session->connect_method, session->connect_role);

	session->cb_connected = cb_connect;
	session->cb_disconnect = cb_disconnect;
	session->cb_recv = cb_recv;
	sis_net_class_open(session);

	LOG(1)("wait work. [%d]\n", session->status);	
	while (!iswork&&!__exit)
	{
		sis_sleep(300);
	}
	LOG(1)("start work. [%d] %d\n", session->status, isset);	
	while (!__exit)
	{
		sis_sleep(1000);
		if (session->connect_role != SIS_NET_ROLE_CLIENT)
		{
			continue;
		}
		if (iswork)
		{
			if (isset)
			{
				s_sis_net_message *msg = sis_net_message_create();
				msg->cid = 1;
				msg->style = SIS_NET_ASK_ARRAY;
				msg->command = sdsnew("set");
				msg->key = sdsnew("myname");
				char val[128];
				sprintf(val, "ding zheng dong [%d]", __id++);
				msg->argv = sdsnew(val);
				int o = sis_net_class_send_ask(session, msg, cb_recv_get);
				printf("send.....[%d, cid = %d].\n", o, msg->cid);
				sis_net_message_destroy(msg);
				isset = false;
			}
			if (isget)
			{
				s_sis_net_message *msg = sis_net_message_create();
				msg->style = SIS_NET_ASK_ARRAY;
				msg->command = sdsnew("get");
				msg->key = sdsnew("myname");
				sis_net_class_send_ask(session, msg, cb_recv_get);
				sis_net_message_destroy(msg);
				isget = false;
			}	
			if (issub)
			{
				s_sis_net_message *msg = sis_net_message_create();
				msg->style = SIS_NET_ASK_ARRAY;
				msg->command = sdsnew("sub");
				msg->key = sdsnew("mysub");
				sis_net_class_send_ask(session, msg, cb_recv_sub);
				sis_net_message_destroy(msg);
				issub = false;
			}		
			if (ispub)
			{
				s_sis_net_message *msg = sis_net_message_create();
				msg->style = SIS_NET_ASK_ARRAY;
				msg->command = sdsnew("pub");
				msg->key = sdsnew("mysub");
				char val[128];
				sprintf(val, "ding pub [%d]", __id++);
				msg->argv = sdsnew(val);
				sis_net_class_send_ask(session, msg, NULL);
				sis_net_message_destroy(msg);
				if (__id > 5)
				{
					ispub = false;
				}
			}	
		}
	}

	safe_memory_stop();
	return 0;		
}
#endif
