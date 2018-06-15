
#include "sts_redis.h"

 
void sts_redis_open(s_sts_socket *sock_)
{
	if (!sock_) return ;
	sock_->handle_read = NULL;
	sock_->status = STS_NET_WAITCONNECT;
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
	sock_->status = STS_NET_CONNECTSTOP;
}
void _sts_redis_connect(s_sts_socket *sock_)
{
	if(sock_->status == STS_NET_CONNECTSTOP) 
	{
		sts_out_error(5)("connect already close.\n");
		return ;
	}
	sock_->status = STS_NET_CONNECTING;

	s_sts_socket *sock = sock_;
	redisContext *context = NULL;
	struct timeval timeout = { 1, 500000 }; // 1.5 seconds
	context = redisConnectWithTimeout(sock->url.ip, sock->url.port, timeout);
	if (context == NULL || context->err)
	{
		if (context)
		{
			sts_out_error(3)("connection error: %s\n", context->errstr);
			redisFree(context);
			context = NULL;
		}
		else
		{
			sts_out_error(3)("connection error: can't allocate redis context\n");
		}
	}
	else
	{
		if (!sock->url.auth) 
		{
			sock->handle_read = context;
			sock->status = STS_NET_CONNECTED;
			return;			
		}
		redisReply *reply;
		printf("auth %s.\n", sock->url.password);
		reply = (redisReply *)redisCommand(context, "auth %s", sock->url.password);
		if (reply)
		{
			freeReplyObject(reply);
			sock->handle_read = context;
			sock->status = STS_NET_CONNECTED;
			return ;
		}
		else 
		{
			printf("auth reply error\n");
			redisFree(context);
			goto fail;
		}
	}
fail:
	sock->status = STS_NET_WAITCONNECT;
	if (sock->handle_read)
	{
		redisFree((redisContext *)sock->handle_read);
		sock->handle_read = NULL;
	}
}

bool _sts_redis_check_connect(s_sts_socket *sock)
{
	if (!sock)
	{
		return false;
	}
	if(sock->status == STS_NET_WAITCONNECT) 
	{
		_sts_redis_connect(sock);
	}
	return (sock->status==STS_NET_CONNECTED);
}
	s_sts_sds	command;   //来源信息专用,当前消息的key  stsdb.get 
	s_sts_sds	key;       //来源信息专用,当前消息的key  sh600600.day 
	s_sts_sds	argv;      //来源信息的参数，为json格式, 无需返回
	s_sts_sds	address;   //来源信息专用, 数据来源信息，需要原样返回；用户写的投递地址

	s_sts_sds   format;    //set发送的数据格式 struct json

	s_sts_list_node   *links;     //来源信息专用, 数据来源链路，每次多一跳就增加一个节点
	s_sts_list_node   *nodes;     //附带的信息数据链表  node->value 为 s_sts_sds 类型

int _sts_redis_get_cmds(s_sts_message_node * mess_)
{
	int o = 0;
	if (mess_->command) o++;
	if (mess_->key) o++;
	if (mess_->address) o++;
	if (mess_->format) o++;
	o += sts_sdsnode_get_count(mess_->nodes);
	return o++;
}
bool sts_redis_send_message(s_sts_socket *sock_, s_sts_message_node * mess_)
{
	printf("redis send %s. status=%d [%s:%d]\n", mess_->key, sock_->status, sock_->url.ip, sock_->url.port);
	if (!_sts_redis_check_connect(sock_)){
		printf("redis check connect fail.\n");
		return false;
	}

	int argc = _sts_redis_get_cmds(mess_);
	char **argvstr = (char **)sts_malloc(sizeof(char *)* argc);
	size_t *argvlen = (size_t *)sts_malloc(sizeof(size_t)* argc);
	memset(argvstr, 0, sizeof(char *)* argc);
	memset(argvlen, 0, sizeof(size_t)* argc);

	int index = 0;
	if (mess_->command)
	{
		argvlen[index] = sts_sdslen(mess_->command);
		argvstr[index] = mess_->command;
		index++;
	}

	if (mess_->key)
	{
		argvlen[index] = sts_sdslen(mess_->key);
		argvstr[index] = mess_->key;
		index++;
	}

	if (mess_->format)
	{
		argvlen[index] = sts_sdslen(mess_->format);
		argvstr[index] = mess_->format;
		index++;
	}

	s_sts_list_node *node = mess_->nodes;
	while(node) 
	{
		argvlen[index] = sts_sdslen(node->value);
		argvstr[index] = node->value;
		index++;
		node = node->next;
	}

	redisReply *reply;

	reply = (redisReply *)redisCommandArgv((redisContext *)sock_->handle_read, index, (const char **)&(argvstr[0]), &(argvlen[0]));
	if (reply)
	{
		printf("send data type %s [%d:%s] \n", mess_->key, reply->type, reply->str);		
		freeReplyObject(reply);
	} else 
	{
		printf("connect break. \n");	
		sock_->status = STS_NET_WAITCONNECT;
	}
	sts_free((void *)argvstr);
	sts_free((void *)argvlen);
	return true;
}

s_sts_message_node *sts_redis_query_message(s_sts_socket *sock_, s_sts_message_node *mess_)
{
	return NULL;
}

#if 0

void _test_set_socket(s_sts_socket *socket, char *code_, char *db_, char *in_, size_t len_)
{
	s_sts_message_node *node=sts_message_node_create();
	
	node->command = sdsempty();
	node->command = sdscatfmt(node->command, "%s.set", socket->url.dbname);
	node->format = sts_sdsnew("json");
	node->key = sdsempty();
	node->key = sdscatfmt(node->key, "%s.%s", code_, db_);
	node->nodes = sts_sdsnode_create(in_, len_);

	sts_redis_send_message(socket, node);
	
	sts_message_node_destroy(node);
}

int main()
{
	s_sts_url url = {
		.protocol = "redis",
		.ip = "127.0.0.1",
		.port = 6379,
		.dbname = "stsdb",
		.auth = false,
		.username = "",
		.password = ""
	};

	s_sts_socket *socket = sts_socket_create(&url,STS_NET_ROLE_REQ);
	sts_redis_open(socket);

	char info[] = "{\"name\"=\"123456\"}";
	_test_set_socket(socket, "sh900600", "info", info, strlen(info));

	sts_redis_close(socket);
}


#endif