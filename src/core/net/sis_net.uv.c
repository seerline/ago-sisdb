
#include <sis_net.uv.h>
#include <sis_math.h>

// ***************************************************** //
// 实在没搞懂libuv的运行机制 明明已经 close 了所有handle
// 执行 uv_stop 后仍然不能保证从 uv_run 中优雅的退出
// 只能在timer.c中的 uv__next_timeout 函数中设置结点为空时的超时为20毫秒
// 原始为 -1 (永远等待) 暂时解决这个问题, 等以后 libuv 升级后看看有没有其他解决方案
// 理想状态是 uv_stop 后系统自动清理所有未完成的工作 然后返回 
// ***************************************************** //

// 下面的函数可以清理所有的未释放的句柄
// typedef void *QUEUE[2];
// #define QUEUE_NEXT(q)       (*(QUEUE **) &((*(q))[0]))
// #define QUEUE_DATA(ptr, type, field)  ((type *) ((char *) (ptr) - offsetof(type, field)))
// #define QUEUE_FOREACH(q, h)  for ((q) = QUEUE_NEXT(h); (q) != (h); (q) = QUEUE_NEXT(q))

// void uv_clear_active_handles(uv_loop_t *loop)
// {
// 	if (loop == NULL || (int)loop->active_handles == -1)
// 	{
// 		return;
// 	}
// 	QUEUE *list;
// 	uv_handle_t *handle;
// 	QUEUE_FOREACH(list, &loop->handle_queue)
// 	{
// 		handle = QUEUE_DATA(list, uv_handle_t, handle_queue);
// 		if (uv_is_active(handle))
// 		{
// 			printf("flags : %x type :%d %p\n",
// 				   handle->flags, handle->type, (void *)handle);
// 			uv_read_stop((uv_stream_t *)handle);
// 			uv_close(handle, NULL);
// 		}
// 	}
// }
// --- SIGPIPE 需要忽略这个信号 ----
// 在多CPU测试 经常会剩余几个包发不出去 这个可能和 wait_queue 有关 避免问题 做一个裸的测试
#define SIS_UV_WRITE_WAIT  5000

//////////////////////////////////////////////////////////////
// s_sis_socket_session
//////////////////////////////////////////////////////////////

s_sis_socket_session *sis_socket_session_create(void *father_)
{
	s_sis_socket_session *session = SIS_MALLOC(s_sis_socket_session, session); 
	session->sid = -1;
	session->father = father_;
	session->uv_w_handle.data = session;
	session->uv_r_buffer = uv_buf_init((char*)sis_malloc(MAX_NET_UV_BUFFSIZE), MAX_NET_UV_BUFFSIZE);
	session->send_nodes = sis_net_nodes_create();
	session->sendnums = 1 * 1024;
	session->sendbuff = sis_malloc(sizeof(uv_buf_t) * session->sendnums);
	memset(session->sendbuff, 0, sizeof(uv_buf_t) * session->sendnums);
	return session;	
}
void sis_socket_session_set_rwcb(s_sis_socket_session *session_,
								cb_socket_recv_after cb_recv_, 
								cb_socket_send_after cb_send_)
{
	session_->cb_recv_after = cb_recv_;
	session_->cb_send_after = cb_send_;
}

void sis_socket_session_destroy(void *session_, int mode_)
{
	s_sis_socket_session *session = (s_sis_socket_session *)session_;
	if (mode_ == 0)
	{
		if (session->send_nodes)
		{
			sis_net_nodes_destroy(session->send_nodes);
			session->send_nodes = NULL;
		}	
		// printf("==0==free %d\n",session->sid);	
	}
	else
	{
		if (session->uv_r_buffer.base)
		{
			sis_free(session->uv_r_buffer.base);
		}
		session->uv_r_buffer.base = NULL;
		session->uv_r_buffer.len = 0;
		sis_free(session->sendbuff);
		if (session->send_nodes)
		{
			sis_net_nodes_destroy(session->send_nodes);
		}
		// printf("==1==free %d\n",session->sid);
		sis_free(session);
	}
}

void sis_socket_session_init(s_sis_socket_session *session_)
{
	session_->sid = -1;
	memset(&session_->uv_w_handle, 0, sizeof(uv_tcp_t));
	session_->uv_w_handle.data = session_;
	sis_net_nodes_clear(session_->send_nodes);
	memset(session_->sendbuff, 0, sizeof(uv_buf_t) * session_->sendnums);
}

/////////////////////////////////////////////////
//  
/////////////////////////////////////////////////
int sis_socket_close_handle(uv_handle_t *handle_, uv_close_cb close_cb_)
{
	if (uv_is_active(handle_))
	{
		uv_read_stop((uv_stream_t*)handle_);
	}
	int o = 1;
	if (!uv_is_closing(handle_))
	{
		uv_close(handle_, close_cb_);
	}
	else
	{
		o = 0;
	}
	return o;
	// printf("close %p : type: %d closing: %lx\n", handle_, handle_->type, handle_->flags);// & UV_HANDLE_CLOSING);
} 
// static void _cb_clear_walk(uv_handle_t* handle, void* arg)
// {
// 	if (!uv_is_closing(handle))
// 	{
// 		uv_close(handle, NULL);
// 	}
// }
static void _uv_exit_loop(uv_loop_t* loop) 
{
	// uv_walk(loop, _cb_clear_walk, NULL);
	// uv_run(loop, UV_RUN_DEFAULT);
	uv_loop_close(loop);
	uv_library_shutdown();      
}

