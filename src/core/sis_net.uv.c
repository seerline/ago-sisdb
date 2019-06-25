
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
 
s_sis_socket_server *sis_socket_server_create()
{
	s_sis_socket_server *socket = SIS_MALLOC(s_sis_socket_server, socket); 

	socket->loop = sis_malloc(sizeof(uv_loop_t));  
	assert(0 == uv_loop_init(socket->loop));

	socket->no = 0;
	socket->isinit = false;
	socket->data = NULL;
	socket->cb_connect_s2c = NULL;
	socket->cb_disconnect_s2c = NULL;

	socket->clients = sis_map_pointer_create();	
	assert(0 == uv_mutex_init(&socket->mutex_clients));
	
	LOG(8)("server create.[%p]\n", socket);
	return socket; 
}
void sis_socket_server_destroy(s_sis_socket_server *sock_)
{
	sis_socket_server_close(sock_);
	LOG(5)("server exit.\n");
	uv_mutex_destroy(&sock_->mutex_clients);
	sis_map_pointer_destroy(sock_->clients);
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
	LOG(8)("session free.[%p]\n", session);
	sis_socket_session_destroy(session);
	LOG(5)("server of client close ok.[%p]\n", handle);
}

void sis_socket_server_close(s_sis_socket_server *sock_)
{ 
	if (!sock_->isinit) 
	{
		return ;
	}
	LOG(5)("clear clients.[%d]\n", sis_map_pointer_getsize(sock_->clients));
	if (sis_map_pointer_getsize(sock_->clients) > 0)
	{
		s_sis_dict_entry *de;
		s_sis_dict_iter *di = sis_dict_get_iter(sock_->clients);
		while ((de = sis_dict_next(di)) != NULL)
		{
			s_sis_socket_session *session = (s_sis_socket_session *)sis_dict_getval(de);
			if (!uv_is_closing((uv_handle_t *)session->client_handle))
			{
				uv_close((uv_handle_t*)session->client_handle, cb_server_of_client_close); 
			}       
		}
		sis_dict_iter_free(di);
		sis_map_pointer_clear(sock_->clients);
	}
	if (!uv_is_closing((uv_handle_t *) &sock_->server))
	{
		uv_close((uv_handle_t*) &sock_->server, cb_server_close);
	}
	LOG(5)("close server.\n");

	uv_stop(sock_->loop); 
	printf("server 1 close [%p]\n", sock_->loop);
	uv_thread_join(&sock_->server_thread_hanlde);  
	printf("server 2 close [%p]\n", sock_->loop);
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

	int o = uv_tcp_init(sock_->loop,&sock_->server);
	if (o) 
	{
		return false;
	}

	sock_->isinit = true;
	sock_->server.data = sock_;

	//iret = uv_tcp_keepalive(&server_, 1, 60);//调用此函数后后续函数会调用出错
	//if (iret) {
	//    errmsg_ = GetUVError(iret);
	//    return false;
	//}
	return true;
}

bool _sis_socket_server_nodelay(s_sis_socket_server *sock_)
{
	int o = uv_tcp_nodelay(&sock_->server,sock_->nodelay);
	if (o) 
	{
		return false;
	}
	return true;	
}

bool _sis_socket_server_keeplive(s_sis_socket_server *sock_)
{
	int o = uv_tcp_keepalive(&sock_->server, sock_->keeplive, sock_->delay);
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
	LOG(5)("server thread stop. [%d]\n", server->isinit);
}
bool _sis_socket_server_run(s_sis_socket_server *sock_)
{
	LOG(5)("server runing...\n");
	int o = uv_thread_create(&sock_->server_thread_hanlde, _thread_server_run, sock_);
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
	o = uv_tcp_bind(&sock_->server, (const struct sockaddr*)&bind_addr, 0);
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
	o = uv_tcp_bind(&sock_->server, (const struct sockaddr*)&bind_addr, 0);
	if (o) 
	{
		return false;
	}
	LOG(5)("server bind %s:%d.\n", sock_->ip, sock_->port);
	return true;
}

