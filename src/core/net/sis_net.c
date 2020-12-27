
#include <sis_net.ws.h>
#include <sis_net.rds.h>
#include <sis_net.h>
#include <sis_net.node.h>

bool sis_net_is_ip4(const char *ip_)
{
	int size = sis_strlen(ip_);
	int ilen = 0;
	const char *ptr = ip_;
	while (*ptr && strchr(".0123456789", (unsigned char)*ptr))
	{
		ptr++;
		ilen++;
	}
	return ilen >= size;
}

//////////////////////////
// s_sis_url
//////////////////////////

s_sis_url *sis_url_create()
{
	s_sis_url *o = SIS_MALLOC(s_sis_url, o);
	o->dict = sis_map_sds_create();	
	return o;
}
void sis_url_destroy(void *url_)
{
	s_sis_url *url = (s_sis_url *)url_;
	sis_map_sds_destroy(url->dict);
	sis_free(url);
}

void sis_url_clone(s_sis_url *src_, s_sis_url *des_)
{	
	des_->io = src_->io;
	des_->role = src_->role;
	des_->version = src_->version;
	des_->protocol = src_->protocol;
	des_->protocol = src_->protocol;
	des_->compress = src_->compress;
	des_->crypt = src_->crypt;
	sis_strcpy(des_->ip4, 16, src_->ip4);
	sis_strcpy(des_->name, 128, src_->name);
	des_->port = src_->port;
	// sis_strcpy(des_->username, 128, src_->username);
	// sis_strcpy(des_->password, 128, src_->password);
	// des_->wait_msec = src_->wait_msec;
	// des_->wait_size = src_->wait_size;
	sis_map_sds_clear(des_->dict);
	if (src_->dict)
	{
		s_sis_dict_entry *de;
		s_sis_dict_iter *di = sis_dict_get_iter(src_->dict);
		while ((de = sis_dict_next(di)) != NULL)
		{
			sis_map_sds_set(des_->dict, sis_dict_getkey(de), sis_dict_getval(de));
		}
		sis_dict_iter_free(di);	
	}
}
void sis_url_set(s_sis_url *url_, const char *key, char *val)
{
	sis_map_sds_set(url_->dict, key, val);
}
char *sis_url_get(s_sis_url *url_, const char *key)
{
	return (char *)sis_map_sds_get(url_->dict, key);
}
void sis_url_set_ip4(s_sis_url *url_, const char *ip_)
{
	if (!ip_)
	{
		return ;
	}
	if (sis_net_is_ip4(ip_))
	{
		sis_strcpy(url_->ip4, 16, ip_);
	}
	else
	{
		sis_strcpy(url_->name, 128, ip_);
		sis_socket_getip4(ip_, url_->ip4, 16);
	}
}

bool sis_url_load(s_sis_json_node *node_, s_sis_url *url_)
{
    if (!node_) 
    {
		// 设置默认值
	    return false;
    }
	{
		const char *str = sis_json_get_str(node_, "io"); // connect accept
	    url_->io = str && !sis_strcasecmp(str, "connect") ? SIS_NET_IO_CONNECT : SIS_NET_IO_WAITCNT;
	}
	{
		const char *str = sis_json_get_str(node_, "role"); 
	    url_->role = (str && !sis_strcasecmp(str, "request")) ? SIS_NET_ROLE_REQUEST : SIS_NET_ROLE_ANSWER;
	}
	url_->version = sis_json_get_int(node_, "version", SIS_NET_VERSION);
	{
		const char *str = sis_json_get_str(node_, "protocol");
		if (str && !sis_strcasecmp(str, "redis"))
		{
			url_->protocol = SIS_NET_PROTOCOL_RDS;
		}
		else if (str && !sis_strcasecmp(str, "ws"))
		{
			url_->protocol = SIS_NET_PROTOCOL_WS;
		}
		else
		{
			url_->protocol = SIS_NET_PROTOCOL_TCP;
		}
	}
	{
		const char *str = sis_json_get_str(node_, "compress");
	    url_->compress = str && !sis_strcasecmp(str, "zbit") ? SIS_NET_ZIP_ZBIT : SIS_NET_ZIP_NONE;
	}
	{
		const char *str = sis_json_get_str(node_, "crypt");
	    url_->crypt = str && !sis_strcasecmp(str, "ssl") ? SIS_NET_CRYPT_SSL : SIS_NET_CRYPT_NONE;
	}
	{
		const char *str = sis_json_get_str(node_, "ip");
		if (str)
		{
			sis_url_set_ip4(url_, str);
		}
		else
		{
			sis_strcpy(url_->ip4, 16, "0.0.0.0");
		}
	}
	url_->port = sis_json_get_int(node_, "port", 7329);
	// {
	// 	const char *str = sis_json_get_str(node_, "username");
	// 	if (str)
	// 	{
	// 		sis_strcpy(url_->username, 128, str);
	// 	}
	// }
	// {
	// 	const char *str = sis_json_get_str(node_, "password");
	// 	if (str)
	// 	{
	// 		sis_strcpy(url_->password, 128, str);
	// 	}
	// }
    s_sis_json_node *argvs = sis_json_cmp_child_node(node_, "argvs");
	if (argvs)
	{
		s_sis_json_node *next = sis_json_first_node(argvs);
		while(next)
		{
			sis_url_set(url_, next->key, next->value);
			LOG(5)("load url info : key = %s, v =%s\n", next->key, next->value);			
			next = next->next;
		} 
	}
    return true;
}
//////////////////////////
// s_sis_net_context
//////////////////////////

s_sis_net_context *sis_net_context_create(s_sis_net_class *cls_, int rid_)
{
	s_sis_net_context *o =SIS_MALLOC(s_sis_net_context, o);
	o->rid = rid_;
	o->father = cls_;
	o->recv_buffer = sis_memory_create();
	o->slots = SIS_MALLOC(s_sis_net_slot, o->slots);
	// 初始化过程全部用 json 交互
	return o;
} 

void sis_net_context_destroy(void *cxt_)
{
	s_sis_net_context *cxt = (s_sis_net_context *)cxt_;

	sis_memory_destroy(cxt->recv_buffer);
	// if(cxt->unpack_memory)
	// {
	// 	sis_memory_destroy(cxt->unpack_memory);
	// }
	sis_free(cxt->slots);
	sis_free(cxt);
}

