
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

/////////////////////////////////////////////////
//  s_sis_net_uv_catch 
// 专门处理uv多线程写入时 async不能1对1返回的错误
/////////////////////////////////////////////////
/////////////////////////////////////////////////
//  s_sis_net_uv_node
/////////////////////////////////////////////////
s_sis_net_uv_node *sis_net_uv_node_create(int cid_)
{
    s_sis_net_uv_node *node = SIS_MALLOC(s_sis_net_uv_node, node);
	node->nodes = sis_net_mems_create();
	node->cid = cid_;
    node->next = NULL;
	node->prev = NULL;
    return node;    
}
void sis_net_uv_node_destroy(s_sis_net_uv_node *node_)
{
	printf("del nodes: %d\n", sis_net_mems_count(node_->nodes));
    sis_net_mems_destroy(node_->nodes);
	if (node_->next)
	{
		node_->next->prev = node_->prev;
	}
	if (node_->prev)
	{
		node_->prev->next = node_->next;
	}
    sis_free(node_);
	// safe_memory_stop();
}

void *_thread_net_reader(void *argv_)
{
    s_sis_net_uv_catch *wqueue = (s_sis_net_uv_catch *)argv_;
	sis_wait_thread_start(wqueue->work_thread);
    while (sis_wait_thread_noexit(wqueue->work_thread))
    {
        // printf("1 --- %p\n", wqueue->live);
		int islive = false;
		s_sis_net_uv_node *node = wqueue->work_node;
		if (node)
		{
			if (node->nodes->rsize > 0)
			{
				// 表示当前还未发送完毕 继续等着
			}
			else
			{
				// 已经发送完毕了 开始检查其他连接
				sis_mutex_lock(&wqueue->lock);
				s_sis_net_uv_node *next = sis_net_uv_catch_next(wqueue, node);
				while (next)
				{
					int count = sis_net_mems_read(next->nodes, 0);
					if (count > 0)
					{
						wqueue->work_node = next;
						islive = true;
						break;
					}
					if (next == node)
					{
						wqueue->work_node = NULL;
						break;
					}
					next = sis_net_uv_catch_next(wqueue, next);
				}
				sis_mutex_unlock(&wqueue->lock);
			}
		}
		if (islive)
		{
			if (wqueue->cb_reader)
            {
                wqueue->cb_reader(wqueue->source);
            }			
		}
        if (sis_wait_thread_wait(wqueue->work_thread, wqueue->work_thread->wait_msec) == SIS_WAIT_TIMEOUT)
        {
            // printf("timeout exit. %d \n", waitmsec);
        }     
    }
	// printf("5 --- %p\n", wqueue);
    sis_wait_thread_stop(wqueue->work_thread);
    return NULL;
}

s_sis_net_uv_catch *sis_net_uv_catch_create(
	void *source_, 
    cb_net_uv_reader *cb_reader_,
    int wait_msec_)
{
    s_sis_net_uv_catch *o = SIS_MALLOC(s_sis_net_uv_catch, o);
    sis_mutex_init(&o->lock, NULL);
    o->count = 0;
	o->source = source_;
 
	o->curr_node = NULL;
	o->work_node = NULL;

    o->cb_reader = cb_reader_;
	int wait_msec = wait_msec_ == 0 ? 300 : wait_msec_;
	o->work_thread = sis_wait_thread_create(wait_msec);
    // 最后再启动线程 顺序不能变 不然数据会等待
    if (!sis_wait_thread_open(o->work_thread, _thread_net_reader, o))
    {
        LOG(1)("can't start reader thread.\n");
        sis_net_uv_catch_destroy(o);
        return NULL;
    }
    return  o;
}
s_sis_net_uv_node *sis_net_uv_catch_tail(s_sis_net_uv_catch *queue_)
{
	s_sis_net_uv_node *uvnode = queue_->curr_node;
	while(uvnode && uvnode->next)
	{
		uvnode = uvnode->next;
	}
	return uvnode;
}
s_sis_net_uv_node *sis_net_uv_catch_head(s_sis_net_uv_catch *queue_)
{
	s_sis_net_uv_node *uvnode = queue_->curr_node;
	while(uvnode && uvnode->prev)
	{
		uvnode = uvnode->prev;
	}
	return uvnode;
}
s_sis_net_uv_node *sis_net_uv_catch_next(s_sis_net_uv_catch *queue_, s_sis_net_uv_node *node_)
{
	if (node_)
	{
		if (!node_->next)
		{
			return sis_net_uv_catch_head(queue_);
		}
		else
		{
			return node_->next;
		}
	}
	return queue_->curr_node;
}

