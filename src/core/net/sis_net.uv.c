
#include <sis_net.uv.h>
#include <sis_math.h>

// sendfile //

// #include <stdio.h>
// #include <uv.h>
// #include <stdlib.h>
 
// uv_loop_t *loop;
// #define DEFAULT_PORT 7000
 
// uv_tcp_t mysocket;
 
// char *path = NULL;
// uv_buf_t iov;
// char buffer[128];
 
// uv_fs_t read_req;
// uv_fs_t open_req;
// void on_read(uv_fs_t *req);
// void on_write(uv_write_t* req, int status)
// {
//     if (status < 0) 
//     {
//         fprintf(stderr, "Write error: %s\n", uv_strerror(status));
//         uv_fs_t close_req;
//         uv_fs_close(uv_default_loop(), &close_req, open_req.result, NULL);
//         uv_close((uv_handle_t *)&mysocket,NULL);
//         exit(-1);
//     }
//     else 
//     {
//         uv_fs_read(uv_default_loop(), &read_req, open_req.result, &iov, 1, -1, on_read);
//     }
// }
 
// void on_read(uv_fs_t *req)
// {
//     if (req->result < 0) 
//     {
//         fprintf(stderr, "Read error: %s\n", uv_strerror(req->result));
//     }
//     else if (req->result == 0) 
//     {
//         uv_fs_t close_req;
//         // synchronous
//         uv_fs_close(uv_default_loop(), &close_req, open_req.result, NULL);
//         uv_close((uv_handle_t *)&mysocket,NULL);
//     }
//     else
//     {
//         iov.len = req->result;
//         uv_write((uv_write_t *)req,(uv_stream_t *)&mysocket,&iov,1,on_write);
//     }
// }
 
// void on_open(uv_fs_t *req)
// {
//     if (req->result >= 0) 
//     {
//         iov = uv_buf_init(buffer, sizeof(buffer));
//         uv_fs_read(uv_default_loop(), &read_req, req->result,&iov, 1, -1, on_read);
//     }
//     else 
//     {
//         fprintf(stderr, "error opening file: %s\n", uv_strerror((int)req->result));
//         uv_close((uv_handle_t *)&mysocket,NULL);
//         exit(-1);
//     }
// }
 
// void on_connect(uv_connect_t* req, int status)
// {
//     if(status < 0)
//     {
//         fprintf(stderr,"Connection error %s\n",uv_strerror(status));
 
//         return;
//     }
 
//     fprintf(stdout,"Connect ok\n");
 
//     uv_fs_open(loop,&open_req,path,O_RDONLY,-1,on_open);
 
 
// }
 
// int main(int argc,char **argv)
// {
//     if(argc < 2)
//     {
//         fprintf(stderr,"Invaild argument!\n");
 
//         exit(1);
//     }
//     loop = uv_default_loop();
 
//     path = argv[1];
 
//     uv_tcp_init(loop,&mysocket);
 
//     struct sockaddr_in addr;
 
//     uv_connect_t* connect = (uv_connect_t*)malloc(sizeof(uv_connect_t));
 
 
//     uv_ip4_addr("127.0.0.1",DEFAULT_PORT,&addr);
 
//     int r = uv_tcp_connect(connect,&mysocket,(const struct sockaddr *)&addr,on_connect);
 
//     if(r)
//     {
//         fprintf(stderr, "connect error %s\n", uv_strerror(r));
//         return 1;
//     }
 
//     return uv_run(loop,UV_RUN_DEFAULT);
// }


// #define LOGUV(code) LOG(5)("%s : %s", uv_err_name(code), uv_strerror(code))
 
//////////////////////////////////////////////////////////////
// s_sis_socket_session
//////////////////////////////////////////////////////////////

s_sis_socket_session *sis_socket_session_create(int sid_)
{
	s_sis_socket_session *session = SIS_MALLOC(s_sis_socket_session, session); 
	session->session_id = sid_;
	session->used = true;
	session->stop_time = 0;

	session->uvhandle = (uv_tcp_t*)sis_malloc(sizeof(uv_tcp_t));
	session->uvhandle->data = session;
	session->read_buffer = uv_buf_init((char*)sis_malloc(MAX_NET_UV_BUFFSIZE), MAX_NET_UV_BUFFSIZE);
    session->write_buffer = uv_buf_init(NULL, 0);
	return session;	
}
void sis_socket_session_set_rwcb(s_sis_socket_session *session_,
								cb_socket_recv_after cb_recv_, 
								cb_socket_send_after cb_send_)
{
	session_->cb_recv_after = cb_recv_;
	session_->cb_send_after = cb_send_;
}

