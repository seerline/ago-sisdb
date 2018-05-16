
#ifndef _STS_FUNCS_H
#define _STS_FUNCS_H

/////////////////////////////////////////////
// 数据类型定义
/////////////////////////////////////////////
#include <sts_types.h>

#define  LOWORD(l)         ( (uint16)(l) )
#define  HIWORD(l)         ( (uint16)(((uint32)(l) >> 16) & 0xFFFF) )
#define  MERGEINT(a,b)     ( (uint32)( ((uint16)(a) << 16) | (uint16)(b)) )

#define  max(a,b)    (((a) > (b)) ? (a) : (b))
#define  min(a,b)    (((a) < (b)) ? (a) : (b))
#define  limit(a,min,max)    (((a) < (min)) ? (min) : (((a) > (max)) ? (max) : (a))) //限制返回值a在某个区域内

#define  P64I  "%ld"

#endif //_STS_FUNCS_H
