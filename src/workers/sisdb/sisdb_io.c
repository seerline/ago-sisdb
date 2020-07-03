//******************************************************
// Copyright (C) 2018, coollyer <seerlinecoin@gmail.com>
//*******************************************************

#include "sisdb_io.h"
#include "sis_net.io.h"
#include "sisdb_collect.h"

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
        s_sis_string_list *klist = sis_string_list_create_w();
        sis_string_list_load(klist, keys_, sis_strlen(keys_), ",");

        int count = sis_string_list_getsize(klist);
        for (int i = 0; i < count; i++)
        {
            const char *key = sis_string_list_get(klist, i);
            s_sisdb_collect *collect = sis_map_pointer_get(sisdb_->collects, key);
            if (collect->style == SISDB_COLLECT_TYPE_CHARS)
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
        s_sis_string_list *klist = sis_string_list_create_w();
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
    s_sisdb_collect *collect = sisdb_kv_create(style_, argv_, sis_sdslen(argv_));
    sis_map_pointer_set(sisdb_->collects, key_, collect);
    // 这里处理订阅
    sisdb_make_sub_message(sisdb_, key_, style_, argv_, sis_sdslen(argv_));
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
        case 's':  // struct
        case 'S':
        case 'b':  // bytes
        case 'B':
            o = SISDB_FORMAT_BYTES;
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
            o = sisdb_collect_fastget_sds(collect, key_);
        }
        else
        {
	        s_sis_json_handle *handle = sis_json_load(argv_, sis_sdslen(argv_));
            if (handle)
            {
                int iformat = sis_from_node_get_format(handle->node, SISDB_FORMAT_CHARS);
                *format_ = iformat & SISDB_FORMAT_CHARS ? SISDB_FORMAT_CHARS : SISDB_FORMAT_BYTES;
                o = sisdb_collect_get_sds(collect, key_, iformat, handle->node);           
                sis_json_close(handle);
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
            if (sis_str_subcmp_strict(collect->sdb->db->name,  sdbs_, ',') >= 0)
            {
                sis_string_list_push(list_, sis_dict_getkey(de), sis_strlen(sis_dict_getkey(de)));
            }
        }
        sis_dict_iter_free(di);
    }
    else
    {
        s_sis_string_list *slists = sis_string_list_create_w();
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

        s_sis_string_list *klists = sis_string_list_create_w();
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
    s_sis_string_list *collects = sis_string_list_create_w();

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
            s_sis_json_node *jval = sisdb_collects_get_last_node(collect, handle->node);
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
    s_sis_string_list *collects = sis_string_list_create_w();
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
    sisdb_make_sub_message(sisdb_, key_, SISDB_COLLECT_TYPE_BYTES, value_, sis_sdslen(value_));

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
    sisdb_make_sub_message(sisdb_, key_, SISDB_COLLECT_TYPE_BYTES, bytes, sis_sdslen(bytes));

    sis_sdsfree(bytes);
    return 0;
}

