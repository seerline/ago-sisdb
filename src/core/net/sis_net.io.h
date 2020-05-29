
#ifndef _SIS_NET_IO_H
#define _SIS_NET_IO_H

#include <sis_core.h>
#include <sis_memory.h>
#include <sis_net.h>

int sis_net_ask_command(s_sis_net_class *net_, int handle_, 
    char *cmd_, char *key_, char *sdb_, char *val_, size_t vlen_);

int sis_net_sub_command(s_sis_net_class *net_, int handle_, 
    char *key_, char *sdb_, char *val_, size_t vlen_);

int sis_net_pub_command(s_sis_net_class *net_, int handle_, 
    char *key_, char *sdb_, char *val_, size_t vlen_);

// 写结果数据
int sis_net_ans_ok(s_sis_net_class *net_, int handle_);
int sis_net_ans_error(s_sis_net_class *net_, int handle_, char *rval_, size_t vlen_);
// 写整数数据
int sis_net_ans_int(s_sis_net_class *net_, int handle_, int64 val_);
// 写正常数据
int sis_net_ans_reply(s_sis_net_class *net_, int handle_, char *rval_, size_t vlen_);

// 写入其他数据 argv 是 s_sis_object 的一个缓存指针 避免重复拷贝
int sis_net_write_argv(s_sis_net_message *net_, int argc_, s_sis_object **argv_);

// #define SIS_NET_METHOD_SYSTEM   "system"   // 基础的函数
// #define SIS_NET_METHOD_PUBLIC   "public"   // 实际启动服务时注册的函数
// #define SIS_NET_METHOD_CUSTOM   "custom"   // 通常指so或python的插件函数

// s_sis_map_pointer *sis_net_command_create();
// void sis_net_command_destroy(s_sis_map_pointer *map_);

// void sis_net_command_append(s_sis_net_class *sock_, s_sis_method *method_);
// void sis_net_command_remove(s_sis_net_class *sock_, const char *name_, const char *style_);

// bool sis_socket_send_reply_info(s_sis_net_class *sock_, int cid_,const char *in_);
// bool sis_socket_send_reply_error(s_sis_net_class *sock_, int cid_,const char *in_);
// bool sis_socket_send_reply_buffer(s_sis_net_class *sock_, int cid_,const char *in_, size_t ilen_);
// bool sis_socket_check_auth(s_sis_net_class *sock_, int cid_);

// // 系统默认的发布和订阅命令
// int cmd_net_auth(void *sock_, void *mess_);

// int cmd_net_sub(void *sock_, void *mess);
// int cmd_net_pub(void *sock_, void *mess);
// int cmd_net_unsub(void *sock_, void *mess);


// 根据不同协议握手步骤有不同 要区别对待
// 只有 io == SIS_NET_IO_WAITCNT 的一方才会要求对方登录 因为端口暴露 必须用户密码正确才会被允许连接
// 用户发送 {cmd:login,username:xxx,password:mmm,compress:none,crypt:none,format:bytes,protocol:ws}  none 可以不写
// 服务端回 {cmd:login,reply: ok,compress:none,crypt:none,format:bytes,protocol:ws}  ok为成功 其他一般原样返回 若服务端不支持就返回服务端默认的配置
// 用户收到该条信息 如果成功就修正自己的slot回调 后面的数据就用新的解析器进行通信了
// bool _check_login_ask(s_sis_net_class *cls, s_sis_net_context *cxt)
// {
// 	bool logined = false;
// 	while(sis_memory_get_size(cxt->recv_buffer) > 0) 
// 	{
// 		s_sis_net_message *mess = sis_net_message_create();
// 		if(_sis_net_class_recv(cxt, cxt->recv_buffer, mess))
// 		{
// 			// 如果返回非零 就说明数据不够 等下一个数据过来再处理
// 			sis_net_message_destroy(mess);
// 			break;
// 		}
// 		// 默认第一个包是login的反馈包
// 		if(!sis_strcasecmp(mess->cmd, "login"))
// 		{
// 			s_sis_memory *reply = sis_memory_create();
// 			char *login = ":{\"cmd\":\"login\",\"reply\":\"ok\"}";
// 			sis_memory_cat(reply, login, strlen(login));
// 			s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, reply); 
// 			sis_socket_server_send(cls->server, cxt->rid, obj);
// 			sis_object_destroy(obj);
// 			logined = true;
// 			break;
// 		}
// 		sis_net_message_destroy(mess);
// 	}
// 	return logined;
// }

