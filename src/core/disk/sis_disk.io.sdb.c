
#include "sis_disk.h"

// 这里是关于 sdb 时序结构化数据 的读写函数
// 这里的函数只管读写 默认在上层已经分配好了数据归属
size_t sis_disk_io_write_noidx(s_sis_disk_files *files_, s_sis_disk_wcatch *wcatch_)
{ 
    // printf("%d %zu -> ", wcatch_->head.zip, sis_memory_get_size(wcatch_->memory));
    if (wcatch_->head.zip == SIS_DISK_ZIP_SNAPPY)
    {
        s_sis_memory *zipmem = sis_memory_create();
        if(sis_snappy_compress(sis_memory(wcatch_->memory), sis_memory_get_size(wcatch_->memory), zipmem))
        {
            sis_memory_swap(wcatch_->memory, zipmem);
        }
        else
        {
            wcatch_->head.zip = SIS_DISK_ZIP_NOZIP;
        }
        sis_memory_destroy(zipmem);        
    }
    // printf("%d %zu\n", wcatch_->head.zip, sis_memory_get_size(wcatch_->memory));
    wcatch_->head.fin = 1;
    size_t size = sis_disk_files_write_saveidx(files_, wcatch_); 
    return size;
}
size_t sis_disk_io_write_sdb_work(s_sis_disk_ctrl *cls_, s_sis_disk_wcatch *wcatch_)
{ 
    size_t size = sis_disk_io_write_noidx(cls_->work_fps, wcatch_);
    if (cls_->work_fps->main_head.index)
    {
        s_sis_disk_idx *node = sis_disk_idx_get(cls_->map_idxs, wcatch_->kname, wcatch_->sname);
        wcatch_->winfo.active++;
        sis_disk_idx_set_unit(node, &wcatch_->winfo);
    }
    return size;
}

static int sis_disk_io_merge_sdb(s_sis_disk_ctrl *cls_, s_sis_disk_wcatch *wcatch_)
{
   s_sis_disk_idx *node = sis_disk_idx_get(cls_->map_idxs, wcatch_->kname, wcatch_->sname);
    // 判断来源数据是否时序的数据
    // 如果来源数据不是序列化的就直接删除老的 
    if ((wcatch_->winfo.start == 0 && wcatch_->winfo.stop == 0) || 
         wcatch_->winfo.start > wcatch_->winfo.stop)
    {
        sis_struct_list_clear(node->idxs);
    }   
    else
    {
        // 暂时处理为时间有重叠就覆盖老的
        int idx = 0;
        while(idx < node->idxs->count)
        {
            s_sis_disk_idx_unit *unit = (s_sis_disk_idx_unit *)sis_struct_list_get(node->idxs, idx);
            if (sis_is_mixed(wcatch_->winfo.start, wcatch_->winfo.stop, unit->start, unit->stop))
            {
                sis_struct_list_delete(node->idxs, idx, 1);
                continue;
            }
            idx++;
        }   
    }
    return 0;
}