//     //服务器-新客户端函数

static void cb_server_read_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer)
{
	if (!handle->data) 
	{
		return;
	}
	s_sis_socket_session *session = (s_sis_socket_session *)handle->data;
	*buffer = session->read_buffer;
}

static void cb_server_read_after(uv_stream_t *handle, ssize_t nread, const uv_buf_t* buffer)
{
	LOG(5)("recv ..\n");
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
			LOG(5)("client normal break.[%d] %s.\n", session->client_id, uv_strerror(nread)); 
		} 
		else //  if (nread == UV_ECONNRESET) 
		{
			LOG(5)("client unusual break.[%d] %s.\n", session->client_id, uv_strerror(nread)); 
		}
		sis_socket_server_delete(server, session->client_id);//连接断开，关闭客户端
		return;
	}

	if (nread > 0 && session->cb_recv) 
	{
		buffer->base[nread] = 0;
		session->cb_recv(server->data, session->client_id, buffer->base, nread);
	}
	LOG(5)("recv .%d. %p %p\n", nread, session, session->cb_recv);
}

static void cb_server_new_connect(uv_stream_t *handle, int status)
{
	if (!handle->data) 
	{
		return;
	}
	LOG(8)("new_connect of server = [%p]\n", handle->data);

	s_sis_socket_server *server = (s_sis_socket_server *)handle->data;

	server->no++;
	server->no = sis_max(1, server->no);

	int cid = server->no;

	s_sis_socket_session *session = sis_socket_session_create(cid); //uv_close回调函数中释放

	session->server = server; //保存服务器的信息

	int o = uv_tcp_init(server->loop, session->client_handle); //析构函数释放
	if (o) 
	{
		sis_socket_session_destroy(session);
		return;
	}

	o = uv_accept((uv_stream_t*)&server->server, (uv_stream_t*)session->client_handle);
	if (o) 
	{
		
		uv_close((uv_handle_t*) session->client_handle, NULL);
		sis_socket_session_destroy(session);
		return;
	}

	char key[16];
	sis_sprintf(key, 16, "%d", cid);

	sis_map_pointer_set(server->clients, key, session);

	if (server->cb_connect_s2c) 
	{
		server->cb_connect_s2c(server->data, cid);
	}
	// printf("len = %d %d %d %d %d %d\n",sizeof(uv_stream_t),sizeof(uv_handle_t),sizeof(uv_tcp_t),
	// 	sizeof(uv_write_t),sizeof(uv_connect_t),sizeof(uv_loop_t));

	LOG(5)("new client [%p] id=%d \n", session->client_handle, cid);

	o = uv_read_start((uv_stream_t*)session->client_handle, cb_server_read_alloc, cb_server_read_after);
	//服务器开始接收客户端的数据

	return;

}

bool _sis_socket_server_listen(s_sis_socket_server *sock_)
{
	int o = uv_listen((uv_stream_t*)&sock_->server, SOMAXCONN, cb_server_new_connect);
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
		return false;
	}

	if (!_sis_socket_server_bind(sock_)) 
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
}
//服务器发送函数
bool sis_socket_server_send(s_sis_socket_server *sock_, int cid_, const char* in_, size_t ilen_)
{
	if (!in_ || ilen_ <= 0) 
	{
		return false;
	}
	char key[16];
	sis_sprintf(key, 16, "%d", cid_);
	s_sis_socket_session *session = sis_map_pointer_get(sock_->clients, key);
	if (!session)
	{
		LOG(5)("can't find client %d.\n", cid_);
		return false;
	}
	//自己控制 data 的生命周期直到write结束
	if (session->write_buffer.len < ilen_) 
	{
		sis_free(session->write_buffer.base);
		session->write_buffer.base = (char*)sis_malloc(ilen_);
		session->write_buffer.len = ilen_;
	}
	memmove(session->write_buffer.base, in_, ilen_);
	uv_buf_t buffer = uv_buf_init((char*)session->write_buffer.base, ilen_);
	int o = uv_write(&session->write_req, (uv_stream_t*)session->client_handle, &buffer, 1, cb_server_write_after);

	if (o) 
	{
		return false;
	}
	return true;
}
 