void sis_socket_session_init(s_sis_socket_session *session_, int sid_)
{
	session_->session_id = sid_;
	session_->used = true;
	session_->stop_time = 0;

	session_->server = NULL;

	if (session_->write_buffer.base)
	{
		sis_object_decr((s_sis_object *)session_->write_buffer.base);
		session_->write_buffer.base = NULL;
		session_->write_buffer.len = 0;
	}

	memset(&session_->write_req, 0, sizeof(uv_write_t));

	sis_free(session_->uvhandle);
	session_->uvhandle = NULL;

	session_->uvhandle = (uv_tcp_t*)sis_malloc(sizeof(uv_tcp_t));
	session_->uvhandle->data = session_;

}
void sis_socket_session_closed(s_sis_socket_session *session_)
{
	session_->used = false;
	session_->stop_time = sis_time_get_now();
}
void sis_socket_session_destroy(void *session_)
{
	s_sis_socket_session *session = (s_sis_socket_session *)session_;
	
	sis_free(session->read_buffer.base);
	session->read_buffer.base = NULL;
	session->read_buffer.len = 0;

	if (session->write_buffer.base)
	{
		sis_object_decr((s_sis_object *)session->write_buffer.base);
		session->write_buffer.base = NULL;
		session->write_buffer.len = 0;
	}

	sis_free(session->uvhandle);
	session->uvhandle = NULL;
	sis_free(session);
}
//////////////////////////////////////////////////////////////
// s_sis_socket_server
//////////////////////////////////////////////////////////////

s_sis_socket_server *sis_socket_server_create()
{
	s_sis_socket_server *socket = SIS_MALLOC(s_sis_socket_server, socket); 

	socket->loop = sis_malloc(sizeof(uv_loop_t));  
	assert(0 == uv_loop_init(socket->loop));

	socket->isinit = false;
	socket->source = socket;
	socket->cb_connected_s2c = NULL;
	socket->cb_disconnect_s2c = NULL;

	socket->sessions = sis_pointer_list_create();	
	socket->sessions->vfree = sis_socket_session_destroy;
	socket->recover_delay = 300;

	LOG(8)("server create.[%p]\n", socket);
	return socket; 
}
void sis_socket_server_destroy(s_sis_socket_server *sock_)
{
	sis_socket_server_close(sock_);
	// while (sock_->isinit)
	// {
	// 	sis_sleep(100);
	// }
	LOG(5)("server exit.\n");
	sis_pointer_list_destroy(sock_->sessions);
	uv_loop_close(sock_->loop); 	
	sis_free(sock_->loop);
	sis_free(sock_);
}

static void cb_server_close(uv_handle_t *handle)
{
	LOG(5)("server close ok.[%p]\n", handle);
}

static void cb_server_of_client_close(uv_handle_t *handle)
{
	s_sis_socket_session *session = (s_sis_socket_session *)handle->data;
	// LOG(8)("session free.[%p]\n", session);
	sis_socket_session_closed(session);
	LOG(5)("server of client close ok.[%p]\n", handle);
}

void sis_socket_server_close(s_sis_socket_server *sock_)
{ 
	if (!sock_->isinit) 
	{
		return ;
	}
	LOG(5)("clear sessions.[%d]\n", sock_->sessions->count);
	for (int i = 0; i < sock_->sessions->count; i++)
	{
		s_sis_socket_session *session = (s_sis_socket_session *)sis_pointer_list_get(sock_->sessions, i);
		if (!session->used)
		{
			// 已经关闭就不再关闭了
			continue;
		}
		if (!uv_is_closing((uv_handle_t *)session->uvhandle))
		{
			uv_close((uv_handle_t*)session->uvhandle, cb_server_of_client_close); 
		}  
	}
	sis_pointer_list_clear(sock_->sessions);

	if (!uv_is_closing((uv_handle_t *) &sock_->server_handle))
	{
		uv_close((uv_handle_t*) &sock_->server_handle, cb_server_close);
	}
	LOG(5)("close server.\n");

	uv_stop(sock_->loop); 
	// printf("server 1 close [%p]\n", sock_->loop);
	if (sock_->server_thread_handle)
	{
		uv_thread_join(&sock_->server_thread_handle);  
	}
	// printf("server 2 close [%p]\n", sock_->loop);
	sock_->isinit = false;
}

bool _sis_socket_server_init(s_sis_socket_server *sock_)
{
	if (sock_->isinit) 
	{
		return true;
	}
	if (!sock_->loop) 
	{
		return false;
	}
	assert(0 == uv_loop_init(sock_->loop));

	int o = uv_tcp_init(sock_->loop,&sock_->server_handle);
	if (o) 
	{
		return false;
	}

	sock_->isinit = true;
	sock_->server_handle.data = sock_;

	//iret = uv_tcp_keepalive(&server_, 1, 60);//调用此函数后后续函数会调用出错
	//if (iret) {
	//    errmsg_ = GetUVError(iret);
	//    return false;
	//}
	return true;
}

bool _sis_socket_server_nodelay(s_sis_socket_server *sock_)
{
	int o = uv_tcp_nodelay(&sock_->server_handle,sock_->nodelay);
	if (o) 
	{
		return false;
	}
	return true;	
}

bool _sis_socket_server_keeplive(s_sis_socket_server *sock_)
{
	int o = uv_tcp_keepalive(&sock_->server_handle, sock_->keeplive, sock_->delay);
	if (o) 
	{
		return false;
	}
	return true;	
}
void _thread_server_run(void* arg)
{
	s_sis_socket_server *server = (s_sis_socket_server*)arg;
	LOG(5)("server thread start. [%p]\n", server);
	uv_run(server->loop, UV_RUN_DEFAULT);
	uv_loop_close(server->loop);
	LOG(5)("server thread stop. [%d]\n", server->isinit);
}
bool _sis_socket_server_run(s_sis_socket_server *sock_)
{
	int o = uv_thread_create(&sock_->server_thread_handle, _thread_server_run, sock_);
	if (o) 
	{
		return false;
	}
	return true;	
}