// static void cb_server_recv_after(void *handle_, int sid_, char* in_, size_t ilen_)
// 	if (!cxt->inited)
// 	{
// 		cxt->inited = 1;
// 		// 未初始化的客户端
// 		// json 格式数据 直接用
// 		// 获取客户端的info信息 如果信息无误 就设置新的格式
// 		// if (_check_login_ask(cls, cxt))
// 		// {
// 		// 	cxt->inited = 1;
// 		// 	sis_net_slot_set(cxt->slots, cxt->info.compress, cxt->info.crypt, cxt->info.format, cxt->info.protocol);
// 		// 	// cxt->status = SIS_NET_WORKING;
// 		// 	// 这里发送登录信息 
// 		// 	if (cls->cb_connected)
// 		// 	{
// 		// 		cls->cb_connected(cls, sid_);
// 		// 	}			
// 		// }
// 		// else
// 		// {
// 		// 	// cxt->status = SIS_NET_LOGINFAIL;
// 		// 	// 登录失败 关闭
// 		// 	sis_socket_server_delete(cls->server,sid_);
// 		// }		
// 	}


// void _send_client_login(s_sis_net_class *cls, s_sis_net_context *cxt)
// {
// 	if (cls->url->io == SIS_NET_IO_WAITCNT)
// 	{
// 		return ;
// 	}
// 	s_sis_memory *mess = sis_memory_create();
// 	char *login = ":{\"cmd\":\"login\"}";
// 	sis_memory_cat(mess, login, strlen(login));
// 	s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, mess); 
// 	sis_socket_client_send(cls->client, obj);
// 	sis_object_destroy(obj);
// }
// bool _check_login_ans(s_sis_net_class *cls, s_sis_net_context *cxt)
// {
// 	bool logined = false;
// 	while(sis_memory_get_size(cxt->recv_buffer) > 0) 
// 	{
// 		s_sis_net_message *mess = sis_net_message_create();
// 		if(_sis_net_class_recv(cxt, cxt->recv_buffer, mess))
// 		{
// 			// 如果返回非零 就说明数据不够 等下一个数据过来再处理
// 			sis_net_message_destroy(mess);
// 			break;
// 		}
// 		// 默认第一个包是login的反馈包
// 		if(!sis_strcasecmp(mess->cmd, "login"))
// 		{
// 			if (!sis_strcasecmp(mess->rval, "ok"))
// 			{
// 				// 登录成功
// 				// 检查登录包 设置
// 				sis_net_memory_info(&cxt->info, NULL);
// 				logined = true;
// 				break;
// 			}
// 		}
// 		sis_net_message_destroy(mess);
// 	}
// 	return logined;
// }

// static void cb_client_recv_after(void* handle_, int sid_, char* in_, size_t ilen_)
// 	if (!cxt->inited)
// 	{
// 		cxt->inited = 1;
// 		// if (_check_login_ans(cls, cxt))
// 		// {
// 		// 	cxt->inited = 1;
// 		// 	sis_net_slot_set(cxt->slots, cxt->info.compress, cxt->info.crypt, cxt->info.format, cxt->info.protocol);
// 		// 	cls->status = SIS_NET_WORKING;
// 		// 	// 这里发送登录信息 
// 		// 	if (cls->cb_connected)
// 		// 	{
// 		// 		cls->cb_connected(cls, sid_);
// 		// 	}			
// 		// }
// 		// else
// 		// {
// 		// 	cls->status = SIS_NET_LOGINFAIL;
// 		// }
// 	}

#endif //_SIS_CRYPT_H
