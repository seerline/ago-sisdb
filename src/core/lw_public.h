
#ifndef _LW_PUBLIC_H
#define _LW_PUBLIC_H

/////////////////////////////////////////////
// 存放linux和windows不同的函数和其他定义
/////////////////////////////////////////////
#ifdef _MSC_VER
#pragma  warning(disable: 4996)
#define  inline __inline
#define  snprintf _snprintf
#define	 random rand
#else
#define  sound(x)
#define  nosound()
#define  LOWORD(l)     ((uint16)(l))
#define  HIWORD(l)     ((uint16)(((uint32)(l) >> 16) & 0xFFFF))
#endif

#define max(a,b)    (((a) > (b)) ? (a) : (b))
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#define limit(a,min,max)    (((a) < (min)) ? (min) : (((a) > (max)) ? (max) : (a))) //限制返回值a在某个区域内
#define MERGEINT(a,b)     ( (uint32)( ((uint16)(a) << 16) | (uint16)(b)) )

/////////////////////////////////////////////
// 打印格式定义
/////////////////////////////////////////////
#ifdef _MSC_VER
#define  P64I  "%I64d"
#else
#define  P64I  "%ld"
#endif

#endif //_LW_PUBLIC_H
