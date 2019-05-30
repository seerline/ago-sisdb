
#ifndef _SIS_NET_UV_H
#define _SIS_NET_UV_H

#include <uv.h>
// #include <sis_net.h>
#include <sis_malloc.h>
#include <sis_str.h>
#include <sis_map.h>

#define MAX_NET_UV_BUFFSIZE (32*1024*1024)

typedef void (*callback_connect_c2s)(void *);
typedef void (*callback_connect_s2c)(void *,int cid_);
typedef void (*callback_socket_recv)(void *,int cid_, const char* in_, size_t ilen_);

typedef struct s_sis_socket_server {
	int no; // 序号

	uv_loop_t *loop;
	uv_tcp_t server;//服务器链接

	bool isinit; // 是否已初始化，用于 close 函数中判断

	s_sis_map_pointer   *clients; // 子客户端链接 s_sis_socket_session
	uv_mutex_t mutex_clients;  // 保护 clients
	uv_thread_t server_thread_hanlde;//线程句柄

	char ip[128];
	int  port;
	unsigned int delay; // 延迟多少秒再次链接
	int nodelay;  // 是否设置为不延时发送
	int keeplive; // 是否检查链接保持

	void *data; // 回调时作为第一个参数传出

	callback_connect_s2c cb_connect_s2c; // 新连接处理回调
	callback_connect_s2c cb_disconnect_s2c; // 新连接处理回调

}s_sis_socket_server;

s_sis_socket_server *sis_socket_server_create();
void sis_socket_server_destroy(s_sis_socket_server *sock_);

void sis_socket_server_close(s_sis_socket_server *sock_);

bool sis_socket_server_open(s_sis_socket_server *sock_);
bool sis_socket_server_open6(s_sis_socket_server *sock_);

bool sis_socket_server_send(s_sis_socket_server *sock_, int cid_, const char* in_, size_t ilen_);

bool sis_socket_server_delete(s_sis_socket_server *sock_, int cid_);

void sis_socket_server_set_recv_cb(s_sis_socket_server *sock_, int cid_, callback_socket_recv cb_);

void sis_socket_server_set_cb(s_sis_socket_server *sock_, 
	callback_connect_s2c connect_cb_,
	callback_connect_s2c disconnect_cb_);

typedef struct s_sis_socket_session {
	int client_id;//客户端id,惟一
	uv_tcp_t* client_handle;//客户端句柄
	s_sis_socket_server* server;//服务器句柄(保存是因为某些回调函数需要到)
	uv_buf_t read_buffer; // 接受数据的 buf
	uv_buf_t write_buffer;// 写数据的buf
	uv_write_t write_req;
	callback_socket_recv cb_recv; // 接收数据回调给用户的函数
}s_sis_socket_session;

inline s_sis_socket_session *sis_socket_session_create(int cid_)
{
	s_sis_socket_session *session = SIS_MALLOC(s_sis_socket_session, session); 

	session->client_id = cid_;
	session->cb_recv = NULL;
	session->client_handle = (uv_tcp_t*)sis_malloc(sizeof(uv_tcp_t));
	session->client_handle->data = session;
	session->read_buffer = uv_buf_init((char*)sis_malloc(MAX_NET_UV_BUFFSIZE), MAX_NET_UV_BUFFSIZE);
    session->write_buffer = uv_buf_init((char*)sis_malloc(MAX_NET_UV_BUFFSIZE), MAX_NET_UV_BUFFSIZE);
	return session;	
}
inline void sis_socket_session_destroy(s_sis_socket_session *session_)
{
	sis_free(session_->read_buffer.base);
	session_->read_buffer.base = NULL;
	session_->read_buffer.len = 0;

	sis_free(session_->write_buffer.base);
	session_->write_buffer.base = NULL;
	session_->write_buffer.len = 0;

	sis_free(session_->client_handle);
	session_->client_handle = NULL;
	sis_free(session_);
}

#define SIS_UV_CONNECT_TIMEOUT   0
#define SIS_UV_CONNECT_FINISH    1
#define SIS_UV_CONNECT_ERROR     2
#define SIS_UV_CONNECT_STOP      3
#define SIS_UV_CONNECT_WAIT      4

typedef struct s_sis_socket_client {

	uv_loop_t *loop;
	uv_tcp_t client_handle;   // 客户端连接
	uv_write_t write_req;     // 写时请求
	uv_connect_t connect_req; // 连接时请求
	uv_thread_t connect_thread_hanlde; // 线程句柄
	uv_timer_t client_timer;  // reconnect timer

	uv_buf_t read_buffer; // 接受数据的buf
	uv_buf_t write_buffer;// 写数据的buf

	uv_mutex_t mutex_write;// 保护write, 保存前一write完成才进行下一write

	int   status;  // 连接状态
	bool  isinit;  // 是否已初始化，用于 close 函数中判断
	bool  isexit;  // 避免多次进入
	char  ip[128];
	int   port;
	unsigned int delay;
	int nodelay; 
	int keeplive;

	void *data; // 回调时作为第一个参数传出

	callback_socket_recv cb_recv; // 回调函数

	callback_connect_c2s cb_connect_c2s; // 链接成功
	callback_connect_c2s cb_disconnect_c2s; // 链接断开

}s_sis_socket_client;

s_sis_socket_client *sis_socket_client_create();
void sis_socket_client_destroy(s_sis_socket_client *sock_);

void sis_socket_client_close(s_sis_socket_client *sock_);

bool sis_socket_client_open(s_sis_socket_client *sock_);
bool sis_socket_client_open6(s_sis_socket_client *sock_);

void sis_socket_client_delete(s_sis_socket_client *sock_);

void sis_socket_client_set_cb(s_sis_socket_client *sock_, 
	callback_connect_c2s connect_cb_,
	callback_connect_c2s disconnect_cb_);

void sis_socket_client_set_recv_cb(s_sis_socket_client *sock_, callback_socket_recv cb_);

bool sis_socket_client_send(s_sis_socket_client *sock_, const char* in_, size_t ilen_);

#endif