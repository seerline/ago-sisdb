
#include <sis_message.h>

/////////////////////////////////
//  message
/////////////////////////////////
void _sis_message_unit_free(void *unit_)
{
    s_sis_message_unit *unit = (s_sis_message_unit *)unit_;
    switch (unit->style)
    {
    case SIS_MESSGE_TYPE_SDS:
        // printf("free msg :%p\n", unit->value);
        sis_sdsfree((s_sis_sds)unit->value);
        break;
    case SIS_MESSGE_TYPE_OWNER:
        if (unit->free)
        {
            unit->free(unit->value);
        }
        break;
    case SIS_MESSGE_TYPE_METHOD:
        break;        
    default:
        break;
    }
    sis_free(unit);
}

s_sis_message *sis_message_create()
{
    s_sis_message *o = sis_net_message_create();
    o->map = sis_map_pointer_create_v(_sis_message_unit_free);
    return o;
}

void sis_message_destroy(s_sis_message *msg_)
{
    sis_net_message_destroy(msg_);
}

static inline void _sis_message_set(s_sis_message *msg_, const char* key_, s_sis_message_unit *unit)
{
    if (!msg_->map)
    {
        msg_->map = sis_map_pointer_create_v(_sis_message_unit_free);
        // printf("==2.5==%p==\n", msg_->map);
    }
    sis_map_pointer_set(msg_->map, key_, unit);
}
/**
 * @brief 从请求应答s_sis_message的键值对map中，根据键获取数据
 * @param msg_ 请求应答数据s_sis_message
 * @param key_ 键
 * @return s_sis_message_unit* 
 */
static inline s_sis_message_unit *_sis_message_get(s_sis_message *msg_, const char* key_)
{
    if (!msg_->map)
    {
        return NULL;
    }
    return (s_sis_message_unit *)sis_map_pointer_get(msg_->map, key_);
}
void sis_message_del(s_sis_message *msg_, const char *key_)
{
    if (msg_->map)
    {
        sis_map_pointer_del(msg_->map, key_);
    }
}

void sis_message_set_int(s_sis_message *msg_, const char* key_, int64 in_)
{
    s_sis_message_unit *unit = SIS_MALLOC(s_sis_message_unit, unit);
    unit->style = SIS_MESSGE_TYPE_INT;
    unit->value = (void *)in_;
    _sis_message_set(msg_, key_, unit);
}
void sis_message_set_bool(s_sis_message *msg_, const char* key_, bool in_)
{
    s_sis_message_unit *unit = SIS_MALLOC(s_sis_message_unit, unit);
    unit->style = SIS_MESSGE_TYPE_BOOL;
    unit->value = NULL;
    if (in_) {unit->value = (void *)1;}
    _sis_message_set(msg_, key_, unit);
}
void sis_message_set_double(s_sis_message *msg_, const char* key_, double in_)
{
    s_sis_message_unit *unit = SIS_MALLOC(s_sis_message_unit, unit);
    unit->style = SIS_MESSGE_TYPE_DOUBLE;
    uint64 in;
    memmove(&in, &in_, sizeof(uint64));
    unit->value = (void *)in;
    _sis_message_set(msg_, key_, unit);
}
void sis_message_set_str(s_sis_message *msg_, const char* key_, char *in_, size_t ilen_)
{
    s_sis_message_unit *unit = SIS_MALLOC(s_sis_message_unit, unit);
    unit->style = SIS_MESSGE_TYPE_SDS;
    unit->value = sis_sdsnewlen(in_, ilen_);
    _sis_message_set(msg_, key_, unit);
}

void sis_message_set_method(s_sis_message *msg_, const char *key_, sis_method_define *method_)
{
    s_sis_message_unit *unit = SIS_MALLOC(s_sis_message_unit, unit);
    unit->style = SIS_MESSGE_TYPE_METHOD;
    unit->value = method_;
    _sis_message_set(msg_, key_, unit);
}

// 用户自定义结构体 如果 sis_free_define = NULL 默认调用 sis_free
void sis_message_set(s_sis_message *msg_, const char *key_, void *in_, sis_free_define *free_)
{
    s_sis_message_unit *unit = SIS_MALLOC(s_sis_message_unit, unit);
    unit->style = SIS_MESSGE_TYPE_OWNER;
    unit->value = in_;
    unit->free = free_;
    _sis_message_set(msg_, key_, unit);
}   