void sis_socket_server_set_recv_cb(s_sis_socket_server *sock_, int cid_, callback_socket_recv cb_)
{
	char key[16];
	sis_sprintf(key, 16, "%d", cid_);
	s_sis_socket_session *session = sis_map_pointer_get(sock_->clients, key);
	if (session)
	{
		session->cb_recv = cb_;
	}
	LOG(8)("set_recv_cb.[%p %p]\n", session, cb_);
}
//服务器-新链接回调函数
void sis_socket_server_set_cb(s_sis_socket_server *sock_, 
	callback_connect_s2c connect_cb_,
	callback_connect_s2c disconnect_cb_)
{
	sock_->cb_connect_s2c = connect_cb_;
	sock_->cb_disconnect_s2c = disconnect_cb_;
}
bool sis_socket_server_delete(s_sis_socket_server *sock_, int cid_)
{
	uv_mutex_lock(&sock_->mutex_clients);

	//  先发断开回调，再去关闭
	if (sock_->cb_disconnect_s2c) 
	{
		sock_->cb_disconnect_s2c(sock_->data, cid_);
	}

	char key[16];
	sis_sprintf(key, 16, "%d", cid_);
	s_sis_socket_session *session = sis_map_pointer_get(sock_->clients, key);
	if (!session)
	{
		LOG(5)("can't find client.[%d]\n", cid_);
		uv_mutex_unlock(&sock_->mutex_clients);
		return false;
	}

	if (uv_is_active((uv_handle_t*)session->client_handle)) 
	{
		uv_read_stop((uv_stream_t*)session->client_handle);
	}
	uv_close((uv_handle_t*)session->client_handle, cb_server_of_client_close);

	sis_map_pointer_del(sock_->clients, key);
	// sis_socket_session_destroy(session);

	LOG(5)("delete client.[%d] %d\n", cid_, sis_map_pointer_getsize(sock_->clients));

	uv_mutex_unlock(&sock_->mutex_clients);

	return true;	
}


/////////////////////////////////////////////////////////////
// s_sis_socket_client define 
/////////////////////////////////////////////////////////////

s_sis_socket_client *sis_socket_client_create()
{
	s_sis_socket_client *socket = SIS_MALLOC(s_sis_socket_client, socket); 

	socket->loop = sis_malloc(sizeof(uv_loop_t));  
	assert(0 == uv_loop_init(socket->loop));

	socket->isinit = false;
	socket->cb_recv = NULL;
	socket->data = NULL;

	socket->cb_connect_c2s = NULL;
	socket->cb_disconnect_c2s = NULL;

	socket->delay = 5000;

	socket->status = SIS_UV_CONNECT_STOP;

	socket->read_buffer = uv_buf_init((char*)sis_malloc(MAX_NET_UV_BUFFSIZE), MAX_NET_UV_BUFFSIZE);
    socket->write_buffer = uv_buf_init((char*)sis_malloc(MAX_NET_UV_BUFFSIZE), MAX_NET_UV_BUFFSIZE);

	socket->write_req.data = socket;
	socket->connect_req.data = socket;

	assert(0 == uv_mutex_init(&socket->mutex_write));
	
	socket->isexit = false;

	return socket;
}
void sis_socket_client_destroy(s_sis_socket_client *sock_)
{	
	if (sock_->isexit)
	{
		return ;
	}
	sock_->isexit = true;

	sis_free(sock_->read_buffer.base);
	sock_->read_buffer.base = NULL;
	sock_->read_buffer.len = 0;

	sis_free(sock_->write_buffer.base);
	sock_->write_buffer.base = NULL;
	sock_->write_buffer.len = 0;

	sis_socket_client_close(sock_);

	uv_mutex_destroy(&sock_->mutex_write);
	// uv_loop_close(sock_->loop); 
	sis_free(sock_->loop);
	sis_free(sock_);
	LOG(5)("client exit .\n");
}

