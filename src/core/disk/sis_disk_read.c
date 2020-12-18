
#include "sis_disk.h"

///////////////////////////
//  s_sis_disk_reader
///////////////////////////
s_sis_disk_reader *sis_disk_reader_create(s_sis_disk_callback *cb_)
{
    s_sis_disk_reader *o = SIS_MALLOC(s_sis_disk_reader, o);
    o->callback = cb_;
    o->issub = 1;
    return o;
}
void sis_disk_reader_destroy(void *cls_)
{
    s_sis_disk_reader *info = (s_sis_disk_reader *)cls_;
    sis_disk_reader_clear(info);
    sis_free(info);
}
void sis_disk_reader_clear(s_sis_disk_reader *cls_)
{
    cls_->issub = 1;
    sis_sdsfree(cls_->keys);
    cls_->keys = NULL;
    sis_sdsfree(cls_->sdbs);
    cls_->sdbs = NULL;
}
void sis_disk_reader_set_key(s_sis_disk_reader *cls_, const char *in_)
{
    sis_sdsfree(cls_->keys);
    cls_->keys = sis_sdsnew(in_);
}
void sis_disk_reader_set_sdb(s_sis_disk_reader *cls_, const char *in_)
{
    sis_sdsfree(cls_->sdbs);
    cls_->sdbs = sis_sdsnew(in_);
}

void sis_disk_reader_set_stime(s_sis_disk_reader *cls_, int scale_, msec_t start_, msec_t stop_)
{
    switch (scale_)
    {
    case SIS_DISK_IDX_TYPE_MSEC:
        cls_->search_sno.start = start_;
        cls_->search_sno.stop = stop_;
        break;
    case SIS_DISK_IDX_TYPE_SEC:
        cls_->search_sec.start = start_;
        cls_->search_sec.stop = stop_;
        break;
    case SIS_DISK_IDX_TYPE_MIN:
        cls_->search_min.start = start_;
        cls_->search_min.stop = stop_;
        break;
    case SIS_DISK_IDX_TYPE_DAY:
        cls_->search_day.start = start_;
        cls_->search_day.stop = stop_;
        break;    
    case SIS_DISK_IDX_TYPE_NONE:
        break;
    default:
        cls_->search_int.start = start_;
        cls_->search_int.stop = stop_;
        break;
    }
}

void sis_disk_reader_get_stime(s_sis_disk_reader *cls_, int scale_, s_sis_msec_pair *pair_)
{
    switch (scale_)
    {
    case SIS_DISK_IDX_TYPE_MSEC:
        memmove(pair_, &cls_->search_sno, sizeof(s_sis_msec_pair));
        break;
    case SIS_DISK_IDX_TYPE_SEC:
        memmove(pair_, &cls_->search_sec, sizeof(s_sis_msec_pair));
        break;
    case SIS_DISK_IDX_TYPE_MIN:
        memmove(pair_, &cls_->search_min, sizeof(s_sis_msec_pair));
        break;
    case SIS_DISK_IDX_TYPE_DAY:
        memmove(pair_, &cls_->search_day, sizeof(s_sis_msec_pair));
        break;   
    case SIS_DISK_IDX_TYPE_NONE:
        pair_->start = 0;
        pair_->stop = 0;
        break;
    default:
        memmove(pair_, &cls_->search_int, sizeof(s_sis_msec_pair));
        break;
    }
}
int  sis_disk_get_idx_style(s_sis_disk_class *cls_,const char *sdb_, s_sis_disk_index_unit *iunit_)
{
    s_sis_disk_dict *sdb = (s_sis_disk_dict *)sis_map_list_get(cls_->sdbs, sdb_); 
    if (!sdb)
    {
        return SIS_DISK_IDX_TYPE_NONE;
    }
    s_sis_disk_dict_unit *unit = NULL; 
    if (iunit_)
    {
        unit = sis_disk_dict_get(sdb, iunit_->sdict); 
    }
    if (!unit)
    {
        unit = sis_disk_dict_last(sdb); 
    }
    if (unit && unit->db->field_mindex)
    {
        switch (unit->db->field_mindex->style)
        {
        case SIS_DYNAMIC_TYPE_INT:
        case SIS_DYNAMIC_TYPE_UINT:
            return SIS_DISK_IDX_TYPE_INT;  
        case SIS_DYNAMIC_TYPE_TICK:
            return SIS_DISK_IDX_TYPE_MSEC;  
        case SIS_DYNAMIC_TYPE_SEC:
            return SIS_DISK_IDX_TYPE_SEC;  
        case SIS_DYNAMIC_TYPE_MINU:
            return SIS_DISK_IDX_TYPE_MIN;  
        case SIS_DYNAMIC_TYPE_DATE:
            return SIS_DISK_IDX_TYPE_DAY;  
        default:
            break;
        }
    }
    return SIS_DISK_IDX_TYPE_NONE;
}
// int sis_reader_sub_filters(s_sis_disk_class *cls_, s_sis_disk_reader *reader_, s_sis_pointer_list *list_)
// {
//     if (!reader_ || !reader_->issub || !list_ || !reader_->keys || sis_sdslen(reader_->keys) < 1)
//     {
//         return 0;
//     }
//     sis_pointer_list_clear(list_);

//     s_sis_string_list *keys = sis_string_list_create();
//     if(!sis_strcasecmp(reader_->keys, "*"))
//     {
//         int count = sis_map_list_getsize(cls_->keys);
//         for (int i = 0; i < count; i++)
//         {
//             s_sis_disk_dict *dict = (s_sis_disk_dict *)sis_map_list_geti(cls_->keys, i);
//             sis_string_list_push(keys, SIS_OBJ_SDS(dict->name), sis_sdslen(SIS_OBJ_SDS(dict->name)));
//         }       
//     }
//     else
//     {
//         sis_string_list_load(keys, reader_->keys, sis_sdslen(reader_->keys), ",");
//     }
//     int issdbs = 0;
//     s_sis_string_list *sdbs = sis_string_list_create();
//     if (reader_->sdbs && sis_sdslen(reader_->sdbs) > 0)
//     {
//         issdbs = 1;
//         if(!sis_strcasecmp(reader_->sdbs, "*"))
//         {
//             int count = sis_map_list_getsize(cls_->sdbs);
//             for (int i = 0; i < count; i++)
//             {
//                 s_sis_disk_dict *dict = (s_sis_disk_dict *)sis_map_list_geti(cls_->sdbs, i);
//                 sis_string_list_push(sdbs, SIS_OBJ_SDS(dict->name), sis_sdslen(SIS_OBJ_SDS(dict->name)));
//             }               
//         }
//         else
//         {
//             sis_string_list_load(sdbs, reader_->sdbs, sis_sdslen(reader_->sdbs), ",");
//         }
//     }
    
//     char info[255];
//     for (int i = 0; i < sis_string_list_getsize(keys); i++)
//     {
//         if (issdbs)
//         {
//             for (int k = 0; k < sis_string_list_getsize(sdbs); k++)
//             {
//                 sis_sprintf(info, 255, "%s.%s", sis_string_list_get(keys, i), sis_string_list_get(sdbs, k));                
//                 s_sis_disk_index *node = (s_sis_disk_index *)sis_map_list_get(cls_->index_infos, info);
//                 if (node)
//                 {
//                     printf("%s : %s\n",node? SIS_OBJ_SDS(node->key) : "nil", info);
//                     sis_pointer_list_push(list_, node);
//                 }
//             }
//         }
//         else
//         {

