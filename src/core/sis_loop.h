#ifndef _sis_loop_h
#define _sis_loop_h

// 优化内存的队列
// 用于计算时序均值和拐点

#include "sis_math.h"
#include "sis_zint.h"

#pragma pack(push,1)

// 内部都为升序 
typedef struct s_sis_floops {
	int      start;      // 起始位置
	int      count;      // 当前数量
	int      size;       // 最大数量
	float   *value;      // 数据区
	// 以下值
	float    sumv;
	float    avgv;
	int      mini;       // 最小值物理位置
	int      maxi;       // 最大值物理位置
} s_sis_floops;

#pragma pack(pop)

s_sis_floops *sis_floops_create(int size); 
void sis_floops_destroy(void *);
void sis_floops_clear(s_sis_floops *);

void sis_floops_reset_size(s_sis_floops *, int newsize);

float sis_floops_get(s_sis_floops *, int index);
int sis_floops_push(s_sis_floops *, float ); 

float sis_floops_get_avgv(s_sis_floops *);
float sis_floops_get_minv(s_sis_floops *);
float sis_floops_get_maxv(s_sis_floops *);

float sis_floops_get_sumv(s_sis_floops *);

// 求最近的斜率
float sis_floops_calc_avgv(s_sis_floops *, int nums);
// 求最近的斜率
float sis_floops_calc_drift(s_sis_floops *, int nums);
// 求波动率
float sis_floops_calc_waver(s_sis_floops *, int nums);
// 返回重新计算的均值
float sis_floops_recalc(s_sis_floops *);

#endif