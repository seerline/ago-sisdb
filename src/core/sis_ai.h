#ifndef _SIS_AI_H
#define _SIS_AI_H

#include <sis_core.h>
#include <sis_list.h>
#include <sis_math.h>
#include <sis_malloc.h>

#define SIS_AI_CONST_PAI     (3.1415927)  // 

//////////////////////////////////////////////
// 求趋势线的参数
/////////////////////////////////////////////
void gauss_solve(int n, double A[], double x[], double b[]);

void polyfit(int n, double x[], double y[], int poly_n, double a[]);

double sis_ai_slope(int n, double ins[]);
double sis_ai_slope_rate(int n, double ins[]);
// 以均值和方差求贝叶斯分类
void   sis_ai_series_argv(int n, double ins[], double *avg, double *vari);
double sis_ai_series(double in, double avg, double vari);

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


#endif //_SIS_MATH_H