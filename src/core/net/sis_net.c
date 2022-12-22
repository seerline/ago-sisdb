
#include <sis_net.ws.h>
#include <sis_net.rds.h>
#include <sis_net.h>
#include <sis_net.msg.h>


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
/** 创建一个空白的网络连接配置数据结构体，字段包含IP、端口、加密方式、压缩方式、通讯协议、连接方式、角色方式等*/
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
/**
 * @brief 根据JSON配置网络参数
 * @param node_ JSON配置
 * @param url_ 网络连接参数
 * @return true 成功
 * @return false JSON配置为空
 */
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
	    url_->compress = str && !sis_strcasecmp(str, "snappy") ? SIS_NET_ZIP_SNAPPY : SIS_NET_ZIP_NONE;
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
	o->recv_memory = sis_memory_create();
	o->recv_nodes = sis_net_mems_create();
	o->slots = SIS_MALLOC(s_sis_net_slot, o->slots);
	// 初始化过程全部用 json 交互
	return o;
} 

void sis_net_context_destroy(void *cxt_)
{
	s_sis_net_context *cxt = (s_sis_net_context *)cxt_;
	// ??? 这里要等待内存不再使用后才释放
	sis_memory_destroy(cxt->recv_memory);
	sis_net_mems_destroy(cxt->recv_nodes);
	sis_free(cxt->slots);
	sis_free(cxt);
}

void sis_net_slot_set(s_sis_net_slot *slots, uint8 compress, uint8 crypt, uint8 protocol)
{
	// slots->slot_net_encoded = NULL; // coded 必须有值
	// slots->slot_net_decoded = NULL; // coded 必须有值	
	slots->slot_net_encrypt = NULL;
	slots->slot_net_decrypt = NULL;
	slots->slot_net_compress = NULL;
	slots->slot_net_uncompress = NULL;
	// slots->slot_net_pack = NULL;  必须有值
	// slots->slot_net_unpack = NULL; 必须有值	

	// 设置回调 这个应该在收到客户端请求后设置 应该针对每个客户进行设置
	if (compress == SIS_NET_ZIP_SNAPPY)
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
/** 根据网络配置数据，生成网络连接处理类的对象 */
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

	o->cxts = sis_map_kint_create();
	o->cxts->type->vfree = sis_net_context_destroy;

	o->cb_source = o;
	o->work_status = SIS_NET_NONE;

	return o;
}

void sis_net_class_destroy(s_sis_net_class *cls_)
{
	s_sis_net_class *cls = (s_sis_net_class *)cls_;

	cls->work_status = SIS_NET_EXIT;
	LOG(5)("net_class exit 1.\n");
	if (cls->read_thread)
	{
		sis_wait_thread_destroy(cls->read_thread);
        cls->read_thread = NULL;
	}	
	LOG(5)("net_class exit 2.\n");
	if (cls->server)
	{
		sis_socket_server_destroy(cls->server);
	}
	if (cls->client)
	{
		sis_socket_client_destroy(cls->client);
	}
	sis_url_destroy(cls->url);

	sis_map_kint_destroy(cls->cxts);
	LOG(5)("net_class exit .\n");
	sis_free(cls);
}

// 发送握手请求
int sis_ws_send_hand_ask(s_sis_net_class *cls, s_sis_net_context *cxt)
{
	s_sis_memory *ask = sis_memory_create();
	sis_net_ws_get_ask(ask);
	if (cls->url->io == SIS_NET_IO_WAITCNT)
	{
		sis_socket_server_send(cls->server, cxt->rid, ask);
	}
	else
	{
		sis_socket_client_send(cls->client, ask);
	}
	sis_memory_destroy(ask);
	return 1; // 成功
}
// 接收握手请求 并回应
int sis_ws_recv_hand_ask(s_sis_net_class *cls, s_sis_net_context *cxt)
{
	int size = sis_net_ws_head_complete(cxt->recv_memory);
	if (size < 0)
	{
		return 0; // 数据不全
	}
	s_sis_memory *ans = sis_memory_create();
	sis_memory_cat(ans, sis_memory(cxt->recv_memory), size);
	sis_memory_move(cxt->recv_memory, size);
	char key[32];
	if (sis_net_ws_get_key(ans, key))
	{
		sis_memory_destroy(ans);
		return -1; // 失败
	}
	sis_net_ws_get_ans(key, ans);

	// printf("send hand ans: %d\n", cxt->rid);
	// sis_out_binary("ans",sis_memory(ans), sis_memory_get_size(ans));
	
	if (cls->url->io == SIS_NET_IO_WAITCNT)
	{
		sis_socket_server_send(cls->server, cxt->rid, ans);
	}
	else
	{
		sis_socket_client_send(cls->client, ans);
	}
	sis_memory_destroy(ans);
	return 1; // 成功 
}

