//******************************************************
// Copyright (C) 2018, coollyer <seerlinecoin@gmail.com>
//*******************************************************

#include "sisdb_io.h"
#include "sis_net.msg.h"
#include "sisdb_collect.h"

s_sis_sds sisdb_one_get_sds(s_sisdb_cxt *sisdb_, const char *key_, uint16 *format_, s_sis_sds argv_)
{
    s_sis_sds o = NULL;
    s_sisdb_collect *collect = sis_map_pointer_get(sisdb_->work_keys, key_);
    if (collect && collect->obj )
    {
        *format_ = collect->style == SISDB_COLLECT_TYPE_CHARS ? SISDB_FORMAT_CHARS : SISDB_FORMAT_BYTES;
        o = sis_sdsdup(SIS_OBJ_SDS(collect->obj));
    }
    return o;
}

// 多单键取值只处理字符串 非字符串不处理
s_sis_sds sisdb_one_gets_sds(s_sisdb_cxt *sisdb_, const char *keys_, s_sis_sds argv_)
{
    s_sis_json_node *jone = sis_json_create_object();
    if (!sis_strcasecmp(keys_, "*"))
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->work_keys);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
            if (collect->style == SISDB_COLLECT_TYPE_CHARS)
            {
                sis_json_object_add_string(jone, sis_dict_getkey(de), SIS_OBJ_GET_CHAR(collect->obj), SIS_OBJ_GET_SIZE(collect->obj));
            }
        }
        sis_dict_iter_free(di);
    }
    else
    {
        s_sis_string_list *klist = sis_string_list_create();
        sis_string_list_load(klist, keys_, sis_strlen(keys_), ",");

        int count = sis_string_list_getsize(klist);
        for (int i = 0; i < count; i++)
        {
            const char *key = sis_string_list_get(klist, i);
            s_sisdb_collect *collect = sis_map_pointer_get(sisdb_->work_keys, key);
            if (collect && collect->style == SISDB_COLLECT_TYPE_CHARS)
            {
                sis_json_object_add_string(jone, key, SIS_OBJ_GET_CHAR(collect->obj), SIS_OBJ_GET_SIZE(collect->obj));
            }
        }
        sis_string_list_destroy(klist);
    }
    s_sis_sds o = sis_json_to_sds(jone, false);
    sis_json_delete_node(jone);
    return o;
}

int sisdb_one_del(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds argv_)
{
    s_sisdb_collect *collect = sis_map_pointer_get(sisdb_->work_keys, key_);
    if (collect && collect->style != SISDB_COLLECT_TYPE_TABLE)
    {
        sis_map_pointer_del(sisdb_->work_keys, key_);
        return 1;
    }
    return 0;
}
int sisdb_one_dels(s_sisdb_cxt *sisdb_, const char *keys_, s_sis_sds argv_)
{
    int o = 0;
    if (!sis_strcasecmp(keys_, "*"))
    {
        // sis_map_pointer_clear(sisdb_->work_keys);
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->work_keys);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
            if (collect->style == SISDB_COLLECT_TYPE_CHARS || collect->style == SISDB_COLLECT_TYPE_BYTES)
            {
                sis_map_pointer_del(sisdb_->work_keys, sis_dict_getkey(de));
                o++;
            }
        }
        sis_dict_iter_free(di);
    }
    else
    {
        s_sis_string_list *klist = sis_string_list_create();
        sis_string_list_load(klist, keys_, sis_strlen(keys_), ",");

        int count = sis_string_list_getsize(klist);
        for (int i = 0; i < count; i++)
        {
            sis_map_pointer_del(sisdb_->work_keys, sis_string_list_get(klist, i));
            o++;
        }
        sis_string_list_destroy(klist);
    }
    return o;
}

int sisdb_one_set(s_sisdb_cxt *sisdb_, const char *key_, uint8 style_, s_sis_sds argv_)
{
    s_sisdb_collect *collect = sisdb_kv_create(style_, key_, argv_, sis_sdslen(argv_));
    sis_map_pointer_set(sisdb_->work_keys, key_, collect);
    return 0;
}

//////////////////////////
// 设置标准的 sdb
/////////////////////////


