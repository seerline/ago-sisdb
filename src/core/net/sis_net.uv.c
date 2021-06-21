
#include <sis_net.uv.h>
#include <sis_math.h>

// ***************************************************** //
// 实在没搞懂libuv的运行机制 明明已经 close 了所有handle
// 执行 uv_stop 后仍然不能保证从 uv_run 中优雅的退出
// 只能在timer.c中的 uv__next_timeout 函数中设置结点为空时的超时为20毫秒
// 原始为 -1 (永远等待) 暂时解决这个问题, 等以后 libuv 升级后看看有没有其他解决方案
// 理想状态是 uv_stop 后系统自动清理所有未完成的工作 然后返回 
// ***************************************************** //
// 理想状态下 libuv负责网络通讯 有数据了用回调返回 所有网络时间通过回调可以解决
// 用户需要发送数据时 应该把发送的数据作为事件注册到libuv中就不管了，由libuv自行调度把数据推到对端
// 如果发生错误，回调出来 交给用户处理 是否重连重发等等
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

/////////////////////////////////////////////////
//  s_sis_net_queue
// 专门处理uv多线程写入时 async不能1对1返回的错误
/////////////////////////////////////////////////
/////////////////////////////////////////////////
//  s_sis_net_node
/////////////////////////////////////////////////
s_sis_net_node *sis_net_node_create(int cid_, s_sis_object *obj_)
{
    s_sis_net_node *node = SIS_MALLOC(s_sis_net_node, node);
    if (obj_)
    {
        sis_object_incr(obj_);
        node->obj = obj_;
    }
	node->cid = cid_;
    node->next = NULL;
    return node;    
}
void sis_net_node_destroy(s_sis_net_node *node_)
{
    if(node_->obj)
    {
        sis_object_decr(node_->obj);
    }
    sis_free(node_);
}
s_sis_net_node *sis_net_queue_pop(s_sis_net_queue *queue_)
{  
    s_sis_net_node *node = NULL;
	if (queue_->count > 0)
	{
		if (queue_->head == queue_->tail)
		{
			node = queue_->head;
			queue_->count = 0;
			queue_->head = NULL;
			queue_->tail = NULL;
		}
		else
		{
			node = queue_->head;
			queue_->head = queue_->head->next;
			queue_->count--; 
		}
	}
    return node;
}
void *_thread_net_reader(void *argv_)
{
    s_sis_net_queue *wqueue = (s_sis_net_queue *)argv_;
	sis_wait_thread_start(wqueue->work_thread);
    while (sis_wait_thread_noexit(wqueue->work_thread))
    {
		s_sis_net_node *node = NULL;
        // printf("1 --- %p\n", wqueue->live);
        sis_mutex_lock(&wqueue->lock);
        if (wqueue->live == NULL)
        {
            node = sis_net_queue_pop(wqueue);
            if (node)
            {
                wqueue->live = node;
            }
        }
        sis_mutex_unlock(&wqueue->lock);
		// printf("1.1 ---  %p %p %p %d\n", node, wqueue->head, wqueue->tail, wqueue->work_thread->wait_msec);
        if (node)
        {
            if (wqueue->cb_reader)
            {
                wqueue->cb_reader(wqueue->source);
            }
        }
        // printf("1.2 ---  %p\n", node);
		if (wqueue->live == NULL && wqueue->count > 0) // 已经发送完毕 不需等待直接下一个
		{
			continue;
		}
		if (sis_wait_thread_wait(wqueue->work_thread, wqueue->work_thread->wait_msec) == SIS_WAIT_TIMEOUT)
		{
			// printf("timeout exit. %d \n", waitmsec);
		}     
    }
    sis_wait_thread_stop(wqueue->work_thread);
    return NULL;
}

s_sis_net_queue *sis_net_queue_create(
	void *source_, 
    cb_net_reader *cb_reader_,
    int wait_msec_)
{
    s_sis_net_queue *o = SIS_MALLOC(s_sis_net_queue, o);
	o->source = source_;
    o->head = NULL;
    o->tail = NULL;
    o->count = 0;
	o->live = NULL;
	
    sis_mutex_init(&o->lock, NULL);

    o->cb_reader = cb_reader_;
    
	int wait_msec = wait_msec_ == 0 ? 300 : wait_msec_;
	o->work_thread = sis_wait_thread_create(wait_msec);
    // 最后再启动线程 顺序不能变 不然数据会等待
    if (!sis_wait_thread_open(o->work_thread, _thread_net_reader, o))
    {
        LOG(1)("can't start reader thread.\n");
        sis_net_queue_destroy(o);
        return NULL;
    }
    return  o;
}
void sis_net_queue_clear(s_sis_net_queue *queue_)
{
    sis_mutex_lock(&queue_->lock);
    while (queue_->count > 0)
    {
        s_sis_net_node *node = sis_net_queue_pop(queue_);
		if (node)
		{
			sis_net_node_destroy(node);
		}
    }
	if (queue_->live)
	{
		sis_net_node_destroy(queue_->live);
		queue_->live = NULL;
	}
    sis_mutex_unlock(&queue_->lock);
}
void sis_net_queue_destroy(s_sis_net_queue *queue_)
{
	if (queue_->work_thread)
	{
		sis_wait_thread_destroy(queue_->work_thread);
	}
	sis_net_queue_clear(queue_);
    // printf("=== sis_net_queue_destroy. %d\n", queue_->count);
    sis_free(queue_);
}