bool _sis_socket_server_bind(s_sis_socket_server *sock_)
{
	struct sockaddr_in bind_addr;

	int o = uv_ip4_addr(sock_->ip, sock_->port, &bind_addr);
	if (o) 
	{
		return false;
	}
	o = uv_tcp_bind(&sock_->server_handle, (const struct sockaddr*)&bind_addr, 0);
	if (o) 
	{
		return false;
	}
	LOG(5)("server bind %s:%d.\n", sock_->ip, sock_->port);
	return true;
}

bool _sis_socket_server_bind6(s_sis_socket_server *sock_)
{
	struct sockaddr_in6 bind_addr;

	int o = uv_ip6_addr(sock_->ip, sock_->port, &bind_addr);
	if (o) 
	{
		return false;
	}
	o = uv_tcp_bind(&sock_->server_handle, (const struct sockaddr*)&bind_addr, 0);
	if (o) 
	{
		return false;
	}
	LOG(5)("server bind %s:%d.\n", sock_->ip, sock_->port);
	return true;
}

//服务器-新客户端函数
static void cb_server_read_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer)
{
	if (!handle->data) 
	{
		return;
	}
	s_sis_socket_session *session = (s_sis_socket_session *)handle->data;
	if (suggested_size > session->read_buffer.len)
	{
		sis_free(session->read_buffer.base);
		session->read_buffer.base = sis_malloc(suggested_size + 1024);
		session->read_buffer.len = suggested_size + 1024;
	}
	*buffer = session->read_buffer;
}

static void cb_server_read_after(uv_stream_t *handle, ssize_t nread, const uv_buf_t* buffer)
{
	if (!handle->data) 
	{
		return;
	}
	s_sis_socket_session *session = (s_sis_socket_session *)handle->data; //服务器的recv带的是 session
	s_sis_socket_server *server = (s_sis_socket_server *)session->server;
	if (nread < 0) 
	{
		if (nread == UV_EOF) 
		{
			LOG(5)("client normal break.[%d] %s.\n", session->session_id, uv_strerror(nread)); 
		} 
		else //  if (nread == UV_ECONNRESET) 
		{
			LOG(5)("client unusual break.[%d] %s.\n", session->session_id, uv_strerror(nread)); 
		}
		sis_socket_server_delete(server, session->session_id);//连接断开，关闭客户端
		return;
	}

	if (nread > 0 && session->cb_recv_after) 
	{
		buffer->base[nread] = 0;
		session->cb_recv_after(server->source, session->session_id, buffer->base, nread);
	}
	LOG(5)("recv .%d. %p %p\n", (int)nread, session, session->cb_recv_after);
}
s_sis_socket_session *sis_socket_server_new_session(s_sis_socket_server *server_)
{
	fsec_t now = sis_time_get_now();
	int count = 0;
	for (int i = 0; i < server_->sessions->count; i++)
	{
		s_sis_socket_session *session = (s_sis_socket_session *)sis_pointer_list_get(server_->sessions, i);
		count++;
		if (session->used)
		{
			continue;
		}
		else
		{
			if (now - session->stop_time > server_->recover_delay)
			{
				sis_socket_session_init(session, session->session_id);
				session->server = server_; 
				return session;
			}
		} 
	}
	if (count > 10000)
	{
		server_->recover_delay = sis_max(server_->recover_delay / 2 , 3);
	}
	// 到这里说明没有符合条件的
	s_sis_socket_session *session = sis_socket_session_create(server_->sessions->count + 1);
	session->server = server_; //保存服务器的信息
	sis_pointer_list_push(server_->sessions, session);
	return session;	
}
static void cb_server_new_connect(uv_stream_t *handle, int status)
{
	if (!handle->data) 
	{
		return;
	}
	LOG(8)("new_connect of server = [%p]\n", handle->data);

	s_sis_socket_server *server = (s_sis_socket_server *)handle->data;

	s_sis_socket_session *session = sis_socket_server_new_session(server);

	int o = uv_tcp_init(server->loop, session->uvhandle); //析构函数释放
	if (o) 
	{
		sis_socket_session_closed(session);
		return;
	}

	o = uv_accept((uv_stream_t*)&server->server_handle, (uv_stream_t*)session->uvhandle);
	if (o) 
	{		
		uv_close((uv_handle_t*) session->uvhandle, NULL);
		sis_socket_session_closed(session);
		return;
	}

	if (server->cb_connected_s2c) 
	{
		server->cb_connected_s2c(server->source, session->session_id);
	}

	LOG(5)("new client [%p] id=%d \n", session->uvhandle, session->session_id);

	o = uv_read_start((uv_stream_t*)session->uvhandle, cb_server_read_alloc, cb_server_read_after);
	//服务器开始接收客户端的数据

	return;

}

bool _sis_socket_server_listen(s_sis_socket_server *sock_)
{
	int o = uv_listen((uv_stream_t*)&sock_->server_handle, SOMAXCONN, cb_server_new_connect);
	if (o) 
	{
		return false;
	}
	LOG(5)("server listen %s:%d.\n", sock_->ip, sock_->port);
	return true;
}