void sis_net_slot_set(s_sis_net_slot *slots, uint8 compress, uint8 crypt, uint8 protocol)
{
	// coded 必须有值
	// slots->slot_net_encoded = NULL;
	// slots->slot_net_decoded = NULL;		
	slots->slot_net_encrypt = NULL;
	slots->slot_net_decrypt = NULL;
	slots->slot_net_compress = NULL;
	slots->slot_net_uncompress = NULL;
	// slots->slot_net_pack = NULL;  必须有值
	// slots->slot_net_unpack = NULL; 必须有值	

	// 设置回调 这个应该在收到客户端请求后设置 应该针对每个客户进行设置
	if (compress == SIS_NET_ZIP_ZBIT)
	{
		// 暂时不支持
		slots->slot_net_compress = NULL;
		slots->slot_net_uncompress = NULL;
	}
	if (crypt == SIS_NET_CRYPT_SSL)
	{
		slots->slot_net_encrypt = sis_net_ssl_encrypt;
		slots->slot_net_decrypt = sis_net_ssl_decrypt;
	}

	slots->slot_net_encoded = sis_net_encoded_normal;
	slots->slot_net_decoded = sis_net_decoded_normal;

	if (protocol == SIS_NET_PROTOCOL_RDS)
	{
		slots->slot_net_compress = NULL;
		slots->slot_net_uncompress = NULL;
		slots->slot_net_encrypt = NULL;
		slots->slot_net_decrypt = NULL;
		slots->slot_net_encoded = sis_net_encoded_rds;
		slots->slot_net_decoded = sis_net_decoded_rds;		
		slots->slot_net_pack = sis_net_pack_rds;
		slots->slot_net_unpack = sis_net_unpack_rds;		
	}	
	else if (protocol == SIS_NET_PROTOCOL_WS)
	{ 
		slots->slot_net_pack = sis_net_pack_ws;
		slots->slot_net_unpack = sis_net_unpack_ws;		
	}
	else
	{
		slots->slot_net_pack = sis_net_pack_tcp;
		slots->slot_net_unpack = sis_net_unpack_tcp;				
	}
	
}
//////////////////////////
// s_sis_net_class
//////////////////////////
s_sis_object *sis_net_send_message(s_sis_net_context *cxt_, s_sis_net_message *mess_);
int sis_net_recv_message(s_sis_net_context *cxt_, s_sis_memory *in_, s_sis_net_message *mess_);

static int cb_sis_reader_recv(void *cls_, s_sis_object *in_)
{
	sis_object_incr(in_);
	// 从队列中来 往上层用户而去
	s_sis_net_class *cls = (s_sis_net_class *)cls_;

	// LOG(8)("reader recv. %d count = %d\n", SIS_OBJ_NETMSG(in_)->cid, cls->ready_recv_cxts->work_queue->rnums);
	// s_sis_net_message *mess = SIS_OBJ_NETMSG(in_);
	// printf("recv mess: [%d] %x : %s %s %s\n %s\n", mess->cid, mess->style, 
	// 	mess->cmd ? mess->cmd : "nil",
	// 	mess->key ? mess->key : "nil",
	// 	mess->val ? mess->val : "nil",
	// 	mess->rval ? mess->rval : "nil");

	char key[16];
	sis_llutoa(SIS_OBJ_NETMSG(in_)->cid, key, 16, 10);

	// printf("recv argv: %x key = %s [%p] %d\n", mess->style, key, mess->argvs, mess->argvs ? mess->argvs->count : 0);

	s_sis_net_context *cxt = sis_map_pointer_get(cls->cxts, key);
	if (!cxt)
	{
		sis_object_decr(in_);
		return -1;
	}
	// 无论是请求数据或者是订阅数据 只要是返回的数据统一转为 net_message 传给上层 并及时销毁
	if (cxt->cb_reply)
	{
		cxt->cb_reply(cxt->cb_source, SIS_OBJ_NETMSG(in_));
	}
	sis_object_decr(in_);
	return 0;
}

//////////////////////////
// s_sis_net_class
//////////////////////////

s_sis_net_class *sis_net_class_create(s_sis_url *url_)
{
	s_sis_net_class *o = SIS_MALLOC(s_sis_net_class, o);
	
	o->url = sis_url_create();
	sis_url_clone(url_, o->url);

	if (o->url->io == SIS_NET_IO_WAITCNT)
	{
		o->server = sis_socket_server_create();
		o->server->cb_source = o;
		o->server->port = o->url->port;
		sis_strcpy(o->server->ip, 128, o->url->ip4);
	}
	else
	{
		o->client = sis_socket_client_create();
		o->client->cb_source = o;
		o->client->port = o->url->port;
		sis_strcpy(o->client->ip, 128, o->url->ip4);
	}

	o->ready_recv_cxts = sis_lock_list_create(16*1024*1024);
    o->reader_recv = sis_lock_reader_create(o->ready_recv_cxts, 
		SIS_UNLOCK_READER_HEAD, o, cb_sis_reader_recv, NULL);
	sis_lock_reader_open(o->reader_recv);

	o->cxts = sis_map_pointer_create_v(sis_net_context_destroy);

	o->cb_source = o;
	o->work_status = SIS_NET_NONE;

	return o;
}

void sis_net_class_destroy(s_sis_net_class *cls_)
{
	s_sis_net_class *cls = (s_sis_net_class *)cls_;

	cls->work_status = SIS_NET_EXIT;

	if (cls->server)
	{
		sis_socket_server_destroy(cls->server);
	}
	if (cls->client)
	{
		sis_socket_client_destroy(cls->client);
	}
	sis_url_destroy(cls->url);

	sis_lock_reader_close(cls->reader_recv);
	sis_lock_list_destroy(cls->ready_recv_cxts);

	sis_map_pointer_destroy(cls->cxts);
	LOG(5)("net_class exit .\n");
	sis_free(cls);
}

// 发送握手请求
int sis_ws_send_hand_ask(s_sis_net_class *cls, s_sis_net_context *cxt)
{
	s_sis_memory *ask = sis_memory_create();
	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, ask); 
	sis_net_ws_get_ask(ask);
	if (cls->url->io == SIS_NET_IO_WAITCNT)
	{
		sis_socket_server_send(cls->server, cxt->rid, obj);
	}
	else
	{
		sis_socket_client_send(cls->client, obj);
	}
	sis_object_destroy(obj);	
	return 1; // 成功
}
// 接收握手请求 并回应
int sis_ws_recv_hand_ask(s_sis_net_class *cls, s_sis_net_context *cxt)
{
	int size = sis_net_ws_head_complete(cxt->recv_buffer);
	if (size < 0)
	{
		return 0; // 数据不全
	}
	s_sis_memory *ans = sis_memory_create();
	sis_memory_cat(ans, sis_memory(cxt->recv_buffer), size);
	char key[32];
	if (sis_net_ws_get_key(ans, key))
	{
		sis_memory_destroy(ans);
		return -1; // 失败
	}
	sis_net_ws_get_ans(key, ans);

	printf("send hand ans: %d\n", cxt->rid);
	sis_out_binary("ans",sis_memory(ans), sis_memory_get_size(ans));

	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, ans); 
	if (cls->url->io == SIS_NET_IO_WAITCNT)
	{
		sis_socket_server_send(cls->server, cxt->rid, obj);
	}
	else
	{
		sis_socket_client_send(cls->client, obj);
	}
	sis_memory_move(cxt->recv_buffer, size);
	sis_object_destroy(obj);	
	return 1; // 成功 
}