//             s_sis_disk_index *node = (s_sis_disk_index *)sis_map_list_get(cls_->index_infos, sis_string_list_get(keys, i));
//             if (node)
//             {
//             // printf("%s : %s\n",node? SIS_OBJ_SDS(node->key) : "nil", sis_string_list_get(keys, i));
//                 sis_pointer_list_push(list_, node);
//             }            
//         }
//     }
//     sis_string_list_destroy(keys);
//     sis_string_list_destroy(sdbs);
//     return list_->count;
// }


int sis_reader_sub_filters(s_sis_disk_class *cls_, s_sis_disk_reader *reader_, s_sis_pointer_list *list_)
{
    if (!reader_ || !reader_->issub || !list_ || !reader_->keys || sis_sdslen(reader_->keys) < 1)
    {
        return 0;
    }
    sis_pointer_list_clear(list_);
    if(!sis_strcasecmp(reader_->keys, "*") && !sis_strcasecmp(reader_->sdbs, "*"))
    {
        // 如果是全部订阅
        for (int k = 0; k < sis_map_list_getsize(cls_->index_infos); k++)
        {
            s_sis_disk_index *node = (s_sis_disk_index *)sis_map_list_geti(cls_->index_infos, k);
            if (node)
            {
                sis_pointer_list_push(list_, node);
            }
        }  
        return list_->count;     
    }

    s_sis_string_list *keys = sis_string_list_create();
    if(!sis_strcasecmp(reader_->keys, "*"))
    {
        int count = sis_map_list_getsize(cls_->keys);
        for (int i = 0; i < count; i++)
        {
            s_sis_disk_dict *dict = (s_sis_disk_dict *)sis_map_list_geti(cls_->keys, i);
            sis_string_list_push(keys, SIS_OBJ_SDS(dict->name), sis_sdslen(SIS_OBJ_SDS(dict->name)));
        }       
    }
    else
    {
        sis_string_list_load(keys, reader_->keys, sis_sdslen(reader_->keys), ",");
    }
    int issdbs = 0;
    s_sis_string_list *sdbs = sis_string_list_create();
    if (reader_->sdbs && sis_sdslen(reader_->sdbs) > 0)
    {
        issdbs = 1;
        if(!sis_strcasecmp(reader_->sdbs, "*"))
        {
            int count = sis_map_list_getsize(cls_->sdbs);
            for (int i = 0; i < count; i++)
            {
                s_sis_disk_dict *dict = (s_sis_disk_dict *)sis_map_list_geti(cls_->sdbs, i);
                sis_string_list_push(sdbs, SIS_OBJ_SDS(dict->name), sis_sdslen(SIS_OBJ_SDS(dict->name)));
            }               
        }
        else
        {
            sis_string_list_load(sdbs, reader_->sdbs, sis_sdslen(reader_->sdbs), ",");
        }
    }
    
    char info[255];
    for (int i = 0; i < sis_string_list_getsize(keys); i++)
    {
        if (issdbs)
        {
            for (int k = 0; k < sis_string_list_getsize(sdbs); k++)
            {
                sis_sprintf(info, 255, "%s.%s", sis_string_list_get(keys, i), sis_string_list_get(sdbs, k));                
                s_sis_disk_index *node = (s_sis_disk_index *)sis_map_list_get(cls_->index_infos, info);
                if (node)
                {
                    // printf("%s : %s\n",node? SIS_OBJ_SDS(node->key) : "nil", info);
                    sis_pointer_list_push(list_, node);
                }
            }
        }
        else
        {

            s_sis_disk_index *node = (s_sis_disk_index *)sis_map_list_get(cls_->index_infos, sis_string_list_get(keys, i));
            if (node)
            {
            // printf("%s : %s\n",node? SIS_OBJ_SDS(node->key) : "nil", sis_string_list_get(keys, i));
                sis_pointer_list_push(list_, node);
            }            
        }
    }
    sis_string_list_destroy(keys);
    sis_string_list_destroy(sdbs);
    return list_->count;
}
////////////////////////////////////////////////////////
// read
////////////////////////////////////////////////////////

int _sis_disk_read_hid_log(s_sis_disk_class *cls_, s_sis_object *obj_)
{
    int count = 0;
    s_sis_disk_callback *callback = cls_->reader->callback; 

    s_sis_memory *memory = SIS_OBJ_MEMORY(obj_);
    while(sis_memory_get_size(memory) > 0)
    {
        // sis_out_binary("read: ", sis_memory(memory), 32);
        if (!sis_memory_try_ssize(memory))
        {
            break;
        }
        int keyi = sis_memory_get_ssize(memory);
        if (!sis_memory_try_ssize(memory))
        {
            break;
        }
        int sdbi = sis_memory_get_ssize(memory);
        s_sis_disk_dict *key = (s_sis_disk_dict *)sis_map_list_geti(cls_->keys, keyi);
        s_sis_disk_dict *sdb = (s_sis_disk_dict *)sis_map_list_geti(cls_->sdbs, sdbi);

        s_sis_disk_dict_unit *unit = sis_disk_dict_last(sdb);

        // printf("keyi = %d sdbi= %d  len= %d %zu \n ", keyi, sdbi, unit->db->size, sis_memory_get_size(memory));

        if (sis_memory_get_size(memory) >= unit->db->size)
        {
            count++;
            s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create());
            sis_memory_cat(SIS_OBJ_MEMORY(obj), sis_memory(memory), unit->db->size);
            sis_memory_move(memory, unit->db->size);
            if (callback && callback->cb_read)
            {
                callback->cb_read(callback->source,
                    SIS_OBJ_SDS(key->name), SIS_OBJ_SDS(sdb->name), obj);
            }            
            sis_object_destroy(obj);
        }   
    }
    // 这里memory==0 需要清理
    sis_memory_pack(memory);
    return count;
}
// 适配 {"k1","k2"} 和 k1,k2
s_sis_memory *_sis_disk_class_key_change(s_sis_memory *in)
{
    s_sis_memory *out = sis_memory_create();
    if (!sis_json_object_valid(sis_memory(in), sis_memory_get_size(in)))
    {
        sis_memory_clone(in, out);
    }
    else
    {
        s_sis_json_handle *injson = sis_json_load(sis_memory(in), sis_memory_get_size(in));
        if (!injson)
        {
            sis_memory_clone(in, out);
        }
        else
        {
            int nums = 0;
            s_sis_json_node *innode = sis_json_first_node(injson->node); 
            while (innode)
            {   
                // printf("%s\n",innode->key ? innode->key : "nil");
                if (nums > 0)
                {
                    sis_memory_cat(out, ",", 1);
                }
                nums++;
                sis_memory_cat(out, innode->key, sis_strlen(innode->key));
                innode = sis_json_next_node(innode);
            }
            sis_json_close(injson);
        }
    }    
    return out;
}

// 返回 解压后的数据长度 为0表示数据错误
// 到这里必须保证已经读取了 head 和 size 并保证数据区全部读入 in
// 不是删除的记录 也不是头 也不是尾

