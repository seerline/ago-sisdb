#ifndef _SIS_STOCK_H
#define _SIS_STOCK_H

#include <sis_core.h>
#include <sis_list.h>

#define SIS_EXRIGHT_FORWORD 1
#define SIS_EXRIGHT_BEHAND  2

// #define SIS_DEBUG_CODE  "SZ399001,SZ000001,SH600600,SH000001,SH603976"
// #define SIS_DEBUG_CODE  "SH600600"

// 这里只保存 最通用的结构和函数
#pragma pack(push, 1)

typedef struct s_stock_dynamic
{
	uint32 time;   //成交时间
	uint32 open;   //开盘
	uint32 high;   //最高
	uint32 low;	//最低
	uint32 close;  //最新
	uint32 volume; //成交量
	uint32 money;  //成交额
	uint32 askp[5];
	uint32 askv[5];
	uint32 bidp[5];
	uint32 bidv[5];
} s_stock_dynamic;


typedef struct s_stock_idx_dynamic
{
	uint32 time;   //成交时间
	uint32 open;   //开盘
	uint32 high;   //最高
	uint32 low;	//最低
	uint32 close;  //最新
	uint32 volume; //成交量
	uint32 money;  //成交额
	uint32 askp[5];
	uint32 ups;
	uint32 downs;
	uint32 now_ups;
	uint32 now_downs;
	uint32 now_flats;
	uint32 bidp[5];
	uint32 now_upamt;
	uint32 now_downamt;
	uint32 now_flatamt;
	uint32 upamt;  // 主动买入金额 价格没变化但有成交量的分摊
	uint32 downamt;  // 主动卖出金额 价格没变化但有成交量的分摊
} s_stock_idx_dynamic;

typedef struct s_stock_info
{
	char market[3];  //名称
	char code[7];	//名称
	char name[16];   //名称
	char search[16]; //拼音
	uint8  type;	  //类型
	uint8  dot;		 //小数点
	uint32 prc_unit; //价格最小分辨率，非常重要，每一个uint32类型的价格都要除以10^price_digit_by才是真正的价格
	uint32 vol_unit; //成交量单位 压缩计算时使用 vol*vol_unit_w = 原始值 目前利用zoom在采数时统一标准，所以默认都为1
	uint32 before;   //昨收
					 // uint32 stophigh; //涨停
					 // uint32 stoplow;  //跌停
} s_stock_info;

typedef struct s_stock_market_info
{
	s_stock_info info;   //
	s_stock_dynamic now; //
} s_stock_market_info;

typedef struct s_stock_info_db
{
	char code[9];	  // 代码
	s_stock_info info; //
} s_stock_info_db;

typedef struct s_stock_right
{
	uint32 time;	  // 日期
	int64 prc_factor; // 价格因子  正代表价格跌，负代表价格涨 (*1000) 和原来价格相加
	int64 vol_factor; // 成交量因子 正代表股份增加， 负代表股数缩减  (*1000) 和原来股数相乘
					  //  实际为一个浮点数，放大10000倍保存，代表保留到小数位4位
} s_stock_right;

typedef struct s_stock_finance
{
	uint32 time; //发布时间
	// uint32 date;      //财务年月
	uint32 ssrq; //上市日期

	uint32 zgb;  //总股本(百股)
	uint32 ltag; //流通A股(百股)

	uint32 mgsy;   //每股收益(元) 4f
	uint32 mgjzc;  //每股净资产(元) 4f
	uint32 jzcsyl; //净资产收益率(%) 2f
	uint32 mggjj;  //每股公积金(元) 2f
	uint32 mgwfp;  //每股未分配利润(元) 4f
	uint32 mgxj;   //每股流动资金(元) Ldzc/zgb 4f

	//---------其他总额--------
	uint32 yysr; //营业收入(万元)
	uint32 jlr;  //净利润(万元)
	uint32 tzsy; //投资收益(万元)
	uint32 yszk; //应收账款(万元)
	uint32 hbzj; //货币资金(万元) -- 总现金
	uint32 ldzj; //流动资金(万元) -- 总现金
	uint32 sykc; //剩余库存(万元)};
} s_stock_finance;

typedef struct s_stock_day
{
	uint32 time;  //日期
	uint32 open;  //开盘
	uint32 high;  //最高
	uint32 low;   //最低
	uint32 close; //最新
	uint32 vol;   //成交量
	uint32 money; //成交额
} s_stock_day;

#pragma pack(pop)

bool sis_stock_cn_get_fullcode(const char *code_, char *fc_, size_t len);

uint32 sis_stock_exright_vol(uint32 now_, uint32 stop_, uint32 vol_, s_sis_struct_list *rights_);
uint32 sis_stock_exright_price(uint32 now_, uint32 stop_, uint32 price_, int unit_, s_sis_struct_list *rights_);

#endif
//_SIS_STOCK_H