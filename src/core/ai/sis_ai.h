#ifndef _SIS_AI_H
#define _SIS_AI_H

#include <sis_core.h>
#include <sis_list.h>
#include <sis_math.h>
#include <sis_malloc.h>

#define SIS_AI_CONST_PAI     (3.1415927)  // 

// 求等比分布的全部情况，返回到list_中, div切分的等分, count为list单条记录的数量
// 返回总数
int sis_cut_ratio_int(s_sis_struct_list *list_, int count_, int div_);
int sis_cut_ratio_double(s_sis_struct_list *list_, int count_, int div_, double min_, double max_);
int sis_cut_ratio_random(s_sis_struct_list *list_, int inc_, double ins_[], int div_, double min_, double max_);
// 得到最小的索引
int sis_ai_get_min_index(int inc_, double ins_[]);

//////////////////////////////////////////////
//  归一化的算法
//  转换所有数据为 0.001 -- 0.999 之间的数据
//////////////////////////////////////////////
#define SIS_AI_MIN  (0.001)
#define SIS_AI_MAX  (0.999)
#define SIS_AI_MID  (0.5)

// 从连续值中求得标准归一值 mid 无用
double sis_ai_normalization_series(double value_, double min_, double max_);
int sis_ai_normalization_series_array(int nums_, double ins_[], double outs_[], double min_, double max_);
// 从连续值中求得标准归一值 以 mid 为分界线 分处于 0.5 两端 
double sis_ai_normalization_split(double value_, double min_, double max_, double mid_);
int sis_ai_normalization_split_array(int nums_, double ins_[], double outs_[], double min_, double max_, double mid_);

// 求最后一点的斜率 正值为越来越大 负值为越来越小
double sis_ai_series_drift(int nums_, double ins_[]);
// 求最后一点的加速度 正值为跌的越来越慢涨的越来越快 负值为跌的越来越快 涨的越来越慢
double sis_ai_series_acceleration(int nums_, double ins_[]);

// 归一化的最近斜率
double sis_ai_normalization_series_drift(int nums_, double ins_[], double min_, double max_);
// 归一化的最近加速度
double sis_ai_normalization_series_acceleration(int nums_, double ins_[], double min_, double max_);

// 求均值和中位数
int sis_ai_get_avg_and_mid(int nums_, double ins_[], double *avg_, double *mid_);

#define SIS_AI_AVG_MIN    10   //  为避免初始波动过于剧烈 小于10按10比例求均值
#define SIS_AI_AVG_MAX    100  //  小于100 求均值 大于100 后按比例求均值

typedef struct s_ai_avg_m
{
	int8   nums;    // 数量
	float  avgm;    // 平均值
} s_ai_avg_m;

typedef struct s_ai_avg_r
{
	int8   nums;    // 数量
	float  avgm;    // 平均值
	float  avgr;    // 平均比例
} s_ai_avg_r;

void sis_ai_calc_avgm(double in_, s_ai_avg_m *avgm_);
void sis_ai_calc_avgr(double son_, double mom_, s_ai_avg_r *avgr_);

/////////////////////////////////////////////////////////
//  求一组数据的最近的趋势和极值
//  长周期的极值判断方法，主要确定局部周期, 取 N*SIS_AI_GOLD4+1取整为局部周期
//  传入参数限制幅度 f 当最大最小价格差距大于 f 趋势落定，
//  趋势确定后，求二元二次方程，得到极值点， 返回趋势,起始点,最大的波幅
/////////////////////////////////////////////////////////
#define SIS_AI_GOLD5   (0.090) //  0.618 ^ 5
#define SIS_AI_GOLD4   (0.146) //  0.618 ^ 4
#define SIS_AI_GOLD3   (0.236) //  0.618 ^ 3
#define SIS_AI_GOLD2   (0.382) //  0.618 ^ 2
#define SIS_AI_GOLD1   (0.618) //  
#define SIS_AI_GOLD    (0.618) //  


int sis_ai_nearest_drift_future(int nums_, double ins_[], double min_, double max_, double *minrate_, int *stop_, double *drift_);
int sis_ai_nearest_drift_formerly(int nums_, double ins_[], double min_, double max_, double *minrate_, int *start_, double *drift_);

