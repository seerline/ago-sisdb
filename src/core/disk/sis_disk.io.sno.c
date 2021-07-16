
#include "sis_disk.h"

// 这里是关于sno的读写函数
size_t _disk_io_write_sno_work(s_sis_disk_ctrl *cls_, s_sis_disk_wcatch *wcatch_)
{ 
    wcatch_->head.fin = 1;
    size_t size = sis_disk_files_write_saveidx(cls_->work_fps, wcatch_); 
    if (wcatch_->head.hid == SIS_DISK_HID_SNO_END)
    { 
        s_sis_disk_idx *node = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, SIS_DISK_SIGN_SNO);
        if (!node)
        {
            node = sis_disk_idx_create(NULL, NULL);
            sis_map_list_set(cls_->map_idxs, SIS_DISK_SIGN_SNO, node);
        }
        sis_disk_idx_set_unit(node, &wcatch_->winfo);
    }
    return size;
}

int cb_incrzip_encode_sno(void *source, char *in, size_t ilen)
{
    s_sis_disk_ctrl *ctrl = (s_sis_disk_ctrl *)source;
    s_sis_disk_wcatch *wcatch = ctrl->wcatch;
    wcatch->head.hid = SIS_DISK_HID_MSG_SNO;
    wcatch->head.zip = SIS_DISK_ZIP_INCRZIP;
    sis_memory_cat(wcatch->memory, in, ilen);        
    size_t size = _disk_io_write_sno_work(ctrl, wcatch);
    sis_disk_wcatch_init(wcatch);
    ctrl->sno_zipsize += size;
    return 0;
}
// 打开前需要设置好 map_kdicts 和 map_sdicts 
int sis_disk_io_write_sno_start(s_sis_disk_ctrl *cls_)
{
    // 文件已经打开并初始化完成 
    // 初始化watch 应该在打开时就初始化好 并设定好位置和偏移
    // 初始化压缩组件
    cls_->sno_incrzip = sis_incrzip_class_create();
    sis_incrzip_set_key(cls_->sno_incrzip, sis_map_list_getsize(cls_->map_kdicts));
    for (int i = 0; i < sis_map_list_getsize(cls_->map_sdicts); i++)
    {
        s_sis_disk_sdict *sdict = (s_sis_disk_sdict *)sis_map_list_geti(cls_->map_sdicts, i);   
        s_sis_dynamic_db *sdb = sis_disk_sdict_last(sdict);    
        sis_incrzip_set_sdb(cls_->sno_incrzip, sdb);
    }
    cls_->sno_msec = 0;
    cls_->sno_count = 0;
    cls_->sno_zipsize = 0;
    sis_incrzip_compress_start(cls_->sno_incrzip, SIS_DISK_MAXLEN_SNOPART, cls_, cb_incrzip_encode_sno);
    return 0;
}
int sis_disk_io_write_sno_stop(s_sis_disk_ctrl *cls_)
{
    sis_incrzip_compress_stop(cls_->sno_incrzip);
    sis_incrzip_class_destroy(cls_->sno_incrzip);
    cls_->sno_incrzip = NULL;
    return 0;
}

void _set_disk_io_sno_msec(s_sis_disk_ctrl *cls_, s_sis_dynamic_db *sdb_, void *in_, size_t ilen_)
{
    msec_t vmsec = 0;
    if (sdb_->field_time)
    {
        vmsec = sis_dynamic_db_get_time(sdb_, 0, in_, ilen_);
        vmsec = sis_time_unit_convert(sdb_->field_time->style, SIS_DYNAMIC_TYPE_MSEC, vmsec);
    }
    if (vmsec > cls_->sno_msec)
    {
        cls_->sno_msec = vmsec;
    }			
}

int sis_disk_io_write_sno(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, void *in_, size_t ilen_)
{
    s_sis_dynamic_db *sdb = sis_disk_sdict_last(sdict_);
    if (!sdb || (ilen_ % sdb->size) != 0)
    {
        return 0;
    }
    _set_disk_io_sno_msec(cls_, sdb, in_, ilen_);
    if (cls_->sno_zipsize > cls_->work_fps->max_page_size)
    {
        s_sis_disk_wcatch *wcatch = cls_->wcatch;
        wcatch->head.hid = SIS_DISK_HID_SNO_END;
        cls_->sno_count++;
        sis_memory_cat_ssize(wcatch->memory, cls_->sno_count); 
        sis_memory_cat_ssize(wcatch->memory, cls_->sno_msec); 
        wcatch->winfo.start = cls_->sno_msec;
        _disk_io_write_sno_work(cls_, wcatch);
        sis_disk_wcatch_init(wcatch);
        cls_->sno_zipsize = 0;
        sis_incrzip_compress_restart(cls_->sno_incrzip);
    }
    int count = ilen_ / sdb->size;
    for (int i = 0; i < count; i++)
    {
        sis_incrzip_compress_step(cls_->sno_incrzip, kdict_->index, sdict_->index, (char *)in_ + i * sdb->size, sdb->size);
    }
    return 0;
}

