
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
	int    status;   // 是否已经初始化 0 没有初始化

	s_sis_conf_handle *config;  // 配置文件句柄

	bool   log_screen;   // 是否输出到屏幕
	int    log_level;    // log 级别
	size_t log_size;     // log 最大的尺寸，超过就备份
	char   log_path[SIS_PATH_LEN];    // log路径
	char   logname[SIS_PATH_LEN];   //log文件名，通常以执行文件名为名

	int    safe_deeply;               // 保存的深度
	int    safe_lasts;                // 保留的备份数量
	char   safe_path[SIS_PATH_LEN];   // 安全备份的路径

	// 以下信息从配置库中读取
	s_sis_db           *sysdb;        // 系统数据表的指针，具体数据在sisdbs中 其他数据集合公用
	s_sis_map_pointer  *sys_exchs;    // 市场的信息 一个市场一个记录 默认记录为 “00”  s_sisdb_sys_exch
	s_sis_pointer_list *sys_infos;    // 股票的信息 实际的info数据 第一条为默认配置，不同才增加 s_sisdb_sys_info

	s_sis_pointer_list *sisdbs;   // 数据集合表，注册时返回该列表，然后循环注册到主框架中 s_sis_db
	char   db_path[SIS_PATH_LEN]; // 数据库输出主路径，

	s_sis_pointer_list *workers;  // 其他服务  s_sisdb_worker

	s_sis_map_pointer  *maps;         // 关键字字典查询表
	s_sis_map_pointer  *methods;      // 数据库操作方法字典

	s_sis_mutex_t write_mutex;        // 全局写入时加锁

	bool switch_output;  // 设置后所有get会同时存盘
	bool switch_super;   // 获取到超级用户权限，可以执行删除操作

}s_sisdb_server;

#pragma pack(pop)


s_sis_pointer_list *sisdb_open(const char *conf_);

int sisdb_init(const char *market_);

s_sisdb_server *sisdb_get_server();
int sisdb_get_server_status();

void sisdb_close();

s_sis_db *sisdb_get_db(const char *dbname_);
s_sis_db *sisdb_get_db_from_table(const char *tbname_);

bool sisdb_save(const char *dbname_);

s_sis_sds sisdb_show_sds(const char *key_, const char *com_);

s_sis_sds sisdb_call_sds(const char *key_, const char *com_);

int sisdb_set_switch(const char *key_);

// bool sisdb_out(const char * key_, const char *com_); 
// 取消该功能，放到sisdb_get_sds利用参数 output:1 表示输出到磁盘
s_sis_sds sisdb_get_sds(s_sis_db *db_, const char *key_, const char *com_);
s_sis_sds sisdb_fast_get_sds(s_sis_db *db_, const char *key_);

int sisdb_delete(s_sis_db *db_, const char *key_, const char *com_, size_t len_);
int sisdb_delete_link(s_sis_db *db_, const char *key_, const char *com_, size_t len_);

// 来源数据为二进制结构
int sisdb_set_struct(s_sis_db *db_, const char *key_, const char *val_, size_t len_);
// 来源数据为JSON数据结构
int sisdb_set_json(s_sis_db *db_, const char *key_, const char *val_, size_t len_);

int sisdb_set(s_sis_db *db_, int type_, const char *key_, const char *val_, size_t len_);

// // table {}
// int sisdb_new(s_sis_db *db_, const char *table_, const char *attrs_, size_t len_);
// // table {}
// // table.scale  1
// // table.fields []
// int sisdb_update(const char *key_, const char *val_, size_t len_);

int   sisdb_write_begin(s_sis_db *db_, int type_, const char *key_, const char *val_, size_t len_);
void  sisdb_write_end(s_sis_db *db_);

#endif  /* _SISDB_IO_H */