int sis_disk_io_write_one(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, void *in_, size_t ilen_)
{
    s_sis_disk_wcatch *wcatch = cls_->wcatch;
    sis_disk_wcatch_setname(wcatch, kdict_->name, NULL);
    wcatch->winfo.sdict = 0;
    sis_disk_io_merge_sdb(cls_, wcatch);
    sis_memory_cat_ssize(wcatch->memory, kdict_->index);
    sis_memory_cat_ssize(wcatch->memory, ilen_);
    sis_memory_cat(wcatch->memory, (char *)in_, ilen_);

    wcatch->winfo.ktype = SIS_SDB_STYLE_ONE;
    wcatch->head.hid = SIS_DISK_HID_MSG_ONE;
    wcatch->head.zip = sis_disk_ctrl_work_zipmode(cls_);
    size_t size = sis_disk_io_write_sdb_work(cls_, wcatch);
    sis_disk_wcatch_init(wcatch);

    return size;
}
int sis_disk_io_write_mul(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, s_sis_pointer_list *ilist_)
{
    s_sis_disk_wcatch *wcatch = cls_->wcatch;
    sis_disk_wcatch_setname(wcatch, kdict_->name, NULL);
    wcatch->winfo.sdict = 0;
    sis_disk_io_merge_sdb(cls_, wcatch);
    sis_memory_cat_ssize(wcatch->memory, kdict_->index);
    sis_memory_cat_ssize(wcatch->memory, ilist_->count);
    for (int i = 0; i < ilist_->count; i++)
    {
        s_sis_sds in = sis_pointer_list_get(ilist_, i);
        sis_memory_cat_ssize(wcatch->memory, sis_sdslen(in));
        sis_memory_cat(wcatch->memory, (char *)in, sis_sdslen(in));
    }
    wcatch->winfo.ktype = SIS_SDB_STYLE_MUL;
    wcatch->head.hid = SIS_DISK_HID_MSG_MUL;
    wcatch->head.zip = sis_disk_ctrl_work_zipmode(cls_);
    size_t size = sis_disk_io_write_sdb_work(cls_, wcatch);
    sis_disk_wcatch_init(wcatch);
    return size;
}
int sis_disk_io_write_non(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, void *in_, size_t ilen_)
{
    s_sis_dynamic_db *sdb = sis_disk_sdict_last(sdict_);
    if (!sdb || (ilen_ % sdb->size) != 0)
    {
        return 0;
    }
    s_sis_disk_wcatch *wcatch = cls_->wcatch;
    sis_disk_wcatch_setname(wcatch, kdict_->name, sdict_->name);
    wcatch->winfo.sdict = sdict_->sdbs->count;
    printf("sis_disk_wcatch_setname:  %p %p\n", wcatch->kname, wcatch->sname);
    sis_disk_io_merge_sdb(cls_, wcatch);
    sis_memory_cat_ssize(wcatch->memory, kdict_->index);
    sis_memory_cat_ssize(wcatch->memory, sdict_->index);
    sis_memory_cat(wcatch->memory, (char *)in_, ilen_);

    wcatch->winfo.ktype = SIS_SDB_STYLE_NON;
    wcatch->head.hid = SIS_DISK_HID_MSG_NON;
    wcatch->head.zip = sis_disk_ctrl_work_zipmode(cls_);
    // 这里应该写入文件末尾 并更新索引信息
    size_t size = sis_disk_io_write_sdb_work(cls_, wcatch);
    sis_disk_wcatch_init(wcatch);

    return size;
} 
int sis_disk_io_write_sdb(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, void *in_, size_t ilen_)
{
    s_sis_dynamic_db *sdb = sis_disk_sdict_last(sdict_);
    if (!sdb || (ilen_ % sdb->size) != 0)
    {
        return 0;
    }
    int count = ilen_ / sdb->size;
    s_sis_disk_wcatch *wcatch = cls_->wcatch;
    sis_disk_wcatch_setname(wcatch, kdict_->name, sdict_->name);
    wcatch->winfo.start = sis_dynamic_db_get_time(sdb, 0, in_, ilen_);
    wcatch->winfo.stop = sis_dynamic_db_get_time(sdb, count - 1, in_, ilen_);
    wcatch->winfo.sdict = sdict_->sdbs->count;
    sis_disk_io_merge_sdb(cls_, wcatch);
    sis_memory_cat_ssize(wcatch->memory, kdict_->index);
    sis_memory_cat_ssize(wcatch->memory, sdict_->index);
    sis_memory_cat(wcatch->memory, (char *)in_, ilen_);

    wcatch->winfo.ktype = SIS_SDB_STYLE_SDB;
    wcatch->head.hid = SIS_DISK_HID_MSG_SDB;
    wcatch->head.zip = sis_disk_ctrl_work_zipmode(cls_);
    size_t size = sis_disk_io_write_sdb_work(cls_, wcatch);
    sis_disk_wcatch_init(wcatch);

    return size;
}

int sis_disk_io_write_map(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, int ktype, int idate, int moved)
{
    // 写map之间需要先写一下 newinfo 确保索引存在
    char name[255];
    if (sdict_)
    {
        sis_sprintf(name, 255, "%s.%s", SIS_OBJ_SDS(kdict_->name), SIS_OBJ_SDS(sdict_->name));
    }
    else
    {
        sis_sprintf(name, 255, "%s", SIS_OBJ_SDS(kdict_->name));
    }
    // printf("write map::: %d %d %d\n", ktype, idate, moved);
    s_sis_disk_map *map = sis_map_list_get(cls_->map_maps, name);
    if (!map)
    {
        if (moved)
        {
            return 0;
        }
        map = sis_disk_map_create(kdict_->name, sdict_ ? sdict_->name : NULL);
        map->ktype = ktype;
        map->active = 1;
        sis_map_list_set(cls_->map_maps, name, map);
    }
  
    s_sis_disk_map_unit *punit = sis_sort_list_find(map->sidxs, idate);
    if (!punit)
    {
        if (moved)
        {
            return 0;
        }
        s_sis_disk_map_unit unit;
        unit.active = 1;
        unit.idate = idate;
        punit = sis_sort_list_set(map->sidxs, unit.idate, &unit);       
    }
    else
    {
        if (!moved)
        {
            return 0;
        }
        punit->active = 0 ;
    }

    s_sis_disk_wcatch *wcatch = cls_->wcatch;
    s_sis_memory *memory = wcatch->memory;
    sis_memory_cat_ssize(memory, kdict_->index);
    if (sdict_)
    {
        sis_memory_cat_ssize(memory, sdict_->index + 1);  // 0 表示没有表键值
    }
    else
    {
        sis_memory_cat_ssize(memory, 0);
    }
    // size_t klen = SIS_OBJ_GET_SIZE(kdict_->name);
    // sis_memory_cat_ssize(memory, klen);
    // sis_memory_cat(memory, SIS_OBJ_SDS(kdict_->name), klen);
    // if (sdict_)
    // {
    //     size_t slen = SIS_OBJ_GET_SIZE(sdict_->name);
    //     sis_memory_cat_ssize(memory, slen);
    //     sis_memory_cat(memory, SIS_OBJ_SDS(sdict_->name), slen);
    // }
    // else
    // {
    //     sis_memory_cat_ssize(memory, 0);
    // }
    sis_memory_cat_byte(memory, map->active, 1);
    sis_memory_cat_byte(memory, map->ktype, 1);
    sis_memory_cat_ssize(memory, 1);
    sis_memory_cat_byte(memory, 1, punit->active);
    sis_memory_cat_ssize(memory, punit->idate);

    wcatch->head.hid = SIS_DISK_HID_MSG_MAP;
    wcatch->head.zip = sis_disk_ctrl_work_zipmode(cls_);
    // size_t size = 
    sis_disk_io_write_sdb_work(cls_, wcatch);
    sis_disk_wcatch_init(wcatch);
    printf("write map:::ok\n");
    return 0;
}