void sis_net_uv_catch_clear(s_sis_net_uv_catch *queue_)
{
    sis_mutex_lock(&queue_->lock);
	s_sis_net_uv_node *node = sis_net_uv_catch_head(queue_);
	while(node)
	{
		s_sis_net_uv_node *next = node->next;
		queue_->count -= sis_net_mems_count(node->nodes); 
		sis_net_uv_node_destroy(node);
		node = next;
	}
	queue_->curr_node = NULL;
	queue_->work_node = NULL;
    sis_mutex_unlock(&queue_->lock);
}
void sis_net_uv_catch_destroy(s_sis_net_uv_catch *queue_)
{
	if (queue_->work_thread)
	{
		sis_wait_thread_destroy(queue_->work_thread);
	}
	sis_net_uv_catch_clear(queue_);
    // printf("=== sis_net_uv_catch_destroy. %d\n", queue_->count);
    sis_free(queue_);
}

void sis_net_uv_catch_del(s_sis_net_uv_catch *queue_, int cid_)
{
    sis_mutex_lock(&queue_->lock);
	s_sis_net_uv_node *head = sis_net_uv_catch_head(queue_);
	s_sis_net_uv_node *node = head;
	printf("del nodes: %d %p\n", cid_, head);
	while(node)
	{
		s_sis_net_uv_node *next = node->next;
		printf("del 1 nodes: %d %d %d\n", cid_, node->cid, queue_->count);
		if (node->cid == cid_)
		{
			if (queue_->curr_node == node)
			{
				if (node->next == NULL)
				{
					queue_->curr_node = node == head ? NULL : head;
				}
				else
				{
					queue_->curr_node = node->next;
				}
			}
			if (queue_->work_node == node)
			{
				queue_->work_node = queue_->curr_node;
			}
			queue_->count -= sis_net_mems_count(node->nodes); 
			sis_net_uv_node_destroy(node);
			break;
		}
		node = next;
	}
    sis_mutex_unlock(&queue_->lock);
}
s_sis_net_uv_node *_net_uv_catch_get(s_sis_net_uv_catch *queue_, int cid_)
{
	// 这里先从头找 后期再说
	s_sis_net_uv_node *head = sis_net_uv_catch_head(queue_);
	s_sis_net_uv_node *node = head;
	while(node)
	{
		s_sis_net_uv_node *next = node->next;
		if (node->cid == cid_)
		{
			return node;
		}
		node = next;
	}
	return NULL;
}
s_sis_net_uv_node *_net_uv_catch_add(s_sis_net_uv_catch *queue_, int cid_)
{
	s_sis_net_uv_node *newnode = sis_net_uv_node_create(cid_);
	if (!queue_->curr_node)
	{
		queue_->curr_node = newnode;
	}
	else
	{
		newnode->prev = queue_->curr_node;
		newnode->next = queue_->curr_node->next;
		if (queue_->curr_node->next)
		{
			queue_->curr_node->next->prev = newnode;
		}
		queue_->curr_node->next = newnode;
	}
	return newnode;
}
size_t sis_net_uv_catch_push(s_sis_net_uv_catch *queue_, int cid_, s_sis_memory *mem_)
{  
	size_t osize = 0;
	sis_mutex_lock(&queue_->lock);
	s_sis_net_uv_node *node = _net_uv_catch_get(queue_, cid_);
	if (!node)
	{
		node = _net_uv_catch_add(queue_, cid_);
	}
	sis_net_mems_cat(node->nodes, sis_memory(mem_), sis_memory_get_size(mem_));
	osize = sis_net_mems_size(node->nodes);
	queue_->count++;
	if (queue_->work_node == NULL)
	{
		queue_->work_node = node;
		// sis_net_nodes_read(queue_->work_node->nodes, 1024);
		// o = 1;
	}
    sis_mutex_unlock(&queue_->lock); 
	// sis_wait_thread_notice(queue_->work_thread);
	return osize;  
}
void sis_net_uv_catch_stop(s_sis_net_uv_catch *queue_)
{
    sis_mutex_lock(&queue_->lock);
	s_sis_net_uv_node *node = queue_->work_node;
	// printf("nodes ++ : %d %d %lld %lld\n", queue_->count, node ? node->cid : -1, 
	// 	node ? node->nodes->rnums : 0, node ? node->nodes->wnums : 0 );
	if (node)
	{
		int count = sis_net_mems_free_read(node->nodes);
		queue_->count -= count;
	}
	// printf("nodes -- : %d %d %lld %lld\n", queue_->count, node ? node->cid : -1, 
	// 	node ? node->nodes->rnums : 0, node ? node->nodes->wnums : 0 );
    sis_mutex_unlock(&queue_->lock);
	sis_wait_thread_notice(queue_->work_thread);
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
// s_sis_socket_session
//////////////////////////////////////////////////////////////

/**
 * @brief 创建新的session并初始化，需要关注的是这里需要将session自身的指针赋值到uv_w_handle.data，这样才可以在后续的回调函数中通过uv_w_handle获取到对应的session
 * @param father_ 服务器句柄 s_sis_socket_server
 * @return s_sis_socket_session* 
 */
s_sis_socket_session *sis_socket_session_create(void *father_)
{
	s_sis_socket_session *session = SIS_MALLOC(s_sis_socket_session, session); 
	session->sid = -1;
	session->father = father_;
	//QQQ 这里只赋值了data，那uv_w_handle的整体赋值在哪里呢？
	session->uv_w_handle.data = session;
	session->uv_r_buffer = uv_buf_init((char*)sis_malloc(MAX_NET_UV_BUFFSIZE), MAX_NET_UV_BUFFSIZE);
	session->send_nodes = sis_net_nodes_create();
	session->sendnums = 16 * 1024;
	session->sendbuff = sis_malloc(sizeof(uv_buf_t) * session->sendnums);
	memset(session->sendbuff, 0, sizeof(uv_buf_t) * session->sendnums);

	session->_uv_send_async_nums = 0;
	session->_uv_recv_async_nums = 0;
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
		session->sendbuff = NULL;
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
//////////////////////////////////////////////////////////////
// s_sis_socket_server
//////////////////////////////////////////////////////////////
static int cb_server_write(void *);
void _cb_server_async_write(uv_async_t* handle);

s_sis_socket_server *sis_socket_server_create()
{
	s_sis_socket_server *server = SIS_MALLOC(s_sis_socket_server, server); 

	server->uv_s_worker = sis_malloc(sizeof(uv_loop_t));  
	assert(0 == uv_loop_init(server->uv_s_worker));

	server->isinit = false;
	server->cb_source = server;
	server->cb_connected_s2c = NULL;
	server->cb_disconnect_s2c = NULL;

	server->sessions = sis_net_list_create(sis_socket_session_destroy);	
	// server->sessions->wait_sec = 180;

	server->isexit = false;
	
	server->write_list = sis_net_uv_catch_create(server, cb_server_write, 10);
	
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
	
	sis_net_uv_catch_destroy(server_->write_list);

	sis_socket_server_close(server_);
	// 等待所有的客户连接断开
	sis_net_list_destroy(server_->sessions);
	uv_loop_close(server_->uv_s_worker); 	
	sis_free(server_->uv_s_worker);
	sis_free(server_);
	LOG(5)("server exit.\n");
}

static void cb_session_closed(uv_handle_t *handle)
{
	s_sis_socket_session *session = (s_sis_socket_session *)handle->data;
	// 再删除
	s_sis_socket_server *server = (s_sis_socket_server *)session->father;
	sis_net_list_stop(server->sessions, session->sid - 1);
	LOG(5)("server of session close ok.[%p] %d %d\n", session, session->sid, server->sessions->cur_count);
}
/** 关闭服务器连接 */
void sis_socket_server_close(s_sis_socket_server *server_)
{ 
	if (!server_->isinit) 
	{
		return ;
	}
	int index = sis_net_list_first(server_->sessions);
	while (index >= 0)
	{
		s_sis_socket_session *session = (s_sis_socket_session *)sis_net_list_get(server_->sessions, index);		
		sis_socket_close_handle((uv_handle_t*)&session->uv_w_handle, NULL);
		// if(!sis_socket_close_handle((uv_handle_t*)&session->uv_w_handle, cb_session_closed))
		{
			sis_net_list_stop(server_->sessions, session->sid - 1);
		}
		index = sis_net_list_next(server_->sessions, index);
		LOG(5)("server close %d. %d\n", index, server_->sessions->cur_count);
	}
	sis_socket_close_handle((uv_handle_t *)&server_->uv_s_handle, NULL);
	// LOG(5)("server close. %d %d\n", server_->uv_s_worker->active_handles, server_->uv_s_worker->active_reqs.count);
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
	/** 这个赋值保证在新连接的回调函数里面，可以通过地一个参数获取s_sis_socket_server的地址*/
	server->uv_s_handle.data = server;

	server->uv_w_async.data = server;
	// assert(0 == uv_async_init(server->uv_s_worker, &server->uv_w_async, _cb_server_async_write));

	return true;
}

void _thread_server(void* arg)
{
	s_sis_socket_server *server = (s_sis_socket_server*)arg;
	LOG(5)("server start.[%p] %s:%d\n", server, server->ip, server->port);
	uv_run(server->uv_s_worker, UV_RUN_DEFAULT);
	LOG(5)("server stop. %d %s:%d\n", server->isinit, server->ip, server->port);
    _uv_exit_loop(server->uv_s_worker);	
	server->uv_s_thread = 0;
}
bool _sis_socket_server_start(s_sis_socket_server *server)
{
	int o = uv_thread_create(&server->uv_s_thread, _thread_server, server);
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
// uv_async 如果多个线程同时发出信号 偶尔会漏掉一个信号
// 造成接收数据中断因此只能以单个队列处理所有多线程发送来的数据
// 尤其在多个CPU环境下 此种现象很常见 

//服务器-新客户端函数
static void cb_server_read_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer)
{
	if (!handle->data) 
	{
		LOG(5)("cb_server_read_after : no session.\n");
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
/**
 * @brief 供uv库调用的读取数据处理函数，主要任务是判断数据是否已经读取完毕，并调用session的数据回调函数
 * @param handle 读取数据的tcp句柄，它的data属性保存的实际上是session
 * @param nread 已读取数据的长度
 * @param buffer 读取数据的缓冲区
 */
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
	// LOG(5)("recv .%d. %p %p\n", (int)nread, session, session->cb_recv_after);
	if (nread > 0 && session->cb_recv_after) 
	{
		buffer->base[nread] = 0;
		session->cb_recv_after(server->cb_source, session->sid, buffer->base, nread);
	}
}
/**
 * @brief 新子客户端接入的回调函数，在连接成功时做以下工作：
 * （1）创建并初始化session
 * （2）accept连接
 * （3）从接入的子客户端连接中读取数据
 * @param handle uv库的tcp句柄
 * @param status 连接错误码，0 成功，非0,连接遇到错误
 */
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
	//分配SESSION ID
	int index = sis_net_list_new(server->sessions, session);
	session->sid = index + 1;

	o = uv_accept((uv_stream_t*)&server->uv_s_handle, (uv_stream_t*)&session->uv_w_handle);
	if (o) 
	{	
		LOG(5)("session cannot accept.\n");	
		uv_close((uv_handle_t*)&session->uv_w_handle, cb_session_closed);
		return;
	}
	// 调用server的新连接处理回调
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
	// LOG(5)("server listen %s:%d.\n", server->ip, server->port);
	return true;
}
/**
 * @brief 监听端口，并开启独立线程运行uv_loop
 * @param server 
 * @return true 
 * @return false 
 */
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
int       _uv_ready_nums = 0;
int64     _uv_ready_size = 0;
#endif

static void cb_server_write_after(uv_write_t *writer_, int status)
{	
	s_sis_socket_server *server = (s_sis_socket_server *)writer_->data;
	s_sis_net_uv_node *node = server->write_list->work_node;
	if (!node)
	{
		printf("no node\n");
		return ;
	}
	s_sis_socket_session *session = sis_net_list_get(server->sessions, node->cid);
	if (!session) 
	{
		// session 已经退出
		printf("cb_server_write_after: %d count = %d \n", node->cid, server->write_list->count);
		sis_net_uv_catch_del(server->write_list, node->cid);
		return ;
	}
	// sis_net_nodes_free_read(session->send_nodes);
	if (status < 0) 
	{
		LOG(5)("server write fail: %s.\n", uv_strerror(status));
		// 应该关闭该连接
		sis_socket_server_delete(server, session->sid); //连接断开，关闭客户端
		return;
	}
#ifdef _UV_DEBUG_
	if (_uv_write_nums % 1000 == 0)
		LOG(5)("uv write stop:  nums :%d delay : %lld \n", _uv_write_nums, sis_time_get_now_msec() - _uv_write_msec);
	_uv_write_nums++;
#endif
	// 通知下一个
	sis_net_uv_catch_stop(server->write_list);
	if (session->cb_send_after)
	{
		session->cb_send_after(server->cb_source, session->sid, status);
	} 
}
int _start_nums = 0;
void _cb_server_async_write(uv_async_t* handle)
{
	s_sis_socket_server *server = (s_sis_socket_server *)handle->data;
	// printf("cb_server_write 0 : %d\n", handle->pending);
	// sis_socket_close_handle((uv_handle_t*)handle, NULL);
	if (!uv_is_closing((uv_handle_t *)handle))
	{
		uv_close((uv_handle_t*)handle, NULL);	//如果async没有关闭，消息队列是会阻塞的
	}
	// printf("cb_server_write 0 : %d\n", handle->pending);
	s_sis_net_uv_node *node = server->write_list->work_node;
	if (!node)
	{
		sis_net_uv_catch_stop(server->write_list);
		return ;
	}
	s_sis_socket_session *session = sis_net_list_get(server->sessions, node->cid);
	if (!session) 
	{
		// session 已经退出
		LOG(5)("_cb_server_async_write: count = %d \n", server->write_list->count);
		sis_net_uv_catch_stop(server->write_list);
		return ;
	}
	int index = 0;
	s_sis_net_mem_node *next = node->nodes->rhead;
	// _uv_ready_size = 0;
	while (next)
	{
		session->sendbuff[index].base = next->memory;
		session->sendbuff[index].len = next->size;
		index++;	
		// _uv_ready_size += next->size;
		next = next->next;
	}
#ifdef _UV_DEBUG_	
	session->_uv_recv_async_nums++;
	_uv_ready_nums += index;
	// LOG(5)("uv write start. nums :%d. %d %d %10lld | %d %d %lld\n", _uv_write_nums, _uv_ready_nums, index, _uv_ready_size, node->nodes->rnums, node->nodes->wnums, node->nodes->wsize);
#endif
	if (index == 0)
	{
		LOG(5)("write null data. %d\n", server->write_list->count);
		return ;
	}
	session->uv_h_write.data = server;
	int o = uv_write(&session->uv_h_write, (uv_stream_t*)&session->uv_w_handle, session->sendbuff, index, cb_server_write_after);
	if (o) 
	{
		sis_socket_server_delete(server, session->sid);
		LOG(5)("server write fail.\n");
	}
}

static int cb_server_write(void *source_)
{
	s_sis_socket_server *server = (s_sis_socket_server *)source_;
	s_sis_net_uv_node *node = server->write_list->work_node;
	if (!node)
	{
		sis_net_uv_catch_stop(server->write_list);
		return 0;
	}
#ifdef _UV_DEBUG_	
	if (_uv_write_nums == 0)
	{
		_uv_write_msec = sis_time_get_now_msec();
	}
	if (_uv_write_nums % 1000 == 0)
	{
		LOG(5)("uv write start. nums :%d.\n", _uv_write_nums);
		_uv_write_msec = sis_time_get_now_msec();
	}
#endif
	// 初始化异步
	server->uv_w_async.data = server;
	int o = uv_async_init(server->uv_s_worker, &server->uv_w_async, _cb_server_async_write);
	if (o) 
	{
		LOG(5)("init async fail.\n");
		return 0;
	}
	uv_async_send(&server->uv_w_async);	
	return 1;
}

//服务器发送函数
bool sis_socket_server_send(s_sis_socket_server *server, int sid_, s_sis_memory *inobj_)
{
	if (server->isexit)
	{
		return false;
	}
	if (!inobj_) 
	{
		return false; 
	}
	// printf("sis_socket_server_send : %d\n", sid_);
	// 放入队列就返回 其他交给内部处理
	
	// 直接断开链接会有问题 暂时不处理发送数据过多断开链接的问题
	// size_t csize = 
	s_sis_socket_session *session = sis_net_list_get(server->sessions, sid_ - 1);
	if (!session)
	{
		return false;
	}
	sis_net_uv_catch_push(server->write_list, sid_ - 1, inobj_);
	
	// if (csize > 16*1024*1024)
	// {
	// 	LOG(5)("client recv too slow. %zu\n", csize);
	// 	sis_socket_server_delete(server, sid_);
	// 	return false;
	// }

	return true;
}
 
 /**
  * @brief 设置session的数据接收回调函数和数据发送回调函数
  * @param server 服务器句柄
  * @param sid_ session id
  * @param cb_recv_ 数据接收回调函数
  * @param cb_send_ 数据发送回调函数
  */
void sis_socket_server_set_rwcb(s_sis_socket_server *server, int sid_, 
	cb_socket_recv_after cb_recv_, cb_socket_send_after cb_send_)
{
	s_sis_socket_session *session = sis_net_list_get(server->sessions, sid_ - 1);
	if (session)
	{
		sis_socket_session_set_rwcb(session, cb_recv_, cb_send_);
	}
}
//设置服务器-新链接回调函数
void sis_socket_server_set_cb(s_sis_socket_server *server, 
	cb_socket_connect cb_connected_,
	cb_socket_connect cb_disconnect_)
{
	server->cb_connected_s2c = cb_connected_;
	server->cb_disconnect_s2c = cb_disconnect_;
}
bool sis_socket_server_delete(s_sis_socket_server *server, int sid_)
{
	// 清理缓存数据
	sis_net_uv_catch_del(server->write_list, sid_ - 1);

	//  先发断开回调，再去关闭
	s_sis_socket_session *session = sis_net_list_get(server->sessions, sid_ - 1);
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
	sis_socket_close_handle((uv_handle_t*)&session->uv_w_handle, cb_session_closed);
	sis_net_list_stop(server->sessions, session->sid - 1);
	// 确定停止了再回调断开链接
	if (server->cb_disconnect_s2c) 
	{
		server->cb_disconnect_s2c(server->cb_source, sid_);
	}
	LOG(5)("delete session.[%d == %d] %p\n", session->sid, sid_, session);

	return true;	
}


/////////////////////////////////////////////////////////////
// s_sis_socket_client define 
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
				// 20220401 张超改
				// sis_socket_client_open(client);
				sis_socket_client_open_sync(client);
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
static int cb_client_write(void *source_);

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

	client->write_list = sis_net_uv_catch_create(client, cb_client_write, 10);

	if (uv_thread_create(&client->uv_reconn_thread, _thread_reconnect, client)) 
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
	
	client_->reconn_status = SIS_UV_CONNECT_EXIT; 
	uv_thread_join(&client_->uv_reconn_thread);  

	sis_socket_client_close(client_);

	sis_net_uv_catch_destroy(client_->write_list);

	sis_socket_session_destroy(client_->session, 1);

	sis_free(client_->uv_c_worker);
	sis_free(client_);
	LOG(5)("client exit.\n");
}

void cb_client_close(uv_handle_t *handle)
{
	s_sis_socket_session *session = (s_sis_socket_session *)handle->data;
	s_sis_socket_client *client = (s_sis_socket_client*)session->father;
	client->work_status |= SIS_UV_CONNECT_STOP;
}
void sis_socket_client_close(s_sis_socket_client *client_)
{
	if (client_->work_status & SIS_UV_CONNECT_EXIT)
	{
		return ; // 已经关闭过
	}
	client_->work_status = SIS_UV_CONNECT_EXIT;

	sis_socket_close_handle((uv_handle_t*)&client_->session->uv_w_handle, cb_client_close);
	
	while (client_->work_status & SIS_UV_CONNECT_STOP)
	{
		sis_sleep(10);
	}
	
	sis_net_uv_catch_clear(client_->write_list);

	uv_stop(client_->uv_c_worker); 

	uv_thread_join(&client_->uv_c_thread);  

	client_->isinit = false;
	LOG(5)("client close ok.\n");

	if (client_->cb_disconnect_c2s) 
	{
		client_->cb_disconnect_c2s(client_->cb_source, 0);
	}	
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
			LOG(5)("server normal break.[%p] %s.\n", handle, uv_strerror(nread)); 
		} 
		else //  if (nread == UV_ECONNRESET) 
		{
			LOG(5)("server unusual break.[%p] %s.\n", handle, uv_strerror(nread)); 
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
		// sis_socket_close_handle((uv_handle_t*)&session->uv_w_handle, NULL);
		client->work_status |= SIS_UV_CONNECT_FAIL;
		return;
	}
	int o = uv_read_start((uv_stream_t*)&session->uv_w_handle, cb_client_read_alloc, cb_client_read_after); //客户端开始接收服务器的数据
	// int o = uv_read_start(handle->handle, cb_client_read_alloc, cb_client_read_after); //客户端开始接收服务器的数据
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
	if (!client_->uv_c_worker) 
	{
		return false;
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
		LOG(5)("tcp init fail.[%d] %s %s\n", o, uv_err_name(o), uv_strerror(o));
		return false;
	}
	client_->isinit = true;
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
		LOG(5)("client connect fail.\n");
		return ;
	}	
	// _cb_after_connect(&uv_c_connect, -1);
	uv_run(client->uv_c_worker, UV_RUN_DEFAULT); 
	// 关闭时问题可参考 test-tcp-close-reset.c
    _uv_exit_loop(client->uv_c_worker);
	LOG(5)("client connect thread stop. [%d]\n", client->work_status);
}

