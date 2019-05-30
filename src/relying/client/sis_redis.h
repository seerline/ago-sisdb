#ifndef _SIS_REDIS_H
#define _SIS_REDIS_H

#include "sis_net.v1.h"
#include "sis_log.h"
#include "hiredis.h"

//---------- socket create 流程 ---------//
	// socket = sis_socket_create(...);
	// socket->open = sis_redis_open;
	// socket->close = sis_redis_close;
	// socket->send_message = sis_redis_send_message;
	// socket->query_message = sis_redis_query_message;
	// sis_socket_destroy(socket);
//---------- socket 使用方法 ----------//
	// sis_socket_open(socket);
	// sis_socket_send_message(socket*,s_sis_net_message*);
	// sis_socket_query_message(socket*,s_sis_net_message*);
	// sis_socket_close(socket);
//-----------------------------------//

void sis_redis_open(s_sis_socket *);
void sis_redis_close(s_sis_socket *);

//仅仅向服务器发送信息，不需要返回响应信息
bool sis_redis_send_message(s_sis_socket *, s_sis_net_message *);
// s_sis_list_node *sis_redis_recv_message(s_sis_socket *, s_sis_net_message *);
//发信息并等待服务器响应,收到一个完整的信息链
s_sis_net_message *sis_redis_query_message(s_sis_socket *, s_sis_net_message *); 

#endif //_SIS_REDIS_H