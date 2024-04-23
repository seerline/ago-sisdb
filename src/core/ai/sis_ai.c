
#include <sis_core.h>
#include <sis_ai.h>

    // 1 1
    // 2 2  ==> (1,1)(1,2)(1,3) (2,1)(2,2)(2,3) (3,1)(3,2)(3,3)
    // 3 3  
    // for (int i = 0; i < divc_; i++)
    // {
    //     for (int j = 0; j < divc_; j++)
    //     {
    //         ins_[0] = divs_[i];           
    //         ins_[1] = divs_[j];   
    //     }
    // }

    // 1 1 1
    // 2 2 2 ==> (1,1,1)(1,1,2)(1,1,3) (1,2,1)(1,2,2)(1,2,3) (1,3,1)(1,3,2)(1,3,3) ... 
    // 3 3 3 
    // for (int i = 0; i < divc_; i++)
    // {
    //     for (int j = 0; j < divc_; j++)
    //     {
    //         for (int k = 0; k < divc_; k++)
    //         {
    //             ins_[0] = divs_[i];
    //             ins_[1] = divs_[j];
    //             ins_[2] = divs_[k];
    //         }
    //     }
    // }

// 针对不同基准值 ins_ 生成数据，以百分比gap为间隔左右个取div个间隔 默认在 （0..100）之间
// （50, 0.1, 5）=> (25,30,35,40,45 |50| 55,60,65,70,75)
int sis_cut_ratio_random(s_sis_struct_list *list_, int inc_, double ins_[], int div_, double min_, double max_)
{
    s_sis_struct_list *ilist = (s_sis_struct_list *)sis_struct_list_create(inc_ * sizeof(int));
    
    int nums = 2 * div_ + 1;
    double gap = 1 / (double)(2 * div_);
    // printf("gap=%f \n",gap);
    sis_cut_ratio_int(ilist, inc_, nums);

    // for (int i = 0; i < ilist->count; i++)
    // {
    //     int *vals = (int *)sis_struct_list_get(ilist, i);
    //     printf("[%d:%d] ",ilist->count, i);
    //     for (int j = 0; j < inc_; j++)
    //     {
    //         printf("%d ", vals[j]);
    //     }
    //     printf("\n");       
    // }

    sis_struct_list_clear(list_);

    double mid = (max_ - min_) / 2.0;
    double *ins = sis_malloc(inc_ * sizeof(double));
    for (int i = 0; i < ilist->count; i++)
    {
        int *vals = (int *)sis_struct_list_get(ilist, i);
        bool ok = true;
        for (int k = 0; k < inc_; k++)
        {
            if (ins_[k] > mid)
            {
                ins[k] = ins_[k] + (max_ - ins_[k]) * (double)(vals[k] - div_ - 1) * gap;
            }
            else if (ins_[k] < mid)
            {
                ins[k] = ins_[k] + (ins_[k] - min_) * (double)(vals[k] - div_ - 1) * gap;
            }
            else
            {
                ins[k] = ins_[k] + ins_[k] * (double)(vals[k] - div_ - 1) * gap;
            } 
            // printf("gap=%f %f : %f %f\n",gap,mid, ins[k], ins_[k]);
            if (ins[k] > max_ || ins[k] < min_)
            {
                ok = false;
                break;
            }
        } 
        if (ok)
        {
            sis_struct_list_push(list_, ins);   
        }    
    }
    sis_free(ins);
    sis_struct_list_destroy(ilist);
    return list_->count;
}
int sis_cut_ratio_double(s_sis_struct_list *list_, int count_, int div_, double min_, double max_)
{
    sis_struct_list_clear(list_);

    double gaps = (max_ - min_) / (double)div_;
    // 提取分割点,忽略最小值
    int nums = div_;
    double *divs = sis_malloc(nums*sizeof(double));
    for (int i = 0; i < nums; i++)
    {
        divs[i] = min_ + (i + 1) * gaps;
    }
    divs[nums - 1] = max_;
    // [1,2,3]
    double *ins = sis_malloc(count_*sizeof(double));
    int count = (int)pow(nums, count_);
    for (int m = 0; m < count; m++)
    {
        for (int i = 0; i < count_; i++)
        {   
            int xx = m / (int)pow(nums, i) % nums;  
            ins[count_ - 1 - i] = divs[xx];
            // printf("%d %d | %d %.0f\n", m, i, xx, divs[xx]);
        }
        sis_struct_list_push(list_, ins);
    }
    sis_free(ins);
    return list_->count;
}

int sis_cut_ratio_int(s_sis_struct_list *list_, int count_, int div_)
{
    sis_struct_list_clear(list_);

    // 提取分割点,忽略最小值
    int nums = div_;
    int *divs = sis_malloc(nums*sizeof(int));
    for (int i = 0; i < nums; i++)
    {
        divs[i] = i + 1;
    }
    // [1,2,3]
    int *ins = sis_malloc(count_*sizeof(int));
    int count = (int)pow(nums, count_);
    for (int m = 0; m < count; m++)
    {
        for (int i = 0; i < count_; i++)
        {   
            int xx = m / (int)pow(nums, i) % nums;  
            ins[count_ - 1 - i] = divs[xx];
            // printf("%d %d | %d %.0f\n", m, i, xx, divs[xx]);
        }
        sis_struct_list_push(list_, ins);
    }
    sis_free(divs);
    sis_free(ins);
    return list_->count;
}

int sis_ai_get_min_index(int inc_, double ins_[])
{
    int index = 0;
    double minv = ins_[0];
    for (int i = 1; i < inc_; i++)
    {
        if (minv > ins_[i])
        {
            minv = ins_[i];
            index = i;
        }
    }
    // printf("sis_ai_get_min_index %d %.2f\n",index, minv);
    return index;
}