void sis_socket_client_open_sync(s_sis_socket_client *client_)
{
	if (client_->work_status == SIS_UV_CONNECT_WAIT)
	{
		return ;
	}
	client_->work_status = SIS_UV_CONNECT_WAIT;
	if (!_sis_socket_client_init(client_)) 
	{
		return ;
	}
	_thread_client(client_);
}

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

static void cb_client_write_after(uv_write_t *writer_, int status)
{
	s_sis_socket_client *client = (s_sis_socket_client *)writer_->data;
	s_sis_socket_session *session  = client->session;
	if (status < 0) 
	{
		LOG(5)("client write error: %s.\n", uv_strerror(status));
		sis_socket_client_close(client);
		return;
	}
	sis_net_uv_catch_stop(client->write_list);
	if (session->cb_send_after)
	{
		session->cb_send_after(client->cb_source, 0, status);
	} 
	// LOG(5)("stop write ....\n");
}

void _cb_client_async_write(uv_async_t* handle)
{
	s_sis_socket_client *client = (s_sis_socket_client *)handle->data;	
	if (!uv_is_closing((uv_handle_t *)handle))
	{
		uv_close((uv_handle_t*)handle, NULL);	//如果async没有关闭，消息队列是会阻塞的
	}
	// LOG(5)("send write ..1..\n");
	s_sis_net_uv_node *node = client->write_list->work_node;
	if (!node)
	{
		LOG(5)("no data.\n");
		return ;
	}
	if (client->work_status & SIS_UV_CONNECT_EXIT)
	{
		LOG(5)("client close. no data.\n");
		return ;
	}
	s_sis_socket_session *session  = client->session;
	int index = 0;
	s_sis_net_mem_node *next = node->nodes->rhead;
	while (next)
	{
		session->sendbuff[index].base = next->memory;
		session->sendbuff[index].len = next->size;
		index++;	
		next = next->next;
	} 
	if (index == 0)
	{
		LOG(5)("write null data. %d\n", client->write_list->count);
		return ;
	}
	session->uv_h_write.data = client;
	int o = uv_write(&session->uv_h_write, (uv_stream_t*)&session->uv_w_handle, session->sendbuff, index, cb_client_write_after);
	if (o) 
	{
		sis_socket_client_close(client);
		LOG(5)("client write fail.\n");
	}
	// LOG(5)("send write ....\n");
	// sis_socket_close_handle((uv_handle_t*)handle, NULL);
}
static int cb_client_write(void *source_)
{
	s_sis_socket_client *client = (s_sis_socket_client *)source_;
	client->uv_w_async.data = client;
	int o = uv_async_init(client->uv_c_worker, &client->uv_w_async, _cb_client_async_write);
	if (o) 
	{
		LOG(5)("init async fail.\n");
		sis_socket_client_close(client);
		return 0;
	}
	// LOG(5)("client async ....\n");
	o = uv_async_send(&client->uv_w_async);
	// LOG(5)("client async ..%d..%d\n", o, client->uv_w_async.async_sent);
	
	return 1;
}
bool sis_socket_client_send(s_sis_socket_client *client_, s_sis_memory *inobj_)
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
	sis_net_uv_catch_push(client_->write_list, 0, inobj_);

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
	// if (cli->sno < 10*1000)
	// {
	// 	cli->sno++;
	// 	s_sis_sds sds = sis_sdsempty();
	// 	sds = sis_sdscatfmt(sds,"i am client. [sno = %i]. %i", cli->sno, (int)sis_time_get_now());
	// 	s_sis_object *obj = sis_object_create(SIS_OBJECT_SDS, sds);
	// 	sis_socket_client_send(cli->client, obj);
	// 	sis_object_destroy(obj);
	// }
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
	// for (int i = 0; i < 10000; i++)
	{
		sis_socket_client_send(cli->client, obj);
	}
	// sis_socket_client_send(cli->client, obj);
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
		// if (client->work_status != SIS_UV_CONNECT_WORK)
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