void sis_disk_io_write_widx_tail(s_sis_disk_ctrl *cls_)
{
    // 同步尾信息
    memmove(&cls_->widx_fps->main_tail, &cls_->work_fps->main_tail, sizeof(s_sis_disk_main_tail));
}

size_t sis_disk_io_write_sdb_widx(s_sis_disk_ctrl *cls_)
{
    s_sis_disk_wcatch *wcatch = cls_->wcatch;
    sis_disk_wcatch_init(wcatch);
    wcatch->head.zip = sis_disk_ctrl_widx_zipmode(cls_);

    size_t size = 0;
    // 到这里头已经写了 尾不用写，直接写中间的内容
    s_sis_memory *memory = wcatch->memory;
    sis_memory_clear(memory);
    s_sis_disk_idx *keynode = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, SIS_DISK_SIGN_KEY);
    if (keynode)
    {
        sis_memory_cat_ssize(memory, keynode->idxs->count);
        for (int k = 0; k < keynode->idxs->count; k++)
        {
            s_sis_disk_idx_unit *unit = sis_disk_idx_get_unit(keynode, k);
            sis_memory_cat_ssize(memory, unit->fidx);
            sis_memory_cat_ssize(memory, unit->offset);
            sis_memory_cat_ssize(memory, unit->size);
        }
        wcatch->head.hid = SIS_DISK_HID_INDEX_KEY;
        size += sis_disk_io_write_noidx(cls_->widx_fps, wcatch);
        printf("+++widx key: %zu\n", size);
    }
    sis_memory_clear(memory);
    s_sis_disk_idx *sdbnode = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, SIS_DISK_SIGN_SDB);
    if (sdbnode)
    {
        sis_memory_cat_ssize(memory, sdbnode->idxs->count);
        for (int k = 0; k < sdbnode->idxs->count; k++)
        {
            s_sis_disk_idx_unit *unit = sis_disk_idx_get_unit(sdbnode, k);
            sis_memory_cat_ssize(memory, unit->fidx);
            sis_memory_cat_ssize(memory, unit->offset);
            sis_memory_cat_ssize(memory, unit->size);
        }
        wcatch->head.hid = SIS_DISK_HID_INDEX_SDB;
        size += sis_disk_io_write_noidx(cls_->widx_fps, wcatch);
        printf("+++widx sdb: %zu\n", size);
    }
    sis_memory_clear(memory);
    wcatch->head.hid = SIS_DISK_HID_INDEX_MSG;
    int count = sis_map_list_getsize(cls_->map_idxs);
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_idx *node = (s_sis_disk_idx *)sis_map_list_geti(cls_->map_idxs, i);
        // 排除key和sdb
        if (node == sdbnode || node == keynode)
        {
            continue;
        }
        printf("write_index : %s %s %d \n", SIS_OBJ_SDS(node->kname), SIS_OBJ_SDS(node->sname), node->idxs->count);
        size_t klen = SIS_OBJ_GET_SIZE(node->kname);
        sis_memory_cat_ssize(memory, klen);
        sis_memory_cat(memory, SIS_OBJ_SDS(node->kname), klen);
        if (node->sname)
        {
            size_t slen = SIS_OBJ_GET_SIZE(node->sname);
            sis_memory_cat_ssize(memory, slen);
            sis_memory_cat(memory, SIS_OBJ_SDS(node->sname), slen);
        }
        else
        {
            sis_memory_cat_ssize(memory, 0);
        }        
        sis_memory_cat_ssize(memory, node->idxs->count);
        for (int k = 0; k < node->idxs->count; k++)
        {
            s_sis_disk_idx_unit *unit = (s_sis_disk_idx_unit *)sis_struct_list_get(node->idxs, k);
            sis_memory_cat_byte( memory, unit->active, 1);
            sis_memory_cat_byte( memory, unit->ktype,  1);
            sis_memory_cat_byte( memory, unit->sdict,  1);
            sis_memory_cat_ssize(memory, unit->fidx);
            sis_memory_cat_ssize(memory, unit->offset);
            sis_memory_cat_ssize(memory, unit->size);
            sis_memory_cat_ssize(memory, unit->start);
            sis_memory_cat_ssize(memory, unit->stop - unit->start);
            printf("write_index === %llu %llu %llu %llu \n", unit->offset, unit->size, unit->start, unit->stop);
        }
        if (sis_memory_get_size(memory) > SIS_DISK_MAXLEN_IDXPAGE)
        {
            size += sis_disk_io_write_noidx(cls_->widx_fps, wcatch);
            sis_memory_clear(memory);
        }
    }   
    if (sis_memory_get_size(memory) > 0)
    {
        size += sis_disk_io_write_noidx(cls_->widx_fps, wcatch);
        sis_memory_clear(memory);
    }
    sis_disk_io_write_widx_tail(cls_);
    sis_disk_files_write_sync(cls_->widx_fps);
    LOG(5)("write_index end %zu \n", size);
    return size;
}
////////////////////////////////
////////      read      ////////
////////////////////////////////

