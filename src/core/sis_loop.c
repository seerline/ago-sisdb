

#include "sis_loop.h" 
#include "sis_ai.h" 

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_floops ------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sis_floops *sis_floops_create(int size)
{
	s_sis_floops *o = SIS_MALLOC(s_sis_floops, o);
	o->size  = size;
	if (size > 0)
	{
		o->value = sis_malloc(sizeof(float) * size);
		memset(o->value, 0, sizeof(float) * size);
	}
	o->mini = -1;
	o->maxi = -1;
	return o;
}
void sis_floops_destroy(void *floops_)
{
	s_sis_floops *floops = floops_;
	sis_free(floops->value);
	sis_free(floops); 
}
void sis_floops_reset_size(s_sis_floops *floops, int newsize)
{
	if (newsize > floops->size)
	{
		float *value = sis_malloc(sizeof(float) * newsize);
		if (floops->count > 0)
		{
			if (floops->start > 0)
			{
				memmove(value, &floops->value[floops->start], (floops->size - floops->start) * sizeof(float));
				memmove(&value[floops->size - floops->start], floops->value, (floops->count + floops->start - floops->size) * sizeof(float));
				floops->start = 0;
			}
			else
			{
				memmove(value, &floops->value[floops->start], floops->count * sizeof(float));
			}
		}
		sis_free(floops->value);
		floops->value = value;
	}
	// if (newsize < floops->size)
	// {
	// 	sis_floops_recalc(floops);
	// }
}

void sis_floops_clear(s_sis_floops *floops)
{
	if (floops)
	{
		floops->start = 0;
		floops->count = 0;
		memset(floops->value, 0, sizeof(float) * floops->size);
		floops->avgv = 0;
		floops->sumv = 0;
		floops->mini = -1;
		floops->maxi = -1;
	}
}

float sis_floops_get(s_sis_floops *floops, int index_)
{
	if (index_ < 0 || index_ > floops->count - 1)
	{
		return 0.0;
	}
	int index = (floops->start + index_) % floops->size;
	return floops->value[index];
}
int sis_floops_push(s_sis_floops *floops, float fv)
{
	if (floops->count < floops->size)
	{
		int index = (floops->start + floops->count) % floops->size;
		// 计算其他值
		if (floops->count <= 0)
		{
			floops->mini = floops->start;
			floops->maxi = floops->start;
			floops->avgv = fv;
			floops->sumv = fv;
			floops->count = 0;
		}
		else
		{
			if (fv >= floops->value[floops->maxi])
			{
				floops->maxi = index;
			}
			if (fv <= floops->value[floops->mini])
			{
				floops->mini = index;
			}
			floops->sumv = floops->sumv + fv;
			floops->avgv = SIS_DIVF(floops->sumv , (floops->count + 1));
		}
		// 赋新值
		floops->value[index] = fv;
		floops->count += 1;
	}
	else
	{
		int index = (floops->start + floops->count) % floops->size;
		{
			if (fv >= floops->value[floops->maxi])
			{
				floops->maxi = index;
			}
			else if (floops->maxi == floops->start)
			{
				// 重新计算最大值
				floops->maxi = (index + 1) % floops->size;
				for (int i = 1; i < floops->count; i++)
				{
					float ofv = sis_floops_get(floops, i);
					if (ofv >= floops->value[floops->maxi])
					{
						floops->maxi = (floops->start + i) % floops->size;
					}
				}
			}
			if (fv <= floops->value[floops->mini])
			{
				floops->mini = index;
			}
			else if (floops->mini == floops->start)
			{
				// 重新计算最小值
				floops->mini = (index + 1) % floops->size;
				for (int i = 1; i < floops->count; i++)
				{
					float ofv = sis_floops_get(floops, i);
					if (ofv <= floops->value[floops->mini])
					{
						floops->mini = (floops->start + i) % floops->size;
					}
				}
			}
			floops->sumv = floops->sumv - floops->value[floops->start] + fv;
			floops->avgv = SIS_DIVF(floops->sumv , floops->count);
		}
		floops->value[index] = fv;
		floops->start = (index + 1) % floops->size;
	}
	return floops->count;
} 
float sis_floops_get_avgv(s_sis_floops *floops)
{
	return floops->avgv;
}
float sis_floops_get_minv(s_sis_floops *floops)
{
	return floops->value[floops->mini];
}
float sis_floops_get_maxv(s_sis_floops *floops)
{
	return floops->value[floops->maxi];
}
float sis_floops_get_sumv(s_sis_floops *floops)
{
	return floops->sumv;
}

