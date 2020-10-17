//******************************************************
// Copyright (C) 2018, coollyer <seerlinecoin@gmail.com>
//*******************************************************

#include "sisdb_io.h"
#include "sis_net.io.h"
#include "sisdb_collect.h"
#include "sis_disk.h"

int sis_str_divide_sds(const char *in_, char ch_, s_sis_sds *one_,  s_sis_sds *two_)
{
    const char *start = in_;
	const char *ptr = in_;
    int count = 0;
	while (ptr && *ptr)
	{
		if (*ptr == ch_)
		{
            *one_ = count > 0 ? sis_sdsnewlen(start, count) : NULL;
			ptr++;
			start = ptr;
            count = 0;
			while (*ptr)
			{
				ptr++;
                count++;
                
			}
            *two_ = count > 0 ? sis_sdsnewlen(start, count) : NULL;
			return 2;
		}
		ptr++;
        count++;
	}
    *one_ = count > 0 ? sis_sdsnewlen(start, count) : NULL;
	return 1;
}

s_sis_sds sisdb_one_get_sds(s_sisdb_cxt *sisdb_, const char *key_, uint16 *format_, s_sis_sds argv_)
{
    s_sis_sds o = NULL;
    s_sisdb_collect *collect = sis_map_pointer_get(sisdb_->collects, key_);
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
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->collects);
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
            s_sisdb_collect *collect = sis_map_pointer_get(sisdb_->collects, key);
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
    s_sisdb_collect *collect = sis_map_pointer_get(sisdb_->collects, key_);
    if (collect && collect->style != SISDB_COLLECT_TYPE_TABLE)
    {
        sis_map_pointer_del(sisdb_->collects, key_);
        return 1;
    }
    return 0;
}
int sisdb_one_dels(s_sisdb_cxt *sisdb_, const char *keys_, s_sis_sds argv_)
{
    int o = 0;
    if (!sis_strcasecmp(keys_, "*"))
    {
        // sis_map_pointer_clear(sisdb_->collects);
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->collects);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
            if (collect->style == SISDB_COLLECT_TYPE_CHARS || collect->style == SISDB_COLLECT_TYPE_BYTES)
            {
                sis_map_pointer_del(sisdb_->collects, sis_dict_getkey(de));
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
            sis_map_pointer_del(sisdb_->collects, sis_string_list_get(klist, i));
            o++;
        }
        sis_string_list_destroy(klist);
    }
    return o;
}

int sisdb_one_set(s_sisdb_cxt *sisdb_, const char *key_, uint8 style_, s_sis_sds argv_)
{
    s_sisdb_collect *collect = sisdb_kv_create(style_, key_, argv_, sis_sdslen(argv_));
    sis_map_pointer_set(sisdb_->collects, key_, collect);
    // 这里处理订阅
    sisdb_make_sub_message(sisdb_, collect, style_, argv_);
    return 0;
}

//////////////////////////
// 设置标准的 sdb
/////////////////////////

