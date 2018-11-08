
#include "sis_redis.h"

 
void sis_redis_open(s_sis_socket *sock_)
{
	if (!sock_) return ;
	sock_->handle_read = NULL;
	sock_->status = SIS_NET_WAITCONNECT;
}
void sis_redis_close(s_sis_socket *sock_)
{
	if (!sock_)
	{
		return;
	}	
	if (sock_->handle_read)
	{
		redisFree((redisContext *)sock_->handle_read);
	}
	sock_->status = SIS_NET_CONNECTSTOP;
}
void _sis_redis_connect(s_sis_socket *sock_)
{
	if(sock_->status == SIS_NET_CONNECTSTOP) 
	{
		sis_out_log(5)("connect already close.\n");
		return ;
	}
	sock_->status = SIS_NET_CONNECTING;

	s_sis_socket *sock = sock_;
	redisContext *context = NULL;
	struct timeval timeout = { 1, 500000 }; // 1.5 seconds
	context = redisConnectWithTimeout(sock->url.ip, sock->url.port, timeout);
	if (context == NULL || context->err)
	{
		if (context)
		{
			sis_out_log(3)("connection error: %s\n", context->errstr);
			redisFree(context);
			context = NULL;
		}
		else
		{
			sis_out_log(3)("connection error: can't allocate redis context\n");
		}
	}
	else
	{
		if (!sock->url.auth) 
		{
			sock->handle_read = context;
			sock->status = SIS_NET_CONNECTED;
			return;			
		}
		redisReply *reply;
		printf("auth %s.\n", sock->url.password);
		reply = (redisReply *)redisCommand(context, "auth %s", sock->url.password);
		if (reply)
		{
			freeReplyObject(reply);
			sock->handle_read = context;
			sock->status = SIS_NET_CONNECTED;
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
	sock->status = SIS_NET_WAITCONNECT;
	if (sock->handle_read)
	{
		redisFree((redisContext *)sock->handle_read);
		sock->handle_read = NULL;
	}
}

bool _sis_redis_check_connect(s_sis_socket *sock)
{
	if (!sock)
	{
		return false;
	}
	if(sock->status == SIS_NET_WAITCONNECT) 
	{
		_sis_redis_connect(sock);
	}
	return (sock->status==SIS_NET_CONNECTED);
}
	// s_sis_sds	command;   //来源信息专用,当前消息的key  sisdb.get 
	// s_sis_sds	key;       //来源信息专用,当前消息的key  sh600600.day 
	// s_sis_sds	argv;      //来源信息的参数，为json格式, 无需返回
	// s_sis_sds	address;   //来源信息专用, 数据来源信息，需要原样返回；用户写的投递地址

	// s_sis_sds   format;    //set发送的数据格式 struct json

	// s_sis_list_node   *links;     //来源信息专用, 数据来源链路，每次多一跳就增加一个节点
	// s_sis_list_node   *nodes;     //附带的信息数据链表  node->value 为 s_sis_sds 类型

int _sis_redis_get_cmds(s_sis_message_node * mess_)
{
	int o = 0;
	if (mess_->command) o++;
	if (mess_->key) o++;
	if (mess_->address) o++;
	if (mess_->format) o++;
	o += sis_sdsnode_get_count(mess_->nodes);
	return o++;
}
bool sis_redis_send_message(s_sis_socket *sock_, s_sis_message_node * mess_)
{
	printf("redis send %s. status=%d [%s:%d]\n", mess_->key, sock_->status, sock_->url.ip, sock_->url.port);
	if (!_sis_redis_check_connect(sock_)){
		printf("redis check connect fail.\n");
		return false;
	}

	int argc = _sis_redis_get_cmds(mess_);
	char **argvstr = (char **)sis_malloc(sizeof(char *)* argc);
	size_t *argvlen = (size_t *)sis_malloc(sizeof(size_t)* argc);
	memset(argvstr, 0, sizeof(char *)* argc);
	memset(argvlen, 0, sizeof(size_t)* argc);

	int index = 0;
	if (mess_->command)
	{
		argvlen[index] = sis_sdslen(mess_->command);
		argvstr[index] = mess_->command;
		index++;
	}

	if (mess_->key)
	{
		argvlen[index] = sis_sdslen(mess_->key);
		argvstr[index] = mess_->key;
		index++;
	}

	if (mess_->format)
	{
		argvlen[index] = sis_sdslen(mess_->format);
		argvstr[index] = mess_->format;
		index++;
	}

	s_sis_list_node *node = mess_->nodes;
	while(node) 
	{
		argvlen[index] = sis_sdslen(node->value);
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
		sock_->status = SIS_NET_WAITCONNECT;
	}
	sis_free((void *)argvstr);
	sis_free((void *)argvlen);
	return true;
}

s_sis_message_node *sis_redis_query_message(s_sis_socket *sock_, s_sis_message_node *mess_)
{
	return NULL;
}

#if 0

void _test_set_socket(s_sis_socket *socket, char *code_, char *db_, char *in_, size_t len_)
{
	s_sis_message_node *node=sis_message_node_create();
	
	node->command = sdsempty();
	node->command = sdscatfmt(node->command, "%s.set", socket->url.dbname);
	node->format = sis_sdsnew("json");
	node->key = sdsempty();
	node->key = sdscatfmt(node->key, "%s.%s", code_, db_);
	node->nodes = sis_sdsnode_create(in_, len_);

	sis_redis_send_message(socket, node);
	
	sis_message_node_destroy(node);
}

int main()
{
	s_sis_url url = {
		.protocol = "redis",
		.ip = "127.0.0.1",
		.port = 6379,
		.dbname = "sisdb",
		.auth = false,
		.username = "",
		.password = ""
	};

	s_sis_socket *socket = sis_socket_create(&url,SIS_NET_ROLE_REQ);
	sis_redis_open(socket);

	char info[] = "{\"name\"=\"123456\"}";
	_test_set_socket(socket, "sh900600", "info", info, strlen(info));

	sis_redis_close(socket);
}


#endif