
#include <sis_net.io.h>
#include <sis_malloc.h>
#include <sis_net.node.h>

int sis_net_ask_command(s_sis_net_class *net_, int handle_, 
    char *cmd_, char *key_, char *sdb_, char *val_, size_t vlen_)
{
    s_sis_net_message *msg = sis_net_message_create();
    msg->style = SIS_NET_ASK_NORMAL;
    msg->cid = handle_;
    msg->cmd = cmd_ ? sdsnew(cmd_) : NULL;
    msg->key = key_ ? sdsnew(key_) : NULL;
    msg->val = val_ ? sdsnewlen(val_, vlen_) : NULL;
    int o = sis_net_class_send(net_, msg);
    sis_net_message_destroy(msg);
    return o;
}

int sis_net_sub_command(s_sis_net_class *net_, int handle_, 
    char *key_, char *sdb_, char *val_, size_t vlen_)
{
    s_sis_net_message *msg = sis_net_message_create();
    msg->style = SIS_NET_ASK_NORMAL;
    msg->cid = handle_;
    msg->cmd = sdsnew("sub");
    msg->key = key_ ? sdsnew(key_) : NULL;
    msg->val = val_ ? sdsnewlen(val_, vlen_) : NULL;
    int o = sis_net_class_send(net_, msg);
    sis_net_message_destroy(msg);
    return o;    
}

int sis_net_pub_command(s_sis_net_class *net_, int handle_, 
    char *key_, char *sdb_, char *val_, size_t vlen_)
{
    s_sis_net_message *msg = sis_net_message_create();
    msg->style = SIS_NET_ASK_NORMAL;
    msg->cid = handle_;
    msg->cmd = sdsnew("pub");
    msg->key = key_ ? sdsnew(key_) : NULL;
    msg->val = val_ ? sdsnewlen(val_, vlen_) : NULL;
    int o = sis_net_class_send(net_, msg);
    sis_net_message_destroy(msg);
    return o;        
}

// 写结果数据
int sis_net_ans_ok(s_sis_net_class *net_, int handle_)
{
    s_sis_net_message *msg = sis_net_message_create();
    msg->style = SIS_NET_REPLY_OK;
    msg->cid = handle_;
    int o = sis_net_class_send(net_, msg);
    sis_net_message_destroy(msg);
    return o;            
}
int sis_net_ans_error(s_sis_net_class *net_, int handle_, char *rval_, size_t vlen_)
{
    s_sis_net_message *msg = sis_net_message_create();
    msg->style = SIS_NET_REPLY_ERROR;
    msg->cid = handle_;
    msg->rval = rval_ ? sdsnewlen(rval_, vlen_) : NULL;
    int o = sis_net_class_send(net_, msg);
    sis_net_message_destroy(msg);
    return o;                
}
// 写整数数据
int sis_net_ans_int(s_sis_net_class *net_, int handle_, int64 val_)
{
    s_sis_net_message *msg = sis_net_message_create();
    msg->style = SIS_NET_REPLY_INT;
    msg->cid = handle_;
    msg->rint = val_;
    int o = sis_net_class_send(net_, msg);
    sis_net_message_destroy(msg);
    return o;      
}
// 写正常数据
int sis_net_ans_reply(s_sis_net_class *net_, int handle_, char *rval_, size_t vlen_)
{
    s_sis_net_message *msg = sis_net_message_create();
    msg->style = SIS_NET_REPLY_NORMAL;
    msg->cid = handle_;
    msg->rval = rval_ ? sdsnewlen(rval_, vlen_) : NULL;
    int o = sis_net_class_send(net_, msg);
    sis_net_message_destroy(msg);
    return o;     
}

// 写入其他数据 argv 是 s_sis_object 底层用法 用户自行发送信息 一般不用 encoded 编码
int sis_net_write_argv(s_sis_net_message *mess_, int argc_, s_sis_object **argv_)
{
    for (int i = 0; i < argc_; i++)
    {
        sis_object_incr(argv_[i]);
        sis_pointer_list_push(mess_->argvs, argv_[i]);
    }
    return mess_->argvs->count;     
}
// int sis_net_send_ask(s_sis_net_class *net_, s_sis_net_message *msg)
// {
//     if (!msg->style)
//     {
//         msg->style = SIS_NET_ASK_NORMAL;
//     }
//     int o = sis_net_class_send(net_, msg);
//     return o;
// }

// static struct s_sis_method _sis_net_method_system[] = {
// 	{"auth",   cmd_net_auth,  SIS_NET_METHOD_SYSTEM, ""},
// 	{"sub",    cmd_net_sub,   SIS_NET_METHOD_SYSTEM, ""},
// 	{"unsub",  cmd_net_unsub, SIS_NET_METHOD_SYSTEM, ""}, 
// 	{"pub",    cmd_net_pub,   SIS_NET_METHOD_SYSTEM, ""},  
// };