int sis_from_node_get_format(s_sis_json_node *node_, int default_)
{
	int o = default_;
	s_sis_json_node *node = sis_json_cmp_child_node(node_, "format");
	if (node && sis_strlen(node->value) > 0)
	{
        char ch = node->value[0];
        switch (ch)
        {
        case 'z':  // bitzip "zip"
        case 'Z':
            o = SISDB_FORMAT_BITZIP;
            break;
        case 's':  // struct 
        case 'S':
            o = SISDB_FORMAT_STRUCT;
            break;
        case 'b':  // bytes
        case 'B':
            // o = SISDB_FORMAT_BYTES;
            o = SISDB_FORMAT_BUFFER;
            break;
        case 'j':
        case 'J':
            o = SISDB_FORMAT_JSON;
            break;
        case 'a':
        case 'A':
            o = SISDB_FORMAT_ARRAY;
            break;
        case 'c':
        case 'C':
            o = SISDB_FORMAT_CSV;
            break;        
        default:
            o = SISDB_FORMAT_STRING;
            break;
        }    
	}
	return o;
}

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
                int iformat = sis_from_node_get_format(handle->node, SISDB_FORMAT_JSON);
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
// 从磁盘中获取数据
s_sis_sds sisdb_disk_get_sno_sds(s_sisdb_cxt *sisdb_, const char *key_, int iformat_, int workdate_, s_sis_json_node *node_)
{
    char fn[32];
    sis_llutoa(workdate_, fn, 32, 10);
    char work_path[512];
    sis_sprintf(work_path, 512, "%s%s/", sisdb_->fast_path, sisdb_->name);
    s_sis_disk_class *read_class = sis_disk_class_create(); 
    if (sis_disk_class_init(read_class, SIS_DISK_TYPE_SNO, work_path, fn) 
        || sis_disk_file_read_start(read_class))
    {
        sis_disk_class_destroy(read_class);
        return NULL;
    }
    s_sis_sds keyn = NULL; s_sis_sds sdbn = NULL; 
    int cmds = sis_str_divide_sds(key_, '.', &keyn, &sdbn);
    if (cmds < 2)
    {
        sis_disk_class_destroy(read_class);
        return NULL;
    }
    s_sisdb_table *tb = sis_map_list_get(sisdb_->sdbs, sdbn);

    s_sis_disk_reader *reader = sis_disk_reader_create(NULL);
    // reader->issub = 0;
    // printf("%s| %s | %s\n", context->read_cb->sub_keys, context->work_sdbs, context->read_cb->sub_sdbs ? context->read_cb->sub_sdbs : "nil");
    sis_disk_reader_set_sdb(reader, sdbn);
    sis_disk_reader_set_key(reader, keyn); 
    sis_sdsfree(keyn); sis_sdsfree(sdbn);

    // sub 是一条一条的输出
    // get 是所有符合条件的一次性输出
    s_sis_object *obj = sis_disk_file_read_get_obj(read_class, reader);
    printf("obj = %p\n",obj);
    sis_disk_reader_destroy(reader);

    sis_disk_file_read_stop(read_class);
    sis_disk_class_destroy(read_class); 

    if (obj)
    {
        const char *fields = NULL;
        if (node_ && sis_json_cmp_child_node(node_, "fields"))
        {
            fields = sis_json_get_str(node_, "fields");
        }
        // printf(" %s iformat_ = %x %d\n", key_, iformat_, obj->style);
        // sis_out_binary("sno", SIS_OBJ_GET_CHAR(obj),SIS_OBJ_GET_SIZE(obj));
        if (iformat_ == SISDB_FORMAT_BYTES)
        {
            bool iswhole = sisdb_field_is_whole(fields);
            if (iswhole)
            {
                s_sis_sds o = sis_sdsnewlen(SIS_OBJ_GET_CHAR(obj),SIS_OBJ_GET_SIZE(obj));
                sis_object_destroy(obj);
                return o;
            }
            s_sis_string_list *field_list = sis_string_list_create();
            sis_string_list_load(field_list, fields, sis_strlen(fields), ",");
            s_sis_sds other = sisdb_collect_struct_to_sds(tb->db, SIS_OBJ_GET_CHAR(obj),SIS_OBJ_GET_SIZE(obj), field_list);
            sis_string_list_destroy(field_list);
            return other;
        }
        s_sis_sds o = sisdb_get_chars_format_sds(tb, key_, iformat_, SIS_OBJ_GET_CHAR(obj),SIS_OBJ_GET_SIZE(obj), fields);
        sis_object_destroy(obj);
        return o;
    }
    return NULL;
}