// 从连续值中求得标准归一值 mid 无用
double sis_ai_normalization_series(double value_, double min_, double max_)
{
    if (max_ <= min_) return SIS_AI_MIN;
    if (value_ <= min_) return SIS_AI_MIN;
    if (value_ >= max_) return SIS_AI_MAX;
    return SIS_AI_MIN + (SIS_AI_MAX - SIS_AI_MIN) * (value_ - min_) / (max_ - min_);
}
int sis_ai_normalization_series_array(int nums_, double ins_[], double outs_[], double min_, double max_)
{
    if (min_ == 0 && max_ == 0)
    {
        min_ = ins_[0];
        max_ = ins_[0];
        for (int m = 1; m < nums_; m++)
        {
            min_ = sis_min(min_, ins_[m]);
            max_ = sis_max(max_, ins_[m]);
        }
    }
    for (int m = 0; m < nums_; m++)
    {
        outs_[m] = sis_ai_normalization_series(ins_[m], min_, max_);
    }
    return nums_;
}
// 从连续值中求得标准归一值 以 mid 为分界线 分处于 0.5 两端 
double sis_ai_normalization_split(double value_, double min_, double max_, double mid_)
{
    if (max_ <= min_ || mid_ <= min_ || mid_ >= max_) return SIS_AI_MIN;
    if (value_ <= min_) return SIS_AI_MIN;
    if (value_ >= max_) return SIS_AI_MAX;
    if (value_ > mid_)
    {
        return SIS_AI_MID + (SIS_AI_MAX - SIS_AI_MID) * (value_ - mid_) / (max_ - mid_);
    }
    return SIS_AI_MIN + (SIS_AI_MID - SIS_AI_MIN) * (value_ - min_) / (mid_ - min_);
}
int sis_ai_normalization_split_array(int nums_, double ins_[], double outs_[], double min_, double max_, double mid_)
{
    if (min_ == 0 && max_ == 0 && mid_ == 0)
    {
        double sum = ins_[0];
        min_ = ins_[0];
        max_ = ins_[0];
        for (int m = 1; m < nums_; m++)
        {
            sum += ins_[m];
            min_ = sis_min(min_, ins_[m]);
            max_ = sis_max(max_, ins_[m]);
        }
        mid_ = sum / (double) nums_;
    }
    for (int m = 0; m < nums_; m++)
    {
        outs_[m] = sis_ai_normalization_split(ins_[m], min_, max_, mid_);
    }
    return nums_;    
}
double sis_ai_series_drift(int nums_, double ins_[])
{
    if (nums_ < 5)
    {
        return 0.0;
    }
    // 求均值
    double avg = 0.0;
    for (int i = 0; i < nums_; i++)
    {
        avg += ins_[i]; 
    }
    avg /= nums_;
    if (SIS_IS_ZERO(avg))
    {
        return 0.0;
    }
    // 求斜率
    double *x = (double *)sis_calloc(sizeof(double) * nums_);
    double *outs = (double *)sis_calloc(sizeof(double) * nums_);
    for (int m = 0; m < nums_; m++)
    {
        x[m] = m; 
        outs[m] = ins_[m] / avg;
    }
    double xs[5];
    sis_ai_polyfit(nums_, x, outs, 3, xs);

    sis_free(x);
    sis_free(outs);

    return (3 * xs[3]*(nums_-1)*(nums_-1) + 2 * xs[2]*(nums_-1) + xs[1]);// / xs[1];     
}
double sis_ai_series_acceleration(int nums_, double ins_[])
{
    if (nums_ < 5)
    {
        return 0.0;
    }
    // 求均值
    double avg = 0.0;
    for (int i = 0; i < nums_; i++)
    {
        avg += ins_[i]; 
    }
    avg /= nums_;
    // 求斜率
    double *x = (double *)sis_calloc(sizeof(double) * nums_);
    double *outs = (double *)sis_calloc(sizeof(double) * nums_);
    for (int m = 0; m < nums_; m++)
    {
        x[m] = m; 
        outs[m] = ins_[m] / avg;
    }
    double xs[5];
    sis_ai_polyfit(nums_, x, outs, 3, xs);

    for (int m = 0; m < nums_; m++)
    {
        x[m] = m; 
        outs[m] = (3 * xs[3]*m*m + 2 * xs[2]*m + xs[1]);
    }
    sis_ai_polyfit(nums_, x, outs, 2, xs);

    sis_free(x);
    sis_free(outs);

    return 2 * xs[2]*(nums_-1) + xs[1];    
}
double sis_ai_normalization_series_drift(int nums_, double ins_[], double min_, double max_)
{
    double *outs = (double *)sis_calloc(sizeof(double) * nums_);
    sis_ai_normalization_series_array(nums_, ins_, outs, min_, max_);
    double o = 0.0;
    o = sis_ai_series_drift(nums_, outs);
    sis_free(outs);
    return o;
}
double sis_ai_normalization_series_acceleration(int nums_, double ins_[], double min_, double max_)
{
    double *outs = (double *)sis_calloc(sizeof(double) * nums_);
    sis_ai_normalization_series_array(nums_, ins_, outs, min_, max_);
    double o = 0.0;
    o = sis_ai_series_acceleration(nums_, outs);
    sis_free(outs);
    return o;
}

// 求均值和中位数
int sis_ai_get_avg_and_mid(int nums_, double ins_[], double *avg_, double *mid_)
{
    if (nums_ > 1)
    {
        if (*avg_)
        {
            double sums = 0.0;
            for (int i = 0; i < nums_; i++)
            {
                sums += ins_[i];
            }
            *avg_ = sums / nums_;
        }
        if (*mid_)
        {
            double *ins = sis_malloc(sizeof(double)*nums_);
            memmove(ins, ins_, sizeof(double)*nums_);
            qsort(ins, nums_, sizeof(double), sis_sort_double_list);
            if (nums_ % 2 == 0)
            {
                int index = nums_ / 2 - 1;
                *mid_ = (ins_[index] + ins_[index + 1]) / 2.0;  
            }
            else
            {
                int index = nums_ / 2;
                *mid_ = ins_[index];  
            }
        }
    }
    else if (nums_ == 1)
    {
        *avg_ = *avg_ ? ins_[0] : 0.0;
        *mid_ = *mid_ ? ins_[0] : 0.0;
    } 
    return 0;
}

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////

void sis_ai_calc_avgm(double in_, s_ai_avg_m *avgm_)
{
    if (SIS_IS_ZERO(in_))
    {
        return ;
    }
    if (avgm_->nums == 0)
    {
        avgm_->nums = 1;
        avgm_->avgm = in_;
    }
    else if (avgm_->nums < SIS_AI_AVG_MIN)
    {
        avgm_->avgm = (avgm_->avgm * (SIS_AI_AVG_MIN - 1) + in_) / SIS_AI_AVG_MIN;
        avgm_->nums ++;                
    }
    else if (avgm_->nums < SIS_AI_AVG_MAX)
    {
        avgm_->avgm = (avgm_->avgm * avgm_->nums + in_) / (avgm_->nums + 1);        
        avgm_->nums ++;
    }
    else
    {
        avgm_->avgm = (avgm_->avgm * (SIS_AI_AVG_MAX - 1) + in_) / SIS_AI_AVG_MAX;                
    }
}

void sis_ai_calc_avgr(double son_, double mom_, s_ai_avg_r *avgr_)
{
    if (SIS_IS_ZERO(mom_) || (SIS_IS_ZERO(son_) && avgr_->nums < SIS_AI_AVG_MIN))
    {
        return ;
    }
    double newv = son_ / mom_;
    if (avgr_->nums == 0)
    {
        avgr_->nums = 1;
        avgr_->avgm = son_;
        avgr_->avgr = newv;
    }
    else if (avgr_->nums < SIS_AI_AVG_MIN)
    {
        double newm = (avgr_->avgm * (SIS_AI_AVG_MIN - 1) + son_);
        avgr_->avgr = (avgr_->avgm * (SIS_AI_AVG_MIN - 1)) / newm * avgr_->avgr + 
                    son_ / newm * newv;
        avgr_->avgm = newm / SIS_AI_AVG_MIN;
        avgr_->nums = avgr_->nums + 1;                
    }
    else if (avgr_->nums < SIS_AI_AVG_MAX)
    {
        double newm = (avgr_->avgm * avgr_->nums + son_);
        avgr_->avgr = (avgr_->avgm * avgr_->nums) / newm * avgr_->avgr + 
                     son_ / newm * newv;
        avgr_->nums = avgr_->nums + 1;
        avgr_->avgm = newm / avgr_->nums;        
    }
    else
    {
        double newm = (avgr_->avgm * (SIS_AI_AVG_MAX - 1) + son_);
        avgr_->avgr = (avgr_->avgm * (SIS_AI_AVG_MAX - 1)) / newm * avgr_->avgr + 
                     son_ / newm * newv;
        avgr_->avgm = newm / SIS_AI_AVG_MAX;                
    }
}

