#ifndef _LW_ZINT_H
#define _LW_ZINT_H

#include "sts_core.h"
#include "lw_public.h"

#pragma pack(push,1)
typedef struct uint24{
	unsigned short m_wLow;
	unsigned char m_cHigh;
}uint24;
#pragma pack(pop)

inline uint32 uint24_to_uint32(struct uint24 u)
{
	return u.m_wLow + (u.m_cHigh << 16);
};
static inline void uint32_to_uint24(uint32 u, uint24 *u24){
	u24->m_wLow = (unsigned short)u; 
	u24->m_cHigh = (unsigned char)(u >> 16);
};

#pragma pack(push,1)
//最大值为 0x1FFFFFFF*16*16*16 大约2万亿，超过0x1FFFFFFF后的数据在最低几位并不能完全还原
typedef struct zint32
{
	int m_nBase : 30;
	unsigned int m_nMul : 2;
}zint32;
#pragma pack(pop)

#define ZINT32_MAXBASE 0x1FFFFFFF

static inline int64 zint32_to_int64(zint32 u){
	int64 n = u.m_nBase;
	for (unsigned int i = 0; i < u.m_nMul; i++)
	{
		n *= 16;
	}
	return n;
};
static inline void int64_to_zint32(int64 u, zint32 *z){
	z->m_nMul = 0;
	while (u>ZINT32_MAXBASE || u<-ZINT32_MAXBASE)
	{
		z->m_nMul++;
		u /= 16;
	}
	z->m_nBase = (int)u;	
};
inline uint32 zint32_out_uint32(zint32 u)
{
	return *(uint32*)&u;
};

inline void uint32_out_zint32(uint32 dw, zint32 *z)
{
	uint32* p = (uint32*)z;
	*p = dw;
};

#endif //_LW_ZINT_H
