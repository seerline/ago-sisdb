#ifndef _SIS_AUTH_H
#define _SIS_AUTH_H

#include "sis_method.h"
#include "sis_net.msg.h"
#include "worker.h"

// 每条命令的三种权限 具备写权限的才保留
#define SIS_ACCESS_NONE       0
#define SIS_ACCESS_READ       1
#define SIS_ACCESS_WRITE      3
#define SIS_ACCESS_PUBSUB     7   // 发布和订阅信息权限
#define SIS_ACCESS_ADMIN     15   // 超级用户 全部权限

#define SIS_ACCESS_SREAD       "read"    // read
#define SIS_ACCESS_SWRITE      "write"   // set sets
#define SIS_ACCESS_SPUBSUB     "pubsub"  // sub pub
#define SIS_ACCESS_SADMIN      "admin"   // del pack save 

typedef struct s_sis_userinfo
{
	s_sis_sds username;
	s_sis_sds password;
	int       access;      // 权限等级
	// uint8    *services;    // 可操作的数据表项 
}s_sis_userinfo;

typedef struct s_sis_auth_cxt
{
	int                 status; // 工作状态 0 表示没有配置 1 表示有配置需要校验 2 表示需要等待
	s_sis_map_pointer  *users;  // 用户列表  s_sis_userinfo
}s_sis_auth_cxt;

bool  sis_auth_init(void *, void *);
void  sis_auth_uninit(void *);

// 判断用户登陆是否合法
int sis_auth_login(void *worker_, void *argv_);
// 获取用户权限
int sis_auth_access(void *worker_, void *argv_);
// 设置一个用户信息
int sis_auth_set(void *worker_, void *argv_);
// 得到一个用户的信息
int sis_auth_get(void *worker_, void *argv_);

#endif