/////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////
s_ai_nearest_drift *sis_ai_nearest_drift_create()
{
    s_ai_nearest_drift *o = SIS_MALLOC(s_ai_nearest_drift, o);
    o->ins = sis_struct_list_create(sizeof(double));
    // o->indexs = sis_struct_list_create(sizeof(int));
    return o;
}
void sis_ai_nearest_drift_destroy(s_ai_nearest_drift *in_)
{
    sis_struct_list_destroy(in_->ins);
    // sis_struct_list_destroy(in_->indexs);
    sis_free(in_);
}
void sis_ai_nearest_drift_clear(s_ai_nearest_drift *in_)
{
    sis_struct_list_clear(in_->ins);
    // sis_struct_list_clear(in_->indexs);
    in_->rate = 0.0;
    in_->drift = 0.0;
    in_->offset = 0;
}
int sis_ai_nearest_drift_push(s_ai_nearest_drift *in_, double val_)
{
    // sis_struct_list_push(in_->indexs, &idx_);
    return sis_struct_list_push(in_->ins, &val_);
}
int sis_ai_nearest_drift_insert(s_ai_nearest_drift *in_, double val_)
{
    // sis_struct_list_insert(in_->indexs, 0, &idx_);
    return sis_struct_list_insert(in_->ins, 0, &val_);
}
// int sis_ai_nearest_drift_get_index(s_ai_nearest_drift *in_, int idx_)
// {
//     return *(int *)sis_struct_list_get(in_->indexs, idx_);
// }
int sis_ai_nearest_drift_size(s_ai_nearest_drift *in_)
{
    return in_->ins->count;
}

int sis_ai_nearest_drift_calc_future(s_ai_nearest_drift *in_, double inrate_, double min_, double max_)
{
    in_->inrate = inrate_;
    in_->rate = inrate_;
    in_->offset = in_->ins->count - 1;
    return sis_ai_nearest_drift_future(in_->ins->count, (double *)in_->ins->buffer, min_, max_, &in_->rate, &in_->offset, &in_->drift);
}

int sis_ai_nearest_drift_calc_formerly(s_ai_nearest_drift *in_, double inrate_, double min_, double max_)
{
    in_->inrate = inrate_;
    in_->rate = inrate_;
    in_->offset = 0;
    return sis_ai_nearest_drift_formerly(in_->ins->count, (double *)in_->ins->buffer, min_, max_, &in_->rate, &in_->offset, &in_->drift);
}

double sis_ai_normalization_split_slope(int nums_, double ins_[], double min_, double max_, double mid_)
{
    if (nums_ < 3)
    {
        return 0.0;
    }
    // 求斜率
    double *x = (double *)sis_calloc(sizeof(double) * nums_);
    for (int m = 0; m < nums_; m++)
    {
        x[m] = m;
    }

    double xs[3];
    double *outs = (double *)sis_calloc(sizeof(double) * nums_);

    sis_ai_normalization_split_array(nums_, ins_, outs, min_, max_, mid_);

    sis_ai_polyfit(nums_, x, outs, 1, xs);

    sis_free(x);
    sis_free(outs);

    return xs[1];   
}
// 返回归一化的斜率 rate -> 按斜率计算的涨跌幅 和实际涨跌可能不一致，
double sis_ai_normalization_series_slope(int nums_, double ins_[], double min_, double max_, double *rate_)
{
    if (nums_ < 3)
    {
        return 0.0;
    }
    if (rate_)
    {
        *rate_ = 0.0;
    }
    // 求斜率
    double *x = (double *)sis_calloc(sizeof(double) * nums_);
    for (int m = 0; m < nums_; m++)
    {
        x[m] = m;
    }

    double xs[3];
    double *outs = (double *)sis_calloc(sizeof(double) * nums_);

    sis_ai_normalization_series_array(nums_, ins_, outs, min_, max_);

    sis_ai_polyfit(nums_, x, outs, 1, xs);

    sis_free(x);
    sis_free(outs);

    if (rate_ && (ins_[0] > 0.000001 || ins_[0] > -0.000001))
    {
        double c = (max_ - min_) * xs[0] / (SIS_AI_MAX - SIS_AI_MIN) + min_;
        // printf("%f %f %f -- %f \n", xs[0], xs[1], xs[2], c);
        *rate_ = (c - ins_[0]) / ins_[0]; 
    }

    return xs[1];
}
double sis_ai_get_slope(int nums_, double ins_[])
{
    return sis_ai_normalization_series_slope(nums_, ins_, 0.0, 0.0, NULL); 
}