bool sis_socket_server_open(s_sis_socket_server *sock_)
{
	sis_socket_server_close(sock_);

	if (!_sis_socket_server_init(sock_)) 
	{
		LOG(5)("open server init fail.\n");
		return false;
	}

	if (!_sis_socket_server_bind(sock_)) 
	{
		LOG(5)("server bind error. %s:%d.\n", sock_->ip, sock_->port);
		return false;
	}

	if (!_sis_socket_server_listen(sock_)) 
	{
		// LOG(5)("open server listen fail.\n");
		return false;
	}
	if (!_sis_socket_server_run(sock_)) 
	{
		// LOG(5)("open server run fail.\n");
		return false;
	}
	return true;
}

bool sis_socket_server_open6(s_sis_socket_server *sock_)
{
	sis_socket_server_close(sock_);

	if (!_sis_socket_server_init(sock_)) 
	{
		return false;
	}

	if (!_sis_socket_server_bind6(sock_)) 
	{
		return false;
	}

	if (!_sis_socket_server_listen(sock_)) 
	{
		return false;
	}
	if (!_sis_socket_server_run(sock_)) 
	{
		return false;
	}
	return true;
}
static void cb_server_write_after(uv_write_t *requst, int status)
{
	if (status < 0) 
	{
		LOG(5)("server write error: %s.\n", uv_strerror(status));
	}
	s_sis_socket_session *session = (s_sis_socket_session *)requst->data;
	
	if (session->write_buffer.base) 
	{
		sis_object_decr((s_sis_object *)session->write_buffer.base);
		session->write_buffer.base = NULL;
		session->write_buffer.len = 0;
	}
	if (session->cb_send_after)
	{
		session->cb_send_after(session->server->source, session->session_id, status);
	} 
}
//服务器发送函数
bool sis_socket_server_send(s_sis_socket_server *sock_, int sid_, s_sis_object *in_)
{
	if (!in_) 
	{
		return false;
	}
	s_sis_socket_session *session = sis_pointer_list_get(sock_->sessions, sid_ - 1);
	if (!session || !session->used)
	{
		LOG(5)("can't find client %d.\n", sid_);
		return false;
	}
	//自己控制 data 的生命周期直到write结束
	sis_object_incr(in_);
	
	session->write_buffer.base = (char *)in_;
	session->write_buffer.len = SIS_OBJ_GET_SIZE(in_);

	uv_buf_t buffer = uv_buf_init(SIS_OBJ_GET_CHAR(session->write_buffer.base), session->write_buffer.len);
	session->write_req.data = session;
	int o = uv_write(&session->write_req, (uv_stream_t*)session->uvhandle, &buffer, 1, cb_server_write_after);

	if (o) 
	{
		return false;
	}
	return true;
}
 
void sis_socket_server_set_rwcb(s_sis_socket_server *sock_, int sid_, 
	cb_socket_recv_after cb_recv_, cb_socket_send_after cb_send_)
{
	s_sis_socket_session *session = sis_pointer_list_get(sock_->sessions, sid_ - 1);
	if (session)
	{
		sis_socket_session_set_rwcb(session, cb_recv_, cb_send_);
	}
	LOG(8)("set_rwcb.[%p %p]\n", session, cb_recv_);
}
//服务器-新链接回调函数
void sis_socket_server_set_cb(s_sis_socket_server *sock_, 
	cb_socket_connect cb_connected_,
	cb_socket_connect cb_disconnect_)
{
	sock_->cb_connected_s2c = cb_connected_;
	sock_->cb_disconnect_s2c = cb_disconnect_;
}
bool sis_socket_server_delete(s_sis_socket_server *sock_, int sid_)
{
	//  先发断开回调，再去关闭
	if (sock_->cb_disconnect_s2c) 
	{
		sock_->cb_disconnect_s2c(sock_->source, sid_);
	}

	s_sis_socket_session *session = sis_pointer_list_get(sock_->sessions, sid_ - 1);
	if (!session || !session->used)
	{
		LOG(5)("can't find client.[%d]\n", sid_);
		return false;
	}

	if (uv_is_active((uv_handle_t*)session->uvhandle)) 
	{
		uv_read_stop((uv_stream_t*)session->uvhandle);
	}
	uv_close((uv_handle_t*)session->uvhandle, cb_server_of_client_close);

	sis_socket_session_closed(session);

	LOG(5)("delete client.[%d] %d\n", sid_, sock_->sessions->count);

	return true;	
}


/////////////////////////////////////////////////////////////
// s_sis_socket_client define 
/////////////////////////////////////////////////////////////
void _thread_reconnect(void* arg)
{
	s_sis_socket_client *client = (s_sis_socket_client*)arg;

	sis_sleep(3000);

	LOG(5)("client reconnect thread start. [%p]\n", client);
	client->reconnect_status = SIS_UV_CONNECT_WORK;
	int count = 0;
	while (!(client->reconnect_status == SIS_UV_CONNECT_EXIT))
	{
		if (count > 25 && client->connect_status != SIS_UV_CONNECT_WORK)
		{
			sis_socket_client_open(client);
			count = 0;
		}
		count++;
		sis_sleep(200);
	}
	LOG(5)("client reconnect thread stop. [%d]\n", client->connect_status);
}

