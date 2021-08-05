
#ifndef _SIS_NET_UV_H
#define _SIS_NET_UV_H

#include <uv.h>
#include <sis_list.h>
#include <sis_time.h>
#include <os_types.h>
#include <os_net.h>
#include <sis_malloc.h>
#include <sis_str.h>
#include <sis_obj.h>
#include <sis_thread.h>
#include <sis_net.node.h>

#define MAX_NET_UV_BUFFSIZE (16 * 1024 * 1024)

typedef void (*cb_socket_connect)(void *, int sid_); // 客户端永远sid == 0 服务端 > 1
typedef void (*cb_socket_recv_after)(void *, int sid_, char *in_, size_t ilen_);
typedef void (*cb_socket_send_after)(void *, int sid_, int status);

// 0 表示已经关闭 1 表示等待关闭
int sis_socket_close_handle(uv_handle_t *handle_, uv_close_cb close_cb_);



/////////////////////////////////////////////////////////////
// s_sis_socket_session  
/////////////////////////////////////////////////////////////

// 用于服务器的会话单元
typedef struct s_sis_socket_session
{
	int                   sid;              // 会话ID 
	void                 *father;           // 服务器句柄(保存是因为某些回调函数需要到)
	int                   write_stop;       // 是否正在停止

	uv_tcp_t              uv_w_handle;	    // 客户端tcp句柄
	uv_buf_t              uv_r_buffer;      // 接受数据的 buffer 有真实的数据区
  
	int                   sendnums;         // 一次发送数量 64 * 1024
	uv_buf_t             *sendbuff;         // 缓存
	s_sis_net_nodes      *send_nodes;       // 发送队列  

	cb_socket_recv_after  cb_recv_after;    // 接收数据回调给用户的函数
	cb_socket_send_after  cb_send_after;    // 回调函数

} s_sis_socket_session;

//////////////////////////////////////////////////////////////
// s_sis_socket_session
//////////////////////////////////////////////////////////////

s_sis_socket_session *sis_socket_session_create(void *);

void sis_socket_session_set_rwcb(s_sis_socket_session *session_,
								cb_socket_recv_after cb_recv_, 
								cb_socket_send_after cb_send_);

void sis_socket_session_destroy(void *session_);

void sis_socket_session_init(s_sis_socket_session *session_);

/////////////////////////////////////////////////////////////
// s_sis_socket_server  
/////////////////////////////////////////////////////////////

typedef struct s_sis_socket_server
{
	char               ip[128];
	int                port;

	uv_loop_t         *uv_s_worker;
	uv_tcp_t           uv_s_handle;		   // 服务器链接
	uv_thread_t        uv_s_thread;        // server 线程句柄
	
	bool               isinit;             // 是否已初始化，用于 close 函数中判断
	bool               isexit;

	int                cur_send_idx;       // 当前发送索引 

	int                write_may;          // 可写标识
	s_sis_mutex_t      write_may_lock;     // 可写锁
	s_sis_wait_thread *write_thread;      

	s_sis_net_list    *sessions;           // 子客户端链接 s_sis_socket_session

	void              *cb_source;	       // 回调时作为第一个参数传出
	cb_socket_connect  cb_connected_s2c;   // 新连接处理回调
	cb_socket_connect  cb_disconnect_s2c;  // 新连接处理回调

} s_sis_socket_server;
 
s_sis_socket_server *sis_socket_server_create();
void sis_socket_server_destroy(s_sis_socket_server *);

void sis_socket_server_close(s_sis_socket_server *);

bool sis_socket_server_open(s_sis_socket_server *);
bool sis_socket_server_open6(s_sis_socket_server *);

bool sis_socket_server_send(s_sis_socket_server *, int sid_, s_sis_object *in_);

bool sis_socket_server_delete(s_sis_socket_server *, int sid_);

void sis_socket_server_set_rwcb(s_sis_socket_server *,
								int sid_, 
								cb_socket_recv_after cb_recv_, 
								cb_socket_send_after cb_send_); // 必须保证发送的数据结构一致

void sis_socket_server_set_cb(s_sis_socket_server *,
							  cb_socket_connect cb_connected_,
							  cb_socket_connect cb_disconnect_);

/////////////////////////////////////////////////////////////
// s_sis_socket_client define 
/////////////////////////////////////////////////////////////

#define SIS_UV_CONNECT_NONE 0
#define SIS_UV_CONNECT_WAIT (1 << 0) // 正在准备连接不要重复进入
#define SIS_UV_CONNECT_STOP (1 << 1) // 连接断开
#define SIS_UV_CONNECT_FAIL (1 << 2) // 连接失败
#define SIS_UV_CONNECT_WORK (1 << 3) // 正常
#define SIS_UV_CONNECT_EXIT (1 << 4) // 端口关闭 有此标志不再重连

typedef struct s_sis_socket_client
{
	char  ip[128];
	int   port;

	uv_loop_t            *uv_c_worker;
	uv_thread_t           uv_c_thread;      // 线程句柄

	bool                  isinit;           // 是否已初始化，用于 close 函数中判断
	bool                  isexit;           // 避免多次进入

	int                   work_status;      // 连接状态
	int                   reconn_status;    // 连接状态
	uv_thread_t           uv_reconn_thread;  //  断开重新连接的线程句柄
	// 千万不要用uv的时间片 是因为退出时 uv_run 会异常

	int                   write_may;        // 可写标识
	s_sis_mutex_t         write_may_lock;   // 可写锁
	s_sis_wait_thread    *write_thread;      

	s_sis_socket_session *session;          // 链接后的实例 断开链接 = sid = -1 来判断

	void                 *cb_source;						 // 回调时作为第一个参数传出
	cb_socket_connect     cb_connected_c2s;  // 链接成功
	cb_socket_connect     cb_disconnect_c2s; // 链接断开

} s_sis_socket_client;

s_sis_socket_client *sis_socket_client_create();
void sis_socket_client_destroy(s_sis_socket_client *);

void sis_socket_client_close(s_sis_socket_client *);

bool sis_socket_client_open(s_sis_socket_client *);
bool sis_socket_client_open6(s_sis_socket_client *);

void sis_socket_client_set_cb(s_sis_socket_client *,
							  cb_socket_connect cb_connected_,
							  cb_socket_connect cb_disconnect_);

void sis_socket_client_set_rwcb(s_sis_socket_client *,
								cb_socket_recv_after cb_recv_, 
								cb_socket_send_after cb_send_); // 必须保证发送的数据结构一致

bool sis_socket_client_send(s_sis_socket_client *, s_sis_object *in_);

#endif