int sis_ai_nearest_drift_future(int nums_, double ins_[], double min_, double max_, double *minrate_, int *stop_, double *drift_)
{
    if (nums_ < 3 || (ins_[0] < 0.00001 && ins_[0] > -0.00001))
    {
        return SIS_AI_DRIFT_MID;
    }
    double first = ins_[0];
    double minv = first;
    double maxv = first;

    int mini = -1;
    int maxi = -1;

    double minrate = *minrate_;
    // 先求最大值
    for (int m = 1; m < nums_; m++)
    {
        if (ins_[m] > maxv)
        {
            maxv = ins_[m];
            double rate = (maxv - first) / first;
            if (rate > minrate) // 有一个下降波动
            {
                maxi = m;
                // break;
            }
        }
        else
        {
            if (maxi >= 0)
            {
                break;
            }
        }
    }
    // 先求最小值
    for (int m = 1; m < nums_; m++)
    {
        if (ins_[m] < minv)
        {
            minv = ins_[m];
            double rate = (first - minv) / first;
            if (rate > minrate) // 有一个上涨波动
            {
                mini = m;
                // break;
            }
        }
        else
        {
            if (mini >= 0)
            {
                break;
            }
        }
    }
    *stop_ = nums_ - 1;
    int o = SIS_AI_DRIFT_MID;
    if (maxi < 0 && mini < 0)  // 无波动
    {
        *stop_ = nums_ - 1;
        // 这里是否要求波动斜率
    }
    else 
    {
        if (maxi > mini)   // 下降
        {
            *stop_ = mini < 0 ? maxi : mini;
            o = SIS_AI_DRIFT_DN;
        }
        if (maxi < mini)   // 上升
        {
            *stop_ = maxi < 0 ? mini : maxi;
            o = SIS_AI_DRIFT_UP;
        }
    }
    // 求斜率
    int count = *stop_ + 1;
    // printf("%d : %d %d %d \n", count, *stop_, mini, maxi);
    double *x = (double *)sis_calloc(sizeof(double) * count);
    for (int m = 0; m < count; m++)
    {
        x[m] = m;
    }

    double xs[3];
    double *outs = (double *)sis_calloc(sizeof(double) * count);

    sis_ai_normalization_series_array(count, ins_, outs, min_, max_);
    // for (size_t i = 0; i < count; i++)
    // {
    //     printf("%.4f - %.4f | ", ins_[i], outs[i]);
    // }
    sis_ai_polyfit(count, x, outs, 1, xs);
    // printf("---- %f %f %f\n", xs[0], xs[1], xs[2]);

    sis_free(x);
    sis_free(outs);

    if (*stop_ == nums_ - 1)
    {
        double c = (max_ - min_) * xs[0] / (SIS_AI_MAX - SIS_AI_MIN) + min_;
        // printf("%f %f %f -- %f \n", xs[0], xs[1], xs[2], c);
        *minrate_ = (c - first) / first; 
    }
    else
    {
        *minrate_ = (ins_[*stop_] -  first) / first; 
    }
    *drift_ = xs[1];
    return o;
}
int sis_ai_nearest_drift_formerly(int nums_, double ins_[], double min_, double max_, double *minrate_, int *start_, double *drift_)
{
    if (nums_ < 3 || (ins_[nums_ - 1] < 0.00001 && ins_[nums_ - 1] > -0.00001))
    {
        return SIS_AI_DRIFT_MID;
    }

    double last = ins_[nums_ - 1];
    double minv = last;
    double maxv = last;

    int mini = -1;
    int maxi = -1;

    double minrate = *minrate_;
    // 先求最大值
    for (int m = nums_ - 2; m >=0; m--)
    {
        if (ins_[m] > maxv)
        {
            maxv = ins_[m];
            double rate = (maxv - last) / last;
            if (rate > minrate) // 有一个下降波动
            {
                maxi = m;
                // break;
            }
        }
        else
        {
            if (maxi >= 0)
            {
                break;
            }
        }
    }
    // 先求最小值
    for (int m = nums_ - 2; m >=0; m--)
    {
        if (ins_[m] < minv)
        {
            minv = ins_[m];
            double rate = (last - minv) / last;
            if (rate > minrate) // 有一个上涨波动
            {
                mini = m;
                // break;
            }
        }
        else
        {
            if (mini >= 0)
            {
                break;
            }
        }
    }
    int o = SIS_AI_DRIFT_MID;
    *start_ = 0;
    if (maxi < 0 && mini < 0)  // 无波动
    {
        *start_ = 0;
        // 这里是否要求波动斜率
    }
    else 
    {
        if (maxi > mini)  // 下降
        {
            *start_ = maxi;
            o = SIS_AI_DRIFT_DN;
        }
        if (maxi < mini) // 上升
        {
            *start_ = mini;
            o = SIS_AI_DRIFT_UP;
        }
    }

    // 求斜率
    int count = nums_ - *start_;
    double *x = (double *)sis_calloc(sizeof(double) * count);
    for (int m = 0; m < count; m++)
    {
        x[m] = m;
    }

    double xs[3];
    double *outs = (double *)sis_calloc(sizeof(double) * count);

    sis_ai_normalization_series_array(count, &ins_[*start_], outs, min_, max_);
    // for (size_t i = 0; i < count; i++)
    // {
    //     printf("%.4f - %.4f | ", ins_[*start_ + i], outs[i]);
    // }
    // printf("\n %.4f - %.4f \n", min_, max_);
    sis_ai_polyfit(count, x, outs, 1, xs);
    // printf("%f %f %f\n", xs[0], xs[1], xs[2]);

    // sis_ai_normalization_series_array(count, &ins_[*start_], outs, 0, 0);
    // for (size_t i = 0; i < count; i++)
    // {
    //     printf("%.4f  ", outs[i]);
    // }
    // printf("\n");
    // sis_ai_polyfit(count, x, outs, 1, xs);
    // printf("%f %f %f\n", xs[0], xs[1], xs[2]);

    // sis_ai_polyfit(count, x, &ins_[*start_], 1, xs);
    // printf("%f %f %f\n", xs[0], xs[1], xs[2]);

    sis_free(x);
    sis_free(outs);

    if (*start_ == 0)
    {
        double c = (max_ - min_) * xs[0] / (SIS_AI_MAX - SIS_AI_MIN) + min_;
        // printf("%f %f %f -- %f \n", xs[0], xs[1], xs[2], c);
        *minrate_ = (last - c) / last; 
    }
    else
    {
        *minrate_ = (last - ins_[*start_]) / last; 
    }
    *drift_ = xs[1];
    return o;
}
// 返回实际的最大 diff
double sis_ai_nearest_diff_formerly(int nums_, double ins_[])
{
    if (nums_ < 2)
    {
        return 0.0;
    }

    double last = ins_[nums_ - 1];
    double minv = last;
    double maxv = last;

    int mini = -1;
    int maxi = -1;

    // 先求最大值
    for (int m = nums_ - 1; m >=0; m--)
    {
        if (ins_[m] > maxv)
        {
            maxv = ins_[m];
            maxi = m;
        }
        else
        {
            if (maxi >= 0)
            {
                break;
            }
        }
    }
    // 先求最小值
    for (int m = nums_ - 1; m >=0; m--)
    {
        if (ins_[m] < minv)
        {
            minv = ins_[m];
            mini = m;
        }
        else
        {
            if (mini >= 0)
            {
                break;
            }
        }
    }
    // printf("\n %d - %d : %.4f - %.4f = %.4f \n", maxi, mini, maxv, minv, last);
    if (maxi > mini)  // 下降
    {
        return last - maxv;
    }
    if (maxi < mini) // 上升
    {
    // printf("\n %d - %d : %.4f - %.4f = %.4f \n", maxi, mini, maxv, minv, last);
        return last - minv;
    }
    return 0.0;
}

