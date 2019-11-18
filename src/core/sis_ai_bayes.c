
#include <sis_ai_bayes.h>
#include <sis_ai.h>

s_ai_bayes_unit *sis_ai_bayes_unit_create(const char *name_)
{
    s_ai_bayes_unit *o = sis_malloc(sizeof(s_ai_bayes_unit));
    memset(o, 0, sizeof(s_ai_bayes_unit));
    sis_strcpy(o->name, 32, name_);
    o->samples = sis_struct_list_create(sizeof(s_ai_bayes_input));
    o->classify = sis_struct_list_create(sizeof(s_ai_bayes_value));
    return o;
}
 
void sis_ai_bayes_unit_destroy(s_ai_bayes_unit *unit_)
{
    sis_struct_list_destroy(unit_->samples);
    sis_struct_list_destroy(unit_->classify);   
    sis_free(unit_);
}

int sis_ai_bayes_unit_push_factor(s_ai_bayes_unit *unit_, int8 value_)
{
    s_ai_bayes_input in;
    memset(&in, 0, sizeof(s_ai_bayes_input));
    in.style = AI_BAYES_TYPE_FACTOR;
    in.class = value_;
    sis_struct_list_push(unit_->samples, &in);

    s_ai_bayes_value *finder = NULL;
    for (int i = 0; i < unit_->classify->count; i++)
    {
        s_ai_bayes_value *val = (s_ai_bayes_value *)sis_struct_list_get(unit_->classify, i);
        if (val->class == in.class)
        {
            finder =  val;
            break;
        }
    }
    if (finder)
    {
        finder->count++;
    }
    else
    {
        s_ai_bayes_value newer;
        newer.class = in.class;
        newer.count = 1;
        sis_struct_list_push(unit_->classify, &newer);
    }
    return 0;    
}
int sis_ai_bayes_unit_push_series(s_ai_bayes_unit *unit_, double value_)
{
    s_ai_bayes_input in;
    memset(&in, 0, sizeof(s_ai_bayes_input));
    in.style = AI_BAYES_TYPE_SERIES;
    in.series = value_;

    double sum = unit_->avg * unit_->samples->count + in.series;
    unit_->avg = sum / (double)(unit_->samples->count + 1);
    
    sis_struct_list_push(unit_->samples, &in);
    sum = 0.0;
    if (unit_->samples->count > 1)
    {
        for (int i = 0; i < unit_->samples->count; i++)
        {
            s_ai_bayes_input *input = (s_ai_bayes_input *)sis_struct_list_get(unit_->samples, i);
            sum += pow(input->series - unit_->avg, 2);
        }
        unit_->vari = sqrt(sum / (unit_->samples->count - 1)); 
    }
    return unit_->samples->count;
}
int sis_ai_bayes_unit_push_random(s_ai_bayes_unit *unit_, double value_)
{
    s_ai_bayes_input in;
    memset(&in, 0, sizeof(s_ai_bayes_input));
    in.style = AI_BAYES_TYPE_RANDOM;
    in.series = value_;

    double sum = unit_->avg * unit_->samples->count + in.series;
    unit_->avg = sum / (double)(unit_->samples->count + 1);
    
    sis_struct_list_push(unit_->samples, &in);
    sum = 0.0;
    if (unit_->samples->count > 1)
    {
        for (int i = 0; i < unit_->samples->count; i++)
        {
            s_ai_bayes_input *input = (s_ai_bayes_input *)sis_struct_list_get(unit_->samples, i);
            sum += pow(input->series - unit_->avg, 2);
        }
        unit_->vari = sqrt(sum / (unit_->samples->count - 1)); 
    }
    return unit_->samples->count;
}

int sis_ai_bayes_unit_get_num(s_ai_bayes_unit *unit_,uint8  class_)
{
    for (int i = 0; i < unit_->classify->count; i++)
    {
        s_ai_bayes_value *val = (s_ai_bayes_value *)sis_struct_list_get(unit_->classify, i);
        if (val->class == class_)
        {
            return val->count;
        }
    }
    return 0;
}
////////////////////////////////
//
////////////////////////////////

s_ai_bayes_class *sis_ai_bayes_create()
{
    s_ai_bayes_class *o = sis_malloc(sizeof(s_ai_bayes_class));
    memset(o, 0, sizeof(s_ai_bayes_class));

    o->factor_add_lists = sis_map_pointer_create_v(sis_ai_bayes_unit_destroy);
    o->factor_mid_lists = sis_map_pointer_create_v(sis_ai_bayes_unit_destroy);
    o->factor_dec_lists = sis_map_pointer_create_v(sis_ai_bayes_unit_destroy);

    o->factor = sis_map_buffer_create();
    o->style_list = sis_map_buffer_create();
    return o;    
}
void sis_ai_bayes_destroy(s_ai_bayes_class *cls_)
{
    sis_map_pointer_destroy(cls_->factor_add_lists);
    sis_map_pointer_destroy(cls_->factor_mid_lists);
    sis_map_pointer_destroy(cls_->factor_dec_lists);

    sis_map_buffer_destroy(cls_->factor);
    sis_map_buffer_destroy(cls_->style_list);
    sis_free(cls_);
}

void sis_ai_bayes_study_init(s_ai_bayes_class *cls_)
{
    sis_map_pointer_clear(cls_->factor_add_lists);
    sis_map_pointer_clear(cls_->factor_mid_lists);
    sis_map_pointer_clear(cls_->factor_dec_lists);

    sis_map_buffer_clear(cls_->factor);
    sis_map_buffer_clear(cls_->style_list);
}
void _sis_ai_bayes_set_style(s_ai_bayes_class *cls_, const char *key_, uint8 style_)
{
    s_ai_bayes_style *style = (s_ai_bayes_style *)sis_map_buffer_get(cls_->style_list, key_);
    if (!style)
    {
        style = (s_ai_bayes_style *)sis_malloc(sizeof(s_ai_bayes_style));
        memset(style, 0, sizeof(s_ai_bayes_style));
        sis_strcpy(style->name, 32, key_);
        style->style = style_;
        sis_map_buffer_set(cls_->style_list, key_, style);
    }
    // printf("style count = %d\n",sis_map_buffer_getsize(cls_->style_list));
}