// 获取sno的数据
s_sis_sds sisdb_get_sno_sds(s_sisdb_cxt *sisdb_, const char *key_, uint16 *format_, s_sis_sds argv_)
{
    s_sis_sds o = NULL;    
    s_sisdb_collect *collect = sisdb_get_collect(sisdb_, key_);  
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
            // 带参数 
            int iformat = sis_from_node_get_format(handle->node, SISDB_FORMAT_JSON);
            *format_ = iformat & SISDB_FORMAT_CHARS ? SISDB_FORMAT_CHARS : SISDB_FORMAT_BYTES;
            // 找 date 字段  
            date_t workdate = sis_json_get_int(handle->node, "date", sisdb_->work_date);
            printf("----- %d %d\n", workdate, sisdb_->work_date);
            if (workdate < sisdb_->work_date)
            {
                // 从磁盘中获取数据
                o = sisdb_disk_get_sno_sds(sisdb_, key_, iformat, workdate, handle->node); 
            }
            else
            {
                o = sisdb_collect_get_sds(collect, key_, iformat, handle->node);                          
            }
            sis_json_close(handle);
        }
        else
        {
            *format_ = SISDB_FORMAT_CHARS;
            o = sisdb_collect_fastget_sds(collect, key_, SISDB_FORMAT_CHARS);
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
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->collects);
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
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->collects);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
            if (collect->style == SISDB_COLLECT_TYPE_TABLE && 
                sis_str_subcmp_strict(collect->sdb->db->name,  sdbs_, ',') >= 0)
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
            int count = sis_map_list_getsize(sisdb_->sdbs);
            for (int i = 0; i < count; i++)
            {
                s_sisdb_table *tb = sis_map_list_geti(sisdb_->sdbs, i);
                sis_string_list_push(slists, tb->db->name, sis_sdslen(tb->db->name));
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
        s_sisdb_collect *collect = sis_map_pointer_get(sisdb_->collects, key);
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

    s_sis_sds o = sis_json_to_sds(jone, false);
    sis_json_delete_node(jone);
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
        sis_map_pointer_del(sisdb_->collects, key_);
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
    // 到这里已经有了collect
    int o = 0;
    if (collect->sdb->style == SISDB_TB_STYLE_SNO)
    {
        o = sisdb_collect_wseries(collect, value_);
    }
    else
    {
        o = sisdb_collect_update(collect, value_);
    }
    
    if (o <= 0)
    {
        return -5;
    }
    // 这里处理订阅
    sisdb_make_sub_message(sisdb_, collect, SISDB_COLLECT_TYPE_BYTES, value_);

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
    // sis_out_binary("bytes:",  bytes, sis_sdslen(bytes));

    int o = 0;
    if (collect->sdb->style == SISDB_TB_STYLE_SNO)
    {
        o = sisdb_collect_wseries(collect, bytes);
    }
    else
    {
        o = sisdb_collect_update(collect, bytes);
    }    
    // printf("count = %d, %s\n", o, bytes ? bytes : "nil");

    if (o <= 0)
    {
        return -5;
    }
    // 这里处理订阅
    sisdb_make_sub_message(sisdb_, collect, SISDB_COLLECT_TYPE_BYTES, bytes);

    sis_sdsfree(bytes);
    return 0;
}
/////////////////
//  sub function
/////////////////
int sisdb_get_format(s_sis_sds argv_)
{
    int iformat = SISDB_FORMAT_CHARS;
    s_sis_json_handle *handle = argv_ ? sis_json_load(argv_, sis_sdslen(argv_)) : NULL;
    if (handle)
    {
        iformat = sis_from_node_get_format(handle->node, SISDB_FORMAT_JSON);
        sis_json_close(handle);
    }
    return iformat;
}
void _send_sub_message(s_sisdb_cxt *sisdb_, s_sis_net_message *info_, s_sisdb_collect *collect_, uint8 style_, const char *in_, size_t ilen_)
{
    // 赋新值
    s_sis_net_message *newinfo = sis_net_message_create();
    newinfo->cid = info_->cid;
    newinfo->source = sis_sdsdup(info_->source);
    newinfo->key = sis_sdsdup(collect_->name);

    if(info_->rfmt & SISDB_FORMAT_CHARS)
    {
        if(style_ == SISDB_COLLECT_TYPE_BYTES)
        {
            if(collect_->sdb)
            {
                // 只有数据表才能转格式
                s_sis_sds out = sisdb_get_chars_format_sds(collect_->sdb, collect_->name, info_->rfmt, in_, ilen_, NULL);
                sis_net_ans_with_chars(newinfo, out, sis_sdslen(out));
                sis_sdsfree(out); 
            }
            else  // 单键
            {
                // 返回数据类型不正确
                // sis_net_ans_with_error(newinfo, "datatype error.", 15);
                sis_net_ans_with_bytes(newinfo, in_, ilen_);
            }
        }
        else
        {
            sis_net_ans_with_chars(newinfo, in_, ilen_);
        }         
    }
    else
    {
        if(style_ == SISDB_COLLECT_TYPE_BYTES)
        {
            sis_net_ans_with_bytes(newinfo, in_, ilen_);
        }
        else
        {
            // 转二进制
            sis_net_ans_with_bytes(newinfo, in_, ilen_);
        }         
    }

    sis_net_class_send(sisdb_->socket, newinfo);
    sis_net_message_destroy(newinfo); 
}

