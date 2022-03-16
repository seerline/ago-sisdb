
#include "sis_disk.h"

// 这里是关于net的读写函数
size_t sis_disk_io_write_sic_work(s_sis_disk_ctrl *cls_, s_sis_disk_wcatch *wcatch_)
{ 
    if (wcatch_->head.zip == SIS_DISK_ZIP_SNAPPY)
    {
        // net 不支持snappy压缩
        wcatch_->head.zip = SIS_DISK_ZIP_NOZIP;
    }
    wcatch_->head.fin = 1;
    size_t size = sis_disk_files_write_saveidx(cls_->work_fps, wcatch_); 
    if (wcatch_->head.hid == SIS_DISK_HID_SIC_NEW  ||
        wcatch_->head.hid == SIS_DISK_HID_DICT_KEY ||
        wcatch_->head.hid == SIS_DISK_HID_DICT_SDB )
    {
        s_sis_disk_idx *node = sis_disk_idx_get(cls_->map_idxs, wcatch_->kname, wcatch_->sname);
        // printf("sis_disk_io_write_sic_work : %p %s \n", node, SIS_OBJ_SDS(wcatch_->kname));
        wcatch_->winfo.active++;
        sis_disk_idx_set_unit(node, &wcatch_->winfo);
    }
    return size;
}
int _write_nums = 0;
size_t _write_size = 0;


int cb_incrzip_encode_sic(void *source, char *in, size_t ilen)
{
    s_sis_disk_ctrl *ctrl = (s_sis_disk_ctrl *)source;
    s_sis_disk_wcatch *wcatch = ctrl->wcatch;
    wcatch->head.hid = SIS_DISK_HID_MSG_SIC;
    wcatch->head.zip = SIS_DISK_ZIP_INCRZIP;
    sis_memory_cat(wcatch->memory, in, ilen);   
    // _write_nums++;
    // _write_size+=ilen;
    // if (_write_size < 300000)
    // {
    //     printf("newnet: %d %zu\n", _write_nums, _write_size);
    //     sis_out_binary("newnet:", in, ilen);     
    // }
    size_t size = sis_disk_io_write_sic_work(ctrl, wcatch);
    sis_disk_wcatch_init(wcatch);
    ctrl->net_zipsize += size;
    return 0;
}
// 打开前需要设置好 map_kdicts 和 map_sdicts 
int sis_disk_io_write_sic_start(s_sis_disk_ctrl *cls_)
{
    // 文件已经打开并初始化完成 
    // 初始化watch 应该在打开时就初始化好 并设定好位置和偏移
    // 初始化压缩组件
    cls_->net_incrzip = sis_incrzip_class_create();
    sis_incrzip_set_key(cls_->net_incrzip, sis_map_list_getsize(cls_->map_kdicts));
    for (int i = 0; i < sis_map_list_getsize(cls_->map_sdicts); i++)
    {
        s_sis_disk_sdict *sdict = (s_sis_disk_sdict *)sis_map_list_geti(cls_->map_sdicts, i);   
        s_sis_dynamic_db *sdb = sis_disk_sdict_last(sdict);    
        sis_incrzip_set_sdb(cls_->net_incrzip, sdb);
    }
    cls_->net_msec = 0;
    cls_->net_pages = 0;
    cls_->net_count = 0;
    cls_->net_zipsize = 0;
    sis_incrzip_compress_start(cls_->net_incrzip, SIS_DISK_MAXLEN_SICPART, cls_, cb_incrzip_encode_sic);
    return 0;
}
int sis_disk_io_write_sic_stop(s_sis_disk_ctrl *cls_)
{
    sis_incrzip_compress_stop(cls_->net_incrzip);
    sis_incrzip_class_destroy(cls_->net_incrzip);
    cls_->net_incrzip = NULL;
    cls_->net_msec = 0;
    cls_->net_pages = 0;
    cls_->net_count = 0;
    cls_->net_zipsize = 0;
    return 0;
}