// 收到回应 判断后返回 这里默认 有101 就是返回正确 不校验key 有空再说
int sis_ws_match_hand_ans(s_sis_net_class *cls, s_sis_net_context *cxt)
{
	int size = sis_net_ws_head_complete(cxt->recv_memory);
	if (size < 0)
	{
		return 0; // 数据不全
	}
	s_sis_memory *ans = sis_memory_create();
	sis_memory_cat(ans, sis_memory(cxt->recv_memory), size);
	sis_memory_move(cxt->recv_memory, size);
	if (sis_net_ws_chk_ans(ans))
	{
		sis_memory_destroy(ans);
		return -1; // 失败
	}
	sis_memory_destroy(ans);
	return 1; // 成功 
}
/**
 * @brief 数据接收回调函数
 * @param handle_ 
 * @param sid_ session id
 * @param in_ 收到的数据
 * @param ilen_ 数据长度
 */
static void cb_server_recv_after(void *handle_, int sid_, char* in_, size_t ilen_)
{
	// sis_out_binary("recv", in_, ilen_);

	s_sis_net_class *cls = (s_sis_net_class *)handle_;
	s_sis_net_context *cxt = sis_map_kint_get(cls->cxts, sid_);
	if (!cxt || cxt->status == SIS_NET_DISCONNECT)
	{
		return ;
	}
	if (cxt->status == SIS_NET_WORKING)
	{
		//QQQ 如何处理分包的问题？
		sis_net_mems_push(cxt->recv_nodes, in_, ilen_);
		// 发送通知
		// sis_wait_thread_notice(cls->read_thread);
	}
	else if (cxt->status == SIS_NET_HANDING)
	{
		sis_memory_cat(cxt->recv_memory, in_, ilen_);
		int o = sis_ws_recv_hand_ask(cls, cxt);
		if (o == 1) // 收到正确的握手信息
		{
			LOG(8)("server hand ok. [%d]\n", sid_);
			// 这里发送登录信息 
			if (cls->cb_connected)
			{
				cls->cb_connected(cls->cb_source, sid_);
			}	
			// 设置工作状态		
			cxt->status = SIS_NET_WORKING;
			// 如果握手包后有数据就直接通知处理
			if (sis_memory_get_size(cxt->recv_memory) > 0)
			{
				sis_wait_thread_notice(cls->read_thread);
			}
		}
		else if (o != 0) // 收到错误的握手信息
		{
			// 如果返回<零 就说明数据出错
			// sis_net_class_close_cxt(cls, sid_);
			cxt->status = SIS_NET_DISCONNECT;
		} // == 0 还没有收到数据		
	}
}

