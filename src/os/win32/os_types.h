
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

/////
// #ifndef int8_t
// typedef int8 int8_t;
// #endif
// #ifndef uint8_t
// typedef uint8 uint8_t;
// #endif
// #ifndef int16_t
// typedef int16 int16_t;
// #endif
// #ifndef uint16_t
// typedef uint16 uint16_t;
// #endif
// #ifndef int32_t
// typedef int32 int32_t;
// #endif
// #ifndef uint32_t
// typedef uint32 uint32_t;
// #endif
// #ifndef int64_t
// typedef int64 int64_t;
// #endif
// #ifndef uint64_t
// typedef uint64 uint64_t;
// #endif
#endif //_OS_TYPES_H
