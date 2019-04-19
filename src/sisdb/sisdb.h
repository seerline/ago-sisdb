
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISDB_H
#define _SISDB_H

#include "sis_core.h"
#include "sis_map.h"
#include "sis_malloc.h"
#include "sis_time.h"
#include "sis_list.h"
#include "sis_json.h"
#include "sis_thread.h"

// 一些重要的常量定义
#define SIS_MAXLEN_CODE  16  // 代码长度
#define SIS_MAXLEN_TABLE 32  // 数据表名长度
#define SIS_MAXLEN_KEY   (SIS_MAXLEN_CODE + SIS_MAXLEN_TABLE)

#define SIS_MAXLEN_NAME  64  // 中文名长度

#define SIS_MAXLEN_COMMAND 128   // 命令长度
#define SIS_MAXLEN_STRING  128   // 输出时单个字符通用长度

// 默认返回最后不超过16K的数据，二进制数据
#define SISDB_MAXLEN_RETURN (16*1024)
// 需要落盘时，最大缓存区尺寸
// 当某个表设置为 disk 时，表示主要保存在磁盘上，而limit为保留在内存的记录数，
// 会申请两倍大小缓存区，当缓存区满，才把前半部分写盘，然后移动内存和相关指针；
// get时，默认只取内存中数据，除非带参数 source:disk 那么才会去检索磁盘中的数据，速度受磁盘访问速度限制
#define SISDB_MAXLEN_DISK_BUFFER (16*1024*1024)

#pragma pack(push, 1)

typedef struct s_sis_db {
	s_sis_sds name;            // 数据库名字 sisdb

	s_sis_json_node    *conf;         // 当前数据表的配置信息 保存时用

	s_sis_sds  value;    // 缺省的变量，新的记录除非为空，否则以这条记录为模版
	
	bool   loading;  // 数据加载中
	
	s_sis_mutex_t  write_mutex;        // 当前数据集合写入时加锁

	s_sis_map_pointer  *dbs;          // 数据表的字典表 s_sisdb_table 数量为数据表数

	s_sis_map_pointer  *collects;     // 数据集合的字典表 s_sisdb_collect 这里实际存放数据，数量为股票个数x数据表数

	s_sis_pointer_list *subscribes;   // 订阅者列表 订阅者有个回调函数

	int 				save_format;   // 存盘文件的方式

	// bool   special;  // 是否证券专用， 必须有system表的定义，并且不可删除和更改
	// 				 // 一定会定义了开收市时间，并且可以使用PRICE以及VOLUME，AMOUNT字段属性
	// s_sis_map_pointer  *sys_exchs;    // 市场的信息 一个市场一个记录 默认记录为 “00”  s_sisdb_sys_exch
	// s_sis_pointer_list *sys_infos;    // 股票的信息 实际的info数据 第一条为默认配置，不同才增加 s_sisdb_sys_info
	// s_sis_plan_task    *init_task;    // 初始化任务

	// s_sis_plan_task    *save_task;
	sis_file_handle    aof_fp;
}s_sis_db;


typedef struct s_sisdb_worker {
	int    status;         //当前的工作状态

    char   classname[SIS_NAME_LEN];
    
    void   *other;   // 保存个性数据
    void   *father;  // server的指针，用于循环退出使用

    s_sis_plan_task    *task;  // 对应的任务 可能有其他任务

	bool(*open)(struct s_sisdb_worker *, s_sis_json_node *);
    void(*working)(struct s_sisdb_worker *);
	void(*close)(struct s_sisdb_worker *);

} s_sisdb_worker;


typedef struct s_sisdb_save_config {
	int    status;         //当前的工作状态 
} s_sisdb_save_config;

typedef struct s_sisdb_init_config {
	char dbname[128];
	char tbname[128];
} s_sisdb_init_config;

#pragma pack(pop)

s_sis_db *sisdb_create(const char *);  //数据库的名称，为空建立一个sys的数据库名
void sisdb_destroy(s_sis_db *);  //关闭一个数据库

s_sisdb_worker *sisdb_worker_create(s_sis_json_node *);
void sisdb_worker_destroy(s_sisdb_worker *);
// 读取公共的配置函数
void sisdb_worker_init(s_sisdb_worker *, s_sis_json_node *);

///////////////////////////
//  save worker define   //
///////////////////////////

bool worker_save_open(s_sisdb_worker *, s_sis_json_node *);
void worker_save_close(s_sisdb_worker *);
void worker_save_working(s_sisdb_worker *);


#endif  /* _SISDB_H */