s_sis_socket_client *sis_socket_client_create()
{
	s_sis_socket_client *socket = SIS_MALLOC(s_sis_socket_client, socket); 

	socket->loop = sis_malloc(sizeof(uv_loop_t));  
	assert(0 == uv_loop_init(socket->loop));

	socket->isinit = false;
	socket->cb_recv_after = NULL;
	socket->source = socket;

	socket->cb_connected_c2s = NULL;
	socket->cb_disconnect_c2s = NULL;

	socket->delay = 5000;

	socket->read_buffer = uv_buf_init((char*)sis_malloc(MAX_NET_UV_BUFFSIZE), MAX_NET_UV_BUFFSIZE);
    socket->write_buffer = uv_buf_init(NULL, 0);

	socket->write_req.data = socket;
	socket->connect_req.data = socket;

	socket->connect_status = SIS_UV_CONNECT_NONE;

	socket->isexit = false;

	int ro = uv_thread_create(&socket->reconnect_thread_handle, _thread_reconnect, socket);
	if (ro) 
	{
		LOG(5)("create timer fail.\n");
	}

	return socket;
}
void sis_socket_client_destroy(s_sis_socket_client *client_)
{	
	if (client_->isexit)
	{
		return ;
	}
	client_->isexit = true;
	sis_socket_client_close(client_);

	client_->reconnect_status = SIS_UV_CONNECT_EXIT; 
	uv_thread_join(&client_->reconnect_thread_handle);  

	sis_free(client_->read_buffer.base);
	client_->read_buffer.base = NULL;
	client_->read_buffer.len = 0;

	if (client_->write_buffer.base)
	{
		sis_object_decr((s_sis_object *)client_->write_buffer.base);
		client_->write_buffer.base = NULL;
		client_->write_buffer.len = 0;
	}

	sis_free(client_->loop);
	sis_free(client_);
	LOG(5)("client exit .\n");
}

void cb_client_close(uv_handle_t *handle)
{
	LOG(5)("client close ok.[%p]\n", handle);
	s_sis_socket_client *client = (s_sis_socket_client*)handle->data;
	client->connect_status |= SIS_UV_CONNECT_STOP;

	if (client->cb_disconnect_c2s) 
	{
		client->cb_disconnect_c2s(client->source, 0);
	}	
}

void sis_socket_client_close(s_sis_socket_client *client_)
{
	if (!client_->isinit) 
	{
		return ;
	}
	if (client_->connect_status & SIS_UV_CONNECT_EXIT)
	{
		return ; // 已经关闭过
	}
	if (client_->connect_status == SIS_UV_CONNECT_WORK)
	{
		client_->connect_status = SIS_UV_CONNECT_EXIT;
		if (uv_is_active((uv_handle_t*)&client_->client_handle)) 
		{
			printf("uv_read_stop [%p]\n", client_->loop);
			uv_read_stop((uv_stream_t*)&client_->client_handle);
		}
		if (!uv_is_closing((uv_handle_t *)&client_->client_handle))
		{
			uv_close((uv_handle_t*)&client_->client_handle, cb_client_close);
		}
		// uv_close((uv_handle_t*) &client_->client_handle, cb_client_close);
	}
	else
	{
		client_->connect_status = SIS_UV_CONNECT_EXIT;
	}
	
	uv_stop(client_->loop); 
	uv_thread_join(&client_->connect_thread_handle);  

	client_->isinit = false;

}

static void cb_client_read_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer)
{
	if (!handle->data) 
	{
		return;
	}
	s_sis_socket_client *client = (s_sis_socket_client*)handle->data;
	if (suggested_size > client->read_buffer.len)
	{
		sis_free(client->read_buffer.base);
		client->read_buffer.base = sis_malloc(suggested_size + 1024);
		client->read_buffer.len = suggested_size + 1024;
	}
	*buffer = client->read_buffer;
}

static void cb_client_read_after(uv_stream_t *handle, ssize_t nread, const uv_buf_t* buffer)
{
	if (!handle->data) 
	{
		return;
	}
	s_sis_socket_client *client = (s_sis_socket_client*)handle->data;  // 服务器的recv带的是 s_sis_socket_client
	if (nread < 0) 
	{
		if (nread == UV_EOF) 
		{
			LOG(5)("server normal break.[%p] %s.\n", handle, uv_strerror(nread)); 
		} 
		else //  if (nread == UV_ECONNRESET) 
		{
			LOG(5)("server unusual break.[%p] %s.\n", handle, uv_strerror(nread)); 
		}
		sis_socket_client_delete(client);
		return;
	}

	if (nread > 0 && client->cb_recv_after) 
	{
		buffer->base[nread] = 0;
		client->cb_recv_after(client->source, 0, buffer->base, nread);
	}

}