void cb_client_close(uv_handle_t *handle)
{
	LOG(5)("client close ok.[%p]\n", handle);
	s_sis_socket_client *client = (s_sis_socket_client*)handle->data;
	client->status = SIS_UV_CONNECT_STOP;

	if (client->cb_disconnect_c2s) 
	{
		client->cb_disconnect_c2s(client->data);
	}	
}

void sis_socket_client_close(s_sis_socket_client *sock_)
{
	if (!sock_->isinit) 
	{
		return ;
	}
	// 关闭时间片
	// uv_timer_stop(&sock_->client_timer);
	// uv_close((uv_handle_t*)&sock_->client_timer, NULL);

	if (sock_->status == SIS_UV_CONNECT_FINISH)
	{
		if (uv_is_active((uv_handle_t*)&sock_->client_handle)) 
		{
			printf("uv_read_stop [%p]\n", sock_->loop);
			uv_read_stop((uv_stream_t*)&sock_->client_handle);
		}
		uv_close((uv_handle_t*) &sock_->client_handle, cb_client_close);
	}

	// printf("client close [%p]\n", sock_->loop);
	uv_stop(sock_->loop); 
	// printf("client 1 close [%p]\n", sock_->loop);
	uv_thread_join(&sock_->connect_thread_hanlde);  
	// printf("client 2 close [%p]\n", sock_->loop);
	sock_->isinit = false;
}

static void cb_client_read_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer)
{
	if (!handle->data) 
	{
		return;
	}
	s_sis_socket_client *client = (s_sis_socket_client*)handle->data;
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

	if (nread > 0 && client->cb_recv) 
	{
		buffer->base[nread] = 0;
		client->cb_recv(client->data, 0, buffer->base, nread);
	}

}

static void _cb_after_connect(uv_connect_t *handle, int status)
{
	LOG(8)("after connect start.\n");
	s_sis_socket_client *client = (s_sis_socket_client *)handle->handle->data;
	if (status)
	{
		client->status = SIS_UV_CONNECT_ERROR;
		LOG(8)("connect error: %s.\n", uv_strerror(status));
		sis_socket_client_delete(client);
		return;
	}
	int o = uv_read_start(handle->handle, cb_client_read_alloc, cb_client_read_after); //客户端开始接收服务器的数据
	if (o)
	{
		LOG(5)("uv_read_start error: %s.\n", uv_strerror(status));
		client->status = SIS_UV_CONNECT_ERROR;
		sis_socket_client_delete(client);
		return;
	}
	client->status = SIS_UV_CONNECT_FINISH;
	if (client->cb_connect_c2s) 
	{
		client->cb_connect_c2s(client->data);
	}
	LOG(8)("after connect ok.[%d]\n", client->status);
}
// void cb_client_timer(uv_timer_t* handle)
// {
// 	s_sis_socket_client *client = (s_sis_socket_client*)handle->data;
// 	// printf("timer... %d \n", client->status);
// 	if (client->status == SIS_UV_CONNECT_STOP)
// 	{
// 	}
// 	// 这里做定时处理，基本没什么用
// }
bool _sis_socket_client_init(s_sis_socket_client *sock_)
{
	if (sock_->isexit) 
	{
		return false;
	}
	if (sock_->isinit) 
	{
		return true;
	}

	if (!sock_->loop) 
	{
		return false;
	}

	assert(0 == uv_loop_init(sock_->loop));

	int o = uv_tcp_init(sock_->loop, &sock_->client_handle);
	if (o) 
	{
		return false;
	}

	sock_->isinit = true;
	sock_->client_handle.data = sock_;
	sock_->client_timer.data = sock_;

	LOG(5)("client (%p) init type = %d\n",&sock_->client_handle,sock_->client_handle.type);

	// uv_timer_init(sock_->loop, &sock_->client_timer);
	// uv_timer_start(&sock_->client_timer, cb_client_timer, 1000, 5000);

	//iret = uv_tcp_keepalive(&sock_->client_handle, 1, 60);//调用此函数后后续函数会调用出错
	//if (iret) {
	//    errmsg_ = GetUVError(iret);
	//    return false;
	//}
	return true;
}

