#ifndef _STS_STOCK_H
#define _STS_STOCK_H

#include <sts_core.h>
#include <sts_list.h>

#define STS_EXRIGHT_FORWORD 1
#define STS_EXRIGHT_BEHAND  2

#pragma pack(push,1)

typedef struct s_stock_dynamic
{
	uint32	time;		    //成交时间
	uint32	open;			//开盘
	uint32	high;			//最高
	uint32	low;			//最低
	uint32	close;			//最新
	uint32	volume;			//成交量
	uint32	money;			//成交额
	uint32	buy1;		
	uint32	buyv1;		
	uint32	sell1;		
	uint32	sellv1;		
	uint32	buy2;
	uint32	buyv2;
	uint32	sell2;
	uint32	sellv2;
	uint32	buy3;
	uint32	buyv3;
	uint32	sell3;
	uint32	sellv3;
	uint32	buy4;
	uint32	buyv4;
	uint32	sell4;
	uint32	sellv4;
	uint32	buy5;
	uint32	buyv5;
	uint32	sell5;
	uint32	sellv5;
}s_stock_dynamic;

typedef struct s_stock_info
{
	char	name[16];		//名称
	char	search[16];		//拼音
	uint8	type;			//STK_TYPE
	uint8	coinunit;	//价格最小分辨率，非常重要，每一个uint32类型的价格都要除以10^price_digit_by才是真正的价格
	uint8	volunit;		//成交量单位 压缩计算时使用 vol*volunit_w = 原始值 目前利用zoom在采数时统一标准，所以默认都为1
	uint32	before;		//昨收
	uint32	stophigh;		//涨停
	uint32	stoplow;		//跌停
}s_stock_info;
typedef struct s_stock_info_db
{
	char	     code[9];		// 代码
	s_stock_info info;      // 
}s_stock_info_db;
typedef struct s_stock_right
{
	uint32 time;      //日期
	int32 sgs;       //每十股送股数(股)  (*1000)
	int32 pxs;       //每十股派息数(元)  (*1000)
	int32 pgs;       //每十股配股数(股)  (*1000)
	int32 pgj;       //配股价(元)       (*1000)
}s_stock_right;

typedef struct s_stock_right_para
{
	int32 volgap;    // 股数
	int32 allot;  // 配股引起的价格变化 
	int32 bonus;  // 分红引起的价格变化
}s_stock_right_para;

typedef struct s_stock_day
{
	uint32	time;			//日期
	uint32	open;			//开盘
	uint32	high;			//最高
	uint32	low;			//最低
	uint32	clsoe;			//最新
	uint32	vol;			//成交量
	uint32	money;			//成交额
}s_stock_day;

#pragma pack(pop)

uint32 sts_stock_exright_vol(uint32 now_, uint32 stop_, uint32 vol_, s_sts_struct_list *rights_);
uint32 sts_stock_exright_price(uint32 now_, uint32 stop_, uint32 price_,int coinunit_, s_sts_struct_list *rights_);

#endif
//_STS_STOCK_H