int sis_ai_bayes_study_push_factor(s_ai_bayes_class *cls_, 
	uint8 status_, const char *key_, 
	int8 value_)
{
    s_sis_map_pointer *map = NULL;
    switch (status_)
    {
    case AI_BAYES_ADD:
        map = cls_->factor_add_lists;
        break;   
    case AI_BAYES_DEC:
        map = cls_->factor_dec_lists;
        break;   
    case AI_BAYES_MID:
        map = cls_->factor_mid_lists;
    default:
        break;
    }
    if (!map)
    {
        return 1;
    }
    s_ai_bayes_unit *unit = (s_ai_bayes_unit *)sis_map_pointer_get(map, key_);
    if (!unit)
    {
        unit = sis_ai_bayes_unit_create(key_);
        sis_map_pointer_set(map, key_, unit);
    }
    sis_ai_bayes_unit_push_factor(unit, value_);
    _sis_ai_bayes_set_style(cls_, key_, AI_BAYES_TYPE_FACTOR);

    return 0;
}

int sis_ai_bayes_study_push_series(s_ai_bayes_class *cls_, 
	uint8 status_, const char *key_, 
	double value_)
{
    s_sis_map_pointer *map = NULL;
    switch (status_)
    {
    case AI_BAYES_ADD:
        map = cls_->factor_add_lists;
        break;   
    case AI_BAYES_DEC:
        map = cls_->factor_dec_lists;
        break;   
    case AI_BAYES_MID:
        map = cls_->factor_mid_lists;
    default:
        break;
    }
    if (!map)
    {
        return 1;
    }
    s_ai_bayes_unit *unit = (s_ai_bayes_unit *)sis_map_pointer_get(map, key_);
    if (!unit)
    {
        unit = sis_ai_bayes_unit_create(key_);
        sis_map_pointer_set(map, key_, unit);
    }
    sis_ai_bayes_unit_push_series(unit, value_);
    _sis_ai_bayes_set_style(cls_, key_, AI_BAYES_TYPE_SERIES);

    return 0;
}

int sis_ai_bayes_study_push_random(s_ai_bayes_class *cls_, 
	uint8 status_, const char *key_, 
	double value_)
{
    s_sis_map_pointer *map = NULL;
    switch (status_)
    {
    case AI_BAYES_ADD:
        map = cls_->factor_add_lists;
        break;   
    case AI_BAYES_DEC:
        map = cls_->factor_dec_lists;
        break;   
    case AI_BAYES_MID:
        map = cls_->factor_mid_lists;
    default:
        break;
    }
    if (!map)
    {
        return 1;
    }
    s_ai_bayes_unit *unit = (s_ai_bayes_unit *)sis_map_pointer_get(map, key_);
    if (!unit)
    {
        unit = sis_ai_bayes_unit_create(key_);
        sis_map_pointer_set(map, key_, unit);
    }
    sis_ai_bayes_unit_push_random(unit, value_);
    _sis_ai_bayes_set_style(cls_, key_, AI_BAYES_TYPE_RANDOM);

    return 0;
}
// 清理数据区 factor
void sis_ai_bayes_check_init(s_ai_bayes_class *cls_)
{
    sis_map_buffer_clear(cls_->factor);
}