//////////////////////////////////////////////////////////////
// s_sis_socket_server
//////////////////////////////////////////////////////////////
s_sis_socket_server *sis_socket_server_create()
{
	s_sis_socket_server *server = SIS_MALLOC(s_sis_socket_server, server); 

	server->uv_s_worker = sis_malloc(sizeof(uv_loop_t));  

	server->isinit = false;
	server->cb_source = server;
	server->cb_connected_s2c = NULL;
	server->cb_disconnect_s2c = NULL;

	server->sessions = sis_net_list_create(sis_socket_session_destroy);	
	server->sessions->wait_sec = 180; // 180 秒后释放资源

	server->isexit = false;
	sis_mutex_create(&server->write_may_lock);

	// LOG(8)("server create.[%p]\n", server);
	return server; 
}
void sis_socket_server_destroy(s_sis_socket_server *server_)
{
	if (server_->isexit)
	{
		return ;
	}
	server_->isexit = true;
	sis_socket_server_close(server_);
	sis_net_list_destroy(server_->sessions);
	sis_mutex_destroy(&server_->write_may_lock);
	uv_loop_close(server_->uv_s_worker); 	
	sis_free(server_->uv_s_worker);
	sis_free(server_);
	LOG(5)("server exit.\n");
}
static void cb_session_closed(uv_handle_t *handle)
{
	s_sis_socket_session *session = (s_sis_socket_session *)handle->data;
	s_sis_socket_server *server = (s_sis_socket_server *)session->father;
	sis_net_list_stop(server->sessions, session->sid - 1);
	LOG(5)("server of session close ok.[%p] %d %d\n", session, session->sid, server->sessions->cur_count);
}

void sis_socket_server_close(s_sis_socket_server *server_)
{ 
	if (!server_->isinit) 
	{
		return ;
	}
	if (server_->write_thread)
	{
		sis_wait_thread_destroy(server_->write_thread);
        server_->write_thread = NULL;
	}	
	while(!server_->write_may)
	{
		sis_sleep(10);
	}
	int index = sis_net_list_first(server_->sessions);
	while (index >= 0)
	{
		s_sis_socket_session *session = (s_sis_socket_session *)sis_net_list_get(server_->sessions, index);		
		if(!sis_socket_close_handle((uv_handle_t*)&session->uv_w_handle, cb_session_closed))
		{
			sis_net_list_stop(server_->sessions, session->sid - 1);
		}
		index = sis_net_list_next(server_->sessions, index);
		LOG(5)("server close %d. %d\n", index, server_->sessions->cur_count);
	}
	// 关闭发送数据的线程
	sis_socket_close_handle((uv_handle_t *)&server_->uv_s_handle, NULL);
	// LOG(0)("server close. %d %d\n", server_->uv_s_worker->active_handles, server_->uv_s_worker->active_reqs.count);
	uv_stop(server_->uv_s_worker); 
	// LOG(5)("server close 3.\n");
	if (server_->uv_s_thread)
	{
		uv_thread_join(&server_->uv_s_thread);  
	}
	server_->isinit = false;
	LOG(5)("server close ok.\n");
}

bool _sis_socket_server_init(s_sis_socket_server *server)
{
	if (server->isinit) 
	{
		return true;
	}
	if (!server->uv_s_worker) 
	{
		return false;
	}
	assert(0 == uv_loop_init(server->uv_s_worker));
	int o = uv_tcp_init(server->uv_s_worker, &server->uv_s_handle);
	if (o) 
	{
		return false;
	}
	server->isinit = true;
	server->uv_s_handle.data = server;
	return true;
}

void _thread_server(void* argv)
{
	s_sis_socket_server *server = (s_sis_socket_server*)argv;
	LOG(5)("server start.[%p] %s:%d\n", server, server->ip, server->port);
	uv_run(server->uv_s_worker, UV_RUN_DEFAULT);
	LOG(5)("server stop. [%d] %s:%d\n", server->write_may, server->ip, server->port);
    _uv_exit_loop(server->uv_s_worker);	
	server->uv_s_thread = 0;
}

void *_thread_server_write(void* argv);

bool _sis_socket_server_start(s_sis_socket_server *server)
{
	int o = uv_thread_create(&server->uv_s_thread, _thread_server, server);
	if (o) 
	{
		return false;
	}
	if (!server->write_thread)
	{
		server->write_thread = sis_wait_thread_create(SIS_UV_WRITE_WAIT);
		if (!sis_wait_thread_open(server->write_thread, _thread_server_write, server))
		{
			sis_wait_thread_destroy(server->write_thread);
			server->write_thread = NULL;
			LOG(1)("can't start server write thread.\n");
			return false;
		}
	}
	return true;	
}

bool _sis_socket_server_bind(s_sis_socket_server *server)
{
	struct sockaddr_in bind_addr;
	int o = uv_ip4_addr(server->ip, server->port, &bind_addr);
	if (o) 
	{
		return false;
	}
	o = uv_tcp_bind(&server->uv_s_handle, (const struct sockaddr*)&bind_addr, 0);
	if (o) 
	{
		return false;
	}
	// LOG(5)("server bind %s:%d.\n", server->ip, server->port);
	return true;
}

bool _sis_socket_server_bind6(s_sis_socket_server *server)
{
	struct sockaddr_in6 bind_addr;
	int o = uv_ip6_addr(server->ip, server->port, &bind_addr);
	if (o) 
	{
		return false;
	}
	o = uv_tcp_bind(&server->uv_s_handle, (const struct sockaddr*)&bind_addr, 0);
	if (o) 
	{
		return false;
	}
	// LOG(5)("server bind %s:%d.\n", server->ip, server->port);
	return true;
}
//////////////////////////////////////////////////////////////////
//服务器-新客户端函数
static void cb_server_read_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer)
{
	if (!handle->data) 
	{
		LOG(5)("cb_server_read_alloc : no session.\n");
		return;
	}
	s_sis_socket_session *session = (s_sis_socket_session *)handle->data;
	if (suggested_size > session->uv_r_buffer.len)
	{
		sis_free(session->uv_r_buffer.base);
		session->uv_r_buffer.base = sis_malloc(suggested_size + 1024);
		session->uv_r_buffer.len = suggested_size + 1024;
	}
	*buffer = session->uv_r_buffer;
}

