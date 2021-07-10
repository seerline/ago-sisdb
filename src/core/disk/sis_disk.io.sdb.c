
#include "sis_disk.h"

// 这里是关于 sdb 时序结构化数据 的读写函数

size_t _disk_io_write_sdb_work(s_sis_disk_ctrl *cls_, s_sis_disk_wcatch *wcatch_)
{ 
    if (wcatch_->head.zip == SIS_DISK_ZIP_INCRZIP)
    {
        s_sis_memory *zipmem = sis_memory_create();
        s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(cls_->map_sdicts, wcatch_->sname);
        s_sis_dynamic_db *db = sis_disk_sdict_last(sdict);
        sis_incrzip_clear(cls_->wincrzip);
        sis_incrzip_set_sdb(cls_->wincrzip, db);
        if(sis_incrzip_compress(cls_->wincrzip, sis_memory(wcatch_->memory), sis_memory_get_size(wcatch_->memory), zipmem) > 0)
        {
            sis_memory_swap(wcatch_->memory, zipmem);
        }
        else
        {
            wcatch_->head.zip = SIS_DISK_ZIP_NOZIP;
        }
        sis_memory_destroy(zipmem);        
    }
    size_t size = sis_disk_files_write_saveidx(cls_->work_fps, wcatch_); 
    if (cls_->work_fps->main_head.index)
    {
        s_sis_disk_idx *node = sis_disk_index_get(cls_->map_idxs, wcatch_->kname, wcatch_->sname);
        wcatch_->winfo.active++;
        sis_disk_index_set_unit(node->index, &wcatch_->winfo);
    }
    return size;
}
size_t _disk_io_write_sdb_widx(s_sis_disk_ctrl *cls_, s_sis_disk_wcatch *wcatch_)
{ 
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
    size_t size = sis_disk_files_write_saveidx(cls_->widx_fps, wcatch_); 
    return size;
}

static int sis_disk_io_merge_non(s_sis_disk_ctrl *cls_, s_sis_disk_wcatch *wcatch_)
{
    s_sis_disk_idx *node = sis_disk_index_get(cls_->map_idxs, wcatch_->kname, wcatch_->sname);
    // 如果数据不是序列化的就直接删除老的 
    if (node)
    {
        sis_struct_list_clear(node->index);
    }
    return 0;
}
int sis_disk_io_write_non(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, void *in_, size_t ilen_)
{
    s_sis_dynamic_db *sdb = sis_disk_dict_last(sdict_);
    if (!sdb || (ilen_ % sdb->size) != 0)
    {
        return 0;
    }
    int count = ilen_ / sdb->size;
    s_sis_disk_wcatch *wcatch = cls_->wcatch;
    wcatch->winfo.sdict = sdict_->sdbs->count;
    sis_disk_io_merge_non(cls_, wcatch);
    sis_memory_cat_ssize(wcatch->memory, kdict_->index);
    sis_memory_cat_ssize(wcatch->memory, sdict_->index);
    sis_memory_cat(wcatch->memory, (char *)in_, ilen_);

    wcatch->winfo.style = SIS_SDB_STYLE_NON;
    wcatch->head.hid = SIS_DISK_HID_MSG_NON;
    wcatch->head.zip = SIS_DISK_ZIP_SNAPPY;
    size_t size = _disk_io_write_non_work(cls_, wcatch);
    sis_disk_wcatch_init(wcatch);

    return 0;
}
int sis_disk_io_write_one(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, void *in_, size_t ilen_)
{
    s_sis_disk_wcatch *wcatch = cls_->wcatch;
    wcatch->winfo.sdict = 0;
    sis_disk_io_merge_non(cls_, wcatch);
    sis_memory_cat_ssize(wcatch->memory, kdict_->index);
    sis_memory_cat_ssize(wcatch->memory, ilen_);
    sis_memory_cat(wcatch->memory, (char *)in_, ilen_);

    wcatch->winfo.style = SIS_SDB_STYLE_ONE;
    wcatch->head.hid = SIS_DISK_HID_MSG_ONE;
    wcatch->head.zip = SIS_DISK_ZIP_SNAPPY;
    size_t size = _disk_io_write_non_work(cls_, wcatch);
    sis_disk_wcatch_init(wcatch);

    return 0;
}
int sis_disk_io_write_mul(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, s_sis_pointer_list *ilist_)
{
    s_sis_disk_wcatch *wcatch = cls_->wcatch;
    wcatch->winfo.sdict = 0;
    sis_disk_io_merge_non(cls_, wcatch);
    sis_memory_cat_ssize(wcatch->memory, kdict_->index);
    sis_memory_cat_ssize(wcatch->memory, ilist_->count);
    for (int i = 0; i < ilist_->count; i++)
    {
        s_sis_sds *in = sis_pointer_list_get(ilist_, i);
        sis_memory_cat_ssize(wcatch->memory, sis_sdslen(in));
        sis_memory_cat(wcatch->memory, (char *)in, sis_sdslen(in));
    }
    wcatch->winfo.style = SIS_SDB_STYLE_MUL;
    wcatch->head.hid = SIS_DISK_HID_MSG_MUL;
    wcatch->head.zip = SIS_DISK_ZIP_SNAPPY;
    size_t size = _disk_io_write_non_work(cls_, wcatch);
    sis_disk_wcatch_init(wcatch);
    return 0;
}