// 收到回应 判断后返回 这里默认 有101 就是返回正确 不校验key 有空再说
int sis_ws_match_hand_ans(s_sis_net_class *cls, s_sis_net_context *cxt)
{
	int size = sis_net_ws_head_complete(cxt->recv_buffer);
	if (size < 0)
	{
		return 0; // 数据不全
	}
	s_sis_memory *ans = sis_memory_create();
	sis_memory_cat(ans, sis_memory(cxt->recv_buffer), size);
	if (sis_net_ws_chk_ans(ans))
	{
		sis_memory_destroy(ans);
		return -1; // 失败
	}
	sis_memory_destroy(ans);
	sis_memory_move(cxt->recv_buffer, size);	
	return 1; // 成功 
}

static void cb_server_recv_after(void *handle_, int sid_, char* in_, size_t ilen_)
{
	// printf("server recv from [%d] client : %d:||%s\n", sid_, (int)ilen_, in_);	

	// sis_out_binary("recv", in_, ilen_);

	s_sis_net_class *cls = (s_sis_net_class *)handle_;

	char key[32];
	sis_llutoa(sid_, key, 32, 10);
	s_sis_net_context *cxt = sis_map_pointer_get(cls->cxts, key);
	if (!cxt)
	{
		return ;
	}
	sis_memory_cat(cxt->recv_buffer, in_, ilen_);
	if (cxt->status == SIS_NET_HANDING)
	{
		// 未初始化的客户端
		int rtn = sis_ws_recv_hand_ask(cls, cxt);
		if (rtn == 1) // 收到正确的握手信息
		{
			printf("server hand ok wait.\n");
			// 此时仅仅是收到握手正确 还未发送信息给client 等发送应答包成功后才设置状态
			// 这里发送登录信息 
			// if (cls->cb_connected)
			// {
			// 	cls->cb_connected(cls->cb_source, sid_);
			// }			
			// cxt->status = SIS_NET_WORKING;
		}
		else if (rtn != 0) // 收到错误的握手信息
		{
			// 如果返回<零 就说明数据出错
			sis_socket_server_delete(cls->server,sid_);
		} // == 0 还没有收到数据
	}
	if (cxt->status == SIS_NET_WORKING)
	{
		// 需要脱壳 拼接成完整包就写入队列
		while(sis_memory_get_size(cxt->recv_buffer) > 0) 
		{
			s_sis_net_message *mess = sis_net_message_create();
			s_sis_object *obj = sis_object_create(SIS_OBJECT_NETMSG, mess);
			int rtn = sis_net_recv_message(cxt, cxt->recv_buffer, mess);
			// printf("recv decoded rtn.[%d] size = %zu\n", rtn, sis_memory_get_size(cxt->recv_buffer));
            // printf("recv mess: [%d] %x : %s %s %s\n %s\n", mess->cid, mess->style, 
            //     mess->cmd ? mess->cmd : "nil",
            //     mess->key ? mess->key : "nil",
            //     mess->val ? mess->val : "nil",
            //     mess->rval ? mess->rval : "nil");

			if (rtn == 1)
			{
				mess->cid = sid_;
				sis_lock_list_push(cls->ready_recv_cxts, obj);
				// LOG(8)("server list recv. %d count = %d\n", sid_, cls->ready_recv_cxts->work_queue->rnums);
			}
			else if (rtn == 0)
			{
				// 如果返回零 就说明数据不够 等下一个数据过来再处理
				sis_object_destroy(obj);
				break;				
			}
			else
			{
				// 如果返回<零 就说明数据格式有问题 断开连接
				sis_object_destroy(obj);
				sis_socket_server_delete(cls->server,sid_);
				break;
			}		
			sis_object_destroy(obj);
		}
		if (sis_memory_get_size(cxt->recv_buffer) == 0)
		{
			sis_memory_clear(cxt->recv_buffer);
		}
	}
}
// int _send_nums = 0;
// msec_t _send_msec = 0;