bool _sis_socket_client_nodelay(s_sis_socket_client *sock_)
{
	int o = uv_tcp_nodelay(&sock_->client_handle,sock_->nodelay);
	if (o) 
	{
		return false;
	}
	return true;	
}

bool _sis_socket_client_keeplive(s_sis_socket_client *sock_)
{
	int o = uv_tcp_keepalive(&sock_->client_handle, sock_->keeplive, sock_->delay);
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

	uv_run(client->loop, UV_RUN_DEFAULT); // (uv_run_mode)status;

	uv_loop_close(client->loop); 

	// client->status = SIS_UV_CONNECT_WAIT;
	LOG(5)("client connect thread stop. [%d]\n", client->status);
}
bool sis_socket_client_open(s_sis_socket_client *sock_)
{
	sis_socket_client_close(sock_);

	if (!_sis_socket_client_init(sock_)) 
	{
		return false;
	}

	LOG(5)("client connect %s:%d.\n", sock_->ip, sock_->port);

	int o = uv_thread_create(&sock_->connect_thread_hanlde, _thread_connect, sock_);
	//触发 _cb_after_connect 才算真正连接成功，所以用线程
	if (o) 
	{
		return false;
	}
	// while (sock_->status == SIS_UV_CONNECT_STOP) 
	// {
	// 	sis_sleep(100);
	// }
	sis_sleep(300);
	LOG(5)("client status %d.\n", sock_->status);

	return sock_->status == SIS_UV_CONNECT_FINISH;
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

	// uv_loop_close(client->loop); 

	LOG(5)("client connect6 thread stop. [%d]\n", client->status);
}
bool sis_socket_client_open6(s_sis_socket_client *sock_)
{
	sis_socket_client_close(sock_);

	if (!_sis_socket_client_init(sock_)) 
	{
		return false;
	}

	LOG(5)("client connect %s:%d.\n", sock_->ip, sock_->port);
	int o = uv_thread_create(&sock_->connect_thread_hanlde, _thread_connect6, sock_);
	//触发 _cb_after_connect 才算真正连接成功，所以用线程
	if (o) 
	{
		return false;
	}
	sis_sleep(300);
	// while (sock_->status == SIS_UV_CONNECT_STOP) 
	// {
	// 	sis_sleep(100);
	// }
	return sock_->status == SIS_UV_CONNECT_FINISH;
}

void sis_socket_client_delete(s_sis_socket_client *sock_)
{
	if (sock_->status == SIS_UV_CONNECT_ERROR)
	{
		if (uv_is_active((uv_handle_t*)&sock_->client_handle)) 
		{
			uv_read_stop((uv_stream_t*)&sock_->client_handle);
		}
		// uv_close((uv_handle_t*)handle, cb_client_close);
	}
	if (!uv_is_closing((uv_handle_t *)&sock_->client_handle))
	{
		uv_close((uv_handle_t*)&sock_->client_handle, cb_client_close);
	}
}

void sis_socket_client_set_cb(s_sis_socket_client *sock_, 
	callback_connect_c2s connect_cb_,
	callback_connect_c2s disconnect_cb_)
{
	sock_->cb_connect_c2s = connect_cb_;
	sock_->cb_disconnect_c2s = disconnect_cb_;
}

void sis_socket_client_set_recv_cb(s_sis_socket_client *sock_, callback_socket_recv cb_)
{
	sock_->cb_recv = cb_;
}

static void cb_client_write_after(uv_write_t *requst, int status)
{
	s_sis_socket_client *client = (s_sis_socket_client*)requst->handle->data; 
	uv_mutex_unlock(&client->mutex_write);
	if (status < 0) 
	{
		LOG(5)("write error: %s.\n", uv_strerror(status));
	}
}

