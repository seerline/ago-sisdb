#ifndef _CALL_H
#define _CALL_H

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define uint32 unsigned int

#pragma pack(push,1)
typedef struct s_stock_right
{
	uint32 m_date_dw; //日期
	uint32 m_sgs_dw;       //每十股送股数(股)  (*1000)
	uint32 m_pxs_dw;       //每十股派息数(元)  (*1000)
	uint32 m_pgs_dw;       //每十股配股数(股)  (*1000)
	uint32 m_pgj_dw;       //配股价(元)        (*1000)
}s_stock_right;
typedef struct s_stock_day
{
	uint32	m_time_dw;			//成交时间
	uint32	m_open_dw;			//开盘
	uint32	m_high_dw;			//最高
	uint32	m_low_dw;			//最低
	uint32	m_new_dw;			//最新
	uint32	m_volume_dw;			//成交量
	uint32	m_amount_dw;			//成交额
}s_stock_day;
#pragma pack(pop)

#endif