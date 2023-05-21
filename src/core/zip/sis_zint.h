#ifndef _SIS_ZINT_H
#define _SIS_ZINT_H

#include <sis_math.h>

#pragma pack(push,1)

// 取值范围 0x0FFF FFFF 268435455 理论上保留两位小数可以支持到268万 无损失
// 小数位最多6位
#define SIS_MAX_ZINT32 0x0FFFFFFF
typedef struct zint32 {
	unsigned int zint    : 28;
	unsigned int attr    : 3;   // 0 有效无缩放 0x7 无效数据 1-6缩小倍数
	unsigned int sign    : 1;   // 0 正 1 负
} zint32;

// 取值范围 0x03FF FFFF FFFF FFFF 
// 小数位最多30位
#define SIS_MAX_ZINT64 0x03FFFFFFFFFFFFFF
typedef struct zint64 {
	unsigned long long zint    : 58;
	unsigned long long attr    : 5;  // 0 有效无缩放 0x1F 无效数据  1-30缩小倍数
	unsigned long long sign    : 1;  // 0 正 1 负
} zint64;

#pragma pack(pop)

static inline zint32 sis_double_to_zint32(double in_, int dot_, bool valid_)
{
    zint32 z = {0};
    if (!valid_ || in_ > (double)SIS_MAX_ZINT32 || in_ < (double)-1*SIS_MAX_ZINT32)
    {
        z.attr = 7;
    }
    else if (SIS_IS_ZERO(in_))
    {
        z.zint = 0;
        z.attr = 0;
        z.sign = 0;
    }
    else
    {
        z.sign = in_ < 0.0;
        double in = z.sign ? -in_ : in_;
        // printf("=1= %f\n", in);
        int maxdot = sis_min(dot_, 6);
        for (int i = 0; i < maxdot; i++)
        {
            if (in * 10 > (double)SIS_MAX_ZINT32)
            {
                z.attr = i;
                break;
            }
            z.attr = i + 1;
            in = in * 10;
        }        
        // printf("=2= %f\n", in_);
        z.zint = (in + 0.5);
        z.attr = z.zint != 0 ? z.attr : 0;
    }
	return z;
};
static inline int32 sis_double_to_int32(double in_, int dot_, bool valid_)
{
    zint32 z  = sis_double_to_zint32(in_, dot_, valid_);
    int32 *i32 = (int32 *)&z;
	return *i32;
}
static inline zint64 sis_double_to_zint64(double in_, int dot_, bool valid_)
{
    zint64 z = {0};
    if (!valid_ || in_ > (double)SIS_MAX_ZINT64 || in_ < (double)-1*SIS_MAX_ZINT64)
    {
        z.attr = 31;
    }
    else
    {
        z.sign = in_ < 0.0;
        double in = z.sign ? -in_ : in_;
        int maxdot = sis_min(dot_, 30);
        for (int i = 0; i < maxdot; i++)
        {
            if (in * 10 > (double)SIS_MAX_ZINT64)
            {
                z.attr = i;
                break;
            }
            z.attr = i + 1;
            in = in * 10;
        }        
        z.zint = (in + 0.5);
        z.attr = z.zint != 0 ? z.attr : 0;
    }
	return z;
};
static inline int64 sis_double_to_int64(double in_, int dot_, bool valid_)
{
    zint64 z = sis_double_to_zint64(in_, dot_, valid_);
	int64 *i64 = (int64 *)&z;
	return *i64;
}
static inline bool sis_zint32_valid(zint32 in_)
{
	return in_.attr != 7;
};
static inline int sis_zint32_dot(zint32 in_)
{
	return in_.attr != 7 ? in_.attr : 0;
};
static inline double sis_zint32_to_double(zint32 in_)
{
    if (in_.attr == 7)
    {
        return 0.0;
    }
    double f = (double)in_.zint / (double)sis_zoom10(in_.attr);
    // printf("32 : %d %d %d %f \n", in_.sign, in_.attr, in_.zint, f);
	return !in_.sign ? f : -f;
};
static inline double sis_int32_to_double(int32 in_)
{
    zint32 *in = (zint32 *)&in_;
    return sis_zint32_to_double(*in);
}
static inline int32 sis_zint32_i(zint32 in_)
{
	return !in_.sign ? in_.zint : -1 * in_.zint; 
};
static inline int32 sis_int32_i(int32 in_)
{
    zint32 *in = (zint32 *)&in_;
	return !in->sign ? in->zint : -1 * in->zint; 
};
static inline bool sis_zint64_valid(zint64 in_)
{
	return in_.attr != 31;
};
static inline int sis_zint64_dot(zint64 in_)
{
	return in_.attr != 31 ? in_.attr : 0;
};

static inline double sis_zint64_to_double(zint64 in_)
{
    if (in_.attr == 31)
    {
        return 0.0;
    }
    double f = (double)in_.zint / (double)sis_zoom10(in_.attr);
    // printf("64 : %d %d %lld %f\n", in_.sign, in_.attr, in_.zint, f);
	return !in_.sign ? f : -f;
};
static inline int64 sis_zint64(int64 in_)
{
    zint64 *in = (zint64 *)&in_;
	return !in->sign ? in->zint : -1 * in->zint; 
};
static inline double sis_int64_to_double(int64 in_)
{
    zint64 *in = (zint64 *)&in_;
    return sis_zint64_to_double(*in);
}
#endif