double sis_ai_drift_series(int nums_, double ins_[], double min_, double max_)
{
    if (nums_ < 3 || (ins_[nums_ - 1] < 0.00001 && ins_[nums_ - 1] > -0.00001))
    {
        return 0.0;
    }
    double minv = min_;
    double maxv = max_;

    for (int m = nums_ - 1; m >=0; m--)
    {
        minv = sis_min(minv, ins_[m]);
        maxv = sis_min(maxv, ins_[m]);
    }
    double *x = (double *)sis_calloc(sizeof(double) * nums_);
    for (int m = 0; m < nums_; m++)
    {
        x[m] = m;
    }

    double xs[3];
    double *outs = (double *)sis_calloc(sizeof(double) * nums_);
    sis_ai_normalization_series_array(nums_, ins_, outs, minv, maxv);

    sis_ai_polyfit(nums_, x, outs, 1, xs);
    // printf("%f %f %f\n", xs[0], xs[1], xs[2]);

    sis_free(x);
    sis_free(outs);

    return xs[1];
}
double sis_ai_drift_split(int nums_, double ins_[], double min_, double max_, double mid_)
{
    if (nums_ < 3 || (ins_[nums_ - 1] < 0.00001 && ins_[nums_ - 1] > -0.00001))
    {
        return 0.0;
    }
    double minv = min_;
    double maxv = max_;

    for (int m = nums_ - 1; m >=0; m--)
    {
        minv = sis_min(minv, ins_[m]);
        maxv = sis_min(maxv, ins_[m]);
    }
    double *x = (double *)sis_calloc(sizeof(double) * nums_);
    for (int m = 0; m < nums_; m++)
    {
        x[m] = m;
    }

    double xs[3];
    double *outs = (double *)sis_calloc(sizeof(double) * nums_);
    sis_ai_normalization_split_array(nums_, ins_, outs, minv, maxv, mid_);

    sis_ai_polyfit(nums_, x, outs, 1, xs);
    // printf("%f %f %f\n", xs[0], xs[1], xs[2]);

    sis_free(x);
    sis_free(outs);

    return xs[1];
}
//////////////////////////////
//
//////////////////////////////

void gauss_solve(int n, double A[], double x[], double b[])
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
/*==================sis_ai_polyfit(n,x,y,poly_n,a)===================*/
/*=======拟合y=a0+a1*x+a2*x^2+……+apoly_n*x^poly_n========*/
/*=====n是数据个数 xy是数据值 poly_n是多项式的项数======*/
/*===返回a0,a1,a2,……a[poly_n]，系数比项数多一（常数项）=====*/
void sis_ai_polyfit(int n, double x[], double y[], int poly_n, double a[])
{
	int i, j;
	double *tempx, *tempy, *sumxx, *sumxy, *ata;
	//void gauss_solve(int n, double A[], double x[], double b[]);
	tempx = (double*)sis_calloc(n * sizeof(double));
	sumxx = (double*)sis_calloc((poly_n * 2 + 1) * sizeof(double));
	tempy = (double*)sis_calloc(n * sizeof(double));
	sumxy = (double*)sis_calloc((poly_n + 1) * sizeof(double));
	ata = (double*)sis_calloc(((poly_n + 1)*(poly_n + 1)) * sizeof(double));
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
	sis_free(tempx);
	sis_free(sumxx);
	sis_free(tempy);
	sis_free(sumxy);
	sis_free(ata);
}
double sis_ai_slope(int n, double ins[])
{
    double a[3];
    double *x = (double *)sis_calloc(sizeof(double) * n);
    for (int m = 0; m < n; m++)
    {
        x[m] = m + 1;
    }
    sis_ai_polyfit(n, x, ins, 1, a);
    sis_free(x);
    // printf("%f %f %f\n", a[0], a[1], a[2]);
    return a[1];
}
// -1 ... +1 之间
double sis_ai_slope_rate(int n, double ins[])
{
    if (n < 3) 
    {
        return  0.0;
    }
    double min = 0.0;
    double max = 0.0;

    double a[3];
    double *x = (double *)sis_calloc(sizeof(double) * n);
    for (int m = 0; m < n; m++)
    {
        x[m] = m;
        min = sis_min(min, ins[m]);
        max = sis_max(max, ins[m]);
        // printf("%f %f %f \n", ins[m], min , max);
    }
    if (max - min > -0.000001 && max - min < 0.000001)
    {
        sis_free(x);
        return 0.0;
    }
    double *y = (double *)sis_calloc(sizeof(double) * n);
    // 保证从 0 出发
    // printf("%f %f \n", min , max);
    for (int m = 0; m < n; m++)
    {
        // y[m] = (ins[m] - ins[0]) / max * m;
        y[m] = (ins[m] - min) / ((max - min) / (n - 1));
        // printf("%f %f %f\n", ins[m], x[m], y[m]);
    }    
    sis_ai_polyfit(n, x, y, 1, a);
    sis_free(x);
    sis_free(y);
    // printf("%f %f %f\n", a[0], a[1], a[2]);
    return a[1];
}
int8 sis_ai_factor_drift_three(double in1, double in2, double in3, double in4)
{
    int8 o = 0;
    if (in4 >= in3) 
    {
        o = o << 1;  o |= 1;
    }
    else
    {
        o = o << 1;
    }    
    if (in3 >= in2) 
    {
        o = o << 1;  o |= 1;
    }
    else
    {
        o = o << 1;
    } 
    if (in2 >= in1) 
    {
        o = o << 1;  o |= 1;
    }
    else
    {
        o = o << 1;
    } 
    return o;
}

int8 sis_ai_factor_drift(int n, double *ins, int level)
{
    if (n < 1) return 0;

    int8 o = 0;
    if (level > n - 1)
    {
        int count = 2 * n - 1;
        double *outs = (double *)sis_malloc(count * sizeof(double));
        for (int i = 0, k = 0; i < n; i++, k++)
        {
            outs[k] = ins[i];
            if (i < n - 1)
            {
                k++;
                outs[k] = (ins[i] + ins[i + 1]) / 2.0;
            }
        }
        o = sis_ai_factor_drift(count, outs, level);
        sis_free(outs);
    }
    else if (level < n - 1)
    {
        int count = n - 1;
        double *outs = (double *)sis_malloc(count *sizeof(double));
        for (int i = n - 1, k = count -1; i > 0; i--, k--)
        {
            outs[k] = (ins[i] + ins[i - 1]) / 2.0;
        }
        o = sis_ai_factor_drift(count, outs, level);
        sis_free(outs);
    } 
    else
    {
        o = 0;
        for (int i = n - 1; i > 0; i--)
        { 
            // printf(" %d: %.0f %.0f = %d\n", i, ins[i], ins[i - 1], o);
            if (ins[i] >= ins[i - 1])
            {
                o = o << 1;
                o |= 1;
            }
            else
            {
                o = o << 1;
                // o |= 0;
            }          
        }
    } 
    return o;
}

