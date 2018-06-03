
#include "sts_redis.h"

 
void sts_redis_open(s_sts_socket *sock_)
{
	if (!sock_) return ;
	sock_->status = STS_NET_WAITCONNECT;
	sock_->handle_read = NULL;
}
void sts_redis_close(s_sts_socket *sock_)
{
	if (!sock_)
	{
		return;
	}	
	if (sock_->handle_read)
	{
		redisFree((redisContext *)sock_->handle_read);
	}
}
void redis_connect(s_sts_socket *sock_){
	// sock->status = STS_NET_CONNECTING;
	s_sts_socket *sock = (s_sts_socket *)sock_;
	redisContext *context = NULL;
	struct timeval timeout = { 1, 500000 }; // 1.5 seconds
	context = redisConnectWithTimeout(sock->url.ip, sock->url.port, timeout);
	if (context == NULL || context->err)
	{
		if (context)
		{
			sts_out_error(3)("connection error: %s\n", context->errstr);
			redisFree(context);
		}
		else
		{
			sts_out_error(3)("connection error: can't allocate redis context\n");
		}
		context = NULL;
		goto fail;
	}
	else
	{
		redisReply *reply;
		printf("auth %s.\n", sock->url.password);
		reply = (redisReply *)redisCommand(context, "auth %s", sock->url.password);
		if (reply)
		{
			freeReplyObject(reply);
			sock->handle_read = context;
			sock->status = STS_NET_CONNECTED;
			return;
		}
		else {
			printf("reply error\n");
			redisFree(context);
			goto fail;
		}
	}
fail:
	if (sock->handle_read)
	{
		redisFree((redisContext *)sock->handle_read);
		sock->handle_read = NULL;
	}
}

bool redis_check_connect(s_sts_socket *sock)
{
	if (!sock)
	{
		return false;
	}
	if(sock->status != STS_NET_CONNECTED) {
		redis_connect(sock);
	}
	return (sock->status==STS_NET_CONNECTED);
}
bool sts_redis_send_message(s_sts_socket *sock_, s_sts_message_node * mess_)
{
	// printf("redis send %s %ld.\n", key_, len_);
	if (!redis_check_connect(sock_)){
		// printf("redis check connect fail.\n");
		return false;
	}
	// int argc = 4;
	// char **argvstr = (char **)sts_malloc(sizeof(char *)* argc);
	// size_t *argvlen = (size_t *)sts_malloc(sizeof(size_t)* argc);
	// memset(argvstr, 0, sizeof(char *)* argc);
	// memset(argvlen, 0, sizeof(size_t)* argc);

	// int index = 0;
	// argvlen[index] = strlen(command_);
	// argvstr[index] = (char *)command_;
	// index++;

	// argvlen[index] = strlen(key_);
	// argvstr[index] = (char *)key_;
	// index++;

	// if (format_){
	// 	argvlen[index] = strlen(format_);
	// 	argvstr[index] = (char *)format_;
	// 	index++;
	// }
	// if (len_ > 0){
	// 	argvlen[index] = len_;
	// 	argvstr[index] = value_;
	// 	index++;
	// }

	// redisReply *reply;

	// reply = (redisReply *)redisCommandArgv((redisContext *)sock->handle_read, index, (const char **)&(argvstr[0]), &(argvlen[0]));
	// if (reply)
	// {
	// 	printf("send data type %s [%d:%s] \n", key_, reply->type, reply->str);		
	// 	freeReplyObject(reply);
	// } else {
	// 	sock->status = STS_NET_CONNECTSTOP;
	// }
	// sts_free((void *)argvstr);
	// sts_free((void *)argvlen);
	return true;
}

s_sts_message_node *sts_redis_query_message(s_sts_socket *sock_, s_sts_message_node *mess_)
{
	return NULL;
}
