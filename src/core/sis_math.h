#ifndef _SIS_MATH_H
#define _SIS_MATH_H

#include <sis_core.h>
#include <math.h>
#include <sis_malloc.h>

#define  SIS_MINV   (0.0000001)

#define  MERGEINT(a,b)     ( (unsigned int)( ((unsigned short)(a) << 16) | (unsigned short)(b)) )

#define  sis_max(a,b)    (((a) > (b)) ? (a) : (b))
#define  sis_min(a,b)    (((a) < (b)) ? (a) : (b))

#define SIS_MAXI(a, b) (((a) > (b)) ? (a) : (b))
#define SIS_MINI(a, b) ((a != 0 ) && (b != 0) ? (((a) < (b)) ? (a) : (b)) : ((a != 0) ? (a) : (b)))

#define SIS_IS_ZERO(a) ((a) == 0 || (((a) > -1 * SIS_MINV) && ((a) < SIS_MINV)))
#define SIS_MINF(a, b) (!SIS_IS_ZERO(a) && !SIS_IS_ZERO(b) ? (((a) < (b)) ? (a) : (b)) : (!SIS_IS_ZERO(a) ? a : b))
#define SIS_MAXF(a, b) (!SIS_IS_ZERO(a) && !SIS_IS_ZERO(b) ? (((a) > (b)) ? (a) : (b)) : (!SIS_IS_ZERO(a) ? a : b))

#define SIS_DIVF(a, b) (SIS_IS_ZERO(b) ? 0.0 : (double)(a) / (double)(b))

#define SIS_FTOI(f, z) ((long long)((f)*(z) + 0.5))
#define SIS_ITOF(f, z) ((double)(f)/(double)(z))

#define SIS_ZOOM_UP(f, z) ({ int _v_ = (double)(f)/(double)(z); _v_ * z < f ? (_v_ + 1) * z : _v_ * z; })
#define SIS_ZOOM_DN(f, z) ({ int _v_ = (double)(f)/(double)(z); _v_ == 0 ? ((f > 0) ? z : 0) : _v_ * z; })

// a <= a
#define SIS_FLOOR(f, z) ((double)((long long)((f)*(z)))/(double)(z))
// a >= a
#define SIS_CEIL(f, z)  ((double)((long long)((f)*(z) + 0.5))/(double)(z))

//限制返回值a在某个区域内
#define  sis_between(a,min,max)    (((a) < (min)) ? (min) : (((a) > (max)) ? (max) : (a)))

static inline int64 sis_abs(int64 n)  
{
	if (n < 0)
	{
		return -1 * n;
	}
	return n;
}
static inline int64 sis_zoom10(int n)  // 3 ==> 1000
{
	int64 o = 1;
	while(n > 0)
	{
		o = o * 10; 
		n--;
	}
	return o;
};

static inline uint32 sis_sqrt10(uint64 v) 
{
  uint32 rtn = 0;
  for (;;) 
  {
    if (v < 10) return rtn;
    if (v < 100) return rtn + 1;
    if (v < 1000) return rtn + 2;
    if (v < 10000) return rtn + 3;
    v /= 10000U;
    rtn += 4;
  }
}

static inline bool sis_isbetween(int value_, int min_, int max_)
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

// 大于基准值的个数
static inline int sis_inside_up(double div_, int count, double in[])
{
	int o = 0;
	for(int i =0 ;i < count; i++)
	{
		if (in[i] > div_)
		{
			o++;
		}
	}
	return o;
}
// 小于基准值的个数
static inline int sis_inside_dn(double div_, int count, double in[])
{
	int o = 0;
	for(int i =0 ;i < count; i++)
	{
		if (in[i] < div_)
		{
			o++;
		}
	}
	return o;
}

// 大于基准值的个数
static inline int sis_inside_upi(int div_, int count, int in[])
{
	int o = 0;
	for(int i =0 ;i < count; i++)
	{
		if (in[i] > div_)
		{
			o++;
		}
	}
	return o;
}
// 小于基准值的个数
static inline int sis_inside_dni(int div_, int min, int count, int in[])
{
	int o = 0;
	for(int i =0 ;i < count; i++)
	{
		if (in[i] < min) continue; 
		if (in[i] < div_)
		{
			o++;
		}
	}
	return o;
}
static inline int sis_bits_get_valid(uint64 in_)
{
	int o = 0;
	uint64 v = 1;
	for (int i = 0; i < 64; i++)
	{
		if ((v << i) & in_)
		{
			o ++;
		}
	}
	return o;
}
static inline int sis_bits_check(uint64 in_, int index_)
{
	if (index_ < 0 || index_ > 63) 
	{
		return 0;
	}
	uint64 v = 1;
	return ((v << index_) & in_) ? 1 : 0;
}
static inline uint64 sis_bits_set(int count_)
{
	if (count_ < 0 || count_ > 63) 
	{
		return 0;
	}
	uint64 o = 0;
	uint64 v = 1;
	for (int i = 0; i < count_; i++)
	{
		o |= (v << i);
	}
	return o;
}
static inline uint64 sis_bits_dec(uint64 in_, int index_)
{
	if (index_ < 0 || index_ > 63) 
	{
		return in_;
	}
	uint64 v1 = (in_ >> (index_ + 1) << (index_ + 1));
	uint64 v2 = (in_ << (64 - index_) >> (64 - index_));
	if (index_ == 0)
    {
        return v1;
    }
	return v1 | v2;
}

// 两组数是否相交 > 最小的 0.382
static inline int sis_is_mixed(double min1, double max1, double min2, double max2)
{
	if (min1 == max2 || min2 == max1)
	{
		return 1;
	}
	if (min1 > max2 || min2 > max1)
	{
		return 0;
	}
	// double min = sis_min(max1 - min1, max2 - min2);
	double gap = sis_min(max1, max2) - sis_max(min1, min2);
	// if (gap > min * 0.382)
	if (gap > 0.001)
	{
		return 1;
	}
	return 0;
}

static inline void sis_init_random()
{
	srand((unsigned)time(NULL));
}
static inline int sis_int_random(int min, int max)
{
	int mid;
	if(min > max)
	{
		mid = max;
		max = min;
		min = mid;
	}
	if (min == max)
	{
		return min;
	}
	mid = max - min + 1;
	mid = rand() % mid + min;
	return mid;
}

static inline double sis_double_random(double min_, double max_)
{
	int min = (int)(min_ * 1000);
	int max = (int)(max_ * 1000);
	int mid;

	if(min > max)
	{
		mid = max;
		max = min;
		min = mid;
	}
	if (min == max)
	{
		return min_;
	}
	mid = max - min + 1;
	mid = rand() % mid + min;
	return (double)mid / 1000.0;
}

static inline int sis_score_random(int maxv_, int minv_)  
{
	int curv = 0;
	if (maxv_ < 1 || minv_ < 1 || maxv_ < minv_)
	{
		return curv;
	}
	for (int i = 0; i < minv_; i++)
	{
		if (sis_int_random(0, maxv_) < minv_)
		{
			curv++;
		}
	}
	return curv;
}
// 获得字符串的hash值 最大不超过nums 便于进行多任务运算
static inline int sis_get_hash(const char *in_, int isize_, int nums_)
{
    if (nums_ < 1)
    {
        return 0;
    }
    int index = 0;
    for (int i = 0; i < isize_; i++)
    {
        index += ((unsigned char)(in_[i]) % nums_);
    }
    index = index % nums_;
    return index;
}
#endif //_SIS_MATH_H

