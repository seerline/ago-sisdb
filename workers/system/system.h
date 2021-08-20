#ifndef _SIS_SYSTEM_H
#define _SIS_SYSTEM_H

#include "sis_method.h"
#include "sis_net.msg.h"
#include "worker.h"

// 每条命令的三种权限 具备写权限的才保留
#define SIS_ACCESS_NONE       0
#define SIS_ACCESS_READ       1
#define SIS_ACCESS_WRITE      3
#define SIS_ACCESS_WORKER     7   // 发布和订阅信息权限
#define SIS_ACCESS_ADMIN     15   // 超级用户 全部权限

#define SIS_ACCESS_SREAD       "read"    // read
#define SIS_ACCESS_SWRITE      "write"   // set sets
#define SIS_ACCESS_SWORKER     "worker"  // sub pub create
#define SIS_ACCESS_SADMIN      "admin"   // del pack save 

typedef struct s_sis_userinfo
{
	s_sis_sds username;
	s_sis_sds password;
	int       access;      // 权限等级
	// uint8    *services;    // 可操作的数据表项 
}s_sis_userinfo;

typedef struct s_sis_workinfo
{
	int              status;       // 0 未运行 1 已经运行
	s_sis_sds        workname;     // 实例名
	s_sis_json_node *config;       // 配置信息
	s_sis_worker    *worker;
} s_sis_workinfo;

typedef struct s_system_cxt
{
	int              status; // 工作状态 0 表示没有配置 1 表示有配置
	s_sis_map_list  *users;  // 用户列表  s_sis_userinfo
	s_sis_map_list  *works;  // 工作列表  s_sis_workinfo
}s_system_cxt;

bool  system_init(void *, void *);
void  system_uninit(void *);

// 判断用户登陆是否合法
int system_login(void *worker_, void *argv_);
// 判断网络来源命令 用户是否有权限
int system_check(void *worker_, void *argv_);
// 设置一个用户信息
int system_set_user(void *worker_, void *argv_);
// 得到一个用户的信息
int system_get_user(void *worker_, void *argv_);
// 打开一个服务
int system_open_work(void *worker_, void *argv_);
// 关闭一个服务
int system_stop_work(void *worker_, void *argv_);
// 存储数据
int system_save(void *worker_, void *argv_);
// 加载数据
int system_load(void *worker_, void *argv_);

#endif