static void cb_server_send_after(void* handle_, int sid_, int status_)
{
	s_sis_net_class *cls = (s_sis_net_class *)handle_;
	char key[32];
	sis_llutoa(sid_, key, 32, 10);
	s_sis_net_context *cxt = sis_map_pointer_get(cls->cxts, key);
	if (cxt)
	{
		if (cxt->status != SIS_NET_WORKING)
		{
			LOG(5)("wait working. [%d]\n", cxt->status);
		}
		if (cxt->status == SIS_NET_HANDING)
		{
			printf("server hand ok.\n");
			if (cls->cb_connected)
			{
				cls->cb_connected(cls->cb_source, sid_);
			}
			cxt->status = SIS_NET_WORKING;
		}
	} 
	else
	{
		LOG(5)("no find context. [%d]\n", (int)sis_map_pointer_getsize(cls->cxts));
	}
	// s_sis_net_class *cls = (s_sis_net_class *)handle_;
	// s_sis_net_context *cxt = sis_map_pointer_get(cls->cxts, key);
	// if (cxt)
	// {
	// 	if (cxt->status == SIS_NET_WORKING)
	// 	{
	// 		_send_nums++;
	// 		if (_send_nums % 1000 == 0)
	// 		{
	// 			printf("server send : %d wait :%d cost :%d\n", _send_nums, cxt->send_cxts->count, sis_time_get_now_msec() - _send_msec);
	// 			_send_msec = sis_time_get_now_msec();
	// 		}
	// 		sis_wait_queue_set_busy(cxt->send_cxts, 0);	
	// 	}
	// 	else if (cxt->status == SIS_NET_HANDANS)
	// 	{
	// 		cxt->status = SIS_NET_WORKING;
	// 		sis_wait_queue_set_busy(cxt->send_cxts, 0);	
	// 		LOG(5)("server ans hand. status = %d wait = %d\n", cxt->status, cxt->send_cxts->count);
	// 	}
	// 	else
	// 	{
	// 		LOG(5)("server send fail.\n");
	// 	}
		
	// }
	// else
	// {
	// 	LOG(5)("server send fail. %s\n", key);
	// }
	// printf("server send to [%d] client. %p %d | %d \n", sid_, cxt, status_, 0);	
	// printf("server send to client. %d [%d]\n", sid_, status_);	

	if (status_)
	{
		LOG(5)("send to client fail. [%d:%d]\n", sid_, status_);
	}	
}
// int __count = 0;
static void cb_client_recv_after(void* handle_, int sid_, char* in_, size_t ilen_)
{
	// LOG(8)("client recv from [%d] server : %d:||%s status = %d\n", sid_, (int)ilen_, in_, cxt->status);
	s_sis_net_class *cls = (s_sis_net_class *)handle_;
	s_sis_net_context *cxt = sis_map_pointer_get(cls->cxts, "0");
	if (!cxt)
	{
		return ;
	}
	sis_memory_cat(cxt->recv_buffer, in_, ilen_);
	if (cxt->status == SIS_NET_HANDING && cls->url->protocol == SIS_NET_PROTOCOL_WS)
	{
		int rtn = sis_ws_match_hand_ans(cls, cxt);
		if (rtn == 1)
		{
			printf("client hand ok.\n");
			// sis_wait_queue_set_busy(cxt->send_cxts, 0);	
			// sis_net_slot_set(cxt->slots, cls->url->compress, cls->url->crypt, cls->url->protocol);
			// 这里发送登录信息 
			if (cls->cb_connected)
			{
				cls->cb_connected(cls->cb_source, sid_);
			}			
			cxt->status = SIS_NET_WORKING;
		}
		else if (rtn != 0)
		{
			// 如果返回<零 就说明数据出错
			sis_socket_client_close(cls->client);
		}
	}
	if (cxt->status == SIS_NET_WORKING)
	{
		// 需要脱壳 拼接成完整包就写入队列
		while(sis_memory_get_size(cxt->recv_buffer) > 0) 
		{
			s_sis_net_message *mess = sis_net_message_create();
			s_sis_object *obj = sis_object_create(SIS_OBJECT_NETMSG, mess);
			int rtn = sis_net_recv_message(cxt, cxt->recv_buffer, mess);
			// if (sis_memory_get_size(cxt->recv_buffer) == 130 || sis_memory_get_size(cxt->recv_buffer) == 131)
			// {
			// 	sis_out_binary("wait", sis_memory(cxt->recv_buffer), sis_memory_get_size(cxt->recv_buffer));
			// 	printf("client recv decoded rtn.[%d] %d %zu size = %zu\n", mess->cid, rtn, ilen_, sis_memory_get_size(cxt->recv_buffer));
			// }

			if (rtn == 1)
			{
				mess->cid = sid_;
				// printf("[%zu %zu] size= %zu\n", cxt->recv_buffer->maxsize / 1000, sis_memory_get_size(cxt->recv_buffer), SIS_OBJ_GET_SIZE(obj));
				sis_lock_list_push(cls->ready_recv_cxts, obj);
				// LOG(8)("client list recv. %d count = %d\n", sid_, cls->ready_recv_cxts->work_queue->rnums);
			}			
			else if (rtn == 0)
			{
				// 如果返回非零 就说明数据不够 等下一个数据过来再处理
				sis_object_destroy(obj);
				break;
			}
			else
			{
				LOG(5)("data error.\n");
				// 如果返回<零 就说明数据出错
				sis_object_destroy(obj);
				sis_socket_client_close(cls->client);
				break;				
			}
			sis_object_destroy(obj);
		}
		// 定时清理接收缓存
		if (sis_memory_get_size(cxt->recv_buffer) == 0)
		{
			sis_memory_clear(cxt->recv_buffer);
		}
	}
}
static void cb_client_send_after(void* handle_, int sid_, int status_)
{
	// s_sis_net_class *cls = (s_sis_net_class *)handle_;

	// // char key[32];
	// // sis_llutoa(sid_, key, 32, 10);
	// s_sis_net_context *cxt = sis_map_pointer_get(cls->cxts, "0");
	// if (cxt)
	// {
	// 	if (cxt->status == SIS_NET_WORKING)
	// 	{
	// 		_send_nums++;
	// 		if (_send_nums % 1000 == 0)
	// 		{
	// 			printf("send : %d wait :%d cost :%d\n", _send_nums, cxt->send_cxts->count, sis_time_get_now_msec() - _send_msec);
	// 			_send_msec = sis_time_get_now_msec();
	// 		}
	// 		sis_wait_queue_set_busy(cxt->send_cxts, 0);	
	// 	}
	// 	else
	// 	{
	// 		LOG(5)("client send hand. status = %d\n", cxt->status);
	// 	}
	// }
	// printf("client send to server. %d [%d]\n", sid_, status_);	
	if (status_)
	{
		LOG(5)("send to server fail. [%d:%d]\n", sid_, status_);
	}
}