static void _cb_after_connect(uv_connect_t *handle, int status)
{
	LOG(8)("after connect start.\n");
	s_sis_socket_client *client = (s_sis_socket_client *)handle->handle->data;
	if (status)
	{
		client->connect_status |= SIS_UV_CONNECT_FAIL;
		LOG(8)("connect error: %s.\n", uv_strerror(status));
		sis_socket_client_delete(client);
		return;
	}
	int o = uv_read_start(handle->handle, cb_client_read_alloc, cb_client_read_after); //客户端开始接收服务器的数据
	if (o)
	{
		LOG(5)("uv_read_start error: %s.\n", uv_strerror(status));
		client->connect_status |= SIS_UV_CONNECT_FAIL;
		sis_socket_client_delete(client);
		return;
	}
	client->connect_status = SIS_UV_CONNECT_WORK;
	if (client->cb_connected_c2s) 
	{
		client->cb_connected_c2s(client->source, 0);
	}
	LOG(8)("after connect ok.[%d]\n", client->connect_status);
}

bool _sis_socket_client_init(s_sis_socket_client *client_)
{
	if (client_->isexit) 
	{
		return false;
	}
	if (client_->isinit) 
	{
		return true;
	}
	if (!client_->loop) 
	{
		return false;
	}
	int rtn = uv_loop_init(client_->loop); 
	if (rtn)
	{
		LOG(5)("loop init fail.[%d] %s %s\n", rtn, uv_err_name(rtn), uv_strerror(rtn));
		return false;
	}

	int o = uv_tcp_init(client_->loop, &client_->client_handle);
	if (o) 
	{
		return false;
	}

	client_->isinit = true;
	client_->client_handle.data = client_;

	LOG(5)("client (%p) init type = %d\n",&client_->client_handle, client_->client_handle.type);

	//iret = uv_tcp_keepalive(&client_->client_handle, 1, 60);//调用此函数后后续函数会调用出错
	//if (iret) {
	//    errmsg_ = GetUVError(iret);
	//    return false;
	//}
	return true;
}

bool _sis_socket_client_nodelay(s_sis_socket_client *client_)
{
	int o = uv_tcp_nodelay(&client_->client_handle,client_->nodelay);
	if (o) 
	{
		return false;
	}
	return true;	
}

bool _sis_socket_client_keeplive(s_sis_socket_client *client_)
{
	int o = uv_tcp_keepalive(&client_->client_handle, client_->keeplive, client_->delay);
	if (o) 
	{
		return false;
	}
	return true;	
}

void _thread_connect(void* arg)
{
	s_sis_socket_client *client = (s_sis_socket_client*)arg;
	LOG(5)("client connect thread start. [%p]\n", client);

	struct sockaddr_in bind_addr;
	int o = uv_ip4_addr(client->ip, client->port, &bind_addr);
	if (o) 
	{
		return;
	}
	o = uv_tcp_connect(&client->connect_req, &client->client_handle, (const struct sockaddr*)&bind_addr, _cb_after_connect);
	if (o) 
	{
		return ;
	}

	// LOG(5)("client connect thread run. [%d]\n", client->connect_status);
	uv_run(client->loop, UV_RUN_DEFAULT); // (uv_run_mode)status;
	// 必须关闭 否则再次（200次）初始化时会失败
	uv_loop_close(client->loop);
	LOG(5)("client connect thread stop. [%d]\n", client->connect_status);
}
bool sis_socket_client_open(s_sis_socket_client *client_)
{
	sis_socket_client_close(client_);

	client_->connect_status = SIS_UV_CONNECT_WAIT;
	if (!_sis_socket_client_init(client_)) 
	{
		return false;
	}

	LOG(5)("client connect %s:%d.\n", client_->ip, client_->port);

	int o = uv_thread_create(&client_->connect_thread_handle, _thread_connect, client_);
	//触发 _cb_after_connect 才算真正连接成功，所以用线程
	if (o) 
	{
		return false;
	}
	LOG(5)("client status %d.\n", client_->connect_status);

	return true;
}
void _thread_connect6(void* arg)
{
	s_sis_socket_client *client = (s_sis_socket_client*)arg;
	LOG(5)("client connect thread start. [%p]\n", client);

	struct sockaddr_in6 bind_addr;
	int o = uv_ip6_addr(client->ip, client->port, &bind_addr);
	if (o) 
	{
		return;
	}
	o = uv_tcp_connect(&client->connect_req, &client->client_handle, (const struct sockaddr*)&bind_addr, _cb_after_connect);
	if (o) 
	{
		return ;
	}

	uv_run(client->loop, UV_RUN_DEFAULT); // (uv_run_mode)status;

	uv_loop_close(client->loop); 

	LOG(5)("client connect6 thread stop. [%d]\n", client->connect_status);
}
bool sis_socket_client_open6(s_sis_socket_client *client_)
{
	sis_socket_client_close(client_);
	client_->connect_status = SIS_UV_CONNECT_WAIT;
	if (!_sis_socket_client_init(client_)) 
	{
		return false;
	}

	LOG(5)("client connect %s:%d.\n", client_->ip, client_->port);
	int o = uv_thread_create(&client_->connect_thread_handle, _thread_connect6, client_);
	//触发 _cb_after_connect 才算真正连接成功，所以用线程
	if (o) 
	{
		return false;
	}
	// sis_sleep(300);
	// while (client_->connect_status == SIS_UV_CONNECT_STOP) 
	// {
	// 	sis_sleep(100);
	// }
	return true;
}

