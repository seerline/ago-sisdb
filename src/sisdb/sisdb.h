
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

#define SIS_MAXLEN_CODE  9
#define SIS_MAXLEN_TABLE 32
#define SIS_MAXLEN_KEY   (SIS_MAXLEN_CODE + SIS_MAXLEN_TABLE)

// 以代码为基本索引，每个代码最基础的信息必须包括
// 价格小数点 默认为 2  统一从info表中dot获取
// 价格单位   默认为 1  比特币，需要设置 千，万等
//			 统一从info表中coinunit获取
// 成交量单位 默认为 100 = 1手  统一从info表中volunit获取
//  		指数单位可能为 10000 = 百手
// 金额单位   默认为 100 = 百元 统一从info表中volunit获取
//  		指数单位可能为 10000 = 万元
// 交易时间 从对应市场的trade-time字段获取
// 工作时间 从对应市场的work-time字段获取

#define SIS_TABLE_EXCH  "exch"   // 默认的市场数据表名称
#define SIS_TABLE_INFO  "info"   // 默认的股票数据表名称

// 以上两个表为必要表，如果不存在就报错

#pragma pack(push,1)

// 市场信息 每个股票都必须跟随的结构化信息，
// 当exch表数据修改后，这里需要对应修改
// 股票信息 每个股票都必须跟随的结构化信息，
// 当info表数据修改后，这里需要对应修改
typedef struct s_sisdb_sysinfo {
	uint8   dot;   // 仅对prcice起作用
	uint32  prc_unit;  // 仅对prcice起作用
	uint32  vol_unit;  // 仅对volume起作用	
	s_sis_time_pair     work_time;
	s_sis_struct_list  *trade_time; // [[930,1130],[1300,1500]] 交易时间，给分钟线用
}s_sisdb_sysinfo;
// 新增股票时如果没有该股票代码，就需要自动在exch和info中生成对应的信息，直到被再次刷新，
// 市场编号默认取代码前两位，
// ------------------------------------------------------ //
// 这里为了避免冗余数据过多，采用配置列表来处理info问题，新创建的info必须是不同的，否则返回指针
// ------------------------------------------------------ //

typedef struct s_sis_db {
	s_sis_sds name;            // 数据库名字 sisdb

	s_sis_sds conf;            // 当前数据表的配置信息 保存时用

	s_sis_map_pointer  *dbs;       // 数据表的字典表 s_sisdb_table 数量为数据表数

	s_sis_struct_list  *info;      // 实际的info数据 第一条为默认配置，设置默认配置时修改
	s_sis_map_pointer  *infos;     // 股票信息的字典表 s_sisdb_sysinfo 数量为股票个数

	s_sis_map_pointer  *collects;  // 数据集合的字典表 s_sisdb_collect 这里实际存放数据，数量为股票个数x数据表数

	s_sis_map_pointer  *map;     // 关键字查询表
	
	bool   loading;  // 数据加载中

//  下面的存盘线程应该建立一个结构体来处理，否则看起来很麻烦
//  等开始处理worktime时一起处理，定时任务线程
    int    save_mode;   // 存盘方式，间隔时间存，还是指定时间存
    s_sis_struct_list  *save_plans;   // plans-work 定时任务 uint16 的数组
    s_sis_time_gap      save_always;  // always-work 循环运行的配置

	s_sis_thread_id_t save_pid;
	s_sis_mutex_t save_mutex;  // save时的锁定

	int save_format;   // 存盘文件的方式

	s_sis_wait thread_wait; //线程内部延时处理

}s_sis_db;

#pragma pack(pop)


s_sis_db *sisdb_create(char *);  //数据库的名称，为空建立一个sys的数据库名
void sisdb_destroy(s_sis_db *);  //关闭一个数据库

s_sisdb_sysinfo *sisdb_get_sysinfo(s_sis_db *db_, const char *key_);

s_sisdb_sysinfo *sisdb_sysinfo_create(s_sis_db *db_, const char *);
void sisdb_sysinfo_rebuild(s_sis_db *db_); ????
void sisdb_sysinfo_destroy(void *);

uint16 sisdb_ttime_to_trade_index(uint64 ttime_, s_sis_struct_list *tradetime_);
uint64 sisdb_trade_index_to_ttime(int date_, int idx_, s_sis_struct_list *tradetime_);

int sis_from_node_get_format(s_sis_db *db_, s_sis_json_node *node_);


#endif  /* _SISDB_H */