int cb_sis_disk_file_read_log(void *source_, s_sis_disk_head *head_, s_sis_object *obj_)
{
    s_sis_disk_class *cls_ = (s_sis_disk_class *)source_;
    s_sis_disk_callback *callback = cls_->reader->callback; 
    // 根据hid不同写入不同的数据到obj
    switch (head_->hid)
    {
    case SIS_DISK_HID_MSG_LOG: // 可能有多个【key + 一条数据】
        _sis_disk_read_hid_log(cls_, obj_);
        break;
    case SIS_DISK_HID_DICT_KEY:
        {
            s_sis_memory *memory = _sis_disk_class_key_change(SIS_OBJ_MEMORY(obj_));
            // newkeys
            sis_disk_class_set_key(cls_, false, sis_memory(memory), sis_memory_get_size(memory));
            if(callback && callback->cb_key)
            {
                callback->cb_key(callback->source, sis_memory(memory), sis_memory_get_size(memory));
            }
            sis_memory_destroy(memory);
        }        
        break;
    case SIS_DISK_HID_DICT_SDB:
        {
            s_sis_memory *memory = SIS_OBJ_MEMORY(obj_);
            sis_disk_class_set_sdb(cls_, false, sis_memory(memory), sis_memory_get_size(memory));
            if(callback && callback->cb_sdb)
            {
                callback->cb_sdb(callback->source, sis_memory(memory), sis_memory_get_size(memory));
            }
        }
        break;
    case SIS_DISK_HID_STREAM:
        {
            if(callback->cb_read)
            {
                callback->cb_read(callback->source, NULL, NULL, obj_);
            }
        }
        break;
    default:
        break;
    }
    if (cls_->isstop)
    {
        return -1;
    }
    return 0;
}

static int _sort_sno_rcatch(const void *arg1, const void *arg2 ) 
{ 
    char *ptr1 = *(char **)arg1;
    char *ptr2 = *(char **)arg2;
    // sis_out_binary("arg1:", arg1, 8);
    // sis_out_binary("arg2:", arg2, 8);
    // s_sis_disk_rcatch *rr = (s_sis_disk_rcatch *)ptr2;
    // printf("--- %p %p | %p %p | %d %s \n", arg1, arg2, ptr1, ptr2, rr->sno, SIS_OBJ_SDS(rr->key));
    return ((s_sis_disk_rcatch *)ptr1)->sno - ((s_sis_disk_rcatch *)ptr2)->sno;
}

int _sis_disk_read_hid_sno(s_sis_disk_class *cls_, s_sis_disk_index_unit *iunit_, s_sis_memory *in_)
{
    // 这里得到了实际的数据 可能有多个数据
    // 注意：从索引过来的信息，通常认为是正确的，工作文件的id可能因为dict变化而不准 但索引能保证字符串正确
    int count = 0;
    s_sis_memory *memory = in_;
    int keyi = sis_memory_get_ssize(memory);
    int sdbi = sis_memory_get_ssize(memory);
    s_sis_disk_dict *key = (s_sis_disk_dict *)sis_map_list_geti(cls_->keys, keyi);
    s_sis_disk_dict *sdb = (s_sis_disk_dict *)sis_map_list_geti(cls_->sdbs, sdbi); 
    if (!key||!sdb)
    {
        LOG(8)("no find .kid = %d : %d  sid = %d : %d\n", 
            keyi, sis_map_list_getsize(cls_->keys), sdbi, sis_map_list_getsize(cls_->sdbs));
        return 0;
    }
    s_sis_disk_dict_unit *unit = NULL; 
    if (iunit_)
    {
        unit = sis_disk_dict_get(sdb, iunit_->sdict); 
    }
    if (!unit)
    {
        unit = sis_disk_dict_last(sdb); 
    }

    while(sis_memory_get_size(memory) > 0)
    {
        uint64 sno = sis_memory_get_ssize(memory);
        // printf("count = %d sno = %d  %s %s\n", count, sno, SIS_OBJ_SDS(key->name), SIS_OBJ_SDS(sdb->name));
        s_sis_disk_rcatch *catch = sis_disk_rcatch_create(sno, key->name, sdb->name);
        s_sis_memory *output = SIS_OBJ_MEMORY(catch->output);
        sis_memory_cat(output, sis_memory(memory), unit->db->size);
        sis_memory_move(memory, unit->db->size);
        // sis_disk_rcatch_destroy(catch);
        sis_pointer_list_push(cls_->sno_rcatch, catch);
        count++;
    }
    // 这里memory==0 需要清理
    sis_memory_pack(memory);
    return count;
}
size_t _calc_sno_rcatch_size(s_sis_disk_class *cls_)
{
    size_t size = 0;
    for (int i = 0; i < cls_->sno_rcatch->count; i++)
    {       
        s_sis_disk_rcatch *catch = (s_sis_disk_rcatch *)sis_pointer_list_get(cls_->sno_rcatch, i);         
        size += sizeof(s_sis_disk_rcatch);
        size += SIS_OBJ_GET_SIZE(catch->key) + SIS_OBJ_GET_SIZE(catch->sdb);
        s_sis_memory *output = SIS_OBJ_MEMORY(catch->output);
        size += sizeof(s_sis_memory) + output->maxsize; 
    }
    return size;
}
int _sis_disk_read_hid_sno_end(s_sis_disk_class *cls_)
{
    // 这里新开一个线程来处理
    if (cls_->sno_rcatch->count > 0 && !cls_->isstop) // 数据读完了
    {
        // 一个块数据读完 对数据进行排序 
        // 指针列表排序 必须直接传入buffer
        // qsort(sis_pointer_list_first(cls_->sno_rcatch), cls_->sno_rcatch->count, sizeof(void *), _sort_sno_rcatch);
        printf("sno_rcatch : start %d\n", cls_->sno_rcatch->count);
        // printf("sno_rcatch_size = %zu \n", _calc_sno_rcatch_size(cls_));
        qsort(cls_->sno_rcatch->buffer, cls_->sno_rcatch->count, sizeof(void *), _sort_sno_rcatch);
        // sis_out_binary("rr:", (const char *)cls_->sno_rcatch->buffer, 24);
        // for (int i = 0; i < 20; i++)
        // {
        //     s_sis_disk_rcatch *rr =(s_sis_disk_rcatch *)sis_pointer_list_get(cls_->sno_rcatch, i);
        //     printf("[%p] %p list no = %d\n", sis_pointer_list_first(cls_->sno_rcatch), rr, rr->sno);     
        // }
        printf("sno_rcatch : stop %d %p\n", cls_->sno_rcatch->count, cls_->reader->callback->cb_read);
        // 然后发布出去
        if (cls_->reader->callback && cls_->reader->callback->cb_read)
        {
            // for (int i = 0; i < cls_->sno_rcatch->count; i++)
            // {       
            //     s_sis_disk_rcatch *catch = (s_sis_disk_rcatch *)sis_pointer_list_get(cls_->sno_rcatch, i);         
            //     cls_->reader->callback->cb_read(cls_->reader->callback->source,
            //         SIS_OBJ_SDS(catch->key), SIS_OBJ_SDS(catch->sdb), 
            //         catch->output);
            //     if (cls_->isstop)
            //     {
            //         break;
            //     }
            // }
        }
    }
    sis_pointer_list_clear(cls_->sno_rcatch);
    return 0;
}
int cb_sis_disk_file_read_sno(void *source_, s_sis_disk_head *head_, s_sis_object *obj_)
{
    s_sis_disk_class *cls_ = (s_sis_disk_class *)source_;
    s_sis_disk_callback *callback = cls_->reader->callback; 
    // 根据hid不同写入不同的数据到obj
    // printf("hid=%d\n", head_->hid);
    switch (head_->hid)
    {
    case SIS_DISK_HID_MSG_SNO: // 只有一个key + 可能多条数据
        _sis_disk_read_hid_sno(cls_, NULL, SIS_OBJ_MEMORY(obj_));
        break;
    case SIS_DISK_HID_SNO_END: // ??? 为什么只有一个sno
        printf("hid=%d\n", head_->hid);
        _sis_disk_read_hid_sno_end(cls_);
        break;
    case SIS_DISK_HID_DICT_KEY:
        {
            s_sis_memory *memory = _sis_disk_class_key_change(SIS_OBJ_MEMORY(obj_));
            // newkeys
            sis_disk_class_set_key(cls_, false, sis_memory(memory), sis_memory_get_size(memory));
            if(callback && callback->cb_key)
            {
                callback->cb_key(callback->source, sis_memory(memory), sis_memory_get_size(memory));
            }
            sis_memory_destroy(memory);
        }        
        break;
    case SIS_DISK_HID_DICT_SDB:
        {
            s_sis_memory *memory = SIS_OBJ_MEMORY(obj_);
            sis_disk_class_set_sdb(cls_, false, sis_memory(memory), sis_memory_get_size(memory));
            // printf("%s , cb_sdb = %p\n", __func__, callback->cb_sdb);
            if(callback && callback->cb_sdb)
            {
                callback->cb_sdb(callback->source, sis_memory(memory), sis_memory_get_size(memory));
            }
        }
        break;
    default:
        break;
    }
    if (cls_->isstop)
    {
        return -1;
    }
    return 0;
}
int sis_read_unit_from_index(s_sis_disk_class *cls_, uint8 *hid_, s_sis_disk_index_unit *iunit_, s_sis_memory *out_)
{
    // 此函数是否需要保存文件的原始位置
    int o = sis_files_read(cls_->work_fps, iunit_->fidx, iunit_->offset, iunit_->size, hid_, out_);
    // sis_out_binary("read index", sis_memory(out_),  sis_memory_get_size(out_));
    return o;
}

