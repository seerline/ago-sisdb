#include "sis_disk_v1.h"


///////////////////////////
//  write
///////////////////////////

size_t sis_disk_v1_write_work(s_sis_disk_v1_class *cls_, int hid_, s_sis_disk_v1_wcatch *wcatch_)
{ 

    size_t size = 0;

    size = sis_disk_v1_write(cls_->work_fps, hid_, wcatch_);

    if (wcatch_ && cls_->work_fps->main_head.index)
    {
        s_sis_disk_v1_index *node = sis_disk_v1_index_get(cls_->index_infos, wcatch_->key, wcatch_->sdb);        
        // printf("write : %s %d \n ",SIS_OBJ_SDS(wcatch_->key), hid_);
        wcatch_->winfo.active++;
        sis_struct_list_push(node->index, &wcatch_->winfo); // 写完盘后增加一条索引记录
    }
    return size;
}

////////////////////
//  write
///////////////////
// size_t sis_disk_v1_file_write_stream(s_sis_disk_v1_class *cls_, void *in_, size_t ilen_)
// {
//     size_t size = 0;
    
//     if (cls_->work_fps->main_head.style == SIS_DISK_TYPE_STREAM)
//     {
//         s_sis_disk_v1_wcatch *wcatch = cls_->src_wcatch;
//         sis_memory_cat(wcatch->memory, (char *)in_, ilen_);
//         size = sis_disk_v1_write(cls_->work_fps, SIS_DISK_HID_STREAM, wcatch);
//         sis_memory_clear(wcatch->memory);
//     }
//     return size;
// }
// 
size_t sis_disk_v1_file_write_stream(s_sis_disk_v1_class *cls_, void *in_, size_t ilen_)
{
    size_t size = 0;
    
    if (cls_->work_fps->main_head.style == SIS_DISK_TYPE_STREAM)
    {
        s_sis_disk_v1_wcatch *wcatch = cls_->src_wcatch;
        sis_memory_cat(wcatch->memory, (char *)in_, ilen_);
        if (sis_memory_get_size(wcatch->memory) > cls_->work_fps->max_page_size)
        {
            size = sis_disk_v1_write(cls_->work_fps, SIS_DISK_HID_STREAM, wcatch);
            // printf("wlog %d\n", size);
            sis_disk_v1_wcatch_clear(wcatch);
            // 流式文件每次必写盘
        }
    }
    return size;
}
size_t sis_disk_v1_file_write_sdb_log(s_sis_disk_v1_class *cls_,
                                   s_sis_disk_v1_dict *key, s_sis_disk_v1_dict *sdb, void *in_, size_t ilen_)
{

    size_t size = 0;
    s_sis_disk_v1_dict_unit *unit = sis_disk_v1_dict_last(sdb);
    // printf(" %s %s: %d %zu \n", SIS_OBJ_SDS(key->name), SIS_OBJ_SDS(sdb->name), unit->db->size, ilen_);
    if (unit && (ilen_ % unit->db->size) == 0)
    {
        s_sis_memory *memory = cls_->src_wcatch->memory;
        int count = ilen_ / unit->db->size;
        for (int i = 0; i < count; i++)
        {
            sis_memory_cat_ssize(memory, key->index);
            sis_memory_cat_ssize(memory, sdb->index);
            sis_memory_cat(memory, (char *)in_ + i * unit->db->size, unit->db->size);
        }
        if (sis_memory_get_size(memory) > cls_->work_fps->max_page_size)
        {
            size = sis_disk_v1_write_work(cls_, SIS_DISK_HID_MSG_LOG, cls_->src_wcatch);
            sis_disk_v1_wcatch_clear(cls_->src_wcatch);
        }
    }
    return size;
}

size_t sis_disk_v1_file_write_sno(s_sis_disk_v1_class *cls_)
{
    size_t size = 0;
    int count = sis_map_list_getsize(cls_->sno_wcatch);
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_v1_wcatch *wcatch = (s_sis_disk_v1_wcatch *)sis_map_list_geti(cls_->sno_wcatch, i);
        if (sis_memory_get_size(wcatch->memory) > 0)
        {
            size += sis_disk_v1_write_work(cls_, SIS_DISK_HID_MSG_SNO, wcatch);
            sis_disk_v1_wcatch_clear(wcatch);
        }      
    }
    // 写完页面加1
    cls_->sno_pages++;
    // 写页结束符号
    size += sis_disk_v1_write_work(cls_, SIS_DISK_HID_SNO_END, NULL);
    cls_->sno_size = 0;
    cls_->sno_series = 0;
    LOG(5)("write sno page ok. pageno= %d, offset=%dM\n", cls_->sno_pages, (int)(sis_disk_v1_offset(cls_->work_fps)/1000000));
    return size;
}

