
#include "sis_redis.h"
#include "sis_comm.h"

 
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
	// s_sis_sds	source;   //来源信息专用, 数据来源信息，需要原样返回；用户写的投递地址

	// s_sis_list_node   *links;     //来源信息专用, 数据来源链路，每次多一跳就增加一个节点
	// s_sis_list_node   *nodes;     //附带的信息数据链表  node->value 为 s_sis_sds 类型

int _sis_redis_get_cmds(s_sis_net_message * mess_)
{
	int o = 0;
	if (mess_->command) o++;
	if (mess_->key) o++;
	if (mess_->argv) o++;
	if (mess_->source) o++;
	o += sis_sdsnode_get_count(mess_->argvs);
	return o++;
}
bool sis_redis_send_message(s_sis_socket *sock_, s_sis_net_message * mess_)
{
	// printf("redis send %s. status=%d [%s:%d]\n", mess_->key, sock_->status, sock_->url.ip, sock_->url.port);
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

	if (mess_->argv)
	{
		// printf("argv len=%d. \n", sis_sdslen(mess_->argv));	
		argvlen[index] = sis_sdslen(mess_->argv);
		argvstr[index] = mess_->argv;
		index++;
	}

	s_sis_list_node *node = mess_->argvs;
	while(node) 
	{
		// printf("node len=%d. \n", sis_sdslen(node->value));	
		argvlen[index] = sis_sdslen(node->value);
		argvstr[index] = node->value;
		index++;
		node = node->next;
	}

	redisReply *reply;

	reply = (redisReply *)redisCommandArgv((redisContext *)sock_->handle_read, index, (const char **)&(argvstr[0]), &(argvlen[0]));
	if (reply)
	{
		// printf("send data type %s [%d:%s] \n", mess_->key, reply->type, reply->str);		
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

s_sis_net_message *sis_redis_query_message(s_sis_socket *sock_, s_sis_net_message *mess_)
{
	// printf("redis send %s. status=%d [%s:%d]\n", mess_->key, sock_->status, sock_->url.ip, sock_->url.port);
	if (!_sis_redis_check_connect(sock_)){
		printf("redis check connect fail.\n");
		return NULL;
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

	if (mess_->argv)
	{
		argvlen[index] = sis_sdslen(mess_->argv);
		argvstr[index] = mess_->argv;
		index++;
	}

	s_sis_net_message *node = NULL;

	redisReply *reply;

	reply = (redisReply *)redisCommandArgv((redisContext *)sock_->handle_read, index, (const char **)&(argvstr[0]), &(argvlen[0]));
	if (reply)
	{
		node = mess_;
		if(reply->type == REDIS_REPLY_STRING) 
		{
			node->argvs = sis_sdsnode_create(reply->str, reply->len);
		} else {

		}
		// printf("query data type %s (%s) [%d:%s] \n", mess_->argv, mess_->key, (int)reply->len, reply->str);		
		freeReplyObject(reply);
	} else 
	{
		printf("connect break. \n");	
		sock_->status = SIS_NET_WAITCONNECT;
	}
	sis_free((void *)argvstr);
	sis_free((void *)argvlen);
	return node;
}

#if 0


int main()
{
	s_sis_url_v1 url = {
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

	// char info[] = "{\"name\"=\"123456\"}";
	// _test_set_socket(socket, "sh900600", "info", info, strlen(info));
	_test_get_socket(socket, "sh", "exch", "{\"format\":\"struct\"}", 20); 

	sis_redis_close(socket);
}


#endif

#if 0
#include "sis_net.node.h"

#define  STREAM_SEND_COUNT  1000*1000
// #define  STREAM_SEND_SIZE   100*1000
#define  STREAM_SEND_SIZE   100

void _test_set_socket(s_sis_socket *socket, char *code_, char *db_, char *in_, size_t len_)
{
	s_sis_net_message *node=sis_socket_node_create();
	
	node->command = sdsempty();
	// node->command = sdscatfmt(node->command, "%s.set", socket->url.dbname);
	node->command = sdscatfmt(node->command, "set");
	node->argv = sdsempty();
	node->key = sdsempty();
	node->key = sdscatfmt(node->key, "%s.%s", code_, db_);
	node->argvs = sis_sdsnode_create(in_, len_);

	sis_redis_send_message(socket, node);
	
	sis_socket_node_destroy(node);
}

void _test_get_socket(s_sis_socket *socket, char *code_, char *db_, char *in_, size_t len_)
{
	s_sis_net_message *node=sis_socket_node_create();
	
	node->command = sdsempty();
	// node->command = sdscatfmt(node->command, "%s.get", socket->url.dbname);
	// node->argv = sis_sdsnew("json");
	node->command = sdscatfmt(node->command, "get");
	node->key = sdsempty();
	node->key = sdscatfmt(node->key, "%s.%s", code_, db_);
	node->argv = sdsnewlen(in_, len_);

	s_sis_net_message *reply = sis_socket_query_message(socket, node);
	if (reply)
	{
		printf("reply : %s\n",(char *)reply->argvs->value);
	}	
	sis_socket_node_destroy(node);
}
s_sis_net_message *sis_redis_query_message_stream(s_sis_socket *sock_, s_sis_net_message *mess_)
{
	printf("redis send %s. status=%d [%s:%d]\n", mess_->key, sock_->status, sock_->url.ip, sock_->url.port);
	if (!_sis_redis_check_connect(sock_)){
		printf("redis check connect fail.\n");
		return NULL;
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

	if (mess_->argv)
	{
		argvlen[index] = sis_sdslen(mess_->argv);
		argvstr[index] = mess_->argv;
		index++;
	}

	s_sis_list_node *node = mess_->argvs;
	while(node) 
	{
		// printf("node len=%d. \n", sis_sdslen(node->value));	
		argvlen[index] = sis_sdslen(node->value);
		argvstr[index] = node->value;
		index++;
		node = node->next;
	}

	s_sis_net_message *out = NULL;

	redisReply *reply;

	reply = (redisReply *)redisCommandArgv((redisContext *)sock_->handle_read, index, (const char **)&(argvstr[0]), &(argvlen[0]));
	if (reply)
	{
		out = mess_;
		if(reply->type == REDIS_REPLY_STRING) 
		{
			out->argvs = sis_sdsnode_create(reply->str, reply->len);
		} else {

		}
		printf("query data type %s (%s) [%d:%s] \n", mess_->argv, mess_->key, (int)reply->len, reply->str);		
		freeReplyObject(reply);
	} else 
	{
		printf("connect break. \n");	
		sock_->status = SIS_NET_WAITCONNECT;
	}
	sis_free((void *)argvstr);
	sis_free((void *)argvlen);
	return out;
}


void _test_send_socket(s_sis_socket *socket, char *code_, char *db_, char *in_, size_t len_)
{
	s_sis_net_message *node=sis_socket_node_create();
	
	node->command = sdsnew("xadd");
	node->key = sdsnew(db_);
	node->argv = sdsnew("*");
	node->argvs = sis_sdsnode_create(code_, strlen(code_));
	node->argvs = sis_sdsnode_push_node(node->argvs,in_,len_);

	s_sis_net_message *reply = sis_socket_query_message(socket, node);
	if (reply)
	{
		// printf("reply : %s\n",(char *)reply->argvs->value);
	}	
	sis_socket_node_destroy(node);
}
int _test_recv_socket(s_sis_socket *socket, char *code_, char *db_, char *in_, size_t len_)
{
	int o =0;
	s_sis_net_message *node=sis_socket_node_create();
	
	node->command = sdsnew("xread");
	node->key = sdsnew("block");
	node->argv = sdsnew("0");
	node->argvs = sis_sdsnode_create("streams", strlen("streams"));
	node->argvs = sis_sdsnode_push_node(node->argvs,db_,strlen(db_));
	node->argvs = sis_sdsnode_push_node(node->argvs,"$",1);

	s_sis_net_message *reply = sis_socket_query_message(socket, node);
	if (reply)
	{
		printf("reply : %s\n",(char *)reply->argvs->value);
		o = sis_sdslen((s_sis_sds)reply->argvs->value);
	}	
	sis_socket_node_destroy(node);
	return o;
}
#include "sis_time.h"
int main(int argc, char *argv[])
{
	s_sis_url_v1 url = {
		.protocol = "redis",
		// .ip = "127.0.0.1",
		.ip = "192.168.3.118",
		.port = 6379,
		.dbname = "sisdb",
		.auth = true,
		.username = "",
		.password = "clxx1110"
	};

	s_sis_socket *socket = sis_socket_create(&url,SIS_NET_ROLE_REQ);
    if(socket) 
    {
        socket->open = sis_redis_open;
        socket->close = sis_redis_close;
        socket->socket_send_message = NULL;
        socket->socket_query_message = sis_redis_query_message_stream;        
        sis_socket_open(socket);   
    }
	

	// char info[] = "{\"name\"=\"123456\"}";
	// _test_set_socket(socket, "sh900600", "info", info, strlen(info));
	char buffer[STREAM_SEND_SIZE];
	memset(buffer, 1, STREAM_SEND_SIZE);

	if (argc > 1)
	{
		size_t bytes = 0;
		int count =0;
		size_t step = (int)(STREAM_SEND_COUNT/10);
		if (step==0) step =1;
		// 接收数据
		time_t start = sis_time_get_now();
		while(1)
		{
			int size = _test_recv_socket(socket, "sh", "market123", NULL, 0); 
			if (size>0)
			{
				count++;
				bytes +=size;
				if (count%step==0)
				{
					printf("[%10d] recv size=%d\n", count, (int)bytes);		
					printf("-----recv : %d = %d \n", (int)bytes, (int)(sis_time_get_now() - start));		
				}
			}
		}		
	}
	else
	{
		// 发送数据
		size_t step = STREAM_SEND_COUNT/10;
		if (step==0) step =1;
		time_t start = sis_time_get_now();
		for(int i = 0; i < STREAM_SEND_COUNT; i++)
		{
			// _test_set_socket(socket, "sh", "market", buffer, STREAM_SEND_SIZE);
			_test_send_socket(socket, "sh", "market1", buffer, STREAM_SEND_SIZE); 
			if (i%step==0)
			printf("[%10d] send size=%d\n", (int)i, i*STREAM_SEND_SIZE);
		}
		printf("-----send end. [%d] = %d \n", STREAM_SEND_COUNT, (int)(sis_time_get_now() - start));		
	}
	

	sis_redis_close(socket);
}
#endif