int sis_disk_read_sub_sno(s_sis_disk_class *cls_, s_sis_disk_reader *reader_)
{
    // 索引已加载
    // 根据索引定位文件位置 然后获取数据并发送
    s_sis_pointer_list *filters = sis_pointer_list_create(); 
    // 获取数据索引列表 --> filters
    sis_reader_sub_filters(cls_, reader_, filters);
    LOG(5)("sub filters count =  %d %d\n",filters->count, cls_->work_fps->main_head.wtime);
    if(filters->count < 1)
    {
        sis_pointer_list_destroy(filters);
        return 0;
    }
    // 得到最小的页码编号 
    // ??? 新的一天数据需要将页码设置为0
    int minpage = -1;
    for (int i = 0; i < filters->count; i++)
    {
        s_sis_disk_index *idxinfo = (s_sis_disk_index *)sis_pointer_list_get(filters, i);
        s_sis_disk_index_unit *unit = sis_struct_list_get(idxinfo->index, 0);
        if (!unit)
        {
            continue;
        }
        minpage = minpage == -1 ? unit->ipage : minpage < unit->ipage ? minpage : unit->ipage;
    }
    // 按页获取数据
    for (uint32 page = minpage; page < cls_->sno_pages; page++)
    {
        sis_pointer_list_clear(cls_->sno_rcatch);
        for (int i = 0; i < filters->count; i++)
        {
            s_sis_disk_index *idxinfo = (s_sis_disk_index *)sis_pointer_list_get(filters, i);
            s_sis_disk_index_unit *unit = sis_struct_list_get(idxinfo->index, idxinfo->cursor);
            // printf("%p %d %d %d: %s %s\n", unit, page, unit ? unit->ipage : 0, cls_->sno_pages, SIS_OBJ_GET_CHAR(idxinfo->key), SIS_OBJ_GET_CHAR(idxinfo->sdb));
            if (!unit)
            {
                continue;
            }
            if (unit->ipage == page)
            {
                // 一页中同一个数据只有一份
                // 先读取指定块 然后写入sno的缓存中
                s_sis_memory *memory = sis_memory_create();
                if (sis_read_unit_from_index(cls_, NULL, unit, memory) > 0)
                {
                    _sis_disk_read_hid_sno(cls_, unit, memory);
                }
                else
                {
                    LOG(5)("index fail.\n");
                }                
                sis_memory_destroy(memory);
                idxinfo->cursor++;
            }
            if (cls_->isstop)
            {
                break;
            }
        }
        // printf("sno_rcatch_size = %zu \n", _calc_sno_rcatch_size(cls_));
        _sis_disk_read_hid_sno_end(cls_);
        // printf("sno_rcatch_size = %zu \n", _calc_sno_rcatch_size(cls_));
        // 这里应该释放了内存
        if (cls_->isstop)
        {
            break;
        }
    }
    printf("sub stop\n");
    sis_pointer_list_clear(cls_->sno_rcatch);
    sis_pointer_list_destroy(filters);
    return 0; 
}