static void cb_server_read_after(uv_stream_t *handle, ssize_t nread, const uv_buf_t* buffer)
{
	if (!handle->data) 
	{
		LOG(5)("cb_server_read_after : no session.\n");
		return;
	}
	s_sis_socket_session *session = (s_sis_socket_session *)handle->data; //服务器的recv带的是 session
	s_sis_socket_server *server = (s_sis_socket_server *)session->father;
	if (nread < 0) 
	{
		if (nread == UV_EOF) 
		{
			LOG(5)("session normal break.[%d] %s.\n", session->sid, uv_strerror(nread)); 
		} 
		else //  if (nread == UV_ECONNRESET) 
		{
			LOG(5)("session unusual break.[%d] %s.\n", session->sid, uv_strerror(nread)); 
		}
		sis_socket_server_delete(server, session->sid); //连接断开，关闭客户端
		return;
	}
	if (nread > 0 && session->cb_recv_after) 
	{
		buffer->base[nread] = 0;
		session->cb_recv_after(server->cb_source, session->sid, buffer->base, nread);
	}
}

static void cb_server_new_connect(uv_stream_t *handle, int status)
{
	s_sis_socket_server *server = (s_sis_socket_server *)handle->data;
	if (status)
	{
		LOG(5)("new connect fail : %s.\n", uv_strerror(status));
		return;
	}
	s_sis_socket_session *session = sis_socket_session_create(server);
	int o = uv_tcp_init(server->uv_s_worker, &session->uv_w_handle); 
	if (o) 
	{
		LOG(5)("no init client %d. close.\n", session->sid);
		sis_socket_session_destroy(session, 1);
		return;
	}
	int index = sis_net_list_new(server->sessions, session);
	session->sid = index + 1;
	o = uv_accept((uv_stream_t*)&server->uv_s_handle, (uv_stream_t*)&session->uv_w_handle);
	if (o) 
	{		
		LOG(5)("session cannot accept.\n");
		uv_close((uv_handle_t*)&session->uv_w_handle, cb_session_closed);
		return;
	}
	if (server->cb_connected_s2c) 
	{
		server->cb_connected_s2c(server->cb_source, session->sid);
	}

	o = uv_read_start((uv_stream_t*)&session->uv_w_handle, cb_server_read_alloc, cb_server_read_after);
	if (o)
	{
		LOG(5)("session cannot start read.\n");
		uv_close((uv_handle_t*)&session->uv_w_handle, cb_session_closed);
		return;
	}
	
	LOG(5)("new session ok. %p id = %d count = %d \n", session, session->sid, server->sessions->cur_count);
}

bool _sis_socket_server_listen(s_sis_socket_server *server)
{
	int o = uv_listen((uv_stream_t*)&server->uv_s_handle, SOMAXCONN, cb_server_new_connect);
	if (o) 
	{
		return false;
	}
	server->write_may = 1;
	// LOG(5)("server listen %s:%d.\n", server->ip, server->port);
	return true;
}

bool sis_socket_server_open(s_sis_socket_server *server)
{
	sis_socket_server_close(server);

	if (!_sis_socket_server_init(server)) 
	{
		LOG(5)("open server init fail.\n");
		return false;
	}

	if (!_sis_socket_server_bind(server)) 
	{
		LOG(5)("server bind error. %s:%d.\n", server->ip, server->port);
		return false;
	}

	if (!_sis_socket_server_listen(server)) 
	{
		LOG(5)("open server listen fail.\n");
		return false;
	}
	
	if (!_sis_socket_server_start(server)) 
	{
		LOG(5)("open server run fail.\n");
		return false;
	}
	return true;
}

bool sis_socket_server_open6(s_sis_socket_server *server)
{
	sis_socket_server_close(server);

	if (!_sis_socket_server_init(server)) 
	{
		return false;
	}

	if (!_sis_socket_server_bind6(server)) 
	{
		return false;
	}

	if (!_sis_socket_server_listen(server)) 
	{
		return false;
	}
	if (!_sis_socket_server_start(server)) 
	{
		return false;
	}
	return true;
}
// #define _UV_DEBUG_
#ifdef _UV_DEBUG_
msec_t    _uv_write_msec = 0;
int       _uv_write_nums = 0;
#endif
static void _server_write_may(s_sis_socket_server *server)
{
	sis_mutex_lock(&server->write_may_lock);
	server->write_may = 1;
	sis_mutex_unlock(&server->write_may_lock);
	// 通知线程开始干活
	printf("--1--%d\n", server->write_may);
	sis_wait_thread_notice(server->write_thread);
}
static void cb_server_write_after(uv_write_t *writer_, int status)
{	
	s_sis_socket_server *server = (s_sis_socket_server *)writer_->data;
	s_sis_socket_session *session = sis_net_list_get(server->sessions, server->cur_send_idx);
#ifdef _UV_DEBUG_
	_uv_write_nums+= session->send_nodes->sendnums;
	// if (_uv_write_nums % 1000 == 0)
	LOG(5)("uv write stop:  nums :%9d status = %3d delay : %lld \n", _uv_write_nums, status, sis_time_get_now_msec() - _uv_write_msec);
	_uv_write_msec = sis_time_get_now_msec();
#endif
	if (!session) 
	{
		// 释放已经发送完毕的内存数据
		_server_write_may(server);
		return;
	}
	sis_net_nodes_free_read(session->send_nodes);

	if (status < 0) 
	{
		LOG(5)("server write fail: %s.\n", uv_strerror(status));
		// 写数据错就关闭该连接
		sis_socket_server_delete(server, session->sid); //连接断开，关闭客户端
		_server_write_may(server);
		return ;
	}
	if (session && session->cb_send_after)
	{
		session->cb_send_after(server->cb_source, session->sid, status);
	}
	_server_write_may(server);
}
static int _server_send_data(s_sis_socket_server *server, s_sis_socket_session *session)
{
	int count = sis_net_nodes_read(session->send_nodes, session->sendnums);
	if (count > 0)
	{
		int index = 0;
		s_sis_net_node *next = session->send_nodes->rhead;
		while (next)
		{
			session->sendbuff[index].base = SIS_OBJ_GET_CHAR(next->obj);
			session->sendbuff[index].len = SIS_OBJ_GET_SIZE(next->obj);
			index++;	
			next = next->next;
		}
		uv_write_t  uv_whandle; 
		uv_whandle.data = server;
		if (uv_write(&uv_whandle, (uv_stream_t *)&session->uv_w_handle, 
			session->sendbuff, index, cb_server_write_after)) 
		{
			printf("server write fail.\n");
			return -1;
		}
	}
	return count;
}