size_t sis_disk_v1_file_write_sdb_sno(s_sis_disk_v1_class *cls_,
                                   s_sis_disk_v1_dict *key, s_sis_disk_v1_dict *sdb, void *in_, size_t ilen_)
{
    s_sis_disk_v1_dict_unit *unit = sis_disk_v1_dict_last(sdb);
    // printf("%p %s %d %zu %d\n", unit, SIS_OBJ_SDS(sdb->name), sdb->units->count, ilen_ , unit->db->size);
    if (unit && (ilen_ % unit->db->size) == 0)
    {
        char infoname[255];
        sis_sprintf(infoname, 255, "%s.%s", SIS_OBJ_SDS(key->name), SIS_OBJ_SDS(sdb->name));        
        s_sis_disk_v1_wcatch *wcatch = (s_sis_disk_v1_wcatch *)sis_map_list_get(cls_->sno_wcatch, infoname);
        if (!wcatch)
        {
            wcatch = sis_disk_v1_wcatch_create(key->name, sdb->name);
            sis_map_list_set(cls_->sno_wcatch, infoname, wcatch);
        }
        if (sis_memory_get_size(wcatch->memory) < 1)
        {
            cls_->sno_size += sis_memory_cat_ssize(wcatch->memory, key->index);
            cls_->sno_size += sis_memory_cat_ssize(wcatch->memory, sdb->index);
            wcatch->winfo.start = sis_dynamic_db_get_time(unit->db, 0, in_, ilen_);
        }
        // sno 因为是序列化存储 不计算停止时间
        // msec_t stop = sis_dynamic_db_get_time(unit->db, count - 1, in_, ilen_);
        // wcatch->winfo.stop = sis_max(wcatch->winfo.stop, stop);

        wcatch->winfo.kdict = key->units->count;
        wcatch->winfo.sdict = sdb->units->count;
        wcatch->winfo.ipage = cls_->sno_pages;

        int count = ilen_ / unit->db->size;
        for (int i = 0; i < count; i++)
        {
            cls_->sno_size += sis_memory_cat_ssize(wcatch->memory, cls_->sno_series);
            cls_->sno_series++;
            cls_->sno_size += sis_memory_cat(wcatch->memory, (char *)in_ + i * unit->db->size, unit->db->size);
        }
        // printf("%zu --> %zu\n", cls_->sno_size ,cls_->work_fps->max_page_size);
        if (cls_->sno_size > cls_->work_fps->max_page_size)
        {
            // printf("%zu %zu %zu %d\n", cls_->sno_size, cls_->work_fps->max_page_size, cls_->sno_series, cls_->sno_pages);
            sis_disk_v1_file_write_sno(cls_);
        }
    }
    return 0;
}

static int sis_write_sdb_merge(s_sis_disk_v1_class *cls_, s_sis_disk_v1_wcatch *wcatch_)
{
    s_sis_disk_v1_index *node = sis_disk_v1_index_get(cls_->index_infos, wcatch_->key, wcatch_->sdb);

    // 判断来源数据是否时序的数据
    // 如果来源数据不是序列化的就直接删除老的 
    if ((wcatch_->winfo.start == 0 && wcatch_->winfo.stop == 0) || 
         wcatch_->winfo.start > wcatch_->winfo.stop)
    {
        sis_struct_list_clear(node->index);
    }   
    else
    {
        // 暂时处理为时间有重叠就覆盖老的
        int idx = 0;
        while(idx < node->index->count)
        {
            s_sis_disk_v1_index_unit *unit = (s_sis_disk_v1_index_unit *)sis_struct_list_get(node->index, idx);
            if (sis_is_mixed(wcatch_->winfo.start, wcatch_->winfo.stop, unit->start, unit->stop))
            {
                sis_struct_list_delete(node->index, idx, 1);
                continue;
            }
            idx++;
        }   
    }     
    return 0;
}
size_t sis_disk_v1_file_write_sdb_one(s_sis_disk_v1_class *cls_,
                                    s_sis_disk_v1_dict *key, s_sis_disk_v1_dict *sdb, void *in_, size_t ilen_)
{
    size_t size = 0;

    s_sis_disk_v1_dict_unit *unit = sis_disk_v1_dict_last(sdb);
    if (unit && (ilen_ % unit->db->size) == 0)
    {
        s_sis_disk_v1_wcatch *wcatch = sis_disk_v1_wcatch_create(key->name, sdb->name);

        int count = ilen_ / unit->db->size;
        wcatch->winfo.start = sis_dynamic_db_get_time(unit->db, 0, in_, ilen_);
        wcatch->winfo.stop = sis_dynamic_db_get_time(unit->db, count - 1, in_, ilen_);
        // printf("write : %d %d %d \n", count, wcatch->winfo.start , wcatch->winfo.stop);

        wcatch->winfo.kdict = key->units->count;
        wcatch->winfo.sdict = sdb->units->count;

        // 这里可以判断新的块和老的块是否有冲突 甚至老的和新的数据结构不同也可以在这里处理
        sis_write_sdb_merge(cls_, wcatch);

        sis_memory_cat_ssize(wcatch->memory, key->index);
        sis_memory_cat_ssize(wcatch->memory, sdb->index);
        sis_memory_cat(wcatch->memory, (char *)in_, ilen_);
        size = sis_disk_v1_write_work(cls_, SIS_DISK_HID_MSG_SDB, wcatch);
        sis_disk_v1_wcatch_destroy(wcatch);
    }
    return size;
}

