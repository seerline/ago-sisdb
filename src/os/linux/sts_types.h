
#ifndef _STS_TYPES_H
#define _STS_TYPES_H

/////////////////////////////////////////////
// 数据类型定义
/////////////////////////////////////////////
#include <stdbool.h>

#ifndef BOOL 
typedef bool BOOL;
#endif
#ifndef TRUE
#define TRUE  true
#endif
#ifndef FALSE
#define FALSE false
#endif
#ifndef pointer 
typedef char * pointer;
#endif
#ifndef int8 
typedef char int8;
#endif
#ifndef uint8 
typedef unsigned char uint8;
#endif
#ifndef int16 
typedef short int16;
#endif
#ifndef uint16 
typedef unsigned short uint16;
#endif
#ifndef int32 
typedef int int32;
#endif
#ifndef uint32 
typedef unsigned int uint32;
#endif
#ifndef int64
typedef long long int64;
#endif
#ifndef uint64
typedef unsigned long long uint64;
#endif

#endif //_STS_TYPES_H