bool sis_socket_client_send(s_sis_socket_client *sock_, const char* in_, size_t ilen_)
{
	printf("send..\n");
	if (!in_ || ilen_ <= 0) 
	{
		return false;
	}
	uv_mutex_lock(&sock_->mutex_write);
	if (sock_->write_buffer.len < ilen_) 
	{
		sis_free(sock_->write_buffer.base);
		sock_->write_buffer.base = (char*)sis_malloc(ilen_);
		sock_->write_buffer.len = ilen_;
	}
	memmove(sock_->write_buffer.base, in_, ilen_);
	uv_buf_t buffer = uv_buf_init((char*)sock_->write_buffer.base, ilen_);

	// uv_async_send()
	int o = uv_write(&sock_->write_req, (uv_stream_t*)&sock_->client_handle, &buffer, 1, cb_client_write_after);

	if (o) 
	{
		uv_mutex_unlock(&sock_->mutex_write);
		return false;
	}
	printf("send.1.\n");
	return true;	
}

bool sis_socket_send(void *sock_, int cid_, const char* in_, size_t ilen_)
{
	if (cid_ == 0)
	{
		s_sis_socket_client *sock = (s_sis_socket_client *)sock_;
		return  sis_socket_client_send(sock, in_, ilen_);
	}
	s_sis_socket_server *sock = (s_sis_socket_server *)sock_;
	return sis_socket_server_send(sock, cid_, in_, ilen_);
}


#if 0

#include <signal.h>

int __isclient = 1;
int __exit = 0;
#define  MAX_TEST_CLIENT = 10;
int status = SIS_UV_CONNECT_STOP;

s_sis_socket_server *server = NULL;
s_sis_socket_client *client = NULL;

void exithandle(int sig)
{
	printf("exit .1. \n");
	
	if (__isclient)
	{
		if (client)
		{
			sis_socket_client_close(client);		
			sis_socket_client_destroy(client);

		}
	}
	else
	{
		sis_socket_server_close(server);		
		sis_socket_server_destroy(server);
	}
	printf("exit .ok . \n");
	__exit = 1;
}
#define ACTIVE_CLIENT 1 // client主动发请求

static void cb_server_recv(void *handle_, int cid_, const char* in_, size_t ilen_)
{
	printf("server recv [%d]:%zu [%s]\n", cid_, ilen_, in_);	
#ifdef ACTIVE_CLIENT
	char str[128];
	sis_sprintf(str, 128, "server recv [cid=%d].", cid_);
	sis_socket_server_send(server, cid_, str, strlen(str));
#endif
}
static void cb_client_recv(void* handle_, int cid, const char* in_, size_t ilen_)
{
	printf("client recv[%zu] [%s]\n", ilen_, in_);	
#ifndef ACTIVE_CLIENT
	sis_socket_client_send(client, "client recv.",13);
#endif
}
static void cb_new_connect(void *handle_, int cid_)
{
	printf("new connect\n");	
	sis_socket_server_set_recv_cb(server, cid_, cb_server_recv);
#ifndef ACTIVE_CLIENT
	char str[128];
	sis_sprintf(str, 128, "i am server. [cid=%d].", cid_);
	sis_socket_server_send(server, cid_, str, strlen(str));
#endif
}