void sis_socket_client_delete(s_sis_socket_client *client_)
{
	if (client_->connect_status & SIS_UV_CONNECT_FAIL)
	{
		if (uv_is_active((uv_handle_t*)&client_->client_handle)) 
		{
			uv_read_stop((uv_stream_t*)&client_->client_handle);
		}
		// uv_close((uv_handle_t*)handle, cb_client_close);
	}
	if (!uv_is_closing((uv_handle_t *)&client_->client_handle))
	{
		uv_close((uv_handle_t*)&client_->client_handle, cb_client_close);
	}
}

void sis_socket_client_set_cb(s_sis_socket_client *client_, 
	cb_socket_connect cb_connected_,
	cb_socket_connect cb_disconnect_)
{
	client_->cb_connected_c2s = cb_connected_;
	client_->cb_disconnect_c2s = cb_disconnect_;
}

void sis_socket_client_set_rwcb(s_sis_socket_client *client_, 
	cb_socket_recv_after cb_recv_, cb_socket_send_after cb_send_)
{
	client_->cb_recv_after = cb_recv_;
	client_->cb_send_after = cb_send_;
}

static void cb_client_write_after(uv_write_t *requst, int status)
{
	s_sis_socket_client *client = (s_sis_socket_client*)requst->handle->data; 
	if (status < 0) 
	{
		LOG(5)("write error: %s.\n", uv_strerror(status));
	}
	if (client->write_buffer.base) 
	{
		printf("infs 2 = %d\n",((s_sis_object *)client->write_buffer.base)->refs);
		sis_object_decr((s_sis_object *)client->write_buffer.base);
		client->write_buffer.base = NULL;
		client->write_buffer.len = 0;
	}
	if (client->cb_send_after)
	{
		client->cb_send_after(client->source, 0, status);
	} 
}

bool sis_socket_client_send(s_sis_socket_client *client_, s_sis_object *in_)
{
	// printf("send..\n");
	if (!in_) 
	{
		return false;
	}
	sis_object_incr(in_);
	
	client_->write_buffer.base = (char *)in_;
	printf("infs 1 = %d\n",((s_sis_object *)client_->write_buffer.base)->refs);

	client_->write_buffer.len = SIS_OBJ_GET_SIZE(in_);

	uv_buf_t buffer = uv_buf_init(SIS_OBJ_GET_CHAR(client_->write_buffer.base), client_->write_buffer.len);
	// uv_async_send()
	int o = uv_write(&client_->write_req, (uv_stream_t*)&client_->client_handle, &buffer, 1, cb_client_write_after);
	if (o) 
	{
		return false;
	}
	return true;	
}


#if 0

#include <signal.h>

int __isclient = 1;
int __exit = 0;
#define  MAX_TEST_CLIENT 1

s_sis_socket_server *server = NULL;


typedef struct s_test_client {
	int sno;
	int status;
	s_sis_socket_client *client;
} s_test_client;

s_test_client *client[MAX_TEST_CLIENT];

void exithandle(int sig)
{
	printf("exit .1. \n");
	
	__exit = 1;
	if (__isclient)
	{
		for (int i = 0; i < MAX_TEST_CLIENT; i++)
		{
			if (client[i])
			{
				sis_socket_client_destroy(client[i]->client);
				sis_free(client[i]);
			}
			/* code */
		}
		
	}
	else
	{
		sis_socket_server_destroy(server);
	}
	__exit = 2;
	printf("exit .ok . \n");
}
#define ACTIVE_CLIENT 1 // client主动发请求

int sno = 0;
static void cb_server_resv_after(void *handle_, int sid_, char* in_, size_t ilen_)
{
	printf("server recv [%d]:%zu [%s]\n", sid_, ilen_, in_);	
#ifdef ACTIVE_CLIENT
	if (sno == 0)
	{	sno = 1;
		s_sis_socket_server *srv = (s_sis_socket_server *)handle_;
		s_sis_sds sds = sis_sdsempty();
		sds = sis_sdscatfmt(sds,"server recv cid =  %i", sid_);
		s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, sds);
		sis_socket_server_send(srv, sid_, obj);
		sis_object_destroy(obj);
	}
#endif
}
static void cb_client_send_after(void* handle_, int cid, int status)
{
	s_test_client *cli = (s_test_client *)handle_;
	if (cli->sno < 10*1000)
	{
		cli->sno++;
		s_sis_sds sds = sis_sdsempty();
		sds = sis_sdscatfmt(sds,"i am client. [sno = %i]. %i", cli->sno, (int)sis_time_get_now());
		s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, sds);
		sis_socket_client_send(cli->client, obj);
		sis_object_destroy(obj);
	}
}
static void cb_client_recv_after(void* handle_, int cid, char* in_, size_t ilen_)
{
	printf("client recv: cid : %d [%zu] [%s]\n", cid, ilen_, in_);	
#ifndef ACTIVE_CLIENT
	s_test_client *cli = (s_test_client *)handle_;
	sis_socket_client_send(cli->client, "client recv.",13);
#endif
}
static void cb_new_connect(void *handle_, int sid_)
{
	printf("new connect . sid_ = %d \n", sid_);	
	s_sis_socket_server *srv = (s_sis_socket_server *)handle_;
	sis_socket_server_set_rwcb(srv, sid_, cb_server_resv_after, NULL);
#ifndef ACTIVE_CLIENT
	char str[128];
	sis_sprintf(str, 128, "i am server. [cid=%d].", sid_);
	sis_socket_server_send(srv, sid_, str, strlen(str));
#endif
}