// 写入 factor
void sis_ai_bayes_check_push_factor(s_ai_bayes_class *cls_, const char *key_, int8 value_)
{
    s_ai_bayes_input *in = SIS_MALLOC(s_ai_bayes_input, in);
    in->style = AI_BAYES_TYPE_FACTOR;
    in->class = value_;
    sis_map_buffer_set(cls_->factor, key_, in);
} 
void sis_ai_bayes_check_push_series(s_ai_bayes_class *cls_, const char *key_, double value_)
{
    s_ai_bayes_input *in = SIS_MALLOC(s_ai_bayes_input, in);
    in->style = AI_BAYES_TYPE_SERIES;
    in->series = value_;
    sis_map_buffer_set(cls_->factor, key_, in);
}
void sis_ai_bayes_check_push_random(s_ai_bayes_class *cls_, const char *key_, double value_)
{
    s_ai_bayes_input *in = SIS_MALLOC(s_ai_bayes_input, in);
    in->style = AI_BAYES_TYPE_RANDOM;
    in->series = value_;
    sis_map_buffer_set(cls_->factor, key_, in);
}
double _sis_ai_bayes_check_calc_factor(s_ai_bayes_unit *unit_, int8 value_)
{
    int nums = unit_->samples->count + 3;
    int son_nums = sis_ai_bayes_unit_get_num(unit_, value_) + 1;
    // printf(" %d / %d = %f\n",son_nums, nums, (double)son_nums / (double)nums);
    return (double)son_nums / (double)nums;
}
double _sis_ai_bayes_check_calc_series(s_ai_bayes_unit *unit_, s_ai_bayes_style *style_, double value_)
{
    int nums = unit_->samples->count + 3;
    int son_nums = 1;
    int class = AI_BAYES_DIV1;
    if (value_ > style_->serise_mid)
    {
        class = AI_BAYES_DIV2;
    }
    for (int i = 0; i < unit_->samples->count; i++)
    {
        s_ai_bayes_input *in = (s_ai_bayes_input *)sis_struct_list_get(unit_->samples, i);
        if (in->series > style_->serise_right)
        {
            if (class == AI_BAYES_DIV2) son_nums ++;
        }
        else
        {
            if (class == AI_BAYES_DIV1) son_nums ++;
        }   
    }
    return (double)son_nums / (double)nums;
}
double _sis_ai_bayes_check_calc_random(s_ai_bayes_unit *unit_, s_ai_bayes_style *style_, double value_)
{
    int nums = unit_->samples->count + 3;
    int son_nums = 1;
    int class = AI_BAYES_DIV1;
    if (value_ > style_->serise_right)
    {
        class = AI_BAYES_DIV3;
    }
    else if (value_ < style_->serise_left)
    {
        class = AI_BAYES_DIV1;
    }
    else
    {
        class = AI_BAYES_DIV2;
    }  
    for (int i = 0; i < unit_->samples->count; i++)
    {
        s_ai_bayes_input *in = (s_ai_bayes_input *)sis_struct_list_get(unit_->samples, i);
        if (in->series > style_->serise_right)
        {
            if (class == AI_BAYES_DIV3) son_nums ++;
        }
        else if (in->series < style_->serise_right)
        {
            if (class == AI_BAYES_DIV1) son_nums ++;
        }
        else
        {
            if (class == AI_BAYES_DIV2) son_nums ++;
        }   
    }
    return (double)son_nums / (double)nums;
}
double sis_ai_bayes_check_output(s_ai_bayes_class *cls_, uint8 status_)
{
    s_sis_map_pointer *map = NULL;
    switch (status_)
    {
    case AI_BAYES_ADD:
        map = cls_->factor_add_lists;
        break;   
    case AI_BAYES_DEC:
        map = cls_->factor_dec_lists;
        break;   
    case AI_BAYES_MID:
        map = cls_->factor_mid_lists;
    default:
        break;
    }
    if (!map)
    {
        return 1;
    }
    if (sis_map_pointer_getsize(map) < 1)
    {
        goto error;
    }
    double out = 0.0;
    s_sis_dict_entry *de;
    s_sis_dict_iter *di = sis_dict_get_iter(cls_->factor);
    while ((de = sis_dict_next(di)) != NULL)
    {
        s_ai_bayes_input *in = (s_ai_bayes_input *)sis_dict_getval(de);
        s_ai_bayes_unit *unit = (s_ai_bayes_unit *)sis_map_pointer_get(map, sis_dict_getkey(de));
        if (!unit)
        {
            sis_dict_iter_free(di); 
            goto error;
        }

        // s_ai_bayes_style *style = sis_ai_bayes_get_style(cls_, unit->name);
        // if (!style->used)
        // {
        //     // 排除掉影响小的特征
        //     continue;
        // }
        if (in->style == AI_BAYES_TYPE_FACTOR)
        {
            // printf("[%s]: ", unit->name);
            double val = _sis_ai_bayes_check_calc_factor(unit, in->class);
            if (val < -0.000001 || val > 0.000001) out += log(val);
            // printf("factor [%s]: %f\n", unit->name, out);
        }
        if (in->style == AI_BAYES_TYPE_SERIES)
        {
            // printf("[%s]: %f\n", unit->name, sis_ai_series_chance(in->series, unit->avg, unit->vari));
            double val = sis_ai_series_chance(in->series, unit->avg, unit->vari);
            // s_ai_bayes_style *style = sis_map_buffer_get(cls_->style_list, unit->name);
            // if (style)  
            // {
            //    out += log(_sis_ai_bayes_check_calc_series(unit, style, in->series)); 
            // } 
            if (val < -0.000001 || val > 0.000001) out += log(val);
            // printf("series [%s]: %f\n", unit->name, out);
        }
        if (in->style == AI_BAYES_TYPE_RANDOM)
        {
            double val = sis_ai_series_chance(in->series, unit->avg, unit->vari);
            // s_ai_bayes_style *style = sis_map_buffer_get(cls_->style_list, unit->name);
            // if (style)  
            // {
            //    out += log(_sis_ai_bayes_check_calc_random(unit, style, in->series)); 
            // }          
            if (val < -0.000001 || val > 0.000001) out += log(val);
            // printf("random [%s]: %f\n", unit->name, out);
        }
    }
    sis_dict_iter_free(di);     
    return out;
error:
    return 0.0;    
}
double _calc_bayes_gain(int count_, int n, int ins[])
{
    if (count_ < 1)
    {
        return 0.0;
    }
    double sum = 0.0;
    for (int i = 0; i < n; i++)
    {
        double out = (double)ins[i] / (double) count_;
        out = ins[i] ? out * log(out)/log(2) : 0.0;
        sum += out;
    }
    return -1 * sum;
}
double _calc_bayes_gain2(int count_, int a1_, int a2_)
{
    double a1 = (double)a1_ / (double) count_;
    double a2 = (double)a2_ / (double) count_;
    a1 = a1_ ? a1 * log(a1)/log(2) : 0.0;
    a2 = a2_ ? a2 * log(a2)/log(2) : 0.0;
    return -1 * (a1 + a2);
}
s_ai_bayes_style *sis_ai_bayes_get_style(s_ai_bayes_class *cls_, const char *key_)
{
    return sis_map_buffer_get(cls_->style_list, key_);
}