static void cb_server_send_after(void* handle_, int sid_, int status_)
{
	s_sis_net_class *cls = (s_sis_net_class *)handle_;
	s_sis_net_context *cxt = sis_map_kint_get(cls->cxts, sid_);
	if (!cxt || cxt->status == SIS_NET_DISCONNECT)
	{
		LOG(5)("no find context. [%d:%d]\n", (int)sis_map_kint_getsize(cls->cxts), sid_);
	}
	if (status_)
	{
		LOG(5)("send to client fail. [%d:%d]\n", sid_, status_);
	}	
}
static void cb_client_recv_after(void* handle_, int sid_, char* in_, size_t ilen_)
{
	s_sis_net_class *cls = (s_sis_net_class *)handle_;
	s_sis_net_context *cxt = sis_map_kint_get(cls->cxts, 0);
	if (!cxt || cxt->status == SIS_NET_DISCONNECT)
	{
		return ;
	}
	// printf("recv .... ");
	if (cxt->status == SIS_NET_WORKING)
	{
		sis_net_mems_push(cxt->recv_nodes, in_, ilen_);
		// 发送通知
		// sis_wait_thread_notice(cls->read_thread);
	}
	else if (cxt->status == SIS_NET_HANDING)
	{
		sis_memory_cat(cxt->recv_memory, in_, ilen_);
		int o = sis_ws_match_hand_ans(cls, cxt);
		if (o == 1)
		{
			LOG(8)("client hand ok.\n");
			// 这里发送登录信息 
			if (cls->cb_connected)
			{
				cls->cb_connected(cls->cb_source, sid_);
			}			
			// 设置工作状态		
			cxt->status = SIS_NET_WORKING;
			// 如果握手包后有数据就直接通知处理
			if (sis_memory_get_size(cxt->recv_memory) > 0)
			{
				sis_wait_thread_notice(cls->read_thread);
			}
		}
		else if (o != 0)
		{
			// 如果返回<零 就说明数据出错
			sis_socket_client_close(cls->client);
		}   // == 0 还没有收到数据		
	}
	// printf("ok\n");
}
static void cb_client_send_after(void* handle_, int sid_, int status_)
{
	if (status_)
	{
		LOG(5)("send to server fail. [%d:%d]\n", sid_, status_);
	}

}
/**
 * @brief 新连接处理回调函数，在生成好session，accept连接成功后调用该函数。主要完成以下内容”
 * （1）在HASH表中查找上下文数据，如果找不到，就生成全新的上下文
 * @param sender_ 与当前网络连接关联的网络连接处理类s_sis_net_class
 * @param sid_ 与客户端关联的sessionId,利用该ID可以获取客户端的上下文环境
 */
static void cb_server_connected(void *sender_, int sid_)
{
	LOG(5)("connected. [%d]\n", sid_);	
	s_sis_net_class *cls = (s_sis_net_class *)sender_;
	s_sis_net_context *cxt = sis_map_kint_get(cls->cxts, sid_);
	if (!cxt)
	{
		//QQQ 既然sid是与之前的上下文没有关联的，那么上下文数据也应该是全新的。这里再重新查找有何意义？
		cxt = (s_sis_net_context *)sis_net_context_create(cls, sid_); 
		sis_map_kint_set(cls->cxts, sid_, cxt);
	}
	else
	{
		cxt->status =  SIS_NET_NONE;
		LOG(1)("connect already exists. [%d]\n", sid_);	
		sis_memory_clear(cxt->recv_memory);
		sis_net_mems_clear(cxt->recv_nodes);
	}

	sis_socket_server_set_rwcb(cls->server, sid_, cb_server_recv_after, cb_server_send_after);
	sis_net_slot_set(cxt->slots, cls->url->compress, cls->url->crypt, cls->url->protocol);
	if (cls->url->protocol == SIS_NET_PROTOCOL_WS)
	{
		cxt->status = SIS_NET_HANDING; // 等待握手包
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
	LOG(5)("disconnect . [%d]\n", sid_);	
	s_sis_net_class *cls = (s_sis_net_class *)handle_;
	s_sis_net_context *cxt = sis_map_kint_get(cls->cxts, sid_);
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
		// 关闭对应实例 这里不能关 一旦关闭会和正在处理的线程冲突
		// sis_net_class_close_cxt(cls, sid_);
	}

}
static void cb_client_connected(void *handle_, int sid_)
{
	printf("client connected . sid_ = %d \n", sid_);	
	s_sis_net_class *cls = (s_sis_net_class *)handle_;
	s_sis_net_context *cxt = sis_map_kint_get(cls->cxts, 0);
	if (!cxt)
	{
		cxt = (s_sis_net_context *)sis_net_context_create(cls, sid_); 
		sis_map_kint_set(cls->cxts, 0, cxt);
	}
	else
	{
		cxt->status =  SIS_NET_NONE;
		sis_memory_clear(cxt->recv_memory);
		sis_net_mems_clear(cxt->recv_nodes);
	}
	printf("connect count = %d \n", (int)sis_map_pointer_getsize(cls->cxts));	
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
	// printf("client disconnect\n");	
	s_sis_net_class *cls = (s_sis_net_class *)handle_;
	s_sis_net_context *cxt = sis_map_kint_get(cls->cxts, 0);
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
		// sis_net_class_close_cxt(cls, 0);
	}

	if (cls->url->io == SIS_NET_IO_CONNECT)
	{
		cls->work_status = SIS_NET_NONE;
	}

}
void sis_net_class_close_cxt(s_sis_net_class *cls_, int sid_)
{
	// 这里应该循环检查有没有断开链接的然后删除
	sis_map_kint_del(cls_->cxts, sid_);
	LOG(8)("cxt %d close, now cxt count = %d \n", sid_, (int)sis_map_kint_getsize(cls_->cxts));	
}