#define CHECK_IS_SDB(s) (s == SIS_DISK_TYPE_LOG || s == SIS_DISK_TYPE_SNO || s == SIS_DISK_TYPE_SDB) ? 1 : 0


size_t _sis_disk_v1_file_write_sdb(s_sis_disk_v1_class *cls_,
                                s_sis_disk_v1_dict *key_, s_sis_disk_v1_dict *sdb_, 
                                void *in_, size_t ilen_)
{
    // 针对不同的类型对数据进行处理
    size_t size = 0;
    switch (cls_->work_fps->main_head.style)
    {
    case SIS_DISK_TYPE_LOG:
        size = sis_disk_v1_file_write_sdb_log(cls_, key_, sdb_, in_, ilen_);
        break;
    case SIS_DISK_TYPE_SNO:
        size = sis_disk_v1_file_write_sdb_sno(cls_, key_, sdb_, in_, ilen_);
        break;
    default: // SIS_DISK_TYPE_SDB
        size = sis_disk_v1_file_write_sdb_one(cls_, key_, sdb_, in_, ilen_);
        break;
    }
    return size;
}

#define SET_MOMORY_STR(m, s, z)          \
    {                                    \
        sis_memory_cat_ssize(m, z);      \
        sis_memory_cat(m, (char *)s, z); \
    }

size_t _sis_disk_v1_file_write_kdb(s_sis_disk_v1_class *cls_,
                               const char *key_, s_sis_disk_v1_dict *sdb_, void *in_, size_t ilen_)
{
    size_t size = 0;
    s_sis_disk_v1_dict_unit *unit = sis_disk_v1_dict_last(sdb_);
    if (unit && (ilen_ % unit->db->size) == 0)
    {
        s_sis_object *key = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(key_));

        s_sis_disk_v1_wcatch *wcatch = sis_disk_v1_wcatch_create(key, sdb_->name);
        
        int count = ilen_ / unit->db->size;
        wcatch->winfo.start = sis_dynamic_db_get_time(unit->db, 0, in_, ilen_);
        wcatch->winfo.stop = sis_dynamic_db_get_time(unit->db, count - 1, in_, ilen_);
        // printf("write : %d %d %d \n", count, wcatch->winfo.start , wcatch->winfo.stop);

        wcatch->winfo.kdict = 0;
        wcatch->winfo.sdict = sdb_->units->count;

        sis_write_sdb_merge(cls_, wcatch);
        SET_MOMORY_STR(wcatch->memory, key_, sis_strlen((char *)key_));
        sis_memory_cat_ssize(wcatch->memory, sdb_->index);
        sis_memory_cat(wcatch->memory, (char *)in_, ilen_);
        size = sis_disk_v1_write_work(cls_, SIS_DISK_HID_MSG_KDB, wcatch);

        sis_disk_v1_wcatch_destroy(wcatch);

        sis_object_destroy(key);
    }
    return size;
}

size_t _sis_disk_v1_file_write_key(s_sis_disk_v1_class *cls_, s_sis_disk_v1_dict *key_, const char *in_, size_t ilen_)
{
    size_t size = 0;
    s_sis_disk_v1_wcatch *wcatch = sis_disk_v1_wcatch_create(key_->name, NULL);
    wcatch->winfo.kdict = key_->units->count;
    wcatch->winfo.sdict = 0;

    sis_write_sdb_merge(cls_, wcatch);
    sis_memory_cat_ssize(wcatch->memory, key_->index);
    SET_MOMORY_STR(wcatch->memory, in_, ilen_);
    size = sis_disk_v1_write_work(cls_, SIS_DISK_HID_MSG_KEY, wcatch);
    sis_disk_v1_wcatch_destroy(wcatch);
    return size;
}