void sisdb_make_sub_message(s_sisdb_cxt *sisdb_, s_sisdb_collect *collect_, uint8 style_, s_sis_sds in_)
{
    // 先处理单键值订阅
    {
        s_sisdb_sub_info *sublist = (s_sisdb_sub_info *)sis_map_pointer_get(sisdb_->sub_single, collect_->name);
        // printf("sublist pub: %p %s\n", sublist, collect_->name);
        if (sublist && sublist->issno == (collect_->sdb->style == SISDB_TB_STYLE_SNO))
        {
            for (int i = 0; i < sublist->netmsgs->count; i++)
            {
                s_sis_net_message *info = (s_sis_net_message *)sis_pointer_list_get(sublist->netmsgs, i);
                // printf("sublist one: %d  %s  format = %d\n", sublist->netmsgs->count, info->key, info->rfmt);
                _send_sub_message(sisdb_, info, collect_, style_, in_, sis_sdslen(in_));
            }
        }
    }
    // 再处理多键值订阅 这个可能耗时 先这样处理
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->sub_multiple);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_sub_info *sublist = (s_sisdb_sub_info *)sis_dict_getval(de);

            bool is_publish = false;
            switch (sublist->subtype)
            {
            case SISDB_SUB_ONE_ALL:
            case SISDB_SUB_TABLE_ALL:
                is_publish = true;
                break;
            case SISDB_SUB_ONE_MUL:
                if (sis_str_subcmp_strict(collect_->name,  sublist->keys, ',') >= 0)
                {
                    is_publish = true;
                }
                break;            
            case SISDB_SUB_TABLE_MUL:
                {
                    // 都是小串
                    char keyn[128], sdbn[128]; 
                    int cmds = sis_str_divide(collect_->name, '.', keyn, sdbn);
                    if (cmds == 2 && sis_str_subcmp_strict(keyn,  sublist->keys, ',') >= 0
                                && sis_str_subcmp_strict(sdbn,  sublist->sdbs, ',') >= 0)
                    {
                        is_publish = true;
                    }
                }
                break;            
            case SISDB_SUB_TABLE_KEY:
                {
                    char keyn[128], sdbn[128]; 
                    int cmds = sis_str_divide(collect_->name, '.', keyn, sdbn);
                    if (cmds == 2 && sis_str_subcmp_strict(keyn,  sublist->keys, ',') >= 0)
                    {
                        is_publish = true;
                    }
                }
                break;            
            case SISDB_SUB_TABLE_SDB:
                {
                    char keyn[128], sdbn[128]; 
                    int cmds = sis_str_divide(collect_->name, '.', keyn, sdbn);
                    if (cmds == 2 && sis_str_subcmp_strict(sdbn,  sublist->sdbs, ',') >= 0)
                    {
                        is_publish = true;
                    }
                }
                break;            
            default:
                break;
            }
            if (is_publish && sublist->issno == (collect_->sdb->style == SISDB_TB_STYLE_SNO))
            {
                for (int i = 0; i < sublist->netmsgs->count; i++)
                {
                    s_sis_net_message *info = (s_sis_net_message *)sis_pointer_list_get(sublist->netmsgs, i);
                    // printf("sublist sdb: %d  %s  format = %d\n", sublist->netmsgs->count, info->key, info->rfmt);
                    _send_sub_message(sisdb_, info, collect_, style_, in_, sis_sdslen(in_));
                }
            }
        }
        sis_dict_iter_free(di);
    }    
}


int sisdb_one_sub(s_sisdb_cxt *sisdb_, s_sis_net_message *netmsg_, bool issno_)
{
	s_sisdb_sub_info *sublist = (s_sisdb_sub_info *)sis_map_pointer_get(sisdb_->sub_single, netmsg_->key);
	if (!sublist)
	{
        sublist = sisdb_sub_info_create(netmsg_);
		sis_map_pointer_set(sisdb_->sub_single, netmsg_->key, sublist);
	}
    sublist->issno = issno_;
	bool isnew = true;
	for (int i = 0; i < sublist->netmsgs->count; i++)
	{
		s_sis_net_message *info = (s_sis_net_message *)sis_pointer_list_get(sublist->netmsgs, i);
		if (info->cid ==  netmsg_->cid)
		{
            info->rfmt = sisdb_get_format(netmsg_->val);
        	isnew = false;
			break;
		}
	}
	if (isnew)
	{
		sis_net_message_incr(netmsg_);
        netmsg_->rfmt = sisdb_get_format(netmsg_->val);
    // printf("sublist sub: %s [%d] format = %d, %d, %d\n",netmsg_->key, isnew, SISDB_FORMAT_CHARS, netmsg_->rfmt, sisdb_get_format(netmsg_->val));
		sis_pointer_list_push(sublist->netmsgs, netmsg_);
	}
    // printf("sublist sub: %s [%d] format = %d, %d, %d\n",netmsg_->key, isnew, SISDB_FORMAT_CHARS, netmsg_->rfmt, sisdb_get_format(netmsg_->val));
    return isnew ? 1 : 0;
}
int _sisdb_unsub_map_list(s_sis_map_pointer *map_, int cid_, bool issno_)
{
    int count = 0;
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(map_);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_sub_info *sublist = (s_sisdb_sub_info *)sis_dict_getval(de);
            if ((issno_ && !sublist->issno) || (!issno_ && sublist->issno))
            {
                continue;
            }
            int i = 0;
            while (i < sublist->netmsgs->count) 
            {
                s_sis_net_message *info = (s_sis_net_message *)sis_pointer_list_get(sublist->netmsgs, i);
                if (info->cid == cid_)
                {
                    sis_pointer_list_delete(sublist->netmsgs, i, 1);
                    count++;
                    if (sublist->netmsgs->count == 0)
                    {
                        sis_map_pointer_del(map_, sis_dict_getkey(de));
                    }
                    break;
                }
                else
                {
                    i++;
                }
            }
        }
        sis_dict_iter_free(di);
    }
    return count;
}
int sisdb_unsub_whole(s_sisdb_cxt *sisdb_, int cid_, bool issno_)
{
    int count = 0;
    
    if (!issno_)
    {
        count += _sisdb_unsub_map_list(sisdb_->sub_single, cid_, false);
    }

    count += _sisdb_unsub_map_list(sisdb_->sub_multiple, cid_, issno_);

    return count;
}