static void cb_server_connected(void *handle_, int sid_)
{
	printf("new connect . sid_ = %d \n", sid_);	
	s_sis_net_class *cls = (s_sis_net_class *)handle_;

	char key[32];
	sis_llutoa(sid_, key, 32, 10);
	s_sis_net_context *cxt = sis_map_pointer_get(cls->cxts, key);
	if (!cxt)
	{
		cxt = (s_sis_net_context *)sis_net_context_create(cls, sid_); 
		sis_map_pointer_set(cls->cxts, key, cxt);
	}
	else
	{
		sis_memory_clear(cxt->recv_buffer);
	}
	sis_socket_server_set_rwcb(cls->server, sid_, cb_server_recv_after, cb_server_send_after);
	// sis_socket_server_set_rwcb(cls->server, sid_, cb_server_recv_after, NULL);
	sis_net_slot_set(cxt->slots, cls->url->compress, cls->url->crypt, cls->url->protocol);
	if (cls->url->protocol == SIS_NET_PROTOCOL_WS)
	{
		cxt->status = SIS_NET_HANDING;
		// sis_wait_queue_set_busy(cxt->send_cxts, 1); //  此时不发客户的正式数据
		// 暂时不发送连接信号 等待客户端验证后才发送
	}
	else
	{
		cxt->status =  SIS_NET_WORKING;
		if (cls->cb_connected)
		{
			cls->cb_connected(cls->cb_source, sid_);
		}
	}
}
static void cb_server_disconnect(void *handle_, int sid_)
{
	printf("client disconnect . sid_ = %d \n", sid_);	

	s_sis_net_class *cls = (s_sis_net_class *)handle_;

	char key[32];
	sis_llutoa(sid_, key, 32, 10);
	s_sis_net_context *cxt = sis_map_pointer_get(cls->cxts, key);
	if (cxt)
	{
		if (cxt->status == SIS_NET_WORKING)
		{
			cxt->status = SIS_NET_DISCONNECT;
			if (cls->cb_disconnect)
			{
				cls->cb_disconnect(cls->cb_source, sid_);
			}
		}
		// 清理发送队列
		sis_net_class_delete(cls, sid_);
	}

}
static void cb_client_connected(void *handle_, int sid_)
{
	printf("client connected . sid_ = %d \n", sid_);	
	s_sis_net_class *cls = (s_sis_net_class *)handle_;
	s_sis_net_context *cxt = sis_map_pointer_get(cls->cxts, "0");
	if (!cxt)
	{
		cxt = (s_sis_net_context *)sis_net_context_create(cls, sid_); 
		sis_map_pointer_set(cls->cxts, "0", cxt);
	}
	else
	{
		sis_memory_clear(cxt->recv_buffer);
	}
	// printf("connect count = %d \n", sis_map_pointer_getsize(cls->cxts));	
	sis_socket_client_set_rwcb(cls->client, cb_client_recv_after, cb_client_send_after);
	sis_net_slot_set(cxt->slots, cls->url->compress, cls->url->crypt, cls->url->protocol);

	//  客户端连接后立即发送握手包 两者通过后才能发送连接成功的信号
	if (cls->url->io == SIS_NET_IO_CONNECT)
	{
		cls->work_status = SIS_NET_WORKING;
	}

	if (cls->url->protocol == SIS_NET_PROTOCOL_WS)
	{
		// 发送握手请求
		cxt->status = SIS_NET_HANDING;
		// sis_wait_queue_set_busy(cxt->send_cxts, 1);
		sis_ws_send_hand_ask(cls, cxt);
	}
	else
	{
		cxt->status = SIS_NET_WORKING;
		if (cls->cb_connected)
		{
			cls->cb_connected(cls->cb_source, sid_);
		}		
	}

}
static void cb_client_disconnect(void *handle_, int sid_)
{
	printf("client disconnect\n");	
	s_sis_net_class *cls = (s_sis_net_class *)handle_;
	s_sis_net_context *cxt = sis_map_pointer_get(cls->cxts, "0");
	if (cxt)
	{
		if (cxt->status == SIS_NET_WORKING)
		{
			cxt->status = SIS_NET_DISCONNECT;
			if (cls->cb_disconnect)
			{
				cls->cb_disconnect(cls->cb_source, sid_);
			}
		}
		sis_net_class_delete(cls, 0);
	}

	if (cls->url->io == SIS_NET_IO_CONNECT)
	{
		cls->work_status = SIS_NET_NONE;
	}

}
void sis_net_class_delete(s_sis_net_class *cls_, int sid_)
{
	if (sid_ == 0)
	{
		sis_map_pointer_del(cls_->cxts, "0");
	}
	else
	{
		char key[32];
		sis_llutoa(sid_, key, 32, 10);
		sis_map_pointer_del(cls_->cxts, key);
	}
	printf("connect count del = %d \n", (int)sis_map_pointer_getsize(cls_->cxts));	
}
bool sis_net_class_open(s_sis_net_class *cls_)
{
	if (cls_->url->io == SIS_NET_IO_WAITCNT)
	{
		sis_socket_server_set_cb(cls_->server, cb_server_connected, cb_server_disconnect);
		if (!sis_socket_server_open(cls_->server))
		{
			LOG(5)("open server fail.\n");
			return false;
		}
		else
		{
			cls_->work_status = SIS_NET_WORKING;
		}
	}
	else
	{
		cls_->client->cb_source = cls_;
		cls_->client->port = cls_->url->port;
		sis_strcpy(cls_->client->ip, 128, cls_->url->ip4);

		sis_socket_client_set_cb(cls_->client, cb_client_connected, cb_client_disconnect);

		if (!sis_socket_client_open(cls_->client))
		{
			LOG(5)("open client fail.\n");
			return false;			
		}
	}
	return true;
}
void sis_net_class_close(s_sis_net_class *cls_)
{
	if (cls_->url->io == SIS_NET_IO_WAITCNT)
	{
		sis_socket_server_close(cls_->server);
	}
	else
	{
		sis_socket_client_close(cls_->client);
	}
}
// 未连接时也需要设置
int sis_net_class_set_cb(s_sis_net_class *cls_, int sid_, void *source_, cb_net_reply cb_)
{
	char key[32];
	sis_llutoa(sid_, key, 32, 10);
	s_sis_net_context *cxt = sis_map_pointer_get(cls_->cxts, key);
	if (cxt)
	{
		cxt->cb_source = source_;
		cxt->cb_reply = cb_;
	}
	return 0;
}
int sis_net_class_set_slot(s_sis_net_class *cls_, int sid_, char *compress_, char * crypt_, char * protocol_)
{
	char key[32];
	sis_llutoa(sid_, key, 32, 10);
	s_sis_net_context *cxt = sis_map_pointer_get(cls_->cxts, key);
	uint8 compress = SIS_NET_ZIP_NONE;
	uint8 crypt = SIS_NET_CRYPT_NONE;
	uint8 protocol = SIS_NET_PROTOCOL_TCP;

	if (protocol_ && !sis_strcasecmp(protocol_, "redis"))
	{
		protocol = SIS_NET_PROTOCOL_RDS;
	}
	else if (protocol_ && !sis_strcasecmp(protocol_, "ws"))
	{
		protocol = SIS_NET_PROTOCOL_WS;
	}
	compress = compress_ && !sis_strcasecmp(compress_, "zbit") ? SIS_NET_ZIP_ZBIT : SIS_NET_ZIP_NONE;
	crypt = crypt_ && !sis_strcasecmp(crypt_, "ssl") ? SIS_NET_CRYPT_SSL : SIS_NET_CRYPT_NONE;

	sis_net_slot_set(cxt->slots, compress, crypt, protocol);

	return 0;
}

int sis_net_class_send(s_sis_net_class *cls_, s_sis_net_message *mess_)
{
	if(cls_->work_status != SIS_NET_WORKING)
	{
		LOG(5)("net no working. %d\n", cls_->work_status);
		return -1;
	}
	sis_net_message_incr(mess_);
	char key[32];
	sis_llutoa(mess_->cid, key, 32, 10);
	s_sis_net_context *cxt = sis_map_pointer_get(cls_->cxts, key);
	if (cxt)
	{
		// printf("reader read +++ [%d] %d | %p \n", mess_->cid, cxt->send_cxts->count ,cxt->send_cxts);
		s_sis_object *sendobj = sis_net_send_message(cxt, mess_); 
		// sis_out_binary("send", SIS_OBJ_GET_CHAR(obj), SIS_OBJ_GET_SIZE(obj));
		if (sendobj)
		{
			if (cls_->url->io == SIS_NET_IO_WAITCNT)
			{
				sis_socket_server_send(cls_->server, mess_->cid, sendobj);
			}
			else
			{
				sis_socket_client_send(cls_->client, sendobj);
			}
			// 这里要等待发送完毕
			sis_object_destroy(sendobj);
		}	
	}
	sis_net_message_decr(mess_);
	return 0;
}

