#ifndef _STS_MATH_H
#define _STS_MATH_H

#include <sts_core.h>
#include <math.h>
#include <sts_malloc.h>

#define  LOWORD(l)         ( (unsigned short)(l) )
#define  HIWORD(l)         ( (unsigned short)(((unsigned int)(l) >> 16) & 0xFFFF) )
#define  MERGEINT(a,b)     ( (unsigned int)( ((unsigned short)(a) << 16) | (unsigned short)(b)) )

#define  sts_max(a,b)    (((a) > (b)) ? (a) : (b))
#define  sts_min(a,b)    (((a) < (b)) ? (a) : (b))

//限制返回值a在某个区域内
#define  sts_between(a,min,max)    (((a) < (min)) ? (min) : (((a) > (max)) ? (max) : (a)))

inline int64 sts_zoom10(int n)  // 3 ==> 1000
{
	int64 o = 1;
	int len = (n > 15) ? 15 : n;
	for (int i = 0; i < len; i++) { o = o * 10; }
	return o;
};

inline int sts_sqrt10(int n)  // 1000 ==> 3  -1000 ==> -3
{
	int rtn = 0;
	while (1)
	{
		n = (int)(n / 10);
		if (n >= 1) { rtn++; }
		else { break; }
	}
	return rtn;
};

inline bool sts_isbetween(int value_, int min_, int max_)
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

//////////////////////////////////////////////
//
/////////////////////////////////////////////
inline void gauss_solve(int n, double A[], double x[], double b[])
{
	int i, j, k, r;
	double max;
	for (k = 0; k < n - 1; k++)
	{
		max = fabs(A[k*n + k]); /*find maxmum*/
		r = k;
		for (i = k + 1; i < n - 1; i++){
			if (max < fabs(A[i*n + i]))
			{
				max = fabs(A[i*n + i]);
				r = i;
			}
		}
		if (r != k){
			for (i = 0; i < n; i++) /*change array:A[k]&A[r] */
			{
				max = A[k*n + i];
				A[k*n + i] = A[r*n + i];
				A[r*n + i] = max;
			}
		}
		max = b[k]; /*change array:b[k]&b[r] */
		b[k] = b[r];
		b[r] = max;
		for (i = k + 1; i < n; i++)
		{
			for (j = k + 1; j < n; j++)
			{
				A[i*n + j] -= A[i*n + k] * A[k*n + j] / A[k*n + k];
			}
			b[i] -= A[i*n + k] * b[k] / A[k*n + k];
		}
	}
	for (i = n - 1; i >= 0; x[i] /= A[i*n + i], i--)
	{
		for (j = i + 1, x[i] = b[i]; j < n; j++)
		{
			x[i] -= A[i*n + j] * x[j];
		}
	}

}
/*==================polyfit(n,x,y,poly_n,a)===================*/
/*=======拟合y=a0+a1*x+a2*x^2+……+apoly_n*x^poly_n========*/
/*=====n是数据个数 xy是数据值 poly_n是多项式的项数======*/
/*===返回a0,a1,a2,……a[poly_n]，系数比项数多一（常数项）=====*/
inline void polyfit(int n, double x[], double y[], int poly_n, double a[])
{
	int i, j;
	double *tempx, *tempy, *sumxx, *sumxy, *ata;
	//void gauss_solve(int n, double A[], double x[], double b[]);
	tempx = (double*)calloc(n, sizeof(double));
	sumxx = (double*)calloc(poly_n * 2 + 1, sizeof(double));
	tempy = (double*)calloc(n, sizeof(double));
	sumxy = (double*)calloc(poly_n + 1, sizeof(double));
	ata = (double*)calloc((poly_n + 1)*(poly_n + 1), sizeof(double));
	for (i = 0; i<n; i++)
	{
		tempx[i] = 1;
		tempy[i] = y[i];
	}
	for (i = 0; i < 2 * poly_n + 1; i++){
		for (sumxx[i] = 0, j = 0; j < n; j++)
		{
			sumxx[i] += tempx[j];
			tempx[j] *= x[j];
		}
	}
	for (i = 0; i < poly_n + 1; i++)
	{
		for (sumxy[i] = 0, j = 0; j < n; j++)
		{
			sumxy[i] += tempy[j];
			tempy[j] *= x[j];
		}
	}
	for (i = 0; i < poly_n + 1; i++)
	{
		for (j = 0; j < poly_n + 1; j++)
		{
			ata[i*(poly_n + 1) + j] = sumxx[i + j];
		}
	}
	gauss_solve(poly_n + 1, ata, a, sumxy);
	sts_free(tempx);
	sts_free(sumxx);
	sts_free(tempy);
	sts_free(sumxy);
	sts_free(ata);
}
#endif //_STS_MATH_H