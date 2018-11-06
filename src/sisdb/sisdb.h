
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

#define SIS_MAXLEN_TABLE 32
#define SIS_MAXLEN_CODE  9

// 以代码为基本索引，每个代码最基础的信息必须包括
// 价格小数点 默认为 2  统一从info表中decimal获取
// 价格单位   默认为 1  比特币，需要设置 千，万等
//			 统一从info表中coinunit获取
// 成交量单位 默认为 100 = 1手  统一从info表中volunit获取
//  		指数单位可能为 10000 = 百手
// 金额单位   默认为 100 = 百元 统一从info表中volunit获取
//  		指数单位可能为 10000 = 万元
// 交易时间 从对应市场的trade-time字段获取
// 工作时间 从对应市场的work-time字段获取

#define SIS_TABLE_EXCH  "exch"   // 默认的市场数据表名称
#define SIS_TABLE_EXCH  "info"   // 默认的股票数据表名称

// 以上两个表为必要表，如果不存在就报错

#pragma pack(push,1)

// 每个股票都必须跟随的结构化信息，
typedef struct s_sisdb_sys_exch {
	s_sis_time_pair     work_time;
	s_sis_struct_list  *trade_time; // [[930,1130],[1300,1500]] 交易时间，给分钟线用
}s_sisdb_sys_exch;

// 每个股票都必须跟随的结构化信息，
typedef struct s_sisdb_sys_info {
	uint8   decimal;
	uint32  coinunit;
	uint32  volunit;	
	// s_sisdb_sys_exch *exch;  // 从此获取worktime和tradetime;
}s_sisdb_sys_info;
// 新增股票时如果没有该股票代码，就需要自动在exch和info中生成对应的信息，直到被再次刷新，
// 市场编号默认取代码前两位，

// sisdb采用key直接定位到数据 key为 SH.600600.DAY 的方式，以便达到最高的效率，
typedef struct s_sisdb_stock {
	s_sis_db            *father;    // 父节点 可通过这里获取 table 结构信息
	s_sisdb_sys_exch    *exch; 
	s_sisdb_sys_info     info;      // 必须携带的信息,每个代码一份独立的数据
	s_sis_collect_unit  *collect;   // 数据区，s_sis_table
	// s_sis_table collect->table;  // 该数据对应的table指针是哪一个，为快速检索，可在table为空时，向dbs追加一个代码
}s_sisdb_stock;

typedef struct s_sis_db {
	s_sis_sds name;            // 数据库名字 sisdb

	s_sis_sds conf;            // 当前数据表的配置信息 保存时用

	s_sis_map_pointer  *dbs;     // 数据表的字典表 s_sis_table
	s_sis_map_pointer  *map;     // 关键字查询表

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


#endif  /* _SISDB_H */