/////////////////
//  sub function
/////////////////
void _send_sub_message(s_sisdb_cxt *sisdb_, s_sis_net_message *info_, const char *key_, uint8 style_, s_sis_sds in_, size_t ilen_)
{
    // s_sis_net_message *newinfo = sis_net_message_clone(info_);
    // if (newinfo->key)
    // {
    //     sis_sdsfree(newinfo->key);
    // }
    // 赋新值
    s_sis_net_message *newinfo = sis_net_message_create();
    newinfo->cid = info_->cid;
    newinfo->source = sis_sdsdup(info_->source);
    newinfo->key = sis_sdsnew(key_);

    if(style_ == SISDB_COLLECT_TYPE_BYTES)
    {
        sis_net_ans_with_bytes(newinfo, in_, sis_sdslen(in_));
    }
    else if (style_ == SISDB_COLLECT_TYPE_CHARS)
    {
        sis_net_ans_with_chars(newinfo, in_, sis_sdslen(in_));
    }
    s_sis_object *obj = sis_object_create(SIS_OBJECT_NETMSG, newinfo);
    sis_share_list_push(sisdb_->pub_list, obj);
    sis_object_destroy(obj);   
}
void sisdb_make_sub_message(s_sisdb_cxt *sisdb_, const char *key_, uint8 style_, s_sis_sds in_, size_t ilen_)
{
    // 先处理单键值订阅
	s_sis_pointer_list *sublist = (s_sis_pointer_list *)sis_map_pointer_get(sisdb_->sub_single, key_);
    printf("sublist pub: %p %s\n", sublist, key_);
    if (sublist)
    {
        for (int i = 0; i < sublist->count; i++)
        {
            s_sis_net_message *info = (s_sis_net_message *)sis_pointer_list_get(sublist, i);
            printf("sublist : %d  %s\n", sublist->count, info->key);
            _send_sub_message(sisdb_, info, key_, style_, in_, ilen_);
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
                break;            
            default:
                break;
            }
            if (is_publish)
            {
                for (int i = 0; i < sublist->netmsgs->count; i++)
                {
                    s_sis_net_message *info = (s_sis_net_message *)sis_pointer_list_get(sublist->netmsgs, i);
                    _send_sub_message(sisdb_, info, key_, style_, in_, ilen_);
                }
            }
        }
        sis_dict_iter_free(di);
    }    
}


int sisdb_one_sub(s_sisdb_cxt *sisdb_, s_sis_net_message *netmsg_)
{
	s_sis_pointer_list *sublist = (s_sis_pointer_list *)sis_map_pointer_get(sisdb_->sub_single, netmsg_->key);
	if (!sublist)
	{
        sublist = sis_pointer_list_create();
		sublist->vfree = sis_net_message_decr;
		sis_map_pointer_set(sisdb_->sub_single, netmsg_->key, sublist);
	}
    printf("sublist sub: %s\n",netmsg_->key);
	bool isnew = true;
	for (int i = 0; i < sublist->count; i++)
	{
		s_sis_net_message *info = (s_sis_net_message *)sis_pointer_list_get(sublist, i);
		if (info->cid ==  netmsg_->cid)
		{
			isnew = false;
			break;
		}
	}
	if (isnew)
	{
		sis_net_message_incr(netmsg_);
		sis_pointer_list_push(sublist, netmsg_);
	}
    return isnew ? 1 : 0;
}