size_t sis_disk_io_write_nonidx(s_sis_disk_ctrl *cls_)
{
    s_sis_disk_wcatch *wcatch = cls_->wcatch;
    sis_disk_wcatch_init(wcatch);
    wcatch->head.zip = SIS_DISK_ZIP_SNAPPY;

    size_t size = 0;
    // 到这里头已经写了 尾不用写，直接写中间的内容
    s_sis_memory *memory = wcatch->memory;
    sis_memory_clear(memory);
    s_sis_disk_idx *keynode = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, SIS_DISK_SIGN_KEY);
    if (keynode)
    {
        sis_memory_cat_ssize(memory, keynode->index->count);
        for (int k = 0; k < keynode->index->count; k++)
        {
            s_sis_disk_idx_unit *unit = sis_disk_idx_get_unit(keynode, k);
            sis_memory_cat_ssize(memory, unit->fidx);
            sis_memory_cat_ssize(memory, unit->offset);
            sis_memory_cat_ssize(memory, unit->size);
        }
        wcatch->head.hid = SIS_DISK_HID_INDEX_KEY;
        size += _disk_io_write_non_widx(cls_, wcatch);
    }
    sis_memory_clear(memory);
    s_sis_disk_idx *sdbnode = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, SIS_DISK_SIGN_SDB);
    if (sdbnode)
    {
        sis_memory_cat_ssize(memory, sdbnode->index->count);
        for (int k = 0; k < sdbnode->index->count; k++)
        {
            s_sis_disk_idx_unit *unit = sis_disk_idx_get_unit(sdbnode, k);
            sis_memory_cat_ssize(memory, unit->fidx);
            sis_memory_cat_ssize(memory, unit->offset);
            sis_memory_cat_ssize(memory, unit->size);
        }
        wcatch->head.hid = SIS_DISK_HID_INDEX_SDB;
        size += _disk_io_write_non_widx(cls_, wcatch);
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
        // printf("write_index %s %d \n", SIS_OBJ_SDS(node->key), node->index->count);
        size_t klen = SIS_OBJ_GET_SIZE(node->kname));
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
        sis_memory_cat_ssize(memory, node->index->count);
        for (int k = 0; k < node->index->count; k++)
        {
            s_sis_disk_idx_unit *unit = (s_sis_disk_idx_unit *)sis_struct_list_get(node->idxs, k);
            sis_memory_cat_byte(memory, unit->active, 1);
            sis_memory_cat_byte(memory, unit->kdict, 1);
            sis_memory_cat_byte(memory, unit->style, 1);
            sis_memory_cat_byte(memory, unit->sdict, 1);
            sis_memory_cat_ssize(memory, unit->fidx);
            sis_memory_cat_ssize(memory, unit->offset);
            sis_memory_cat_ssize(memory, unit->size);
            sis_memory_cat_ssize(memory, unit->start);
            sis_memory_cat_ssize(memory, unit->stop - unit->start);
        }
        if (sis_memory_get_size(memory) > SIS_DISK_MAXLEN_IDXPAGE)
        {
            size += _disk_io_write_non_widx(cls_, wcatch);
            sis_memory_clear(memory);
        }
    }   
    if (sis_memory_get_size(memory) > 0)
    {
        size += _disk_io_write_non_widx(cls_, wcatch);
        sis_memory_clear(memory);
    }
    sis_disk_files_write_sync(cls_->widx_fps);
    LOG(5)("write_index end %zu \n", size);
    return size;
}
////////////////////////////////
////////      read      ////////
////////////////////////////////