// s_sis_map_pointer *sis_net_command_create()
// {
// 	int nums = sizeof(_sis_net_method_system) / sizeof(struct s_sis_method);
// 	s_sis_map_pointer *map = sis_map_pointer_create_v(sis_free_call);
// 	for (int i = 0; i < nums; i++)
// 	{
// 		s_sis_method *c = SIS_MALLOC(s_sis_method, c);
// 		memmove(c, &_sis_net_method_system[i], sizeof(s_sis_method));
// 		char key[128];
// 		sis_sprintf(key, 128, "%s.%s", c->style, c->name);
// 		sis_map_pointer_set(map, key, c);
// 	}
// 	return map;
// }
// void sis_net_command_destroy(s_sis_map_pointer *map_)
// {
// 	sis_method_map_destroy(map_);
// }

// void sis_net_command_append(s_sis_net_class *sock_, s_sis_method *method_)
// {
// 	if(!method_)
// 	{
// 		return ;
// 	}
// 	char key[128];
// 	sis_sprintf(key, 128, "%s.%s", method_->style, method_->name);
// 	sis_map_pointer_set(sock_->map_command, key, method_);
// }
// void sis_net_command_remove(s_sis_net_class *sock_, const char *name_, const char *style_)
// {
// 	char key[128];
// 	sis_sprintf(key, 128, "%s.%s", style_, name_);
// 	sis_map_pointer_del(sock_->map_command, key);
// }

// ////////////////////////////////////////////////////////////////////////////////
// // command list
// ////////////////////////////////////////////////////////////////////////////////
// #define SIS_NET_REPLY_SHORT_LEN 1024
// bool sis_socket_send_reply_info(s_sis_net_class *sock_, int cid_,const char *in_)
// {
// 	char str[SIS_NET_REPLY_SHORT_LEN];
// 	sis_sprintf(str, SIS_NET_REPLY_SHORT_LEN, "+%s\r\n", in_);
// 	if (sock_->connect_method == SIS_NET_IO_CONNECT)
// 	{
// 		// return sis_socket_client_send(sock_->client, str, strlen(str));
// 	}
// 	else
// 	{
// 		// return sis_socket_server_send(sock_->server, cid_, str, strlen(str));
// 	}
	
// }
// bool sis_socket_send_reply_error(s_sis_net_class *sock_, int cid_,const char *in_)
// {
// 	char str[SIS_NET_REPLY_SHORT_LEN];
// 	sis_sprintf(str, SIS_NET_REPLY_SHORT_LEN, "-%s\r\n", in_);
// 	if (sock_->connect_method == SIS_NET_IO_CONNECT)
// 	{
// 		// return  sis_socket_client_send(sock_->client, str, strlen(str));
// 	}
// 	else
// 	{
// 		// return sis_socket_server_send(sock_->server, cid_, str, strlen(str));
// 	}
// }
// bool sis_socket_send_reply_buffer(s_sis_net_class *sock_, int cid_,const char *in_, size_t ilen_)
// {
// 	if (sock_->connect_method == SIS_NET_IO_CONNECT)
// 	{
// 		// return sis_socket_client_send(sock_->client, (char *)in_, ilen_);
// 	}
// 	else
// 	{
// 		// return sis_socket_server_send(sock_->server, cid_, (char *)in_, ilen_);
// 	}
// }
// bool sis_socket_check_auth(s_sis_net_class *sock_, int cid_)
// {
// 	char key[16];
// 	sis_sprintf(key, 16, "%d", cid_);
// 	s_sis_net_client *client = sis_map_pointer_get(sock_->sessions, key);
// 	if (!client->auth)
// 	{
// 		sis_socket_send_reply_error(sock_, cid_, "no auth.");
// 		return false;
// 	}
// 	return true;
// }

// int cmd_net_auth(void *sock_, void *mess_)
// {
// 	s_sis_net_class *sock = (s_sis_net_class *)sock_;
// 	s_sis_net_message *mess = (s_sis_net_message *)mess_;

// 	if (sock->cb_auth)
// 	{
// 		if (!sock->cb_auth(sock_, mess->key, mess->argv))
// 		{
// 			sis_socket_send_reply_error(sock, mess->cid, "auth fail.");
// 			return SIS_METHOD_ERROR;
// 		}
// 	}
// 	char key[16];
// 	sis_sprintf(key, 16, "%d", mess->cid);
// 	s_sis_net_client *client = sis_map_pointer_get(sock->sessions, key);
// 	if (client)
// 	{
// 		client->auth = true;
// 	}
// 	sis_socket_send_reply_info(sock, mess->cid, SIS_REPLY_MSG_OK);
// 	return SIS_METHOD_OK;
// }
// int cmd_net_unsub(void *sock_, void *mess_)
// {
// 	s_sis_net_class *sock = (s_sis_net_class *)sock_;
// 	s_sis_net_message *mess = (s_sis_net_message *)mess_;
// 	if(!sis_socket_check_auth(sock, mess->cid))
// 	{
// 		return SIS_METHOD_ERROR;
// 	}