////////////////////////////////////////////////
//    s_sis_net_class other define     
////////////////////////////////////////////////
int sis_net_recv_message(s_sis_net_context *cxt_, s_sis_memory *in_, s_sis_net_message *mess_)
{
	s_sis_net_slot *call = cxt_->slots;
	s_sis_memory *inmem = sis_memory_create();
	s_sis_memory *inmemptr = inmem;
	// 成功或者不成功 in_ 都会移动位置到新的位置
	s_sis_memory_info info;
	int rtn = call->slot_net_unpack(in_, &info, inmemptr);
	if (rtn == 0)
	{
		// 数据不完整 拼接数据
		sis_memory_destroy(inmemptr);
		return 0;
	} 
	else if (rtn == -1)
	{
		sis_memory_destroy(inmemptr);
		return -1;
	}
	if (info.is_bytes)
	{
		s_sis_memory *outmem = sis_memory_create();
		s_sis_memory *outmemptr = outmem;
		if (info.is_crypt && call->slot_net_decrypt)
		{
			if (call->slot_net_decrypt(inmemptr, outmemptr))
			{
				SIS_MEMORY_SWAP(inmemptr, outmemptr);
			}
		}
		if (info.is_compress && call->slot_net_uncompress)
		{
			if (call->slot_net_uncompress(inmemptr, outmemptr))
			{
				SIS_MEMORY_SWAP(inmemptr, outmemptr);
			}
		}
		sis_memory_destroy(outmemptr);
	}

	mess_->format = info.is_bytes;  // 这里设置数据区格式

 	if (call->slot_net_decoded)
	{
		if (!(call->slot_net_decoded(inmemptr, mess_)))
		{
			sis_memory_destroy(inmemptr);			
			return -2;
		}
	}
	sis_memory_destroy(inmemptr);
	return 1;
}

// int sis_net_recv_message(s_sis_net_context *cxt_, s_sis_memory *in_, s_sis_net_message *mess_)
// {
// 	s_sis_net_slot *call = cxt_->slots;
// 	if (!cxt_->unpack_memory)
// 	{
// 		cxt_->unpack_memory = sis_memory_create();
// 	}
// 	s_sis_memory *inmem = cxt_->unpack_memory;
// 	s_sis_memory *inmemptr = inmem;
// 	s_sis_memory_info info;
// 	int complete = call->slot_net_unpack(in_, &info, inmemptr);
// 	if (rtn == 0)
// 	{
// 		// 数据不完整 等待拼接下一块数据
// 		return 0;
// 	} 
// 	else if (rtn == -1)
// 	{
// 		goto _xx_;
// 	}
// 	// rtn == 1 表示数据完整 
// 	if (info.is_bytes)
// 	{
// 		s_sis_memory *outmem = sis_memory_create();
// 		s_sis_memory *outmemptr = outmem;
// 		if (info.is_crypt && call->slot_net_decrypt)
// 		{
// 			if (call->slot_net_decrypt(inmemptr, outmemptr))
// 			{
// 				SIS_MEMORY_SWAP(inmemptr, outmemptr);
// 			}
// 		}
// 		if (info.is_compress && call->slot_net_uncompress)
// 		{
// 			if (call->slot_net_uncompress(inmemptr, outmemptr))
// 			{
// 				SIS_MEMORY_SWAP(inmemptr, outmemptr);
// 			}
// 		}
// 		sis_memory_destroy(outmemptr);
// 	}

// 	mess_->format = info.is_bytes;  // 这里设置数据区格式

//  	if (call->slot_net_decoded)
// 	{
// 		if (!(call->slot_net_decoded(inmemptr, mess_)))
// 		{
// 			goto _xx_;
// 		}
// 	}
// _ok_:  // 完全正确
// 	sis_memory_destroy(inmemptr);
// 	cxt_->unpack_memory = NULL;	
// 	return 1;
// _xx_:  // 出现错误
// 	sis_memory_destroy(inmemptr);
// 	cxt_->unpack_memory = NULL;	
// 	return -1;
// }
s_sis_object *sis_net_send_message(s_sis_net_context *cxt_, s_sis_net_message *mess_)
{
	s_sis_net_slot *call = cxt_->slots;
	s_sis_memory *inmem = sis_memory_create();
	s_sis_memory *inmemptr = inmem;
	if (call->slot_net_encoded)
	{
		if (!call->slot_net_encoded(mess_, inmemptr))
		{
			sis_memory_destroy(inmem);
			return NULL;
		}
	}
	s_sis_memory *outmem = sis_memory_create();
	s_sis_memory *outmemptr = outmem;

	s_sis_memory_info info;
	info.is_bytes = mess_->format == SIS_NET_FORMAT_BYTES ? 1 : 0;
	info.is_compress = 0;
	info.is_crypt = 0;
	info.is_crc16 = 0;
	if (info.is_bytes)
	{
		if (call->slot_net_compress)
		{
			if (call->slot_net_compress(inmemptr, outmemptr))
			{
				info.is_compress = 1;
				SIS_MEMORY_SWAP(inmemptr, outmemptr);
			}
		}
		if (call->slot_net_encrypt)
		{
			if (call->slot_net_encrypt(inmemptr, outmemptr))
			{
				info.is_crypt = 1;
				SIS_MEMORY_SWAP(inmemptr, outmemptr);
			}
		}
	}
	if (call->slot_net_pack)
	{
		if (call->slot_net_pack(inmemptr, &info, outmemptr))
		{
			SIS_MEMORY_SWAP(inmemptr, outmemptr);
		}
	}
	sis_memory_destroy(outmemptr);

	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, inmemptr);

	return obj;
}

#if 0
// 
#include "sis_net.io.h"

#define TEST_PORT 7329
#define TEST_SIP "0.0.0.0"
#define TEST_IP "127.0.0.1"
int __sno = 0; 
s_sis_net_class *session = NULL;

int __exit = 0;