void *_thread_server_write(void* argv)
{
	s_sis_socket_server *server = (s_sis_socket_server *)argv;
    sis_wait_thread_start(server->write_thread);
	server->cur_send_idx = -1;
    while (sis_wait_thread_noexit(server->write_thread))
    {
		sis_mutex_lock(&server->write_may_lock);
		// printf("--.2.-- %d %d\n", server->write_may, server->sessions->cur_count);
		if (server->write_may == 0)
		{
			sis_mutex_unlock(&server->write_may_lock);
	       	sis_wait_thread_wait(server->write_thread, server->write_thread->wait_msec);
			continue;
		}
		else
		{
			server->write_may = 0;
			sis_mutex_unlock(&server->write_may_lock);
		}
		// 这里进入独占写操作
		int nouse = 1;
		int count = 0;
		while(count <= server->sessions->cur_count)
		{
			int index = sis_net_list_next(server->sessions, server->cur_send_idx);
			s_sis_socket_session *session = sis_net_list_get(server->sessions, index);
			if (!session)
			{
				// printf("--.1111.1.-- %d %d %d %d\n", index, server->cur_send_idx, server->sessions->cur_count, server->sessions->max_count);
				break;
			}
			server->cur_send_idx = index;
			// printf("--.3.1.-- %d\n", server->write_may);
			int ok = _server_send_data(server, session);
			// printf("ssss :%d %d %p %d %d\n", index, ok, session, server->sessions->cur_count, server->cur_send_idx);
			if (ok > 0)
			{
				// 可能没执行下面代码就发送成功 不过并太不影响
				nouse = 0;
				server->cur_send_idx = index;
				break;
			}
			else if (ok < 0)
			{
				// printf("--.3.1.-- %d %d\n", session->sid, server->write_may);
				// 写入错误 关闭该链接
				sis_socket_server_delete(server, session->sid);
			}
			count++;
		}
		if (nouse)
		{
			// 已经轮询一圈都没有数据 转为可写状态
			sis_mutex_lock(&server->write_may_lock);
			server->write_may = 1;
			sis_mutex_unlock(&server->write_may_lock);
		}
		// printf("--.3.3.-- %d\n", server->write_thread->wait_msec);
       	sis_wait_thread_wait(server->write_thread, server->write_thread->wait_msec);
    }
    sis_wait_thread_stop(server->write_thread);
	return NULL;
}

//服务器发送函数
bool sis_socket_server_send(s_sis_socket_server *server_, int sid_, s_sis_object *inobj_)
{
	if (server_->isexit)
	{
		return false;
	}
	if (!inobj_) 
	{
		return false;
	}
	s_sis_socket_session *session = sis_net_list_get(server_->sessions, sid_ - 1);
	if (!session)
	{
		LOG(5)("can't find client %d.\n", sid_);
		return false;
	}
	// 放入队列就返回 其他交给内部处理
	sis_net_nodes_push(session->send_nodes, inobj_);
	sis_wait_thread_notice(server_->write_thread);
	return true;
}
 
void sis_socket_server_set_rwcb(s_sis_socket_server *server_, int sid_, 
	cb_socket_recv_after cb_recv_, cb_socket_send_after cb_send_)
{
	s_sis_socket_session *session = sis_net_list_get(server_->sessions, sid_ - 1);
	if (session)
	{
		sis_socket_session_set_rwcb(session, cb_recv_, cb_send_);
	}
}
//服务器-新链接回调函数
void sis_socket_server_set_cb(s_sis_socket_server *server_, 
	cb_socket_connect cb_connected_,
	cb_socket_connect cb_disconnect_)
{
	server_->cb_connected_s2c = cb_connected_;
	server_->cb_disconnect_s2c = cb_disconnect_;
}
bool sis_socket_server_delete(s_sis_socket_server *server_, int sid_)
{
	s_sis_socket_session *session = sis_net_list_get(server_->sessions, sid_ - 1);
	if (!session)
	{
		LOG(5)("can't find session.[%d:%d]\n", server_->sessions->cur_count ,sid_);
		return false;
	}
	// if (session->write_stop == 1)
	// {
	// 	// 已经发出关闭信号 避免二次进入
	// 	return true;
	// }
	// session->write_stop = 1;

	//  先发断开回调，再去关闭
	if (server_->cb_disconnect_s2c) 
	{
		server_->cb_disconnect_s2c(server_->cb_source, sid_);
	}
	sis_socket_close_handle((uv_handle_t*)&session->uv_w_handle, cb_session_closed);
	LOG(5)("start close session.[%d == %d] %p\n", session->sid, sid_, session);
	return true;	
}


/////////////////////////////////////////////////////////////
// s_sis_socket_client 
/////////////////////////////////////////////////////////////
void _thread_reconnect(void* argv)
{
	s_sis_socket_client *client = (s_sis_socket_client*)argv;

	LOG(5)("client reconnect open. [%p]\n", client);
	client->reconn_status = SIS_UV_CONNECT_WORK;	
	int count = 0;
	while (!(client->reconn_status == SIS_UV_CONNECT_EXIT))
	{
		// 只有下面两个结果标记可以定时重连
		if (client->work_status != SIS_UV_CONNECT_WORK)
		{
			if (count > 25)
			{
				sis_socket_client_open(client);
				count = 0;
			}
			count++;
		}
		else
		{
			count = 0;
		}
		sis_sleep(200);
	}
	LOG(5)("client reconnect stop. [%d]\n", client->work_status);
}

s_sis_socket_client *sis_socket_client_create()
{
	s_sis_socket_client *client = SIS_MALLOC(s_sis_socket_client, client); 

	client->uv_c_worker = sis_malloc(sizeof(uv_loop_t));  

	client->isinit = false;

	client->cb_source = client;
	client->cb_connected_c2s = NULL;
	client->cb_disconnect_c2s = NULL;

	client->session = sis_socket_session_create(client);

	client->work_status = SIS_UV_CONNECT_NONE;

	client->isexit = false;

	sis_mutex_create(&client->write_may_lock);
	
	if (uv_thread_create(&client->uv_reconn_thread, _thread_reconnect, client)) 
	{
		LOG(5)("create keep connect fail.\n");
	}
	// LOG(8)("client create.[%p]\n", client);
	return client;
}
void sis_socket_client_destroy(s_sis_socket_client *client_)
{	
	if (client_->isexit)
	{
		return ;
	}
	client_->isexit = true;
	// 先停止重连线程
	client_->reconn_status = SIS_UV_CONNECT_EXIT; 
	uv_thread_join(&client_->uv_reconn_thread);  
	
	// 再关闭链接
	sis_socket_client_close(client_);

	sis_socket_session_destroy(client_->session, 1);
	sis_mutex_destroy(&client_->write_may_lock);
	sis_free(client_->uv_c_worker);
	sis_free(client_);
	LOG(5)("client exit.\n");
}