static void cb_connected(void *handle_)
{
	printf("client connect\n");	
#ifdef ACTIVE_CLIENT
	sis_socket_client_send(client, "i am client.",13);
#endif
}
static void cb_disconnected(void *handle_)
{
	printf("client disconnect\n");	
	status = SIS_UV_CONNECT_WAIT;

}
// void cb_reconnect(void *argv)
// {
// 	while()
// }
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
		client = sis_socket_client_create();

		// uv_thread_create(&connect_thread, cb_reconnect, client);

		client->port = 7777;
		sis_strcpy(client->ip, 128, "127.0.0.1");

		sis_socket_client_set_cb(client, cb_connected, cb_disconnected);
		sis_socket_client_set_recv_cb(client, cb_client_recv);

		sis_socket_client_open(client);

		printf("client working ... %d \n", client->status == SIS_UV_CONNECT_FINISH);

	}

	while (!__exit)
	{
		sis_sleep(3000);
		if (status == SIS_UV_CONNECT_WAIT)
		{
			status = SIS_UV_CONNECT_STOP;
			sis_socket_client_close(client);		
			sis_socket_client_open(client);
			printf("client working .1. %d \n", client->status == SIS_UV_CONNECT_FINISH);
		}
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


#if 0

#include <signal.h>
uv_loop_t *__loop;
int __isclient = 1;
int __exit = 0;
#define  MAX_TEST_CLIENT = 10;
uv_thread_t thread_handle;
uv_connect_t connect_req;
uv_tcp_t client_handle;
uv_timer_t client_timer;

void on_close(uv_handle_t* handle)
{
	printf("%p close \n", handle);
	uv_stop(__loop);
}
void on_shutdown(uv_shutdown_t* req, int status)
{
	uv_close((uv_handle_t *)&client_handle,on_close);
    free(req);
}

void exithandle(int sig)
{
	printf("exit .1. \n");
	
	// uv_stop(__loop);
	// // uv_timer_stop(&client_timer);
	// // uv_close((uv_handle_t*)&client_timer, NULL);

	// // uv_close((uv_handle_t *)&client_handle,on_close);
	// // sis_sleep(100);

	// iov = uv_buf_init("close", 5);
	// uv_write(&write_req, (uv_stream_t*)&client_handle, &iov, 1, after_write);
	// uv_read_stop(readhandle);
	uv_read_stop((uv_stream_t *)&client_handle);
	// // uv_shutdown_t* req = (uv_shutdown_t*)malloc(sizeof(uv_shutdown_t));
	// // req->handle = (uv_stream_t *)&client_handle;
	// // uv_shutdown(req, (uv_stream_t *)&client_handle, on_shutdown);

	if (!uv_is_closing((uv_handle_t *)&client_handle))
	{
		uv_close((uv_handle_t *)&client_handle,NULL);
	}
	
	uv_stop(__loop);

	printf("exit .wait. \n");

	uv_thread_join(&thread_handle);  

	__exit = 1;

	printf("exit .ok . \n");

}
static void cb_read_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer)
{
	printf("cb_client_read_alloc.\n");
}

static void cb_read_after(uv_stream_t *handle, ssize_t nread, const uv_buf_t* buffer)
{
	printf("cb_read_after.[%d]\n", nread);
}
static void _cb_connect(uv_connect_t *handle, int status)
{
	printf("after connect start. %p - %p\n", handle->handle, &client_handle);

	int o = uv_read_start((uv_stream_t *)&client_handle, cb_read_alloc, cb_read_after); //客户端开始接收服务器的数据
	if (o)
	{
		// uv_read_start 如何停止服务以便于退出
		LOG(5)("uv_read_start error: %s.\n", uv_strerror(status));
		return;
	}

}

void _thread_run(void* arg)
{

	printf("thread ok.\n");
	uv_run(__loop, UV_RUN_DEFAULT);
	printf("thread end.\n");

	uv_loop_close(__loop);
	sis_free(__loop);
}

int main() {
	signal(SIGINT, exithandle);

	__loop = sis_malloc(sizeof(uv_loop_t));  
	assert(0 == uv_loop_init(__loop));

    // uv_run(__loop, UV_RUN_DEFAULT);
	// _thread_run(NULL);
	struct sockaddr_in bind_addr;
	int o = uv_ip4_addr("127.0.0.1", 7777, &bind_addr);
	if (o) 
	{
		printf("ip error.[%d]\n", o);
		return 0;
	}
	o = uv_tcp_init(__loop,&client_handle);
	if (o) 
	{
		printf("uv_tcp_init error.[%d]\n", o);
		return 0;
	}
	o = uv_tcp_connect(&connect_req, &client_handle, (const struct sockaddr*)&bind_addr, _cb_connect);
	if (o) 
	{
		printf("connect error.[%d]\n", o);
		return 0;
	}
	// uv_timer_init(__loop, &client_timer);
	// uv_timer_start(&client_timer, NULL, 100, 0);
 
	uv_thread_create(&thread_handle, _thread_run, NULL);

	while (!__exit)
	{
		sis_sleep(300);
	}

	printf("close . \n");
    return 0;
}
#endif