int8 sis_ai_factor_drift_pair(int n, double *asks, double *bids, int level)
{
    if (n < 1) return 0;

    int8 o = 0;
    if (level > n)
    {
        int count = 2 * n - 1;
        double *oasks = (double *)sis_malloc(count *sizeof(double));
        double *obids = (double *)sis_malloc(count *sizeof(double));
        for (int i = 0, k = 0; i < n; i++, k++)
        {
            oasks[k] = asks[i];
            if (i < n - 1)
            {
                k++;
                oasks[k] = (asks[i] + asks[i + 1]) / 2.0;
            }
        }
        for (int i = 0, k = 0; i < n; i++, k++)
        {
            obids[k] = bids[i];
            if (i < n - 1)
            {
                k++;
                obids[k] = (bids[i] + bids[i + 1]) / 2.0;
            }
        }
        o = sis_ai_factor_drift_pair(count, oasks, obids, level);
        sis_free(oasks);
        sis_free(obids);
    }
    else if (level < n)
    {
        int count = n - 1;
        double *oasks = (double *)sis_malloc(count *sizeof(double));
        double *obids = (double *)sis_malloc(count *sizeof(double));
        for (int i = n - 1, k = count - 1; i > 0; i--, k--)
        {
            oasks[k] = (asks[i] + asks[i - 1]) / 2.0;
        }
        for (int i = n - 1, k = count - 1; i > 0; i--, k--)
        {
            obids[k] = (bids[i] + bids[i - 1]) / 2.0;
        }
        o = sis_ai_factor_drift_pair(count, oasks, obids, level);
        sis_free(oasks);
        sis_free(obids);
    } 
    else
    {
        o = 0;
        for (int i = n - 1; i > 0; i--)
        { 
            // printf(" %d: %.0f %.0f = %d\n", i, ins[i], ins[i - 1], o);
            if (asks[i] >= bids[i])
            {
                o = o << 1;
                o |= 1;
            }
            else
            {
                o = o << 1;
                // o |= 0;
            }          
        }
    } 
    return o;    
}

void   sis_ai_series_argv(int n, double ins[], double *avg, double *vari)
{
    *avg = 0.0;
    *vari = 0.0;
    if (n < 1)
    {
        return ;
    }
    double sum = 0.0;
    for (int i = 0; i < n; i++)
    {
        sum += ins[i];
    }
    *avg = sum / (double)n;
    sum = 0.0;
    for (int i = 0; i < n; i++)
    {
        sum += pow(ins[i] - *avg, 2);
    }    
    *vari = sqrt(sum / (n-1));
}

double sis_ai_series_chance(double in, double avg, double vari)
{
    if (vari == 0)
    {
        return 0.0;
    }
    return exp(-1*pow(in - avg, 2)/(2*pow(vari,2))) / (sqrt(2*SIS_AI_CONST_PAI)*vari);
}
// 16 个数最大
static  double __fibonacci[] = {
    // 0.3, 0.477, 0.6, 0.7, 0.778, 0.845, 0.9, 0.954, 1.0
    1, 1, 2, 3, 5, 8, 13, 21, 34, 55
    // , 89, 144, 233, 377, 610, 987
};
double _get_calc_fibonacci(int index_, int count_)
{
    int index = ((float)index_ / (float)count_ * 10.0);
    return __fibonacci[index];
}
double sis_ai_fibonacci_avg(int n, double ins[])
{
    if ( n < 1 )
    {
        return 0.0;
    }
    double denominator = 0.0;
    double avg = 0.0;
    for (int i = 0; i < n; i++)
    {
        double factor = _get_calc_fibonacci(i, n);
        denominator += factor;
        avg += ins[i] * factor;
    }
    avg /= denominator; 
    return avg;
}

/////////////////////////////////////////////////////
//  下面是通用的求组合结果的类
////////////////////////////////////////////////////

// 求速度 
s_sis_calc_cycle *sis_calc_cycle_create(int style_)
{
    s_sis_calc_cycle *calc = (s_sis_calc_cycle *)sis_malloc(sizeof(s_sis_calc_cycle));
    calc->style = style_;
    calc->list = sis_struct_list_create(sizeof(double));
    return calc;
}
void sis_calc_cycle_destroy(s_sis_calc_cycle *calc_)
{
    sis_struct_list_destroy(calc_->list);
    sis_free(calc_);
}
int sis_calc_cycle_push(s_sis_calc_cycle *calc_, double value_)
{
    sis_struct_list_push(calc_->list, &value_);
    return calc_->list->count;
}
void sis_calc_cycle_clear(s_sis_calc_cycle *calc_)
{
    sis_struct_list_clear(calc_->list);
}

double *sis_calc_cycle_get(s_sis_calc_cycle *calc_)
{
    return (double *)sis_struct_list_get(calc_->list, 0);
}

int sis_calc_cycle_set_float(s_sis_calc_cycle *calc_, float *value_, int count_)
{  
    sis_struct_list_clear(calc_->list);
    if(calc_->style == JUDGE_STYLE_TREND)
    {
        int o = TREND_DIR_NONE;
        if (count_ < 2)
        {
            return o;
        }
        for(int i = 0; i < count_; i++)
        {
            double val = (double)value_[i];
            sis_struct_list_push(calc_->list, &val);
        }
        return sis_calc_cycle_out(calc_);
    }
    else
    {
        int o = SPEED_NONE_LESS;
        if (count_ < 3)
        {
            return o;
        }
        for(int i = 0; i < count_; i++)
        {
            double val = (double)value_[i];
            sis_struct_list_push(calc_->list, &val);
        }
        return sis_calc_cycle_out(calc_);
    }
    
}
///////////////////////////////////////////////////
// 计算速度时，碰到符号不同即停止计算
// 用第二个减去第一个，生成新的数列，加起来求均值，如果第一个差值大于均值表示速度越来越快，否则慢
// 求速度最少3个值，少于3个无法判断，通常利用连续的周期数据
//////////////////////////
// 求趋势，最少2个数值
// 第一个值大于第二个值，依次类推，如果都是这样，表示趋势形成
// 通常用菲数列周期来输入数据
/////////////////////////
int sis_calc_cycle_out(s_sis_calc_cycle *calc_)
{
    int o = JUDGE_STYLE_NONE;
    switch (calc_->style)
    {
        case JUDGE_STYLE_TREND:
            {
                o = TREND_DIR_NONE;
                int ups = 1, dns = 1;
                double ago = *(double *)sis_struct_list_get(calc_->list, 0);
                for(int i = 1; i < calc_->list->count; i++)
                {      
                    double val = *(double *)sis_struct_list_get(calc_->list, i);              
                    if (ago > val)
                    {
                        ups++;
                    }
                    if (ago < val)
                    {
                        dns++;
                    }
                    if(ups >= calc_->list->count)
                    {
                        o = TREND_DIR_INCR;
                        break;
                    }
                    if(dns >= calc_->list->count)
                    {
                        o = TREND_DIR_DECR;
                        break;
                    }
                    ago = val;
                }
            }
            break;
        case JUDGE_STYLE_SPEED:
            {
                o = SPEED_NONE_LESS;
                bool isup = false, isdn = false;
                int cycle = 0;

                for(int i = 0; i < calc_->list->count; i++)
                {
                    double val = *(double *)sis_struct_list_get(calc_->list, i);
                    if (!isup&&!isdn)
                    {
                        if (val  > 0.01)
                        {
                            isup = true;
                        }
                        if (val  < -0.01)
                        {
                            isdn = true;
                        } 
                        cycle++;
                        continue;
                    } 
                    if (isup&&val  < 0.01)
                    {
                        break;
                    }
                    if (isdn&&val  > -0.01)
                    {
                        break;
                    }
                    cycle++;
                }
                // cycle 为实际周期
                if(cycle < 3) 
                {
                    break;
                }
                double avg = 0.0;
                double first = *(double *)sis_struct_list_get(calc_->list, 0);
                double ago = first;
                for(int i = 1; i < cycle; i++)
                {      
                    double val = *(double *)sis_struct_list_get(calc_->list, i); 
                    if(i==1)
                    {
                        first = val - ago;
                    }
                    avg += val - ago;
                    // printf(" [%d:%d]-- %.2f, += %.2f - %.2f\n",cycle, i, avg, val, ago);
                    ago = val;
                }
                avg = avg / (cycle - 1);
                if (isup)
                {
                    // printf(" == %.2f,%.2f\n",avg, first);
                    if (avg > first)
                    {
                        o = SPEED_INCR_SLOW;
                    }
                    else 
                    {
                        o = SPEED_INCR_FAST;
                    }
                }
                if (isdn)
                {
                    if (avg < first)
                    {
                        o = SPEED_DECR_SLOW;
                    }
                    else 
                    {
                        o = SPEED_DECR_FAST;
                    }
                }
            }
            break;
        default:
            break;
    }
    
    return o;
}

