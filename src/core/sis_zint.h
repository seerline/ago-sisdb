#ifndef _SIS_ZINT_H
#define _SIS_ZINT_H

#include "sis_core.h"

#pragma pack(push,1)

typedef struct uint24{
	unsigned short low;
	unsigned char  high;
}uint24;

#pragma pack(pop)

static inline uint32 uint24_to_uint32(struct uint24 in_)
{
	return in_.low + (in_.high << 16);
};
static inline void uint32_to_uint24(uint32 in_, struct uint24 *out_){
	out_->low = (unsigned short)in_; 
	out_->high = (unsigned char)(in_ >> 16);
};

#pragma pack(push,1)

#define ZINT32_MAXBASE 0x1FFFFFFF

//最大值为 0x1FFFFFFF*16*16*16 大约2万亿，超过0x1FFFFFFF后的数据在最低几位并不能完全还原
typedef struct zint32
{
	int base : 30;
	unsigned int power : 2;
}zint32;
#pragma pack(pop)

static inline int64 zint32_to_int64(zint32 u){
	int64 n = u.base;
	for (unsigned int i = 0; i < u.power; i++)
	{
		n *= 16;
	}
	return n;
};
static inline void int64_to_zint32(int64 u, zint32 *z){
	z->power = 0;
	while (u>ZINT32_MAXBASE || u<-ZINT32_MAXBASE)
	{
		z->power++;
		u /= 16;
	}
	z->base = (int)u;	
};
inline uint32 zint32_out_uint32(zint32 in_)
{
	uint32 *u = (uint32*)&in_;
	return *u;
};

inline void uint32_out_zint32(uint32 dw, zint32 *z)
{
	uint32* p = (uint32*)z;
	*p = dw;
};

#endif //_SIS_ZINT_H