void exithandle(int sig)
{
	printf("exit .1. \n");	
	__exit = 1;
	if (session)
	{
		sis_net_class_destroy(session);
	}
	printf("exit .ok. \n");
	__exit = 2;
}
void cb_recv(void *sock_, s_sis_net_message *msg)
{
	s_sis_net_class *socket = (s_sis_net_class *)sock_;
	if (msg->style & SIS_NET_ASK)
	{
		printf("recv query: [%d] %d : %s %s %s [++%d++]\n", msg->cid, __sno, 
			msg->cmd ? msg->cmd : "nil",
			msg->key ? msg->key : "nil",
			msg->val ? msg->val : "nil",
			(int)sis_map_pointer_getsize(socket->cxts));
		s_sis_sds reply = sis_sdsempty();
		reply = sis_sdscatfmt(reply, "%S %S ok.", msg->cmd, msg->key);
		
		// sis_net_ans_with_chars(msg, reply, sis_sdslen(reply));
		sis_net_ans_with_bytes(msg, reply);
		// sis_sleep(3000);
		sis_net_class_send(socket, msg);
		// sis_net_class_send(socket, msg);
		// sis_net_class_send(socket, msg);
				
		// sis_net_class_send(socket, msg); // 连续发送会出错
	}
	else
	{
		printf("recv msg: [%d] %d : %s\n", msg->cid, msg->style, msg->rval ? msg->rval : "nil");
	}
	
}
static void _cb_connected(void *handle_, int sid)
{
	s_sis_net_class *socket = (s_sis_net_class *)handle_;
	
	sis_net_class_set_cb(socket, sid, socket, cb_recv);

	if (socket->url->role== SIS_NET_ROLE_REQUEST)
	{		
		s_sis_net_message *msg = sis_net_message_create();
	    msg->cid = sid;
		// sis_net_ask_with_chars(msg, "set", "myname", "ding", 4);
		sis_net_ask_with_bytes(msg, "set", "myname", "ding", 4);

		int rtn = sis_net_class_send(socket, msg);

		// s_sis_net_message *msg1 = sis_net_message_create();
		// sis_net_ask_with_bytes(msg1, "set", "myname1", "ding", 4);
		// sis_net_class_send(socket, msg1);
		// sis_net_message_destroy(msg1);

		// s_sis_net_message *msg2 = sis_net_message_create();
		// sis_net_ask_with_bytes(msg2, "set", "myname2", "ding", 4);
		// sis_net_class_send(socket, msg2);
		// sis_net_message_destroy(msg2);

		sis_net_message_destroy(msg);
		LOG(5)("client [%d] connect ok. rtn = [%d]\n", sid, rtn);	
	}
	else
	{
		LOG(5)("client connect ok. [%d]\n", sid);

	}
}
static void _cb_disconnect(void *handle_, int sid)
{
	s_sis_net_class *socket = (s_sis_net_class *)handle_;
	if (socket->url->role== SIS_NET_ROLE_REQUEST)
	{
		LOG(5)("client disconnect.\n");	
	}
	else
	{
		LOG(5)("server disconnect.\n");	
	}
}


int main(int argc, const char **argv)
{
	if (argc < 2)
	{
		return 0;
	}
	safe_memory_start();

	signal(SIGINT, exithandle);

	s_sis_url url_srv = { SIS_NET_IO_WAITCNT, SIS_NET_ROLE_ANSWER, 1, SIS_NET_PROTOCOL_WS, 0, 0, TEST_SIP, TEST_PORT, NULL};
	s_sis_url url_cli = { SIS_NET_IO_CONNECT, SIS_NET_ROLE_REQUEST, 1, SIS_NET_PROTOCOL_WS, 0, 0, TEST_IP, TEST_PORT, NULL};
	// s_sis_url url_srv = { SIS_NET_IO_WAITCNT, SIS_NET_ROLE_REQUEST, 1, SIS_NET_PROTOCOL_WS, 0, 0, TEST_IP, TEST_PORT, NULL};
	// s_sis_url url_cli = { SIS_NET_IO_CONNECT, SIS_NET_ROLE_ANSWER, 1, SIS_NET_PROTOCOL_WS, 0, 0, TEST_IP, TEST_PORT, NULL};
	// s_sis_url url_srv = { SIS_NET_IO_CONNECT, SIS_NET_ROLE_ANSWER, 1, 0, 0, 0, 0, TEST_IP, TEST_PORT, NULL};
	// s_sis_url url_cli = { SIS_NET_IO_WAITCNT, SIS_NET_ROLE_REQUEST, 1, 0, 0, 0, 0, TEST_IP, TEST_PORT, NULL};
	// s_sis_url url_srv = { SIS_NET_IO_CONNECT, SIS_NET_ROLE_REQUEST, 1, 0, 0, 0, 0, TEST_IP, TEST_PORT, NULL};
	// s_sis_url url_cli = { SIS_NET_IO_WAITCNT, SIS_NET_ROLE_ANSWER, 1, 0, 0, 0, 0, TEST_IP, TEST_PORT, NULL};

	if (argv[1][0] == 's')
	{	
		session = sis_net_class_create(&url_srv);
	}
	else
	{
		session = sis_net_class_create(&url_cli);
	}
	printf("%s:%d\n",session->url->ip, session->url->port);

	session->cb_connected = _cb_connected;
	session->cb_disconnect = _cb_disconnect;

	sis_net_class_open(session);

	sis_sleep(5000);
	if (session->url->role == SIS_NET_ROLE_REQUEST)
	{
		s_sis_net_message *msg = sis_net_message_create();
		msg->cid = 0;
		if(argv[1][0] == 's') 
		{
			msg->cid = 1;
		}
		s_sis_sds reply = sis_sdsnew("this ok.");
		sis_net_ans_with_bytes(msg, reply);
		// sis_net_ask_with_bytes(msg, "pub", "info", "ding", 4);
		for (int i = 0; i < 100000; i++)
		{
			printf("------- %d ------\n", i);
			sis_net_class_send(session, msg);
		}
		// sis_sleep(5000);
		sis_net_message_destroy(msg);
	}

	
	// sis_sleep(10000);
	// if (sis_map_pointer_getsize(session->cxts) == 2)
	// {
	// 	s_sis_net_message *msg = sis_net_message_create();
	// 	sis_net_ask_with_bytes(msg, "pub", "myname", "ding", 4);

	// 	s_sis_net_message *msg2 = sis_net_message_clone(msg);
	// 	msg->cid = 2;
	// 	sis_net_class_send(session, msg);
	// 	msg2->cid = 1;
	// 	sis_net_class_send(session, msg2);

	// 	sis_net_message_destroy(msg);
	// 	sis_net_message_destroy(msg2);
	// }

	while(__exit != 2)
	{
		sis_sleep(300);
	}
	safe_memory_stop();
	return 0;		
}
#endif


#if 0
// 测试打包数据的网络最大流量
// 约每秒30M
#include "sis_net.io.h"

#define TEST_PORT 7329
#define TEST_SIP "0.0.0.0"
#define TEST_IP "127.0.0.1"

s_sis_net_class *session = NULL;

int    connectid = -1;
msec_t start_time = 0;

int64  maxnums = 30*1000;

int    sendnums = 0;
int    recvnums = 0; 
int64  sendsize = 0; 
int64  recvsize = 0; 

int    replynums = 0; 
int64  replysize = 0; 

int64  bagssize = 16000; // 16K

pthread_t  send_thread;