size_t sis_disk_io_write_sno_widx(s_sis_disk_ctrl *cls_)
{
    s_sis_disk_wcatch *wcatch = cls_->wcatch;
    sis_disk_wcatch_init(wcatch);

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
        wcatch->head.fin = 1;
        wcatch->head.hid = SIS_DISK_HID_INDEX_KEY;
        size += sis_disk_files_write_saveidx(cls_->widx_fps, wcatch);
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
        wcatch->head.fin = 1;
        wcatch->head.hid = SIS_DISK_HID_INDEX_SDB;
        size += sis_disk_files_write_saveidx(cls_->widx_fps, wcatch);
    }
    sis_memory_clear(memory);
    s_sis_disk_idx *snonode = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, SIS_DISK_SIGN_SNO);
    if (snonode)
    {
        sis_memory_cat_ssize(memory, snonode->idxs->count);
        for (int k = 0; k < snonode->idxs->count; k++)
        {
            s_sis_disk_idx_unit *unit = sis_disk_idx_get_unit(snonode, k);
            sis_memory_cat_ssize(memory, unit->fidx);
            sis_memory_cat_ssize(memory, unit->offset);
            sis_memory_cat_ssize(memory, unit->size);
            sis_memory_cat_ssize(memory, unit->start);
        }
        wcatch->head.fin = 1;
        wcatch->head.hid = SIS_DISK_HID_INDEX_SNO;
        size += sis_disk_files_write_saveidx(cls_->widx_fps, wcatch);
    } 
    sis_disk_files_write_sync(cls_->widx_fps);
    LOG(5)("write_index end %zu \n", size);
    return size;
}
////////////////////////////////
////////      read      ////////
////////////////////////////////

static int cb_incrzip_decode_sno(void *source_, int kidx, int sidx, char *in, size_t ilen)
{
    s_sis_disk_ctrl *ctrl = (s_sis_disk_ctrl *)source_;

    s_sis_disk_reader_cb *callback = ctrl->rcatch->callback; 
    // 这里可以做过滤
    s_sis_object *kname = sis_disk_kdict_get_name(ctrl->map_kdicts, kidx);
    s_sis_object *sname = sis_disk_sdict_get_name(ctrl->map_sdicts, sidx);
    sis_object_incr(kname);
    sis_object_incr(sname);
    if (ctrl->rcatch->iswhole || (kname && sname &&
        sis_str_subcmp(SIS_OBJ_SDS(kname), ctrl->rcatch->sub_keys, ',') >= 0 &&
        sis_str_subcmp(SIS_OBJ_SDS(sname), ctrl->rcatch->sub_sdbs, ',') >= 0))
    {
        if (callback->cb_realdate)
        {
            callback->cb_realdate(callback->cb_source, kidx, sidx, in, ilen);
        }
        if (callback->cb_userdate)
        {
            callback->cb_userdate(callback->cb_source, SIS_OBJ_SDS(kname), SIS_OBJ_SDS(sname), in, ilen);     
        }
    }
    sis_object_decr(kname);
    sis_object_decr(sname);
    return 0;
}

