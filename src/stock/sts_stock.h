#ifndef _STS_STOCK_H
#define _STS_STOCK_H

#include <sts_core.h>


#pragma pack(push,1)
typedef struct s_stock_right
{
	uint32 time; //日期
	uint32 sgs;       //每十股送股数(股)  (*1000)
	uint32 pxs;       //每十股派息数(元)  (*1000)
	uint32 pgs;       //每十股配股数(股)  (*1000)
	uint32 pgj;       //配股价(元)        (*1000)
}s_stock_right;
typedef struct s_stock_day
{
	uint32	time;			//成交时间
	uint32	open;			//开盘
	uint32	high;			//最高
	uint32	low;			//最低
	uint32	clsoe;			//最新
	uint32	vol;			//成交量
	uint32	money;			//成交额
}s_stock_day;
#pragma pack(pop)

#endif
//_STS_STOCK_H