// 为保证最快速度，尽量不加参数
// 默认返回最后不超过 64K 的数据，以json格式
s_sis_sds sisdb_get_sds(s_sisdb_cxt *sisdb_, const char *key_, uint16 *format_, s_sis_sds argv_)
{
    s_sis_sds o = NULL;    
    s_sisdb_collect *collect = sisdb_get_collect(sisdb_, key_);  
    if (collect)
    {
        if (!argv_)
        {
            *format_ = SISDB_FORMAT_CHARS;
            o = sisdb_collect_fastget_sds(collect, key_, SISDB_FORMAT_CHARS);
        }
        else
        {
	        s_sis_json_handle *handle = sis_json_load(argv_, sis_sdslen(argv_));
            if (handle)
            {
                int iformat = sis_db_get_format_from_node(handle->node, SISDB_FORMAT_JSON);
                *format_ = iformat & SISDB_FORMAT_CHARS ? SISDB_FORMAT_CHARS : SISDB_FORMAT_BYTES;
                o = sisdb_collect_get_sds(collect, key_, iformat, handle->node);           
                sis_json_close(handle);
            }
            else
            {
                *format_ = SISDB_FORMAT_CHARS;
                o = sisdb_collect_fastget_sds(collect, key_, SISDB_FORMAT_CHARS);
            }
            
        } 
    }
    return o;
}

int _sisdb_get_filter(s_sisdb_cxt *sisdb_, s_sis_string_list *list_, const char *keys_, const char *sdbs_)
{
    sis_string_list_clear(list_);
    if (!sis_strcasecmp(sdbs_, "*") && !sis_strcasecmp(keys_, "*"))
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->work_keys);
        while ((de = sis_dict_next(di)) != NULL)
        {
            sis_string_list_push(list_, sis_dict_getkey(de), sis_strlen(sis_dict_getkey(de)));
        }
        sis_dict_iter_free(di);
        return sis_string_list_getsize(list_);
    }
    
    if (!sis_strcasecmp(keys_, "*"))
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->work_keys);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
            if (collect->style == SISDB_COLLECT_TYPE_TABLE && 
                sis_str_subcmp_strict(collect->sdb->name,  sdbs_, ',') >= 0)
            {
                sis_string_list_push(list_, sis_dict_getkey(de), sis_strlen(sis_dict_getkey(de)));
            }
        }
        sis_dict_iter_free(di);
    }
    else
    {
        s_sis_string_list *slists = sis_string_list_create();
        if (!sis_strcasecmp(sdbs_, "*"))
        {
            int count = sis_map_list_getsize(sisdb_->work_sdbs);
            for (int i = 0; i < count; i++)
            {
                s_sisdb_table *table = sis_map_list_geti(sisdb_->work_sdbs, i);
                sis_string_list_push(slists, table->name, sis_sdslen(table->name));
            }  
        }
        else
        {
            sis_string_list_load(slists, sdbs_, sis_strlen(sdbs_), ",");  
        }

        s_sis_string_list *klists = sis_string_list_create();
        sis_string_list_load(klists, keys_, sis_strlen(keys_), ",");  
        int kcount = sis_string_list_getsize(klists);
        int scount = sis_string_list_getsize(slists);
        char key[128];
        for (int k = 0; k < kcount; k++)
        {
            for (int i = 0; i < scount; i++)
            {
                sis_sprintf(key, 128, "%s.%s", sis_string_list_get(klists, k), sis_string_list_get(slists, i));
                sis_string_list_push(list_, key, sis_strlen(key));
            }         
        }
        sis_string_list_destroy(klists);
        sis_string_list_destroy(slists);
    }
    return sis_string_list_getsize(list_);
}

// 多键取值只返回字符串 并且只返回最后一条记录
s_sis_sds sisdb_gets_sds(s_sisdb_cxt *sisdb_, const char *keys_,const  char *sdbs_, s_sis_sds argv_)
{
    s_sis_string_list *collects = sis_string_list_create();

    int count = _sisdb_get_filter(sisdb_, collects, keys_, sdbs_);

    s_sis_json_handle *handle = NULL;
    if (argv_)
    {
        handle = sis_json_load(argv_, sis_sdslen(argv_));
    } 
    
    s_sis_json_node *jone = sis_json_create_object();
    for (int i = 0; i < count; i++)
    {
        const char *key = sis_string_list_get(collects, i);
        s_sisdb_collect *collect = sis_map_pointer_get(sisdb_->work_keys, key);
        if (collect)
        {
            s_sis_json_node *jval = sisdb_collects_get_last_node(collect, handle ? handle->node : NULL);
            if (jval)
            {
                sis_json_object_add_node(jone, key, jval);
            }
        }
    }
    sis_string_list_destroy(collects);
    if (handle)
    {
        sis_json_close(handle);
    }

    s_sis_sds o = sis_json_to_sds(jone, true);
    sis_json_delete_node(jone);
    return o;
}