// void _make_read_data(s_sis_net_class *cls, s_sis_net_context *cxt)
// {
// 	int count = sis_net_nodes_read(cxt->recv_nodes, 1024);
// 	if (count > 0)
// 	{
// 		s_sis_net_node *next = cxt->recv_nodes->rhead;
// 		int status = 0;
// 		while (next && cxt->status == SIS_NET_WORKING)
// 		{
// 			// sis_out_binary("recv_memory", sis_memory(cxt->recv_memory), sis_memory_get_size(cxt->recv_memory));
// 			s_sis_net_message *netmsg = sis_net_message_create();
// 			if (status == 0)
// 			{
// 				if (sis_memory_get_size(cxt->recv_memory) == 0)
// 				{
// 					sis_memory_swap(SIS_OBJ_MEMORY(next->obj), cxt->recv_memory);
// 				}
// 				else
// 				{
// 					sis_memory_cat(cxt->recv_memory, SIS_OBJ_GET_CHAR(next->obj), SIS_OBJ_GET_SIZE(next->obj));
// 				}
// 			}
// 			// sis_out_binary("recv_memory", sis_memory(cxt->recv_memory), sis_memory_get_size(cxt->recv_memory));
// 			status = sis_net_recv_message(cxt, cxt->recv_memory, netmsg);
// 			// printf("recv netmsg: %d %d\n", count, status);
// 			if (status == 1)
// 			{
// 				netmsg->cid = cxt->rid;
// 				// SIS_NET_SHOW_MSG("recv netmsg:", netmsg);
// 				if (cxt->cb_reply)
// 				{
// 					cxt->cb_reply(cxt->cb_source, netmsg);
// 				}
// 				sis_net_message_destroy(netmsg);
// 				continue;
// 			}			
// 			else if (status == 0)
// 			{
// 				// 如果返回非零 就说明数据不够 等下一个数据过来再处理
// 				next = next->next;
// 				sis_net_message_destroy(netmsg);
// 				continue;
// 			}
// 			else
// 			{
// 				LOG(5)("read data error.[%d]\n", cxt->rid);
// 				// 如果返回<零 就说明数据出错
// 				// 这里要检查循环中删除会不会出问题
// 				// sis_net_class_close_cxt(cls, cxt->rid);
// 				cxt->status = SIS_NET_DISCONNECT;
// 				break;				
// 			}
// 			mem = sis_net_mems_pop(context->work_nodes);
// 		}
// 		sis_net_mems_free_read(context->work_nodes);
// 	}
// }

/**
 * @brief 将收到的数据从s_sis_net_mem中取出，合成s_sis_net_message
 * @param cls 
 * @param cxt 
 */
