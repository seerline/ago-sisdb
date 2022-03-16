
#ifndef _SIS_NET_S_H
#define _SIS_NET_S_H

// #include <sis_list.h>
// #include <sis_time.h>
// #include <os_types.h>
// #include <os_net.h>
// #include <sis_malloc.h>
// #include <sis_str.h>
// #include <sis_obj.h>
// #include <sis_thread.h>
// #include <sis_net.node.h>

// #define MAX_NET_S_BUFFSIZE (16 * 1024 * 1024)

// typedef void (*cb_socket_connect)(void *, int sid_); // 客户端永远sid == 0 服务端 > 1
// typedef void (*cb_socket_recv_after)(void *, int sid_, char *in_, size_t ilen_);
// typedef void (*cb_socket_send_after)(void *, int sid_, int status);

// int sis_socket_close_handle(uv_handle_t *handle_, uv_close_cb close_cb_);

// /////////////////////////////////////////////////
// //  s_sis_net_uv_catch 
// /////////////////////////////////////////////////
// // ??? uv 在大量写入时 会自动增长缓存 并且当流量降下来后并没有释放机制
// // 好的处理方式是 当发现积累数据过多自动返回错误，这一点以后有时间再解决
// // ??? 客户段在断线重连过程中 有内存泄漏的情况 4-8K 有时间查查
// #define UV_NODE_MAXLEN  1024 
// // 发起始结点 全部处理完再返回
// typedef int (cb_net_uv_reader)(void *);

// // 队列结点
// typedef struct s_sis_net_uv_node {
// 	int                       cid;
// 	s_sis_net_nodes          *nodes;
//     struct s_sis_net_uv_node *next;
//     struct s_sis_net_uv_node *prev;
// } s_sis_net_uv_node;

// typedef struct s_sis_net_uv_catch {

//     s_sis_mutex_t         lock;  
//     int                   count;      // 所有链接的数据数量

// 	void                 *source;     // server : s_sis_socket_server || s_sis_socket_client
	
//     s_sis_net_uv_node    *curr_node;  // 存储数据的最新节点 处理完就下一个 均匀发送
//     s_sis_net_uv_node    *work_node;  // 存储数据的最新节点 处理完就下一个 均匀发送

//     cb_net_uv_reader     *cb_reader;
//     s_sis_wait_thread    *work_thread;  // 工作线程
// } s_sis_net_uv_catch;


// s_sis_net_uv_catch *sis_net_uv_catch_create(void *source_, cb_net_uv_reader *cb_reader_, int wait_nums_);
// void sis_net_uv_catch_destroy(s_sis_net_uv_catch *queue_);
// // 返回缓存大小
// size_t sis_net_uv_catch_push(s_sis_net_uv_catch *queue_, int cid_, s_sis_object *obj_);
// void sis_net_uv_catch_stop(s_sis_net_uv_catch *queue_);

// s_sis_net_uv_node *sis_net_uv_catch_tail(s_sis_net_uv_catch *queue_);
// s_sis_net_uv_node *sis_net_uv_catch_head(s_sis_net_uv_catch *queue_);
// s_sis_net_uv_node *sis_net_uv_catch_next(s_sis_net_uv_catch *queue_, s_sis_net_uv_node *node_);

// // 移除cid所有数据
// void sis_net_uv_catch_del(s_sis_net_uv_catch *queue_, int cid_);

// // 用于服务器的会话单元
// typedef struct s_sis_socket_session
// {
// 	int                   sid;   //会话 
// 	void                 *father; //服务器句柄(保存是因为某些回调函数需要到)

// 	uv_tcp_t              uv_w_handle;	    // 客户端tcp句柄
// 	uv_buf_t              uv_r_buffer;      // 接受数据的 buffer 有真实的数据区
  
// 	int                   sendnums;         // 一次发送数量 64 * 1024
// 	uv_buf_t             *sendbuff;         // 缓存
// 	s_sis_net_nodes      *send_nodes;       // 发送队列  
	
// 	int                   write_stop; // 是否已经关闭
// 	uv_write_t            uv_h_write;   // 写时请求

// 	volatile int    _uv_send_async_nums;
// 	volatile int    _uv_recv_async_nums;

// 	cb_socket_recv_after  cb_recv_after; // 接收数据回调给用户的函数
// 	cb_socket_send_after  cb_send_after; // 回调函数
// } s_sis_socket_session;
// //////////////////////////////////////////////////////////////
// // s_sis_socket_session
// //////////////////////////////////////////////////////////////

// s_sis_socket_session *sis_socket_session_create(void *);

// void sis_socket_session_set_rwcb(s_sis_socket_session *session_,
// 								cb_socket_recv_after cb_recv_, 
// 								cb_socket_send_after cb_send_);