double sis_ai_bayes_chance_get_gain_factor(s_ai_bayes_class *cls_, const char *key_)
{
    s_ai_bayes_unit *add_unit = sis_map_pointer_get(cls_->factor_add_lists, key_);
    s_ai_bayes_unit *dec_unit = sis_map_pointer_get(cls_->factor_dec_lists, key_);

    // printf("name=%s  %p %p\n", key_, add_unit, dec_unit);

    if (!add_unit||!dec_unit)
    {
        return 0.0;
    }

    int adds = add_unit->samples->count;
    int decs = dec_unit->samples->count;
    int nums = adds + decs;
    double out = _calc_bayes_gain2(adds + decs, adds, decs);
    // printf("[%d %d %d] %f\n",adds, decs, nums, out);
    // 合并所有类
    s_sis_struct_list *gain_class = sis_struct_list_create(sizeof(s_ai_bayes_gain_factor));
    
    for (int i = 0; i < add_unit->classify->count; i++)
    {
        s_ai_bayes_value *val = (s_ai_bayes_value *)sis_struct_list_get(add_unit->classify, i);
        s_ai_bayes_gain_factor gain;
        memset(&gain, 0, sizeof(s_ai_bayes_gain_factor));
        gain.class = val->class;
        gain.adds = val->count;
        sis_struct_list_push(gain_class, &gain);
    }
    for (int i = 0; i < dec_unit->classify->count; i++)
    {
        s_ai_bayes_value *val = (s_ai_bayes_value *)sis_struct_list_get(dec_unit->classify, i);
        s_ai_bayes_gain_factor *gain_ptr = NULL;
        for (int k = 0; k < gain_class->count; k++)
        {
            s_ai_bayes_gain_factor *gain_val = (s_ai_bayes_gain_factor *)sis_struct_list_get(gain_class, k);
            if (gain_val->class == val->class)
            {
                gain_ptr = gain_val;
                gain_ptr->decs = val->count;
            }
        }
        if (!gain_ptr)
        {
            s_ai_bayes_gain_factor gain;
            memset(&gain, 0, sizeof(s_ai_bayes_gain_factor));
            gain.class = val->class;
            gain.decs = val->count;
            sis_struct_list_push(gain_class, &gain);
        }
    }
    // 处理所有数据
    double sum = 0.0;
    for (int k = 0; k < gain_class->count; k++)
    {
        s_ai_bayes_gain_factor *gain_val = (s_ai_bayes_gain_factor *)sis_struct_list_get(gain_class, k);
        int gains = gain_val->adds + gain_val->decs;
        gain_val->gain = _calc_bayes_gain2(gains, gain_val->adds, gain_val->decs);
        sum += (double)gains / (double)nums * gain_val->gain;
        // printf("[%d %d] %d %d %d %f %f\n",gains, nums, gain_val->class, gain_val->adds, 
        //     gain_val->decs, gain_val->gain, sum);
    }
    sis_struct_list_destroy(gain_class);
    out -= sum;
    return out;
}

void _sis_ai_bayes_get_mids(s_sis_struct_list *list_, s_sis_struct_list *mids_)
{
    int count = list_->count - 1;
    bool isavg = false;
    if (count > 16)
    {
        count = 16;
        isavg = true;
    } 
    if (!isavg) 
    {
        count = list_->count - 1;
        double min = ((s_ai_bayes_gain_series *)sis_struct_list_first(list_))->series;
        double max = 0.0;      
        for (int i = 1; i < list_->count; i++)
        {
            max = ((s_ai_bayes_gain_series *)sis_struct_list_get(list_, i))->series;
            double val = min + (max - min) / 2.0;
            // printf("mid=%.2f\n",val);
            sis_struct_list_push(mids_, &val);
            min = max;
        } 
    }
    else
    {
        count = sis_min(count, list_->count - 1);        
        double min = ((s_ai_bayes_gain_series *)sis_struct_list_first(list_))->series;
        double max = ((s_ai_bayes_gain_series *)sis_struct_list_last(list_))->series;
        double gap = (max - min) / (double)count;
        for (int i = 0; i < count; i++)
        {
            double val = min + (i + 1 ) * gap;
            sis_struct_list_push(mids_, &val);
        }  
    }
}
int _sort_gain_series(const void *arg1, const void *arg2 ) 
{ 
    return ((s_ai_bayes_gain_series *)arg1)->series > ((s_ai_bayes_gain_series *)arg2)->series;
}