int _sis_disk_rget_hid_sno(s_sis_disk_class *cls_, s_sis_disk_index_unit *iunit_, s_sis_memory *inmem_, s_sis_object *obj_)
{
    // 这里得到了实际的数据 可能有多个数据
    // 注意：从索引过来的信息，通常认为是正确的，工作文件的id可能因为dict变化而不准 但索引能保证字符串正确
    int count = 0;
    s_sis_memory *memory = inmem_;
    int keyi = sis_memory_get_ssize(memory);
    int sdbi = sis_memory_get_ssize(memory);
    s_sis_disk_dict *key = (s_sis_disk_dict *)sis_map_list_geti(cls_->keys, keyi);
    s_sis_disk_dict *sdb = (s_sis_disk_dict *)sis_map_list_geti(cls_->sdbs, sdbi); 
    if (!key||!sdb)
    {
        LOG(8)("no find .kid = %d : %d  sid = %d : %d\n", 
            keyi, sis_map_list_getsize(cls_->keys), sdbi, sis_map_list_getsize(cls_->sdbs));
        return 0;
    }
    s_sis_disk_dict_unit *unit = NULL; 
    if (iunit_)
    {
        unit = sis_disk_dict_get(sdb, iunit_->sdict); 
    }
    if (!unit)
    {
        unit = sis_disk_dict_last(sdb); 
    }
    
    while(sis_memory_get_size(memory) >= unit->db->size)
    {
        count++;
        sis_memory_get_ssize(memory);
        // printf("count = %d sno = %d  %s %s\n", count, sno, SIS_OBJ_SDS(key->name), SIS_OBJ_SDS(sdb->name));
        s_sis_memory *output = SIS_OBJ_MEMORY(obj_);
        sis_memory_cat(output, sis_memory(memory), unit->db->size);
        sis_memory_move(memory, unit->db->size);        
    }
    sis_memory_pack(memory);
    return count;
}
int _sis_disk_rget_hid_sdb(s_sis_disk_class *cls_, s_sis_disk_index_unit *iunit_, s_sis_memory *inmem_, s_sis_object *obj_)
{
    int count = 0;
    s_sis_memory *memory = inmem_;
    int keyi = sis_memory_get_ssize(memory);
    int sdbi = sis_memory_get_ssize(memory);
    s_sis_disk_dict *key = (s_sis_disk_dict *)sis_map_list_geti(cls_->keys, keyi);
    s_sis_disk_dict *sdb = (s_sis_disk_dict *)sis_map_list_geti(cls_->sdbs, sdbi); 
    if (!key||!sdb)
    {
        LOG(8)("no find .kid = %d : %d  sid = %d : %d\n", 
            keyi, sis_map_list_getsize(cls_->keys), sdbi, sis_map_list_getsize(cls_->sdbs));
        return 0;
    }
    
    s_sis_disk_dict_unit *unit = NULL; 
    if (iunit_)
    {
        unit = sis_disk_dict_get(sdb, iunit_->sdict); 
    }
    if (!unit)
    {
        unit = sis_disk_dict_last(sdb); 
    }
    while (sis_memory_get_size(memory) >= unit->db->size)
    {
        count++;
        sis_memory_cat(SIS_OBJ_MEMORY(obj_), sis_memory(memory), unit->db->size);
        sis_memory_move(memory, unit->db->size);
    } 
    sis_memory_pack(memory);    
    return count;
}
int _sis_disk_rget_hid_kdb(s_sis_disk_class *cls_, s_sis_disk_index_unit *iunit_, s_sis_memory *inmem_, s_sis_object *obj_)
{
    int count = 0;

    s_sis_memory *memory = inmem_;
    int klen = sis_memory_get_ssize(memory);
    s_sis_sds key = sis_sdsnewlen(sis_memory(memory), klen);
    sis_memory_move(memory, klen);
    int sdbi = sis_memory_get_ssize(memory);

    s_sis_disk_dict *sdb = (s_sis_disk_dict *)sis_map_list_geti(cls_->sdbs, sdbi);
    if (!sdb)
    {
        LOG(8)("no find .sid = %d : %d\n", sdbi, sis_map_list_getsize(cls_->sdbs));
        return 0;
    }
    // 和log不同就在于字典的获取
    s_sis_disk_dict_unit *unit = NULL; 
    if (iunit_)
    {
        unit = sis_disk_dict_get(sdb, iunit_->sdict); 
    }
    if (!unit)
    {
        unit = sis_disk_dict_last(sdb); 
    }

    while (sis_memory_get_size(memory) >= unit->db->size)
    {
        count++;
        sis_memory_cat(SIS_OBJ_MEMORY(obj_), sis_memory(memory), unit->db->size);
        sis_memory_move(memory, unit->db->size);
    }
    sis_memory_pack(memory);    
    sis_sdsfree(key); 
    return count;
}
int _sis_disk_rget_hid_key(s_sis_disk_class *cls_, s_sis_disk_index_unit *iunit_, s_sis_memory *inmem_, s_sis_object *obj_)
{
   int count = 0;
    s_sis_memory *memory = inmem_;
    int keyi = sis_memory_get_ssize(memory);
    s_sis_disk_dict *key = (s_sis_disk_dict *)sis_map_list_geti(cls_->keys, keyi);
    if (!key)
    {
        LOG(8)("no find .kid = %d : %d\n", keyi, sis_map_list_getsize(cls_->keys));
        return 0;
    }

    int vlen = sis_memory_get_ssize(memory);

    if (sis_memory_get_size(memory) >= vlen)
    {
        count = 1;
        sis_memory_cat(SIS_OBJ_MEMORY(obj_), sis_memory(memory), vlen);
        sis_memory_move(memory, vlen);
    }
    return count;
}
int _sis_disk_rget_hid_any(s_sis_disk_class *cls_, s_sis_disk_index_unit *iunit_, s_sis_memory *inmem_, s_sis_object *obj_)
{
    int count = 0;

    s_sis_memory *memory = inmem_;
    int klen = sis_memory_get_ssize(memory);
    s_sis_sds key = sis_sdsnewlen(sis_memory(memory), klen);
    sis_memory_move(memory, klen);
    int vlen = sis_memory_get_ssize(memory);
    printf("read any: %s : %d\n", key, vlen);

    if (sis_memory_get_size(memory) >= vlen)
    {
        count = 1;
        sis_memory_cat(SIS_OBJ_MEMORY(obj_), sis_memory(memory), vlen);
        sis_memory_move(memory, vlen);
    }
    sis_sdsfree(key); 
    return count;
}
////////////////////////

int _sis_disk_read_hid_sdb(s_sis_disk_class *cls_, s_sis_disk_index_unit *iunit_, s_sis_memory *inmem_)
{
    int count = 0;
    s_sis_disk_callback *callback = cls_->reader->callback; 

    s_sis_memory *memory = inmem_;

    int keyi = sis_memory_get_ssize(memory);
    int sdbi = sis_memory_get_ssize(memory);

    if (!cls_->reader->issub)
    {
        return 0;
    }
    s_sis_disk_dict *key = (s_sis_disk_dict *)sis_map_list_geti(cls_->keys, keyi);
    s_sis_disk_dict *sdb = (s_sis_disk_dict *)sis_map_list_geti(cls_->sdbs, sdbi); 
    
    s_sis_disk_dict_unit *unit = NULL; 
    if (iunit_)
    {
        unit = sis_disk_dict_get(sdb, iunit_->sdict); 
    }
    if (!unit)
    {
        unit = sis_disk_dict_last(sdb); 
    }
    if (cls_->reader->isone)
    {
        s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create());
        sis_memory_cat(SIS_OBJ_MEMORY(obj), sis_memory(memory), sis_memory_get_size(memory));
        if (callback && callback->cb_read)
        {
            callback->cb_read(callback->source,
                SIS_OBJ_SDS(key->name), SIS_OBJ_SDS(sdb->name), obj);
        }            
        sis_object_destroy(obj);
    }
    else
    {
        while (sis_memory_get_size(memory) >= unit->db->size)
        {
            count++;
            s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create());
            sis_memory_cat(SIS_OBJ_MEMORY(obj), sis_memory(memory), unit->db->size);
            sis_memory_move(memory, unit->db->size);
            if (callback && callback->cb_read)
            {
                callback->cb_read(callback->source,
                    SIS_OBJ_SDS(key->name), SIS_OBJ_SDS(sdb->name), obj);
            }            
            sis_object_destroy(obj);
        }  
        sis_memory_pack(memory);        
    }
    return count;
}
int _sis_disk_read_hid_kdb(s_sis_disk_class *cls_, s_sis_disk_index_unit *iunit_, s_sis_memory *inmem_)
{
    int count = 0;
    s_sis_disk_callback *callback = cls_->reader->callback; 

    s_sis_memory *memory = inmem_;
    int klen = sis_memory_get_ssize(memory);
    s_sis_sds key = sis_sdsnewlen(sis_memory(memory), klen);
    sis_memory_move(memory, klen);
    int sdbi = sis_memory_get_ssize(memory);

    if (!cls_->reader->issub)
    {
        sis_sdsfree(key); 
        return 0;
    }
    s_sis_disk_dict *sdb = (s_sis_disk_dict *)sis_map_list_geti(cls_->sdbs, sdbi);
    // 和log不同就在于字典的获取
    s_sis_disk_dict_unit *unit = NULL; 
    if (iunit_)
    {
        unit = sis_disk_dict_get(sdb, iunit_->sdict); 
    }
    if (!unit)
    {
        unit = sis_disk_dict_last(sdb); 
    }

    if (cls_->reader->isone)
    {
        s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create());
        sis_memory_cat(SIS_OBJ_MEMORY(obj), sis_memory(memory), sis_memory_get_size(memory));
        if (callback && callback->cb_read)
        {
            callback->cb_read(callback->source,
                key, SIS_OBJ_SDS(sdb->name), obj);
        }            
        sis_object_destroy(obj);
    }
    else
    {
        while (sis_memory_get_size(memory) >= unit->db->size)
        {
            count++;
            s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create());
            sis_memory_cat(SIS_OBJ_MEMORY(obj), sis_memory(memory), unit->db->size);
            sis_memory_move(memory, unit->db->size);
            if (callback && callback->cb_read)
            {
                callback->cb_read(callback->source,
                                    key, SIS_OBJ_SDS(sdb->name), obj);
            }
            sis_object_destroy(obj);
        }
        sis_memory_pack(memory);
    }
    sis_sdsfree(key); 
    return count;
}
int _sis_disk_read_hid_key(s_sis_disk_class *cls_, s_sis_disk_index_unit *iunit_, s_sis_memory *inmem_)
{
   int count = 0;
    s_sis_disk_callback *callback = cls_->reader->callback; 

    s_sis_memory *memory = inmem_;
    int keyi = sis_memory_get_ssize(memory);
    s_sis_disk_dict *key = (s_sis_disk_dict *)sis_map_list_geti(cls_->keys, keyi);
    int vlen = sis_memory_get_ssize(memory);

    // printf("%d %d %zu\n", keyi, vlen, sis_memory_get_size(memory));
    if (!cls_->reader->issub)
    {
        return 0;
    }
    if (sis_memory_get_size(memory) >= vlen)
    {
        count = 1;
        s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create());
        sis_memory_cat(SIS_OBJ_MEMORY(obj), sis_memory(memory), vlen);
        sis_memory_move(memory, vlen);
        if (callback && callback->cb_read)
        {
            callback->cb_read(callback->source,
                SIS_OBJ_SDS(key->name), NULL, obj);
        }            
        sis_object_destroy(obj);
    }
    return count;
}
int _sis_disk_read_hid_any(s_sis_disk_class *cls_, s_sis_disk_index_unit *iunit_, s_sis_memory *inmem_)
{
    int count = 0;
    s_sis_disk_callback *callback = cls_->reader->callback; 

    s_sis_memory *memory = inmem_;
    int klen = sis_memory_get_ssize(memory);
    s_sis_sds key = sis_sdsnewlen(sis_memory(memory), klen);
    sis_memory_move(memory, klen);
    int vlen = sis_memory_get_ssize(memory);
    printf("read any: %s : %d\n", key, vlen);

    if (!cls_->reader->issub)
    {
        sis_sdsfree(key); 
        return 0;
    }
    if (sis_memory_get_size(memory) >= vlen)
    {
        count = 1;
        s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create());
        sis_memory_cat(SIS_OBJ_MEMORY(obj), sis_memory(memory), vlen);
        sis_memory_move(memory, vlen);
        if (callback && callback->cb_read)
        {
            callback->cb_read(callback->source,
                key, NULL, obj);
        }            
        sis_object_destroy(obj);
    }
    sis_sdsfree(key); 
    return count;
}

