
#include "sis_disk.h"

// 这里是关于sno的读写函数
///////////////////////////////////////////////////////
// s_sis_disk_sno_rctrl
///////////////////////////////////////////////////////

s_sis_disk_sno_rctrl *sis_disk_sno_rctrl_create(int pagenums_)
{
    s_sis_disk_sno_rctrl *o = SIS_MALLOC(s_sis_disk_sno_rctrl, o);
    o->rsno_idxs = sis_pointer_list_create();
    o->rsno_idxs->vfree = sis_struct_list_destroy;
    o->rsno_mems = sis_pointer_list_create();
    o->rsno_mems->vfree = sis_memory_destroy;
    o->cursor_blk =  0;
    o->cursor_rec = -1;
    o->pagenums = pagenums_ == 0 ? 1024 : pagenums_ ;
    return o;
}
void sis_disk_sno_rctrl_destroy(s_sis_disk_sno_rctrl *rctrl_)
{
    sis_pointer_list_destroy(rctrl_->rsno_idxs);
    sis_pointer_list_destroy(rctrl_->rsno_mems);
    sis_free(rctrl_);
}
// 清理所有数据
void sis_disk_sno_rctrl_clear(s_sis_disk_sno_rctrl *rctrl_)
{
    rctrl_->count = 0;
    rctrl_->cursor_blk =  0;
    rctrl_->cursor_rec = -1;
    // sis_pointer_list_clear(rctrl_->rsno_idxs);
    // 为增加效率 不释放已经申请的数据索引
    for (int i = 0; i < rctrl_->rsno_idxs->count; i++)
    {
        s_sis_struct_list *slist = sis_pointer_list_get(rctrl_->rsno_idxs, i);
        sis_struct_list_clear(slist);
        memset(slist->buffer, 0, slist->maxcount * slist->len);
    }
    sis_pointer_list_clear(rctrl_->rsno_mems);
}
void sis_disk_sno_rctrl_set(s_sis_disk_sno_rctrl *rctrl_, int sno_, s_sis_db_chars *chars_)
{
    int ipage = sno_ / rctrl_->pagenums + 1;
    while (ipage > rctrl_->rsno_idxs->count)
    {
        s_sis_struct_list *slist = sis_struct_list_create(sizeof(s_sis_db_chars));
        sis_struct_list_set_size(slist, rctrl_->pagenums);
        memset(slist->buffer, 0, slist->maxcount * slist->len);
        sis_pointer_list_push(rctrl_->rsno_idxs, slist);
    }
    s_sis_struct_list *slist = sis_pointer_list_get(rctrl_->rsno_idxs, ipage - 1);
    int irec = sno_ % rctrl_->pagenums;
    memmove((char *)slist->buffer + (irec * slist->len), chars_, slist->len);
    if (irec > slist->count - 1)
    {
        slist->count = irec + 1;
    }
}
// #include "stk_struct.v3.h"
// 放入一个标准块 返回实际的数量
int sis_disk_sno_rctrl_push(s_sis_disk_sno_rctrl *rctrl_, const char *kname_, const char *sname_, int dbsize_, s_sis_memory *imem_)
{
    s_sis_db_chars chars;
    chars.kname = kname_;
    chars.sname = sname_;
    chars.size = dbsize_;
    size_t isize = sis_memory_get_size(imem_);
    s_sis_memory *memory = sis_memory_create_size(isize);
    s_sis_struct_list *snos = sis_struct_list_create(sizeof(int));
    sis_struct_list_set_maxsize(snos, isize / dbsize_);
    while(sis_memory_get_size(imem_) > 0)
    {
        int sno = sis_memory_get_ssize(imem_);
        sis_struct_list_push(snos, &sno);
        sis_memory_cat(memory, sis_memory(imem_), dbsize_);
        sis_memory_move(imem_, dbsize_);
        rctrl_->count++;
    }  

    if (snos->count > 0)
    {
        for (int i = 0; i < snos->count; i++)
        {
            chars.data = sis_memory(memory) + i * dbsize_;
            int *sno = sis_struct_list_get(snos, i);
            // s_v4_stk_snapshot *snap = (s_v4_stk_snapshot *)chars.data;
            // printf("::: %s %s %d %d\n", kname_, sname_, *sno, sis_msec_get_itime(snap->time));
            sis_disk_sno_rctrl_set(rctrl_, *sno, &chars);            
        }       
        sis_pointer_list_push(rctrl_->rsno_mems ,memory);
    }  
    else
    {
        sis_memory_destroy(memory);
    }
    sis_struct_list_destroy(snos);
    return 0; 
}
// 从头开始读数据
void sis_disk_sno_rctrl_init(s_sis_disk_sno_rctrl *rctrl_)
{
    rctrl_->cursor_blk = 0;
    rctrl_->cursor_rec =-1;
}
// static int _ropo_nums = 0;
// 弹出一条记录 只是移动 cursor 指针 
s_sis_db_chars *sis_disk_sno_rctrl_rpop(s_sis_disk_sno_rctrl *rctrl_)
{
    s_sis_db_chars *chars = NULL;
    int cursor = 0;
    while (rctrl_->cursor_blk < rctrl_->rsno_idxs->count)
    {
        s_sis_struct_list *slist = sis_pointer_list_get(rctrl_->rsno_idxs, rctrl_->cursor_blk);
next:
        cursor = rctrl_->cursor_rec + 1;
        // if (_ropo_nums++ % 10000 == 0)
        // printf("%d|%d %d %d %d\n", slist->count, rctrl_->cursor_blk, cursor, rctrl_->rsno_idxs->count, rctrl_->rsno_mems->count);
        if (cursor < slist->count)
        {
            rctrl_->cursor_rec = cursor;
            s_sis_db_chars *curchars = sis_struct_list_get(slist, cursor);
        // if (_ropo_nums % 10000 == 0)
        // printf("%s %p|%d %d %d %d %d\n", curchars->kname, curchars->data, rctrl_->cursor_blk, rctrl_->rsno_idxs->count, cursor, slist->count,  rctrl_->rsno_mems->count);
            if (curchars->data)
            {
                chars = curchars;
                break;
            }
            else
            {
                goto next;
            }
        }
        else
        {
            rctrl_->cursor_blk++;
            rctrl_->cursor_rec =-1;
        }   
    }
    return chars;
}