bool sis_message_exist(s_sis_message *msg_, const char *key_)
{
    return _sis_message_get(msg_, key_) ? true : false;
}
int sis_message_get_cmd(const char *icmd_, s_sis_sds *service_, s_sis_sds *command_)
{
    int o = 0;
    if (icmd_)
    {
        sis_str_divide_sds(icmd_, '.', service_, command_);
        // printf("%s %p %p  %s %s \n", icmd_, service_, command_, *service_, *command_);
        if (*command_ == NULL)
        {
            *command_ = *service_;
            *service_ = NULL;
            o = 1;
        }
        else
        {
            o = 2;
        }
        // printf("%s %p %p  %s %s \n", icmd_, service_, command_, *service_, *command_);
    }
    return o;
}
int64 sis_message_get_int(s_sis_message *msg_, const char *key_)
{
    s_sis_message_unit *unit = _sis_message_get(msg_, key_);
    if (unit && unit->style == SIS_MESSGE_TYPE_INT)
    {
        return (int64)unit->value;
    }
    return 0;
}
bool sis_message_get_bool(s_sis_message *msg_, const char *key_)
{
    s_sis_message_unit *unit = _sis_message_get(msg_, key_);
    if (unit && unit->style == SIS_MESSGE_TYPE_BOOL)
    {
        return unit->value ? true : false;
    }
    return false;
}
double sis_message_get_double(s_sis_message *msg_, const char *key_)
{
    s_sis_message_unit *unit = _sis_message_get(msg_, key_);
    if (unit && unit->style == SIS_MESSGE_TYPE_DOUBLE)
    {
        uint64 in = (uint64)unit->value;
        double o;
        memmove(&o, &in, sizeof(uint64));
        return o;
    }
    return 0.0;    
}

s_sis_sds sis_message_get_str(s_sis_message *msg_, const char *key_)
{
    s_sis_message_unit *unit = _sis_message_get(msg_, key_);
    if (unit && unit->style == SIS_MESSGE_TYPE_SDS)
    {
        return (s_sis_sds)unit->value;
    }
    return NULL;
}
/**
 * @brief 通过函数名从请求应答数据s_sis_message中获取函数指针，该函数指针如果存在的话，应该存储于s_sis_message内部的键值对中
 * @param msg_ 请求应答数据s_sis_message
 * @param key_ 函数名
 * @return sis_method_define* 函数指针
 */
sis_method_define *sis_message_get_method(s_sis_message *msg_, const char *key_)
{
    s_sis_message_unit *unit = _sis_message_get(msg_, key_);
    if (unit && unit->style == SIS_MESSGE_TYPE_METHOD)
    {
        return (sis_method_define *)unit->value;
    }
    return NULL;
}
// 用户自定义结构体 如果 sis_free_define = NULL 不释放
void *sis_message_get(s_sis_message *msg_, const char *key_)
{
    s_sis_message_unit *unit = _sis_message_get(msg_, key_);
    if (unit && unit->style == SIS_MESSGE_TYPE_OWNER)
    {
        return unit->value;
    }
    return NULL;
}

#if 0
// test speed
#include "sis_math.h"
#include "sis_time.h"
int main()
{
    const char *keys[10] = {
        "sh000001", "sh000011","sh001001","sh010001","sh100001",
        "sz000001", "sz000011","sz001001","sz010001","sz100001",
    };
    sis_init_random();
    int count = 1000*1000;
    int mod = count/10;
    msec_t nowmsec = sis_time_get_now_msec();
    for (int i = 0; i < count; i++)
    {
        int index = sis_int_random(0,9);
        if (i%mod==0)
        {
            printf("%10d : %10d : %s\n", i, index, keys[index]);
        }
    }
    printf("cost : %lld\n", sis_time_get_now_msec() - nowmsec);

    s_sis_message *msg = sis_message_create();
    for (int i = 0; i < 10; i++)
    {
        sis_message_set(msg, keys[i], (void *)keys[i], NULL);
    }
    nowmsec = sis_time_get_now_msec();
    for (int i = 0; i < count; i++)
    {
        const char *key = sis_message_get(msg, keys[i%10]);
        if (i%mod==0)
        {
            printf("%10d : %s\n", i, key);
        }
    }
    printf("cost : %lld\n", sis_time_get_now_msec() - nowmsec);
    sis_message_destroy(msg);
    return 0;
}
#endif