double sis_ai_bayes_chance_get_gain_series(s_ai_bayes_class *cls_, const char *key_, double *div_)
{
    s_ai_bayes_unit *add_unit = sis_map_pointer_get(cls_->factor_add_lists, key_);
    s_ai_bayes_unit *dec_unit = sis_map_pointer_get(cls_->factor_dec_lists, key_);

    if (!add_unit||!dec_unit)
    {
        return 0.0;
    }
    
    // printf("%s %d %p %p\n",key_, sis_map_pointer_getsize(cls_->factor_add_lists) ,add_unit, dec_unit);
    int adds = add_unit->samples->count;
    int decs = dec_unit->samples->count;
    int nums = adds + decs;
    double out = _calc_bayes_gain2(adds + decs, adds, decs);

    // 设置count的目的是为了减少计算量     
    s_sis_struct_list *series_list = sis_struct_list_create(sizeof(s_ai_bayes_gain_series));
    for (int i = 0; i < add_unit->samples->count; i++)
    {
        s_ai_bayes_input *in = (s_ai_bayes_input *)sis_struct_list_get(add_unit->samples, i);
        s_ai_bayes_gain_series gain;
        gain.style = AI_BAYES_ADD;
        gain.series = in->series;
        sis_struct_list_push(series_list, &gain);
    }
    for (int i = 0; i < dec_unit->samples->count; i++)
    {
        s_ai_bayes_input *in = (s_ai_bayes_input *)sis_struct_list_get(dec_unit->samples, i);
        s_ai_bayes_gain_series gain;
        gain.style = AI_BAYES_DEC;
        gain.series = in->series;
        sis_struct_list_push(series_list, &gain);
    }
    if (series_list->count <= 5)
    {
        // 数据太少不计算
        sis_struct_list_destroy(series_list);
        return 0.0;
    }
    // 排序后取中值点
    qsort(sis_struct_list_first(series_list), series_list->count, sizeof(s_ai_bayes_gain_series), _sort_gain_series);
    
    s_sis_struct_list *mids = sis_struct_list_create(sizeof(double));

    _sis_ai_bayes_get_mids(series_list, mids);
    int count = mids->count;
    // 得到实际的中值数
    double out_gain = 0.0; 
    for (int i = 0; i < count; i++)
    {
        double *mid = (double *)sis_struct_list_get(mids, i);
        int adds_min = 0;
        int decs_min = 0;
        int adds_max = 0;
        int decs_max = 0;
        for (int k = 0; k < series_list->count; k++)
        {
            s_ai_bayes_gain_series *gain_val = (s_ai_bayes_gain_series *)sis_struct_list_get(series_list, k);
            if (gain_val->style == AI_BAYES_ADD) 
            {
                if (gain_val->series > *mid)
                {
                    adds_max++;
                }
                else
                {
                    adds_min++;
                }
            }
            if (gain_val->style == AI_BAYES_DEC)
            {
                if (gain_val->series > *mid)
                {
                    decs_max++;
                }
                else
                {
                    decs_min++;
                }
            }
        }
        double min_gain = _calc_bayes_gain2(adds_min + decs_min, adds_min, decs_min);
        double max_gain = _calc_bayes_gain2(adds_max + decs_max, adds_max, decs_max);
        // printf("%f %f | %f %f\n", (double)(adds_min + decs_min) / (double)nums, min_gain, 
        //                   (double)(adds_max + decs_max) / (double)nums, max_gain);
        double cur_gain = (double)(adds_min + decs_min) / (double)nums * min_gain + 
                          (double)(adds_max + decs_max) / (double)nums * max_gain;

        // printf("gain = %f %f %f [%d,%d,%d,%d]\n", out, cur_gain, *mid, adds_min, decs_min, adds_max, decs_max);
        cur_gain = out - cur_gain;

        if (cur_gain > out_gain)
        {
            out_gain = cur_gain;
            *div_ = *mid;
        }
    }
    // printf("gain = %f %d %d\n", out_gain, count, series_list->count);
    sis_struct_list_destroy(mids);
    sis_struct_list_destroy(series_list);
    return out_gain;
}
void _sis_ai_bayes_get_mids_random(s_sis_struct_list *list_, s_sis_struct_list *mids_)
{
    int count = list_->count - 1;
    bool isavg = false;
    if (count > 16)
    {
        // 16*15/2 = 120 
        // 最大45种组合
        count = 16;
        isavg = true;
    } 
    if (!isavg) 
    {
        count = list_->count - 1;
        double min = ((s_ai_bayes_gain_series *)sis_struct_list_first(list_))->series;
        s_ai_bayes_box_random box;
        double max = 0.0;      
        for (int i = 1; i < list_->count; i++)
        {
            max = ((s_ai_bayes_gain_series *)sis_struct_list_get(list_, i))->series;
            box.left = min + (max - min) / 2.0;
            min = max;
            for (int k = i + 1; k < list_->count; k++)
            {
                max = ((s_ai_bayes_gain_series *)sis_struct_list_get(list_, k))->series;
                box.right = min + (max - min) / 2.0;
                sis_struct_list_push(mids_, &box);
            }
        } 
    }
    else
    {
        count = sis_min(count, list_->count - 1);        
        double min = ((s_ai_bayes_gain_series *)sis_struct_list_first(list_))->series;
        double max = ((s_ai_bayes_gain_series *)sis_struct_list_last(list_))->series;
        double gap = (max - min) / (double)count;
        s_ai_bayes_box_random box;
        for (int i = 0; i < count; i++)
        {
            box.left = min + (i + 1) * gap;
            for (int k = i + 1; k < count; k++)
            {
                box.right = min + (k + 1) * gap;
                sis_struct_list_push(mids_, &box);
            }
        }  
    }
}
double sis_ai_bayes_chance_get_gain_random(s_ai_bayes_class *cls_, const char *key_, double *left_, double *right_)
{
    s_ai_bayes_unit *add_unit = sis_map_pointer_get(cls_->factor_add_lists, key_);
    s_ai_bayes_unit *dec_unit = sis_map_pointer_get(cls_->factor_dec_lists, key_);

    if (!add_unit||!dec_unit)
    {
        return 0.0;
    }
    
    int adds = add_unit->samples->count;
    int decs = dec_unit->samples->count;
    int nums = adds + decs;
    double out = _calc_bayes_gain2(adds + decs, adds, decs);

    // 设置 count 的目的是为了减少计算量     
    s_sis_struct_list *series_list = sis_struct_list_create(sizeof(s_ai_bayes_gain_series));
    for (int i = 0; i < add_unit->samples->count; i++)
    {
        s_ai_bayes_input *in = (s_ai_bayes_input *)sis_struct_list_get(add_unit->samples, i);
        s_ai_bayes_gain_series gain;
        gain.style = AI_BAYES_ADD;
        gain.series = in->series;
        sis_struct_list_push(series_list, &gain);
    }
    for (int i = 0; i < dec_unit->samples->count; i++)
    {
        s_ai_bayes_input *in = (s_ai_bayes_input *)sis_struct_list_get(dec_unit->samples, i);
        s_ai_bayes_gain_series gain;
        gain.style = AI_BAYES_DEC;
        gain.series = in->series;
        sis_struct_list_push(series_list, &gain);
    }
    if (series_list->count <= 5)
    {
        // 数据太少不计算
        sis_struct_list_destroy(series_list);
        return 0.0;
    }
    // 排序后取中值点
    qsort(sis_struct_list_first(series_list), series_list->count, sizeof(s_ai_bayes_gain_series), _sort_gain_series);
    
    s_sis_struct_list *mids = sis_struct_list_create(sizeof(s_ai_bayes_box_random));

    *left_ = 0.0;
    *right_ = 0.0;

    _sis_ai_bayes_get_mids_random(series_list, mids);
    int count = mids->count;
    // 得到实际的中值数
    double out_gain = 0.0; 
    for (int i = 0; i < count; i++)
    {
        s_ai_bayes_box_random *mid = (s_ai_bayes_box_random *)sis_struct_list_get(mids, i);
        int adds[3] = {0,0,0};
        int decs[3] = {0,0,0};
        for (int k = 0; k < series_list->count; k++)
        {
            s_ai_bayes_gain_series *gain_val = (s_ai_bayes_gain_series *)sis_struct_list_get(series_list, k);
            if (gain_val->style == AI_BAYES_ADD) 
            {
                if (gain_val->series > mid->right)
                {
                    adds[2]++;
                }
                else if (gain_val->series < mid->left)
                {
                    adds[0]++;
                }
                else
                {
                    adds[1]++;
                }
            }
            if (gain_val->style == AI_BAYES_DEC)
            {
                if (gain_val->series > mid->right)
                {
                    decs[2]++;
                }
                else if (gain_val->series < mid->left)
                {
                    decs[0]++;
                }
                else
                {
                    decs[1]++;
                }
            }
        }
        double cur_gain = 0.0;
        for (int i = 0; i < 3; i++)
        {
            double gain = _calc_bayes_gain2(adds[i] + decs[i], adds[i], decs[i]);
            cur_gain += (double)(adds[i] + decs[i]) / (double)nums * gain;
        }
        printf(":: = %f %f  | %f  %f [%d,%d,%d],[%d,%d,%d]\n", out, cur_gain, mid->left, mid->right, adds[0], adds[1], adds[2], decs[0], decs[1], decs[2]);
        cur_gain = out - cur_gain;

        if (cur_gain > out_gain)
        {
            out_gain = cur_gain;
            *left_ = mid->left;
            *right_ = mid->right;
        }
    }
    printf("%s = %f %d %d : %f %f\n", key_, out_gain, count, series_list->count, *left_, *right_);
    sis_struct_list_destroy(mids);
    sis_struct_list_destroy(series_list);
    return out_gain;
}
int sis_ai_bayes_study_output(s_ai_bayes_class *cls_)
{
    if (sis_dict_getsize(cls_->style_list) < 1)
    {
        return -1;
    }
    double max_gain = 0.0;
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(cls_->style_list);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_ai_bayes_style *style = (s_ai_bayes_style *)sis_dict_getval(de);
            if (style->style == AI_BAYES_TYPE_FACTOR)
            {
                style->gain = sis_ai_bayes_chance_get_gain_factor(cls_, style->name);
            }
            if (style->style == AI_BAYES_TYPE_SERIES)
            {
                style->gain = sis_ai_bayes_chance_get_gain_series(cls_, style->name, &style->serise_mid);
            }
            if (style->style == AI_BAYES_TYPE_RANDOM)
            {
                style->gain = sis_ai_bayes_chance_get_gain_random(cls_, style->name, &style->serise_left, &style->serise_right);
            }
            // printf("name=%s\n", style->name);
            max_gain = sis_max(max_gain, style->gain);
        }
        sis_dict_iter_free(di);
    }

    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(cls_->style_list);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_ai_bayes_style *style = (s_ai_bayes_style *)sis_dict_getval(de);
            if (style->gain * 10 < max_gain)
            {
                style->used = false;
            }
            else
            {
                style->used = true;
            }
        }
        sis_dict_iter_free(di);
    }
    return 0;
}
// list 为从小到大的顺序化数列
int sis_ai_bayes_classify(double in_, int n, double ins[])
{
    // -0.05 -0.02 0.02 0.05
    for (int i = 0; i < n; i++)
    {
        if (in_ < ins[i])
        {
            return i;
        }        
    }
    return n + 1;
}