static void _set_disk_io_sic_msec(s_sis_disk_ctrl *cls_, s_sis_dynamic_db *sdb_, void *in_, size_t ilen_)
{
    if (cls_->net_msec == 0)
    {
        msec_t vmsec = 0;
        if (sdb_->field_time)
        {
            vmsec = sis_dynamic_db_get_time(sdb_, 0, in_, ilen_);
            vmsec = sis_time_unit_convert(sdb_->field_time->style, SIS_DYNAMIC_TYPE_MSEC, vmsec);
        }
        cls_->net_msec = vmsec;
    }
}

int sis_disk_io_write_sic(s_sis_disk_ctrl *cls_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, void *in_, size_t ilen_)
{
    s_sis_dynamic_db *sdb = sis_disk_sdict_last(sdict_);
    if (!sdb || (ilen_ % sdb->size) != 0)
    {
        return 0;
    }
    // cls_->work_fps->max_page_size = 10*1024*1024;

    _set_disk_io_sic_msec(cls_, sdb, in_, ilen_);
    if ((cls_->net_zipsize == 0 && cls_->net_count == 0) || 
         cls_->net_zipsize > cls_->work_fps->max_page_size)
    {
        cls_->net_pages++;

        s_sis_object *mapobj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(SIS_DISK_SIGN_NEW));
        s_sis_disk_wcatch *wcatch = sis_disk_wcatch_create(mapobj, NULL);
        wcatch->head.hid = SIS_DISK_HID_SIC_NEW;
        wcatch->head.zip = SIS_DISK_ZIP_NOZIP;
        sis_memory_cat_ssize(wcatch->memory, cls_->net_msec); 
        sis_memory_cat_ssize(wcatch->memory, cls_->net_pages); 
        sis_memory_cat_ssize(wcatch->memory, cls_->net_count); 
        wcatch->winfo.start = cls_->net_msec;
        sis_disk_io_write_sic_work(cls_, wcatch);
        sis_disk_wcatch_destroy(wcatch);
        sis_object_destroy(mapobj);

        cls_->net_msec = 0;
        cls_->net_zipsize = 0;
        sis_incrzip_compress_restart(cls_->net_incrzip, 1);
        printf("write: %d %d %s\n", kdict_->index, sdict_->index, SIS_OBJ_GET_CHAR(sdict_->name));
    }
    int count = ilen_ / sdb->size;
    cls_->net_count += count;
    for (int i = 0; i < count; i++)
    {
        sis_incrzip_compress_step(cls_->net_incrzip, kdict_->index, sdict_->index, (char *)in_ + i * sdb->size, sdb->size);
    }
    return 0;
}

