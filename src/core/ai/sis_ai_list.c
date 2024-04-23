
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
int sis_ai_list_getsize(s_sis_ai_list *list_)
{
    return list_->klists->count;
}

#if 0
int main()
{
    const char *codes = "SZ000415,SZ000517,SZ000607,SZ000700,SZ000751,SZ000762,SZ000822,SZ000898,SZ000933,SZ000963,SZ000983,SZ002077,SZ002085,SZ002104,SZ002128,SZ002166,SZ002182,SZ002192,SZ002223,SZ002329,SZ002335,SZ002415,SZ002424,SZ002518,SZ002655,SZ002741,SZ002785,SZ002805,SZ002865,SZ002920,SZ002936,SZ002950,SZ003022,SZ300014,SZ300118,SZ300331,SZ300401,SZ300421,SZ300486,SZ300629,SZ300655,SZ300682,SZ300800,SH600020,SH600282,SH600732,SH600751,SH600795,SH600888,SH600938,SH601600,SH601669,SH601699,SH601727,SH601992,SH603139,SH603169,SH603217,SH603299,SH603313,SH603606,SH603688,SH603722,SH603876,SH603993,SZ000672,SZ000709,SZ000878,SZ000919,SZ000921,SZ000989,SZ002054,SZ002124,SZ002379,SZ002807,SZ002907,SZ002947,SZ002958,SZ300206,SZ300322,SZ300358,SZ300483,SH600170,SH600198,SH600208,SH600237,SH600529,SH600606,SH600808,SH601225,SH601288,SH601618,SH601688,SH601818,SH601898,SH601916,SH601919,SH603938,SZ000666,SZ000887,SZ002017,SZ002050,SZ002074,SZ002121,SZ002219,SZ002255,SZ002562,SZ002703,SZ300091,SZ300412,SZ300536,SZ300586,SZ300890,SH600026,SH600478,SH600664,SH600777,SH600784,SH601388,SH601866,SH601975,SH601985,SH603067,SH603127,SH603505,SH603638,SH603790,SZ000717,SZ000821,SZ001308,SZ002045,SZ002057,SZ002179,SZ002268,SZ002458,SZ002580,SZ300057,SZ300181,SZ300250,SH600017,SH600339,SH600536,SH600567,SH600839,SH601236,SZ000035,SZ000567,SZ000733,SZ002488,SZ002965,SZ300379,SZ300566,SZ300712,SH601456,SH601689,SH601958,SH603595,SH603915,SH603966,SZ000656,SZ000831,SZ002352,SZ002602,SZ002943,SZ300158,SZ300221,SZ300326,SZ300632,SZ300694,SZ300700,SZ300777,SH600027,SH600179,SH600188,SH600231,SH600276,SH601872,SH603227,SH603608,SZ000034,SZ000739,SZ002468,SZ002600,SZ002937,SZ300003,SZ300347,SZ300766,SZ300831,SZ300909,SH600521,SH603083,SH603699,SZ000503,SZ000600,SZ000617,SZ000688,SZ000970,SZ002112,SZ002212,SZ002517,SZ002585,SZ002625,SZ300395,SZ300396,SH600623,SH600973,SH600996,SH601995,SH603032,SH603138,SH603421,SH603766,SZ000519,SZ002906,SZ003039,SZ300055,SZ300438,SZ300735,SZ300825,SZ301069,SH600063,SH600150,SH601886,SH603297,SH603803,SZ002386,SH600057,SH600076,SH600097,SH600160,SH600337,SH600370,SH600527,SH600611,SH603189,SH603739,SH605358,SH605377,SZ000066,SZ002106,SZ002275,SZ002343,SZ300512,SH600162,SH600569,SH600977,SH601005,SH601168,SH601186,SH601390,SZ000819,SZ002009,SZ002091,SZ002125,SZ002416,SZ300054,SZ300073,SZ300247,SZ300504,SZ300604,SH600499,SH600988,SH601137,SH601633,SH603259,SH603661,SZ000507,SZ002073,SZ002109,SZ002533,SZ300432,SZ300481,SZ300643,SH600416,SH600523,SH601012,SH601988,SH603077,SH603920,SZ000155,SZ000420,SZ000523,SZ000707,SZ000959,SZ002997,SZ300121,SZ300212,SZ300471,SZ300601,SH601233,SH601989,SZ000408,SZ000973,SZ002190,SZ002436,SZ002493,SZ002895,SZ300031,SZ300040,SZ300317,SZ300364,SZ300708,SZ300759,SZ300773,SH600458,SH600735,SH601966,SZ000927,SZ002859,SZ003012,SZ300190,SZ300425,SZ300519,SZ300587,SZ300647,SH600259,SH600487,SH600741,SH603900,SZ002334,SZ002487,SZ300059,SZ300239,SZ300748,SZ300953,SH600172,SH600522,SH601127,SH603178,SH603363,SZ002304,SZ002414,SZ002567,SZ002594,SZ002821,SZ300005,SZ300035,SZ300496,SH600293,SH600392,SH600782,SH601179,SZ000799,SZ002365,SZ002529,SZ002812,SZ300115,SZ300631,SZ300975,SH600111,SH600215,SH600703,SH600769,SH600865,SZ002284,SZ002388,SZ002623,SZ300199,SZ300252,SZ300475,SZ300681,SZ000400,SZ000568,SZ002036,SZ300105,SZ300149,SZ300343,SZ300568,SZ300755,SH600299,SH600596,SH601016,SH601606,SH601717,SZ002205,SZ002538,SZ002539,SZ002926,SZ300409,SZ300477,SZ300498,SH600256,SH600295,SH600351,SH600552,SH600673,SH601678,SH603501,SH603612,SZ002408,SZ002735,SZ300497,SZ300683,SH600173,SH600219,SH600545,SH601088,SH601800,SH601857,SH603116,SH605507,SZ002374,SZ002783,SZ301062,SH600331,SH600547,SH600783,SH601668,SH605589,SZ000728,SZ002010,SZ002015,SZ002127,SZ300168,SZ300289,SZ300460,SH600867,SH600929,SH601598,SH603398,SZ002110,SZ002550,SZ300214,SZ300510,SH600851,SH600854,SH600926,SH601825,SH603167,SH603326,SH603600";
    s_sis_string_list *klist = sis_string_list_create();
    sis_string_list_load(klist, codes, sis_strlen(codes), ",");
    int count = sis_string_list_getsize(klist);
    int nums[16] = {0};
    for (int i = 0; i < count; i++)
    {
        const char *code = sis_string_list_get(klist, i);
        int index = sis_get_hash(code, sis_strlen(code), 16);
        nums[index]++;
        printf("%s : %d\n", code, index);
    }
    for (int i = 0; i < 16; i++)
    {
        printf("%d : %d %d\n", count, i, nums[i]);
    }
    sis_string_list_destroy(klist);
}
#endif
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

    for (int i = 0; i < sis_ai_list_getsize(list); i++)
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