// 返回 解压后的数据长度 为0表示数据错误
// 到这里必须保证已经读取了 head 和 size 并保证数据区全部读入 in
// 不是删除的记录 也不是头 也不是尾

size_t sis_disk_file_read_of_index(s_sis_disk_class *cls_, uint8 hid_, s_sis_disk_index_unit *iunit_, s_sis_memory *inmem_)
{
    // 根据hid不同写入不同的数据到obj
    // printf("+++ %d %zu\n", hid_, iunit_->offset);
    switch (hid_)
    {
    case SIS_DISK_HID_MSG_SDB: // 只有一个key + 可能多条数据
        _sis_disk_read_hid_sdb(cls_, iunit_, inmem_);
        break;
    case SIS_DISK_HID_MSG_KDB:
        _sis_disk_read_hid_kdb(cls_, iunit_, inmem_);
        break;
    case SIS_DISK_HID_MSG_KEY:
        _sis_disk_read_hid_key(cls_, iunit_, inmem_);
        break;
    case SIS_DISK_HID_MSG_ANY:
        _sis_disk_read_hid_any(cls_, iunit_, inmem_);
        break;
    default:
        break;
    }
    return 0;
}

size_t sis_disk_file_rget_of_index(s_sis_disk_class *cls_, uint8 hid_, s_sis_disk_index_unit *iunit_, s_sis_memory *inmem_, s_sis_object *obj_)
{
    // 根据hid不同写入不同的数据到obj
    printf("+++ %d %zu | %zu %zu\n", hid_, iunit_->offset, iunit_->size, sis_memory_get_size(inmem_));
    switch (hid_)
    {
    case SIS_DISK_HID_MSG_SNO: // 只有一个key + 可能多条数据
        _sis_disk_rget_hid_sno(cls_, iunit_, inmem_, obj_);
        break;
    case SIS_DISK_HID_MSG_SDB: // 只有一个key + 可能多条数据
        _sis_disk_rget_hid_sdb(cls_, iunit_, inmem_, obj_);
        break;
    case SIS_DISK_HID_MSG_KDB:
        _sis_disk_rget_hid_kdb(cls_, iunit_, inmem_, obj_);
        break;
    case SIS_DISK_HID_MSG_KEY:
        _sis_disk_rget_hid_key(cls_, iunit_, inmem_, obj_);
        break;
    case SIS_DISK_HID_MSG_ANY:
        _sis_disk_rget_hid_any(cls_, iunit_, inmem_, obj_);
        break;
    default:
        break;
    }
    return 0;
}
                // sis_memory_cat(SIS_OBJ_MEMORY(obj), sis_memory(memory), sis_memory_get_size(memory));

int sis_disk_read_sub_sdb(s_sis_disk_class *cls_, s_sis_disk_reader *reader_)
{
    // 索引已加载
    // 根据索引定位文件位置 然后获取数据并发送
    s_sis_pointer_list *filters = sis_pointer_list_create(); 
    // 获取数据索引列表 --> filters
    sis_reader_sub_filters(cls_, reader_, filters);
    
    // printf("filters : %d \n", filters->count);
    if(filters->count < 1)
    {
        sis_pointer_list_destroy(filters);
        return 0;
    }
    s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create());
    s_sis_memory *memory = SIS_OBJ_MEMORY(obj);

    for (int i = 0; i < filters->count; i++)
    {
        s_sis_disk_index *idxinfo = (s_sis_disk_index *)sis_pointer_list_get(filters, i);
        if (!idxinfo)
        {
            continue;
        }
        for (int k = 0; k < idxinfo->index->count; k++)
        {
            s_sis_disk_index_unit *unit = sis_struct_list_get(idxinfo->index, k);
            if (idxinfo->sdb)
            {
                s_sis_msec_pair search;
                int style = sis_disk_get_idx_style(cls_, SIS_OBJ_GET_CHAR(idxinfo->sdb), unit);
                sis_disk_reader_get_stime(reader_, style, &search);

                // printf(" %d %d %d %d\n", search.start, search.stop, unit->start, unit->stop);
                if (sis_msec_pair_whole(&search) || 
                    (unit->stop == 0 && unit->start == 0) ||
                    sis_is_mixed(search.start, search.stop, unit->start, unit->stop) )
                {
                    uint8 hid = 0;
                    sis_memory_clear(memory);
                    if (sis_read_unit_from_index(cls_, &hid, unit, memory) > 0)
                    {
                        sis_disk_file_read_of_index(cls_, hid, unit, memory);
                    }     
                }
            }
            else
            {
                // 单键值处理
                uint8 hid = 0;
                sis_memory_clear(memory);
                if (sis_read_unit_from_index(cls_, &hid, unit, memory) > 0)
                {
                    sis_disk_file_read_of_index(cls_, hid, unit, memory);
                }                     
            }
            
        }  
    }
    sis_object_destroy(obj);
    sis_pointer_list_destroy(filters);
    return 0;
}