s_sis_net_node *sis_net_queue_push(s_sis_net_queue *queue_, int cid_, s_sis_object *obj_)
{  
	s_sis_net_node *new_node = sis_net_node_create(cid_, obj_);
	sis_mutex_lock(&queue_->lock);
    if (queue_->live == NULL && queue_->count == 0)
    {
        queue_->live = new_node;
		sis_mutex_unlock(&queue_->lock); 
		return queue_->live;
    }
	if (!queue_->head)
	{
		queue_->head = new_node;
		queue_->tail = queue_->head;
	}
	else
	{			
		queue_->tail->next = new_node;
		queue_->tail = new_node;
	}		
	queue_->count++;
    sis_mutex_unlock(&queue_->lock); 
	return NULL;  
}
void sis_net_queue_stop(s_sis_net_queue *queue_)
{
    sis_mutex_lock(&queue_->lock);
	if (queue_->live)
	{
		sis_net_node_destroy(queue_->live);
		queue_->live = NULL;
	}
    sis_mutex_unlock(&queue_->lock);
	// printf("stop --- %d\n", queue_->count);
	sis_wait_thread_notice(queue_->work_thread);
	// printf("stop --- %d\n", queue_->count);
}
/////////////////////////////////////////////////
//  
/////////////////////////////////////////////////

