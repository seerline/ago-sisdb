//******************************************************
// Copyright (C) 2018, coollyer <seerlinecoin@gmail.com>
//*******************************************************

#include "sisdb_io.h"
#include "sisdb_collect.h"

s_sis_sds sisdb_single_get_sds(s_sisdb_cxt *sisdb_, const char *key_, uint16 *format_, s_sis_sds argv_)
{
    s_sis_sds o = NULL;
    s_sisdb_kv *info = sis_map_pointer_get(sisdb_->kvs, key_);
    if (info && info->value)
    {
        *format_ = info->format;
        o = sis_sdsdup(info->value);
    }
    return o;
}

// 多单键取值只处理字符串 非字符串不处理
s_sis_sds sisdb_single_gets_sds(s_sisdb_cxt *sisdb_, const char *keys_, s_sis_sds argv_)
{
    s_sis_json_node *jone = sis_json_create_object();
    if (!sis_strcasecmp(keys_, "*"))
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->kvs);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_kv *info = (s_sisdb_kv *)sis_dict_getval(de);
            if (info->format == SISDB_FORMAT_CHARS)
            {
                sis_json_object_add_string(jone, sis_dict_getkey(de), info->value, sis_sdslen(info->value));
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
            s_sisdb_kv *info = sis_map_pointer_get(sisdb_->kvs, key);
            if (info->format == SISDB_FORMAT_CHARS)
            {
                sis_json_object_add_string(jone, key, info->value, sis_sdslen(info->value));
            }
        }
        sis_string_list_destroy(klist);
    }
    s_sis_sds o = sis_json_to_sds(jone, false);
    sis_json_delete_node(jone);
    return o;
}

int sisdb_single_del(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds argv_)
{
    sis_map_pointer_del(sisdb_->kvs, key_);
    return 0;
}
int sisdb_single_dels(s_sisdb_cxt *sisdb_, const char *keys_, s_sis_sds argv_)
{
    if (!sis_strcasecmp(keys_, "*"))
    {
        sis_map_pointer_clear(sisdb_->kvs);
    }
    else
    {
        s_sis_string_list *klist = sis_string_list_create_w();
        sis_string_list_load(klist, keys_, sis_strlen(keys_), ",");

        int count = sis_string_list_getsize(klist);
        for (int i = 0; i < count; i++)
        {
            sis_map_pointer_del(sisdb_->collects, sis_string_list_get(klist, i));
        }
        sis_string_list_destroy(klist);
    }
    return 0;
}

int sisdb_single_set(s_sisdb_cxt *sisdb_, const char *key_, uint16 format_, s_sis_sds argv_)
{
    s_sisdb_kv *info = sisdb_kv_create(format_, argv_);
    sis_map_pointer_set(sisdb_->kvs, key_, info);
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
    if (collect->value->count == 0)
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
    int o = sisdb_collect_update(collect, value_);
    
    if (o <= 0)
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
    s_sis_sds value = NULL;
    if (value_[0] == '{')
    {
        value = sisdb_collect_json_to_struct_sds(collect, value_);
    }
    if (value_[0] == '[')
    {
        value = sisdb_collect_array_to_struct_sds(collect, value_);
    }
    if (!value || sis_sdslen(value) < 1)
    {
        return -3;
    }
    int o = sisdb_collect_update(collect, value); 
    if (o <= 0)
    {
        return -5;
    }
    return 0;
}

// int sis_net_class_subscibe(s_sis_net_class *cls_, s_sis_net_message *mess_)
// {
// 	if(cls_->work_status != SIS_NET_WORKING)
// 	{
// 		return -1;
// 	}
// 	sis_net_message_incr(mess_);
// 	s_sis_object *obj = sis_object_create(SIS_OBJECT_NETMSG, mess_);