// void sis_socket_session_destroy(void *session_, int mode_);

// void sis_socket_session_init(s_sis_socket_session *session_);

// /////////////////////////////////////////////////////////////
// // s_sis_socket_server define 
// /////////////////////////////////////////////////////////////

// typedef struct s_sis_socket_server
// {

// 	uv_loop_t           *uv_s_worker;
// 	uv_tcp_t             uv_s_handle;	//服务器链接
// 	uv_thread_t          uv_s_thread; // server 线程句柄
// 	uv_async_t           uv_w_async;
	      
// 	bool                 isinit;     // 是否已初始化，用于 close 函数中判断
// 	bool                 isexit;     
// 	// ??? 客户断线后 删除了对象 却没清理发送队列
// 	s_sis_net_uv_catch  *write_list; // 发送队列   
// 	s_sis_net_list      *sessions; // 子客户端链接 s_sis_socket_session

// 	char                 ip[128];
// 	int                  port;
                
// 	void                *cb_source;			// 回调时作为第一个参数传出

// 	cb_socket_connect    cb_connected_s2c;  // 新连接处理回调
// 	cb_socket_connect    cb_disconnect_s2c; // 新连接处理回调

// } s_sis_socket_server;

// s_sis_socket_server *sis_socket_server_create();
// void sis_socket_server_destroy(s_sis_socket_server *);

// void sis_socket_server_close(s_sis_socket_server *);

// bool sis_socket_server_open(s_sis_socket_server *);
// bool sis_socket_server_open6(s_sis_socket_server *);

// bool sis_socket_server_send(s_sis_socket_server *, int sid_, s_sis_object *in_);

// bool sis_socket_server_delete(s_sis_socket_server *, int sid_);

// void sis_socket_server_set_rwcb(s_sis_socket_server *,
// 								int sid_, 
// 								cb_socket_recv_after cb_recv_, 
// 								cb_socket_send_after cb_send_); // 必须保证发送的数据结构一致

// void sis_socket_server_set_cb(s_sis_socket_server *,
// 							  cb_socket_connect cb_connected_,
// 							  cb_socket_connect cb_disconnect_);

// /////////////////////////////////////////////////////////////
// // s_sis_socket_client define 
// /////////////////////////////////////////////////////////////

// #define SIS_UV_CONNECT_NONE 0
// #define SIS_UV_CONNECT_WAIT (1 << 0) // 正在准备连接不要重复进入
// #define SIS_UV_CONNECT_STOP (1 << 1) // 连接断开
// #define SIS_UV_CONNECT_FAIL (1 << 2) // 连接失败
// #define SIS_UV_CONNECT_WORK (1 << 3) // 正常
// #define SIS_UV_CONNECT_EXIT (1 << 4) // 端口关闭 有此标志不再重连

// typedef struct s_sis_socket_client
// {
// 	uv_loop_t            *uv_c_worker;
// 	uv_thread_t           uv_c_thread; // 线程句柄
// 	uv_async_t            uv_w_async;

// 	s_sis_socket_session *session;          // 链接后的实例 断开链接 = sid = -1 来判断

// 	bool                  isinit; // 是否已初始化，用于 close 函数中判断
// 	bool                  isexit; // 避免多次进入

// 	int                   work_status;      // 连接状态
// 	int                   reconn_status;    // 连接状态
// 	uv_thread_t           uv_reconn_thread;  //  断开重新连接的线程句柄

// 	s_sis_net_uv_catch   *write_list; // 发送队列   

// 	char                  ip[128];
// 	int                   port;
// 	void                 *cb_source;						 // 回调时作为第一个参数传出
// 	cb_socket_connect     cb_connected_c2s;  // 链接成功
// 	cb_socket_connect     cb_disconnect_c2s; // 链接断开

// } s_sis_socket_client;

// s_sis_socket_client *sis_socket_client_create();
// void sis_socket_client_destroy(s_sis_socket_client *);

// void sis_socket_client_close(s_sis_socket_client *);

// bool sis_socket_client_open(s_sis_socket_client *);
// bool sis_socket_client_open6(s_sis_socket_client *);

// void sis_socket_client_set_cb(s_sis_socket_client *,
// 							  cb_socket_connect cb_connected_,
// 							  cb_socket_connect cb_disconnect_);

// void sis_socket_client_set_rwcb(s_sis_socket_client *,
// 								cb_socket_recv_after cb_recv_, 
// 								cb_socket_send_after cb_send_); // 必须保证发送的数据结构一致

// bool sis_socket_client_send(s_sis_socket_client *, s_sis_object *in_);

#endif