double sis_ai_drift_series(int nums_, double ins_[], double min_, double max_);
double sis_ai_drift_split(int nums_, double ins_[], double min_, double max_, double mid_);

#define	SIS_AI_DRIFT_UP   (1)   
#define	SIS_AI_DRIFT_MID  (0)   
#define	SIS_AI_DRIFT_DN   (-1)   

// 返回最近一个趋势的斜率, 大于 0 表示趋势向上, rate_会返回实际涨跌幅 start_返回拐点 **ins不能有 0** 
typedef struct s_ai_nearest_drift
{
    s_sis_struct_list *ins; // 输入数据
    // s_sis_struct_list *indexs; // 输入数据的索引 可能是不连续的
    double inrate;  // 趋势判断的最小变化幅度
    double inmax;
    double inmin;
    /////////////////////////////////////
    // int    out;     // 高，低，中
    int    offset;  // 最近趋势的起点或者终点
    double rate;    // 最近趋势的实际变化幅度
    double drift;   // 趋势变化的斜率, ** 仅仅判断方向 **
} s_ai_nearest_drift;

s_ai_nearest_drift *sis_ai_nearest_drift_create();
void sis_ai_nearest_drift_destroy(s_ai_nearest_drift *);
void sis_ai_nearest_drift_clear(s_ai_nearest_drift *in_);

int sis_ai_nearest_drift_size(s_ai_nearest_drift *in_);

int sis_ai_nearest_drift_push(s_ai_nearest_drift *in_, double val_);
int sis_ai_nearest_drift_insert(s_ai_nearest_drift *in_, double val_);

// int sis_ai_nearest_drift_push(s_ai_nearest_drift *in_, double val_, int index_);
// int sis_ai_nearest_drift_insert(s_ai_nearest_drift *in_, double val_, int index_);
// int sis_ai_nearest_drift_get_index(s_ai_nearest_drift *in_, int idx_);

int sis_ai_nearest_drift_calc_future(s_ai_nearest_drift *in_, double inrate_, double min_, double max_);
int sis_ai_nearest_drift_calc_formerly(s_ai_nearest_drift *in_, double inrate_, double min_, double max_);

double sis_ai_nearest_diff_formerly(int nums_, double ins_[]);

double sis_ai_normalization_series_slope(int nums_, double ins_[], double min_, double max_, double *rate_);
double sis_ai_normalization_split_slope(int nums_, double ins_[], double min_, double max_, double mid_);

double sis_ai_get_slope(int nums_, double ins_[]);

//////////////////////////////////////////////
// 求趋势线的参数
/////////////////////////////////////////////
void gauss_solve(int n, double A[], double x[], double b[]);

void sis_ai_polyfit(int n, double x[], double y[], int poly_n, double a[]);

double sis_ai_slope(int n, double ins[]);
double sis_ai_slope_rate(int n, double ins[]);
// 求样本均值和标准方差
void   sis_ai_series_argv(int n, double ins[], double *avg, double *vari);
// 求基于连续样本值的概率密度
double sis_ai_series_chance(double in, double avg, double vari);
// 求菲数列的均值, 最近的值权重最高
double sis_ai_fibonacci_avg(int n, double ins[]);

// 判断类型
#define JUDGE_STYLE_NONE     0  // 
#define JUDGE_STYLE_TREND    1  // 趋势类
#define JUDGE_STYLE_SPEED    2  // 趋势变化速度 

// 判断方向和趋势，上涨或下跌
#define TREND_DIR_DECR    -1  // 趋势为递减
#define TREND_DIR_NONE     0  // 一定周期内忽上忽下
#define TREND_DIR_INCR     1  // 趋势为递增
// 判断趋势中的速度
#define SPEED_INCR_FAST    2  // 越来越快
#define SPEED_INCR_SLOW    1  // 越来越慢
#define SPEED_NONE_LESS    0  // 一定周期内变化值太小，认为变化无规律
#define SPEED_DECR_SLOW   -1  // 越来越慢
#define SPEED_DECR_FAST   -2  // 越来越快
// 是否可以计算出股票的动能
// E = mv^2/2 
// 