size_t _sis_disk_v1_file_write_any(s_sis_disk_v1_class *cls_, const char *key_, const char *in_, size_t ilen_)
{
    size_t size = 0;
    s_sis_object *key = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(key_));
    
    s_sis_disk_v1_wcatch *wcatch = sis_disk_v1_wcatch_create(key, NULL);
    wcatch->winfo.kdict = 0;
    wcatch->winfo.sdict = 0;
    sis_write_sdb_merge(cls_, wcatch);
    SET_MOMORY_STR(wcatch->memory, key_, sis_strlen((char *)key_));
    SET_MOMORY_STR(wcatch->memory, in_, ilen_);
    size = sis_disk_v1_write_work(cls_, SIS_DISK_HID_MSG_ANY, wcatch);
    sis_disk_v1_wcatch_destroy(wcatch);
    sis_object_destroy(key);
    return size;
}
size_t sis_disk_v1_file_write_sdbi(s_sis_disk_v1_class *cls_,
                                int keyi_, int sdbi_, void *in_, size_t ilen_)
{
    if (!in_ || !ilen_)
    {
        return 0;
    }
    int size = 0;
    s_sis_disk_v1_dict *key = (s_sis_disk_v1_dict *)sis_map_list_geti(cls_->keys, keyi_);
    s_sis_disk_v1_dict *sdb = (s_sis_disk_v1_dict *)sis_map_list_geti(cls_->sdbs, sdbi_);
    if (key && sdb)
    {
        size = _sis_disk_v1_file_write_sdb(cls_, key, sdb, in_, ilen_);
    } 
    return size;    
}
int sis_disk_v1_class_add_key(s_sis_disk_v1_class *cls_, const char *key_)
{
    s_sis_sds msg = sis_sdsempty();
    msg = sis_sdscatfmt(msg, "%s", key_);
    int o = sis_disk_v1_class_set_key(cls_, true, msg, sis_sdslen(msg));
    sis_sdsfree(msg);
    return o;
}