int cb_sis_disk_io_read_sno(void *source_, s_sis_disk_head *head_, char *imem_, size_t isize_)
{
    s_sis_disk_ctrl *ctrl = (s_sis_disk_ctrl *)source_;
    s_sis_disk_reader_cb *callback = ctrl->rcatch->callback; 
    // 根据hid不同写入不同的数据到obj
    switch (head_->hid)
    {
    case SIS_DISK_HID_MSG_SNO: // 只有一个key + 可能多条数据
        if(callback->cb_original)
        {
            callback->cb_original(callback->cb_source, head_, imem_, isize_);
        }
        if (callback->cb_realdate || callback->cb_userdate)
        {
            sis_incrzip_uncompress_step(ctrl->sno_incrzip, imem_, isize_);
        }
        break;
    case SIS_DISK_HID_SNO_END: 
        if(callback->cb_original)
        {
            callback->cb_original(callback->cb_source, head_, imem_, isize_);
        }
        break;
    case SIS_DISK_HID_DICT_KEY:
        if(callback && callback->cb_dict_keys)
        {
            callback->cb_dict_keys(callback->cb_source, imem_, isize_);
        }
        break;
    case SIS_DISK_HID_DICT_SDB:
        if(callback && callback->cb_dict_sdbs)
        {
            callback->cb_dict_sdbs(callback->cb_source, imem_, isize_);
        }
        break;
    default:
        LOG(5)("other hid : %d at sno.\n", head_->hid);
        break;
    }
    if (ctrl->isstop)
    {
        return -1;
    }
    return 0;
}

int sis_disk_io_sub_sno(s_sis_disk_ctrl *cls_, const char *subkeys_, const char *subsdbs_,
                    s_sis_msec_pair *search_, void *cb_)
{
    if (!cb_)
    {
        return -1;
    }
    sis_disk_rcatch_init_of_sub(cls_->rcatch, subkeys_, subsdbs_, search_, cb_);
    cls_->isstop = false;  // 用户可以随时中断
    s_sis_disk_reader_cb *callback = cls_->rcatch->callback;   

    if (callback->cb_realdate || callback->cb_userdate)
    {
        cls_->sno_incrzip = sis_incrzip_class_create();
        sis_incrzip_set_key(cls_->sno_incrzip, sis_map_list_getsize(cls_->map_kdicts));
        for (int i = 0; i < sis_map_list_getsize(cls_->map_sdicts); i++)
        {
            s_sis_disk_sdict *sdict = (s_sis_disk_sdict *)sis_map_list_geti(cls_->map_sdicts, i);   
            s_sis_dynamic_db *sdb = sis_disk_sdict_last(sdict);    
            sis_incrzip_set_sdb(cls_->sno_incrzip, sdb);
        }
        sis_incrzip_uncompress_start(cls_->sno_incrzip, cls_, cb_incrzip_decode_sno);
    }

    if(callback->cb_start)
    {
        callback->cb_start(callback->cb_source, cls_->open_date);
    }
    // 每次都从头读起
    sis_disk_files_read_fulltext(cls_->work_fps, cls_, cb_sis_disk_io_read_sno);

    // 不是因为中断而结束 就发送stop标志
    if(callback->cb_stop && !cls_->isstop)
    {
        callback->cb_stop(callback->cb_source, cls_->stop_date);
    }
    if (callback->cb_realdate || callback->cb_userdate)
    {
        sis_incrzip_uncompress_stop(cls_->sno_incrzip);
        sis_incrzip_class_destroy(cls_->sno_incrzip);
        cls_->sno_incrzip = NULL;
    }
    return 0;
}

int cb_sis_disk_io_read_sno_widx(void *source_, s_sis_disk_head *head_, char *imem_, size_t isize_)
{
    s_sis_disk_ctrl *cls_ = (s_sis_disk_ctrl *)source_;
    // 直接写到内存中
    s_sis_memory *memory = sis_memory_create_size(isize_);
    sis_memory_cat(memory, imem_, isize_);
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
    case SIS_DISK_HID_INDEX_SNO:
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
                unit.start = sis_memory_get_ssize(memory);
                sis_struct_list_push(node->idxs, &unit);
            }
            sis_map_list_set(cls_->map_idxs, SIS_DISK_SIGN_SNO, node);   
        }        
        break;    
    default:
        LOG(5)("other hid : %d at snoidx.\n", head_->hid);
        break;
    }
    sis_memory_destroy(memory);
    if (cls_->isstop)
    {
        return -1;
    }
    return 0;
}

int sis_disk_io_read_sno_widx(s_sis_disk_ctrl *cls_)
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
            LOG(5)("open widx fail.[%s]\n", cls_->widx_fps->cur_name);
            return SIS_DISK_CMD_NO_OPEN_IDX;
        }
        sis_disk_files_read_fulltext(cls_->widx_fps, cls_, cb_sis_disk_io_read_sno_widx);
        sis_disk_files_close(cls_->widx_fps);
         return SIS_DISK_CMD_OK;
    }
    return SIS_DISK_CMD_NO_IDX;
}