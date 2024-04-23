#ifndef _SIS_AI_CORR_H
#define _SIS_AI_CORR_H

typedef struct s_sis_ai_corrcoef
{
    int8   valid;     // 是否有效
    int    offset;    // 第二队列相对第一队列的最优偏移值
	double corrcoef;  // 相关系数
} s_sis_ai_corrcoef;

#ifdef __cplusplus
extern "C" {
#endif
// 两个序列的相关性
double sis_ai_corrcoef(double *i1, double *i2, int size);
// 两个序列的最大相关性 offset保存偏移记录数
double sis_ai_corrcoef_offset(double *i1, double *i2, int size, int *offset);

int sis_ai_corrcoef_max(double *i1, double *i2, int size, s_sis_ai_corrcoef *);
// 两个序列的方向相关性 正正相关 正负相关 取最大值返回
double sis_ai_corr_dir(double *i1, double *i2, int size);

double sis_ai_corr_dir_offset(double *i1, double *i2, int size, int *offset);

int sis_ai_corr_dir_max(double *i1, double *i2, int size, s_sis_ai_corrcoef *);

#ifdef __cplusplus
}
#endif

#endif /* __NN_H */