void cb_client_close(uv_handle_t *handle)
{
	s_sis_socket_session *session = (s_sis_socket_session *)handle->data;
	s_sis_socket_client *client = (s_sis_socket_client*)session->father;

	client->work_status |= SIS_UV_CONNECT_STOP;
	if (client->cb_disconnect_c2s) 
	{
		client->cb_disconnect_c2s(client->cb_source, 0);
	}
}

void sis_socket_client_close(s_sis_socket_client *client_)
{
	if (!client_->isinit) 
	{
		client_->work_status |= SIS_UV_CONNECT_STOP;
		return ;
	}
	if (client_->work_status & SIS_UV_CONNECT_EXIT)
	{
		return ; // 已经关闭过
	}
	client_->work_status = SIS_UV_CONNECT_EXIT;

	if (client_->write_thread)
	{
		sis_wait_thread_destroy(client_->write_thread);
        client_->write_thread = NULL;
	}
	sis_socket_close_handle((uv_handle_t*)&client_->session->uv_w_handle, cb_client_close);
	// 清理数据
	// sis_socket_session_clear(client_->session);

	uv_stop(client_->uv_c_worker); 

	if (client_->uv_c_thread)
	{
		uv_thread_join(&client_->uv_c_thread);  
	}
	client_->isinit = false;
	LOG(5)("client close ok.\n");
}

static void cb_client_read_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer)
{
	if (!handle->data) 
	{
		LOG(5)("cb_server_read_alloc : no session.\n");
		return;
	}
	s_sis_socket_session *session = (s_sis_socket_session *)handle->data;
	if (suggested_size > session->uv_r_buffer.len)
	{
		sis_free(session->uv_r_buffer.base);
		session->uv_r_buffer.base = sis_malloc(suggested_size + 1024);
		session->uv_r_buffer.len = suggested_size + 1024;
	}
	*buffer = session->uv_r_buffer;
}

static void cb_client_read_after(uv_stream_t *handle, ssize_t nread, const uv_buf_t* buffer)
{
	if (!handle->data) 
	{
		LOG(5)("cb_client_read_after : no session.\n");
		return;
	}
	s_sis_socket_session *session = (s_sis_socket_session *)handle->data; //服务器的recv带的是 session
	s_sis_socket_client *client = (s_sis_socket_client*)session->father;
	if (nread < 0) 
	{
		if (nread == UV_EOF) 
		{
			LOG(5)("client normal break.[%p] %s.\n", handle, uv_strerror(nread)); 
		} 
		else //  if (nread == UV_ECONNRESET) 
		{
			LOG(5)("client unusual break.[%p] %s.\n", handle, uv_strerror(nread)); 
		}
		sis_socket_client_close(client);
		return;
	}
	if (nread > 0 && session->cb_recv_after) 
	{
		buffer->base[nread] = 0;
		session->cb_recv_after(client->cb_source, 0, buffer->base, nread);
	}
}

static void _cb_after_connect(uv_connect_t *handle, int status)
{
	s_sis_socket_session *session = (s_sis_socket_session *)handle->data; 
	s_sis_socket_client *client = (s_sis_socket_client *)session->father;
	if (status)
	{
		LOG(8)("client connect error: %s.\n", uv_strerror(status));
		sis_socket_close_handle((uv_handle_t*)&session->uv_w_handle, NULL);
		client->work_status |= SIS_UV_CONNECT_FAIL;
		return;
	}
	int o = uv_read_start((uv_stream_t*)&session->uv_w_handle, cb_client_read_alloc, cb_client_read_after); //客户端开始接收服务器的数据
	if (o)
	{
		LOG(5)("client cannot start read: %s.\n", uv_strerror(status));
		sis_socket_close_handle((uv_handle_t*)&session->uv_w_handle, NULL);
		client->work_status |= SIS_UV_CONNECT_FAIL;
		return;
	}
	client->work_status = SIS_UV_CONNECT_WORK;
	if (client->cb_connected_c2s) 
	{
		client->cb_connected_c2s(client->cb_source, 0);
	}
	client->write_may = 1;
	LOG(8)("client connect ok.[%d]\n", client->work_status);
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
	int o = uv_loop_init(client_->uv_c_worker); 
	if (o)
	{
		LOG(5)("loop init fail.[%d] %s %s\n", o, uv_err_name(o), uv_strerror(o));
		return false;
	}
	sis_socket_session_init(client_->session);
	o = uv_tcp_init(client_->uv_c_worker, &client_->session->uv_w_handle);
	if (o) 
	{
		return false;
	}
	client_->isinit = true;
	return true;
}

void _thread_client(void* arg)
{
	s_sis_socket_client *client = (s_sis_socket_client*)arg;
	LOG(5)("client thread start. [%p] %s:%d\n", client, client->ip, client->port);
	struct sockaddr_in bind_addr;
	int o = uv_ip4_addr(client->ip, client->port, &bind_addr);
	if (o) 
	{
		return;
	}
	o = uv_tcp_init(client->uv_c_worker, &client->session->uv_w_handle);
  	if (o) 
	{
		return ;
	}
	uv_connect_t  uv_c_connect;	    // 连接时请求 主要传递当前类
	uv_c_connect.data = client->session;
	o = uv_tcp_connect(&uv_c_connect, &client->session->uv_w_handle, (const struct sockaddr*)&bind_addr, _cb_after_connect);
	if (o) 
	{
		return ;
	}	
	uv_run(client->uv_c_worker, UV_RUN_DEFAULT); 
	// 关闭时问题可参考 test-tcp-close-reset.c
    _uv_exit_loop(client->uv_c_worker);
	LOG(5)("client thread stop. [%d]\n", client->work_status);
}