int sisdb_one_unsub(s_sisdb_cxt *sisdb_, s_sis_net_message *netmsg_, bool issno_)
{
	s_sisdb_sub_info *sublist = (s_sisdb_sub_info *)sis_map_pointer_get(sisdb_->sub_single, netmsg_->key);
    if (sublist && sublist->issno == issno_)
    {
        int i = 0;
        while (i < sublist->netmsgs->count) 
        {
            s_sis_net_message *info = (s_sis_net_message *)sis_pointer_list_get(sublist->netmsgs, i);
            if (info->cid == netmsg_->cid)
            {
                sis_pointer_list_delete(sublist->netmsgs, i, 1);
                if (sublist->netmsgs->count == 0)
                {
                    sis_map_pointer_del(sisdb_->sub_multiple, netmsg_->key);
                }
                break;
            }
            else
            {
                i++;
            }
        }
        return 1;
    }
    return 0 ;
}

int sisdb_multiple_sub(s_sisdb_cxt *sisdb_, s_sis_net_message *netmsg_, bool issno_)
{
	s_sisdb_sub_info *sublist = (s_sisdb_sub_info *)sis_map_pointer_get(sisdb_->sub_multiple, netmsg_->key);
	if (!sublist)
	{
        sublist = sisdb_sub_info_create(netmsg_);
		sis_map_pointer_set(sisdb_->sub_multiple, netmsg_->key, sublist);
	}
    sublist->issno = issno_;
    // printf("multiple_sub: %s\n",netmsg_->key);
	bool isnew = true;
	for (int i = 0; i < sublist->netmsgs->count; i++)
	{
		s_sis_net_message *info = (s_sis_net_message *)sis_pointer_list_get(sublist->netmsgs, i);
		if (info->cid ==  netmsg_->cid)
		{
            info->rfmt = sisdb_get_format(netmsg_->val);
			isnew = false;
			break;
		}
	}
	if (isnew)
	{
		sis_net_message_incr(netmsg_);
        netmsg_->rfmt = sisdb_get_format(netmsg_->val);
		sis_pointer_list_push(sublist->netmsgs, netmsg_);
	}
    return isnew ? 1 : 0;
}
int sisdb_multiple_unsub(s_sisdb_cxt *sisdb_, s_sis_net_message *netmsg_, bool issno_)
{
	s_sisdb_sub_info *sublist = (s_sisdb_sub_info *)sis_map_pointer_get(sisdb_->sub_multiple, netmsg_->key);
    if (sublist && sublist->issno == issno_)
    {
        int i = 0;
        while (i < sublist->netmsgs->count) 
        {
            s_sis_net_message *info = (s_sis_net_message *)sis_pointer_list_get(sublist->netmsgs, i);
            if (info->cid == netmsg_->cid)
            {
                sis_pointer_list_delete(sublist->netmsgs, i, 1);
                if (sublist->netmsgs->count == 0)
                {
                    sis_map_pointer_del(sisdb_->sub_multiple, netmsg_->key);
                }
                break;
            }
            else
            {
                i++;
            }
        }
        return 1;
    }
    return 0 ;
}

static void cb_begin(void *src, msec_t tt)
{
    s_sisdb_subsno_info *info = (s_sisdb_subsno_info *)src;
    s_sis_net_message *newinfo = sis_net_message_create();
    newinfo->cid = info->netmsg->cid;
    newinfo->source = sis_sdsdup(info->netmsg->source);

    char fn[32];
    sis_llutoa(tt, fn, 32, 10);

    sis_net_ans_with_sub_start(newinfo, fn);
    sis_net_class_send(info->sisdb->socket, newinfo); 
    sis_net_message_destroy(newinfo); 
}