// 不能做IO直通车 只有LOG才能做直通车
// 返回原始数据 
int sis_disk_io_read_sdb(s_sis_disk_ctrl *cls_, s_sis_disk_rcatch *rcatch_)
{
    if (sis_disk_files_read_fromidx(cls_->work_fps, rcatch_) == 0)
    {
        return -1;
    }     
    // printf(" ==== %zu\n", sis_memory_get_size(rcatch_->memory));
    if (sis_disk_ctrl_unzip_sdb(cls_, rcatch_) == 0)
    {
        return -2;
    }
    // printf(" ==== %zu\n", sis_memory_get_size(rcatch_->memory));
    switch (rcatch_->head.hid)
    {
    case SIS_DISK_HID_MSG_ONE:
        rcatch_->kidx = sis_memory_get_ssize(rcatch_->memory);
        break;
    case SIS_DISK_HID_MSG_MUL:
        rcatch_->kidx = sis_memory_get_ssize(rcatch_->memory);
        break;
    case SIS_DISK_HID_MSG_SDB:
    case SIS_DISK_HID_MSG_NON:
        rcatch_->kidx = sis_memory_get_ssize(rcatch_->memory);
        rcatch_->sidx = sis_memory_get_ssize(rcatch_->memory);
        break;
    case SIS_DISK_HID_MSG_SNO:
        rcatch_->kidx   = sis_memory_get_ssize(rcatch_->memory);
        rcatch_->sidx   = sis_memory_get_ssize(rcatch_->memory);
        // rcatch_->series = sis_memory_get_ssize(rcatch_->memory);
        break;
    case SIS_DISK_HID_DICT_KEY:
    case SIS_DISK_HID_DICT_SDB:
    default:
        break;
    }
    // printf(" ==== %d %zu\n", rcatch_->head.hid, sis_memory_get_size(rcatch_->memory));
    return 0;
}

////////////////////////////////
////////      read      ////////
////////////////////////////////

int cb_sis_disk_io_read_sdb(void *source_, s_sis_disk_head *head_, char *imem_, size_t isize_)
{
    s_sis_disk_ctrl *ctrl = (s_sis_disk_ctrl *)source_;
    s_sis_disk_reader_cb *callback = ctrl->rcatch->callback; 

    // 根据hid不同写入不同的数据到obj
    switch (head_->hid)
    {
    case SIS_DISK_HID_MSG_SDB: // 只有一个key + 可能多条数据
        if(callback->cb_original)
        {
            callback->cb_original(callback->cb_source, head_, imem_, isize_);
        }
        break;
    case SIS_DISK_HID_DICT_KEY:
        // if(callback && callback->cb_dict_keys)
        // {
        //     callback->cb_dict_keys(callback->cb_source, imem_, isize_);
        // }
        break;
    case SIS_DISK_HID_DICT_SDB:
        // if(callback && callback->cb_dict_sdbs)
        // {
        //     callback->cb_dict_sdbs(callback->cb_source, imem_, isize_);
        // }
        break;
    default:
        LOG(5)("other hid : %d at sdb.\n", head_->hid);
        break;
    }
    if (ctrl->isstop)
    {
        return -1;
    }
    return 0;
}
static void _disk_io_callback_sdb_dict(s_sis_disk_ctrl *cls_)
{
    s_sis_disk_reader_cb *callback = cls_->rcatch->callback;   
    if (callback->cb_dict_keys)
    {
        s_sis_sds msg = sis_disk_ctrl_get_keys_sds(cls_);
        if (sis_sdslen(msg) > 2) 
        {
            callback->cb_dict_keys(callback->cb_source, msg, sis_sdslen(msg));
        }
        sis_sdsfree(msg);
    }
    if (callback->cb_dict_sdbs)
    {
        s_sis_sds msg = sis_disk_ctrl_get_sdbs_sds(cls_);
        // printf("sdbs :%s\n", msg);
        if (sis_sdslen(msg) > 2) 
        {
            callback->cb_dict_sdbs(callback->cb_source, msg, sis_sdslen(msg));
        }
        sis_sdsfree(msg);                
    }
}
int sis_disk_io_sub_sdb(s_sis_disk_ctrl *cls_, void *cb_)
{
    if (!cb_)
    {
        return -1;
    }
    sis_disk_rcatch_init_of_sub(cls_->rcatch, NULL, NULL, NULL, cb_);
    cls_->isstop = false;  // 用户可以随时中断
    s_sis_disk_reader_cb *callback = cls_->rcatch->callback;   
    if(callback->cb_start)
    {
        callback->cb_start(callback->cb_source, cls_->open_date);
    }
    // 每次都从头读起
    _disk_io_callback_sdb_dict(cls_);
    sis_disk_files_read_fulltext(cls_->work_fps, cls_, cb_sis_disk_io_read_sdb);

    // 不是因为中断而结束 就发送stop标志
    if (cls_->isstop)
    {
        if(callback->cb_break)
        {
            callback->cb_break(callback->cb_source, cls_->stop_date);
        }
    }
    else
    {
        if(callback->cb_stop)
        {
            callback->cb_stop(callback->cb_source, cls_->stop_date);
        }
    }

    return 0;
}

