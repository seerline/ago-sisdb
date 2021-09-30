
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

#define MAX_NET_UV_BUFFSIZE (16 * 1024 * 1024)

typedef void (*cb_socket_connect)(void *, int sid_); // 客户端永远sid == 0 服务端 > 1
typedef void (*cb_socket_recv_after)(void *, int sid_, char *in_, size_t ilen_);
typedef void (*cb_socket_send_after)(void *, int sid_, int status);

void sis_socket_close_handle(uv_handle_t *handle_, uv_close_cb close_cb_);

/////////////////////////////////////////////////////////////
// s_sis_socket_server define 
/////////////////////////////////////////////////////////////

struct s_sis_socket_server;

/////////////////////////////////////////////////
//  s_sis_net_queue
/////////////////////////////////////////////////

// 队列结点
typedef struct s_sis_net_node1 {
	int                    cid;
    s_sis_object          *obj;    // 数据区
    struct s_sis_net_node1 *next;
} s_sis_net_node1;

// 发起始结点 全部处理完再返回
typedef int (cb_net_reader)(void *);

typedef struct s_sis_net_queue {

    s_sis_mutex_t      lock;  
    int                count;

	void              *source; // server : s_sis_socket_server || s_sis_socket_client

    s_sis_net_node1    *head;
    s_sis_net_node1    *tail;

    s_sis_net_node1    *live;  // 当前激活的节点 只能一个一个处理

    cb_net_reader     *cb_reader;
    s_sis_wait_thread *work_thread;  // 工作线程
} s_sis_net_queue;


s_sis_net_queue *sis_net_queue_create(void *source_, cb_net_reader *cb_reader_, int wait_nums_);
void sis_net_queue_destroy(s_sis_net_queue *queue_);
s_sis_net_node1 *sis_net_queue_push(s_sis_net_queue *queue_, int cid_, s_sis_object *obj_);
void sis_net_queue_stop(s_sis_net_queue *queue_);

// 用于服务器的会话单元
typedef struct s_sis_socket_session
{
	int       sid;   //会话 

	uv_tcp_t *work_handle;	//客户端句柄
	uv_buf_t  read_buffer;  // 接受数据的 buffer 有真实的数据区
	uv_buf_t  write_buffer; // 写数据的 buffer 没有真实的数据区 直接使用传入的数据缓存指针
	
	int        write_stop; // 是否已经关闭
	uv_async_t write_async;
	uv_write_t write_req;   // 写时请求

	volatile int    _uv_send_async_nums;
	volatile int    _uv_recv_async_nums;

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
	bool isexit;
	// ??? 客户断线后 删除了对象 却没清理发送队列
	s_sis_net_queue  *write_list; // 发送队列   
	s_sis_index_list *sessions; // 子客户端链接 s_sis_socket_session

	char ip[128];
	int port;

	void *cb_source;			// 回调时作为第一个参数传出

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
	uv_loop_t    *loop;
	uv_tcp_t     client_handle;			   // 客户端句柄
	uv_thread_t  client_thread_handle; // 线程句柄
	uv_connect_t connect_req;		   // 连接时请求 主要传递当前类

	uv_buf_t read_buffer;  // 接受数据的 buffer 有真实的数据区
	uv_buf_t write_buffer; // 写数据的 buffer 没有真实的数据区 直接使用传入的数据缓存指针

	uv_async_t write_async;
	uv_write_t write_req;  // 写时请求

	int work_status; // 连接状态

	int keep_connect_status; // 连接状态
	// 千万不要用uv的时间片 是因为退出时 uv_run 会异常
	uv_thread_t keep_connect_thread_handle; //  断开重新连接的线程句柄

	bool isinit; // 是否已初始化，用于 close 函数中判断
	bool isexit; // 避免多次进入

	s_sis_net_queue  *write_list; // 发送队列   

	char  ip[128];
	int   port;
	void *cb_source;						 // 回调时作为第一个参数传出
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

void sis_socket_client_set_cb(s_sis_socket_client *,
							  cb_socket_connect cb_connected_,
							  cb_socket_connect cb_disconnect_);

void sis_socket_client_set_rwcb(s_sis_socket_client *,
								cb_socket_recv_after cb_recv_, 
								cb_socket_send_after cb_send_); // 必须保证发送的数据结构一致

bool sis_socket_client_send(s_sis_socket_client *, s_sis_object *in_);

#endif