static void cb_read(void *src, const char *key_, const char *sdb_, s_sis_object *obj_)
{
    s_sisdb_subsno_info *info = (s_sisdb_subsno_info *)src;
    s_sis_net_message *newinfo = sis_net_message_create();
    newinfo->cid = info->netmsg->cid;
    newinfo->source = sis_sdsdup(info->netmsg->source);
    newinfo->key = sis_sdsnew(key_);
    newinfo->key = sis_sdscatfmt(newinfo->key, ".%s", sdb_);

    if(info->netmsg->rfmt & SISDB_FORMAT_CHARS)
    {
        s_sisdb_table *tb = sis_map_list_get(info->sisdb->sdbs, sdb_);
        s_sis_sds out = sisdb_get_chars_format_sds(tb, newinfo->key, info->netmsg->rfmt, 
                SIS_OBJ_GET_CHAR(obj_), SIS_OBJ_GET_SIZE(obj_), NULL);
        sis_net_ans_with_chars(newinfo, out, sis_sdslen(out));
        sis_sdsfree(out); 
    }
    else
    {
        sis_net_ans_with_bytes(newinfo, SIS_OBJ_GET_CHAR(obj_), SIS_OBJ_GET_SIZE(obj_));
    }
    sis_net_class_send(info->sisdb->socket, newinfo);
    sis_net_message_destroy(newinfo); 
    // 这里通过回调把数据传递出去 
} 
static void cb_end(void *src, msec_t tt)
{
    s_sisdb_subsno_info *info = (s_sisdb_subsno_info *)src;
    s_sis_net_message *newinfo = sis_net_message_create();
    newinfo->cid = info->netmsg->cid;
    newinfo->source = sis_sdsdup(info->netmsg->source);

    char fn[32];
    sis_llutoa(tt, fn, 32, 10);

    sis_net_ans_with_sub_stop(newinfo, fn);
    sis_net_class_send(info->sisdb->socket, newinfo); 
    sis_net_message_destroy(newinfo); 
}
// subkey = k1,k2.db1,db2  pubkey = k1.db1
bool _subsno_filter(s_sisdb_sub_info *subinfo_, const char *pubkey_)
{
    bool  is_publish = false;
    switch (subinfo_->subtype)
    {
    case SISDB_SUB_TABLE_ALL:
        is_publish = true;
        break;
    case SISDB_SUB_TABLE_MUL:
        {
            s_sis_sds keyn = NULL; s_sis_sds sdbn = NULL; 
            int cmds = sis_str_divide_sds(pubkey_, '.', &keyn, &sdbn);
            if (cmds == 2 && sis_str_subcmp_strict(keyn,  subinfo_->keys, ',') >= 0
                        && sis_str_subcmp_strict(sdbn,  subinfo_->sdbs, ',') >= 0)
            {
                is_publish = true;
            }
            sis_sdsfree(keyn); sis_sdsfree(sdbn);
        }
        break;            
    case SISDB_SUB_TABLE_KEY:
        {
            s_sis_sds keyn = NULL; s_sis_sds sdbn = NULL; 
            int cmds = sis_str_divide_sds(pubkey_, '.', &keyn, &sdbn);
            if (cmds == 2 && sis_str_subcmp_strict(keyn,  subinfo_->keys, ',') >= 0)
            {
                is_publish = true;
            }
            sis_sdsfree(keyn); sis_sdsfree(sdbn);
        }
        break;            
    case SISDB_SUB_TABLE_SDB:
        {
            s_sis_sds keyn = NULL; s_sis_sds sdbn = NULL; 
            int cmds = sis_str_divide_sds(pubkey_, '.', &keyn, &sdbn);
            if (cmds == 2 && sis_str_subcmp_strict(sdbn,  subinfo_->sdbs, ',') >= 0)
            {
                is_publish = true;
            }
            sis_sdsfree(keyn); sis_sdsfree(sdbn);
        }
        break;            
    default:
        break;
    }
    return is_publish;
}