#if 0
int main()
{
    int count = 7;
    // double in[7] = { 5.0,6.0,8.0,12.0,20.0};
    // double in[7] = { 5.0,10.0,14.0,12.0,11.0}; //越涨越快

    // double in[7] = { -5.0,-6.0,-8.0,-12.0,-20.0};
    float in[7] = { -5.0,-10.0,-14.0,-12.0,-11.0}; //越跌越快

    s_sis_calc_cycle *cycle = sis_calc_cycle_create(JUDGE_STYLE_SPEED);
    
    sis_calc_cycle_set_float(cycle,in, count);
    // for(int i = 0; i < count; i++)
    // {
    //     sis_calc_cycle_push(cycle, in[i]);
    // }
    int out = sis_calc_cycle_out(cycle);
    printf("out: %d\n", out);
    sis_calc_cycle_destroy(cycle);
 
    return 0;
}

#endif
#if 0
int main()
{
    // double in1[8] = { 1, 2, 3, 4, 5, 6, 7, 8};
    double in1[8] = { 2, 4, 6, 8, 10, 12, 14, 16};
    double in2[8] = { 3, 4, 5, 6, 7, 8, 9, 10};
    double in3[8] = { 1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008};
    double in4[8] = { 1001, 1010, 1030, 1050, 1060, 1075, 1090, 1100};
    // double in3[8] = { 10, 20, 30, 40, 50, 60, 70, 80};
    // double in4[8] = { 30, 50, 70, 90, 110, 130, 150, 170};
    // double in3[8] = { 10, 9, 8, 7, 6, 4, 4, 3};
 
    // double in1[5] = { 312, 638, 338, 182, 1209};
    // double in2[5] = { 702, 1158, 1131, 27559, 1426};

    // double in3[5] = { 65727, 68336, 68383, 67642, 50425};
    // double in4[5] = { 50914, 48605, 50597, 51816, 60696};

    int count = 8;
    // for (int i = 0; i < count; i++)
    // {
    //     in4[i] = -1 *in4[i];
    // }
    printf("slope: %.5f\n", sis_ai_slope_rate(count, in1));
    printf("slope: %.5f\n", sis_ai_slope_rate(count, in2));
    printf("slope: %.5f\n", sis_ai_slope_rate(count, in3));
    printf("slope: %.5f\n", sis_ai_slope_rate(count, in4));

    return 0;
}
#endif
#if 0
int main()
{
    double good[8] = { 0.697, 0.774, 0.634, 0.608, 0.556, 0.403, 0.481, 0.437};
    double error[9] = { 0.666, 0.243, 0.245, 0.343, 0.639, 0.657, 0.360, 0.593, 0.719};
    
    double avg;
    double vari;
    double value;

    int count = 8;
    sis_ai_series_argv(count, good, &avg, &vari);
    printf("para: %.5f %.5f\n", avg, vari);
 
    value = sis_ai_series_chance(good[0], avg, vari);
    printf("out: %.5f\n", value);

    count = 9;
    sis_ai_series_argv(count, error, &avg, &vari);
    printf("para: %.5f %.5f\n", avg, vari);
 
    value = sis_ai_series_chance(good[0], avg, vari);
    printf("out: %.5f\n", value);

    s_sis_calc_cycle *list = sis_calc_cycle_create(0);    
    for (int i = 0; i < 8; i++)
    {
        count = sis_calc_cycle_push(list, good[i]*1000); 
    }
    sis_ai_series_argv(count, sis_calc_cycle_get(list), &avg, &vari);
    printf("para: %.5f %.5f %.5f\n", avg, vari, 
        sis_ai_series_chance( 1697 ,avg, vari));

    sis_calc_cycle_destroy(list);
    return 0;
}

#endif

#if 0
int main()
{
    double ins[8] = { 1, 2, 3, 4, 5, 6, 7, 8};    
    // printf(" %d \n", sis_ai_factor_drift(8, ins, 3));

    return 0;
}
#endif

#if 0
int main()
{
    // double ins[10] = { 100.50, 99.95, 100.10, 100.20, 100.35, 100.45, 100.75, 100.85, 100.98, 101};    
    // double ins[10] = { 101.0, 101.50, 102.55, 99.30, 100.35, 100.85, 101.5, 101.85, 100.98, 101};    
    // double ins[10] = { 101.0, 101.50, 101.55, 101.30, 100.35, 100.85, 101.5, 101.85, 101.98, 101}; 
    double ins[10] = { 101.95, 101.80, 100.10, 99.60, 101.5, 104.4, 101.3, 101.2, 101.1, 101};    
    double max = 110.0;
    double min = 90.0;

    double rate = 0.01;
    // int start = 0;

    // double drift = sis_ai_nearest_drift_formerly(10, ins, 90.0, 110.0, &rate, &start);
    // printf(" drift = %f , rate = %f, start = %d \n", drift, rate, start);

    // rate = 0.01;
    // drift = sis_ai_nearest_drift_future(10, ins, min, max, &rate, &start);
    // printf(" drift = %f , rate = %f, start = %d \n", drift, rate, start);

    // 测试类
    s_ai_nearest_drift *cls = sis_ai_nearest_drift_create();
    for (int m = 0; m < 10; m++)
    {
        sis_ai_nearest_drift_push(cls, ins[m]);
    }
    sis_ai_nearest_drift_calc_formerly(cls, rate, min, max);
    printf(" drift = %f , rate = %f, start = %d \n", cls->drift, cls->rate, cls->offset);
    sis_ai_nearest_drift_calc_future(cls, rate, min, max);
    printf(" drift = %f , rate = %f, start = %d \n", cls->drift, cls->rate, cls->offset);
    sis_ai_nearest_drift_destroy(cls);
    return 0;
}
#endif