void *_thread_client_write(void* argv);

bool sis_socket_client_open(s_sis_socket_client *client_)
{
	if (client_->work_status == SIS_UV_CONNECT_WAIT)
	{
		return false;
	}
	client_->work_status = SIS_UV_CONNECT_WAIT;
	if (!_sis_socket_client_init(client_)) 
	{
		return false;
	}
	// LOG(5)("client connect %s:%d status = %d \n", client_->ip, client_->port, client_->work_status);
	int o = uv_thread_create(&client_->uv_c_thread, _thread_client, client_);
	//触发 _cb_after_connect 才算真正连接成功，所以用线程
	if (o) 
	{
		return false;
	}
	if (!client_->write_thread)
	{
		client_->write_thread = sis_wait_thread_create(SIS_UV_WRITE_WAIT);
		if (!sis_wait_thread_open(client_->write_thread, _thread_client_write, client_))
		{
			sis_wait_thread_destroy(client_->write_thread);
			client_->write_thread = NULL;
			LOG(1)("can't start client write thread.\n");
			return false;
		}
	}
	return true;
}
void _thread_connect6(void* arg)
{
	s_sis_socket_client *client = (s_sis_socket_client*)arg;
	LOG(5)("client connect6 thread start. [%d]\n", client->work_status);
	struct sockaddr_in6 bind_addr;
	int o = uv_ip6_addr(client->ip, client->port, &bind_addr);
	if (o) 
	{
		return;
	}
	o = uv_tcp_init(client->uv_c_worker, &client->session->uv_w_handle);
  	if (o) 
	{
		return ;
	}
	uv_connect_t  uv_c_connect;	    // 连接时请求 主要传递当前类
	uv_c_connect.data = client->session;
	o = uv_tcp_connect(&uv_c_connect, &client->session->uv_w_handle, (const struct sockaddr*)&bind_addr, _cb_after_connect);
	if (o) 
	{
		return ;
	}
	uv_run(client->uv_c_worker, UV_RUN_DEFAULT); 
	_uv_exit_loop(client->uv_c_worker);
	LOG(5)("client connect6 thread stop. [%d]\n", client->work_status);
}
bool sis_socket_client_open6(s_sis_socket_client *client_)
{
	if (client_->work_status == SIS_UV_CONNECT_WAIT)
	{
		return false;
	}
	client_->work_status = SIS_UV_CONNECT_WAIT;
	if (!_sis_socket_client_init(client_)) 
	{
		return false;
	}

	LOG(5)("client connect %s:%d status = %d \n", client_->ip, client_->port, client_->work_status);
	int o = uv_thread_create(&client_->uv_c_thread, _thread_connect6, client_);
	//触发 _cb_after_connect 才算真正连接成功，所以用线程
	if (o) 
	{
		return false;
	}
	return true;
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
	client_->session->cb_recv_after = cb_recv_;
	client_->session->cb_send_after = cb_send_;
}
static void _client_write_may(s_sis_socket_client *client)
{
	// 设置缓存可写
	sis_mutex_lock(&client->write_may_lock);
	client->write_may = 1;
	sis_mutex_unlock(&client->write_may_lock);
	// 通知线程开始干活
	sis_wait_thread_notice(client->write_thread);
}

static void cb_client_write_after(uv_write_t *writer_, int status)
{
	s_sis_socket_client *client = (s_sis_socket_client *)writer_->data; 
	s_sis_socket_session *session  = client->session;

	if (status < 0) 
	{
		LOG(5)("client write error: %s.\n", uv_strerror(status));
		sis_socket_client_close(client);
		_client_write_may(client);
		return;
	}
	// 释放已经发送完毕的内存数据
	sis_net_nodes_free_read(session->send_nodes);
	if (session->cb_send_after)
	{
		session->cb_send_after(client->cb_source, 0, status);
	} 
	_client_write_may(client);
}
void *_thread_client_write(void* argv)
{
	s_sis_socket_client *client = (s_sis_socket_client *)argv;
    sis_wait_thread_start(client->write_thread);
    while (sis_wait_thread_noexit(client->write_thread))
    {
		sis_mutex_lock(&client->write_may_lock);
		if (client->write_may == 0)
		{
			sis_mutex_unlock(&client->write_may_lock);
	       	sis_wait_thread_wait(client->write_thread, client->write_thread->wait_msec);
			continue;
		}
		else
		{
			client->write_may = 0;
			sis_mutex_unlock(&client->write_may_lock);
		}
		s_sis_socket_session *session = client->session;
		int count = sis_net_nodes_read(session->send_nodes, session->sendnums);
		if (count > 0)
		{
			int index = 0;
			s_sis_net_node *next = session->send_nodes->rhead;
			while (next)
			{
				session->sendbuff[index].base = SIS_OBJ_GET_CHAR(next->obj);
				session->sendbuff[index].len = SIS_OBJ_GET_SIZE(next->obj);
				index++;	
				next = next->next;
			}
			uv_write_t  uv_whandle; 
			uv_whandle.data = client;
			if (uv_write(&uv_whandle, (uv_stream_t *)&session->uv_w_handle, 
						session->sendbuff, index, cb_client_write_after)) 
			{
				// 关闭链接 从新开始
				sis_socket_client_close(client);
			}
		}
		if (count == 0)
		{
			sis_mutex_lock(&client->write_may_lock);
			client->write_may = 1;
			sis_mutex_unlock(&client->write_may_lock);
		}
       	sis_wait_thread_wait(client->write_thread, client->write_thread->wait_msec);
    }
    sis_wait_thread_stop(client->write_thread);
	return NULL;
}

bool sis_socket_client_send(s_sis_socket_client *client_, s_sis_object *inobj_)
{
	if (client_->isexit)
	{
		return false;
	}
	if (!inobj_) 
	{
		return false;
	}
	sis_net_nodes_push(client_->session->send_nodes, inobj_);
	sis_wait_thread_notice(client_->write_thread);
	return true;
}