static void cb_recv_data(void *socket_, s_sis_net_message *msg)
{
	s_sis_net_class *socket = (s_sis_net_class *)socket_;

	if (socket->url->role== SIS_NET_ROLE_REQUEST)
	{		
		if (msg->style & (SIS_NET_RCMD | SIS_NET_ARGVS))
		{
			msec_t now_time = sis_time_get_now_msec();
			s_sis_sds reply = SIS_OBJ_GET_CHAR(sis_pointer_list_get(msg->argvs, 0)); 
			recvnums++;
			recvsize+=sis_sdslen(reply);
			msec_t *recv_time = (msec_t *)reply;
			if (recvnums%100==0)
			printf("=1=recv: %d delay = %llu \n", recvnums, now_time - *recv_time);
		}
		else
		{
			printf("=1=recv no type. %x\n", msg->style);
		}
	}
	else
	{	
		if (msg->style & (SIS_NET_ASK | SIS_NET_ARGVS))
		{
			msec_t now_time = sis_time_get_now_msec();
			s_sis_sds reply = SIS_OBJ_GET_CHAR(sis_pointer_list_get(msg->argvs, 0)); 
			recvnums++;
			recvsize+=sis_sdslen(reply);
			msec_t *recv_time = (msec_t *)reply;
			// sis_out_binary(".1.", reply, 16);
			if (recvnums%100==0 || recvnums > maxnums -10 )
			printf("=2=recv: %d delay = %llu \n", recvnums, now_time - *recv_time);

			if (start_time == 0)
			{
				start_time = sis_time_get_now_msec();
			}
			replynums++;
			// replysize+= 8;//sis_sdslen(reply);
			// memmove(reply, &now_time, sizeof(msec_t));
			// // sis_out_binary(".2.", reply, 16);
			// s_sis_net_message *newmsg = sis_net_message_create();
			// newmsg->cid = connectid;
			// // sis_net_ans_with_bytes(newmsg, reply, sis_sdslen(reply));
			// sis_net_ans_with_bytes(newmsg, reply, 8);
			// sis_net_class_send(socket, newmsg);
			// sis_net_message_destroy(newmsg);
		}
		else
		{
			printf("=2=recv no type. %x\n", msg->style);
		}		
	}
}
static void *thread_send_data(void *arg)
{
    while(connectid == -1)
    {
        sis_sleep(10);
    }
	start_time = sis_time_get_now_msec();
	sendnums = 0;
	char imem[bagssize];
	while(sendnums < maxnums)
	{
		sendnums++;
		sendsize+=bagssize;
	    
		msec_t now_time = sis_time_get_now_msec();
		memmove(imem, &now_time, sizeof(msec_t));
		// sis_out_binary(".1.", imem, 16);
		s_sis_net_message *msg = sis_net_message_create();
		msg->cid = connectid;
		sis_net_ask_with_bytes(msg, NULL, NULL, imem, bagssize);
		sis_net_class_send(session, msg);
		sis_net_message_destroy(msg);
		// sis_sleep(1);
	}
    return NULL;
}

static void _cb_connected(void *handle_, int sid)
{
	s_sis_net_class *socket = (s_sis_net_class *)handle_;
	
	sis_net_class_set_cb(socket, sid, socket, cb_recv_data);
	
	connectid = sid;

	if (socket->url->role== SIS_NET_ROLE_REQUEST)
	{		
		// 创建发送线程
		LOG(5)("==client start.  [%d]\n", sid);	
		pthread_create(&send_thread, NULL, thread_send_data, NULL);
		// pthread_create(&send_thread, NULL, thread_send_data, NULL);
	}
	else
	{
		LOG(5)("==server start. [%d]\n", sid);
	}
}
static void _cb_disconnect(void *handle_, int sid)
{
	s_sis_net_class *socket = (s_sis_net_class *)handle_;
	connectid = -1;
	if (socket->url->role== SIS_NET_ROLE_REQUEST)
	{
		LOG(5)("==client stop.\n");	
	}
	else
	{
		LOG(5)("==server stop.\n");	
	}
}
// 单CPU 40M左右 多CPU 60M左右 基本够用 有时间再优化
int exit_ = 0;
void exithandle(int sig)
{
	if (exit_ == 1)
	{
		abort();
	}
	exit_ = 1;
}
void breakhandle(int sig)
{
	printf("..................break......%d \n", sig);
	// abort();
}
int main(int argc, const char **argv)
{
	if (argc < 2)
	{
		return 0;
	}
	for (int i = 1; i < 100000; i++)
	{
		if (i!=SIGSEGV)
		signal(i, breakhandle);
	}
	sis_socket_init();
	signal(SIGINT, exithandle);

	safe_memory_start();

	// s_sis_url url_srv = { SIS_NET_IO_WAITCNT, SIS_NET_ROLE_ANSWER, 1, SIS_NET_PROTOCOL_WS, 0, 0, TEST_SIP, "", TEST_PORT, NULL};
	// s_sis_url url_cli = { SIS_NET_IO_CONNECT, SIS_NET_ROLE_REQUEST, 1, SIS_NET_PROTOCOL_WS, 0, 0, TEST_IP, "", TEST_PORT, NULL};
	s_sis_url url_srv = { SIS_NET_IO_WAITCNT, SIS_NET_ROLE_REQUEST, 1, SIS_NET_PROTOCOL_WS, 0, 0, TEST_SIP, "", TEST_PORT, NULL};
	s_sis_url url_cli = { SIS_NET_IO_CONNECT, SIS_NET_ROLE_ANSWER, 1, SIS_NET_PROTOCOL_WS, 0, 0, TEST_IP, "", TEST_PORT, NULL};

	if (argv[1][0] == 's')
	{	
		session = sis_net_class_create(&url_srv);
	}
	else
	{
		session = sis_net_class_create(&url_cli);
	}

	session->cb_connected = _cb_connected;
	session->cb_disconnect = _cb_disconnect;

	sis_net_class_open(session);
	if (session->url->role== SIS_NET_ROLE_REQUEST)
	{
		// while (sendnums < maxnums || recvnums < maxnums)
		while (1)//sendnums < maxnums || _send_nums < maxnums)
		{
			sis_sleep(100);
			if (exit_) break;
		}

		// sis_sleep(100);
	}
	else
	{
		while (recvnums < maxnums || replynums < maxnums)
		{
			sis_sleep(100);
			if (exit_) break;
		}
		// sis_sleep(100);
	}
	// while (!exit_)
	// {
	// 	sis_sleep(100);
	// }
	int64 costmsec = sis_time_get_now_msec() - start_time;
	printf("==cost %lld ms , send : [%d] %lld(k) ==> recv : [%d] %lld(k) \n", costmsec, sendnums, sendsize/costmsec, recvnums, recvsize/costmsec);  

	sis_net_class_destroy(session);

	safe_memory_stop();
	return 0;		
}
#endif