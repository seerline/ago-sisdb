
#ifndef _SIS_NET_CMD_H
#define _SIS_NET_CMD_H

#include <sis_net.h>
#include <sis_malloc.h>
#include <sis_str.h>
#include <sis_net.node.h>

#define SIS_NET_METHOD_SYSTEM   "system"   // 基础的函数
#define SIS_NET_METHOD_PUBLIC   "public"   // 实际启动服务时注册的函数
#define SIS_NET_METHOD_CUSTOM   "custom"   // 通常指so或python的插件函数

s_sis_map_pointer *sis_net_command_create();
void sis_net_command_destroy(s_sis_map_pointer *map_);

void sis_net_command_append(s_sis_net_class *sock_, s_sis_method *method_);
void sis_net_command_remove(s_sis_net_class *sock_, const char *name_, const char *style_);

// 系统默认的发布和订阅命令
void *sis_net_cmd_sub(void *sock_, void *mess);
void *sis_net_cmd_pub(void *sock_, void *mess);
void *sis_net_cmd_unsub(void *sock_, void *mess);


#endif //_SIS_NET_H