s_sis_object *sis_disk_read_get_obj(s_sis_disk_class *cls_, s_sis_disk_reader *reader_)
{
    // 非订阅方式只能获取唯一值 key+sdb
    if (!reader_ || reader_->issub || !reader_->keys || sis_sdslen(reader_->keys) < 1)
    {
        return NULL;
    }
    char info[255];
    if (reader_->sdbs || sis_sdslen(reader_->sdbs))
    {
        sis_sprintf(info, 255, "%s.%s", reader_->keys, reader_->sdbs);
    }   
    else
    {
        sis_sprintf(info, 255, "%s", reader_->keys);
    }
    s_sis_disk_index *idxinfo = (s_sis_disk_index *)sis_map_list_get(cls_->index_infos, info);
    if (!idxinfo)
    {
        return NULL;
    }

    s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create());

    s_sis_memory *memory = sis_memory_create();
    for (int k = 0; k < idxinfo->index->count; k++)
    {
        s_sis_disk_index_unit *unit = sis_struct_list_get(idxinfo->index, k);
        printf("%s %d | %d %d %d %d\n",__func__, idxinfo->index->count, (int)reader_->search_sno.start, (int)reader_->search_sno.stop, (int)unit->start, (int)unit->stop);
        s_sis_msec_pair search;
        int style = sis_disk_get_idx_style(cls_, SIS_OBJ_GET_CHAR(idxinfo->sdb), unit);
        sis_disk_reader_get_stime(reader_, style, &search);

        if (sis_msec_pair_whole(&search)|| 
            (unit->stop == 0 && unit->start == 0) ||
            sis_is_mixed(search.start, search.stop, unit->start, unit->stop) )
        {
            uint8 hid = 0;
            sis_memory_clear(memory);
            if (sis_read_unit_from_index(cls_, &hid, unit, memory) > 0)
            {
                sis_disk_file_rget_of_index(cls_, hid, unit, memory, obj);
                // sis_memory_cat(SIS_OBJ_MEMORY(obj), sis_memory(memory), sis_memory_get_size(memory));
            }     
        }
    } 
    sis_memory_destroy(memory);
    // 如果严格卡时间可以在这里做过滤
    if (reader_->callback)
    {
        if (sis_memory_get_size(SIS_OBJ_MEMORY(obj)) > 0 && reader_->callback->cb_read)
        {
            reader_->callback->cb_read(reader_->callback->source, reader_->keys, reader_->sdbs, obj);
        }
        // 如果有回调就释放
        sis_object_destroy(obj);
        return NULL;
    }
    // 没有回调表示堵塞获取数据
    return obj;
}
////////////////////
//  read index
///////////////////
int sis_disk_file_read_dict(s_sis_disk_class *cls_)
{
    s_sis_memory *memory = sis_memory_create();
    {
        s_sis_disk_index *node = (s_sis_disk_index *)sis_map_list_get(cls_->index_infos, SIS_DISK_SIGN_KEY);
        if (node)
        {
            for (int k = 0; k < node->index->count; k++)
            {
                s_sis_disk_index_unit *unit = sis_disk_index_get_unit(node, k);
                sis_memory_clear(memory);
                if (sis_read_unit_from_index(cls_, NULL, unit, memory) > 0)
                {
                    // sis_out_binary("key", sis_memory(memory), sis_memory_get_size(memory));
                    // newkeys
                    s_sis_memory *newmemory = _sis_disk_class_key_change(memory);
                    // sis_out_binary("newkeys", sis_memory(newmemory), sis_memory_get_size(newmemory));
                    sis_disk_class_set_key(cls_, false, sis_memory(newmemory), sis_memory_get_size(newmemory));
                    sis_memory_destroy(newmemory);
                }
            }
        }
    }
    {
        s_sis_disk_index *node = (s_sis_disk_index *)sis_map_list_get(cls_->index_infos, SIS_DISK_SIGN_SDB);
        if (node)
        {
            
            for (int k = 0; k < node->index->count; k++)
            {
                s_sis_disk_index_unit *unit = sis_disk_index_get_unit(node, k);
                sis_memory_clear(memory);
                if (sis_read_unit_from_index(cls_, NULL, unit, memory) > 0)
                {
                    // sis_out_binary("sdb", sis_memory(memory), 64);
                    sis_disk_class_set_sdb(cls_, false, sis_memory(memory), sis_memory_get_size(memory));
                }
            }
        }
    }
    sis_memory_destroy(memory);
    return 0;
}