// 	s_sis_struct_list *cids = (s_sis_struct_list *)sis_map_pointer_get(sock->sub_clients, mess->key);
// 	if (!cids)
// 	{
// 		return SIS_METHOD_ERROR;
// 	}
// 	for (int i = 0; i < cids->count; i++)
// 	{
// 		int cid = *(int *)sis_struct_list_get(cids, i);
// 		if (cid == mess->cid)
// 		{
// 			sis_struct_list_delete(cids, i, 1);
// 			sis_socket_send_reply_info(sock, mess->cid, SIS_REPLY_MSG_OK);
// 			return SIS_METHOD_OK;
// 		}
// 	}
// 	sis_socket_send_reply_error(sock, mess->cid, "no find sub key.");
// 	return SIS_METHOD_ERROR;
// }

// int cmd_net_sub(void *sock_, void *mess_)
// {
// 	s_sis_net_class *sock = (s_sis_net_class *)sock_;
// 	s_sis_net_message *mess = (s_sis_net_message *)mess_;
// 	if(!sis_socket_check_auth(sock, mess->cid))
// 	{
// 		return SIS_METHOD_ERROR;
// 	}

// 	s_sis_struct_list *cids = (s_sis_struct_list *)sis_map_pointer_get(sock->sub_clients, mess->key);
// 	if (!cids)
// 	{
// 		cids = sis_struct_list_create(sizeof(int));
// 		sis_map_pointer_set(sock->sub_clients, mess->key, cids);
// 	}
// 	for (int i = 0; i < cids->count; i++)
// 	{
// 		int cid = *(int *)sis_struct_list_get(cids, i);
// 		if (cid == mess->cid)
// 		{
// 			sis_socket_send_reply_error(sock, mess->cid, "already sub.");
// 			return SIS_METHOD_ERROR;
// 		}
// 	}
// 	sis_struct_list_push(cids, &mess->cid);
// 	sis_socket_send_reply_info(sock, mess->cid, SIS_REPLY_MSG_OK);
// 	return SIS_METHOD_OK;
// }
// int cmd_net_pub(void *sock_, void *mess_)
// {
// 	s_sis_net_class *sock = (s_sis_net_class *)sock_;
// 	s_sis_net_message *mess = sis_net_message_clone((s_sis_net_message *)mess_);
// 	if(!sis_socket_check_auth(sock, mess->cid))
// 	{
// 		return SIS_METHOD_ERROR;
// 	}

// 	mess->style = SIS_NET_REPLY_ARRAY;
// 	mess->subpub = 1;
// 	s_sis_list_node *newnode = NULL;
// 	if (mess->argv)
// 	{
// 		newnode = sis_sdsnode_create(mess->argv, sis_sdslen(mess->argv));
// 	}
// 	s_sis_list_node *node = mess->argvs;
// 	while (node != NULL)
// 	{
// 		newnode = sis_sdsnode_push_node(newnode, node->value, sis_sdslen((s_sis_sds)node->value));
// 		node = node->next;
// 	};
// 	mess->rlist = newnode;

// 	s_sis_sds out = sock->cb_pack(sock, mess);

// 	if (!out)
// 	{
// 		sis_socket_send_reply_error(sock, mess->cid, "unknown format.");
// 		sis_net_message_destroy(mess);
// 		return 0;
// 	}
// 	// 
// 	sis_socket_send_reply_info(sock, mess->cid, SIS_REPLY_MSG_OK);

// 	int64 oks = 0;
// 	s_sis_struct_list *cids = (s_sis_struct_list *)sis_map_pointer_get(sock->sub_clients, mess->key);
// 	if (cids)
// 	{
// 		for (int i = 0; i < cids->count; i++)
// 		{
// 			int cid = *(int *)sis_struct_list_get(cids, i);
// 			if (sis_socket_send_reply_buffer(sock, cid, out, sis_sdslen(out)))
// 			// if (_sis_net_class_sendto(sock_, cid, out, sis_sdslen(out)))
// 			{
// 				oks++;
// 			}
// 		}
// 	}
// 	printf("publish key =%lld\n",oks);
// 	sis_sdsfree(out);
// 	sis_net_message_destroy(mess);
// 	return (int) oks;	
// }

