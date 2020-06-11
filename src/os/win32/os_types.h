
#ifndef _OS_TYPES_H
#define _OS_TYPES_H

// windows and linux is same

/////////////////////////////////////////////
// 数据类型定义
/////////////////////////////////////////////
#include <stdbool.h>

#ifndef BOOL 
typedef int BOOL;
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
#ifndef float32
typedef float float32;
#endif
#ifndef float64
typedef double float64;
#endif
#ifndef ssize_t
typedef long long ssize_t;
#endif
#ifndef msec_t
typedef unsigned long long msec_t;
#endif
#ifndef fsec_t
typedef unsigned long long fsec_t;
#endif
#ifndef date_t
typedef unsigned int date_t;
#endif
#ifndef minute_t
typedef unsigned int minute_t;
#endif
#endif //_OS_TYPES_H