#if 0

#define TEST_RANDOM

typedef struct _water
{
    uint8 x[6];
    double y[2];
} _water;

#ifdef TEST_RANDOM1
static struct _water ok[] = {
    { {1, 1, 1, 1, 1, 1}, {0.697, 0.460}},
    { {2, 1, 2, 1, 1, 1}, {0.774, 0.376}},
    { {2, 1, 1, 1, 1, 1}, {0.634, 0.264}},
    { {1, 1, 2, 1, 1, 1}, {0.608, 0.318}},
    { {3, 1, 1, 1, 1, 1}, {0.556, 0.215}},
    { {1, 2, 1, 1, 1, 2}, {0.403, 0.237}},
    { {2, 2, 1, 2, 2, 2}, {0.481, 0.149}},
    { {2, 2, 1, 1, 2, 1}, {0.437, 0.211}},
};
static struct _water err[] = {
    { {2, 2, 2, 2, 2, 1}, {0.666, 0.091}},
    { {1, 3, 3, 1, 3, 2}, {0.243, 0.267}},
    { {3, 3, 3, 3, 3, 1}, {0.245, 0.057}},
    { {3, 1, 1, 3, 3, 2}, {0.343, 0.099}},
    { {1, 2, 1, 2, 1, 1}, {0.639, 0.161}},
    { {3, 2, 2, 2, 1, 1}, {0.657, 0.198}},
    { {2, 2, 1, 1, 2, 2}, {0.360, 0.370}},
    { {3, 1, 1, 3, 3, 1}, {0.593, 0.042}},
    { {1, 1, 2, 2, 2, 1}, {0.719, 0.103}},
};
// add = -3.079671 
// dec = -8.004071 
#else
// 大于1的连续值会造成取值过小，连续值建议用比率来传入
static struct _water ok[] = {
    { {1, 1, 1, 1, 1, 1}, {-0.697, 460}},
    { {2, 1, 2, 1, 1, 1}, {-0.774, 376}},
    { {2, 1, 1, 1, 1, 1}, {-0.634, 264}},
    { {1, 1, 2, 1, 1, 1}, {-0.608, 318}},
    { {3, 1, 1, 1, 1, 1}, {-0.556, 215}},
    { {1, 2, 1, 1, 1, 2}, {0.403, 237}},
    { {2, 2, 1, 2, 2, 2}, {0.481, -149}},
    { {2, 2, 1, 1, 2, 1}, {-0.437, -211}},
};
static struct _water err[] = {
    { {2, 2, 2, 2, 2, 1}, {-0.666, -91}},
    { {1, 3, 3, 1, 3, 2}, {-0.243, -267}},
    { {3, 3, 3, 3, 3, 1}, {-0.245, -57}},
    { {3, 1, 1, 3, 3, 2}, {0.343, -99}},
    { {1, 2, 1, 2, 1, 1}, {-0.639, -161}},
    { {3, 2, 2, 2, 1, 1}, {-0.657, -198}},
    { {2, 2, 1, 1, 2, 2}, {0.360, -1370}},
    { {3, 1, 1, 3, 3, 1}, {-0.593, -42}},
    { {1, 1, 2, 2, 2, 1}, {-0.719, -103}},
};
#endif
int main()
{
    s_ai_bayes_class *class = sis_ai_bayes_create();

    char name[32];
    for (int i = 0; i < 8; i++)
    {
        for (int k = 0; k < 6; k++)
        {
            sis_sprintf(name, 32, "x%d", k);
            sis_ai_bayes_study_push_factor(class, AI_BAYES_ADD, name, ok[i].x[k]);
        }
        for (int k = 0; k < 2; k++)
        {
            sis_sprintf(name, 32, "y%d", k);
#ifdef TEST_RANDOM
            sis_ai_bayes_study_push_random(class, AI_BAYES_ADD, name, ok[i].y[k]);
#else
            sis_ai_bayes_study_push_series(class, AI_BAYES_ADD, name, ok[i].y[k]);
#endif               
        }
    }
    for (int i = 0; i < 9; i++)
    {
        for (int k = 0; k < 6; k++)
        {
            sis_sprintf(name, 32, "x%d", k);
            sis_ai_bayes_study_push_factor(class, AI_BAYES_DEC, name, err[i].x[k]);
        }
        for (int k = 0; k < 2; k++)
        {
            sis_sprintf(name, 32, "y%d", k);
#ifdef TEST_RANDOM
            sis_ai_bayes_study_push_random(class, AI_BAYES_DEC, name, err[i].y[k]);
#else
            sis_ai_bayes_study_push_series(class, AI_BAYES_DEC, name, err[i].y[k]);
#endif   
        }
    }
    printf("%d %d %d\n", 
            (int)sis_map_pointer_getsize(class->factor_add_lists),
            (int)sis_map_pointer_getsize(class->factor_dec_lists),
            (int)sis_map_pointer_getsize(class->factor_mid_lists));
    // {
    //     s_sis_dict_entry *de;
    //     s_sis_dict_iter *di = sis_dict_get_iter(class->factor_add_lists);
    //     while ((de = sis_dict_next(di)) != NULL)
    //     {
    //         s_ai_bayes_unit *unit = (s_ai_bayes_unit *)sis_dict_getval(de);
    //         printf("add [%s] avg=%f, vari=%f\n", unit->name, unit->avg, unit->vari);
    //         for (int i = 0; i < unit->classify->count; i++)
    //         {
    //             s_ai_bayes_value *value = (s_ai_bayes_value *)sis_struct_list_get(unit->classify, i);
    //             printf("class : %d %d\n", value->count, value->class);
    //         }
    //         for (int i = 0; i < unit->samples->count; i++)
    //         {
    //             s_ai_bayes_input *input = (s_ai_bayes_input *)sis_struct_list_get(unit->samples, i);
    //             printf("value : %d %d %f\n", input->style, input->class, input->series);
    //         }
            
    //     }
    //     sis_dict_iter_free(di);
    // }
    // {
    //     s_sis_dict_entry *de;
    //     s_sis_dict_iter *di = sis_dict_get_iter(class->factor_dec_lists);
    //     while ((de = sis_dict_next(di)) != NULL)
    //     {
    //         s_ai_bayes_unit *unit = (s_ai_bayes_unit *)sis_dict_getval(de);
    //         printf("dec [%s] avg=%f, vari=%f\n", unit->name, unit->avg, unit->vari);
    //         for (int i = 0; i < unit->classify->count; i++)
    //         {
    //             s_ai_bayes_value *value = (s_ai_bayes_value *)sis_struct_list_get(unit->classify, i);
    //             printf("class : %d %d\n", value->count, value->class);
    //         }
    //         for (int i = 0; i < unit->samples->count; i++)
    //         {
    //             s_ai_bayes_input *input = (s_ai_bayes_input *)sis_struct_list_get(unit->samples, i);
    //             printf("value : %d %d %f\n", input->style, input->class, input->series);
    //         }
            
    //     }
    //     sis_dict_iter_free(di);
    // }

    // 求增益 和相关其他值
    {
        sis_ai_bayes_study_output(class);
        
        for (int k = 0; k < 6; k++)
        {
            sis_sprintf(name, 32, "x%d", k);
            s_ai_bayes_style *style = sis_ai_bayes_get_style(class, name);
            printf("rate [%s:%d:%d] %f %f %f %f \n", name, style->used, style->style, 
                style->gain, style->serise_mid, 
                style->serise_left, style->serise_right); 
        }
        for (int k = 0; k < 2; k++)
        {
            sis_sprintf(name, 32, "y%d", k);
            s_ai_bayes_style *style = sis_ai_bayes_get_style(class, name);
            printf("rate [%s:%d:%d] %f %f %f %f \n", name, style->used, style->style, 
                style->gain, style->serise_mid, 
                style->serise_left, style->serise_right); 
        }
    }

    sis_ai_bayes_check_init(class);
    for (int k = 0; k < 6; k++)
    {
        sis_sprintf(name, 32, "x%d", k);
        sis_ai_bayes_check_push_factor(class, name, ok[1].x[k]);
    }
    for (int k = 0; k < 2; k++)
    {
        sis_sprintf(name, 32, "y%d", k);
#ifdef TEST_RANDOM
        sis_ai_bayes_check_push_random(class, name, ok[1].y[k]);
#else
        sis_ai_bayes_check_push_series(class, name, ok[1].y[k]);
#endif
    }

    double add = sis_ai_bayes_check_output(class, AI_BAYES_ADD);
    printf("add = %f \n", add);
    double dec = sis_ai_bayes_check_output(class, AI_BAYES_DEC);
    printf("dec = %f \n", dec);

    sis_ai_bayes_destroy(class);
    return 0;
}

