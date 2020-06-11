
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
        sis_sdsfree((s_sis_sds)unit->value);
        break;
    case SIS_MESSGE_TYPE_STRLIST:
        sis_string_list_destroy(unit->value);
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
    s_sis_message *o = SIS_MALLOC(s_sis_message, o);
    o->maps = sis_map_pointer_create_v(_sis_message_unit_free);
    return o;
}

void sis_message_destroy(s_sis_message *msg_)
{
    sis_map_pointer_destroy(msg_->maps); 
    sis_free(msg_);
}

void sis_message_del(s_sis_message *msg_, const char *key_)
{
    sis_map_pointer_del(msg_->maps, key_);
}

void sis_message_set_int(s_sis_message *msg_, const char* key_, int64 in_)
{
    s_sis_message_unit *unit = SIS_MALLOC(s_sis_message_unit, unit);
    unit->style = SIS_MESSGE_TYPE_INT;
    unit->value = (void *)in_;
    sis_map_pointer_set(msg_->maps, key_, unit);
}
void sis_message_set_bool(s_sis_message *msg_, const char* key_, bool in_)
{
    s_sis_message_unit *unit = SIS_MALLOC(s_sis_message_unit, unit);
    unit->style = SIS_MESSGE_TYPE_BOOL;
    unit->value = NULL;
    if (in_) unit->value = (void *)1;
    sis_map_pointer_set(msg_->maps, key_, unit);
}
void sis_message_set_double(s_sis_message *msg_, const char* key_, double in_)
{
    s_sis_message_unit *unit = SIS_MALLOC(s_sis_message_unit, unit);
    unit->style = SIS_MESSGE_TYPE_DOUBLE;
    uint64 in;
    memmove(&in, &in_, sizeof(uint64));
    unit->value = (void *)in;
    sis_map_pointer_set(msg_->maps, key_, unit);
}
void sis_message_set_str(s_sis_message *msg_, const char* key_, char *in_, size_t ilen_)
{
    s_sis_message_unit *unit = SIS_MALLOC(s_sis_message_unit, unit);
    unit->style = SIS_MESSGE_TYPE_SDS;
    unit->value = sdsnewlen(in_, ilen_);
    sis_map_pointer_set(msg_->maps, key_, unit);
}
// void sis_message_set_strlist(s_sis_message *msg_, const char* key_, s_sis_string_list *list_)
// {
//     s_sis_message_unit *unit = SIS_MALLOC(s_sis_message_unit, unit);
//     unit->style = SIS_MESSGE_TYPE_STRLIST;
//     unit->value = list_;
//     sis_map_pointer_set(msg_->maps, key_, unit);
// }

void sis_message_set_method(s_sis_message *msg_, const char *key_, sis_method_define *method_)
{
    s_sis_message_unit *unit = SIS_MALLOC(s_sis_message_unit, unit);
    unit->style = SIS_MESSGE_TYPE_METHOD;
    unit->value = method_;
    sis_map_pointer_set(msg_->maps, key_, unit);
}

// 用户自定义结构体 如果 sis_free_define = NULL 默认调用 sis_free
void sis_message_set(s_sis_message *msg_, const char *key_, void *in_, sis_free_define *free_)
{
    s_sis_message_unit *unit = SIS_MALLOC(s_sis_message_unit, unit);
    unit->style = SIS_MESSGE_TYPE_OWNER;
    unit->value = in_;
    unit->free = free_;
    sis_map_pointer_set(msg_->maps, key_, unit);
}    
int64 sis_message_get_int(s_sis_message *msg_, const char *key_)
{
    s_sis_message_unit *unit = (s_sis_message_unit *)sis_map_pointer_get(msg_->maps, key_);
    if (unit && unit->style == SIS_MESSGE_TYPE_INT)
    {
        return (int64)unit->value;
    }
    return 0;
}
bool sis_message_get_bool(s_sis_message *msg_, const char *key_)
{
    s_sis_message_unit *unit = (s_sis_message_unit *)sis_map_pointer_get(msg_->maps, key_);
    if (unit && unit->style == SIS_MESSGE_TYPE_BOOL)
    {
        return unit->value ? true : false;
    }
    return false;
}
double sis_message_get_double(s_sis_message *msg_, const char *key_)
{
    s_sis_message_unit *unit = (s_sis_message_unit *)sis_map_pointer_get(msg_->maps, key_);
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
    s_sis_message_unit *unit = (s_sis_message_unit *)sis_map_pointer_get(msg_->maps, key_);
    if (unit && unit->style == SIS_MESSGE_TYPE_SDS)
    {
        return (s_sis_sds)unit->value;
    }
    return NULL;
}
// s_sis_string_list *sis_message_get_strlist(s_sis_message *msg_, const char *key_)
// {
//     s_sis_message_unit *unit = (s_sis_message_unit *)sis_map_pointer_get(msg_->maps, key_);
//     if (unit && unit->style == SIS_MESSGE_TYPE_STRLIST)
//     {
//         return (s_sis_string_list *)unit->value;
//     }
//     return NULL;
// }
sis_method_define *sis_message_get_method(s_sis_message *msg_, const char *key_)
{
    s_sis_message_unit *unit = (s_sis_message_unit *)sis_map_pointer_get(msg_->maps, key_);
    if (unit && unit->style == SIS_MESSGE_TYPE_METHOD)
    {
        return (sis_method_define *)unit->value;
    }
    return NULL;
}
// 用户自定义结构体 如果 sis_free_define = NULL 默认调用 sis_free
void *sis_message_get(s_sis_message *msg_, const char *key_)
{
    s_sis_message_unit *unit = (s_sis_message_unit *)sis_map_pointer_get(msg_->maps, key_);
    if (unit && unit->style == SIS_MESSGE_TYPE_OWNER)
    {
        return unit->value;
    }
    return NULL;
}