int cb_sis_disk_io_read_sdb_widx(void *source_, s_sis_disk_head *head_, char *imem_, size_t isize_)
{
    s_sis_disk_ctrl *cls_ = (s_sis_disk_ctrl *)source_;
    // 直接写到内存中
    s_sis_memory *memory = sis_memory_create();
    sis_disk_io_unzip_widx(head_, imem_, isize_, memory);
    char name[255];
    int count = 0;
    while(sis_memory_get_size(memory) > 0)
    {
        switch (head_->hid)
        {
        case SIS_DISK_HID_INDEX_KEY:
            {            
                s_sis_disk_idx *node = sis_disk_idx_create(NULL, NULL);          
                int blocks = sis_memory_get_ssize(memory);
                for (int i = 0; i < blocks; i++)
                {
                    s_sis_disk_idx_unit unit;
                    memset(&unit, 0, sizeof(s_sis_disk_idx_unit));
                    unit.fidx = sis_memory_get_ssize(memory);
                    unit.offset = sis_memory_get_ssize(memory);
                    unit.size = sis_memory_get_ssize(memory);
                    sis_struct_list_push(node->idxs, &unit);
                }
                sis_map_list_set(cls_->map_idxs, SIS_DISK_SIGN_KEY, node);  
                sis_disk_ctrl_read_kdict(cls_, node);
            }        
            break;
        case SIS_DISK_HID_INDEX_SDB:
            {
                s_sis_disk_idx *node = sis_disk_idx_create(NULL, NULL);
                int blocks = sis_memory_get_ssize(memory);
                for (int i = 0; i < blocks; i++)
                {
                    s_sis_disk_idx_unit unit;
                    memset(&unit, 0, sizeof(s_sis_disk_idx_unit));
                    unit.fidx = sis_memory_get_ssize(memory);
                    unit.offset = sis_memory_get_ssize(memory);
                    unit.size = sis_memory_get_ssize(memory);
                    sis_struct_list_push(node->idxs, &unit);
                }
                sis_map_list_set(cls_->map_idxs, SIS_DISK_SIGN_SDB, node);   
                sis_disk_ctrl_read_sdict(cls_, node);
            }
            break;
        case SIS_DISK_HID_INDEX_MSG:
            {
                int klen = sis_memory_get_ssize(memory);
                s_sis_sds kname = sis_sdsnewlen(sis_memory(memory), klen);
                s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(cls_->map_kdicts, kname);
                sis_memory_move(memory, klen);
                
                int slen = sis_memory_get_ssize(memory);
                s_sis_disk_sdict *sdict = NULL;
                if (slen > 0)
                {
                    s_sis_sds sname = sis_sdsnewlen(sis_memory(memory), slen);
                    sdict = sis_disk_map_get_sdict(cls_->map_sdicts, sname);
                    sis_memory_move(memory, slen);
                    sis_sprintf(name, 255, "%s.%s",kname, sname);
                    sis_sdsfree(sname);
                }
                else
                {
                    sis_sprintf(name, 255, "%s", kname);
                }
                // printf("== %s %s %p %p | %s %s\n", name, kname, kdict, sdict, SIS_OBJ_GET_CHAR(kdict->name), sdict ? SIS_OBJ_GET_CHAR(sdict->name) : "nil");
                sis_sdsfree(kname);
                printf("== %d %s %s | %s %s\n", cls_->style, cls_->work_fps->cur_name, name, 
                    kdict ? SIS_OBJ_GET_CHAR(kdict->name) : "nil", 
                    sdict ? SIS_OBJ_GET_CHAR(sdict->name) : "nil");
                s_sis_disk_idx *node = sis_disk_idx_create(kdict->name, sdict ? sdict->name : NULL);
                int blocks = sis_memory_get_ssize(memory);
                for (int i = 0; i < blocks; i++)
                {
                    s_sis_disk_idx_unit unit;
                    memset(&unit, 0, sizeof(s_sis_disk_idx_unit));
                    unit.active = sis_memory_get_byte(memory, 1);
                    unit.ktype  = sis_memory_get_byte(memory, 1);
                    unit.sdict  = sis_memory_get_byte(memory, 1);
                    unit.fidx   = sis_memory_get_ssize(memory);
                    unit.offset = sis_memory_get_ssize(memory);
                    unit.size   = sis_memory_get_ssize(memory);
                    unit.start  = sis_memory_get_ssize(memory);
                    unit.stop   = unit.start + sis_memory_get_ssize(memory);   
                    unit.ipage  = 0;
                    sis_struct_list_push(node->idxs, &unit);
                    printf("::: %d %d %s %s\n", i, unit.fidx, (char *)node->kname->ptr, (char *)node->sname->ptr);
                }
                sis_map_list_set(cls_->map_idxs, name, node);   
                printf("== %d %s %s %s\n", cls_->style, name, SIS_OBJ_GET_CHAR(node->kname), SIS_OBJ_GET_CHAR(node->sname));
            }        
            break;    
        default:
            LOG(5)("other hid : %d at sdbidx.\n", head_->hid);
            break;
        }
        count++;
    }
    sis_memory_destroy(memory);
    if (cls_->isstop)
    {
        return -1;
    }
    return count;
}