size_t sis_disk_v1_file_write_sdb(s_sis_disk_v1_class *cls_,
                                const char *key_, const char *sdb_, void *in_, size_t ilen_)
{
    if (!in_ || !ilen_)
    {
        return 0;
    }
    if (!CHECK_IS_SDB(cls_->work_fps->main_head.style))
    {
        return 0;
    }
    int size = 0;

    s_sis_disk_v1_dict *key = (s_sis_disk_v1_dict *)sis_map_list_get(cls_->keys, key_);
    if (!key && key_)
    {
        // 增加新的key
        sis_disk_v1_class_add_key(cls_, key_);
        key = (s_sis_disk_v1_dict *)sis_map_list_get(cls_->keys, key_);
    }
    s_sis_disk_v1_dict *sdb = (s_sis_disk_v1_dict *)sis_map_list_get(cls_->sdbs, sdb_);
    // if (!sis_strcasecmp(key_, "SZ002979")||!sis_strcasecmp(key_, "SZ002980"))
    // {
    //     int index = sis_map_list_get_index(cls_->keys, key_);
    //     printf("check : %d %s %s %s\n", index, key_, sdb_, sdb ? SIS_OBJ_SDS(sdb->name) : "nil");
    // }
    if (key && sdb)
    {
        size = _sis_disk_v1_file_write_sdb(cls_, key, sdb, in_, ilen_);
    }
    return size;
}
size_t sis_disk_v1_file_write_kdb(s_sis_disk_v1_class *cls_,
                                const char *key_, const char *sdb_, void *in_, size_t ilen_)
{ 
    if (!in_ || !ilen_)
    {
        return 0;
    }
    if (cls_->work_fps->main_head.style != SIS_DISK_TYPE_SDB)
    {
        return 0;
    }
    int size = 0;

    s_sis_disk_v1_dict *sdb = (s_sis_disk_v1_dict *)sis_map_list_get(cls_->sdbs, sdb_);
    if (sdb && key_)
    {
        size = _sis_disk_v1_file_write_kdb(cls_, key_, sdb, in_, ilen_);
    }
    return size;
}
size_t sis_disk_v1_file_write_key(s_sis_disk_v1_class *cls_,
                                const char *key_, void *in_, size_t ilen_)
{ 
    if (!in_ || !ilen_)
    {
        return 0;
    }
    if (cls_->work_fps->main_head.style != SIS_DISK_TYPE_SDB)
    {
        return 0;
    }
    int size = 0;

    s_sis_disk_v1_dict *key = (s_sis_disk_v1_dict *)sis_map_list_get(cls_->keys, key_);
    if (!key && key_)
    {
        // 增加新的key
        sis_disk_v1_class_add_key(cls_, key_);
        key = (s_sis_disk_v1_dict *)sis_map_list_get(cls_->keys, key_);
    }
    if (key)
    {
        size = _sis_disk_v1_file_write_key(cls_, key, in_, ilen_);
    }
    return size;
}
size_t sis_disk_v1_file_write_any(s_sis_disk_v1_class *cls_,
                                const char *key_, void *in_, size_t ilen_)
{ 
    if (!in_ || !ilen_)
    {
        return 0;
    }
    if (cls_->work_fps->main_head.style != SIS_DISK_TYPE_SDB)
    {
        return 0;
    }
    int size = 0;

    if (key_)
    {
        size = _sis_disk_v1_file_write_any(cls_, key_, in_, ilen_);
    }
    return size;
}
// newkeys
s_sis_sds sis_disk_v1_file_get_keys(s_sis_disk_v1_class *cls_, bool onlyincr_)
{
    s_sis_sds msg = sis_sdsempty();
    int nums = 0;
    {
        int count = sis_map_list_getsize(cls_->keys);
        for(int i = 0; i < count; i++)
        {
            s_sis_disk_v1_dict *info = (s_sis_disk_v1_dict *)sis_map_list_geti(cls_->keys, i);
            for(int k = 0; k < info->units->count; k++)
            {
                s_sis_disk_v1_dict_unit *unit = sis_disk_v1_dict_get(info, k);
                // LOG(1)("w writed = %s %d\n", SIS_OBJ_GET_CHAR(info->name), unit->writed);
                if (onlyincr_ && unit->writed)
                {
                    continue;
                }
                if (nums > 0)
                {
                    msg = sis_sdscatfmt(msg, ",%s", SIS_OBJ_SDS(info->name));
                }
                else
                {
                    msg = sis_sdscatfmt(msg, "%s", SIS_OBJ_SDS(info->name));
                }
                nums++;
                if (onlyincr_) 
                {
                    unit->writed = 1;
                }      
            }
        }
    }
    return msg;
}
size_t sis_disk_v1_file_write_key_dict(s_sis_disk_v1_class *cls_)
{
    size_t size = 0;
    // 写 键表
    s_sis_sds msg = sis_disk_v1_file_get_keys(cls_, true);
    printf("new key:%s\n", msg);
    if (sis_sdslen(msg) > 2)
    {
        s_sis_object *mapobj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(SIS_DISK_SIGN_KEY));
        s_sis_disk_v1_wcatch *wcatch = sis_disk_v1_wcatch_create(mapobj, NULL);
        sis_memory_cat(wcatch->memory, msg, sis_sdslen(msg));
        size = sis_disk_v1_write_work(cls_, SIS_DISK_HID_DICT_KEY, wcatch);
        sis_disk_v1_wcatch_destroy(wcatch);
        sis_object_destroy(mapobj);
    }
    sis_sdsfree(msg);
    return size;
}
s_sis_sds sis_disk_v1_file_get_sdbs(s_sis_disk_v1_class *cls_, bool onlyincr_)
{
    int count = sis_map_list_getsize(cls_->sdbs);
    s_sis_json_node *sdbs_node = sis_json_create_object();
    {
        for(int i = 0; i < count; i++)
        {
            s_sis_disk_v1_dict *info = (s_sis_disk_v1_dict *)sis_map_list_geti(cls_->sdbs, i);
            // printf("+++++ %d %d %s\n",count, info->units->count, SIS_OBJ_SDS(info->name));
            for(int k = 0; k < info->units->count; k++)
            {
                s_sis_disk_v1_dict_unit *unit = sis_disk_v1_dict_get(info, k);
                // LOG(1)("w writed = %s %d %d\n", SIS_OBJ_GET_CHAR(info->name), unit->writed, unit->db->size);
                if (onlyincr_ && unit->writed)
                {
                    continue;
                }
                if (onlyincr_) 
                {
                    unit->writed = 1;
                }
                sis_json_object_add_node(sdbs_node, SIS_OBJ_SDS(info->name), sis_dynamic_dbinfo_to_json(unit->db));
            }
        }
    }
    s_sis_sds msg = sis_json_to_sds(sdbs_node, true);
    sis_json_delete_node(sdbs_node);
    return msg;
}