// 不能做IO直通车 只有SNO和LOG才能做直通车
// 返回原始数据
int sis_disk_io_read_non(s_sis_disk_ctrl *cls_, s_sis_disk_rcatch *rcatch_)
{
    if (sis_disk_files_read_fromidx(cls_->work_fps, rcatch_) == 0)
    {
        return -1;
    }     
    if (sis_disk_io_uncompress(cls_, rcatch_) == 0)
    {
        return -2;
    }
    return 0;
}

int cb_sis_disk_io_read_nonidx(void *source_, s_sis_disk_head *head_, char *imem_, size_t isize_)
{
    s_sis_disk_ctrl *cls_ = (s_sis_disk_ctrl *)source_;
    // 直接写到内存中
    s_sis_memory *memory = sis_memory_create();
    sis_disk_io_idx_uncompress(head_, imem_, isize_, memory);
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
                    sis_struct_list_push(node->index, &unit);
                }
                sis_map_list_set(cls_->map_idxs, SIS_DISK_SIGN_KEY, node);   
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
                    sis_struct_list_push(node->index, &unit);
                }
                sis_map_list_set(cls_->map_idxs, SIS_DISK_SIGN_SDB, node);   
            }
            break;
        // ??? 这里应该先读取sdb和key的信息 这样索引的字符串就可以使用引用了
        case SIS_DISK_HID_INDEX_MSG:
            {
                int s1 = sis_memory_get_ssize(memory);
                s_sis_object *kname = sis_object_create(SIS_OBJECT_SDS, sis_sdsnewlen(sis_memory(memory), s1));
                sis_memory_move(memory, s1);
                int s2 = sis_memory_get_ssize(memory);
                s_sis_object *sname = NULL;
                if (s2 > 0)
                {
                    sname = sis_object_create(SIS_OBJECT_SDS, sis_sdsnewlen(sis_memory(memory), s2));
                    sis_memory_move(memory, s2);
                    sis_sprintf(name, 255, "%s.%s", SIS_OBJ_SDS(kname), SIS_OBJ_SDS(sname));
                }
                else
                {
                    sis_sprintf(name, 255, "%s", SIS_OBJ_SDS(kname));
                }
                s_sis_disk_idx *node = sis_disk_idx_create(kname, sname);
                int blocks = sis_memory_get_ssize(memory);
                for (int i = 0; i < blocks; i++)
                {
                    s_sis_disk_idx_unit unit;
                    memset(&unit, 0, sizeof(s_sis_disk_idx_unit));
                    unit.active = sis_memory_get_byte(memory, 1);
                    unit.kdict = sis_memory_get_byte(memory, 1);
                    unit.style = sis_memory_get_byte(memory, 1);
                    unit.sdict = sis_memory_get_byte(memory, 1);
                    unit.fidx = sis_memory_get_ssize(memory);
                    unit.offset = sis_memory_get_ssize(memory);
                    unit.size = sis_memory_get_ssize(memory);
                    unit.start = sis_memory_get_ssize(memory);
                    unit.stop = unit.start + sis_memory_get_ssize(memory);    
                    sis_struct_list_push(node->index, &unit);
                }
                sis_object_destroy(kname);
                sis_object_destroy(sname);
                sis_map_list_set(cls_->map_idxs, name, node);   
            }        
            break;    
        default:
            LOG(5)("other hid : %d at snoidx.\n", head_->hid);
            break;
        }
        count++;
    }
    sis_memory_destroy(memory);
    if (ctrl->isstop)
    {
        return -1;
    }
    return count;
}

int sis_disk_io_read_nonidx(s_sis_disk_ctrl *cls_)
{
    if (cls_->work_fps->main_head.index)
    {
        sis_map_list_clear(cls_->map_idxs);
        if (sis_disk_files_open(cls_->widx_fps, SIS_DISK_ACCESS_RDONLY))
        {
            LOG(5)("open idxfile fail.[%s]\n", cls_->widx_fps->cur_name);
            return SIS_DISK_CMD_NO_OPEN_IDX;
        }
        sis_disk_files_read_fulltext(cls_->widx_fps, cls_, cb_sis_disk_io_read_nonidx);
        sis_disk_io_read_dict(cls_);
        sis_disk_files_close(cls_->widx_fps);
         return SIS_DISK_CMD_OK;
    }
    return SIS_DISK_CMD_NO_IDX;
}