// 一个完整配置的结构
typedef struct s_sis_calc_cycle
{
    int   style;           // 类型 就是买卖信号
    s_sis_struct_list *list;
} s_sis_calc_cycle;

s_sis_calc_cycle *sis_calc_cycle_create(int style_);
void sis_calc_cycle_destroy(s_sis_calc_cycle *calc_);
void sis_calc_cycle_clear(s_sis_calc_cycle *calc_);

int sis_calc_cycle_push(s_sis_calc_cycle *calc_, double value_);
double *sis_calc_cycle_get(s_sis_calc_cycle *calc_);
int sis_calc_cycle_set_float(s_sis_calc_cycle *calc_, float *value_, int count_);
int sis_calc_cycle_out(s_sis_calc_cycle *calc_);


// 趋势特征
// 取最近的3个二向特征 最左表示最近时间的特征
#define FACTOR_BAGUA_111  (111)   
#define FACTOR_BAGUA_110  (110)   
#define FACTOR_BAGUA_101  (101)   
#define FACTOR_BAGUA_100  (100)   
#define FACTOR_BAGUA_011  ( 11)   
#define FACTOR_BAGUA_010  ( 10)   
#define FACTOR_BAGUA_001  (  1)   
#define FACTOR_BAGUA_000  (  0)   

int8 sis_ai_factor_drift(int n, double *ins, int level);
int8 sis_ai_factor_drift_pair(int n, double *asks, double *bids, int level);

// 返回值根据level级别 比如返回 FACTOR_BAGUA_000 实际返回为1000 如果小于1000就表示特征值未达标 
static inline int sis_ai_factor(int n, double ins[], int level)
{
    if (level > n)
    {
        return 0;
    }
    int ok = 0;
    int o = 0;
    for (int i = n - 1; i >= level - 1 && ok < level; i--)
    { 
        if (ins[i] >= ins[i - 1])
        {
            if (ins[i - 1] > ins[i - 2])
            {
                o = o * 10 + 1;
                ok++;
            }
        }
        else
        {
            if (ins[i - 1] < ins[i - 2])
            {
                o = o * 10;
                ok++;
            }
        }
    }
    if (ok >= level)
    {
        o += sis_zoom10(level);
    }
    return o;
}

static inline int sis_ai_factor_pair(int n, double asks[], double bids[])
{
    if (n < 3)
    {
        return 0;
    }
    int o = 0;
    for (int i = n - 1; i >= n - 3 ; i--)
    { 
        if (asks[i] >= bids[i])
        {
            o = o * 10 + 1;
        }
        else
        {
            o = o * 10;
        }
    }
    return o + 1000;
}
static inline int sis_ai_factor_bagua(int n, double ins[], int level)
{
    if (level > n)
    {
        return 0;
    }
    int ok = 0;
    int o = 0;
    for (int i = n - 1; i >= level - 1 && ok < level; i--)
    { 
        if (ins[i] >= ins[i - 1])
        {
            if ( (ins[i - 1] > ins[i - 2]) || (i >= 3 && ins[i] > ins[i - 1] + ins[i - 2] + ins[i - 3]) )
            {
                o = o * 10 + 1;
                ok++;
            }
        } 
        else 
        {
            if ( (ins[i - 1] < ins[i - 2]) || (i >= 3 && ins[i] < ins[i - 1] + ins[i - 2] + ins[i - 3]) )
            {
                o = o * 10;
                ok++;
            }
        }
    }
    if (ok >= level)
    {
        o += sis_zoom10(level);
    }
    return o;
}

// 转 1011 --> 3
static inline int sis_ai_factor_get_bagua(int in, int level)
{
    int min = sis_zoom10(level);
    if (in < min)
    {
        return 0;
    }
    int o = 0;
    for (int i = 0; i < level; i++)
    {
        int z = sis_zoom10(i + 1);
        int w = in % z;
        w /= sis_zoom10(i);
        if ( w > 0) o = o * 2 + 1;
        else o = o * 2;
    }
    return o;
}
#endif //_SIS_MATH_H