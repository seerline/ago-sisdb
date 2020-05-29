
#include <sis_ai_list.h>

s_sis_ai_list *sis_ai_list_create()
{
    s_sis_ai_list *list = SIS_MALLOC(s_sis_ai_list, list);
    list->fields = sis_struct_list_create(sizeof(s_sis_ai_list_field));
    list->klists = sis_struct_list_create(sizeof(s_sis_ai_list_key));
    list->vlists = sis_pointer_list_create();
    list->vlists->vfree = sis_free_call;
    list->isinit = 0;
    return list;
}
void sis_ai_list_destroy(s_sis_ai_list *list_)
{
    sis_struct_list_destroy(list_->fields);
    sis_struct_list_destroy(list_->klists);
    sis_pointer_list_destroy(list_->vlists);
    sis_free(list_);
}
void sis_ai_list_clear(s_sis_ai_list *list_)
{
    sis_struct_list_clear(list_->fields);
    sis_struct_list_clear(list_->klists);
    sis_pointer_list_clear(list_->vlists);
    list_->isinit = 0;
}

int sis_ai_list_init_field(s_sis_ai_list *list_, const char *fn_)
{
    if (list_->isinit)
    {
        return -1;
    }
    s_sis_ai_list_field field;
    sis_strcpy(field.name, 64, fn_);
    field.agoidx = -1;
    return sis_struct_list_push(list_->fields, &field);
}
int sis_ai_list_init_finished(s_sis_ai_list *list_)
{
    if (list_->isinit)
    {
        return -1;
    }
    list_->isinit = 1;
    return list_->fields->count;
}
int sis_ai_list_key_push(s_sis_ai_list *list_,  int time, double newp, double output)
{
    if (!list_->isinit)
    {
        return -1;
    }
    s_sis_ai_list_key key;
    key.time = time;
    key.newp = newp;
    key.output = output;

    double *val = sis_malloc(sizeof(double) * list_->fields->count);
    memset(val, 0, sizeof(double) * list_->fields->count);
    sis_pointer_list_push(list_->vlists, val);
    sis_struct_list_push(list_->klists, &key);
    return list_->klists->count;
}

int _sis_ai_list_find_field(s_sis_struct_list *list_, const char *fn_)
{
    for (int i = 0; i < list_->count; i++)
    {
        s_sis_ai_list_field *field = (s_sis_ai_list_field *)sis_struct_list_get(list_, i);
        if (!sis_strcasecmp(field->name, fn_))
        {
            return i;
        }
    }
    return -1;
}
int _sis_ai_list_find_time(s_sis_struct_list *list_, int time_)
{
    int o = -1;
    for (int i = 0; i < list_->count; i++)
    {
        s_sis_ai_list_key *key = (s_sis_ai_list_key *)sis_struct_list_get(list_, i);
        if (time_ < key->time)
        {
            break;
        }
        o = i;
    }
    return o;
}
// 根据时间插入对应的数据, 需要补齐缺失的数据
int sis_ai_list_value_push(s_sis_ai_list *list_, int time, const char *fn_, double value_)
{
    if (!list_->isinit)
    {
        return -1;
    }
    int fidx = _sis_ai_list_find_field(list_->fields, fn_);
    s_sis_ai_list_field *field = (s_sis_ai_list_field *)sis_struct_list_get(list_->fields, fidx);
    if (!field)
    {
        return -2;
    }
    if (list_->klists->count < 1)
    {
        return -3;
    }
    // if (field->agoidx < 0)
    // {
    //     field->agoidx = 0;
    // }
    int kidx = _sis_ai_list_find_time(list_->klists, time);
    // printf("%d %d time= %d\n", kidx ,field->agoidx, time);
    if (kidx < field->agoidx)
    {
        return -4;  // 不能反向修改数据
    }
    else if (kidx > field->agoidx)
    {
        field->agoidx++;
    }
    
    // printf("%d %d time= %d %f\n", kidx ,field->agoidx, time, value_);
    // + 5 是为了最多后续5个周期用前置的数据
    for (int i = field->agoidx; i <= kidx + 5 && i < list_->klists->count; i++)
    // 大于基准时间的数据都用最近的数据
    // for (int i = field->agoidx; i < list_->klists->count; i++)
    {
        double *val = (double *)sis_pointer_list_get(list_->vlists, i);
        if (val)
        {
            val[fidx] = value_;
        }      
    }
    field->agoidx = kidx;
    return kidx;
}

const char *sis_ai_list_get_field(s_sis_ai_list *list_, int index_)
{
    if (!list_->isinit)
    {
        return NULL;
    }
    s_sis_ai_list_field *field = sis_struct_list_get(list_->fields, index_);
    if (field)
    {
        return (const char *)field->name;
    }
    return NULL;
}
s_sis_ai_list_key *sis_ai_list_get_key(s_sis_ai_list *list_, int index_)
{
    if (!list_->isinit)
    {
        return NULL;
    }
    return sis_struct_list_get(list_->klists, index_);
}

double sis_ai_list_get(s_sis_ai_list *list_, int fidx_, int kidx_)
{
    if (!list_->isinit)
    {
        return 0.0;
    }
    double *val = (double *)sis_pointer_list_get(list_->vlists, kidx_);
    if (val && fidx_ >= 0 && fidx_ < list_->fields->count)
    {
        return val[fidx_];
    }
    return 0.0;
}

int sis_ai_list_get_field_size(s_sis_ai_list *list_)
{
    return list_->fields->count;
}
int sis_ai_list_get_size(s_sis_ai_list *list_)
{
    return list_->klists->count;
}

#if 0
int main()
{
    s_sis_ai_list *list = sis_ai_list_create();

    sis_ai_list_init_field(list, "m1");
    sis_ai_list_init_field(list, "m2");
    sis_ai_list_init_field(list, "m3");
    sis_ai_list_init_finished(list);

    sis_ai_list_key_push(list, 10, 10, 1);
    sis_ai_list_key_push(list, 20, 11, 0);
    sis_ai_list_key_push(list, 30, 12, -1);
    sis_ai_list_key_push(list, 40, 11, 0);
    sis_ai_list_key_push(list, 50, 10, 1);

    sis_ai_list_value_push(list, 15, "m1", 0.123);
    sis_ai_list_value_push(list, 18, "m1", 0.125);
    sis_ai_list_value_push(list, 35, "m1", 0.134);
    sis_ai_list_value_push(list, 50, "m1", 0.145);
    // sis_ai_list_value_push(list, 15, "m1", 0.123);

    sis_ai_list_value_push(list, 30, "m2", 0.111);
    sis_ai_list_value_push(list, 40, "m3", 0.122);

    for (int i = 0; i < sis_ai_list_get_size(list); i++)
    {
        s_sis_ai_list_key *key = sis_ai_list_get_key(list,i);
        printf("%d : %f : %d ",key->time, key->newp, key->output);
        for (int k = 0; k < sis_ai_list_get_field_size(list); k++)
        {
            printf("%s : %f |",sis_ai_list_get_field(list, k), sis_ai_list_get(list, k, i));
        }
        printf("\n");
    }

    sis_ai_list_destroy(list);
    return 0;
}
#endif