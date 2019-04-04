#ifndef _SISDB_SYS_H
#define _SISDB_SYS_H

#include <sis_core.h>
#include <sis_list.h>
#include <sisdb.h>
#include <sisdb_map.h>

#define SIS_EXRIGHT_FORWORD 1
#define SIS_EXRIGHT_BEHAND  2

#define SIS_DEFAULT_EXCH  "00"   // 默认的市场编号存储缺省配置

#define SIS_TABLE_EXCH   "exch"   // 默认的市场数据表名称
#define SIS_TABLE_INFO   "info"   // 默认的股票数据表名称
#define SIS_TABLE_USER   "user"   // 默认的用户数据表名称

// 以上两个表为必要表，如果不存在就报错
#define SIS_TRADETIME_MAX_NUM  4   //

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


// 新增股票时如果没有该股票代码，就需要自动在exch和info中生成对应的信息，直到被再次刷新，
// 市场编号默认取代码前两位，
// ------------------------------------------------------ //
// 这里为了避免冗余数据过多，采用配置列表来处理info问题，新创建的info必须是不同的，否则返回指针
// ------------------------------------------------------ //

#define SIS_MARKET_STATUS_NOINIT    0
#define SIS_MARKET_STATUS_INITED    1  // 正常工作状态
#define SIS_MARKET_STATUS_INITING   2  // 开始初始化
#define SIS_MARKET_STATUS_CLOSE     3  // 已收盘,0&3 此状态下可以初始化

#pragma pack(push, 1)
typedef struct s_sisdb_sys_info {
	uint8   dot;       // 仅对price起作用
	uint32  pzoom;     // 仅对price起作用
	uint32  vunit;     // 仅对volume起作用	
}s_sisdb_sys_info;

typedef struct s_sisdb_sys_exch {
	char    market[3]; 
	uint8  	status;      // 状态
	time_t  init_time;    // 最新收到now的时间
	uint32  init_date;    // 最新收到now的日期
	s_sis_time_pair  work_time; // 初始化时间 单位分钟 900 
	uint8   		 trade_slot; // 交易实际时间段
	s_sis_time_pair  trade_time[SIS_TRADETIME_MAX_NUM]; // [[930,1130],[1300,1500]] 交易时间，给分钟线用
}s_sisdb_sys_exch;

#pragma pack(pop)

s_sisdb_sys_exch *sisdb_sys_create_exch(s_sis_db *db_, const char *code_);
s_sisdb_sys_info *sisdb_sys_create_info(s_sis_db *db_, const char *code_);

// 分走两套体系，一套是正常流程，还有一套是收市后或前，倒数据使用，直接判断具备初始化属性的collect是否是新的一天
// 是就删除重来
void sisdb_sys_flush_work_time(void *);
void sisdb_sys_load_default(s_sis_db *db_, s_sis_json_node *table_);

void sisdb_sys_check_write(s_sis_db *db_, const char *key_, void *src_);

uint16 sisdb_ttime_to_trade_index(uint64 ttime_, s_sisdb_sys_exch *cfg_);
uint64 sisdb_trade_index_to_ttime(int date_, int idx_, s_sisdb_sys_exch *cfg_);

///////////--------------------------///
///////////////////////////
//  init worker define   //
///////////////////////////
bool worker_init_open(s_sisdb_worker *worker_, s_sis_json_node *node_);
void worker_init_close(s_sisdb_worker *worker_);
void worker_init_working(s_sisdb_worker *worker_);

#endif
//_SIS_STOCK_H