// 只获取单键数值
s_sis_sds sisdb_one_keys_sds(s_sisdb_cxt *sisdb_, const char *keys_)
{
    s_sis_sds o = NULL;
    s_sis_dict_entry *de;
    s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->work_keys);
    while ((de = sis_dict_next(di)) != NULL)
    {
        s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
        if (collect->style != SISDB_COLLECT_TYPE_TABLE)
        {
            if (!o)
            {
                o = sis_sdsnew(sis_dict_getkey(de));
            }
            else
            {
                o = sis_sdscatfmt(o, ",%s", sis_dict_getkey(de));
            }
        }
    }
    sis_dict_iter_free(di);
    return o;
}
s_sis_sds sisdb_keys_sds(s_sisdb_cxt *sisdb_, const char *keys_, s_sis_sds argv_)
{
    s_sis_map_int *keys = sis_map_int_create();

    {
        char kname[128], sname[128]; 
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->work_keys);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
            // printf("%s\n", (char *)sis_dict_getkey(de));
            if (collect->style == SISDB_COLLECT_TYPE_TABLE)
            {
                int cmds = sis_str_divide(sis_dict_getkey(de), '.', kname, sname);
                if (cmds == 2)
                {
                    if (sis_map_int_get(keys, kname) == -1)
                    {
                        sis_map_int_set(keys, kname, 1);
                    }
                }
            }
        }
        sis_dict_iter_free(di);
    }
    s_sis_sds o = NULL;
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(keys);
        while ((de = sis_dict_next(di)) != NULL)
        {
            if (!o)
            {
                o = sis_sdsnew(sis_dict_getkey(de));
            }
            else
            {
                o = sis_sdscatfmt(o, ",%s", sis_dict_getkey(de));
            }
        }
        sis_dict_iter_free(di);
    }
    sis_map_int_destroy(keys);

    return o;
}
// 必须带参数 否则不执行删除操作
int sisdb_del(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds argv_)
{
    // printf("sisdb_del : %s, %s\n",key_, argv_);
    s_sisdb_collect *collect = sisdb_get_collect(sisdb_, key_);  
    if (!collect || !argv_)
    {
        return -1;
    }
    s_sis_json_handle *handle = sis_json_load(argv_, sis_sdslen(argv_));
    if (!handle)
    {
        return -2;
    }
    // 返回剩余的记录个数
    sisdb_collect_delete(collect, handle->node);
    sis_json_close(handle);
    if (SIS_OBJ_LIST(collect->obj)->count == 0)
    {
        // 没有数据 就直接删除key
        sis_map_pointer_del(sisdb_->work_keys, key_);
    }
    return 0;
}
int sisdb_dels(s_sisdb_cxt *sisdb_, const char *keys_, const char *sdbs_, s_sis_sds argv_)
{
    s_sis_string_list *collects = sis_string_list_create();
    int count = _sisdb_get_filter(sisdb_, collects, keys_, sdbs_);
    for (int i = 0; i < count; i++)
    {
        sisdb_del(sisdb_, sis_string_list_get(collects, i), argv_);
    }
    sis_string_list_destroy(collects);
    return 0;    
}

int sisdb_set_bytes(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds value_)
{
    if(!value_ || sis_sdslen(value_) < 1)
    {
        return -1;
    }
    s_sisdb_collect *collect = sisdb_get_collect(sisdb_, key_);
    if (!collect)
    {
        collect = sisdb_collect_create(sisdb_, key_);
        if (!collect)
        {
            return -2;
        }    
    }
    if (sisdb_collect_update(collect, value_) <= 0)
    {
        return -5;
    }

    return 0;
}

int sisdb_set_chars(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds value_)
{
    if(!value_ || sis_sdslen(value_) < 1 || (value_[0] != '{' && value_[0] != '['))
    {
        return -1;
    }
    // 数据转为 二进制后
    s_sisdb_collect *collect = sisdb_get_collect(sisdb_, key_);
    if (!collect)
    {
        collect = sisdb_collect_create(sisdb_, key_);
        if (!collect)
        {
            return -2;
        }    
    }
    // 到这里已经有了collect
    s_sis_sds bytes = NULL;
    if (value_[0] == '{')
    {
        bytes = sisdb_collect_json_to_struct_sds(collect, value_);
    }
    if (value_[0] == '[')
    {
        bytes = sisdb_collect_array_to_struct_sds(collect, value_);
    }
    if (!bytes)
    {
        return -3;
    }
    // printf("%p\n",bytes);
    // sis_out_binary("bytes:",  bytes, sis_sdslen(bytes));

    if (sisdb_collect_update(collect, bytes) <= 0)
    {
        return -5;
    }

    sis_sdsfree(bytes);
    return 0;
}

