#ifndef _SIS_AI_CORR_H
#define _SIS_AI_CORR_H

#ifdef __cplusplus
extern "C" {
#endif
// 两个序列的相关性
double sis_ai_corrcoef(double *i1, double *i2, int size);
// 两个序列的方向相关性 正正相关 正负相关 取最大值返回
double sis_ai_corr_dir(double *i1, double *i2, int size);

#ifdef __cplusplus
}
#endif

#endif /* __NN_H */