void *_thread_publish_realtime(void *argv_)
{
    s_sisdb_subsno_info *info = (s_sisdb_subsno_info *)argv_;

    // 临时生成一个订阅信息表
    s_sisdb_sub_info *subinfo = sisdb_sub_info_create(info->netmsg);
    subinfo->issno = true;

    int  readys = 0;
    bool send_waited = false;
    bool send_started = false;

    while(info->status != SISDB_SUBSNO_EXIT)
    {
        if (readys >= info->sisdb->series->count)
        {
            if (!send_waited && info->sisdb->series->count > 0)
            {
                s_sis_net_message *newinfo = sis_net_message_create();
                newinfo->cid = info->netmsg->cid;
                newinfo->source = sis_sdsdup(info->netmsg->source);
                char fn[32];
                sis_llutoa(info->sisdb->work_date, fn, 32, 10);
                sis_net_ans_with_sub_wait(newinfo, fn);
                sis_net_class_send(info->sisdb->socket, newinfo); 
                sis_net_message_destroy(newinfo); 
                send_waited = true;
            }
            sis_sleep(30);
            continue;
        }
        if (!send_started)
        {
            cb_begin(info, info->sisdb->work_date);
            send_started = true;
        }
        s_sisdb_collect_sno *sno = sis_node_list_get(info->sisdb->series, readys);
        // 这里先判断是否结束
        if (sno->collect == NULL && sno->recno == -1)
        {
            cb_end(info, info->sisdb->work_date);
            break;
        }
        
        if (_subsno_filter(subinfo, sno->collect->name))
        {
            s_sis_struct_list *value = SIS_OBJ_LIST(sno->collect->obj);   
            _send_sub_message(info->sisdb, info->netmsg, sno->collect, SISDB_COLLECT_TYPE_BYTES, 
                sis_struct_list_get(value, sno->recno), sno->collect->sdb->db->size);
        }
        readys++;
    }
    sisdb_sub_info_destroy(subinfo);
    // 当天数据接收完毕应该取消订阅
    sis_thread_finish(&info->thread_cxt);

    // 线程结束应该从map表中剔除
    sis_map_pointer_del(info->sisdb->subsno_worker, info->netmsg->key);
    return NULL;
}

void *_thread_publish_history(void *argv_)
{
    // 从文件中获取数据
    s_sisdb_subsno_info *info = (s_sisdb_subsno_info *)argv_;

    char fn[32];
    sis_llutoa(info->subdate, fn, 32, 10);
    char work_path[512];
    sis_sprintf(work_path, 512, "%s%s/", info->sisdb->fast_path, info->sisdb->name);
    s_sis_disk_class *read_class = sis_disk_class_create(); 
    if (sis_disk_class_init(read_class, SIS_DISK_TYPE_SNO, work_path, fn) 
        || sis_disk_file_read_start(read_class))
    {
        s_sis_net_message *newinfo = sis_net_message_create();
        newinfo->cid = info->netmsg->cid;
        newinfo->source = sis_sdsdup(info->netmsg->source);
        sis_net_ans_with_sub_stop(newinfo, fn);
        sis_net_class_send(info->sisdb->socket, newinfo); 
        sis_net_message_destroy(newinfo); 
        sis_disk_class_destroy(read_class);
        return NULL;
    }
    s_sis_sds keyn = NULL; s_sis_sds sdbn = NULL; 
    int cmds = sis_str_divide_sds(info->netmsg->key, '.', &keyn, &sdbn);
    if (cmds < 2)
    {
        sis_disk_class_destroy(read_class);
        return NULL;
    }
    s_sis_disk_callback *callback = SIS_MALLOC(s_sis_disk_callback, callback);
    callback->source = info;
    callback->cb_begin = cb_begin;
    callback->cb_read = cb_read;
    callback->cb_end = cb_end;

    s_sis_disk_reader *reader = sis_disk_reader_create(callback);
    // printf("%s| %s | %s\n", context->read_cb->sub_keys, context->work_sdbs, context->read_cb->sub_sdbs ? context->read_cb->sub_sdbs : "nil");

    sis_disk_reader_set_sdb(reader, sdbn);
    sis_disk_reader_set_key(reader, keyn);
    
    sis_sdsfree(keyn); sis_sdsfree(sdbn);

    // sub 是一条一条的输出
    sis_disk_file_read_sub(read_class, reader);
    // get 是所有符合条件的一次性输出
    // sis_disk_file_read_get(read_class, reader);
    sis_disk_reader_destroy(reader);

    sis_free(callback);

    sis_disk_file_read_stop(read_class);
    sis_disk_class_destroy(read_class); 


    sis_thread_finish(&info->thread_cxt);
    // 线程结束应该从map表中剔除
    char subkey[255];
    sis_sprintf(subkey, 255, "%s-%d", info->netmsg->key, (int)info->subdate);
    sis_map_pointer_del(info->sisdb->subsno_worker, subkey);

    return NULL;
}
int _start_subsno_worker(s_sisdb_cxt *sisdb_, s_sis_net_message *netmsg_, date_t subdate_, bool ismul_)
{
    
    if (subdate_ == 0 || subdate_ == sisdb_->work_date)
    {
        s_sisdb_subsno_info *info = sisdb_subsno_info_create(sisdb_, netmsg_);
        info->ismul = ismul_;
        sis_map_pointer_set(sisdb_->subsno_worker, netmsg_->key, info);
        // 线程号和key对应 便于取消订阅
        info->status = SISDB_SUBSNO_WORK;
        if (!sis_thread_create(_thread_publish_realtime, info, &info->thread_cxt))
        {
            LOG(1)("can't start _thread_publish_realtime.\n");
            return -1;
        }
    } else if (subdate_ < sisdb_->work_date)
    {
        s_sisdb_subsno_info *info = sisdb_subsno_info_create(sisdb_, netmsg_);
        info->ismul = ismul_;
        info->subdate = subdate_;
        char subkey[255];
        sis_sprintf(subkey, 255, "%s-%d", netmsg_->key, (int)subdate_);
        sis_map_pointer_set(sisdb_->subsno_worker, subkey, info);

        info->status = SISDB_SUBSNO_WORK;
        if (!sis_thread_create(_thread_publish_history, info, &info->thread_cxt))
        {
            LOG(1)("can't start _thread_publish_history.\n");
            return -2;
        }       
    }
    else
    {
        LOG(5)("subdate [] > today [].\n", subdate_, sisdb_->work_date);
        return -3;
    }
    return 0;
}
int sisdb_one_subsno(s_sisdb_cxt *sisdb_, s_sis_net_message *netmsg_)
{
    s_sis_json_handle *handle = netmsg_->val ? sis_json_load(netmsg_->val, sis_sdslen(netmsg_->val)) : NULL;
    int out = 1;
    if (!handle)
    {
        return sisdb_one_sub(sisdb_, netmsg_, true);
    }
    
    int    start   = sis_json_get_int(handle->node, "start", -1); // 0 从头开始 -1 从尾开始
    date_t subdate = sis_json_get_int(handle->node, "date", 0);
    if ((subdate == 0 || subdate == sisdb_->work_date) && start == -1)
    {
        // 从尾部开始
        out = sisdb_one_sub(sisdb_, netmsg_, true);
        goto _exit_;
    }
    else
    {
        if (_start_subsno_worker(sisdb_, netmsg_, subdate, false))
        {
            out = 0;
            goto _exit_;
        }
    }
_exit_:
    sis_json_close(handle);
    return out; 
}