void _make_read_data(s_sis_net_class *cls, s_sis_net_context *cxt)
{
	int count = sis_net_mems_read(cxt->recv_nodes, 0);
	if (count > 0)
	{
		// printf("recv : %d %zu\n", count, cxt->recv_nodes->rsize);
		s_sis_net_message *netmsg = sis_net_message_create();
		int status = 0;
		s_sis_net_mem *mem = sis_net_mems_pop(cxt->recv_nodes);
		while(mem && cxt->status == SIS_NET_WORKING)
		{
			// sis_out_binary("recv_memory", sis_memory(cxt->recv_memory), sis_memory_get_size(cxt->recv_memory));
			if (status == 0)
			{
				sis_memory_cat(cxt->recv_memory, mem->data, mem->size);
			}
			// sis_out_binary("recv_memory", sis_memory(cxt->recv_memory), sis_memory_get_size(cxt->recv_memory));
			status = sis_net_recv_message(cxt, cxt->recv_memory, netmsg);
			// printf("recv netmsg: %d %d\n", count, status);
			if (status == 1)
			{
				netmsg->cid = cxt->rid;
				// SIS_NET_SHOW_MSG("recv netmsg:", netmsg);
				// if (netmsg->switchs.has_argvs)
				if (cxt->cb_reply)
				{
					cxt->cb_reply(cxt->cb_source, netmsg);
				}
				sis_net_message_destroy(netmsg);
				netmsg = sis_net_message_create();
				continue;
			}			
			else if (status == 0)
			{
				// 如果返回非零 就说明数据不够 等下一个数据过来再处理
				mem = sis_net_mems_pop(cxt->recv_nodes);
				continue;
			}
			else
			{
				LOG(5)("read data error.[%d]\n", cxt->rid);
				// 如果返回<零 就说明数据出错
				// 这里要检查循环中删除会不会出问题
				// sis_net_class_close_cxt(cls, cxt->rid);
				cxt->status = SIS_NET_DISCONNECT;
				break;				
			}
			// mem = sis_net_mems_pop(cxt->recv_nodes);
		}
		sis_net_message_destroy(netmsg);
		sis_net_mems_free_read(cxt->recv_nodes);
	}
	// 这里清理一下接收缓存
	sis_memory_pack(cxt->recv_memory);  
	// printf("recv_memory size = %zu %zu | %zu\n", cxt->recv_memory->maxsize, cxt->recv_memory->size, sis_net_mems_size(cxt->recv_nodes));
}
/**
 * @brief 网络连接处理类s_sis_net_class的后台数据处理函数
 * @param argv 网络连接处理类 s_sis_net_class
 * @return void* 
 */
void *_thread_net_class_read(void* argv)
{
	s_sis_net_class *cls = (s_sis_net_class *)argv;
    sis_wait_thread_start(cls->read_thread);
	// cls->cur_read_cxt = NULL;
    while (sis_wait_thread_noexit(cls->read_thread))
    {
		{
			s_sis_dict_entry *de;
			s_sis_dict_iter *di = sis_dict_get_iter(cls->cxts);
			while ((de = sis_dict_next(di)) != NULL)
			{
				s_sis_net_context *cxt = (s_sis_net_context *)sis_dict_getval(de);
				// if (!cls->cur_read_cxt)
				// {
				// 	cls->cur_read_cxt = cxt;
				// }
				if (cxt->status == SIS_NET_WORKING)
				{
					_make_read_data(cls, cxt);
					// 处理过程中不允许直接退出 需要等待跳出循环后
				}
			}
			sis_dict_iter_free(di);
		}
		// 下面检查有没有断开的连接 有就清理
		{
			s_sis_struct_list *rids = sis_struct_list_create(sizeof(int));
			s_sis_dict_entry *de;
			s_sis_dict_iter *di = sis_dict_get_iter(cls->cxts);
			while ((de = sis_dict_next(di)) != NULL)
			{
				s_sis_net_context *cxt = (s_sis_net_context *)sis_dict_getval(de);
				if (cxt->status == SIS_NET_DISCONNECT)
				{
					sis_struct_list_push(rids, &cxt->rid);
				}
			}
			sis_dict_iter_free(di);
			// 由于字典遍历中删除 会有很小概率出现问题，所有通常仅仅记录信息 后面再删
			for (int i = 0; i < rids->count; i++)
			{
				int *rid = (int *)sis_struct_list_get(rids, i);
				sis_map_kint_del(cls->cxts, *rid);
			}
			sis_struct_list_destroy(rids);
		}

		sis_wait_thread_wait(cls->read_thread, cls->read_thread->wait_msec);
    }
    sis_wait_thread_stop(cls->read_thread);
	return NULL;
}
/**
 * @brief 启动网络连接，如果是服务器模式，则先设置好回调函数地址，然后开始监听端口
 * @param cls_ 网络配置对象
 * @return true 成功，false 失败
 */
