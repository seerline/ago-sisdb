#include "sis_disk.h"

///////////////////////////
//  s_sis_disk_reader
///////////////////////////
s_sis_disk_reader *sis_disk_reader_create(s_sis_disk_callback *cb_)
{
    s_sis_disk_reader *o = SIS_MALLOC(s_sis_disk_reader, o);
    o->callback = cb_;
    o->issub = 1;
    o->whole = 0;
    o->memory = sis_memory_create();
    return o;
}
void sis_disk_reader_destroy(void *cls_)
{
    s_sis_disk_reader *info = (s_sis_disk_reader *)cls_;
    sis_sdsfree(cls_->sub_keys);
    sis_sdsfree(cls_->sub_sdbs);
    sis_memory_destroy(info->memory);
    sis_free(info);
}
void sis_disk_reader_init(s_sis_disk_reader *cls_)
{
    cls_->rhead.fin = 1;
    cls_->rhead.hid = 0;
    cls_->rhead.zip = 0;
    cls_->rinfo     = NULL;
    sis_memory_clear(cls_->memory);
}
void sis_disk_reader_clear(s_sis_disk_reader *cls_)
{
    sis_sdsfree(cls_->sub_keys);
    cls_->sub_keys = NULL;
    sis_sdsfree(cls_->sub_sdbs);
    cls_->sub_sdbs = NULL;

    cls_->issub = 1;
    cls_->whole = 0;
    
    cls_->search_msec.start = 0;
    cls_->search_msec.stop  = 0;

    sis_disk_reader_init(cls_);
}
void sis_disk_reader_set_key(s_sis_disk_reader *cls_, const char *in_)
{
    sis_sdsfree(cls_->sub_keys);
    cls_->sub_keys = sis_sdsnew(in_);
}
void sis_disk_reader_set_sdb(s_sis_disk_reader *cls_, const char *in_)
{
    sis_sdsfree(cls_->sdbs);
    cls_->sdbs = sis_sdsnew(in_);
}

void sis_disk_reader_set_stime(s_sis_disk_reader *cls_, msec_t start_, msec_t stop_)
{
    cls_->search_msec.start = start_;
    cls_->search_msec.stop = stop_;
}

int sis_reader_sub_filters(s_sis_disk_ctrl *cls_, s_sis_disk_reader *reader_, s_sis_pointer_list *list_)
{
    if (!reader_ || !reader_->issub || !list_ || !reader_->keys || sis_sdslen(reader_->keys) < 1)
    {
        return 0;
    }
    sis_pointer_list_clear(list_);
    if(!sis_strcasecmp(reader_->keys, "*") && !sis_strcasecmp(reader_->sdbs, "*"))
    {
        // 如果是全部订阅
        for (int k = 0; k < sis_map_list_getsize(cls_->map_idxs); k++)
        {
            s_sis_disk_idx *node = (s_sis_disk_idx *)sis_map_list_geti(cls_->map_idxs, k);
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
                // printf("%s %s\n", info, reader_->sdbs);            
                s_sis_disk_idx *node = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, info);
                if (node)
                {
                    // printf("%s : %s\n",node? SIS_OBJ_SDS(node->key) : "nil", info);
                    sis_pointer_list_push(list_, node);
                }
            }
        }
        else
        {

            s_sis_disk_idx *node = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, sis_string_list_get(keys, i));
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



///////////////////////////
//  tools
///////////////////////////


s_sis_disk_idx *sis_disk_idx_get(s_sis_map_list *map_, s_sis_object *kname_, s_sis_object *sname_)
{
    if (!kname_||!sname_)
    {
        return NULL;
    }
    s_sis_sds key = sis_sdsnew(SIS_OBJ_SDS(kname_));
    key = sis_sdscatfmt(key, ".%s", SIS_OBJ_SDS(sname_));
    LOG(0)("merge : %s\n", key);
    s_sis_disk_idx *node = (s_sis_disk_idx *)sis_map_list_get(map_, key);
    if (!node)
    {
        node = sis_disk_idx_create(key_, sdb_);
        sis_map_list_set(map_, key, node);
    }
    sis_sdsfree(key);
    return node;
}

void sis_disk_file_delete(s_sis_disk_ctrl *cls_)
{
    int count = sis_disk_files_delete(cls_->work_fps);
    LOG(5)("delete file count = %d.\n", count);
}

void sis_disk_file_move(s_sis_disk_ctrl *cls_, const char *path_)
{
    char fn[255];
    char newfn[255];
    for (int  i = 0; i < cls_->work_fps->lists->count; i++)
    {
        s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->work_fps->lists, i);
        sis_file_getname(unit->fn, fn, 255);
        sis_sprintf(newfn, 255, "%s/%s", path_, fn);
        sis_file_rename(unit->fn, newfn);
    }
    if (cls_->work_fps->main_head.index)
    {
        for (int  i = 0; i < cls_->widx_fps->lists->count; i++)
        {
            s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->widx_fps->lists, i);
            sis_file_getname(unit->fn, fn, 255);
            sis_sprintf(newfn, 255, "%s/%s", path_, fn);
            sis_file_rename(unit->fn, newfn);
        }
    }
}
int sis_disk_file_valid_widx(s_sis_disk_ctrl *cls_)
{
    if (cls_->work_fps->main_head.index)
    {
        if (!sis_file_exists(cls_->widx_fps->cur_name))
        {
            LOG(5)
            ("idxfile no exists.[%s]\n", cls_->widx_fps->cur_name);
            return SIS_DISK_CMD_NO_EXISTS_IDX;
        }
        return SIS_DISK_CMD_OK;
    }
    return SIS_DISK_CMD_NO_IDX;
}

// 检查文件是否有效
int sis_disk_file_valid_work(s_sis_disk_ctrl *cls_)
{
    // 通常判断work和index的尾部是否一样 一样表示完整 否则
    // 检查work file 是否完整 如果不完整就设置最后一个块的位置 并重建索引
    // 如果work file 完整 就检查索引文件是否完整 不完整就重建索引
    // 重建失败就返回 1
    return SIS_DISK_CMD_OK;
}