// 	s_sis_pointer_list *list = (s_sis_pointer_list *)sis_map_pointer_get(cls_->map_subs, mess_->key);
// 	if (!list)
// 	{
// 		list = sis_pointer_list_create();
// 		list->vfree = sis_object_decr;
// 		sis_map_pointer_set(cls_->map_subs, mess_->key, list);
// 	}
// 	bool isnew = true;
// 	for (int i = 0; i < list->count; i++)
// 	{
// 		s_sis_object *info = (s_sis_object *)sis_pointer_list_get(list, i);
// 		if (SIS_OBJ_NETMSG(info)->cid ==  mess_->cid)
// 		{
// 			isnew = false;
// 			break;
// 		}
// 	}
// 	if (isnew)
// 	{
// 		sis_object_incr(obj);
// 		sis_pointer_list_push(list, obj);
// 	}
// 	// 断线重连后自动发送订阅信息用
// 	sis_share_list_push(cls_->ready_send_cxts, obj);
// 	sis_object_destroy(obj);
// 	return 0;
// }
// int sis_net_class_publish(s_sis_net_class *cls_, s_sis_net_message *mess_)
// {
// 	if(cls_->work_status != SIS_NET_WORKING)
// 	{
// 		return -1;
// 	}
// 	s_sis_pointer_list *list = (s_sis_pointer_list *)sis_map_pointer_get(cls_->map_pubs, mess_->key);
// 	if (!list)
// 	{
// 		return -2;
// 	}
// 	for (int i = 0; i < list->count; i++)
// 	{
// 		s_sis_object *info = (s_sis_object *)sis_pointer_list_get(list, i);
// 		if (mess_->cid == -1 || mess_->cid == SIS_OBJ_NETMSG(info)->cid )
// 		{
// 			s_sis_net_message *newmsg = sis_net_message_clone(mess_);
// 			newmsg->cid = SIS_OBJ_NETMSG(info)->cid;
// 			if (!newmsg->source)
// 			{
// 				newmsg->source = sis_sdsempty();
// 			}
// 			newmsg->source = sis_sdscpy(newmsg->source, SIS_OBJ_NETMSG(info)->source);
// 			s_sis_object *obj = sis_object_create(SIS_OBJECT_NETMSG, newmsg);
// 			sis_share_list_push(cls_->ready_send_cxts, obj);
// 			sis_object_destroy(obj);
// 		}
// 	}
// 	return 0;
// }
// int sis_net_class_pub_add(s_sis_net_class *cls_, s_sis_net_message *mess_)
// {
// 	sis_net_message_incr(mess_);
// 	s_sis_object *obj = sis_object_create(SIS_OBJECT_NETMSG, mess_);
// 	s_sis_pointer_list *list = (s_sis_pointer_list *)sis_map_pointer_get(cls_->map_pubs, mess_->key);
// 	if (!list)
// 	{
// 		list = sis_pointer_list_create();
// 		list->vfree = sis_object_decr;
// 		sis_map_pointer_set(cls_->map_pubs, mess_->key, list);
// 	}
// 	bool isnew = true;
// 	for (int i = 0; i < list->count; i++)
// 	{
// 		s_sis_object *info = (s_sis_object *)sis_pointer_list_get(list, i);
// 		if (SIS_OBJ_NETMSG(info)->cid ==  mess_->cid)
// 		{
// 			isnew = false;
// 			break;
// 		}
// 	}
// 	if (isnew)
// 	{
// 		sis_object_incr(obj);
// 		sis_pointer_list_push(list, obj);
// 	}
// 	sis_object_destroy(obj);
// 	return 0;
// }
// int sis_net_class_pub_del(s_sis_net_class *cls_, int sid_)
// {
// 	s_sis_dict_entry *de;
// 	s_sis_dict_iter *di = sis_dict_get_iter(cls_->map_pubs);
// 	while ((de = sis_dict_next(di)) != NULL)
// 	{
// 		s_sis_pointer_list *list = (s_sis_pointer_list *)sis_dict_getval(de);
// 		for (int i = 0; i < list->count; )
// 		{
// 			s_sis_object *info = (s_sis_object *)sis_pointer_list_get(list, i);
// 			if (SIS_OBJ_NETMSG(info)->cid ==  sid_)
// 			{
// 				sis_pointer_list_delete(list, i, 1);
// 			}
// 			else
// 			{
// 				i++;
// 			}			
// 		}			
// 	}
// 	sis_dict_iter_free(di);
// 	return 0;
// }