static void cb_connected(void *handle_, int sid_)
{
	printf("client connect\n");	
#ifdef ACTIVE_CLIENT
	s_test_client *cli = (s_test_client *)handle_;
	cli->status = 1;
	cli->sno = 0;
	s_sis_sds sds = sis_sdsempty();
	sds = sis_sdscatfmt(sds,"i am client. [sno = %i].", cli->sno);
	s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, sds);
	// printf("send : %s\n", sds);	
	sis_socket_client_send(cli->client, obj);
	sis_object_destroy(obj);
#endif
}
static void cb_disconnected(void *handle_, int sid_)
{
	printf("client disconnect\n");	
	s_test_client *cli = (s_test_client *)handle_;
	cli->status = 0;

}

uv_thread_t connect_thread;
int main(int argc, char **argv)
{
	safe_memory_start();

	signal(SIGINT, exithandle);

	if (argc < 2)
	{
		// it is server
		__isclient = 0;

		server = sis_socket_server_create();

		server->port = 7777;
		sis_strcpy(server->ip, 128, "0.0.0.0");

		sis_socket_server_set_cb(server, cb_new_connect, NULL);

		sis_socket_server_open(server);

		printf("server working ... \n");

	}
	else
	{
		for (int i = 0; i < MAX_TEST_CLIENT; i++)
		{
			client[i] = (s_test_client *)sis_malloc(sizeof(s_test_client));
			client[i]->client = sis_socket_client_create();
			client[i]->client->source = client[i];

			client[i]->client->port = 7777;
			sis_strcpy(client[i]->client->ip, 128, "127.0.0.1");

			sis_socket_client_set_cb(client[i]->client, cb_connected, cb_disconnected);
			sis_socket_client_set_rwcb(client[i]->client, cb_client_recv_after, cb_client_send_after);

			sis_socket_client_open(client[i]->client);

			printf("client working ... %d \n", client[i]->status);
			/* code */
		}
		

	}

	while (!__exit)
	{
		// if (__isclient)
		// {
		// 	for (int i = 0; i < MAX_TEST_CLIENT; i++)
		// 	{
		// 		if (client[i]->status  == 0)
		// 		{
		// 			sis_socket_client_open(client[i]->client);
		// 			printf("client working .%d. status %d \n", i , client[i]->status );
		// 		}
		// 	}
		// }
		sis_sleep(50);
	}
	while(__exit != 2)
	{
		sis_sleep(300);
	}	
	safe_memory_stop();
	return 0;	
}


#endif

#if 0

int __exit = 0;
s_sis_socket_client *client = NULL;

void exithandle(int sig)
{
	printf("exit .1. \n");	
	__exit = 1;
	sis_sleep(50);
	if (client)
	{
		sis_socket_client_destroy(client);
		client = NULL;
	}
	__exit = 2;
	printf("exit .ok . \n");
}
int main()
{
	safe_memory_start();

	signal(SIGINT, exithandle);

	int sno = 0;

	client = sis_socket_client_create();

	client->port = 7777;
	sis_strcpy(client->ip, 128, "127.0.0.1");

	sis_socket_client_set_cb(client, NULL, NULL);
	sis_socket_client_set_rwcb(client, NULL, NULL);

	sis_socket_client_open(client);
	
	while (!__exit)
	{
		// if (client->connect_status != SIS_UV_CONNECT_WORK)
		// {
		// 	bool rtn = sis_socket_client_open(client);
		// 	printf("client reopen . %d %d\n", sno++, rtn);
		// }
		sis_sleep(100);
	}
	printf("client close.\n");
	while(__exit != 2)
	{
		sis_sleep(300);
	}	
	safe_memory_stop();
	return 0;
}
#endif

#if 0

#include <signal.h>
uv_loop_t *__loop;
int __isclient = 1;
int __exit = 0;
#define  MAX_TEST_CLIENT = 10;
uv_thread_t thandle;

void exithandle(int sig)
{
	printf("exit .1. \n");
	
	uv_stop(__loop);
	printf("exit .ok . \n");

	uv_thread_join(&thandle);  

	__exit = 1;
}

int64_t counter = 0;

void wait_for_a_while(uv_idle_t* handle) {
    counter++;

    if (counter >= 1000000)
    {
		uv_idle_stop(handle);
	}
	if (counter%1000 == 0)
	{
		printf("Idling... %d\n", counter);
		sis_sleep(10);
	}
}

void _thread_run(void* arg)
{
	uv_run(__loop, UV_RUN_DEFAULT);
	
	printf("thread end . \n");

	uv_loop_close(__loop);
	sis_free(__loop);
}

int main() {
	signal(SIGINT, exithandle);

	__loop = sis_malloc(sizeof(uv_loop_t));  
	assert(0 == uv_loop_init(__loop));

    uv_idle_t idler;

    uv_idle_init(__loop, &idler);
    uv_idle_start(&idler, wait_for_a_while);

    printf("Idling...\n");
    // uv_run(__loop, UV_RUN_DEFAULT);
	uv_thread_create(&thandle, _thread_run, NULL);

	while (!__exit)
	{
		sis_sleep(300);
	}

	printf("close . \n");
    return 0;
}
#endif