float sis_floops_calc_avgv(s_sis_floops *floops, int nums)
{
	if (nums < 0)
	{
		nums = floops->count;
	}
	else
	{
		nums = sis_min(nums, floops->count);
	}
	if (nums <= 0)
	{
		return 0.0;
	}
	double vv = 0.0;
	for (int i = nums - 1, k = floops->count - 1; i >= 0 && k >= 0; i--, k--)
	{
		vv += sis_floops_get(floops, k);
	}
	return SIS_DIVF(vv, nums);
}
float sis_floops_calc_drift(s_sis_floops *floops, int nums)
{
	if (nums < 0)
	{
		nums = floops->count;
	}
	else
	{
		nums = sis_min(nums, floops->count);
	}
	if (nums <= 0)
	{
		return 0.0;
	}
	double *vv = sis_malloc(floops->count * sizeof(double));
	for (int i = nums - 1, k = floops->count - 1; i >= 0 && k >= 0; i--, k--)
	{
		vv[i] = sis_floops_get(floops, k);
	}
	float v = sis_ai_series_drift(nums, vv) * 100.0;
	sis_free(vv);
	return v;
}
float sis_floops_calc_waver(s_sis_floops *floops, int nums)
{
	if (nums < 0)
	{
		nums = floops->count;
	}
	else
	{
		nums = sis_min(nums, floops->count);
	}
	if (nums <= 1)
	{
		return 0.0;
	}
	double waver = 0.0;
	for (int i = 0; i < nums; i++)
	{
		double w = sis_floops_get(floops, i + 1);
		w = SIS_DIVF(fabs(w - sis_floops_get(floops, i)), w);
		waver += w;
	}
	return waver / nums;
}

float sis_floops_recalc(s_sis_floops *floops)
{
	if (floops->count <= 0)
	{
		return 0.0;
	}
	floops->avgv = 0.0;
	floops->sumv = 0.0;
	int inited = 0;
	for (int i = 0; i < floops->count; i++)
	{
		float fv = sis_floops_get(floops, i);
		floops->sumv += fv;
		if (inited == 0)
		{
			floops->mini = floops->start;
			floops->maxi = floops->start;
			inited = 1;
		}
		else
		{
			if (fv >= floops->value[floops->maxi])
			{
				floops->maxi = (floops->start + i) % floops->size;
			}
			if (fv <= floops->value[floops->mini])
			{
				floops->mini = (floops->start + i) % floops->size;
			}
		}
	}
	floops->avgv = SIS_DIVF(floops->sumv, floops->count);
	return floops->avgv;
}


#if 0
void printf_floops(s_sis_floops *g)
{
	printf("==0== count:%d %d %d | %d %f %d %f  %.6f===== \n", g->count, g->size, g->start, 
		g->maxi, sis_floops_get_maxv(g), 
		g->mini, sis_floops_get_minv(g), g->avgv);
	for (int i = 0; i < g->count; i++)
	{
		printf(":%d: %f | %f\n", i, g->value[i], sis_floops_get(g, i)); 
	}
}

int main()
{	
	safe_memory_start();
	s_sis_floops *g = sis_floops_create(4);

	// double groupv[] = {10.22, 5.34, -2.23, -5.2, -0.0, 0.5,  1.22,  4.33, -2.23,  6.88}; 
	// double groupv[] = {1.22,  8.34,  2.31, 1.2, 2.01, 1.05, 3.22, 0.33,  1.2, 1.88}; 
	double groupv[] = {2,  1,  3,  4,  5,  5,  6,  7,  5, 8}; 

	for (int i = 0; i < 10; i++)
	{
		sis_floops_push(g, groupv[i]);
		printf_floops(g);
	}
	sis_floops_recalc(g);
	printf_floops(g);
	sis_floops_clear(g);
	printf_floops(g);
	// safe_memory_stop();
	sis_floops_destroy(g);
	safe_memory_stop();
	return 0;
}


#endif