
#ifndef _OS_TYPES_H
#define _OS_TYPES_H

/////////////////////////////////////////////
// �������Ͷ���
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
#ifndef int64_t
typedef long int64_t;
#endif
#ifndef uint64_t
typedef unsigned long uint64_t;
#endif
#ifndef int64
typedef long long int64;
#endif
#ifndef uint64
typedef unsigned long long uint64;
#endif

#endif //_OS_TYPES_H