#ifndef _STS_REDIS_H
#define _STS_REDIS_H

#include "sts_net.h"
#include "sts_log.h"
#include "hiredis.h"

//---------- socket create 流程 ---------//
	// socket = sts_socket_create(...);
	// socket->open = sts_redis_open;
	// socket->close = sts_redis_close;
	// socket->send_message = sts_redis_send_message;
	// socket->query_message = sts_redis_query_message;
	// sts_socket_destroy(socket);
//---------- socket 使用方法 ----------//
	// sts_socket_open(socket);
	// sts_socket_send_message(socket*,s_sts_message_node*);
	// sts_socket_query_message(socket*,s_sts_message_node*);
	// sts_socket_close(socket);
//-----------------------------------//

void sts_redis_open(s_sts_socket *);
void sts_redis_close(s_sts_socket *);

//仅仅向服务器发送信息，不需要返回响应信息
bool sts_redis_send_message(s_sts_socket *, s_sts_message_node *);
// s_sts_list_node *sts_redis_recv_message(s_sts_socket *, s_sts_message_node *);
//发信息并等待服务器响应,收到一个完整的信息链
s_sts_message_node *sts_redis_query_message(s_sts_socket *, s_sts_message_node *); 

#endif //_STS_REDIS_H