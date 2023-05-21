
#include <sis_core.h>
#include <sis_ai_corr.h>
#include <sis_math.h>

// 计算相关系数
// i1	自变量A数组
// i2	自变量B数组
// size 数组长度
double sis_ai_corrcoef(double *i1, double *i2, int size)
{
    double sum1, sum2, ave1, ave2;
    //求和
    sum1 = 0;
    for (int i = 0; i < size; i++)
    {
        sum1 += i1[i];
    }
    sum2 = 0;
    for (int i = 0; i < size; i++)
    {
        sum2 += i2[i];
    }
    //求平均值
    ave1 = sum1 / (double)size;
    ave2 = sum2 / (double)size;

    //计算相关系数
    double r1 = 0, r2 = 0, r3 = 0;
    for (long i = 0; i < size; i++)
    {
        r1 += (i1[i] - ave1) * (i2[i] - ave2);
        r2 += pow((i1[i] - ave1), 2);
        r3 += pow((i2[i] - ave2), 2);
    }
    return SIS_IS_ZERO(r2 * r3) ? 0.0 : (r1 / sqrt(r2 * r3));
}
// 偏移值不大于总数据量的4/10 避免过度拟合
double sis_ai_corrcoef_offset(double *i1, double *i2, int size, int *offset)
{
    int maxoff = *offset;
    maxoff = sis_min(maxoff, size * 0.382 + 1);
    double maxcorr = sis_ai_corrcoef(i1, i2, size);
    *offset = 0;
    for (int i = 1; i <= maxoff; i++)
    {
        double corr = sis_ai_corrcoef(i1, &i2[i], size - i);
        if (corr >= maxcorr)
        {
            *offset = i;
            maxcorr = corr;
        }
        else
        {
            break;
        }
    }
    return maxcorr;
}
int sis_ai_corrcoef_max(double *i1, double *i2, int size, s_sis_ai_corrcoef *corr)
{
    if (corr && size > 5)
    {
        corr->offset = size * 0.382 + 1;
        corr->offset = sis_max(corr->offset, 1);
        corr->corrcoef = sis_ai_corrcoef_offset(i1, i2, size, &corr->offset);
        return 1;
    }
    return 0;
}
#define CORR_MINV (0.00000001)
double sis_ai_corr_dir(double *i1, double *i2, int size)
{
    int sames = 0;
    int diffs = 0;
    for (int i = 0; i < size; i++)
    {
        if ((i1[i] > CORR_MINV && i2[i] > CORR_MINV) || (i1[i] < -1 * CORR_MINV && i2[i] < -1 * CORR_MINV))
        {
            sames = sames + 1;
        }
        else
        {
            diffs = diffs + 1;
        }
    }
    if (sames == diffs)
    {
        return 0.0;
    }
    double corr = sames > diffs ? (double)sames / (double)size : -1 * (double)diffs / (double)size;
    return (corr - 0.5) * 2.0;
}

double sis_ai_corr_dir_offset(double *i1, double *i2, int size, int *offset)
{
    int maxoff = *offset;
    maxoff = sis_min(maxoff, size * 0.382 + 1);
    double maxcorr = sis_ai_corr_dir(i1, i2, size);
    *offset = 0;
    for (int i = 1; i <= maxoff; i++)
    {
        double corr = sis_ai_corr_dir(i1, &i2[i], size - i);
        if (corr >= maxcorr)
        {
            *offset = i;
            maxcorr = corr;
        }
        else
        {
            break;
        }
    }
    return maxcorr;
}
int sis_ai_corr_dir_max(double *i1, double *i2, int size, s_sis_ai_corrcoef *corr)
{
    if (corr && size > 5)
    {
        corr->offset = size * 0.382 + 1;
        corr->offset = sis_max(corr->offset, 1);
        corr->corrcoef = sis_ai_corr_dir_offset(i1, i2, size, &corr->offset);
        return 1;
    }
    return 0;
}

#if 0
int main()
{
    double in1[12] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};   
    double in2[12] = { 8.1, 6.5, 3.1, 0.1, 1.3, 2.1, 3.5, 4.9, 5.1, 6.1, 7.1, 8.9};  
    printf(" %.4f \n", sis_ai_corrcoef(in1, in2, 12));

    printf(" %.4f \n", sis_ai_corrcoef(in1, &in2[1], 11));
    printf(" %.4f \n", sis_ai_corrcoef(in1, &in2[2], 10));
    printf(" %.4f \n", sis_ai_corrcoef(in1, &in2[3], 9));
    printf(" %.4f \n", sis_ai_corrcoef(in1, &in2[4], 8));

    int offset = 5;
    printf(" %.4f offset = %d\n", sis_ai_corrcoef_offset(in1, in2, 12, &offset), offset);
    for (int i = 0; i < 12; i++)
    {
        in2[i] = in1[i] * 1000;
    }
    in2[8] /= 10; 
    printf(" %.4f \n", sis_ai_corrcoef(in1, in2, 12));

    double in11[12]; 
    double in21[12];
    for (int i = 0; i < 12; i++)
    {
        in11[i] = (int)in1[i] % 2 == 0 ? -1 : 1;
        in21[i] = in11[i];
    }
    in21[8] *= -1; 
    in21[4] *= -1; 
    printf(" %.4f \n", sis_ai_corrcoef(in11, in21, 12));
    return 0;
}
#endif