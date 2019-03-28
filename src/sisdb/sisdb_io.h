
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISDB_IO_H
#define _SISDB_IO_H

#include "sis_core.h"
#include "sis_conf.h"
#include "sis_str.h"
#include "sis_file.h"

#include "sisdb.h"
#include "sisdb_table.h"
#include "sisdb_map.h"

#pragma pack(push,1)

#define  SISDB_MAX_ONE_OUTS  100

typedef struct s_sisdb_server
{
	int    status; // 是否已经初始化 0 没有初始化

	char   service_name[SIS_MAXLEN_TABLE];  //服务名
	// 以下信息从配置库中读取
	char   db_path[SIS_PATH_LEN];    //数据库输出主路径，

	bool   log_screen;
	int    log_level;
	size_t log_size;
	char   log_path[SIS_PATH_LEN];   //log路径

	int    safe_deeply;     // 保存的深度
	int    safe_lasts;      // 保留的备份数量
	char   safe_path[SIS_PATH_LEN];   //安全备份的路径

	// s_sis_db *db;    // 数据库
	s_sis_pointer_list *sisdbs;  // 数据集合表，注册时返回该列表，然后循环注册到主框架中

	bool switch_output;  // 设置后所有get会同时存盘
	bool switch_super;   // 获取到超级用户权限，可以执行删除操作

}s_sisdb_server;

#pragma pack(pop)

char * sisdb_open(const char *conf_);

int sisdb_init(const char *market_);

void sisdb_close();

s_sisdb_server *sisdb_get_server();
int sisdb_get_server_status();

bool sisdb_save();

s_sis_sds sisdb_show_sds(const char *key_, const char *com_);

s_sis_sds sisdb_call_sds(const char *key_, const char *com_);

s_sis_sds sisdb_get_sds(const char *key_, const char *com_);

s_sis_sds sisdb_fast_get_sds(const char *key_);

int sisdb_set_switch(const char *key_);

// bool sisdb_out(const char * key_, const char *com_); 
// 取消该功能，放到sisdb_get_sds利用参数 output:1 表示输出到磁盘

int sisdb_delete(const char *key_, const char *com_, size_t len_);

int sisdb_del(const char *key_, const char *com_, size_t len_);

// 来源数据为二进制结构
int sisdb_set_struct(const char *key_, const char *val_, size_t len_);
// 来源数据为JSON数据结构
int sisdb_set_json(const char *key_, const char *val_, size_t len_);

int sisdb_set(int type_, const char *key_, const char *val_, size_t len_);

// table {}
int sisdb_new(const char *table_, const char *attrs_, size_t len_);
// table {}
// table.scale  1
// table.fields []
int sisdb_update(const char *key_, const char *val_, size_t len_);

int   sisdb_write_begin(int type_, const char *key_, const char *val_, size_t len_);
void  sisdb_write_end();

#endif  /* _SISDB_IO_H */