int sisdb_unsub_whole(s_sisdb_cxt *sisdb_, int cid_)
{
    int count = 0;
    // 单键值定义
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->sub_single);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sis_pointer_list *sublist = (s_sis_pointer_list *)sis_dict_getval(de);
            int i = 0;
            while (i < sublist->count)
            {
                s_sis_net_message *info = (s_sis_net_message *)sis_pointer_list_get(sublist, i);
                if (info->cid == cid_)
                {
                    sis_pointer_list_delete(sublist, i, 1);
                    count++;
                    if (sublist->count == 0)
                    {
                        sis_map_pointer_del(sisdb_->sub_single, sis_dict_getkey(de));
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
    // 多键值定义
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->sub_multiple);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_sub_info *sublist = (s_sisdb_sub_info *)sis_dict_getval(de);
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
                        sis_map_pointer_del(sisdb_->sub_multiple, sis_dict_getkey(de));
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

int sisdb_one_unsub(s_sisdb_cxt *sisdb_, s_sis_net_message *netmsg_)
{
	s_sis_pointer_list *sublist = (s_sis_pointer_list *)sis_map_pointer_get(sisdb_->sub_single, netmsg_->key);
    if (sublist)
    {
        int i = 0;
        while (i < sublist->count) 
        {
            s_sis_net_message *info = (s_sis_net_message *)sis_pointer_list_get(sublist, i);
            if (info->cid ==  netmsg_->cid)
            {
                sis_pointer_list_delete(sublist, i, 1);
                if (sublist->count == 0)
                {
                    sis_map_pointer_del(sisdb_->sub_single, netmsg_->key);
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

int sisdb_multiple_sub(s_sisdb_cxt *sisdb_, s_sis_net_message *netmsg_)
{
	s_sisdb_sub_info *sublist = (s_sisdb_sub_info *)sis_map_pointer_get(sisdb_->sub_multiple, netmsg_->key);
	if (!sublist)
	{
        s_sisdb_sub_info *info = sisdb_sub_info_create(netmsg_);
		sis_map_pointer_set(sisdb_->sub_multiple, netmsg_->key, info);
	}
    printf("multiple_sub: %s\n",netmsg_->key);
	bool isnew = true;
	for (int i = 0; i < sublist->netmsgs->count; i++)
	{
		s_sis_net_message *info = (s_sis_net_message *)sis_pointer_list_get(sublist->netmsgs, i);
		if (info->cid ==  netmsg_->cid)
		{
			isnew = false;
			break;
		}
	}
	if (isnew)
	{
		sis_net_message_incr(netmsg_);
		sis_pointer_list_push(sublist->netmsgs, netmsg_);
	}
    return isnew ? 1 : 0;
}
int sisdb_multiple_unsub(s_sisdb_cxt *sisdb_, s_sis_net_message *netmsg_)
{
	s_sisdb_sub_info *sublist = (s_sisdb_sub_info *)sis_map_pointer_get(sisdb_->sub_multiple, netmsg_->key);
    if (sublist)
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
bool _subsno_filter(const char *subkey_, const char *key_)
{
    return true;
}
void *_thread_publish_realtime(void *argv_)
{
    s_sisdb_subsno_info *info = (s_sisdb_subsno_info *)argv_;

    int count = info->sisdb->series->count;
    for (int i = 0; i < count; i++)
    {
        if (info->status == SISDB_SUBSNO_EXIT)
        {
            break;
        }
        s_sisdb_collect_sno *sno = sis_node_list_get(info->sisdb->series, i);
        if (_subsno_filter(info->netmsg->key, sno->collect->key))
        {
            s_sis_net_message *newinfo = sis_net_message_create();
            newinfo->cid = info->netmsg->cid;
            newinfo->source = sis_sdsdup(info->netmsg->source);
            newinfo->key = sis_sdsnew(sno->collect->key);
            // 暂时全部返回二进制数据
            s_sis_struct_list *value = SIS_OBJ_LIST(sno->collect->obj);
            sis_net_ans_with_bytes(newinfo, sis_struct_list_get(value, sno->recno), sno->collect->sdb->db->size);
            s_sis_object *obj = sis_object_create(SIS_OBJECT_NETMSG, newinfo);
            sis_share_list_push(info->sisdb->pub_list, obj);
            sis_object_destroy(obj);               
        }
    }
    sis_thread_finish(&info->thread_cxt);
    if (info->ismul)
    {
        sisdb_multiple_sub(info->sisdb, info->netmsg);
    }
    else
    {
        sisdb_one_sub(info->sisdb, info->netmsg);
    }
    // 线程结束应该从map表中剔除
    return NULL;
}

// void market_rfile_sub_excute(void *worker_) 
// {
//     s_sis_worker *worker = (s_sis_worker *)worker_;
//     s_market_rfile_cxt *context = (s_market_rfile_cxt *)worker->context;
//     char fn[32];
    
//     sis_llutoa(context->read_cb->sub_date, fn, 32, 10);
    
//     if (sis_disk_class_init(context->read_class, SIS_DISK_TYPE_SNO, context->work_path, fn)
//      || sis_disk_file_read_start(context->read_class))
//     {
//         if (context->read_cb->cb_sub_stop)
//         {
//             if (context->ischars)
//             {
//                 context->read_cb->cb_sub_stop(context->read_cb->source, fn);
//             }
//             else
//             {
//                 context->read_cb->cb_sub_stop(context->read_cb->source, &context->read_cb->sub_date);
//             }
//         }
//         return ;
//     }
//     s_sis_disk_callback *callback = SIS_MALLOC(s_sis_disk_callback, callback);
//     callback->source = worker;
//     callback->cb_begin = cb_begin;
//     callback->cb_key = cb_key;
//     callback->cb_sdb = cb_sdb;
//     callback->cb_read = cb_read;
//     callback->cb_end = cb_end;

//     s_sis_disk_reader *reader = sis_disk_reader_create(callback);
//     // printf("%s| %s | %s\n", context->read_cb->sub_codes, context->work_sdbs, context->read_cb->sub_sdbs ? context->read_cb->sub_sdbs : "null");
//     if (!context->read_cb->sub_sdbs || sis_sdslen(context->read_cb->sub_sdbs) < 1)
//     {
//         sis_disk_reader_set_sdb(reader, context->work_sdbs);  
//     }  
//     else
//     {
//         sis_disk_reader_set_sdb(reader, context->read_cb->sub_sdbs);   
//     }  
//     sis_disk_reader_set_key(reader, context->read_cb->sub_codes);

//     // sub 是一条一条的输出
//     sis_disk_file_read_sub(context->read_class, reader);
//     // get 是所有符合条件的一次性输出
//     // sis_disk_file_read_get(rwf, reader);
//     sis_disk_reader_destroy(reader);
    
//     sis_free(callback);

//     sis_disk_file_read_stop(context->read_class);
// }
void *_thread_publish_history(void *argv_)
{
    // 从文件中获取数据
    s_sisdb_subsno_info *info = (s_sisdb_subsno_info *)argv_;


    sis_thread_finish(&info->thread_cxt);
    // 线程结束应该从map表中剔除
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
        if (!sis_thread_create(_thread_publish_realtime, info, &info->thread_cxt))
        {
            LOG(1)("can't start _thread_publish_realtime.\n");
            return -1;
        }
    } else if (subdate_ < sisdb_->work_date)
    {
        s_sisdb_subsno_info *info = sisdb_subsno_info_create(sisdb_, netmsg_);
        info->ismul = ismul_;
        char subkey[255];
        sis_sprintf(subkey, 255, "%s-%d", netmsg_->key, (int)subdate_);
        sis_map_pointer_set(sisdb_->subsno_worker, subkey, info);
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
    s_sis_json_handle *handle = sis_json_load(netmsg_->val, sis_sdslen(netmsg_->val));
    if (!handle)
    {
        return sisdb_one_sub(sisdb_, netmsg_);
    }
    
    int    start   = sis_json_get_int(handle->node, "start", -1); // 0 从头开始 -1 从尾开始
    date_t subdate = sis_json_get_int(handle->node, "date", 0);
    if ((subdate == 0 || subdate == sisdb_->work_date) && start == -1)
    {
        // 从尾部开始
        return sisdb_one_sub(sisdb_, netmsg_);
    }
    else
    {
        if (_start_subsno_worker(sisdb_, netmsg_, subdate, false))
        {
            return 0;
        }
    }
    return 1; 
}

int sisdb_multiple_subsno(s_sisdb_cxt *sisdb_, s_sis_net_message *netmsg_)
{
    s_sis_json_handle *handle = sis_json_load(netmsg_->val, sis_sdslen(netmsg_->val));
    if (!handle)
    {
        return sisdb_multiple_sub(sisdb_, netmsg_);
    }
    
    int    start   = sis_json_get_int(handle->node, "start", -1); // 0 从头开始 -1 从尾开始
    date_t subdate = sis_json_get_int(handle->node, "date", 0);
    if ((subdate == 0 || subdate == sisdb_->work_date) && start == -1)
    {
        // 从尾部开始
        return sisdb_multiple_sub(sisdb_, netmsg_);
    }
    else
    {
        if (_start_subsno_worker(sisdb_, netmsg_, subdate, true))
        {
            return 0;
        }
    }
    return 1; 
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
    return count;   
}