int sis_disk_io_read_sdb_widx(s_sis_disk_ctrl *cls_)
{
    if (cls_->work_fps->main_head.index)
    {
        if( sis_disk_ctrl_valid_widx(cls_) != SIS_DISK_CMD_OK)
        {
            LOG(5)("widx is no valid.[%s]\n", cls_->widx_fps->cur_name);
            return SIS_DISK_CMD_NO_VAILD;
        }
        sis_map_list_clear(cls_->map_idxs);
        if (sis_disk_files_open(cls_->widx_fps, SIS_DISK_ACCESS_RDONLY))
        {
            LOG(5)("open idxfile fail.[%s]\n", cls_->widx_fps->cur_name);
            return SIS_DISK_CMD_NO_OPEN_IDX;
        }
        // printf("==1== %d\n", cls_->style);
        sis_disk_files_read_fulltext(cls_->widx_fps, cls_, cb_sis_disk_io_read_sdb_widx);
        sis_disk_files_close(cls_->widx_fps);
        // printf("==2== %d\n", cls_->style);
        return SIS_DISK_CMD_OK;
    }
    return SIS_DISK_CMD_NO_IDX;
}

int cb_sis_disk_io_read_sdb_map(void *source_, s_sis_disk_head *head_, char *imem_, size_t isize_)
{
    s_sis_disk_ctrl *ctrl = (s_sis_disk_ctrl *)source_;
    // map文件只处理 key 和 sdb 

    s_sis_memory *memory = sis_memory_create();
    sis_disk_io_unzip_widx(head_, imem_, isize_, memory);
    char name[255];
    switch (head_->hid)
    {
    case SIS_DISK_HID_DICT_KEY:
        sis_disk_reader_set_kdict(ctrl->map_kdicts, sis_memory(memory), sis_memory_get_size(memory));
        break;
    case SIS_DISK_HID_DICT_SDB:
        sis_disk_reader_set_sdict(ctrl->map_sdicts, sis_memory(memory), sis_memory_get_size(memory));
        // sis_out_binary("sdbs", sis_memory(memory), sis_memory_get_size(memory));
        break;
    case SIS_DISK_HID_MSG_MAP:
        {
            int kidx = sis_memory_get_ssize(memory);
            s_sis_disk_kdict *kdict = sis_map_list_geti(ctrl->map_kdicts, kidx);
            s_sis_disk_sdict *sdict = NULL;
            int sidx = sis_memory_get_ssize(memory) - 1;
            if (sidx >= 0)
            {
                sdict = sis_map_list_geti(ctrl->map_sdicts, sidx);
            }
            printf("sdict : %p %d %d | %d\n", sdict,  kidx, sidx, sis_map_list_getsize(ctrl->map_sdicts));

            if (sdict)
            {
                sis_sprintf(name, 255, "%s.%s", SIS_OBJ_GET_CHAR(kdict->name), SIS_OBJ_GET_CHAR(sdict->name));
            }
            else
            {
                sis_sprintf(name, 255, "%s", SIS_OBJ_GET_CHAR(kdict->name));
            }

            s_sis_disk_map *map = sis_disk_map_create(kdict->name, sdict ? sdict->name : NULL);
            map->active = sis_memory_get_byte(memory, 1);
            map->ktype =sis_memory_get_byte(memory, 1);
            int blocks = sis_memory_get_ssize(memory);
            for (int i = 0; i < blocks; i++)
            {
                s_sis_disk_map_unit unit;
                memset(&unit, 0, sizeof(s_sis_disk_map_unit));
                unit.active = sis_memory_get_byte(memory, 1);
                unit.idate = sis_memory_get_ssize(memory);
                sis_sort_list_set(map->sidxs, unit.idate, &unit);
                printf("newmap : %s %d %d | %d %d\n", name, unit.active, unit.idate, kidx, sidx);
            }
            s_sis_disk_map *agomap = sis_map_list_get(ctrl->map_maps, name);  
            if (agomap)
            {
                sis_disk_map_merge(agomap, map);
                sis_disk_map_destroy(map);
                // for (int i = 0; i < sis_sort_list_getsize(agomap->sidxs); i++)
                // {
                //     s_sis_disk_map_unit *unit = sis_sort_list_get(agomap->sidxs, i);
                //     printf("+++map : %s %d %d\n", name, unit->active, unit->idate);
                // }
            }
            else
            {
                sis_map_list_set(ctrl->map_maps, name, map);
            }
        }
        break;
    default:
        LOG(5)("other hid : %d at map.\n", head_->hid);
        break;
    }
    sis_memory_destroy(memory);
    return 0;
}
int sis_disk_io_read_sdb_map(s_sis_disk_ctrl *cls_)
{
    if (cls_->work_fps->main_head.style != SIS_DISK_TYPE_SDB)
    {
        return SIS_DISK_CMD_NO_VAILD;
    }
    // 默认此时文件已打开 其他都清理干净
    sis_map_list_clear(cls_->map_kdicts);
    sis_map_list_clear(cls_->map_sdicts);
    sis_pointer_list_clear(cls_->new_kinfos);
    sis_pointer_list_clear(cls_->new_sinfos);

    sis_map_list_clear(cls_->map_maps);

    sis_disk_files_read_fulltext(cls_->work_fps, cls_, cb_sis_disk_io_read_sdb_map);

    return SIS_DISK_CMD_NO_IDX;
}