///////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////

// 打开前需要设置好 map_kdicts 和 map_sdicts 
int sis_disk_io_write_sno_start(s_sis_disk_ctrl *cls_)
{
    // 文件已经打开并初始化完成 
    // 初始化watch 应该在打开时就初始化好 并设定好位置和偏移
    // 初始化压缩组件
    // cls_->sno_pages = 0;
    // 这里应该判断文件是否存在 如果存在去找最新的page 
    // 并且还要判断如果没有写新页结束标记 就写上页的结束标志 然后开始写新数据
    // 现在这里直接赋予 0 要求SNO必须一次写完整成功 否则文件会出错
    cls_->sno_msec = 0;
    cls_->sno_count = 0;
    cls_->sno_size = 0;
    cls_->sno_series = 0;
    cls_->sno_wcatch = sis_map_list_create(sis_disk_wcatch_destroy);
    return 0;
}
size_t _disk_io_write_sno_watch(s_sis_disk_ctrl *cls_)
{
    size_t size = 0;
    int count = sis_map_list_getsize(cls_->sno_wcatch);
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_wcatch *wcatch = (s_sis_disk_wcatch *)sis_map_list_geti(cls_->sno_wcatch, i);
        wcatch->head.hid = SIS_DISK_HID_MSG_SNO;
        wcatch->head.zip = SIS_DISK_ZIP_SNAPPY;
        if (sis_memory_get_size(wcatch->memory) > 0)
        {
            size += sis_disk_io_write_sdb_work(cls_, wcatch);
            sis_disk_wcatch_clear(wcatch);
        }      
    }
    cls_->sno_pages++;
    // 写页结束符号
    s_sis_object *mapobj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(SIS_DISK_SIGN_END));
    s_sis_disk_wcatch *wcatch = sis_disk_wcatch_create(mapobj, NULL);
    wcatch->head.hid = SIS_DISK_HID_SNO_END;
    wcatch->head.zip = SIS_DISK_ZIP_NOZIP;
    sis_memory_cat_ssize(wcatch->memory, cls_->sno_msec); 
    sis_memory_cat_ssize(wcatch->memory, cls_->sno_pages); 
    sis_memory_cat_ssize(wcatch->memory, cls_->sno_count); 
    wcatch->winfo.start = cls_->sno_msec;
    size += sis_disk_io_write_sdb_work(cls_, wcatch);
    sis_disk_wcatch_destroy(wcatch);
    sis_object_destroy(mapobj);

    cls_->sno_size = 0;
    cls_->sno_series = 0;

    LOG(5)("write sno page ok. pageno= %d count = %lld\n", cls_->sno_pages, cls_->sno_count);
    return size;
}
int sis_disk_io_write_sno_stop(s_sis_disk_ctrl *cls_)
{
    _disk_io_write_sno_watch(cls_);
    sis_map_list_destroy(cls_->sno_wcatch);
    cls_->sno_wcatch = NULL;
    return 0;
}
static void _set_disk_io_sno_msec(s_sis_disk_ctrl *cls_, s_sis_dynamic_db *sdb_, int index, void *in_, size_t ilen_)
{
    msec_t vmsec = 0;
    if (sdb_->field_time)
    {
        vmsec = sis_dynamic_db_get_time(sdb_, index, in_, ilen_);
        vmsec = sis_time_unit_convert(sdb_->field_time->style, SIS_DYNAMIC_TYPE_MSEC, vmsec);
    }
    cls_->sno_msec = sis_max(cls_->sno_msec, vmsec);
}
int sis_disk_io_write_sno(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, void *in_, size_t ilen_)
{
    s_sis_dynamic_db *sdb = sis_disk_sdict_last(sdict_);
    if (!sdb || (ilen_ % sdb->size) != 0)
    {
        return 0;
    }
    char snoname[255];
    sis_sprintf(snoname, 255, "%s.%s", SIS_OBJ_SDS(kdict_->name), SIS_OBJ_SDS(sdict_->name));        
    s_sis_disk_wcatch *wcatch = (s_sis_disk_wcatch *)sis_map_list_get(cls_->sno_wcatch, snoname);
    if (!wcatch)
    {
        wcatch = sis_disk_wcatch_create(kdict_->name, sdict_->name);
        sis_map_list_set(cls_->sno_wcatch, snoname, wcatch);
    }
    if (sis_memory_get_size(wcatch->memory) < 1)
    {
        cls_->sno_size += sis_memory_cat_ssize(wcatch->memory, kdict_->index);
        cls_->sno_size += sis_memory_cat_ssize(wcatch->memory, sdict_->index);
        wcatch->winfo.start = sis_dynamic_db_get_time(sdb, 0, in_, ilen_);
    }
    wcatch->winfo.sdict = sdict_->sdbs->count;
    wcatch->winfo.ipage = cls_->sno_pages;

    int count = ilen_ / sdb->size;
    cls_->sno_count += count;
    for (int i = 0; i < count; i++)
    {
        cls_->sno_size += sis_memory_cat_ssize(wcatch->memory, cls_->sno_series);
        cls_->sno_series++;
        cls_->sno_size += sis_memory_cat(wcatch->memory, (char *)in_ + i * sdb->size, sdb->size);
    }
    _set_disk_io_sno_msec(cls_, sdb, count - 1, in_, ilen_);
    // if (cls_->sno_series % 100000 == 1)
    // printf("%zu %s %lld %d %d--> %zu\n", cls_->sno_size , snoname, cls_->sno_count, count, cls_->sno_series, cls_->work_fps->max_page_size);
    if (cls_->sno_size > cls_->work_fps->max_page_size)
    {
        // printf("%zu %zu %zu %d\n", cls_->sno_size, cls_->work_fps->max_page_size, cls_->sno_series, cls_->sno_pages);
        _disk_io_write_sno_watch(cls_);
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
        wcatch->head.zip = SIS_DISK_ZIP_SNAPPY;
        size += sis_disk_io_write_noidx(cls_->widx_fps, wcatch);
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
        wcatch->head.zip = SIS_DISK_ZIP_SNAPPY;
        size += sis_disk_io_write_noidx(cls_->widx_fps, wcatch);
    }
    sis_memory_clear(memory);
    s_sis_disk_idx *snonode = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, SIS_DISK_SIGN_END);
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
        wcatch->head.hid = SIS_DISK_HID_INDEX_END;
        wcatch->head.zip = SIS_DISK_ZIP_SNAPPY;
        size += sis_disk_io_write_noidx(cls_->widx_fps, wcatch);
    } 
    sis_memory_clear(memory);
    wcatch->head.hid = SIS_DISK_HID_INDEX_SNO;
    wcatch->head.zip = SIS_DISK_ZIP_SNAPPY;
    int count = sis_map_list_getsize(cls_->map_idxs);
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_idx *node = (s_sis_disk_idx *)sis_map_list_geti(cls_->map_idxs, i);
        // 排除key和sdb
        if (node == sdbnode || node == keynode || node == snonode)
        {
            continue;
        }
        // printf("write_index %s %d \n", SIS_OBJ_SDS(node->key), node->idxs->count);
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
            sis_memory_cat_byte(memory, unit->active, 1);
            sis_memory_cat_byte(memory, unit->ktype, 1);
            sis_memory_cat_byte(memory, unit->sdict, 1);
            sis_memory_cat_ssize(memory, unit->fidx);
            sis_memory_cat_ssize(memory, unit->offset);
            sis_memory_cat_ssize(memory, unit->size);
            sis_memory_cat_ssize(memory, unit->start);
            sis_memory_cat_ssize(memory, unit->ipage);
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
    return size;
}
////////////////////////////////
////////      read      ////////
////////////////////////////////