#endif


#if 0

// 根据数据特征模拟传入数据，求最后的评判结果

int main()
{
    s_ai_bayes_class *class = sis_ai_bayes_create();

    sis_init_random();
    char name[32];
    for (int i = 0; i < 100; i++)
    {
        for (int k = 0; k < 5; k++)
        {
            sis_sprintf(name, 32, "x%d", k);
            // sis_ai_bayes_study_push_random(class, AI_BAYES_ADD, name, sis_get_random(0.5 , 0.9));
            // sis_ai_bayes_study_push_random(class, AI_BAYES_DEC, name, sis_get_random(-0.9 , -0.5));
            // sis_ai_bayes_study_push_random(class, AI_BAYES_MID, name, sis_get_random(-0.4 , 0.4));
            sis_ai_bayes_study_push_random(class, AI_BAYES_ADD, name, sis_get_random(0.6 , 0.99));
            sis_ai_bayes_study_push_random(class, AI_BAYES_DEC, name, sis_get_random(0.01 , 0.5));
            sis_ai_bayes_study_push_random(class, AI_BAYES_MID, name, sis_get_random(0.41 , 0.699));
        }
    }
    for (int i = 0; i < 100; i++)
    {
        for (int k = 0; k < 5; k++)
        {
            sis_sprintf(name, 32, "x%d", k);
            // sis_ai_bayes_study_push_random(class, AI_BAYES_ADD, name, sis_get_random(0.5 , 0.9));
            // sis_ai_bayes_study_push_random(class, AI_BAYES_DEC, name, sis_get_random(-0.9 , -0.5));
            // sis_ai_bayes_study_push_random(class, AI_BAYES_MID, name, sis_get_random(-0.4 , 0.4));
            sis_ai_bayes_study_push_random(class, AI_BAYES_ADD, name, sis_get_random(0.01 , 0.2));
            sis_ai_bayes_study_push_random(class, AI_BAYES_DEC, name, sis_get_random(0.01 , 0.5));
            sis_ai_bayes_study_push_random(class, AI_BAYES_MID, name, sis_get_random(0.41 , 0.699));
        }
    }
    printf("%d %d %d\n", 
            (int)sis_map_pointer_getsize(class->factor_add_lists),
            (int)sis_map_pointer_getsize(class->factor_dec_lists),
            (int)sis_map_pointer_getsize(class->factor_mid_lists));
    // {
    //     s_sis_dict_entry *de;
    //     s_sis_dict_iter *di = sis_dict_get_iter(class->factor_add_lists);
    //     while ((de = sis_dict_next(di)) != NULL)
    //     {
    //         s_ai_bayes_unit *unit = (s_ai_bayes_unit *)sis_dict_getval(de);
    //         printf("add [%s] avg=%f, vari=%f\n", unit->name, unit->avg, unit->vari);
    //         for (int i = 0; i < unit->classify->count; i++)
    //         {
    //             s_ai_bayes_value *value = (s_ai_bayes_value *)sis_struct_list_get(unit->classify, i);
    //             printf("class : %d %d\n", value->count, value->class);
    //         }
    //         for (int i = 0; i < unit->samples->count; i++)
    //         {
    //             s_ai_bayes_input *input = (s_ai_bayes_input *)sis_struct_list_get(unit->samples, i);
    //             printf("value : %d %d %f\n", input->style, input->class, input->series);
    //         }
            
    //     }
    //     sis_dict_iter_free(di);
    // }
    // {
    //     s_sis_dict_entry *de;
    //     s_sis_dict_iter *di = sis_dict_get_iter(class->factor_dec_lists);
    //     while ((de = sis_dict_next(di)) != NULL)
    //     {
    //         s_ai_bayes_unit *unit = (s_ai_bayes_unit *)sis_dict_getval(de);
    //         printf("dec [%s] avg=%f, vari=%f\n", unit->name, unit->avg, unit->vari);
    //         for (int i = 0; i < unit->classify->count; i++)
    //         {
    //             s_ai_bayes_value *value = (s_ai_bayes_value *)sis_struct_list_get(unit->classify, i);
    //             printf("class : %d %d\n", value->count, value->class);
    //         }
    //         for (int i = 0; i < unit->samples->count; i++)
    //         {
    //             s_ai_bayes_input *input = (s_ai_bayes_input *)sis_struct_list_get(unit->samples, i);
    //             printf("value : %d %d %f\n", input->style, input->class, input->series);
    //         }
            
    //     }
    //     sis_dict_iter_free(di);
    // }
    // 求增益 和相关其他值
    {
        sis_ai_bayes_study_output(class);
        
        for (int k = 0; k < 5; k++)
        {
            sis_sprintf(name, 32, "x%d", k);
            s_ai_bayes_style *style = sis_ai_bayes_get_style(class, name);
            printf("rate [%s:%d:%d] %f %f %f %f \n", name, style->used, style->style, 
                style->gain, style->serise_mid, 
                style->serise_left, style->serise_right); 
        }
    }

    sis_ai_bayes_check_init(class);

    for (int k = 0; k < 5; k++)
    {
        sis_sprintf(name, 32, "x%d", k);
        // sis_ai_bayes_check_push_random(class, name, sis_get_random(0.5 , 0.9));
        // sis_ai_bayes_check_push_random(class, name, sis_get_random(0.4 , 0.6));
        // sis_ai_bayes_check_push_random(class, name, sis_get_random(0.1 , 0.2));
        sis_ai_bayes_check_push_random(class, name, sis_get_random(0.6 , 0.8));
        // sis_ai_bayes_study_push_random(class, AI_BAYES_MID, name, sis_get_random(-0.6 , 0.4));
    }
    // sis_ai_bayes_check_push_random(class, "x0", 0.65);
    // sis_ai_bayes_check_push_random(class, "x1", 0.1);
    // sis_ai_bayes_check_push_random(class, "x2", 0.75);
    // sis_ai_bayes_check_push_random(class, "x3", 0.15);
    // sis_ai_bayes_check_push_random(class, "x4", 0.85);

    double add = sis_ai_bayes_check_output(class, AI_BAYES_ADD);
    printf("add = %f \n", add);
    double dec = sis_ai_bayes_check_output(class, AI_BAYES_DEC);
    printf("dec = %f \n", dec);
    double mid = sis_ai_bayes_check_output(class, AI_BAYES_MID);
    printf("mid = %f \n", mid);

    sis_ai_bayes_destroy(class);
    return 0;
}

#endif