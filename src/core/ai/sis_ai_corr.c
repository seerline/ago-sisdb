
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
    for (int i = 0; i < size; i++)
    {
        r1 += (i1[i] - ave1) * (i2[i] - ave2);
        r2 += pow((i1[i] - ave1), 2);
        r3 += pow((i2[i] - ave2), 2);
    }
    return SIS_IS_ZERO(r2 * r3) ? 0.0 : (r1 / sqrt(r2 * r3));
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
    for (int i = 0; i < 12; i++)
    {
        in2[i] = in1[i] * 1000;
    }
    printf(" %.4f \n", sis_ai_corrcoef(in1, in2, 12));
    return 0;
}
#endif