#ifndef _SISDB_SYS_H
#define _SISDB_SYS_H

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

typedef struct s_sisdb_userinfo
{
	s_sis_sds username;
	s_sis_sds password;
	int       access;      // 权限等级
	// uint8    *services;    // 可操作的数据表项 
}s_sisdb_userinfo;

typedef struct s_sisdb_workinfo
{
	int              work_status;  // 0 未运行 1 已经运行
	s_sis_sds        workname;     // 实例名
	s_sis_json_node *config;       // 配置信息
	s_sis_worker    *worker;
} s_sisdb_workinfo;

s_sisdb_userinfo *sis_userinfo_create(const char *username_, const char *password_, int access_);
void sis_userinfo_destroy(void *userinfo_);
s_sisdb_workinfo *sis_workinfo_create_of_json(s_sis_json_node *incfg_);
s_sisdb_workinfo *sis_workinfo_create(const char *name_, const char *config_);
void sis_workinfo_destroy(void *workinfo_);

const char *sis_sys_access_itoa(int access);
int sis_sys_access_atoi(const char * access);
bool sis_sys_access_method(s_sis_method *method, int access);

#endif