bool sis_net_class_open(s_sis_net_class *cls_)
{
	if (!cls_->read_thread)
	{
		cls_->read_thread = sis_wait_thread_create(10);
		if (!sis_wait_thread_open(cls_->read_thread, _thread_net_class_read, cls_))
		{
			sis_wait_thread_destroy(cls_->read_thread);
			cls_->read_thread = NULL;
			LOG(1)("can't start read thread.\n");
			return false;
		}
	}
	// 服务器模式
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
	// 客户端模式
	else
	{
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
	if (cls_->read_thread)
	{
		sis_wait_thread_destroy(cls_->read_thread);
        cls_->read_thread = NULL;
	}
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
/**
 * @brief 根据SESSION_ID从网络连接对象的字典中获取该连接的上下文s_sis_net_context，并注册相应的数据应答处理回调函数
 * @param cls_ 本项目的网络连接对象s_sis_net_class
 * @param sid_ 当前连接的SESSION_ID
 * @param source_ 当前连接对应的worker
 * @param cb_ 当前连接的数据应答处理回调函数
 * @return int 永远为0 
 */
int sis_net_class_set_cb(s_sis_net_class *cls_, int sid_, void *source_, cb_net_reply cb_)
{
	s_sis_net_context *cxt = sis_map_kint_get(cls_->cxts, sid_);
	if (cxt)
	{
		cxt->cb_source = source_;
		cxt->cb_reply = cb_;
	}
	return 0;
}
int sis_net_class_set_slot(s_sis_net_class *cls_, int sid_, char *compress_, char * crypt_, char * protocol_)
{
	s_sis_net_context *cxt = sis_map_kint_get(cls_->cxts, sid_);
	if (!cxt)
	{
		return -1;
	}
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
	compress = compress_ && !sis_strcasecmp(compress_, "snappy") ? SIS_NET_ZIP_SNAPPY : SIS_NET_ZIP_NONE;
	crypt = crypt_ && !sis_strcasecmp(crypt_, "ssl") ? SIS_NET_CRYPT_SSL : SIS_NET_CRYPT_NONE;

	sis_net_slot_set(cxt->slots, compress, crypt, protocol);

	return 0;
}

int sis_net_class_send(s_sis_net_class *cls_, s_sis_net_message *mess_)
{
	if(cls_->work_status != SIS_NET_WORKING)
	{
		// LOG(5)("net no working. %d\n", cls_->work_status);
		return -1;
	}
	// SIS_NET_SHOW_MSG("send:", mess_);
	// sis_net_message_incr(mess_);
	s_sis_net_context *cxt = sis_map_kint_get(cls_->cxts, mess_->cid);
	if (cxt)
	{
		// printf("reader read +++ [%d] %d | %p \n", mess_->cid, cxt->send_cxts->count ,cxt->send_cxts);
		s_sis_memory *sendmem = sis_net_make_message(cxt, mess_); 
		// sis_out_binary("send", SIS_OBJ_GET_CHAR(sendobj), SIS_OBJ_GET_SIZE(sendobj));
		if (sendmem)
		{
			if (cls_->url->io == SIS_NET_IO_WAITCNT)
			{
				sis_socket_server_send(cls_->server, mess_->cid, sendmem);
			}
			else
			{
				sis_socket_client_send(cls_->client, sendmem);
			}
			sis_memory_destroy(sendmem);
		}	
	}
	// sis_net_message_decr(mess_);
	return 0;
}

////////////////////////////////////////////////
//    s_sis_net_class other define     
////////////////////////////////////////////////

int sis_net_recv_message(s_sis_net_context *cxt_, s_sis_memory *in_, s_sis_net_message *mess_)
{
	s_sis_net_slot *call = cxt_->slots;
	s_sis_memory *inmem = sis_memory_create_size(16 * 1024);
	s_sis_memory *inmemptr = inmem;
	// 成功或者不成功 in_ 都会移动位置到新的位置
	s_sis_net_tail info;
	int o = call->slot_net_unpack(in_, &info, inmemptr);
	if (o == 0)
	{
		// 数据不完整 拼接数据
		sis_memory_destroy(inmemptr);
		return 0;
	} 
	else if (o == -1)
	{
		sis_memory_destroy(inmemptr);
		return -1;
	}
	if (info.bytes)
	{
		s_sis_memory *outmem = sis_memory_create_size(16 * 1024);
		s_sis_memory *outmemptr = outmem;
		if (info.crypt && call->slot_net_decrypt)
		{
			if (call->slot_net_decrypt(inmemptr, outmemptr))
			{
				SIS_MEMORY_SWAP(inmemptr, outmemptr);
			}
		}
		if (info.compress && call->slot_net_uncompress)
		{
			if (call->slot_net_uncompress(inmemptr, outmemptr))
			{
				SIS_MEMORY_SWAP(inmemptr, outmemptr);
			}
		}
		sis_memory_destroy(outmemptr);
	}
	// sis_out_binary("recv", sis_memory(inmemptr), sis_memory_get_size(inmemptr));
	mess_->format = info.bytes;  // 这里设置数据区格式

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

s_sis_object *sis_net_send_message(s_sis_net_context *cxt_, s_sis_net_message *mess_)
{
	s_sis_net_slot *call = cxt_->slots;
	s_sis_memory *inmem = sis_memory_create_size(16 * 1024);
	s_sis_memory *inmemptr = inmem;
	if (call->slot_net_encoded)
	{
		if (!call->slot_net_encoded(mess_, inmemptr))
		{
			sis_memory_destroy(inmem);
			return NULL;
		}
	}
	s_sis_memory *outmem = sis_memory_create_size(16 * 1024);
	s_sis_memory *outmemptr = outmem;

	s_sis_net_tail info;
	info.bytes = mess_->format == SIS_NET_FORMAT_BYTES ? 1 : 0;
	info.compress = 0;
	info.crypt = 0;
	info.crc16 = 0;
	if (info.bytes)
	{
		if (call->slot_net_compress)
		{
			if (call->slot_net_compress(inmemptr, outmemptr))
			{
				info.compress = 1;
				SIS_MEMORY_SWAP(inmemptr, outmemptr);
			}
		}
		if (call->slot_net_encrypt)
		{
			if (call->slot_net_encrypt(inmemptr, outmemptr))
			{
				info.crypt = 1;
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

s_sis_memory *sis_net_make_message(s_sis_net_context *cxt_, s_sis_net_message *mess_)
{
	s_sis_net_slot *call = cxt_->slots;
	s_sis_memory *inmem = sis_memory_create_size(16 * 1024);
	s_sis_memory *inmemptr = inmem;
	if (call->slot_net_encoded)
	{
		if (!call->slot_net_encoded(mess_, inmemptr))
		{
			sis_memory_destroy(inmem);
			return NULL;
		}
	}
	s_sis_memory *outmem = sis_memory_create_size(16 * 1024);
	s_sis_memory *outmemptr = outmem;

	s_sis_net_tail info;
	info.bytes = mess_->format == SIS_NET_FORMAT_BYTES ? 1 : 0;
	info.compress = 0;
	info.crypt = 0;
	info.crc16 = 0;
	if (info.bytes)
	{
		if (call->slot_net_compress)
		{
			if (call->slot_net_compress(inmemptr, outmemptr))
			{
				info.compress = 1;
				SIS_MEMORY_SWAP(inmemptr, outmemptr);
			}
		}
		if (call->slot_net_encrypt)
		{
			if (call->slot_net_encrypt(inmemptr, outmemptr))
			{
				info.crypt = 1;
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
	return inmemptr;
}


#if 0
// 测试网络通讯协议是否正常
#include "sis_net.msg.h"

#define TEST_PORT 7329
#define TEST_SIP "0.0.0.0"
#define TEST_IP "127.0.0.1"
int __sno = 0; 
s_sis_net_class *session = NULL;

int __exit = 0;

void exithandle(int sig)
{
	if (__exit == 1)
	{
		exit(0);
	}
	printf("--- exit .1. \n");	
	__exit = 1;
	if (session)
	{
		sis_net_class_destroy(session);
	}
	printf("--- exit .ok. \n");
	__exit = 2;
}
void cb_recv(void *sock_, s_sis_net_message *msg)
{
	s_sis_net_class *socket = (s_sis_net_class *)sock_;
	SIS_NET_SHOW_MSG("recv:", msg);
	if (!msg->switchs.sw_tag)
	{
		s_sis_sds reply = sis_sdsempty();
		reply = sis_sdscatfmt(reply, "%S %S %S ok.", msg->service, msg->cmd, msg->subject);
		
		sis_net_message_set_byte(msg, reply, sis_sdslen(reply));
		sis_net_class_send(socket, msg);
		// 测试连续发送 会出错
		sis_net_message_set_char(msg, reply, sis_sdslen(reply));
		sis_net_class_send(socket, msg);
		sis_sdsfree(reply);
	}
}
static void _cb_connected(void *handle_, int sid)
{
	LOG(5)("--- connect ok. [%d]\n", sid);
	s_sis_net_class *socket = (s_sis_net_class *)handle_;
	
	sis_net_class_set_cb(socket, sid, socket, cb_recv);

	if (socket->url->role== SIS_NET_ROLE_REQUEST)
	{		
		s_sis_net_message *msg = sis_net_message_create();
	    msg->cid = sid;
		sis_net_message_set_service(msg, "sisdb");
		sis_net_message_set_cmd(msg, "set");
		sis_net_message_set_subject(msg, "myname", "xp");
		sis_net_message_set_char(msg, "ding", 4);
		// int rtn = 
		sis_net_class_send(socket, msg);

		// s_sis_net_message *msg1 = sis_net_message_create();
		// sis_net_message_set_byte(msg1, "set", "myname1", "ding", 4);
		// sis_net_class_send(socket, msg1);
		// sis_net_message_destroy(msg1);

		// s_sis_net_message *msg2 = sis_net_message_create();
		// sis_net_message_set_byte(msg2, "set", "myname2", "ding", 4);
		// sis_net_class_send(socket, msg2);
		// sis_net_message_destroy(msg2);

		sis_net_message_destroy(msg);
		LOG(5)("--- client [%d] send ok. rtn = [%d]\n", sid, 0);	
	}
}
static void _cb_disconnect(void *handle_, int sid)
{
	s_sis_net_class *socket = (s_sis_net_class *)handle_;
	if (socket->url->role== SIS_NET_ROLE_REQUEST)
	{
		LOG(5)("--- client disconnect. %d\n", sid);	
	}
	else
	{
		LOG(5)("--- server disconnect. %d \n", sid);	
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

	s_sis_url url_srv = { SIS_NET_IO_WAITCNT, SIS_NET_ROLE_ANSWER, 1, SIS_NET_PROTOCOL_WS, 0, 0, TEST_SIP, "", TEST_PORT, NULL};
	s_sis_url url_cli = { SIS_NET_IO_CONNECT, SIS_NET_ROLE_REQUEST, 1, SIS_NET_PROTOCOL_WS, 0, 0, TEST_IP, "", TEST_PORT, NULL};

	if (argv[1][0] == 's')
	{	
		session = sis_net_class_create(&url_srv);
	}
	else
	{
		session = sis_net_class_create(&url_cli);
	}
	printf("--- %s:%d\n",session->url->ip4, session->url->port);

	session->cb_connected = _cb_connected;
	session->cb_disconnect = _cb_disconnect;

	sis_net_class_open(session);

	// sis_sleep(5000);
	// if (session->url->role == SIS_NET_ROLE_REQUEST)
	// {
	// 	s_sis_net_message *msg = sis_net_message_create();
	// 	msg->cid = 0;
	// 	if(argv[1][0] == 's') 
	// 	{
	// 		msg->cid = 1;
	// 	}
	// 	s_sis_sds reply = sis_sdsnew("this ok.");
	// 	sis_net_message_set_byte(msg, reply, sis_sdslen(reply));
	// 	// sis_net_message_set_byte(msg, "pub", "info", "ding", 4);
	// 	sis_sdsfree(reply);
	// 	for (int i = 0; i < 100000; i++)
	// 	{
	// 		printf("------- %d ------\n", i);
	// 		sis_net_class_send(session, msg);
	// 	}
	// 	// sis_sleep(5000);
	// 	sis_net_message_destroy(msg);
	// }
	
	// sis_sleep(10000);
	// if (sis_map_pointer_getsize(session->cxts) == 2)
	// {
	// 	s_sis_net_message *msg = sis_net_message_create();
	// 	sis_net_message_set_byte(msg, "pub", "myname", "ding", 4);

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

