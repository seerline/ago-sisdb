
#include <sis_net.cmd.h>
#include <sis_list.h>

static struct s_sis_method _sis_net_method_system[] = {
	{"sub",    SIS_NET_METHOD_SYSTEM, sis_net_cmd_sub, ""},
	{"unsub",  SIS_NET_METHOD_SYSTEM, sis_net_cmd_unsub, ""}, 
	{"pub",    SIS_NET_METHOD_SYSTEM, sis_net_cmd_pub, ""},  
};

s_sis_map_pointer *sis_net_command_create()
{
	int nums = sizeof(_sis_net_method_system) / sizeof(struct s_sis_method);
	s_sis_map_pointer *map = sis_map_pointer_create_v(sis_free_call);
	for (int i = 0; i < nums; i++)
	{
		s_sis_method *c = SIS_MALLOC(s_sis_method, c);
		memmove(c, &_sis_net_method_system[i], sizeof(s_sis_method));
		char key[128];
		sis_sprintf(key, 128, "%s.%s", c->style, c->name);
		sis_map_pointer_set(map, key, c);
	}
	return map;
}
void sis_net_command_destroy(s_sis_map_pointer *map_)
{
	sis_method_map_destroy(map_);
}

void sis_net_command_append(s_sis_net_class *sock_, s_sis_method *method_)
{
	if(!method_)
	{
		return ;
	}
	char key[128];
	sis_sprintf(key, 128, "%s.%s", method_->style, method_->name);
	sis_map_pointer_set(sock_->map_command, key, method_);
}
void sis_net_command_remove(s_sis_net_class *sock_, const char *name_, const char *style_)
{
	char key[128];
	sis_sprintf(key, 128, "%s.%s", style_, name_);
	sis_map_pointer_del(sock_->map_command, key);
}

////////////////////////////////////////////////////////////////////////////////
// command list
////////////////////////////////////////////////////////////////////////////////
void * sis_net_cmd_unsub(void *sock_, void *mess_)
{
	s_sis_net_class *sock = (s_sis_net_class *)sock_;
	s_sis_net_message *mess = (s_sis_net_message *)mess_;

	s_sis_struct_list *cids = (s_sis_struct_list *)sis_map_pointer_get(sock->sub_clients, mess->key);
	if (!cids)
	{
		return SIS_METHOD_VOID_FALSE;
	}
	for (int i = 0; i < cids->count; i++)
	{
		int cid = *(int *)sis_struct_list_get(cids, i);
		if (cid == mess->cid)
		{
			sis_struct_list_delete(cids, i, 1);
			return SIS_METHOD_VOID_TRUE;
		}
	}
	return SIS_METHOD_VOID_FALSE;
}

void * sis_net_cmd_sub(void *sock_, void *mess_)
{
	s_sis_net_class *sock = (s_sis_net_class *)sock_;
	s_sis_net_message *mess = (s_sis_net_message *)mess_;

	s_sis_struct_list *cids = (s_sis_struct_list *)sis_map_pointer_get(sock->sub_clients, mess->key);
	if (!cids)
	{
		cids = sis_struct_list_create(sizeof(int), NULL, 0);
		sis_map_pointer_set(sock->sub_clients, mess->key, cids);
	}
	for (int i = 0; i < cids->count; i++)
	{
		int cid = *(int *)sis_struct_list_get(cids, i);
		if (cid == mess->cid)
		{
			return (void *)0;
		}
	}
	sis_struct_list_push(cids, &mess->cid);
	return (void *)1;
}
void * sis_net_cmd_pub(void *sock_, void *mess_)
{
	s_sis_net_class *sock = (s_sis_net_class *)sock_;
	s_sis_net_message *mess = (s_sis_net_message *)mess_;

	s_sis_sds out = sock->cb_pack_ask(sock, mess_);
	if (!out)
	{
		return (void *)-3;
	}
	int64 oks = 0;
	s_sis_struct_list *cids = (s_sis_struct_list *)sis_map_pointer_get(sock->sub_clients, mess->key);
	if (cids)
	{
		for (int i = 0; i < cids->count; i++)
		{
			int cid = *(int *)sis_struct_list_get(cids, i);
			if (sis_socket_server_send(sock->server, cid, out, sis_sdslen(out)))
			// if (_sis_net_class_sendto(sock_, cid, out, sis_sdslen(out)))
			{
				oks++;
			}
		}
	}
	printf("publish key =%lld\n",oks);
	sis_sdsfree(out);
	return (void *)oks;	
}