size_t sis_disk_io_write_sic_widx(s_sis_disk_ctrl *cls_)
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
        size += sis_disk_io_write_noidx(cls_->widx_fps, wcatch);
    }
    sis_memory_clear(memory);
    s_sis_disk_idx *netnode = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, SIS_DISK_SIGN_NEW);
    if (netnode)
    {
        sis_memory_cat_ssize(memory, netnode->idxs->count);
        for (int k = 0; k < netnode->idxs->count; k++)
        {
            s_sis_disk_idx_unit *unit = sis_disk_idx_get_unit(netnode, k);
            sis_memory_cat_ssize(memory, unit->fidx);
            sis_memory_cat_ssize(memory, unit->offset);
            sis_memory_cat_ssize(memory, unit->size);
            sis_memory_cat_ssize(memory, unit->start);
        }
        wcatch->head.fin = 1;
        wcatch->head.hid = SIS_DISK_HID_INDEX_NEW;
        size += sis_disk_io_write_noidx(cls_->widx_fps, wcatch);
    } 
    sis_disk_io_write_widx_tail(cls_);
    sis_disk_files_write_sync(cls_->widx_fps);
    // LOG(5)("write_index end %zu \n", size);
    return size;
}
////////////////////////////////
////////      read      ////////
////////////////////////////////
int _readcode_nums = 0;
size_t _readcode_size = 0;
static int cb_incrzip_decode_sic(void *source_, int kidx, int sidx, char *in, size_t ilen)
{
    s_sis_disk_ctrl *ctrl = (s_sis_disk_ctrl *)source_;

    s_sis_disk_reader_cb *callback = ctrl->rcatch->callback; 
    // 这里可以做过滤
    s_sis_object *kname = sis_disk_kdict_get_name(ctrl->map_kdicts, kidx);
    s_sis_object *sname = sis_disk_sdict_get_name(ctrl->map_sdicts, sidx);
    _readcode_nums++;
    _readcode_size+=ilen;
    if (_readcode_nums % 100000 == 0)
    {
        printf("net: %s.%s %d %zu %zu\n", SIS_OBJ_SDS(kname), SIS_OBJ_SDS(sname), _readcode_nums, ilen, _readcode_size);
    }
    sis_object_incr(kname);
    sis_object_incr(sname);
    if (ctrl->rcatch->iswhole || (kname && sname &&
        sis_str_subcmp(SIS_OBJ_SDS(kname), ctrl->rcatch->sub_keys, ',') >= 0 &&
        sis_str_subcmp(SIS_OBJ_SDS(sname), ctrl->rcatch->sub_sdbs, ',') >= 0))
    {
        if (callback->cb_bytedata)
        {
            callback->cb_bytedata(callback->cb_source, kidx, sidx, in, ilen);
        }
        if (callback->cb_chardata)
        {
            callback->cb_chardata(callback->cb_source, SIS_OBJ_SDS(kname), SIS_OBJ_SDS(sname), in, ilen);     
        }
    }
    sis_object_decr(kname);
    sis_object_decr(sname);
    return 0;
}
int cb_sis_disk_io_read_sic(void *source_, s_sis_disk_head *head_, char *imem_, size_t isize_)
{
    s_sis_disk_ctrl *ctrl = (s_sis_disk_ctrl *)source_;
    s_sis_disk_reader_cb *callback = ctrl->rcatch->callback; 
    // 根据hid不同写入不同的数据到obj
    switch (head_->hid)
    {
    case SIS_DISK_HID_MSG_SIC: // 只有一个key + 可能多条数据
        if(callback->cb_original)
        {
            callback->cb_original(callback->cb_source, head_, imem_, isize_);
        }
        if (callback->cb_bytedata || callback->cb_chardata)
        {
            // _read_nums++;
            // _read_size+=isize_;
            // printf("newnet: %d %zu\n", _read_nums,_read_size );
            // sis_out_binary("read", imem_, isize_);
            sis_incrzip_uncompress_step(ctrl->net_incrzip, imem_, isize_);
        }
        break;
    case SIS_DISK_HID_SIC_NEW: 
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
        LOG(5)("other hid : %d at net.\n", head_->hid);
        break;
    }
    if (ctrl->isstop)
    {
        return -1;
    }
    return 0;
}

int sis_disk_io_sub_sic(s_sis_disk_ctrl *cls_, const char *subkeys_, const char *subsdbs_,
                    s_sis_msec_pair *search_, void *cb_)
{
    if (!cb_)
    {
        return -1;
    }
    sis_disk_rcatch_init_of_sub(cls_->rcatch, subkeys_, subsdbs_, search_, cb_);
    cls_->isstop = false;  // 用户可以随时中断
    s_sis_disk_reader_cb *callback = cls_->rcatch->callback;   

    if (callback->cb_bytedata || callback->cb_chardata)
    {
        cls_->net_incrzip = sis_incrzip_class_create();
        sis_incrzip_set_key(cls_->net_incrzip, sis_map_list_getsize(cls_->map_kdicts));
        for (int i = 0; i < sis_map_list_getsize(cls_->map_sdicts); i++)
        {
            s_sis_disk_sdict *sdict = (s_sis_disk_sdict *)sis_map_list_geti(cls_->map_sdicts, i);   
            s_sis_dynamic_db *sdb = sis_disk_sdict_last(sdict);    
            sis_incrzip_set_sdb(cls_->net_incrzip, sdb);
        }
        sis_incrzip_uncompress_start(cls_->net_incrzip, cls_, cb_incrzip_decode_sic);
    }

    if(callback->cb_start)
    {
        callback->cb_start(callback->cb_source, cls_->open_date);
    }
    // 每次都从头读起
    sis_disk_files_read_fulltext(cls_->work_fps, cls_, cb_sis_disk_io_read_sic);

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
    if (callback->cb_bytedata || callback->cb_chardata)
    {
        sis_incrzip_uncompress_stop(cls_->net_incrzip);
        sis_incrzip_class_destroy(cls_->net_incrzip);
        cls_->net_incrzip = NULL;
    }
    return 0;
}

