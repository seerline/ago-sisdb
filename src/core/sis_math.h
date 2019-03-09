#ifndef _SIS_MATH_H
#define _SIS_MATH_H

#include <sis_core.h>
#include <math.h>
#include <sis_malloc.h>

#define  LOWORD(l)         ( (unsigned short)(l) )
#define  HIWORD(l)         ( (unsigned short)(((unsigned int)(l) >> 16) & 0xFFFF) )
#define  MERGEINT(a,b)     ( (unsigned int)( ((unsigned short)(a) << 16) | (unsigned short)(b)) )

#define  sis_max(a,b)    (((a) > (b)) ? (a) : (b))
#define  sis_min(a,b)    (((a) < (b)) ? (a) : (b))

//限制返回值a在某个区域内
#define  sis_between(a,min,max)    (((a) < (min)) ? (min) : (((a) > (max)) ? (max) : (a)))

inline int64 sis_zoom10(int n)  // 3 ==> 1000
{
	int64 o = 1;
	int len = (n > 15) ? 15 : n;
	for (int i = 0; i < len; i++) { o = o * 10; }
	return o;
};

inline int sis_sqrt10(int n)  // 1000 ==> 3  -1000 ==> -3
{
	int rtn = 0;
	while (1)
	{
		n = (int)(n / 10);
		if (n >= 1) { rtn++; }
		else { break; }
	}
	return rtn;
};

inline bool sis_isbetween(int value_, int min_, int max_)
{
	int min = min_;
	int max = max_;
	if (min_ > max_) {
		min = max_;
		max = min_;
	}
	if (value_ >= min&&value_ <= max){
		return true;
	}
	return false;
}

#endif //_SIS_MATH_H