int sisdb_multiple_subsno(s_sisdb_cxt *sisdb_, s_sis_net_message *netmsg_)
{
    s_sis_json_handle *handle = netmsg_->val ? sis_json_load(netmsg_->val, sis_sdslen(netmsg_->val)) : NULL;
    int out = 1;
    if (!handle)
    {
        return sisdb_multiple_sub(sisdb_, netmsg_, true);
    }
    
    int    start   = sis_json_get_int(handle->node, "start", -1); // 0 从头开始 -1 从尾开始
    date_t subdate = sis_json_get_int(handle->node, "date", 0);
    if ((subdate == 0 || subdate == sisdb_->work_date) && start == -1)
    {
        // 从尾部开始
        out = sisdb_multiple_sub(sisdb_, netmsg_, true);
        goto _exit_;
    }
    else
    {
        if (_start_subsno_worker(sisdb_, netmsg_, subdate, true))
        {
            out = 0;
            goto _exit_;
        }
    }
_exit_:
    sis_json_close(handle);
    return out; 
}

int sisdb_unsubsno_whole(s_sisdb_cxt *sisdb_, int cid_)
{
    int count = 0;
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->subsno_worker);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_subsno_info *info = (s_sisdb_subsno_info *)sis_dict_getval(de);
            info->status = SISDB_SUBSNO_EXIT;
            count++;
        }
        sis_dict_iter_free(di);
    }
    sisdb_unsub_whole(sisdb_, cid_, true);
    return count;
}
int sisdb_one_unsubsno(s_sisdb_cxt *sisdb_,s_sis_net_message *netmsg_)
{
    int count = 0;
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->subsno_worker);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_subsno_info *info = (s_sisdb_subsno_info *)sis_dict_getval(de);
            if (!info->ismul)
            {
                info->status = SISDB_SUBSNO_EXIT;
                count++;
            }
        }
        sis_dict_iter_free(di);
    }
    count += sisdb_one_unsub(sisdb_, netmsg_, true);
    return count;
}

int sisdb_multiple_unsubsno(s_sisdb_cxt *sisdb_, s_sis_net_message *netmsg_)
{
    int count = 0;
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->subsno_worker);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_subsno_info *info = (s_sisdb_subsno_info *)sis_dict_getval(de);
            if (info->ismul)
            {
                info->status = SISDB_SUBSNO_EXIT;
                count++;
            }
        }
        sis_dict_iter_free(di);
    }
    count += sisdb_multiple_unsub(sisdb_, netmsg_, true);
    return count;   
}