size_t sis_disk_v1_file_write_sdb_dict(s_sis_disk_v1_class *cls_)
{
    size_t size = 0;
    s_sis_sds msg = sis_disk_v1_file_get_sdbs(cls_, true);
    printf("new sdb:%s\n", msg);
    if (sis_sdslen(msg) > 2)
    {
        s_sis_object *mapobj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(SIS_DISK_SIGN_SDB));
        s_sis_disk_v1_wcatch *wcatch = sis_disk_v1_wcatch_create(mapobj, NULL);
        sis_memory_cat(wcatch->memory, msg, sis_sdslen(msg));
        size = sis_disk_v1_write_work(cls_, SIS_DISK_HID_DICT_SDB, wcatch);
        sis_disk_v1_wcatch_destroy(wcatch);
        sis_object_destroy(mapobj);
    }
    sis_sdsfree(msg);
    return size;
}

// 索引每次都重写一遍
size_t sis_disk_v1_file_write_index(s_sis_disk_v1_class *cls_)
{
    if (!sis_disk_v1_seek(cls_->index_fps))
    {
        return 0;
    }

    size_t size = 0;
    // 到这里头已经写了 尾不用写，直接写中间的内容
    s_sis_memory *memory = cls_->src_wcatch->memory;
    sis_memory_clear(memory);
    s_sis_disk_v1_index *keynode = (s_sis_disk_v1_index *)sis_map_list_get(cls_->index_infos, SIS_DISK_SIGN_KEY);
    if (keynode)
    {
        sis_memory_cat_ssize(memory, keynode->index->count);
        for (int k = 0; k < keynode->index->count; k++)
        {
            s_sis_disk_v1_index_unit *unit = sis_disk_v1_index_get_unit(keynode, k);
            sis_memory_cat_ssize(memory, unit->fidx);
            sis_memory_cat_ssize(memory, unit->offset);
            sis_memory_cat_ssize(memory, unit->size);
        }
        size += sis_disk_v1_write(cls_->index_fps, SIS_DISK_HID_INDEX_KEY, cls_->src_wcatch);
    }
    sis_memory_clear(memory);
    s_sis_disk_v1_index *sdbnode = (s_sis_disk_v1_index *)sis_map_list_get(cls_->index_infos, SIS_DISK_SIGN_SDB);
    if (sdbnode)
    {
        sis_memory_cat_ssize(memory, sdbnode->index->count);
        for (int k = 0; k < sdbnode->index->count; k++)
        {
            s_sis_disk_v1_index_unit *unit = sis_disk_v1_index_get_unit(sdbnode, k);
            sis_memory_cat_ssize(memory, unit->fidx);
            sis_memory_cat_ssize(memory, unit->offset);
            sis_memory_cat_ssize(memory, unit->size);
        }
        size += sis_disk_v1_write(cls_->index_fps, SIS_DISK_HID_INDEX_SDB, cls_->src_wcatch);
    }
    int count = sis_map_list_getsize(cls_->index_infos);
    sis_memory_clear(memory);
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_v1_index *node = (s_sis_disk_v1_index *)sis_map_list_geti(cls_->index_infos, i);
        // 排除key和sdb
        if (node == sdbnode || node == keynode)
        {
            continue;
        }
        // printf("write_index %s %d \n", SIS_OBJ_SDS(node->key), node->index->count);
        size_t klen = sis_sdslen(SIS_OBJ_SDS(node->key));
        sis_memory_cat_ssize(memory, klen);
        sis_memory_cat(memory, SIS_OBJ_SDS(node->key), klen);
        if (node->sdb)
        {
            size_t slen = sis_sdslen(SIS_OBJ_SDS(node->sdb));
            sis_memory_cat_ssize(memory, slen);
            sis_memory_cat(memory, SIS_OBJ_SDS(node->sdb), slen);
        }
        else
        {
            sis_memory_cat_ssize(memory, 0);
        }        
        sis_memory_cat_ssize(memory, node->index->count);
        for (int k = 0; k < node->index->count; k++)
        {
            s_sis_disk_v1_index_unit *unit = (s_sis_disk_v1_index_unit *)sis_struct_list_get(node->index, k);
            sis_memory_cat_byte(memory, unit->active, 1);
            sis_memory_cat_byte(memory, unit->kdict, 1);
            sis_memory_cat_byte(memory, unit->sdict, 1);
            sis_memory_cat_byte(memory, unit->fidx, 1);
            sis_memory_cat_ssize(memory, unit->offset);
            sis_memory_cat_ssize(memory, unit->size);
            sis_memory_cat_ssize(memory, unit->start);
            if (cls_->work_fps->main_head.style == SIS_DISK_TYPE_SNO)
            {
                sis_memory_cat_ssize(memory, unit->ipage);
            }
            else
            {
                sis_memory_cat_ssize(memory, unit->stop - unit->start);
            }
        }
        if (sis_memory_get_size(memory) > SIS_DISK_MAXLEN_MINPAGE)
        {
            if (cls_->work_fps->main_head.style == SIS_DISK_TYPE_SNO)
            {
                size += sis_disk_v1_write(cls_->index_fps, SIS_DISK_HID_INDEX_SNO, cls_->src_wcatch);
            }
            else
            {
                size += sis_disk_v1_write(cls_->index_fps, SIS_DISK_HID_INDEX_MSG, cls_->src_wcatch);    
            }           
            sis_memory_clear(memory);
        }   
    }
    if (sis_memory_get_size(memory) > 0)
    {
        if (cls_->work_fps->main_head.style == SIS_DISK_TYPE_SNO)
        {
            size += sis_disk_v1_write(cls_->index_fps, SIS_DISK_HID_INDEX_SNO, cls_->src_wcatch);
        }
        else
        {
            size += sis_disk_v1_write(cls_->index_fps, SIS_DISK_HID_INDEX_MSG, cls_->src_wcatch);    
        }
        sis_memory_clear(memory);
    }   
    sis_disk_v1_write_sync(cls_->index_fps);
    LOG(5)("write_index end %zu \n", size);
    return size;
}

