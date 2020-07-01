
#ifndef _SIS_NET_UV_H
#define _SIS_NET_UV_H

#include <uv.h>
#include <sis_list.h>
#include <sis_time.h>
#include <os_types.h>
#include <sis_malloc.h>
#include <sis_str.h>
#include <sis_obj.h>

#define MAX_NET_UV_BUFFSIZE (16 * 1024 * 1024)

typedef void (*cb_socket_connect)(void *, int sid_); // 客户端永远sid == 0 服务端 > 1
typedef void (*cb_socket_recv_after)(void *, int sid_, char *in_, size_t ilen_);
typedef void (*cb_socket_send_after)(void *, int sid_, int status);

/////////////////////////////////////////////////////////////
// s_sis_socket_server define 
/////////////////////////////////////////////////////////////

struct s_sis_socket_server;

// 用于服务器的会话单元
typedef struct s_sis_socket_session
{
	int session_id;   //会话id,惟一
	bool used;		  // 是否可用 关闭后设置为 0 超过600秒后可以重新使用
	fsec_t stop_time; // 关闭时的时间

	uv_tcp_t *uvhandle;	    //客户端句柄
	uv_buf_t  read_buffer;  // 接受数据的 buffer 有真实的数据区
	uv_buf_t  write_buffer; // 写数据的 buffer 没有真实的数据区 直接使用传入的数据缓存指针
	

	uv_async_t write_async;
	uv_write_t write_req;

	struct s_sis_socket_server *server; //服务器句柄(保存是因为某些回调函数需要到)

	cb_socket_recv_after cb_recv_after; // 接收数据回调给用户的函数
	cb_socket_send_after cb_send_after; // 回调函数
} s_sis_socket_session;

typedef struct s_sis_socket_server
{

	uv_loop_t *loop;
	uv_tcp_t server_handle;			  //服务器链接
	uv_thread_t server_thread_handle; // server 线程句柄

	bool isinit; // 是否已初始化，用于 close 函数中判断

	int recover_delay;			  // 客户端断开后 session资源释放的延时 默认300秒 数量过大时每次减半 最小不低于 3
	s_sis_pointer_list *sessions; // 子客户端链接 s_sis_socket_session

	char ip[128];
	int port;
	unsigned int delay;		// 保持连接的延时
	unsigned char nodelay;  // 是否设置为不延时发送
	unsigned char keeplive; // 是否检查链接保持
	void *source;			// 回调时作为第一个参数传出

	cb_socket_connect cb_connected_s2c;  // 新连接处理回调
	cb_socket_connect cb_disconnect_s2c; // 新连接处理回调

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
	uv_loop_t *loop;
	uv_tcp_t client_handle;			   // 客户端句柄
	uv_thread_t connect_thread_handle; // 线程句柄
	uv_connect_t connect_req;		   // 连接时请求 主要传递当前类

	uv_buf_t read_buffer;  // 接受数据的 buffer 有真实的数据区
	uv_buf_t write_buffer; // 写数据的 buffer 没有真实的数据区 直接使用传入的数据缓存指针

	uv_async_t write_async;
	uv_write_t write_req;  // 写时请求

	int connect_status; // 连接状态

	int reconnect_status; // 连接状态
	// 千万不要用uv的时间片 是因为退出时 uv_run 会异常
	uv_thread_t reconnect_thread_handle; //  断开重新连接的线程句柄

	bool isinit; // 是否已初始化，用于 close 函数中判断
	bool isexit; // 避免多次进入

	char ip[128];
	int port;
	unsigned int delay;					 // 保持连接的延时
	unsigned char nodelay;				 // 是否设置为不延时发送
	unsigned char keeplive;				 // 是否检查链接保持
	void *source;						 // 回调时作为第一个参数传出
	cb_socket_connect cb_connected_c2s;  // 链接成功
	cb_socket_connect cb_disconnect_c2s; // 链接断开

	cb_socket_recv_after cb_recv_after; // 回调函数
	cb_socket_send_after cb_send_after; // 回调函数

} s_sis_socket_client;

s_sis_socket_client *sis_socket_client_create();
void sis_socket_client_destroy(s_sis_socket_client *);

void sis_socket_client_close(s_sis_socket_client *);

bool sis_socket_client_open(s_sis_socket_client *);
bool sis_socket_client_open6(s_sis_socket_client *);

void sis_socket_client_delete(s_sis_socket_client *);

void sis_socket_client_set_cb(s_sis_socket_client *,
							  cb_socket_connect cb_connected_,
							  cb_socket_connect cb_disconnect_);

void sis_socket_client_set_rwcb(s_sis_socket_client *,
								cb_socket_recv_after cb_recv_, 
								cb_socket_send_after cb_send_); // 必须保证发送的数据结构一致

bool sis_socket_client_send(s_sis_socket_client *, s_sis_object *in_);

#endif