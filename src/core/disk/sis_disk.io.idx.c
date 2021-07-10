
#include "sis_disk.h"


size_t _disk_io_write_widx(s_sis_disk_ctrl *cls_, s_sis_disk_wcatch *wcatch_)
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

size_t sis_disk_io_write_widx(s_sis_disk_ctrl *cls_)
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

size_t sis_disk_io_uncompress_widx(s_sis_disk_head  *head_, char *in_, size_t ilen_, s_sis_memoey *memory_)
{
    size_t size = 0;
    if (head_->zip == SIS_DISK_ZIP_NOZIP)
    {
        sis_memory_cat(memory_, in_, ilen_);
    }
    else if (head_->zip == SIS_DISK_ZIP_SNAPPY)
    {
        if(sis_snappy_uncompress(in_, ilen_, memory_))
        {
            size = sis_memory_get_size(memory_);
        }
        else
        {
            LOG(5)("snappy_uncompress fail. %d %zu \n", head_->hid, ilen_);
            return 0;   
        }
    }
    return size;
}

int sis_disk_io_read_dict(s_sis_disk_ctrl *cls_)
{
    s_sis_memory *memory = sis_memory_create();
    {
        s_sis_disk_idx *node = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, SIS_DISK_SIGN_KEY);
        if (node)
        {
            for (int k = 0; k < node->index->count; k++)
            {
                s_sis_disk_idx_unit *unit = sis_disk_idx_get_unit(node, k);
                sis_disk_rcatch_init(cls_->rcatch, unit);
                if (sis_disk_files_read_fromidx(cls_->work_fps, cls_->rcatch) > 0)
                {
                    sis_disk_io_uncompress(cls_, cls_->rcatch);
                    sis_disk_reader_set_kdict(cls_->map_kdicts, sis_memory(cls_->rcatch->memory), sis_memory_get_size(cls_->rcatch->memory));
                }
            }
        }
    }
    {
        s_sis_disk_idx *node = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, SIS_DISK_SIGN_SDB);
        if (node)
        {
            
            for (int k = 0; k < node->index->count; k++)
            {
                s_sis_disk_idx_unit *unit = sis_disk_idx_get_unit(node, k);
                sis_disk_rcatch_init(cls_->rcatch, unit);
                if (sis_disk_files_read_fromidx(cls_->work_fps, cls_->rcatch) > 0)
                {
                    sis_disk_io_uncompress(cls_, cls_->rcatch);
                    sis_disk_reader_set_sdict(cls_->map_sdicts, sis_memory(cls_->rcatch->memory), sis_memory_get_size(cls_->rcatch->memory));
                }
            }
        }
    }
    sis_memory_destroy(memory);
    return 0;
}

int cb_sis_disk_io_read_widx(void *source_, s_sis_disk_head *head_, char *imem_, size_t isize_)
{
    s_sis_disk_ctrl *cls_ = (s_sis_disk_ctrl *)source_;
    // 直接写到内存中
    s_sis_memory *memory = sis_memory_create();
    sis_disk_io_uncompress_widx(head_, imem_, isize_, memory);
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
                sis_disk_io_read_kdict(cls_);
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
                sis_disk_io_read_sdict(cls_);
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
                if (size > 0)
                {
                    s_sis_sds sname = sis_sdsnewlen(sis_memory(memory), slen);
                    sdict = sis_disk_map_get_sdict(cls_->map_sdicts, sname);
                    sis_memory_move(memory, slen);
                    sis_sprintf(name, 255, "%s.%s",kname, sname);
                }
                else
                {
                    sis_sprintf(name, 255, "%s", kname);
                }
                sis_sdsfree(kname);
                sis_sdsfree(sname);
                s_sis_disk_idx *node = sis_disk_idx_create(kdict->name, sdict ? sdict->name : NULL);
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

int sis_disk_io_read_widx(s_sis_disk_ctrl *cls_)
{
    if (cls_->work_fps->main_head.index)
    {
        sis_map_list_clear(cls_->map_idxs);
        if (sis_disk_files_open(cls_->widx_fps, SIS_DISK_ACCESS_RDONLY))
        {
            LOG(5)("open idxfile fail.[%s]\n", cls_->widx_fps->cur_name);
            return SIS_DISK_CMD_NO_OPEN_IDX;
        }
        sis_disk_files_read_fulltext(cls_->widx_fps, cls_, cb_sis_disk_io_read_widx);
        sis_disk_files_close(cls_->widx_fps);
         return SIS_DISK_CMD_OK;
    }
    return SIS_DISK_CMD_NO_IDX;
}