size_t sis_disk_v1_file_write_surplus(s_sis_disk_v1_class *cls_)
{
    size_t size = 0;
    switch (cls_->work_fps->main_head.style)
    {
    case SIS_DISK_TYPE_STREAM:
    {
        if (sis_memory_get_size(cls_->src_wcatch->memory) > 0)
        {
            size = sis_disk_v1_write_work(cls_, SIS_DISK_HID_STREAM, cls_->src_wcatch);
            sis_disk_v1_wcatch_clear(cls_->src_wcatch);
        }
    }
    break;
    case SIS_DISK_TYPE_LOG:
    {
        if (sis_memory_get_size(cls_->src_wcatch->memory) > 0)
        {
            size = sis_disk_v1_write_work(cls_, SIS_DISK_HID_MSG_LOG, cls_->src_wcatch);
            sis_disk_v1_wcatch_clear(cls_->src_wcatch);
        }
    }
    break;
    case SIS_DISK_TYPE_SNO:
    {
        size = sis_disk_v1_file_write_sno(cls_);
        LOG(5)("write sno : %zu\n", size);
    }
    break;
    default: // SIS_DISK_TYPE_SDB  // 所有数据及时写入，

        break;
    }
    // 把缓存的数据全部写盘
    sis_disk_v1_write_sync(cls_->work_fps);
    return size;
}

void sis_disk_v1_file_delete(s_sis_disk_v1_class *cls_)
{
    int count = sis_disk_v1_delete(cls_->work_fps);
    LOG(5)("delete file count = %d.\n", count);
}

void sis_disk_v1_file_move(s_sis_disk_v1_class *cls_, const char *path_)
{
    char fn[255];
    char newfn[255];
    for (int  i = 0; i < cls_->work_fps->lists->count; i++)
    {
        s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->work_fps->lists, i);
        sis_file_getname(unit->fn, fn, 255);
        sis_sprintf(newfn, 255, "%s/%s", path_, fn);
        sis_file_rename(unit->fn, newfn);
    }
    if (cls_->work_fps->main_head.index)
    {
        for (int  i = 0; i < cls_->index_fps->lists->count; i++)
        {
            s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->index_fps->lists, i);
            sis_file_getname(unit->fn, fn, 255);
            sis_sprintf(newfn, 255, "%s/%s", path_, fn);
            sis_file_rename(unit->fn, newfn);
        }
    }
}
int sis_disk_v1_file_valid_idx(s_sis_disk_v1_class *cls_)
{
    if (cls_->work_fps->main_head.index)
    {
        if (!sis_file_exists(cls_->index_fps->cur_name))
        {
            LOG(5)
            ("idxfile no exists.[%s]\n", cls_->index_fps->cur_name);
            return SIS_DISK_CMD_NO_EXISTS_IDX;
        }
        return SIS_DISK_CMD_OK;
    }
    return SIS_DISK_CMD_NO_IDX;
}

// 检查文件是否有效
int sis_disk_v1_file_valid(s_sis_disk_v1_class *cls_)
{
    // 通常判断work和index的尾部是否一样 一样表示完整 否则
    // 检查work file 是否完整 如果不完整就设置最后一个块的位置 并重建索引
    // 如果work file 完整 就检查索引文件是否完整 不完整就重建索引
    // 重建失败就返回 1
    return SIS_DISK_CMD_OK;
}

