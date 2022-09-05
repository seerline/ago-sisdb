#ifndef _SIS_AI_CORR_H
#define _SIS_AI_CORR_H

typedef struct s_sis_ai_corrcoef
{
    int    offset;    // 第二队列相对第一队列的最优偏移值
	double corrcoef;  // 相关系数
} s_sis_ai_corrcoef;

#ifdef __cplusplus
extern "C" {
#endif

double sis_ai_corrcoef(double *i1, double *i2, int size);

double sis_ai_corrcoef_ex(double *i1, double *i2, int size, int *offset);

int sis_ai_corrcoef_max(double *i1, double *i2, int size, s_sis_ai_corrcoef *);

#ifdef __cplusplus
}
#endif

#endif /* __NN_H */