// 因为可能会和其他数据库联合输出 
// 订阅全部信息 返回原始数据 
int _sis_disk_read_hid_sno(s_sis_disk_ctrl *ctrl, s_sis_memory *memory)
{ 
    if (ctrl->sno_rctrl)
    {
        int kidx = sis_memory_get_ssize(memory);
        int sidx = sis_memory_get_ssize(memory);
        s_sis_object *kname = sis_disk_kdict_get_name(ctrl->map_kdicts, kidx);
        s_sis_disk_sdict *sdict = sis_map_list_geti(ctrl->map_sdicts, sidx);
        if (!kname||!sdict)
        {
            return 0;
        }
        s_sis_dynamic_db *sdb = sis_disk_sdict_last(sdict);
        sis_disk_sno_rctrl_push(ctrl->sno_rctrl, SIS_OBJ_SDS(kname), SIS_OBJ_SDS(sdict->name), sdb->size, memory);
    }
    return 1;
}
void sis_disk_sno_rctrl_start(s_sis_disk_ctrl *cls_)
{
    s_sis_db_chars *chars = sis_disk_sno_rctrl_rpop(cls_->sno_rctrl);
    s_sis_disk_reader_cb *callback = cls_->rcatch->callback; 
    LOG(8)("start chars= %p %p %p\n", chars, callback->cb_bytedata, callback->cb_chardata);
    while (chars && !cls_->isstop)
    {
        if(callback->cb_chardata)
        {
            callback->cb_chardata(callback->cb_source, chars->kname, chars->sname, 
                chars->data, chars->size);
        }
        if(callback->cb_bytedata)
        {
            int kidx = sis_disk_kdict_get_idx(cls_->map_kdicts, chars->kname);
            int sidx = sis_disk_sdict_get_idx(cls_->map_sdicts, chars->sname);
            // printf("start chars= %d %d : %d\n", kidx, sidx, chars->size);
            callback->cb_bytedata(callback->cb_source, kidx, sidx, 
                chars->data, chars->size);
        }        
        chars = sis_disk_sno_rctrl_rpop(cls_->sno_rctrl);
    }
    LOG(8)("start chars ok= %p\n", chars);
}
int cb_sis_disk_io_read_sno(void *source_, s_sis_disk_head *head_, char *imem_, size_t isize_)
{
    s_sis_disk_ctrl *ctrl = (s_sis_disk_ctrl *)source_;
    s_sis_disk_reader_cb *callback = ctrl->rcatch->callback; 
    // 根据hid不同写入不同的数据到obj
    s_sis_memory *memory = ctrl->rcatch->memory;
    if (sis_disk_ctrl_unzip(ctrl, head_, imem_, isize_, memory) > 0)
    {
        // printf("cb_sis_disk_io_read_sno, %d %d %zu %zu\n", head_->hid, head_->zip, isize_, sis_memory_get_size(memory));
        head_->zip = SIS_DISK_ZIP_NOZIP;
        switch (head_->hid)
        {
        case SIS_DISK_HID_MSG_SNO: // 只有一个key + 可能多条数据
            if(callback->cb_original)
            {
                callback->cb_original(callback->cb_source, head_, sis_memory(memory), sis_memory_get_size(memory));
            }
            _sis_disk_read_hid_sno(ctrl, memory);
            break;
        case SIS_DISK_HID_SNO_END: 
            if(callback->cb_original)
            {
                callback->cb_original(callback->cb_source, head_, sis_memory(memory), sis_memory_get_size(memory));
            }
            // printf("+++++=== 1.1  %d\n", ctrl->isstop);
            if (ctrl->sno_rctrl)
            {
                sis_disk_sno_rctrl_start(ctrl);
                // printf("+++++=== 1.1.1 %d\n", ctrl->isstop);
                sis_disk_sno_rctrl_clear(ctrl->sno_rctrl);
            }
            // printf("+++++=== 1.0  %d\n", ctrl->isstop);
            break;
        case SIS_DISK_HID_DICT_KEY:
            // if(callback && callback->cb_dict_keys)
            // {
            //     callback->cb_dict_keys(callback->cb_source, sis_memory(memory), sis_memory_get_size(memory));
            // }
            break;
        case SIS_DISK_HID_DICT_SDB:
            // if(callback && callback->cb_dict_sdbs)
            // {
            //     callback->cb_dict_sdbs(callback->cb_source, sis_memory(memory), sis_memory_get_size(memory));
            // }
            break;
        default:
            LOG(5)("other hid : %d at sno.\n", head_->hid);
            break;
        }
    }
    // printf("sub 11111...................%d\n", ctrl->isstop);
    if (ctrl->isstop)
    {
        return -1;
    }
    return 0;
}
/////////////////
//  sub function
static void _disk_io_callback_sno_dict(s_sis_disk_ctrl *cls_, s_sis_disk_reader_cb *callback)
{
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

void _disk_io_sub_sno_parts(s_sis_disk_ctrl *ctrl, s_sis_disk_rcatch *rcatch, s_sis_pointer_list *subparts)
{
    sis_pointer_list_clear(subparts);
    int nums = sis_map_list_getsize(ctrl->map_idxs);
    for (int k = 0; k < nums; k++)
    {
        s_sis_disk_idx *subidx = (s_sis_disk_idx *)sis_map_list_geti(ctrl->map_idxs, k);
        if (!subidx->kname) //表示为key sdb 或 sno net
        {
            continue;
        }
        // printf("== filter : %d %p %p\n", nums, subidx->kname, subidx->sname);
        if ((!sis_strcasecmp(rcatch->sub_sdbs, "*") || sis_str_subcmp_strict(SIS_OBJ_SDS(subidx->sname), rcatch->sub_sdbs, ',') >= 0) &&
            (!sis_strcasecmp(rcatch->sub_keys, "*") || sis_str_subcmp(SIS_OBJ_SDS(subidx->kname), rcatch->sub_keys, ',') >= 0))
        {
            // printf("== adda : %d %p %p\n", count, subidx->kname, subidx->sname);
            sis_pointer_list_push(subparts, subidx);
        }
    }      
}
int sis_disk_io_sub_sno_part(s_sis_disk_ctrl *cls_, s_sis_disk_rcatch *rcatch_)
{
    s_sis_pointer_list *subparts = sis_pointer_list_create(); 
    // 获取数据索引列表 --> filters
    _disk_io_sub_sno_parts(cls_, rcatch_, subparts);
    LOG(5)("sub filters count =  %d %s %s\n", subparts->count, rcatch_->sub_keys, rcatch_->sub_sdbs);
    if(subparts->count < 1)
    {
        sis_pointer_list_destroy(subparts);
        return 0;
    }
    int minpage = -1;
    for (int i = 0; i < subparts->count; i++)
    {
        s_sis_disk_idx *subidx = (s_sis_disk_idx *)sis_pointer_list_get(subparts, i);
        subidx->cursor = 0;
        s_sis_disk_idx_unit *unit = sis_struct_list_get(subidx->idxs, 0);
        minpage = minpage == -1 ? unit->ipage : minpage < unit->ipage ? minpage : unit->ipage;
    }
    // 按页获取数据
    s_sis_memory *omem = sis_memory_create();
    s_sis_memory *imem = sis_memory_create();
    for (uint32 page = minpage; page < cls_->sno_pages; page++)
    {
        // LOG(5)("sno === ipage = %d : %d. subparts = %d\n", page, cls_->sno_pages, subparts->count);
        for (int i = 0; i < subparts->count; i++)
        {
            s_sis_disk_idx *subidx = (s_sis_disk_idx *)sis_pointer_list_get(subparts, i);
            s_sis_disk_idx_unit *unit = sis_struct_list_get(subidx->idxs, subidx->cursor);
            if (unit && unit->ipage == page)
            {
                // 一页中同一个数据只有一份
                // 先读取指定块 然后写入sno的缓存中
                s_sis_disk_head head = {0};
                sis_memory_clear(omem);
                if(sis_disk_files_read(cls_->work_fps, unit->fidx, unit->offset, unit->size, &head, omem) > 0)
                {
                    sis_memory_clear(imem);
                    if (sis_disk_ctrl_unzip(cls_, &head, sis_memory(omem), sis_memory_get_size(omem), imem) > 0)
                    {
                        _sis_disk_read_hid_sno(cls_, imem);
                    }
                }
                else
                {
                    LOG(5)("sno index fail.\n");
                }
                subidx->cursor++;
            }
            if (cls_->isstop)
            {
                break;
            }
        }
        // 这里应该释放了内存
        if (cls_->isstop)
        {
            break;
        }
        if (cls_->sno_rctrl)
        {
            sis_disk_sno_rctrl_start(cls_);
            sis_disk_sno_rctrl_clear(cls_->sno_rctrl);
        }
    } // page
    sis_memory_destroy(imem);
    sis_memory_destroy(omem);
    sis_pointer_list_destroy(subparts); 
    return 0;
}

/**
 * @brief 
 * @param cls_ 
 * @param subkeys_ 需要读取行情的股票列表
 * @param subsdbs_ 需要读取行情的数据格式，JSON
 * @param search_ 
 * @param cb_ 回调函数组合
 * @return int 
 */
int sis_disk_io_sub_sno(s_sis_disk_ctrl *cls_, const char *subkeys_, const char *subsdbs_,
                    s_sis_msec_pair *search_, void *cb_)
{
    if (!cb_)
    {
        return -1;
    }
    sis_disk_rcatch_init_of_sub(cls_->rcatch, subkeys_, subsdbs_, search_, cb_);
    cls_->isstop = false;  // 用户可以随时中断

    /**
     * @brief 回调函数组合，实际上这里的callback与函数入参cb_完全相等
     */
    s_sis_disk_reader_cb *callback = cls_->rcatch->callback;   

    if (callback->cb_bytedata || callback->cb_chardata)
    {
        cls_->sno_rctrl = sis_disk_sno_rctrl_create(1024*1024); 
    }

    // 通知订阅者文件读取已开始
    if(callback->cb_start)
    {
        callback->cb_start(callback->cb_source, cls_->open_date);
    }
    cls_->sno_count = 0;
    if (cls_->rcatch->iswhole)
    {
        // 合并返回key和sdb
        _disk_io_callback_sno_dict(cls_, callback);
        sis_disk_files_read_fulltext(cls_->work_fps, cls_, cb_sis_disk_io_read_sno);
    }
    else
    {
        // 按顺序返回订阅的股票
        _disk_io_callback_sno_dict(cls_, callback);
        sis_disk_io_sub_sno_part(cls_, cls_->rcatch); 
    }
    // printf("sub sno stop...................%d\n", cls_->isstop);
    // 不是因为中断而结束 就发送stop标志
    if (callback->cb_bytedata || callback->cb_chardata)
    {
        sis_disk_sno_rctrl_destroy(cls_->sno_rctrl);
        cls_->sno_rctrl = NULL;
    }
    if (cls_->isstop)
    {
        // 通知订阅者文件读取已中断
        if(callback->cb_break)
        {
            callback->cb_break(callback->cb_source, cls_->stop_date);
        }
    }
    else
    {
        // 通知订阅者文件读取已正常结束
        if(callback->cb_stop)
        {
            callback->cb_stop(callback->cb_source, cls_->stop_date);
        }
    }
    return 0;
}

int cb_sis_disk_io_read_sno_widx(void *source_, s_sis_disk_head *head_, char *imem_, size_t isize_)
{
    s_sis_disk_ctrl *cls_ = (s_sis_disk_ctrl *)source_;
    // 直接写到内存中
    s_sis_memory *memory = sis_memory_create();
    sis_disk_io_unzip_widx(head_, imem_, isize_, memory);
    char name[255];
    int  count = 0;
    int  maxpage = 0;
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
        case SIS_DISK_HID_INDEX_END:
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
                sis_map_list_set(cls_->map_idxs, SIS_DISK_SIGN_END, node);   
            }        
            break;    
        case SIS_DISK_HID_INDEX_SNO:
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
                sis_sdsfree(kname);
                // printf("%d %s %s\n", cls_->style, cls_->work_fps->cur_name, name);

                s_sis_disk_idx *node = sis_disk_idx_create(kdict->name, sdict ? sdict->name : NULL);
                int blocks = sis_memory_get_ssize(memory);
                for (int i = 0; i < blocks; i++)
                {
                    s_sis_disk_idx_unit unit;
                    memset(&unit, 0, sizeof(s_sis_disk_idx_unit));
                    unit.active = sis_memory_get_byte(memory, 1);
                    unit.ktype = sis_memory_get_byte(memory, 1);
                    unit.sdict = sis_memory_get_byte(memory, 1);
                    unit.fidx = sis_memory_get_ssize(memory);  // 和 SBD 不同
                    unit.offset = sis_memory_get_ssize(memory);
                    unit.size = sis_memory_get_ssize(memory);
                    unit.start = sis_memory_get_ssize(memory);
                    unit.ipage = sis_memory_get_ssize(memory);  // 和 SBD 不同
                    maxpage = sis_max(maxpage, unit.ipage);
                    unit.stop = 0;                              // 和 SBD 不同
                    sis_struct_list_push(node->idxs, &unit);
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
    cls_->sno_pages = maxpage + 1;
    sis_memory_destroy(memory);
    if (cls_->isstop)
    {
        return -1;
    }
    return count;
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

#if 0
#include "sis_disk.h"
// 测试SNO文件读写
static int    __read_nums = 0;
static size_t __read_size = 0;
static msec_t __read_msec = 0;
static int    __write_nums = 1*10;//00*1000;
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
    \"sdate\":{\"fields\":{\"time\":[\"D\",4],\"value\":[\"U\",4]}},\
    \"sminu\":{\"fields\":{\"time\":[\"M\",4],\"value\":[\"U\",4]}},\
    \"sssec\":{\"fields\":{\"time\":[\"S\",4],\"value\":[\"U\",4]}},\
    \"smsec\":{\"fields\":{\"time\":[\"T\",8],\"value\":[\"U\",4]}}}";

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
static int    __read_nums1 = 0;
static void cb_original(void *src, s_sis_disk_head *head_, void *out_, size_t olen_)
{
    printf("%d : %d\n", head_->hid, head_->zip);
    sis_out_binary("sno", out_, olen_);
    __read_nums1++;
    if (__read_nums1 % 1000 == 0 || __read_nums1 < 10)
    {
        printf("%s : %zu %d\n", __func__, olen_, __read_nums1);
    }
}
static void cb_chardata(void *src, const char *kname_, const char *sname_, void *out_, size_t olen_)
{
    __read_nums++;
    if (__read_nums % sis_max((__write_nums / 10), 1) == 0 || __read_nums < 10)
    {
        printf("%s : %s.%s %zu %d\n", __func__, kname_, sname_, olen_, __read_nums);
    }
    // if (__read_nums == 300000)
    // {
    //     sis_disk_reader_unsub((s_sis_disk_reader *)src);
    // }
}

void write_sno(s_sis_disk_writer *cxt)
{
    s_info info_data[3] = { 
        { "k10000" },
        { "k20000" },
        { "k30000" },
    };
    s_msec snap_data[10] = { 
        { 1000,  3000},
        { 2000,  4000},
        { 3000,  5000},
        { 4000,  6000},
        { 5000,  7000},
        { 6000,  8000},
        { 7000,  9000},
        { 8000,  9900},
        { 9000,  9000},
        { 10000, 9988},
    };

    sis_disk_writer_open(cxt, 0);
    sis_disk_writer_set_kdict(cxt, inkeys, sis_strlen(inkeys));
    sis_disk_writer_set_sdict(cxt, insdbs, sis_strlen(insdbs));
    // cxt->munit->work_fps->max_page_size
    sis_disk_writer_start(cxt);
    int count = __write_nums;
    __write_msec = sis_time_get_now_msec();
    for (int k = 0; k < count; k++)
    {
        __write_size += sis_disk_writer_sno(cxt, "k11", "info", &info_data[0], sizeof(s_info));
        __write_size += sis_disk_writer_sno(cxt, "k12", "info", &info_data[1], sizeof(s_info));
        __write_size += sis_disk_writer_sno(cxt, "k3", "info", &info_data[2], sizeof(s_info));
        for (int i = 0; i < 8; i++)
        {
            __write_size += sis_disk_writer_sno(cxt, "k11", "smsec", &snap_data[i], sizeof(s_msec));
        }
        for (int i = 0; i < 5; i++)
        {
            __write_size += sis_disk_writer_sno(cxt, "k12", "smsec", &snap_data[i], sizeof(s_msec));
        }
        for (int i = 0; i < 7; i++)
        {
            __write_size += sis_disk_writer_sno(cxt, "k3", "smsec", &snap_data[i], sizeof(s_msec));
        }
    }
    sis_disk_writer_stop(cxt);
    sis_disk_writer_close(cxt);
    printf("write end %d %zu | cost: %lld.\n", __write_nums, __write_size, sis_time_get_now_msec() - __write_msec);
}
void test_map_list_speed()
{
    char *info[7] = {
         "k1",   
         "k2",   
         "k3",   
         "smsec",
         "sdate",
         "sminu",
         "sssec",
    };
    s_sis_map_list *slist = sis_map_list_create(NULL);
    sis_map_list_set(slist, info[0], info[0]);
    sis_map_list_set(slist, info[1], info[1]);
    sis_map_list_set(slist, info[2], info[2]);
    sis_map_list_set(slist, info[3], info[3]);
    sis_map_list_set(slist, info[4], info[4]);
    sis_map_list_set(slist, info[5], info[5]);
    sis_map_list_set(slist, info[6], info[6]);

    __write_msec = sis_time_get_now_msec();
    for (int i = 0; i < __write_nums * 20; i++)
    {
        int k = sis_int_random(0, 6);
        sis_map_list_get_index(slist, info[k]);
    }
    printf("stop cost: %lld.\n", sis_time_get_now_msec() - __write_msec);
    sis_map_list_destroy(slist);
}
int main(int argc, char **argv)
{
    safe_memory_start();

    // test_map_list_speed();
    if (argc < 2)
    {
        s_sis_disk_writer *wcxt = sis_disk_writer_create(".", "wlog", SIS_DISK_TYPE_SNO);
        write_sno(wcxt);
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
        s_sis_disk_reader *rcxt = sis_disk_reader_create(".", "wlog", SIS_DISK_TYPE_SNO, &cb);
        // s_sis_disk_reader *rcxt = sis_disk_reader_create("../../data/", "test4", SIS_DISK_TYPE_SNO, &cb);
        cb.cb_source = rcxt;
        // sis_disk_reader_sub_sno(rcxt, NULL, NULL, 0);
        sis_disk_reader_sub_sno(rcxt, "k1", "info", 0);
        sis_disk_reader_destroy(rcxt);
    }


    safe_memory_stop();
    return 0;
}
#endif