int sis_disk_v1_file_write_start(s_sis_disk_v1_class *cls_)
{
    if (!cls_->isinit)
    {
        return -10;
    }
    int access = SIS_DISK_ACCESS_APPEND;
    if (!sis_file_exists(cls_->work_fps->cur_name))
    {
        access = SIS_DISK_ACCESS_CREATE;
    }
    // printf("-5--ss-- %d\n", cls_->work_fps->main_head.wtime);
    if (access == SIS_DISK_ACCESS_CREATE)
    {
        // 这里要设置从0页开始
        cls_->sno_pages = 0;
        // 以从新创建新文件的方式打开文件 如果以前有文件直接删除创建新的
        if (sis_disk_v1_open(cls_->work_fps, SIS_DISK_ACCESS_CREATE))
        {
            LOG(2)
            ("create file fail. [%s]\n", cls_->work_fps->cur_name);
            return -5;
        }
    }
    else
    {
        // 以在文件后面追加数据的方式打开文件
        // 对已经存在的文件进行合法性检查 如果文件不完整 就打开失败 由外部程序来处理异常，这样相对安全
        int vo = sis_disk_v1_file_valid(cls_);
        if ( vo != SIS_DISK_CMD_OK)
        {
            LOG(5)
            ("open is no valid.[%s]\n", cls_->work_fps->cur_name);
            return -2;
        }
        if (sis_disk_v1_open(cls_->work_fps, SIS_DISK_ACCESS_APPEND))
        {
            LOG(5)
            ("open file fail.[%s]\n", cls_->work_fps->cur_name);
            return -3;
        }    
        if (cls_->work_fps->main_head.index && sis_disk_v1_file_valid_idx(cls_) == SIS_DISK_CMD_OK)
        {
            sis_map_list_clear(cls_->index_infos);
            if (sis_disk_v1_open(cls_->index_fps, SIS_DISK_ACCESS_RDONLY))
            {
                LOG(5)
                ("open idxfile fail.[%s]\n", cls_->index_fps->cur_name);
                return -4;
            }
            // 加载索引项
            // 读取到 索引数据时 需要设置最大可用的 cls_->sno_pages 的值
            sis_disk_v1_read_fulltext(cls_->index_fps, cls_, cb_sis_disk_v1_file_read_index);
            sis_disk_v1_file_read_dict(cls_);
            sis_disk_v1_close(cls_->index_fps);
        }        
    } 
    cls_->status = SIS_DISK_STATUS_OPENED;

    // 打开已有文件会先加载字典 写字典表如果发现和加载的字典表一样就不重复写入

    cls_->sno_size = 0;   // 当前块的总大小
    cls_->sno_series = 0; // 当前块的序号 每个新的page重新计数

    // 写文件前强制修正一次写入位置
    if (!sis_disk_v1_seek(cls_->work_fps))
    {
        return -8;
    }

    return 0;
}

// 写入结束标记，并生成索引文件
int sis_disk_v1_file_write_stop(s_sis_disk_v1_class *cls_)
{
    if (cls_->status == SIS_DISK_STATUS_CLOSED)
    {
        return 0;
    }
    // 根据文件类型写索引，并关闭文件
    sis_disk_v1_file_write_surplus(cls_);

    if (cls_->work_fps->main_head.index)
    {
        // printf("-6--ss-- %d\n", cls_->work_fps->main_head.wtime);
        cls_->index_fps->main_head.workers = cls_->work_fps->lists->count;
        // 当前索引就1个文件 以后有需求再说
        // 难点是要先统计索引的时间和热度，以及可以加载内存的数据量 来计算出把索引分为几个
        // 具体思路是 设定内存中给索引 1G 那么根据索引未压缩的数据量 按热度先后排序
        // 保留经常访问的数据到0号文件 其他分别放到1或者2号....
        // open后就会写索引头
        // 索引文件每次都重新写
        if (sis_disk_v1_open(cls_->index_fps, SIS_DISK_ACCESS_CREATE))
        {
            LOG(5)
            ("open idxfile fail.[%s] %d\n", cls_->index_fps->cur_name, cls_->index_fps->main_head.workers);
            return -2;
        }
        sis_disk_v1_file_write_index(cls_);
        sis_disk_v1_close(cls_->index_fps);
    }
    // 必须等索引写好才关闭工作句柄
    sis_disk_v1_close(cls_->work_fps);

    sis_disk_v1_class_clear(cls_);

    cls_->status = SIS_DISK_STATUS_CLOSED;
    return 0;
}