#if 0
#include "sis_disk.h"
// 测试SNO文件读写
static int    __read_nums = 0;
static size_t __read_size = 0;
static msec_t __read_msec = 0;
static int    __write_nums = 10;//*1000*1000;
static size_t __write_size = 0;
static msec_t __write_msec = 0;

#pragma pack(push,1)
typedef struct s_info
{
    char name[10];
} s_info;

typedef struct s_tsec {
	int time;
	int value;
} s_tsec;

typedef struct s_msec {
	msec_t time;
	int    value;
} s_msec;
#pragma pack(pop)
char *inkeys = "k1,k2,k3";
const char *insdbs = "{\"info\":{\"fields\":{\"name\":[\"C\",10]}},\
    \"sdate\":{\"fields\":{\"time\":[\"D\",4],\"value\":[\"I\",4]}},\
    \"sminu\":{\"fields\":{\"time\":[\"M\",4],\"value\":[\"I\",4]}},\
    \"sssec\":{\"fields\":{\"time\":[\"S\",4],\"value\":[\"I\",4]}},\
    \"smsec\":{\"fields\":{\"time\":[\"T\",8],\"value\":[\"I\",4]}}}";

static void cb_start(void *src, int tt)
{
    printf("%s : %d\n", __func__, tt);
    __read_msec = sis_time_get_now_msec();
}
static void cb_stop(void *src, int tt)
{
    printf("%s : %d cost: %lld\n", __func__, tt, sis_time_get_now_msec() - __read_msec);
}
static void cb_key(void *src, void *key_, size_t size) 
{
    __read_size += size;
    s_sis_sds info = sis_sdsnewlen((char *)key_, size);
    printf("%s %d : %s\n", __func__, (int)size, info);
    sis_sdsfree(info);
}
static void cb_sdb(void *src, void *sdb_, size_t size)  
{
    __read_size += size;
    s_sis_sds info = sis_sdsnewlen((char *)sdb_, size);
    printf("%s %d : %s\n", __func__, (int)size, info);
    sis_sdsfree(info);
}
static void cb_break(void *src, int tt)
{
    printf("%s : %d\n", __func__, tt);
}
static void cb_original(void *src, s_sis_disk_head *head_, void *out_, size_t olen_)
{
    __read_nums++;
    if (__read_nums % sis_max((__write_nums / 10), 1) == 0 || __read_nums < 10)
    {
        printf("%s : %zu %d\n", __func__, olen_, __read_nums);
    }
}
static void cb_chardata(void *src, const char *kname_, const char *sname_, void *out_, size_t olen_)
{
    __read_nums++;
    if (__read_nums % sis_max((__write_nums / 10), 1) == 0 || __read_nums < 10)
    {
        printf("%s : %s.%s %zu %d\n", __func__, kname_, sname_, olen_, __read_nums);
        // sis_out_binary("::", out_, olen_);
    }
    if (__read_nums == 300000)
    {
        sis_disk_reader_unsub((s_sis_disk_reader *)src);
    }
}
void read_sdb(s_sis_disk_reader *cxt)
{
    // sis_disk_reader_open(cxt);
    // // s_sis_msec_pair pair = {1620793985999, 1620793985999};
    // s_sis_msec_pair pair = {1620793979000, 1620795500999};
    // s_sis_object *obj = sis_disk_reader_get_obj(cxt, "k2", "sminu", &pair);
    // // s_sis_object *obj = sis_disk_reader_get_obj(cxt, "k3", "smsec", &pair);
    // if (obj)
    // {
    //     sis_out_binary("get", SIS_OBJ_GET_CHAR(obj), SIS_OBJ_GET_SIZE(obj));
    //     sis_object_destroy(obj);
    // }
    // sis_disk_reader_close(cxt);

    sis_disk_reader_open(cxt);
    s_sis_object *obj = sis_disk_reader_get_one(cxt, "k5");
    if (obj)
    {
        sis_out_binary(".out.",SIS_OBJ_GET_CHAR(obj), SIS_OBJ_GET_SIZE(obj));
        sis_object_destroy(obj);
    }
    s_sis_msec_pair pair = {1511925510999, 1621315511000};
    // s_sis_msec_pair pair = {1511925510999, 1620795500999};
    // sis_disk_reader_sub_sdb(cxt, "*", "*", &pair);
    sis_disk_reader_sub_sdb(cxt, "*", "*", &pair);
    sis_disk_reader_close(cxt);

}
void write_sdb(s_sis_disk_writer *cxt)
{
    s_info info_data[3] = { 
        { "k10000" },
        { "k20000" },
        { "k30000" },
    };
    s_msec snap_data[10] = { 
        { 1620793979000,  1},
        { 1620793979500,  2},
        { 1620793979999,  3},
        { 1620793985999,  4},
        { 1620794000999,  5},
        { 1620795500999,  6},
        { 1620795510999,  7},
        { 1620905510999,  8},
        { 1621315510999,  9},
        { 1511925510999,  10},
    };

    sis_disk_writer_open(cxt, 0);
    sis_disk_writer_set_kdict(cxt, inkeys, sis_strlen(inkeys));
    sis_disk_writer_set_sdict(cxt, insdbs, sis_strlen(insdbs));

    sis_disk_writer_one(cxt, "k5", (void *)"my is dog", 9);

    int count = __write_nums;
    __write_msec = sis_time_get_now_msec();

    __write_size += sis_disk_writer_sdb(cxt, "k1", "info", &info_data[0], sizeof(s_info));
    __write_size += sis_disk_writer_sdb(cxt, "k2", "info", &info_data[1], sizeof(s_info));
    __write_size += sis_disk_writer_sdb(cxt, "k3", "info", &info_data[2], sizeof(s_info));
    for (int k = 0; k < count; k++)
    {
        for (int i = 6; i < 10; i++) // 4
        {
            s_tsec date_data;
            date_data.time = sis_time_get_idate(snap_data[i].time / 1000);
            date_data.value = snap_data[i].value;
            __write_size += sis_disk_writer_sdb(cxt, "k1", "sdate", &date_data, sizeof(s_tsec));
        }
        for (int i = 0; i < 7; i++) // 5
        {
            s_tsec ssec_data;
            ssec_data.time = snap_data[i].time / 1000;
            ssec_data.value = snap_data[i].value;
            __write_size += sis_disk_writer_sdb(cxt, "k2", "sssec", &ssec_data, sizeof(s_tsec));
        }
        for (int i = 0; i < 6; i++)  // 3
        {
            s_tsec minu_data;
            minu_data.time = snap_data[i].time / 1000 / 60;
            
            minu_data.value = snap_data[i].value;
            // printf("%d : %d\n", minu_data.time, minu_data.value);
            __write_size += sis_disk_writer_sdb(cxt, "k2", "sminu", &minu_data, sizeof(s_tsec));
        }
        for (int i = 0; i < 5; i++) // 5
        {
            __write_size += sis_disk_writer_sdb(cxt, "k3", "smsec", &snap_data[i], sizeof(s_msec));
        }
    }
    sis_disk_writer_close(cxt);
    printf("write end %d %zu | cost: %lld.\n", __write_nums, __write_size, sis_time_get_now_msec() - __write_msec);
}

int main(int argc, char **argv)
{
    safe_memory_start();

    if (argc < 2)
    {
        s_sis_disk_writer *wcxt = sis_disk_writer_create(".", "wlog", SIS_DISK_TYPE_SDB);
        write_sdb(wcxt);
        // write_sdb_msec(wcxt);  // 按时间写入
        sis_disk_writer_destroy(wcxt);
    }
    else
    {
        s_sis_disk_reader_cb cb = {0};
        cb.cb_source = NULL;
        cb.cb_start = cb_start;
        cb.cb_stop = cb_stop;
        cb.cb_dict_keys = cb_key;
        cb.cb_dict_sdbs = cb_sdb;
        cb.cb_break = cb_break;
        cb.cb_original = cb_original;
        cb.cb_chardata  = cb_chardata;
        s_sis_disk_reader *rcxt = sis_disk_reader_create(".", "wlog", SIS_DISK_TYPE_SDB, &cb);
        cb.cb_source = rcxt;
        read_sdb(rcxt);
        sis_disk_reader_destroy(rcxt);
    }


    safe_memory_stop();
    return 0;
}
#endif