int cb_sis_disk_io_read_sic_widx(void *source_, s_sis_disk_head *head_, char *imem_, size_t isize_)
{
    s_sis_disk_ctrl *cls_ = (s_sis_disk_ctrl *)source_;
    // 直接写到内存中
    s_sis_memory *memory = sis_memory_create();
    sis_disk_io_unzip_widx(head_, imem_, isize_, memory);
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
    case SIS_DISK_HID_INDEX_NEW:
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
            sis_map_list_set(cls_->map_idxs, SIS_DISK_SIGN_NEW, node);   
        }        
        break;    
    default:
        LOG(5)("other hid : %d at netidx.\n", head_->hid);
        break;
    }
    sis_memory_destroy(memory);
    if (cls_->isstop)
    {
        return -1;
    }
    return 0;
}

int sis_disk_io_read_sic_widx(s_sis_disk_ctrl *cls_)
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
        sis_disk_files_read_fulltext(cls_->widx_fps, cls_, cb_sis_disk_io_read_sic_widx);
        sis_disk_files_close(cls_->widx_fps);
        return SIS_DISK_CMD_OK;
    }
    return SIS_DISK_CMD_NO_IDX;
}

#if 0
#include "sis_disk.h"
// 测试NET文件读写
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

void write_sic(s_sis_disk_writer *cxt)
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
        __write_size += sis_disk_writer_sic(cxt, "k1", "info", &info_data[0], sizeof(s_info));
        __write_size += sis_disk_writer_sic(cxt, "k2", "info", &info_data[1], sizeof(s_info));
        __write_size += sis_disk_writer_sic(cxt, "k3", "info", &info_data[2], sizeof(s_info));
        // for (int i = 0; i < 8; i++)
        // {
        //     __write_size += sis_disk_writer_sic(cxt, "k1", "smsec", &snap_data[i], sizeof(s_msec));
        // }
        // for (int i = 0; i < 5; i++)
        // {
        //     __write_size += sis_disk_writer_sic(cxt, "k2", "smsec", &snap_data[i], sizeof(s_msec));
        // }
        // for (int i = 0; i < 7; i++)
        // {
        //     __write_size += sis_disk_writer_sic(cxt, "k3", "smsec", &snap_data[i], sizeof(s_msec));
        // }
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

    test_map_list_speed();
    if (argc < 2)
    {
        s_sis_disk_writer *wcxt = sis_disk_writer_create(".", "wlog", SIS_DISK_TYPE_SIC);
        write_sic(wcxt);
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
        // cb.cb_original = cb_original;
        cb.cb_chardata  = cb_chardata;
        s_sis_disk_reader *rcxt = sis_disk_reader_create(".", "wlog", SIS_DISK_TYPE_SIC, &cb);
        // s_sis_disk_reader *rcxt = sis_disk_reader_create("../../data/", "test4", SIS_DISK_TYPE_SIC, &cb);
        cb.cb_source = rcxt;
        sis_disk_reader_sub_sic(rcxt, NULL, NULL, 0);
        // sis_disk_reader_sub_sic(rcxt, "*", "*", 20210531);
        sis_disk_reader_destroy(rcxt);
    }


    safe_memory_stop();
    return 0;
}
#endif