#if 0
// 下面是多线程的收发包例子 基本能跑满带宽
// 不用外部函数 用基本uv函数
#include "sis_list.lock.h"

typedef struct s_net_nodes {
	uv_tcp_t           handle;	
	uv_thread_t        thread;
	// uv_async_t         async_sp;
	uv_write_t         write_sp;

	s_sis_mutex_t      lock;  
    int                nums;  
    s_sis_unlock_node *head; 
	s_sis_unlock_node *tail;  
} s_net_nodes;


// 这个例子测试服务端发送数据的速度 client 记录收到字节数
// 需要支持多个客户端的请求 
// 对每个客户端建立一个链表 
// 一旦有客户写入数据 就启动异步通知 
// 收到通知后就开始从头遍历用户链表 只要有数据就把需要发送的数据链 放入发送队列 
// 发送完成后删除发送链表

size_t      speed_recv_size = 0;
size_t      speed_recv_curr = 0;
msec_t      speed_recv_msec = 0;

uv_loop_t  *server_loop = NULL;
uv_loop_t  *client_loop = NULL;

uv_tcp_t    uv_s_handle;	
uv_tcp_t    client_handle;	

uv_buf_t    send_buffer;
uv_buf_t    recv_buffer;

int client_count = 0;
static void cb_client_recv_after(uv_stream_t *handle, ssize_t nread, const uv_buf_t* buffer)
{
	if (nread < 0) 
	{
		exit(100);
	}
	speed_recv_size += nread;
	// printf("cb_client_recv_after %12zu %12zu %lld\n", speed_recv_size, speed_recv_msec, sis_time_get_now_msec());
	if (speed_recv_msec == 0) 
	{
		speed_recv_msec = sis_time_get_now_msec();
	}
	else 
	{
		int offset = sis_time_get_now_msec() - speed_recv_msec;
		if (offset >= 1000)
		{
			speed_recv_msec = sis_time_get_now_msec();
			printf("cost : %5d recv : %12zu, %10zu speed(M/s): %lld\n", offset, speed_recv_size, speed_recv_curr, (long long)speed_recv_curr/offset/1000);
			speed_recv_curr = 0; 
		}
	}
	speed_recv_curr += nread;
}
static void cb_client_recv_before(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer)
{
	// printf("cb_client_recv_before ok\n");
	if (suggested_size > recv_buffer.len)
	{
		sis_free(recv_buffer.base);
		recv_buffer.base = sis_malloc(suggested_size + 1024);
		recv_buffer.len = suggested_size + 1024;
	}
	*buffer = recv_buffer;
}
static void _cb_client_connect(uv_connect_t *handle, int status)
{
	if(uv_read_start(handle->handle, cb_client_recv_before, cb_client_recv_after)) exit(2);
}

// 这样写也是可以的 做好同步就行了 写入就发送 发送完再读数据有数据就处理 没数据就等待
// 无论如何libuv上次数据传输完毕后 必须一次性把临时缓存中数据全部发出去 否则速度慢
// 但奇怪的是如果在发送成功回调中发送数据 就很快 怀疑是如果不在在回调中发送数据 就会进入消息队列
// 而我再次写入数据时，消息队列可能有未知的耗时操作导致速度慢了
// 无论如何 要求只要发送就必须把需要发送的一次性推出去，否则速度会很慢
// 这样又要求每个客户端自己保存自己的数据 有点痛苦 等空的时候换掉他 

void net_nodes_add(s_net_nodes *service, s_sis_object *obj)
{
	sis_mutex_lock(&service->lock);
	service->nums++;
	s_sis_unlock_node *newnode = sis_unlock_node_create(obj);
	sis_object_decr(newnode->obj);
	if (!service->head)
	{
		service->head = newnode;
		service->tail = newnode;
	}
	else if (service->head == service->tail)
	{
		service->head->next = newnode;
		service->tail = newnode;
	}
	else
	{
		service->tail->next = newnode;
		service->tail = newnode;
	}
	sis_mutex_unlock(&service->lock);

}
// volatile
int may_write = 1;
s_sis_mutex_t      may_write_lock;  
#define MAY_WRITE_LOCK
s_sis_wait_thread  *work_thread;

static void _cb_write_after(uv_write_t *write_, int status)
{
	if (write_)
	{
		printf("_cb_write_after 1 = %d\n", may_write);
		s_sis_unlock_node *node = (s_sis_unlock_node *)write_->data;
		while (node)
		{
			s_sis_unlock_node *next = node->next;
			// sis_object_decr(node->obj);
			sis_free(node);
			node = next;
		}
	}
#ifdef MAY_WRITE_LOCK
	sis_mutex_lock(&may_write_lock);
	may_write = 1;
	sis_mutex_unlock(&may_write_lock);
#else
	while(!BCAS(&may_write, 0, 1))
	{
		sis_sleep(1);
	}
#endif
	printf("_cb_write_after 2 = %d %d\n", may_write, status);
	if (status)
	{
		uv_close((uv_handle_t *)write_->send_handle, NULL);
	}
	sis_wait_thread_notice(work_thread);
}
uv_buf_t *write_lists =  NULL;
int       write_maxcount = 0;
int64     write_nums = 0;

s_sis_pointer_list *services = NULL;

int ago_service_index =  0; // 
int cur_service_index = -1; // >=零表示需要等待  
bool send_data(s_net_nodes* service)
{
	if (!service)
	{
		return false;
	}
	sis_mutex_lock(&service->lock);
	s_sis_unlock_node *node = service->head;
	int count = service->nums;
	service->head = NULL;
	service->tail = NULL;
	service->nums = 0;
	sis_mutex_unlock(&service->lock);

	write_nums += count;
	printf("_cb_async_write : %p %d %lld\n", service, count, write_nums);
	if (count > 0)
	{
		if (count > write_maxcount)
		{
			if (write_maxcount > 0)
			{
				sis_free(write_lists);
			}
			write_lists = sis_malloc(10 * count * sizeof(uv_buf_t));
			write_maxcount = 10 * count;
		}
		s_sis_unlock_node *next = node;		
		for (int i = 0; i < count; i++)
		{
			write_lists[i].base =  SIS_OBJ_GET_CHAR(next->obj);
			write_lists[i].len = SIS_OBJ_GET_SIZE(next->obj);
			next = next->next;
		}
		service->write_sp.data = node;
		if (uv_write(&service->write_sp, (uv_stream_t *)&service->handle, write_lists, count, _cb_write_after)) 
		{
			printf("server write fail.\n");
			return false;
		}
	}
	else
	{
		return false;
	}
	return true;
}