void sis_socket_close_handle(uv_handle_t *handle_, uv_close_cb close_cb_)
{
	if (uv_is_active(handle_))
	{
		uv_read_stop((uv_stream_t*)handle_);
	}
	if (!uv_is_closing(handle_))
	{
		uv_close(handle_, close_cb_);
	}
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
// s_sis_socket_session
//////////////////////////////////////////////////////////////

s_sis_socket_session *sis_socket_session_create(s_sis_socket_server *server_, int sid_)
{
	s_sis_socket_session *session = SIS_MALLOC(s_sis_socket_session, session); 
	session->sid = sid_;
	session->server = server_;

	session->work_handle = SIS_MALLOC(uv_tcp_t,  session->work_handle);
	session->work_handle->data = session;

	session->_uv_send_async_nums = 0;
	session->_uv_recv_async_nums = 0;

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

void sis_socket_session_destroy(void *session_)
{
	s_sis_socket_session *session = (s_sis_socket_session *)session_;
	if (session->read_buffer.base)
	{
		sis_free(session->read_buffer.base);
	}
	session->read_buffer.base = NULL;
	session->read_buffer.len = 0;

	if (session->write_buffer.base)
	{
		sis_object_decr((s_sis_object *)session->write_buffer.base);
		session->write_buffer.base = NULL;
		session->write_buffer.len = 0;
	}

	sis_free(session->work_handle);
	session->work_handle = NULL;
	sis_free(session);
}
//////////////////////////////////////////////////////////////
// s_sis_socket_server
//////////////////////////////////////////////////////////////
static int cb_server_write(void *);

s_sis_socket_server *sis_socket_server_create()
{
	s_sis_socket_server *server = SIS_MALLOC(s_sis_socket_server, server); 

	server->loop = sis_malloc(sizeof(uv_loop_t));  
	assert(0 == uv_loop_init(server->loop));

	server->isinit = false;
	server->cb_source = server;
	server->cb_connected_s2c = NULL;
	server->cb_disconnect_s2c = NULL;

	server->sessions = sis_index_list_create(1024);	
	server->sessions->vfree = sis_socket_session_destroy;
	server->sessions->wait_sec = 100; // 300

	server->isexit = false;
	
	server->write_list = sis_net_queue_create(server, cb_server_write, 333);
	
	LOG(8)("server create.[%p]\n", server);
	return server; 
}
void sis_socket_server_destroy(s_sis_socket_server *server_)
{
	if (server_->isexit)
	{
		return ;
	}
	server_->isexit = true;
	
	sis_net_queue_destroy(server_->write_list);

	sis_socket_server_close(server_);
	// 等待所有的客户连接断开
	while (sis_index_list_uses(server_->sessions) > 0)
	{
		LOG(8)("wait session.\n");
		sis_sleep(1000);
	}
	LOG(5)("server exit.\n");
	sis_index_list_destroy(server_->sessions);
	uv_loop_close(server_->loop); 	
	sis_free(server_->loop);
	sis_free(server_);
}

static void cb_session_closed(uv_handle_t *handle)
{
	s_sis_socket_session *session = (s_sis_socket_session *)handle->data;
	// 再删除
	sis_index_list_del(session->server->sessions, session->sid - 1);

	LOG(5)("server of session close ok.[%p] %d\n", session, session->sid);
}

void sis_socket_server_close(s_sis_socket_server *server_)
{ 
	if (!server_->isinit) 
	{
		return ;
	}
	int index = sis_index_list_first(server_->sessions);
	while (index >= 0)
	{
		s_sis_socket_session *session = (s_sis_socket_session *)sis_index_list_get(server_->sessions, index);
		
		// sis_socket_close_handle((uv_handle_t*)&session->write_async, NULL);
		sis_socket_close_handle((uv_handle_t*)session->work_handle, cb_session_closed);
		// uv_shutdown_t shutdown_req;
		// uv_shutdown(&shutdown_req, (uv_stream_t*)session->work_handle, NULL);
		index = sis_index_list_next(server_->sessions, index);
	}
	// printf("server 0 close [%d] %d\n", server_->loop->active_handles, server_->loop->active_reqs.count);
	// if (!uv_is_closing((uv_handle_t *) &server_->server_handle))
	// {
	// 	uv_close((uv_handle_t*) &server_->server_handle, NULL);
	// }
	sis_socket_close_handle((uv_handle_t *)&server_->server_handle, NULL);
	// uv_shutdown_t shutdown_req;
	// uv_shutdown(&shutdown_req, (uv_stream_t*)&server_->server_handle, NULL);	
	// uv_tcp_close_reset((uv_tcp_t *)&server_->server_handle, NULL);

	// 最好的方法就是把所有的active都关闭了
	// uv_clear_active_handles(server_->loop);

	// 检查是否有未释放的句柄
	// if ((int)server_->loop->active_handles > 0)
	// {
	// 	FILE *file = fopen("hserver.txt", "aw+");
	// 	fprintf(file, "---- %d - %d ---\n", sis_time_get_idate(0), sis_time_get_itime(0));
	// 	// uv_print_active_handles(server_->loop, file);
	// 	uv_print_all_handles(server_->loop, file);
	// 	fclose(file);
	// }
	LOG(5)("server close. %d %d\n", server_->loop->active_handles, server_->loop->active_reqs.count);

	uv_stop(server_->loop); 
	
	if (server_->server_thread_handle)
	{
		uv_thread_join(&server_->server_thread_handle);  
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
	if (!server->loop) 
	{
		return false;
	}
	assert(0 == uv_loop_init(server->loop));

	int o = uv_tcp_init(server->loop, &server->server_handle);
	if (o) 
	{
		return false;
	}
	server->isinit = true;
	server->server_handle.data = server;
	return true;
}

void _thread_server(void* arg)
{
	s_sis_socket_server *server = (s_sis_socket_server*)arg;
	LOG(5)("server thread start.[%p]\n", server);
	uv_run(server->loop, UV_RUN_DEFAULT);
	LOG(5)("server thread stop. [%d]\n", server->isinit);
    _uv_exit_loop(server->loop);	
	server->server_thread_handle = 0;
}
bool _sis_socket_server_start(s_sis_socket_server *server)
{
	int o = uv_thread_create(&server->server_thread_handle, _thread_server, server);
	if (o) 
	{
		return false;
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
	o = uv_tcp_bind(&server->server_handle, (const struct sockaddr*)&bind_addr, 0);
	if (o) 
	{
		return false;
	}
	LOG(5)("server bind %s:%d.\n", server->ip, server->port);
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
	o = uv_tcp_bind(&server->server_handle, (const struct sockaddr*)&bind_addr, 0);
	if (o) 
	{
		return false;
	}
	LOG(5)("server bind %s:%d.\n", server->ip, server->port);
	return true;
}
//////////////////////////////////////////////////////////////////
// uv_async 如果多个线程同时发出信号 偶尔会漏掉一个信号
// 造成接收数据中断因此只能以单个队列处理所有多线程发送来的数据
// 尤其在多个CPU环境下 此种现象很常见 

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
		printf("---- null .\n");
		return;
	}
	s_sis_socket_session *session = (s_sis_socket_session *)handle->data; //服务器的recv带的是 session
	s_sis_socket_server *server = (s_sis_socket_server *)session->server;
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
	// LOG(5)("recv .%d. %p %p\n", (int)nread, session, session->cb_recv_after);
	if (nread > 0 && session->cb_recv_after) 
	{
		buffer->base[nread] = 0;
		session->cb_recv_after(server->cb_source, session->sid, buffer->base, nread);
	}
}

static void cb_server_new_connect(uv_stream_t *handle, int status)
{
	s_sis_socket_server *server = (s_sis_socket_server *)handle->data;

	int index = sis_index_list_new(server->sessions);
	if (index < 0)
	{
		LOG(5)("server connect nums > %d. close.\n", server->sessions->count);
		return ;
	}
	s_sis_socket_session *session = sis_socket_session_create(server, index + 1);
	sis_index_list_set(server->sessions, index, session);

	int o = uv_tcp_init(server->loop, session->work_handle); 
	if (o) 
	{
		sis_index_list_del(server->sessions, session->sid - 1);
		return;
	}
	o = uv_accept((uv_stream_t*)&server->server_handle, (uv_stream_t*)session->work_handle);
	if (o) 
	{		
		uv_close((uv_handle_t*)session->work_handle, cb_session_closed);
		return;
	}
	if (server->cb_connected_s2c) 
	{
		server->cb_connected_s2c(server->cb_source, session->sid);
	}
	
	uv_read_start((uv_stream_t*)session->work_handle, cb_server_read_alloc, cb_server_read_after);
	
	LOG(5)("new session ok. %p id = %d count = %d \n", session, session->sid, server->sessions->count);
	//服务器开始接收客户端的数据
	return ;
}

bool _sis_socket_server_listen(s_sis_socket_server *server)
{
	int o = uv_listen((uv_stream_t*)&server->server_handle, SOMAXCONN, cb_server_new_connect);
	if (o) 
	{
		return false;
	}
	LOG(5)("server listen %s:%d.\n", server->ip, server->port);
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

static void cb_server_write_after(uv_write_t *requst, int status)
{	
	s_sis_socket_server *server = (s_sis_socket_server *)requst->data;
	s_sis_net_node *node = server->write_list->live;
	s_sis_socket_session *session = sis_index_list_get(server->sessions, node->cid);
	if (!session) 
	{
		// session 已经退出
		printf("cb_server_write_after ------%d \n", server->write_list->count);
		sis_net_queue_stop(server->write_list);
		return ;
	}
	if (status < 0) 
	{
		LOG(5)("server write fail: %s.\n", uv_strerror(status));
		// 应该关闭该连接
		sis_socket_server_delete(server, session->sid); //连接断开，关闭客户端
		sis_net_queue_stop(server->write_list);
		return;
	}
#ifdef _UV_DEBUG_
	_uv_write_nums++;
	if (_uv_write_nums % 1000 == 0)
	LOG(5)("uv write stop:  nums :%d delay : %lld \n", _uv_write_nums, sis_time_get_now_msec() - _uv_write_msec);
#endif
	// 通知下一个
	sis_net_queue_stop(server->write_list);
	if (session->cb_send_after)
	{
		session->cb_send_after(server->cb_source, session->sid, status);
	} 
}
void _cb_server_async_write(uv_async_t* handle)
{
	s_sis_socket_server *server = (s_sis_socket_server *)handle->data;
	s_sis_net_node *node = server->write_list->live;

	// sis_socket_close_handle((uv_handle_t*)handle, NULL);
	if (!uv_is_closing((uv_handle_t *)handle))
	{
		uv_close((uv_handle_t*)handle, NULL);	//如果async没有关闭，消息队列是会阻塞的
	}

	s_sis_socket_session *session = sis_index_list_get(server->sessions, node->cid);
	if (!session) 
	{
		// session 已经退出
		printf("_cb_server_async_write ------%d \n", server->write_list->count);
		sis_net_queue_stop(server->write_list);
		return ;
	}
	uv_buf_t buffer = uv_buf_init(SIS_OBJ_GET_CHAR(node->obj), SIS_OBJ_GET_SIZE(node->obj));
	session->write_req.data = server;	
#ifdef _UV_DEBUG_	
	session->_uv_recv_async_nums++;
#endif
	// printf("_cb_server_async_write %p  %d %d\n",session , session->sid, node->cid);
	int o = uv_write(&session->write_req, (uv_stream_t*)session->work_handle, &buffer, 1, cb_server_write_after);
	if (o) 
	{
		sis_socket_server_delete(server, session->sid);
		sis_net_queue_stop(server->write_list);
		LOG(5)("server write fail.\n");
	}
	// if (!uv_is_closing((uv_handle_t *)handle))
	// {
	// 	uv_close((uv_handle_t*)handle, NULL);	//如果async没有关闭，消息队列是会阻塞的
	// }
}

static int cb_server_write(void *source_)
{
	s_sis_socket_server *server = (s_sis_socket_server *)source_;
	s_sis_net_node *node = server->write_list->live;
	s_sis_socket_session *session = sis_index_list_get(server->sessions, node->cid);
	if (!session) 
	{
		printf("cb_server_write ------%d \n", server->write_list->count);
		// session 已经退出 处理下一条
		sis_net_queue_stop(server->write_list);
		// printf("cb_server_write ------%d \n", server->write_list->count);
		return 0;
	}
	// printf("cb_server_write %p  %d %d\n",session , session->sid, node->cid);
#ifdef _UV_DEBUG_	
	if (_uv_write_nums == 0)
	{
		_uv_write_msec = sis_time_get_now_msec();
	}
	if (_uv_write_nums % 1000 == 0)
	{
		LOG(5)("uv write start. nums :%d.\n", _uv_write_nums);
		_uv_write_msec = sis_time_get_now_msec();

		int index = sis_index_list_first(server->sessions);
		while(index >= 0)
		{
			s_sis_socket_session *unit = (s_sis_socket_session *)sis_index_list_get(server->sessions, index);
			if (unit->_uv_recv_async_nums != unit->_uv_send_async_nums)
			{
				LOG(2)("async %d != %d \n", unit->_uv_recv_async_nums, unit->_uv_send_async_nums);
			}
			else			
			{
				LOG(5)("async %d != %d \n", unit->_uv_recv_async_nums, unit->_uv_send_async_nums);
			}
			index = sis_index_list_next(server->sessions, index);
		}
	}
	session->_uv_send_async_nums++;
#endif
	session->write_async.data = server;
	int o = uv_async_init(server->loop, &session->write_async, _cb_server_async_write);
	if (o) 
	{
		LOG(5)("init async fail.\n");
		// 失败就断开连接
		sis_socket_server_delete(server, session->sid);
		sis_net_queue_stop(server->write_list);
		return 0;
	}
	// 垃圾东西 多CPU写入居然某些有人收不到信号 不是号称线程安全吗！！！
	o = uv_async_send(&session->write_async);	
	if (o) 
	{
		LOG(5)("send async fail.\n");
		// 失败就断开连接
		sis_socket_server_delete(server, session->sid);
		sis_net_queue_stop(server->write_list);
		return 0;
	}
	return 1;
}

//服务器发送函数
bool sis_socket_server_send(s_sis_socket_server *server, int sid_, s_sis_object *inobj_)
{
	if (server->isexit)
	{
		return false;
	}
	if (!inobj_) 
	{
		return false;
	}
	s_sis_socket_session *session = sis_index_list_get(server->sessions, sid_ - 1);
	if (!session)
	{
		LOG(5)("can't find client %d.\n", sid_);
		return false;
	}
	// 放入队列就返回 其他交给内部处理
	if (sis_net_queue_push(server->write_list, sid_ - 1, inobj_))
	{
		cb_server_write(server);
	}

	return true;
}
 
void sis_socket_server_set_rwcb(s_sis_socket_server *server, int sid_, 
	cb_socket_recv_after cb_recv_, cb_socket_send_after cb_send_)
{
	s_sis_socket_session *session = sis_index_list_get(server->sessions, sid_ - 1);
	if (session)
	{
		sis_socket_session_set_rwcb(session, cb_recv_, cb_send_);
	}
}
//服务器-新链接回调函数
void sis_socket_server_set_cb(s_sis_socket_server *server, 
	cb_socket_connect cb_connected_,
	cb_socket_connect cb_disconnect_)
{
	server->cb_connected_s2c = cb_connected_;
	server->cb_disconnect_s2c = cb_disconnect_;
}
bool sis_socket_server_delete(s_sis_socket_server *server, int sid_)
{
	//  先发断开回调，再去关闭
	s_sis_socket_session *session = sis_index_list_get(server->sessions, sid_ - 1);
	if (!session)
	{
		LOG(5)("can't find client.[%d]\n", sid_);
		return false;
	}
	if (session->write_stop == 1)
	{
		// 已经发出关闭信号 避免二次进入
		return true;
	}
	session->write_stop = 1;
	if (server->cb_disconnect_s2c) 
	{
		server->cb_disconnect_s2c(server->cb_source, sid_);
	}
	
	// sis_socket_close_handle((uv_handle_t*)&session->write_async, NULL);
	sis_socket_close_handle((uv_handle_t*)session->work_handle, cb_session_closed);

	LOG(5)("delete session.[%d == %d] %p\n", session->sid, sid_, session);

	return true;	
}


/////////////////////////////////////////////////////////////
// s_sis_socket_client define 
/////////////////////////////////////////////////////////////
void _thread_keep_connect(void* arg)
{
	s_sis_socket_client *client = (s_sis_socket_client*)arg;

	LOG(5)("client keep connect thread start. [%p]\n", client);
	client->keep_connect_status = SIS_UV_CONNECT_WORK;	
	int count = 0;
	while (!(client->keep_connect_status == SIS_UV_CONNECT_EXIT))
	{
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
	LOG(5)("client keep connect thread stop. [%d]\n", client->work_status);
}
static int cb_client_write(void *);

s_sis_socket_client *sis_socket_client_create()
{
	s_sis_socket_client *client = SIS_MALLOC(s_sis_socket_client, client); 

	client->loop = sis_malloc(sizeof(uv_loop_t));  
	assert(0 == uv_loop_init(client->loop));

	client->isinit = false;
	client->cb_recv_after = NULL;
	client->cb_source = client;

	client->cb_connected_c2s = NULL;
	client->cb_disconnect_c2s = NULL;

	client->read_buffer = uv_buf_init((char*)sis_malloc(MAX_NET_UV_BUFFSIZE), MAX_NET_UV_BUFFSIZE);
    client->write_buffer = uv_buf_init(NULL, 0);

	client->write_req.data = client;
	client->connect_req.data = client;

	client->work_status = SIS_UV_CONNECT_NONE;

	client->isexit = false;

	client->write_list = sis_net_queue_create(client, cb_client_write, 333);

	if (uv_thread_create(&client->keep_connect_thread_handle, _thread_keep_connect, client)) 
	{
		LOG(5)("create keep connect fail.\n");
	}
	return client;
}
void sis_socket_client_destroy(s_sis_socket_client *client_)
{	
	if (client_->isexit)
	{
		return ;
	}
	client_->isexit = true;
	
	sis_net_queue_destroy(client_->write_list);

	client_->keep_connect_status = SIS_UV_CONNECT_EXIT; 
	uv_thread_join(&client_->keep_connect_thread_handle);  

	sis_socket_client_close(client_);

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
	LOG(5)("client exit.\n");
}

void cb_client_close(uv_handle_t *handle)
{
	s_sis_socket_client *client = (s_sis_socket_client*)handle->data;
	
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
		return ;
	}
	if (client_->work_status & SIS_UV_CONNECT_EXIT)
	{
		return ; // 已经关闭过
	}
	client_->work_status = SIS_UV_CONNECT_EXIT;
	// if (client_->work_status == SIS_UV_CONNECT_WORK)
	{
		// sis_socket_close_handle((uv_handle_t*)&client_->write_async, NULL);
		sis_socket_close_handle((uv_handle_t*)&client_->client_handle, cb_client_close);
	}
	
	// sis_memory_clear(client_->read_buffer);

	sis_net_queue_clear(client_->write_list);
	// 检查是否有未释放的句柄
	// if ((int)client_->loop->active_handles > 0)
	// {
	// 	FILE *file = fopen("hclient.txt", "aw+");
	// 	fprintf(file, "---- %d - %d ---\n", sis_time_get_idate(0), sis_time_get_itime(0));
	// 	// uv_print_active_handles(client_->loop, file);
	// 	uv_print_all_handles(client_->loop, file);
	// 	fclose(file);
	// }

	// LOG(5)("client close. %d %d\n", client_->loop->active_handles, client_->loop->active_reqs);

	uv_stop(client_->loop); 

	uv_thread_join(&client_->client_thread_handle);  

	client_->isinit = false;

	LOG(5)("client close ok.\n");
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
		sis_socket_client_close(client);
		return;
	}

	if (nread > 0 && client->cb_recv_after) 
	{
		buffer->base[nread] = 0;
		client->cb_recv_after(client->cb_source, 0, buffer->base, nread);
	}
}

static void _cb_after_connect(uv_connect_t *handle, int status)
{
	s_sis_socket_client *client = (s_sis_socket_client *)handle->handle->data;
	if (status)
	{
		LOG(8)("client connect error: %s.\n", uv_strerror(status));
		sis_socket_close_handle((uv_handle_t*)&client->client_handle, NULL);
		return;
	}
	int o = uv_read_start(handle->handle, cb_client_read_alloc, cb_client_read_after); //客户端开始接收服务器的数据
	if (o)
	{
		LOG(5)("client cannot start read: %s.\n", uv_strerror(status));
		sis_socket_close_handle((uv_handle_t*)&client->client_handle, NULL);
		return;
	}
	client->work_status = SIS_UV_CONNECT_WORK;
	if (client->cb_connected_c2s) 
	{
		client->cb_connected_c2s(client->cb_source, 0);
	}
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
	if (!client_->loop) 
	{
		return false;
	}
	int o = uv_loop_init(client_->loop); 
	if (o)
	{
		LOG(5)("loop init fail.[%d] %s %s\n", o, uv_err_name(o), uv_strerror(o));
		return false;
	}
	o = uv_tcp_init(client_->loop, &client_->client_handle);
	if (o) 
	{
		return false;
	}
	client_->isinit = true;
	client_->client_handle.data = client_;

	return true;
}

void _thread_client(void* arg)
{
	s_sis_socket_client *client = (s_sis_socket_client*)arg;
	LOG(5)("client connect thread start. [%p]\n", client);

	struct sockaddr_in bind_addr;
	int o = uv_ip4_addr(client->ip, client->port, &bind_addr);
	if (o) 
	{
		return;
	}
	o = uv_tcp_init(client->loop, &client->client_handle);
  	if (o) 
	{
		return ;
	}
	o = uv_tcp_connect(&client->connect_req, &client->client_handle, (const struct sockaddr*)&bind_addr, _cb_after_connect);
	if (o) 
	{
		return ;
	}	
	uv_run(client->loop, UV_RUN_DEFAULT); 
	// 关闭时问题可参考 test-tcp-close-reset.c
    _uv_exit_loop(client->loop);
	LOG(5)("client connect thread stop. [%d]\n", client->work_status);
}
bool sis_socket_client_open(s_sis_socket_client *client_)
{
	client_->work_status = SIS_UV_CONNECT_WAIT;
	if (!_sis_socket_client_init(client_)) 
	{
		return false;
	}
	LOG(5)("client connect %s:%d status = %d \n", client_->ip, client_->port, client_->work_status);
	int o = uv_thread_create(&client_->client_thread_handle, _thread_client, client_);
	//触发 _cb_after_connect 才算真正连接成功，所以用线程
	if (o) 
	{
		return false;
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
	o = uv_tcp_init(client->loop, &client->client_handle);
  	if (o) 
	{
		return ;
	}
	o = uv_tcp_connect(&client->connect_req, &client->client_handle, (const struct sockaddr*)&bind_addr, _cb_after_connect);
	if (o) 
	{
		return ;
	}
	uv_run(client->loop, UV_RUN_DEFAULT); 
	_uv_exit_loop(client->loop);
	LOG(5)("client connect6 thread stop. [%d]\n", client->work_status);
}
bool sis_socket_client_open6(s_sis_socket_client *client_)
{
	client_->work_status = SIS_UV_CONNECT_WAIT;
	if (!_sis_socket_client_init(client_)) 
	{
		return false;
	}

	LOG(5)("client connect %s:%d status = %d \n", client_->ip, client_->port, client_->work_status);
	int o = uv_thread_create(&client_->client_thread_handle, _thread_connect6, client_);
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
	client_->cb_recv_after = cb_recv_;
	client_->cb_send_after = cb_send_;
}

static void cb_client_write_after(uv_write_t *requst, int status)
{
	s_sis_socket_client *client = (s_sis_socket_client *)requst->data;
	if (status < 0) 
	{
		LOG(5)("client write error: %s.\n", uv_strerror(status));
		sis_socket_client_close(client);
		sis_net_queue_stop(client->write_list);
		return;
	}
	sis_net_queue_stop(client->write_list);
	if (client->cb_send_after)
	{
		client->cb_send_after(client->cb_source, 0, status);
	} 
}

void _cb_client_async_write(uv_async_t* handle)
{
	s_sis_socket_client *client = (s_sis_socket_client *)handle->data;	
	s_sis_net_node *node = client->write_list->live;

	if (!uv_is_closing((uv_handle_t *)handle))
	{
		uv_close((uv_handle_t*)handle, NULL);	//如果async没有关闭，消息队列是会阻塞的
	}

	if (!node || (client->work_status & SIS_UV_CONNECT_EXIT))
	{
		LOG(5)("client close. no data.\n");
		return ;
	}

	uv_buf_t buffer = uv_buf_init(SIS_OBJ_GET_CHAR(node->obj), SIS_OBJ_GET_SIZE(node->obj));
	client->write_req.data = client;
	int o = uv_write(&client->write_req, (uv_stream_t*)&client->client_handle, &buffer, 1, cb_client_write_after);
	if (o) 
	{
		sis_socket_client_close(client);
		sis_net_queue_stop(client->write_list);
		LOG(5)("client write fail.\n");
	}
	// sis_socket_close_handle((uv_handle_t*)handle, NULL);
}
static int cb_client_write(void *source_)
{
	s_sis_socket_client *client = (s_sis_socket_client *)source_;
	client->write_async.data = client;
	int o = uv_async_init(client->loop, &client->write_async, _cb_client_async_write);
	if (o) 
	{
		LOG(5)("init async fail.\n");
		sis_socket_client_close(client);
		sis_net_queue_stop(client->write_list);
		return 0;
	}

	o = uv_async_send(&client->write_async);
	if (o) 
	{
		LOG(5)("send async fail.\n");
		sis_socket_client_close(client);
		sis_net_queue_stop(client->write_list);
		return 0;
	}
	return 1;
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
	// 放入队列就返回 其他交给内部处理
	if (sis_net_queue_push(client_->write_list, 0, inobj_))
	{
		cb_client_write(client_);
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
#define SPEED_SERVER 1 
// 测试服务端发送数据的速度 client记录收到字节数
// 经过测试比实际带宽慢10倍
size_t speed_send_size = 0;
size_t speed_recv_size = 0;
size_t speed_recv_curr = 0;
msec_t speed_recv_msec = 0;

#define ACTIVE_CLIENT 1 // client主动发请求

int sno = 0;
static void cb_server_recv_after(void *handle_, int sid_, char* in_, size_t ilen_)
{
	// printf("server recv [%d]:%zu [%s]\n", sid_, ilen_, in_);	
#ifdef ACTIVE_CLIENT
	if (sno == 0)
	{	sno = 1;
		s_sis_socket_server *srv = (s_sis_socket_server *)handle_;
		s_sis_sds str = sis_sdsempty();
		str = sis_sdscatfmt(str,"server recv cid =  %i", sid_);
		s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, str);
		sis_socket_server_send(srv, sid_, obj);
		sis_object_destroy(obj);
	}
#endif
}
static void cb_client_send_after(void* handle_, int cid, int status)
{
	s_test_client *cli = (s_test_client *)handle_;
	// if (cli->sno < 10*1000)
	// {
	// 	cli->sno++;
	// 	s_sis_sds str = sis_sdsempty();
	// 	str = sis_sdscatfmt(str,"i am client. [sno = %i]. %i", cli->sno, (int)sis_time_get_now());
	// 	s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, str);
	// 	sis_socket_client_send(cli->client, obj);
	// 	sis_object_destroy(obj);
	// }
}
#ifdef SPEED_SERVER
static void cb_client_recv_after(void* handle_, int cid, char* in_, size_t ilen_)
{
	speed_recv_size += ilen_;
	if (speed_recv_msec == 0) 
	{
		speed_recv_msec = sis_time_get_now_msec();
	}
	else 
	{
		int offset = sis_time_get_now_msec() - speed_recv_msec;
		if (sis_time_get_now_msec() - speed_recv_msec > 1000)
		{
			speed_recv_msec = sis_time_get_now_msec();
			printf("cost : %5d recv : %12zu, %10zu speed(k/s): %d\n", offset, speed_recv_size, speed_recv_curr, (int)speed_recv_curr/offset);
			speed_recv_curr = 0; 
		}
	}
	speed_recv_curr += ilen_;
}


static void cb_new_connect(void *handle_, int sid_)
{
	printf("new connect . sid_ = %d \n", sid_);	
	s_sis_socket_server *srv = (s_sis_socket_server *)handle_;
	sis_socket_server_set_rwcb(srv, sid_, cb_server_recv_after, NULL);
	int count = 1000*1000;
	int sendsize = 16384;
	s_sis_sds str = sis_sdsnewlen(NULL, sendsize);
	s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, str);
	for (int i = 0; i < count; i++)
	{
		sis_socket_server_send(srv, sid_, obj);
	}
	sis_object_destroy(obj);
	printf("send end. %d \n", sid_);	
}

static void cb_connected(void *handle_, int sid_)
{
	printf("client connect\n");	
}
#else
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
	sis_socket_server_set_rwcb(srv, sid_, cb_server_recv_after, NULL);
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
	s_sis_sds str = sis_sdsempty();
	str = sis_sdscatfmt(str,"i am client. [sno = %i].", cli->sno);
	s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, str);
	// printf("send : %s\n", str);	
	// for (int i = 0; i < 10000; i++)
	{
		sis_socket_client_send(cli->client, obj);
	}
	// sis_socket_client_send(cli->client, obj);
	sis_object_destroy(obj);
#endif
}
#endif

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
			client[i]->client->cb_source = client[i];

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

#define SPEED_SERVER 1 
// 测试服务端发送数据的速度 client记录收到字节数
// 经过测试比实际带宽慢10倍
size_t speed_send_size = 0;
size_t speed_recv_size = 0;
size_t speed_recv_curr = 0;
msec_t speed_recv_msec = 0;

int __exit = 0;
uv_loop_t *server_loop = NULL;
uv_loop_t *client_loop = NULL;

uv_tcp_t    server_handle;	
uv_thread_t server_thread;

uv_tcp_t    client_handle;	
uv_thread_t client_thread;

uv_buf_t read_buffer;
uv_buf_t write_buffer;


static void cb_client_recv_after(uv_stream_t *handle, ssize_t nread, const uv_buf_t* buffer)
{
	if (nread < 0) 
	{
		exit(100);
	}
	speed_recv_size += nread;
	if (speed_recv_msec == 0) 
	{
		speed_recv_msec = sis_time_get_now_msec();
	}
	else 
	{
		int offset = sis_time_get_now_msec() - speed_recv_msec;
		if (sis_time_get_now_msec() - speed_recv_msec > 1000)
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
	if (suggested_size > read_buffer.len)
	{
		sis_free(read_buffer.base);
		read_buffer.base = sis_malloc(suggested_size + 1024);
		read_buffer.len = suggested_size + 1024;
	}
	*buffer = read_buffer;
}
static void _cb_client_connect(uv_connect_t *handle, int status)
{
	if(uv_read_start(handle->handle, cb_client_recv_before, cb_client_recv_after)) 
		exit(2);

}
int count = 0;
int maxcount = 1000*1000*10;
int sendsize = 16384;
uv_write_t write_req;

#define THREAD_WRITE 1
#ifndef THREAD_WRITE
// 直接写可以跑满带宽
static void cb_server_write_ok(uv_write_t *requst, int status)
{
	if (count > maxcount)
	{
		printf("send ok\n");
		return ;
	}
	if(uv_write(&write_req, (uv_stream_t*)&client_handle, &write_buffer, 1, cb_server_write_ok))
	{
		exit(4);
	}
	count++;
}

static void cb_new_connect(uv_stream_t *handle, int status)
{
	printf("new connect . sid_ = 1 \n");	
	if(uv_tcp_init(server_loop, &client_handle)) exit(2);

	if (uv_accept((uv_stream_t*)&server_handle, (uv_stream_t*)&client_handle)) exit(3);
	
	count = 0;
	// 直接写数据基本能跑满 10G - 7G
	cb_server_write_ok(NULL, 0);
}
#else
// 这样写也是可以的 做好同步就行了 写入就发送 发送完再读数据有数据就处理 没数据就等待
// 无论如何libuv上次数据传输完毕后 必须一次性把临时缓存中数据全部发出去 否则速度慢
// 但奇怪的是如果在发送成功回调中发送数据 就很快 怀疑是如果不在在回调中发送数据 就会进入消息队列
// 而我再次写入数据时，消息队列可能有未知的耗时操作导致速度慢了
// 无论如何 要求只要发送就必须把需要发送的一次性推出去，否则速度会很慢
// 这样又要求每个客户端自己保存自己的数据 有点痛苦 等空的时候换掉他 

s_sis_struct_list  *write_list;
s_sis_mutex_t       mmmm;
s_sis_wait_thread  *work_thread;
volatile int may_write = 0;

static void cb_server_write_ok(uv_write_t *requst, int status);
static void _write_one()
{
	may_write = 0;
	printf("=1=%d\n", may_write);
	// if(uv_write(&write_req, (uv_stream_t*)&client_handle, &write_buffer, 1, cb_server_write_ok))
	if(uv_write(&write_req, (uv_stream_t*)&client_handle, 
		sis_struct_list_first(write_list), write_list->count, cb_server_write_ok))
	{
		printf("write fail\n");
	}
}

static void cb_server_write_ok(uv_write_t *requst, int status)
{
	may_write = 1;
	printf("=2=%d\n", may_write);
	// _write_one();
	// sis_wait_thread_notice(work_thread);
}

void _thread_read1(void* arg)
{
	// return ;
	while (1)
    {
		if (may_write == 1)
		{
			printf("=3=%d %d\n", may_write, write_list->count);
			if (write_list->count > 0)
			{
				// sis_mutex_lock(&mmmm);
				// sis_struct_list_pop(write_list);
				_write_one();
				sis_struct_list_clear(write_list);
				// sis_mutex_unlock(&mmmm);
			}
		}
		else
		{
			// printf("%d\n", may_write);
		}
	}
}
// void _thread_read(void* arg)
// {
// 	sis_wait_thread_start(work_thread);
// 	while (sis_wait_thread_noexit(work_thread))
//     {
// 		if (may_write == 1)
// 		{
// 			if (write_list->count > 0)
// 			{
// 				// sis_mutex_lock(&mmmm);
// 				// sis_struct_list_pop(write_list);
// 				_write_one();
// 				// sis_mutex_unlock(&mmmm);
// 			}
// 		}
// 		sis_sleep(1);
// 		// printf("timeout exit. %d \n", 1);
// 		// if (sis_wait_thread_wait(work_thread, 50) == SIS_WAIT_TIMEOUT)
// 		// {
// 		// 	printf("timeout exit. %d \n", 2);
// 		// }  
// 	}
// 	sis_wait_thread_stop(work_thread);
// }
void _thread_write(void* arg)
{
	uv_buf_t buffer;
	buffer.base = write_buffer.base;
	buffer.len = write_buffer.len;
	for (int i = 0; i < maxcount; i++)
	{
		// sis_mutex_lock(&mmmm);
		sis_struct_list_push(write_list, &buffer);
		// sis_mutex_unlock(&mmmm);
	}
	may_write = 1;
	// cb_server_write_ok(NULL, 0);
	// sis_wait_thread_notice(work_thread);
	// sis_mutex_unlock(&mmmm);
	printf("write count = %d %d\n", may_write, write_list->count);
}

static void cb_new_connect(uv_stream_t *handle, int status)
{
	printf("new connect . sid_ = 1 \n");	
	if(uv_tcp_init(server_loop, &client_handle)) exit(2);

	if (uv_accept((uv_stream_t*)&server_handle, (uv_stream_t*)&client_handle)) exit(3);
	
	// may_write = 1;
	count = 0;
	// 启动一个线程 写看看速度
	write_list = sis_struct_list_create(sizeof(uv_buf_t));
	sis_struct_list_set_size(write_list, maxcount + 1);
	
	uv_thread_create(&server_thread, _thread_write, NULL);
	uv_thread_create(&client_thread, _thread_read1, NULL);
}
#endif
int main(int argc, char **argv)
{
	if (argc < 2)
	{
#ifdef THREAD_WRITE
		sis_mutex_create(&mmmm);
		// work_thread = sis_wait_thread_create(50);
		// if (!sis_wait_thread_open(work_thread, _thread_read, NULL))
		// {
		// 	LOG(1)("can't start reader thread.\n");
		// 	return NULL;
		// }
#endif
		// it is server
		write_buffer = uv_buf_init((char*)sis_malloc(sendsize), sendsize);
		server_loop = sis_malloc(sizeof(uv_loop_t)); 
		uv_loop_init(server_loop);


		if(uv_tcp_init(server_loop, &server_handle)) exit(0);


		struct sockaddr_in bind_addr;
		if(uv_ip4_addr("192.168.3.118", 7777, &bind_addr)) exit(0);

		if(uv_tcp_bind(&server_handle, (const struct sockaddr*)&bind_addr, 0)) exit(0);
		
		if(uv_listen((uv_stream_t*)&server_handle, SOMAXCONN, cb_new_connect)) exit(0);
		printf("server ok\n");
		uv_run(server_loop, UV_RUN_DEFAULT);

		uv_loop_close(server_loop); 		
	}
	else
	{
		read_buffer = uv_buf_init((char*)sis_malloc(sendsize), sendsize);

		client_loop = sis_malloc(sizeof(uv_loop_t)); 
		uv_loop_init(client_loop);

		if (uv_tcp_init(client_loop, &client_handle)) 
		{
			printf("==0==\n");
			return 0;
		}
		struct sockaddr_in bind_addr;
		if (uv_ip4_addr("192.168.3.118", 7777, &bind_addr)) 
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
// sendfile //

#include <stdio.h>
#include <uv.h>
#include <stdlib.h>
 
uv_loop_t *loop;
#define DEFAULT_PORT 7000
 
uv_tcp_t mysocket;
 
char *path = NULL;
uv_buf_t iov;
char buffer[128];
 
uv_fs_t read_req;
uv_fs_t open_req;
void on_read(uv_fs_t *req);
void on_write(uv_write_t* req, int status)
{
    if (status < 0) 
    {
        fprintf(stderr, "Write error: %s\n", uv_strerror(status));
        uv_fs_t close_req;
        uv_fs_close(uv_default_loop(), &close_req, open_req.result, NULL);
        uv_close((uv_handle_t *)&mysocket,NULL);
        exit(-1);
    }
    else 
    {
        uv_fs_read(uv_default_loop(), &read_req, open_req.result, &iov, 1, -1, on_read);
    }
}
 
void on_read(uv_fs_t *req)
{
    if (req->result < 0) 
    {
        fprintf(stderr, "Read error: %s\n", uv_strerror(req->result));
    }
    else if (req->result == 0) 
    {
        uv_fs_t close_req;
        // synchronous
        uv_fs_close(uv_default_loop(), &close_req, open_req.result, NULL);
        uv_close((uv_handle_t *)&mysocket,NULL);
    }
    else
    {
        iov.len = req->result;
        uv_write((uv_write_t *)req,(uv_stream_t *)&mysocket,&iov,1,on_write);
    }
}
 
void on_open(uv_fs_t *req)
{
    if (req->result >= 0) 
    {
        iov = uv_buf_init(buffer, sizeof(buffer));
        uv_fs_read(uv_default_loop(), &read_req, req->result,&iov, 1, -1, on_read);
    }
    else 
    {
        fprintf(stderr, "error opening file: %s\n", uv_strerror((int)req->result));
        uv_close((uv_handle_t *)&mysocket,NULL);
        exit(-1);
    }
}
 
void on_connect(uv_connect_t* req, int status)
{
    if(status < 0)
    {
        fprintf(stderr,"Connection error %s\n",uv_strerror(status));
 
        return;
    }
 
    fprintf(stdout,"Connect ok\n");
 
    uv_fs_open(loop,&open_req,path,O_RDONLY,-1,on_open);
 
 
}
 
int main(int argc,char **argv)
{
    if(argc < 2)
    {
        fprintf(stderr,"Invaild argument!\n");
 
        exit(1);
    }
    loop = uv_default_loop();
 
    path = argv[1];
 
    uv_tcp_init(loop,&mysocket);
 
    struct sockaddr_in addr;
 
    uv_connect_t* connect = (uv_connect_t*)malloc(sizeof(uv_connect_t));
 
 
    uv_ip4_addr("127.0.0.1",DEFAULT_PORT,&addr);
 
    int r = uv_tcp_connect(connect,&mysocket,(const struct sockaddr *)&addr,on_connect);
 
    if(r)
    {
        fprintf(stderr, "connect error %s\n", uv_strerror(r));
        return 1;
    }
 
    return uv_run(loop,UV_RUN_DEFAULT);
}
#endif