int cb_sis_disk_file_read_index(void *source_, s_sis_disk_head *head_, s_sis_object *obj_)
{
    s_sis_disk_class *cls_ = (s_sis_disk_class *)source_;
    // 直接写到内存中
    int  maxpages = 0;
    int  count = 0;
    char name[255];
    s_sis_memory *memory = SIS_OBJ_MEMORY(obj_);
    while(sis_memory_get_size(memory) > 0)
    {
        if (head_->hid == SIS_DISK_HID_INDEX_KEY)
        {            
            s_sis_disk_index *node = sis_disk_index_create(NULL, NULL);
            
            int blocks = sis_memory_get_ssize(memory);
            for (int i = 0; i < blocks; i++)
            {
                s_sis_disk_index_unit unit;
                memset(&unit, 0, sizeof(s_sis_disk_index_unit));
                unit.fidx = sis_memory_get_ssize(memory);
                unit.offset = sis_memory_get_ssize(memory);
                unit.size = sis_memory_get_ssize(memory);
                sis_struct_list_push(node->index, &unit);
            }
            sis_map_list_set(cls_->index_infos, SIS_DISK_SIGN_KEY, node);   
        }
        else if (head_->hid == SIS_DISK_HID_INDEX_SDB)
        {
            s_sis_disk_index *node = sis_disk_index_create(NULL, NULL);
            int blocks = sis_memory_get_ssize(memory);
            for (int i = 0; i < blocks; i++)
            {
                s_sis_disk_index_unit unit;
                memset(&unit, 0, sizeof(s_sis_disk_index_unit));
                unit.fidx = sis_memory_get_ssize(memory);
                unit.offset = sis_memory_get_ssize(memory);
                unit.size = sis_memory_get_ssize(memory);
                sis_struct_list_push(node->index, &unit);
            }
            sis_map_list_set(cls_->index_infos, SIS_DISK_SIGN_SDB, node);   
        }
        else
        {
            int s1 = sis_memory_get_ssize(memory);
            s_sis_object *key = sis_object_create(SIS_OBJECT_SDS, sis_sdsnewlen(sis_memory(memory), s1));
            sis_memory_move(memory, s1);
            int s2 = sis_memory_get_ssize(memory);
            s_sis_object *sdb = NULL;
            if (s2 > 0)
            {
                sdb = sis_object_create(SIS_OBJECT_SDS, sis_sdsnewlen(sis_memory(memory), s2));
                sis_memory_move(memory, s2);
                sis_sprintf(name, 255, "%s.%s", SIS_OBJ_SDS(key), SIS_OBJ_SDS(sdb));
            }
            else
            {
                sis_sprintf(name, 255, "%s", SIS_OBJ_SDS(key));
            }
            // printf("index : %s %s  %d name = %s\n", SIS_OBJ_SDS(key), sdb ? SIS_OBJ_SDS(sdb) : "", s2, name);
            s_sis_disk_index *node = sis_disk_index_create(key, sdb);
            int blocks = sis_memory_get_ssize(memory);
            for (int i = 0; i < blocks; i++)
            {
                s_sis_disk_index_unit unit;
                unit.active = sis_memory_get_byte(memory, 1);
                unit.kdict = sis_memory_get_byte(memory, 1);
                unit.sdict = sis_memory_get_byte(memory, 1);
                unit.fidx = sis_memory_get_byte(memory, 1);
                unit.offset = sis_memory_get_ssize(memory);
                unit.size = sis_memory_get_ssize(memory);
                unit.start = sis_memory_get_ssize(memory);
                if (head_->hid == SIS_DISK_HID_INDEX_SNO)
                {
                    unit.ipage = sis_memory_get_ssize(memory);  
                    maxpages = sis_max(maxpages, unit.ipage);
                    unit.stop = 0;    
                }
                else
                {
                    unit.ipage = 0;
                    unit.stop = unit.start + sis_memory_get_ssize(memory);    
                }
                // printf("    [%d] : %d %d  - %d\n", i, (int)unit.start, (int)unit.stop, unit.ipage);
                sis_struct_list_push(node->index, &unit);
            }
            sis_object_destroy(key);
            sis_object_destroy(sdb);
            // printf(" +++ %s : %d %d  - %d\n", name);
            sis_map_list_set(cls_->index_infos, name, node);   
        }
        count++;
    }
    // 这里memory==0 需要清理
    sis_memory_pack(memory);
    cls_->sno_pages = maxpages + 1; // 表示新写入的 page 以此为准
    if (cls_->isstop)
    {
        return -1;
    }
    return count;
}
void _disk_file_call_dict(s_sis_disk_class *cls_, s_sis_disk_callback *callback)
{
    if (callback->cb_key)
    {
        s_sis_sds msg = sis_disk_file_get_keys(cls_, false);
        if (sis_sdslen(msg) > 2) 
        {
            callback->cb_key(callback->source, msg, sis_sdslen(msg));
        }
        sis_sdsfree(msg);
    }
    if (callback->cb_sdb)
    {
        s_sis_sds msg = sis_disk_file_get_sdbs(cls_, false);
        if (sis_sdslen(msg) > 2) 
        {
            callback->cb_sdb(callback->source, msg, sis_sdslen(msg));
        }
        sis_sdsfree(msg);                
    }
}
// 只支持 stream log sno sdb
int sis_disk_file_read_sub(s_sis_disk_class *cls_, s_sis_disk_reader *reader_)
{
    if (!reader_ || !reader_->callback)
    {
        return -1;
    }
    cls_->reader = reader_;
    cls_->isstop = false;

    s_sis_disk_callback *callback = reader_->callback;   
    if(callback->cb_begin)
    {
        callback->cb_begin(callback->source, cls_->work_fps->main_head.wtime);
    }
    reader_->issub = 1;
    switch (cls_->work_fps->main_head.style)
    {
    case SIS_DISK_TYPE_STREAM:
        // 每次都从头读起
        sis_files_read_fulltext(cls_->work_fps, cls_, cb_sis_disk_file_read_log);
        break;
    case SIS_DISK_TYPE_LOG:
        // 清理key 和 dbs 序列表 
        // 每次都从头读起
        sis_map_list_clear(cls_->keys);
        sis_map_list_clear(cls_->sdbs);
        sis_files_read_fulltext(cls_->work_fps, cls_, cb_sis_disk_file_read_log);
        break;
    case SIS_DISK_TYPE_SNO:
        // 因为有索引，所以文件打开时就已经加载了keys和sdbs
        // 需要把同一时间块的数据全部读完后 排序播放后 才能读取下一个时间块的数据
        
        if (!sis_strcasecmp(reader_->keys, "*") && !sis_strcasecmp(reader_->sdbs, "*"))
        {
            // 顺序读取所有文件
            sis_files_read_fulltext(cls_->work_fps, cls_, cb_sis_disk_file_read_sno);
        }
        else
        {
            _disk_file_call_dict(cls_, callback);
            sis_disk_read_sub_sno(cls_, reader_);  
        }
        break;
    default:  // SIS_DISK_TYPE_SDB
    // sdb 因为有废弃的数据 所以只能通过索引去读取数据
    // 如果索引丢弃 原则是后面的key覆盖前面的key
        {
            _disk_file_call_dict(cls_, callback);
            sis_disk_read_sub_sdb(cls_, reader_);  
        }
        break;
    }
    // 不是因为中断而结束
    if(callback->cb_end && !cls_->isstop)
    {
        callback->cb_end(callback->source, cls_->work_fps->main_head.wtime);
    }
    cls_->reader = NULL;
    return 0;
}

// 只支持 sno sdb
// 只能获取单一key和sdb的数据
s_sis_object *sis_disk_file_read_get_obj(s_sis_disk_class *cls_, s_sis_disk_reader *reader_)
{
    if (cls_->work_fps->main_head.style != SIS_DISK_TYPE_SNO &&
        cls_->work_fps->main_head.style != SIS_DISK_TYPE_SDB)
    {
        return NULL;
    }
    if (!reader_)
    {
        return NULL;
    }
    cls_->reader = reader_;
    cls_->isstop = false;
    reader_->issub = 0;

    s_sis_object *obj = sis_disk_read_get_obj(cls_, reader_);

    cls_->reader = NULL;
    return obj;
}



// 只读数据
int sis_disk_file_read_start(s_sis_disk_class *cls_)
{
    if (!cls_->isinit)
    {
        return SIS_DISK_CMD_NO_INIT;
    }
    // 对已经存在的文件进行合法性检查 如果文件不完整 就打开失败 由外部程序来处理异常，这样相对安全
    if (!sis_file_exists(cls_->work_fps->cur_name))
    {
        return SIS_DISK_CMD_NO_EXISTS;
    }
    if (cls_->work_fps->main_head.index)
    {
        if (!sis_file_exists(cls_->index_fps->cur_name))
        {
            LOG(5)
            ("idxfile no exists.[%s]\n", cls_->index_fps->cur_name);
            return SIS_DISK_CMD_NO_EXISTS_IDX;
        }
    }
    int o = sis_files_open(cls_->work_fps, SIS_DISK_ACCESS_RDONLY);
    if (o)
    {
        LOG(5)
        ("open file fail.[%s:%d]\n", cls_->work_fps->cur_name, o);
        return SIS_DISK_CMD_NO_OPEN;
    }
    if (cls_->work_fps->main_head.index)
    {
        sis_map_list_clear(cls_->index_infos);
        if (sis_files_open(cls_->index_fps, SIS_DISK_ACCESS_RDONLY))
        {
            LOG(5)
            ("open idxfile fail.[%s]\n", cls_->index_fps->cur_name);
            return SIS_DISK_CMD_NO_OPEN_IDX;
        }
        sis_files_read_fulltext(cls_->index_fps, cls_, cb_sis_disk_file_read_index);
        sis_disk_file_read_dict(cls_);
        sis_files_close(cls_->index_fps);
    }
    cls_->status = SIS_DISK_STATUS_OPENED;
    return SIS_DISK_CMD_OK;
}

// 关闭文件
int sis_disk_file_read_stop(s_sis_disk_class *cls_)
{
   if (cls_->status == SIS_DISK_STATUS_CLOSED)
    {
        return 0;
    }
    // 根据文件类型写索引，并关闭文件
    sis_files_close(cls_->work_fps);
    if (cls_->work_fps->main_head.index)
    {
        sis_files_close(cls_->index_fps);
    }
    sis_disk_class_clear(cls_);
    return 0;
}

s_sis_disk_index *sis_disk_index_get(s_sis_map_list *map_, s_sis_object *key_, s_sis_object *sdb_)
{
    if (!key_)
    {
        return NULL;
    }
    char key[255];
    if (sdb_)
    {
        sis_sprintf(key, 255, "%s.%s", SIS_OBJ_SDS(key_), SIS_OBJ_SDS(sdb_));
    }
    else
    {   
        sis_sprintf(key, 255, "%s", SIS_OBJ_SDS(key_));
    }
    s_sis_disk_index *node = (s_sis_disk_index *)sis_map_list_get(map_, key);
    if (!node)
    {
        node = sis_disk_index_create(key_, sdb_);
        sis_map_list_set(map_, key, node);
    }
    return node;
}