#if 0
int main()
{
    // 10 元股票为例 0.1 为 1%
    double ins[10] = { 0.21, 0.20, 0.15, 0.18, 0.1, -0.1, -0.12, -0.05, 0.021, 0.053};    
    double diff = sis_ai_nearest_diff_formerly(6, ins);
    printf(" diff = %f \n", diff);

    // rate = 0.01;
    // drift = sis_ai_nearest_drift_future(10, ins, min, max, &rate, &start);
    // printf(" drift = %f , rate = %f, start = %d \n", drift, rate, start);

    // 测试类
    // s_ai_nearest_drift *cls = sis_ai_nearest_drift_create();
    // for (int m = 0; m < 10; m++)
    // {
    //     sis_ai_nearest_drift_push(cls, ins[m]);
    // }
    // sis_ai_nearest_drift_calc_formerly(cls, rate, min, max);
    // printf(" drift = %f , rate = %f, start = %d \n", cls->drift, cls->rate, cls->offset);
    // sis_ai_nearest_drift_calc_future(cls, rate, min, max);
    // printf(" drift = %f , rate = %f, start = %d \n", cls->drift, cls->rate, cls->offset);
    // sis_ai_nearest_drift_destroy(cls);
    return 0;
}
#endif

#if 0

int main()
{
    // double ins[] = { 11.00, 10.5, 10.1, 9.8, 9.6, 9.5, 9.45, 9.55, 9.75, 10.1, 10.5};    // 越跌越慢
    // double ins[] = { 11.00, 10.95, 10.85, 10.6, 10.3, 9.9, 9.4, 9.5, 9.7, 10.0, 10.5, 11.0, 12.0};   // 越跌越快
    // double ins[] = { 8.5, 8.6, 8.8, 9.1, 9.5, 10.0, 10.6, 11.3, 11.2, 11.0, 10.7, 10.3, 9.8};   // 越涨越快
    double ins[] = { 8.5, 9.1, 9.6, 10.0, 10.3, 10.5, 10.6, 11.3, 11.2, 11.0, 10.7, 10.3, 9.8};   // 越涨越慢

    double ins1[sizeof(ins)/sizeof(double)];
    for (int i = 0; i < sizeof(ins)/sizeof(double); i++)
    {
        
        double rate = sis_ai_series_drift(i + 1, ins);
        double rate1 = sis_ai_series_acceleration(i + 1, ins);
        printf(" [%d] rate = %f %f\n", i, rate, rate1);
    }
    for (int i = 0; i < sizeof(ins)/sizeof(double); i++)
    {
        ins1[i] = ins[i] * 100;
    }
    for (int i = 0; i < sizeof(ins)/sizeof(double); i++)
    {
        double rate = sis_ai_series_drift(i + 1, ins1);
        double rate1 = sis_ai_series_acceleration(i + 1, ins);
        printf(" [%d] rate = %f %f\n", i, rate, rate1);

    }
    for (int i = 0; i < sizeof(ins)/sizeof(double); i++)
    {
        ins1[i] = ins[i] + 1000;
    }
    for (int i = 0; i < sizeof(ins)/sizeof(double); i++)
    {
        double rate = sis_ai_series_drift(i + 1, ins1);
        double rate1 = sis_ai_series_acceleration(i + 1, ins);
        printf(" [%d] rate = %f %f\n", i, rate, rate1);

    }
    return 0;
}
#endif
#if 0
int main()
{
    // 10 元股票为例 0.1 为 1%
    // double ins[10] = { 10.11, 10.12, 10.25, 10.55, 10.99, 10.15, 10.25, 10.35, 10.65, 10.95};   
    // double ins[10] = { 9.0, 9.05, 9.15, 9.35, 10.6, 10.9, 10.5, 10.35, 10.65, 10.95};   
    // double ins[10] = { 11.00, 10.5, 10.1, 9.8, 9.6, 9.5, 9.45, 9.6, 9.8, 10.2};   
    // double ins[10] = { 11.00, 10.9, 10.7, 10.4, 10.0, 9.5, 8.9, 8.2, 8,3, 8.5};   

    double x1[10] = { 1,2,3,4,5,6,7,8,9, 10};   
    double x2[10] = { 1,2,3,5,7,9,12,15,18,21};   // 1走2 2走3 3走4
    // double x2[10] = { 1,2,3,4,7,11,16,22,29,37};   
    double y1[10] = { 10.1, 10.2, 10.4, 10.7, 11.1, 11.6, 11.4, 11.3, 12.5, 12.8 };   

    // double ins10[10];
    // for (int i = 0; i < 10; i++)
    // {
    //     ins10[i] = ins[i] * 10;
    // }
    double xs[3];

    for (int i = 3; i < 10; i++)
    {
        sis_ai_polyfit(i + 1, x1, y1, 2, xs);
        printf("[%d] a: %7.3f b: %7.3f : %7.3f %7.3f ",i, xs[2], xs[1], xs[2]*x1[i]*x1[i]+xs[1]*x1[i]+xs[0], 2*xs[2]*x1[i]+xs[1]);
        sis_ai_polyfit(i + 1, x2, y1, 2, xs);
        printf("| a: %7.3f b: %7.3f : %7.3f %7.3f \n",xs[2], xs[1], xs[2]*x2[i]*x2[i]+xs[1]*x2[i]+xs[0], 2*xs[2]*x2[i]+xs[1]);
    }


    // for (int i = 0; i < 10; i++)
    // {
    //     // double rate10 = sis_ai_normalization_series_acceleration(i + 1, ins10, 90.0, 110.0);
    //     double rate = sis_ai_normalization_series_acceleration(i + 1, ins, 8.0, 11.0);
    // //     double rate1;
    // //     double slope = sis_ai_normalization_series_slope(i + 1, ins, 9.0, 11.0, &rate1);
    // //     printf(" [%d] rate = %f %f | %f\n", i, rate,slope, rate1);
    // }
    return 0;
}
#endif

#if 0
int main()
{
    int count = 4;
    s_sis_struct_list *list = sis_struct_list_create(count*sizeof(int));
    sis_cut_ratio_int(list, count, 3);
    for (int i = 0; i < list->count; i++)
    {
        int *vals = (int *)sis_struct_list_get(list, i);
        printf("[%d:%d] ",list->count, i);
        for (int j = 0; j < count; j++)
        {
            printf("%d ", vals[j]);
        }
        printf("\n");       
    }
}
#endif
#if 0
int main()
{
    double ins[3] = { 5.00, 3.00, 9.00 };
    int count = 3;
    s_sis_struct_list *list = sis_struct_list_create(count*sizeof(double));
    sis_cut_ratio_random(list, count, ins, 5, 0.1, 10.0);
    for (int i = 0; i < list->count; i++)
    {
        double *vals = (double *)sis_struct_list_get(list, i);
        printf("[%d:%d] ",list->count, i);
        for (int j = 0; j < count; j++)
        {
            printf("%.2f ", vals[j]);
        }
        printf("\n");       
    }
}
#endif