void *_thread_read(void* arg)
{
	sis_wait_thread_start(work_thread);
	while (sis_wait_thread_noexit(work_thread))
    {
#ifdef MAY_WRITE_LOCK
		sis_mutex_lock(&may_write_lock);
		if (may_write == 1)
		{
			may_write = 0;
			sis_mutex_unlock(&may_write_lock);
		}
		else
		{
			sis_mutex_unlock(&may_write_lock);
			sis_wait_thread_wait(work_thread, 1000);
			continue;
		}
#else
		while(!BCAS(&may_write, 1, 0))
		{
			sis_sleep(1);
    	}
#endif
		int nouse = 1;
		if (services->count > 0)
		{
			int index = cur_service_index + 1;
			int count = 0;
			while(count < services->count)
			{
				index = index % services->count;
				s_net_nodes* service = sis_pointer_list_get(services, index);
				if (send_data(service))
				{
					nouse = 0;
					cur_service_index = index;
					break;
				}
				else
				{
					index++;
				}
				count++;
			}
		}	
		if (nouse)
		{
#ifdef MAY_WRITE_LOCK
		sis_mutex_lock(&may_write_lock);
		may_write = 1;
		sis_mutex_unlock(&may_write_lock);
#else
			while(!BCAS(&may_write, 0, 1))
			{
				sis_sleep(1);
			}			
#endif
		}	
		// sis_sleep(1);
		// printf("timeout exit. %d \n", 1);
		if (sis_wait_thread_wait(work_thread, 1000) == SIS_WAIT_TIMEOUT)
		{
			printf("timeout exit. %d \n", may_write);
		}
	}
	sis_wait_thread_stop(work_thread);
	return NULL;
}
// 持续发送数据
int count = 0;
int maxcount = 1000*1000*1000;
int sendsize = 16384;

size_t      speed_send_size = 0;
size_t      send_speed_size = 0;
msec_t      speed_send_msec = 0;
int   send_counts = 0;
void _thread_write(void* arg)
{
	s_net_nodes *service = (s_net_nodes *)arg;
	s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnewlen(send_buffer.base, send_buffer.len));
	printf("_thread_write : %p\n", service);
	speed_send_msec = sis_time_get_now_msec();
	// for (int i = 0; i < maxcount; i++)
	for (int i = 0; ; i++)
	{
		speed_send_size += sendsize;
		 
		if (speed_send_size > (int64)1000000000 * 3)
		{
			msec_t offset = sis_time_get_now_msec() - speed_send_msec;
			if (offset < 1000)
			{
				sis_sleep(1000 - offset);
			}
			speed_send_msec = sis_time_get_now_msec();
			speed_send_size = 0;
		}
		net_nodes_add(service, obj);
		// if (send_counts > 1000)
		{
			sis_wait_thread_notice(work_thread);
			send_counts = 0;
		}
		send_counts++;
	}
	printf("thread send ok.\n");
}

static void _cb_new_connect(uv_stream_t *handle, int status)
{
	s_net_nodes *service = SIS_MALLOC(s_net_nodes, service);
	sis_mutex_init(&service->lock, NULL);

	printf("new connect . %d \n", ++client_count);	
	if(uv_tcp_init(server_loop, &service->handle)) exit(2);

	if (uv_accept((uv_stream_t*)&uv_s_handle, (uv_stream_t*)&service->handle)) exit(3);
	// 启动发送数据的线程	
	sis_pointer_list_push(services, service);
	uv_thread_create(&service->thread, _thread_write, service);
	printf("_cb_new_connect ok.\n");
}


int main(int argc, char **argv)
{
	if (argc < 2)
	{
		sis_mutex_init(&may_write_lock, NULL);

		send_buffer = uv_buf_init((char*)sis_malloc(sendsize), sendsize);

		work_thread = sis_wait_thread_create(50);
		if (!sis_wait_thread_open(work_thread, _thread_read, NULL))
		{
			LOG(1)("can't start reader thread.\n");
			return 0;
		}
		// it is server
		services = sis_pointer_list_create();
		

		server_loop = sis_malloc(sizeof(uv_loop_t)); 
		uv_loop_init(server_loop);

		if(uv_tcp_init(server_loop, &uv_s_handle)) exit(0);

		struct sockaddr_in bind_addr;
		if(uv_ip4_addr("127.0.0.1", 7777, &bind_addr)) exit(0);

		if(uv_tcp_bind(&uv_s_handle, (const struct sockaddr*)&bind_addr, 0)) exit(0);
		
		if(uv_listen((uv_stream_t*)&uv_s_handle, SOMAXCONN, _cb_new_connect)) exit(0);
		
		printf("server ok\n");
		
		uv_run(server_loop, UV_RUN_DEFAULT);

		uv_loop_close(server_loop); 

	}
	printf("client close.\n");
	while(1)
	{
		recv_buffer = uv_buf_init((char*)sis_malloc(sendsize), sendsize);

		client_loop = sis_malloc(sizeof(uv_loop_t)); 
		uv_loop_init(client_loop);

		if (uv_tcp_init(client_loop, &client_handle)) 
		{
			printf("==0==\n");
			return 0;
		}
		struct sockaddr_in bind_addr;
		if (uv_ip4_addr("127.0.0.1", 7777, &bind_addr)) 
		{
			printf("==1==\n");
			return 0;
		}

		uv_connect_t connect_req;
		if(uv_tcp_connect(&connect_req, &client_handle, (const struct sockaddr*)&bind_addr, _cb_client_connect)) 
		{
			printf("==2==\n");
			return 0;
		}

		printf("client ok\n");
		uv_run(client_loop, UV_RUN_DEFAULT); 
		uv_loop_close(client_loop); 
	}

	return 0;
}
#endif
