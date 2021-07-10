
#include "sis_disk.h"
#include "sis_time.h"
///////////////////////////
//  s_sis_disk_idx
///////////////////////////
s_sis_disk_idx *sis_disk_idx_create(s_sis_object *kname_, s_sis_object *sname_)
{
    s_sis_disk_idx *o = SIS_MALLOC(s_sis_disk_idx, o);
    if (kname_)
    {
        o->kname = sis_object_incr(kname_);
    }
    if (sname_)
    {
        o->sname = sis_object_incr(sname_);
    }
    o->idxs = sis_struct_list_create(sizeof(s_sis_disk_idx_unit));
    return o;
}
void sis_disk_idx_destroy(void *in_)
{
    s_sis_disk_idx *o = (s_sis_disk_idx *)in_;
    sis_object_decr(o->kname);
    sis_object_decr(o->sname);
    sis_struct_list_destroy(o->idxs);
    sis_free(o);
}
int sis_disk_idx_set_unit(s_sis_disk_idx *cls_, s_sis_disk_idx_unit *unit_)
{
    return sis_struct_list_push(cls_->idxs, unit_);
}

s_sis_disk_idx_unit *sis_disk_idx_get_unit(s_sis_disk_idx *cls_, int index_)
{
    return (s_sis_disk_idx_unit *)sis_struct_list_get(cls_->idxs, index_);
}


////////////////////////////////////////////////////////
// s_sis_disk_ctrl
////////////////////////////////////////////////////////

s_sis_disk_ctrl *sis_disk_unit_create(int style_, const char *fpath_, const char *fname_, int wdate_)
{
    s_sis_disk_ctrl *o = SIS_MALLOC(s_sis_disk_ctrl, o);
    o->wcatch = sis_disk_wcatch_create(NULL, NULL);
    o->work_fps = sis_disk_files_create();
    o->style = style_;

    o->open_date = wdate_ == 0 ? sis_time_get_idate(0) : wdate_;
    o->stop_date = o->open_date;
    o->work_fps->main_head.style = style_;
    o->work_fps->main_head.index = 0;
    o->work_fps->main_head.wdate = o->open_date;

    // o->map_keys = sis_map_list_create(sis_disk_dict_destroy);
    // o->map_sdbs = sis_map_list_create(sis_disk_dict_destroy);

    char work_fn[1024];
    char widx_fn[1024];
    switch (style_)
    {
    case SIS_DISK_TYPE_SNO: 
        {
            int nyear = o->open_date / 10000;
            sis_sprintf(work_fn, 1024, "%s/%s/%s/%d/%d.%s",
                    fpath_, fname_, SIS_DISK_SNO_CHAR, nyear, o->open_date, SIS_DISK_SNO_CHAR);
            // 这里虽然没有设置压缩方式 实际传入的数据就已经按增量压缩好了
            o->work_fps->max_page_size = SIS_DISK_MAXLEN_SNOPAGE;
            o->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
        }
        break;
    case SIS_DISK_TYPE_SDB_YEAR:  // name/20000101-20091231.sdb
        {
            int nyear = o->open_date / 100000 * 10;
            o->open_date = (nyear + 0) * 10000 +  101;
            o->stop_date = (nyear + 9) * 10000 + 1231;

            sis_sprintf(work_fn, 1024, "%s/%s/%d-%d.%s",
                    fpath_, fname_, o->open_date, o->stop_date, SIS_DISK_SDB_CHAR);
            cls_->work_fps->main_head.index = 1;
            cls_->work_fps->max_page_size = 0;
            cls_->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
            sis_sprintf(widx_fn, 1024, "%s/%s/%d-%d.%s",
                    fpath_, fname_, o->open_date, o->stop_date, SIS_DISK_IDX_CHAR);
        }
        break;
    case SIS_DISK_TYPE_SDB_DATE:  // name/2021/20210606.sdb
        {
            // 初始化小于日期时序的文件
            int nyear = o->open_date / 10000;
            sis_sprintf(work_fn, 1024, "%s/%s/%d/%d.%s",
                    fpath_, fname_, nyear, o->open_date, SIS_DISK_SDB_CHAR);
            o->work_fps->main_head.index = 1;
            o->work_fps->max_page_size = 0;
            o->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
            sis_sprintf(widx_fn, 1024, "%s/%s/%d/%d.%s",
                    fpath_, fname_, nyear, o->open_date, SIS_DISK_IDX_CHAR);
        }
        break;
    case SIS_DISK_TYPE_SDB_NONE: // name/name.sdb
        {
            int nyear = o->open_date / 10000;
            sis_sprintf(work_fn, 1024, "%s/%s/%s.%s",
                    fpath_, fname_, fname_, SIS_DISK_SDB_CHAR);
            // 这里虽然没有设置压缩方式 实际传入的数据就已经按增量压缩好了
            o->work_fps->main_head.index = 1;
            o->work_fps->max_page_size = 0;
            o->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
            sis_sprintf(widx_fn, 1024, "%s/%s/%s.%s",
                    fpath_, fname_, fname_, SIS_DISK_IDX_CHAR);
        }
        break;
    default: // SIS_DISK_TYPE_LOG
        // 其他类型的文件直接传入文件名 用户可自定义类型
        {
            sis_sprintf(work_fn, 1024, "%s/%s/%d.%s", fpath_, fname_, o->open_date, SIS_DISK_LOG_CHAR);
            o->work_fps->max_page_size = 0; // LOG文件有数据直接写
            o->work_fps->max_file_size = 0; // 顺序读写的文件 不需要设置最大值
        }
        break;
    }
    // 工作文件和索引文件保持同样的随机码
    sis_str_get_random(o->work_fps->main_tail.crc, 16);
    
    sis_disk_files_init(o->work_fps, work_fn);

    if (o->work_fps->main_head.index)
    {
        // 初始化无时序的索引文件
        o->map_idxs = sis_map_list_create(sis_disk_idx_destroy);
        o->widx_fps = sis_disk_files_create();
        o->widx_fps->main_head.hid = SIS_DISK_HID_HEAD;
        o->widx_fps->main_head.style = SIS_DISK_TYPE_SDB_IDX;
        o->widx_fps->max_page_size = SIS_DISK_MAXLEN_IDXPAGE;
        o->widx_fps->max_file_size = SIS_DISK_MAXLEN_INDEX;
        o->widx_fps->main_head.wdate = o->work_fps->main_head.wdate;
        sis_disk_files_init(o->widx_fps, widx_fn);
        memmove(o->widx_fps->main_tail.crc, o->work_fps->main_tail.crc, 16);
    }
    o->status = SIS_DISK_STATUS_CLOSED;
    return 0;
}

void sis_disk_unit_destroy(s_sis_disk_ctrl *cls_)
{
    // sis_map_list_destroy(cls_->map_keys);
    // sis_map_list_destroy(cls_->map_sdbs);
    sis_disk_wcatch_destroy(cls_->wcatch);
    if (o->work_fps->main_head.index)
    {
        sis_map_list_destroy(cls_->map_idxs);
        sis_disk_files_destroy(cls_->widx_fps);
    }
    sis_disk_files_destroy(cls_->work_fps);
    sis_free(cls_);
}

void sis_disk_class_clear(s_sis_disk_ctrl *cls_)
{
    // 创建时的信息不能删除 其他信息可清理
    // sis_map_list_clear(cls_->map_keys);
    // sis_map_list_clear(cls_->map_sdbs);

    sis_disk_wcatch_clear(cls_->wcatch);

    sis_disk_files_clear(cls_->work_fps);

    sis_map_list_clear(cls_->map_idxs);
    sis_disk_files_clear(cls_->widx_fps);

    sis_disk_class_init(cls_, cls_->style, cls_->fpath, cls_->fname, cls_->open_date);
}
 
/////////////////////////////////////////////////
// pack
/////////////////////////////////////////////////

static void cb_dict_keys(void *worker_, void *key_, size_t size) 
{
    // printf("pack : %s : %s\n", __func__, (char *)key_);
    s_sis_disk_ctrl *wfile = (s_sis_disk_ctrl *)worker_; 
    sis_disk_class_write_key(wfile, key_, size);
    sis_disk_file_write_key_dict(wfile);
}

static void cb_dict_sdbs(void *worker_, void *sdb_, size_t size)  
{
    // printf("pack : %s : %s\n", __func__, (char *)sdb_);
    s_sis_disk_ctrl *wfile = (s_sis_disk_ctrl *)worker_; 
    sis_disk_class_write_sdb(wfile, sdb_, size);
    sis_disk_file_write_sdb_dict(wfile);
}

// static void cb_realdata(void *worker_, const char *key_, const char *sdb_, void *out_, size_t olen_)
// {
//     // printf("pack : %s : %s %s\n", __func__, (char *)key_, (char *)sdb_);
//     s_sis_disk_ctrl *wfile = (s_sis_disk_ctrl *)worker_; 
//     sis_disk_file_write_sdb(wfile, key_, sdb_, out_, olen_);
// } 
static void cb_realdata(void *worker_, int kidx_, int sidx_, void *out_, size_t olen_)
{
    // printf("pack : %s : %s %s\n", __func__, (char *)key_, (char *)sdb_);
    // s_sis_disk_ctrl *wfile = (s_sis_disk_ctrl *)worker_; 
    // sis_disk_file_write_sdb(wfile, key_, sdb_, out_, olen_);
} 

size_t sis_disk_file_pack(s_sis_disk_ctrl *src_, s_sis_disk_ctrl *des_)
{
    // 开始新文件
    sis_disk_file_delete(des_);
    if (!SIS_DISK_IS_SDB(src_->style) || !SIS_DISK_IS_SDB(des_->style))
    {
        return 0;
    }
    if (sis_disk_file_write_start(des_))
    {
        return 0;
    }
    if (sis_disk_file_read_start(src_))
    {
        return 0;
    }
    s_sis_disk_callback *callback = SIS_MALLOC(s_sis_disk_callback, callback);
    callback->cb_source = des_;
    callback->cb_start = NULL;
    callback->cb_dict_keys = cb_dict_keys;
    callback->cb_dict_sdbs = cb_dict_sdbs;
    callback->cb_realdata = cb_realdata;
    callback->cb_stop = NULL;

    s_sis_disk_reader *reader = sis_disk_reader_create(callback);
    sis_disk_reader_set_key(reader, "*");
    sis_disk_reader_set_sdb(reader, "*");

    reader->whole = 0; // 一次性输出
    sis_disk_file_read_sub(src_, reader);

    sis_disk_reader_destroy(reader);    
    sis_free(callback);    
    sis_disk_file_read_stop(src_);

    sis_disk_file_write_stop(des_);
    return 1;
}


/////////////////


#include "sis_disk.io.h"


///////////////////////////
//  s_sis_disk_writer
///////////////////////////
// 如果目录下已经有不同类型文件 返回错误
s_sis_disk_writer *sis_disk_writer_create(const char *path_, const char *name_, int style_)
{
    s_sis_disk_writer *o = SIS_MALLOC(s_sis_disk_writer, o);
    o->style = style_;
    o->fpath = name_ ? sis_sdsnew(path_) : sis_sdsnew("./"); 
    o->fname = name_ ? sis_sdsnew(name_) : sis_sdsnew("sisdb");
    return o;
}
void sis_disk_writer_destroy(void *writer_)
{
    s_sis_disk_writer *writer = (s_sis_disk_writer *)writer_;
    if (writer->status)
    {
        sis_disk_writer_close(writer);
    }
    sis_sdsfree(writer->fpath);
    sis_sdsfree(writer->fname);
    sis_free(writer);
}
    // case SIS_DISK_TYPE_SDB_YEAR:  // name/2000-2009.sdb
    //     {
    //         // 初始化日期时序的文件 5 年一个节点
    //         // int nyear = cls_->open_date / 10000;
    //         // int mdate  = nyear % 10;
    //         // nyear = nyear / 10 * 10;
    //         // if (mdate > 4)
    //         // {
    //         //     cls_->open_date = (nyear + 5) * 10000 +  101;
    //         //     cls_->stop_date = (nyear + 9) * 10000 + 1231;
    //         // }
    //         // else
    //         // {
    //         //     cls_->open_date = (nyear + 0) * 10000 +  101;
    //         //     cls_->stop_date = (nyear + 4) * 10000 + 1231;
    //         // }

    //         int nyear = cls_->open_date / 100000 * 10;
    //         cls_->open_date = (nyear + 0) * 10000 +  101;
    //         cls_->stop_date = (nyear + 9) * 10000 + 1231;

    //         sis_sprintf(work_fn, 1024, "%s/%s/%d-%d.%s",
    //                 cls_->fpath, cls_->fname, cls_->open_date, cls_->stop_date, SIS_DISK_SDB_CHAR);
    //         cls_->work_fps->main_head.index = 1;
    //         cls_->work_fps->max_page_size = 0;
    //         cls_->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
    //         sis_sprintf(widx_fn, 1024, "%s/%s/%d-%d.%s",
    //                 cls_->fpath, cls_->fname, cls_->open_date, cls_->stop_date, SIS_DISK_IDX_CHAR);
    //     }
    //     break;
    // case SIS_DISK_TYPE_SDB_DATE:  // name/2021/20210606.sdb
    //     {
    //         // 初始化小于日期时序的文件
    //         int nyear = cls_->open_date / 10000;
    //         sis_sprintf(work_fn, 1024, "%s/%s/%d/%d.%s",
    //                 cls_->fpath, cls_->fname, nyear, cls_->open_date, SIS_DISK_SDB_CHAR);
    //         cls_->work_fps->main_head.index = 1;
    //         cls_->work_fps->max_page_size = 0;
    //         cls_->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
    //         sis_sprintf(widx_fn, 1024, "%s/%s/%d/%d.%s",
    //                 cls_->fpath, cls_->fname, nyear, cls_->open_date, SIS_DISK_IDX_CHAR);
    //     }
    //     break;
    // case SIS_DISK_TYPE_SDB_NOTS:       // name/name.sdb
    //     {
    //         // 初始化无时序的文件
    //         sis_sprintf(work_fn, 1024, "%s/%s/%s.%s",
    //                 cls_->fpath, cls_->fname, cls_->fname, SIS_DISK_SDB_CHAR);
    //         // 数据类型复杂直接按通用压缩方式压缩
    //         cls_->work_fps->main_head.index = 1;
    //         cls_->work_fps->max_page_size = 0;
    //         cls_->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
    //         sis_sprintf(widx_fn, 1024, "%s/%s/%s.%s",
    //                 cls_->fpath, cls_->fname, cls_->fname, SIS_DISK_IDX_CHAR);
    //     }
    //     break;
int sis_disk_writer_open(s_sis_disk_writer *writer_, int idate_)
{
    if (writer_->status)
    {
        return 0;
    }
    switch (writer_->style)
    {
    case SIS_DISK_TYPE_SNO:
        writer_->wdict = sis_disk_map_create();
        writer_->wunit = sis_disk_unit_create(SIS_DISK_TYPE_SNO, writer_->fpath, writer_->fname, idate_);
        break;   
    case SIS_DISK_TYPE_SDB:
        writer_->units = sis_map_kint_create();
        writer_->units->type->vfree = sis_disk_unit_close;
        writer_->wdict = sis_disk_map_create();
        writer_->wunit = sis_disk_unit_create(SIS_DISK_TYPE_SDB_NONE, writer_->fpath, writer_->fname, idate_);
        break;   
    default: // SIS_DISK_TYPE_LOG
        writer_->wunit = sis_disk_unit_create(SIS_DISK_TYPE_LOG, writer_->fpath, writer_->fname, idate_);
        break;
    }
    if (sis_disk_unit_wopen(writer_->wunit))
    {
        sis_disk_unit_destroy(writer_->wunit);
        return 0;
    }
    writer_->status = 1;
    return 1;
}
// 关闭所有文件 重写索引
void sis_disk_writer_close(s_sis_disk_writer *writer_)
{
    if (writer_->status)
    {
        sis_disk_unit_close(writer_->wunit);
        if (writer_->units)
        {
            sis_map_kint_destroy(writer_->units);
            writer_->units = NULL;
        }
        if (writer_->wdict)
        {
            sis_disk_map_destroy(writer_->wdict);
            writer_->wdict = NULL;
        }
        writer_->status = 0;
    }
}

void sis_disk_writer_kdict_changed(s_sis_disk_writer *writer_)
{
    // 向子文件传递
    sis_disk_unit_sync_sdict(writer_->wunit, writer_->wdict->map_keys);
    // 向其他子文件传递
    if (writer_->units)
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(writer_->units);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sis_disk_ctrl *unit = (s_sis_disk_ctrl *)sis_dict_getval(de);
            sis_disk_unit_sync_kdict(unit, writer_->wdict->map_keys);   
        }
        sis_dict_iter_free(di);   
    }
}
void sis_disk_writer_sdict_changed(s_sis_disk_writer *writer_)
{
    // 向子文件传递
    sis_disk_unit_sync_sdict(writer_->wunit, writer_->wdict->map_sdbs);
    // 向其他子文件传递
    if (writer_->units)
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(writer_->units);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sis_disk_ctrl *unit = (s_sis_disk_ctrl *)sis_dict_getval(de);
            sis_disk_unit_sync_sdict(unit, writer_->wdict->map_sdbs);   
        }
        sis_dict_iter_free(di);   
    }
}
// 写入键值信息 - 可以多次写 新增的添加到末尾 仅支持 SNO SDB 
// 打开一个文件后 强制同步一下当前的KS,然后每次更新KS都同步
// 以保证后续写入数据时MAP是最新的
int sis_disk_writer_set_kdict(s_sis_disk_writer *writer_, const char *in_, size_t ilen_)
{
    if (writer_->style == SIS_DISK_TYPE_LOG)
    {
        return 0;
    }
    bool changed = false;
    s_sis_string_list *klist = sis_string_list_create();
    sis_string_list_load(klist, in_, ilen_, ",");
    int count = sis_string_list_getsize(klist);
    for (int i = 0; i < count; i++)
    {
        const char *key = sis_string_list_get(klist, i);
        if (!sis_map_list_get(writer_->wdict->map_keys, key))
        {
            sis_map_list_set(writer_->wdict->map_keys, key, sis_sdsnew(key));
            changed = true;
        }
    }
    sis_string_list_destroy(klist);

    if (changed)
    {
        sis_disk_writer_kdict_changed(writer_);
    }
    return  sis_map_list_getsize(writer_->wdict->map_keys);  
}    
// 设置表结构体 - 根据不同的时间尺度设置不同的标记 仅支持 SNO SDB
// 只传递增量和变动的DB
int sis_disk_writer_set_sdict(s_sis_disk_writer *writer_, const char *in_, size_t ilen_)
{
    if (writer_->style == SIS_DISK_TYPE_LOG || !in_ || ilen_ < 16) // {k:{fields:[[]]}}
    {
        return 0;
    }
    s_sis_json_handle *injson = sis_json_load(in_, ilen_);
    if (!injson)
    {
        return 0;
    }
    bool changed = false;
    s_sis_json_node *innode = sis_json_first_node(injson->node); 
    while (innode)
    {
        s_sis_ddb *newdb = sis_dynamic_db_create(innode);
        if (newdb)
        {
            s_sis_ddb *agodb = sis_map_list_get(writer_->map_sdbs, innode->key);
            if (!agodb || !sis_dynamic_dbinfo_same(agodb, newdb))
            {
                sis_map_list_set(writer_->map_sdbs, innode->key, newdb);
                changed = true;
            }
        }  
        innode = sis_json_next_node(innode);
    }
    sis_json_close(injson);
    if (changed)
    {
        sis_disk_writer_sdict_changed(writer_);
    }
    return  sis_map_list_getsize(writer_->map_sdbs);    
}
//////////////////////////////////////////
//   log 
//////////////////////////////////////////
// 写入数据 仅支持 LOG 不管数据多少 直接写盘 
size_t sis_disk_writer_log(s_sis_disk_writer *writer_, void *in_, size_t ilen_)
{
    if (writer_->style != SIS_DISK_TYPE_LOG) 
    {
        return 0;
    }   
    s_sis_disk_head head = {0};
    head.fin = 1;
    head.zip = 0;
    head.hid = SIS_DISK_HID_MSG_LOG;
    return  sis_disk_unit_write(writer_->wunit, &head, in_, ilen_);
}
//////////////////////////////////////////
//   sno 
//////////////////////////////////////////
static int cb_incrzip_encode(void *writer_, char *in_, size_t ilen_)
{
    s_sis_disk_writer *writer = (s_sis_disk_writer *)writer_;
    s_sis_disk_head head = {0};
    head.fin = 1;
    head.zip = SIS_DISK_ZIP_INCRZIP;
    head.hid = SIS_DISK_HID_MSG_SNO;
    sis_disk_unit_write_sno(writer->wunit, &head, in_, ilen_);
    writer_->sno_cur_size += ilen_;
    return 0;
}
// 开始写入数据 后面的数据只有符合条件才会写盘 仅支持 SNO SDB
int sis_disk_writer_start(s_sis_disk_writer *writer_, int sendzip_)
{
    if (writer_->style != SIS_DISK_TYPE_SNO) 
    {
        return -1;
    }   
    if (sendzip_ == 0)
    {
        writer_->incrzip = sis_incrzip_class_create();
        writer_->sno_max_size = 16*1024*1024;
        writer_->sno_cur_size = 0;
        sis_incrzip_set_key(writer_->incrzip, sis_map_list_getsize(writer_->map_keys));
        int count = sis_map_list_getsize(writer_->map_sdbs)
        for (int i = 0; i < count; i++)
        {
            s_sis_ddb *db = sis_map_list_geti(writer_->wdict->map_sdbs, i);
            sis_incrzip_set_sdb(writer_->incrzip, db);
        }
        sis_incrzip_compress_start(writer_->incrzip, 16384, writer_, cb_incrzip_encode);
    }
    return 0;
}
// 数据传入结束 剩余全部写盘 仅支持 SNO SDB
void sis_disk_writer_stop(s_sis_disk_writer *writer_)
{
    if (writer_->style != SIS_DISK_TYPE_SNO) 
    {
        return 0;
    } 
    if (writer_->incrzip)
    {
        sis_incrzip_compress_stop(writer_->incrzip);
        sis_incrzip_class_destroy(writer_->incrzip);
        writer_->incrzip = NULL;
    }
}
size_t sis_disk_writer_snozip(s_sis_disk_writer *writer_, void *in_, size_t ilen_)
{
    if (writer_->style != SIS_DISK_TYPE_SNO || writer_->incrzip) 
    {
        return 0;
    }   
    s_sis_disk_head head = {0};
    head.fin = 1;
    head.zip = SIS_DISK_ZIP_INCRZIP;
    head.hid = SIS_DISK_HID_MSG_SNO;
    return  sis_disk_unit_write_sno(writer_->wunit, &head, in_, ilen_);
}

// 写入数据 仅支持 SNO 直接写已经压缩好的sno数据包
int sis_disk_writer_sno(s_sis_disk_writer *writer_, const char *kname_, const char *sname_, void *in_, size_t ilen_)
{
    if (writer_->style != SIS_DISK_TYPE_SNO || !writer_->incrzip) 
    {
        return -1;
    }   
    if (writer_->sno_cur_size > writer_->sno_max_size)
    {
        writer_->sno_cur_size = 0;
        sis_incrzip_set_restart(writer_->incrzip);
    }
    int  kidx = sis_map_list_get_index(writer_->wdict->map_keys, kname_);
    int  sidx = sis_map_list_get_index(writer_->wdict->map_sdbs, sname_);
    sis_incrzip_set_step(writer_->incrzip, kidx, sidx, in_, ilen_);
    return 0;
}

size_t sis_disk_writer_sno_one(s_sis_disk_writer *writer_, const char *kname_, int style_, void *in_, size_t ilen_)
{
    if (writer_->style != SIS_DISK_TYPE_SNO) 
    {
        return -1;
    } 
    if (writer_->incrzip) 
    {
        writer_->sno_cur_size = 0;
        sis_incrzip_set_restart(writer_->incrzip);
    }
    return sis_disk_writer_sdb_one(writer_, kname_, style_, in_, ilen_);
}
size_t sis_disk_writer_sno_mul(s_sis_disk_writer *writer_, const char *kname_, int style_, s_sis_pointer_list *inlist_)
{
    if (writer_->style != SIS_DISK_TYPE_SNO) 
    {
        return -1;
    } 
    if (writer_->incrzip) 
    {
        writer_->sno_cur_size = 0;
        sis_incrzip_set_restart(writer_->incrzip);
    }
    return sis_disk_writer_sdb_mul(writer_, kname_, style_, inlist_);
}
//////////////////////////////////////////
//   sdb 
//////////////////////////////////////////

// int _disk_writer_get_date()
// {

// }
// int _disk_writer_is_year10(int start_, int stop_)
// {
//     int nyear = cls_->open_date / 100000 * 10;
//     cls_->open_date = (nyear + 0) * 10000 +  101;
//     cls_->stop_date = (nyear + 9) * 10000 + 1231;
//     int start = start_ / 10000;
// }

// size_t sis_disk_writer_sdb_year(s_sis_disk_writer *writer_, const char *kname_, s_sis_ddb *sdb_, void *in_, size_t ilen_)
// {
//     // 根据数据定位文件名 并修改units 
//     int count = ilen_ / sdb_->size; 
//     int start = sis_dynamic_db_get_time(sdb_, 0, in_, ilen_);
//     int stop = sis_dynamic_db_get_time(sdb_, count - 1, in_, ilen_);

//     int nyear = start / 100000 * 10;
//     int open_date = nyear / 100000 * 10;
//     int stop_date = nyear / 100000 * 10;
//     int index = 0;
//     while(index < count)
//     {
//         int idate = sis_dynamic_db_get_time(sdb_, index, in_, ilen_);
//         if (idate >= open_date && idate <= stop_date)
//         {
//             index++;
//             continue;
//         }

//     }
//     if (_disk_writer_is_year10(start, stop))
//     {
//         // 生成一个文件 然后一次写入数据
//     }
//     else
//     {
//         // 生成一组文件 然后 逐条写入数据
//     }
// }
size_t sis_disk_writer_sdb_date(s_sis_disk_writer *writer_, const char *kname_, s_sis_ddb *sdb_, void *in_, size_t ilen_)
{
    int count = ilen_ / sdb_->size; 
    int start = sis_dynamic_db_get_time(sdb_, 0, in_, ilen_);
    int stop = sis_dynamic_db_get_time(sdb_, count - 1, in_, ilen_);
    if (start != stop)
    {
        // 生成一组文件 然后 逐条写入数据
    } 
    else
    {
        // 生成一个文件 然后一次写入数据
    }
}

size_t sis_disk_writer_sdb_none(s_sis_disk_writer *writer_, int kidx_, int sidx_, void *in_, size_t ilen_)
{
    s_sis_disk_head head = {0};
    head.fin = 1;

    sis_memory_clear(writer_->memory);
    sis_memory_cat_ssize(writer_->memory, kidx_);
    sis_memory_cat_ssize(writer_->memory, sidx_);
    if(sis_snappy_compress(in_, ilen_, writer_->zipmem))
    {
        head.zip = SIS_DISK_ZIP_SNAPPY;
        sis_memory_cat(writer_->memory, sis_memory(writer_->zipmem), sis_memory_get_size(writer_->zipmem));
    }
    else
    {
        head.zip = SIS_DISK_ZIP_NOZIP;
        sis_memory_cat(writer_->memory, in_, ilen_);
    }
    head.hid = SIS_DISK_HID_MSG_NON;  
    return  sis_disk_unit_write(writer_->wunit, &head, sis_memory(writer_->memory), sis_memory_get_size(writer_->memory));
}

size_t _disk_writer_sdb(s_sis_disk_writer *writer_, int kidx_, int sidx_, void *in_, size_t ilen_)
{
    // 到这里ks一定存在
    s_sis_sds  key = sis_map_list_geti(writer_->wdict->map_keys, kidx_);
    s_sis_ddb *sdb = sis_map_list_geti(writer_->wdict->map_sdbs, sidx_);
    size_t osize = 0;
    int scale = sis_disk_get_sdb_scale(sdb_);
    switch (scale)
    {
    case SIS_SDB_SCALE_YEAR:
        osize = sis_disk_writer_sdb_year(writer_, kname_, sdb_, in_, ilen_);
        break;
    case SIS_SDB_SCALE_DATE:
        osize = sis_disk_writer_sdb_date(writer_, kname_, sdb_, in_, ilen_);
        break;
    default: // SIS_SDB_SCALE_NONE
        osize = sis_disk_writer_sdb_none(writer_->wunit, kname_, sdb_, in_, ilen_);
        break;
    }
    return osize;
}

// 写入标准数据 kname 如果没有就新增 sname 必须字典已经有了数据 
// 需要根据数据的时间字段 确定对应的文件
size_t sis_disk_writer_sdb(s_sis_disk_writer *writer_, const char *kname_, const char *sname_, void *in_, size_t ilen_)
{
    if (writer_->style != SIS_DISK_TYPE_SDB) 
    {
        return 0;
    }   
    int  sidx = sis_map_list_get_index(writer_->wdict->map_sdbs, sname_);
    if (sidx < 0 )
    {
        return 0;
    }
    int  kidx = sis_map_list_get_index(writer_->wdict->map_keys, kname_);
    if (kidx < 0)
    {
        writer_->kname = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(kname_));
        sis_object_incr(writer_->kname);
        writer_->kidx = sis_map_list_set(writer_->wdict->map_keys, kname_, writer_->kname);
        sis_disk_writer_kdict_changed(writer_);
    }
    writer_->sname = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(kname_));
    return _disk_writer_sdb(writer_, kidx, sidx, in_, ilen_);
}
// 按索引写入数据 kidx_ sidx_ 必须都有效
size_t sis_disk_writer_sdb_idx(s_sis_disk_writer *writer_, int kidx_, int sidx_, void *in_, size_t ilen_)
{
    if (writer_->style != SIS_DISK_TYPE_SDB) 
    {
        return 0;
    }
    if (kidx_ < 0 || kidx_ > sis_map_list_getsize(writer_->wdict->map_keys) - 1 || 
        sidx_ < 0 || sidx_ > sis_map_list_getsize(writer_->wdict->map_sdbs) - 1)
    {
        return 0;
    }
    return  _disk_writer_sdb(writer_, kidx_, sidx_, in_, ilen_);
}

size_t sis_disk_writer_sdb_one(s_sis_disk_writer *writer_, const char *kname_, int style_, void *in_, size_t ilen_)
{
    if (style_ != SIS_SDB_STYLE_BYTE && style_ != SIS_SDB_STYLE_JSON && style_ != SIS_SDB_STYLE_CHAR)
    {
        return 0;
    }
    s_sis_disk_head head = {0};
    head.fin = 1;
    sis_memory_clear(writer_->memory);
    sis_memory_cat_ssize(writer_->memory, sis_strlen(kname_));
    sis_memory_cat(writer_->memory, kname_, sis_strlen(kname_));
    sis_memory_cat_ssize(writer_->memory, style_);
    sis_memory_cat_ssize(writer_->memory, ilen_);
    sis_memory_cat(writer_->memory, in_, ilen_);
    if(sis_snappy_compress(sis_memory(writer_->memory), sis_memory_get_size(writer_->memory), writer_->zipmem))
    {
        head.zip = SIS_DISK_ZIP_SNAPPY;
        sis_memory_swap(writer_->memory, writer_->zipmem);
    }
    else
    {
        head.zip = SIS_DISK_ZIP_NOZIP;
    }
    head.hid = SIS_DISK_HID_MSG_ONE;  
    return  sis_disk_unit_write(writer_->wunit, &head, sis_memory(writer_->memory), sis_memory_get_size(writer_->memory));
}
size_t sis_disk_writer_sdb_mul(s_sis_disk_writer *writer_, const char *kname_, int style_, s_sis_pointer_list *inlist_)
{
    if (style_ != SIS_SDB_STYLE_BYTES && style_ != SIS_SDB_STYLE_JSONS && style_ != SIS_SDB_STYLE_CHARS)
    {
        return 0;
    }
    s_sis_disk_head head = {0};
    head.fin = 1;
    sis_memory_clear(writer_->memory);
    sis_memory_cat_ssize(writer_->memory, sis_strlen(kname_));
    sis_memory_cat(writer_->memory, kname_, sis_strlen(kname_));
    sis_memory_cat_ssize(writer_->memory, style_);
    sis_memory_cat_ssize(writer_->memory, inlist_->count);
    for (int i = 0; i < inlist_->count; i++)
    {
        s_sis_sds mem = (s_sis_sds)sis_pointer_list_get(inlist_, i);
        sis_memory_cat_ssize(writer_->memory, sis_sdslen(mem));
        sis_memory_cat(writer_->memory, mem, sis_sdslen(mem));
    }   
    if(sis_snappy_compress(sis_memory(writer_->memory), sis_memory_get_size(writer_->memory), writer_->zipmem))
    {
        head.zip = SIS_DISK_ZIP_SNAPPY;
        sis_memory_swap(writer_->memory, writer_->zipmem);
    }
    else
    {
        head.zip = SIS_DISK_ZIP_NOZIP;
    }
    head.hid = SIS_DISK_HID_MSG_MUL;   
    return  sis_disk_unit_write(writer_->wunit, &head, sis_memory(writer_->memory), sis_memory_get_size(writer_->memory));
}

///////////////////////////
//  s_sis_disk_reader
///////////////////////////
s_sis_disk_reader *sis_disk_reader_create(const char *path_, const char *name_, s_sis_disk_reader_cb *cb_);
void sis_disk_reader_destroy(void *);

// 打开 准备读 首先加载IDX到内存中 就知道目录下支持哪些数据了 LOG SNO SDB
int sis_disk_reader_open(s_sis_disk_reader *);
// 关闭所有文件 设置了不同订阅条件后可以重新
void sis_disk_reader_close(s_sis_disk_reader *);

// 从对应文件中获取数据 拼成完整的数据返回 只支持 SDB 单键单表
// 多表按时序输出通过该函数获取全部数据后 排序输出
s_sis_object *sis_disk_reader_get_obj(s_sis_disk_reader *, const char *kname_, const char *sname_, s_sis_msec_pair *smsec_);

// 以流的方式读取文件 从文件中一条一条发出 按时序 无时序的会最先发出 只支持 SDB 
// 可支持多个key和sdb订阅 k1,k2,k3  db1,db2,db3
// 按磁盘存储的块 所有键无时序的先发 然后有时序读取第一块 然后排序返回 依次回调 cb_realdate 直到所有数据发送完毕 
int sis_disk_reader_sub(s_sis_disk_reader *, const char *keys_, const char *sdbs_, s_sis_msec_pair *smsec_);

// 顺序读取 仅支持 LOG  通过回调的 cb_original 返回数据
int sis_disk_reader_sub_log(s_sis_disk_reader *, int idate_);

// 顺序读取 仅支持 SNO  通过回调的 cb_original 或 cb_realdate 返回数据
// 如果定义了 cb_realdate 就解压数据再返回
// 可支持多个key和sdb订阅 k1,k2,k3  db1,db2,db3
int sis_disk_reader_sub_sno(s_sis_disk_reader *, const char *keys_, const char *sdbs_, int idate_);

// 取消一个正在订阅的任务 只有处于非订阅状态下才能订阅 避免重复订阅
int sis_disk_reader_unsub(s_sis_disk_reader *);


///////////////////////////
//  s_sis_disk_control
///////////////////////////

// 不论该目录下有任何类型文件 全部删除
int sis_disk_io_clear(const char *path_, const char *name_)
{
    // 暂时不支持
    // 1.检索目录下面所有相关文件 生成文件列表
    // 2.统计文件数据大小 和磁盘空闲大小 如果空闲足够 就继续 否则返回错误
    // 3.挨个锁住文件和索引，mv文件到日期命名的safe目录
    // 4.如果有文件移动失败 就等待继续 直到全部移动完成
    // -- 不直接删除 避免误操作 数据库文件存储已经做到所有不同组文件分离 -- //
    return 0;
}


#if 0

int __nums = 0;
size_t __size = 0;

static void cb_begin1(void *src, msec_t tt)
{
    printf("%s : %llu\n", __func__, tt);
}
static void cb_key1(void *src, void *key_, size_t size) 
{
    __size += size;
    s_sis_sds info = sis_sdsnewlen((char *)key_, size);
    printf("%s %d :%s\n", __func__, (int)size, info);
    sis_sdsfree(info);
}
static void cb_sdb1(void *src, void *sdb_, size_t size)  
{
    __size += size;
    s_sis_sds info = sis_sdsnewlen((char *)sdb_, size);
    printf("%s %d : %s\n", __func__, (int)size, info);
    sis_sdsfree(info);
}

static void cb_read_stream1(void *src, const char *key_, const char *sdb_, s_sis_object *obj_)
{
    __nums++;
    __size += sis_memory_get_size((s_sis_memory *)obj_->ptr);
    // if (__nums < 2 || __nums % 1000 == 0)
    {
        printf("%s : %d %zu\n", __func__, __nums, __size);
        sis_out_binary(" ", sis_memory((s_sis_memory *)obj_->ptr), 16);
                // sis_memory_get_size((s_sis_memory *)obj_->ptr));
    }
} 

static void cb_read1(void *src, const char *key_, const char *sdb_, s_sis_object *obj_)
{
    __nums++;
    __size += sis_memory_get_size((s_sis_memory *)obj_->ptr);
    // if (__nums < 2 || __nums % 100 == 0)
    {
        printf(" %s : [%d] %zu ",__func__, __nums, __size);
        if (key_) printf(" %s ", key_);
        if (sdb_) printf(" %s ", sdb_);
        
        s_sis_disk_ctrl *rwf = (s_sis_disk_ctrl *)src;
        s_sis_disk_dict *sdb = (s_sis_disk_dict *)sis_map_list_get(rwf->sdbs, sdb_);
        if (sdb)
        {
            s_sis_disk_dict_unit *unit =  sis_disk_dict_last(sdb);
            s_sis_sds info = sis_dynamic_db_to_csv_sds(unit->db, SIS_OBJ_GET_CHAR(obj_), SIS_OBJ_GET_SIZE(obj_)); 
            printf(" %s \n", info);
            sis_sdsfree(info);
        }
        else
        {
            printf("\n");
            sis_out_binary("data:", SIS_OBJ_GET_CHAR(obj_), SIS_OBJ_GET_SIZE(obj_));
        }
        
    }
} 
static void cb_end1(void *src, msec_t tt)
{
    printf("%s : %llu\n", __func__, tt);
}
// test stream
int test_stream()
{
    // s_sis_sds s = sis_sdsempty();
    // char c = 'A';
    // s = sis_sdscatfmt(s,"%s", &c);
    // printf("%c %s\n", c, s);
    // return 0;
    sis_log_open(NULL, 10, 0);
    safe_memory_start();
    s_sis_disk_ctrl *rwf = sis_disk_class_create();
    // 先写
    sis_disk_class_init(rwf, SIS_DISK_TYPE_STREAM, "dbs", "1111.aof");

    sis_disk_file_write_start(rwf);

    int count = 1*1000*1000;
    sis_disk_class_set_size(rwf, 400*1000, 20*1000);
    // sis_disk_class_set_size(rwf, 0, 300*1000);
    for (int i = 0; i < count; i++)
    {
        sis_disk_file_write_log(rwf, "012345678987654321", 18);
    }  
    sis_disk_file_write_stop(rwf);
    // 后读
    printf("write end .\n");

    sis_disk_file_read_start(rwf);
    s_sis_disk_callback *callback = SIS_MALLOC(s_sis_disk_callback, callback);
    callback->source = rwf;
    callback->cb_begin = cb_begin1;
    callback->cb_read = cb_read_stream1;
    callback->cb_end = cb_end1;
    __nums = 0;

    s_sis_disk_reader *reader = sis_disk_reader_create(callback);
    sis_disk_file_read_sub(rwf, reader);

    sis_disk_file_read_stop(rwf);
    sis_disk_reader_destroy(reader);
    sis_free(callback);
    printf("read end __nums = %d \n", __nums);

    sis_disk_class_destroy(rwf);
    safe_memory_stop();
    sis_log_close();
    return 0;
}


char *keys = "{\"k1\",\"k3\",\"k3\"}";
// char *keys = "{\"k1\":{\"dp\":2},\"k2\":{\"dp\":2},\"k3\":{\"dp\":3}}";
char *sdbs = "{\"info\":{\"fields\":{\"name\":[\"C\",10]}},\"snap\":{\"fields\":{\"time\":[\"U\",8],\"newp\":[\"F\",8,1,2],\"vol\":[\"I\",4]}}}";
#pragma pack(push,1)
typedef struct s_info
{
    char name[10];
}s_info;
typedef struct s_snap
{
    uint64 time;
    double newp; 
    uint32 vol;
}s_snap;
#pragma pack(pop)
s_info info_data[3] = { 
    { "k10000" },
    { "k20000" },
    { "k30000" },
};
s_snap snap_data[10] = { 
    { 1000, 128.561, 3000},
    { 2000, 129.562, 4000},
    { 3000, 130.563, 5000},
    { 4000, 121.567, 6000},
    { 5000, 122.561,  7000},
    { 6000, 123.562,  8000},
    { 7000, 124.563,  9000},
    { 8000, 125.564,  9900},
    { 9000, 124.563,  9000},
    { 10000, 125.564,  9900},
};
size_t write_hq_1(s_sis_disk_ctrl *rwf)
{
    size_t size = 0;
    int count  = 1000;
    for (int k = 0; k < count; k++)
    {
        for (int i = 0; i < 8; i++)
        {
            size += sis_disk_file_write_sdb(rwf, "k1", "snap", &snap_data[i], sizeof(s_snap));
        }
        // printf("%zu\n", size);
        for (int i = 0; i < 5; i++)
        {
            size += sis_disk_file_write_sdb(rwf, "k2", "snap", &snap_data[i], sizeof(s_snap));
        }
        // printf("%zu\n", size);
        for (int i = 0; i < 7; i++)
        {
            size += sis_disk_file_write_sdb(rwf, "k3", "snap", &snap_data[i], sizeof(s_snap));
        }
    }
    return size;
}

size_t write_hq(s_sis_disk_ctrl *rwf)
{
    size_t size = 0;
    int count  = 3000;
    s_snap snap;
    int sno = 1;
    for (int k = 0; k < count; k++)
    {        
        for (int i = 0; i < 10; i++)
        {
            snap.time = sno++;
            snap.newp = 100.12 + i;
            snap.vol  = 10000*k + i;
            size += sis_disk_file_write_sdb(rwf, "k1", "snap", &snap, sizeof(s_snap));
        }
        // printf("%zu\n", size);
        for (int i = 0; i < 10; i++)
        {
            snap.time = sno++;
            snap.newp = 200.12 + i;
            snap.vol  = 20000*k + i;
            size += sis_disk_file_write_sdb(rwf, "k2", "snap", &snap, sizeof(s_snap));
        }
        // printf("%zu\n", size);
        for (int i = 0; i < 10; i++)
        {
            snap.time = sno++;
            snap.newp = 400.12 + i;
            snap.vol  = 40000*k + i;
            size += sis_disk_file_write_sdb(rwf, "k3", "snap", &snap, sizeof(s_snap));
        }
    }
    return size;
}
size_t write_after(s_sis_disk_ctrl *rwf)
{
    size_t size = 0;
    int count  = 500;
    int sno = 1;
    for (int k = 0; k < count; k++)
    {        
        for (int i = 0; i < 10; i++)
        {
            snap_data[i].time = sno++;
            snap_data[i].newp = 100.12 + i;
            snap_data[i].vol  = 10000*k + i;
        }
        size += sis_disk_file_write_sdb(rwf, "k1", "snap", &snap_data[0], 10 * sizeof(s_snap));
        // printf("%zu\n", size);
        for (int i = 0; i < 10; i++)
        {
            snap_data[i].time = sno++;
            snap_data[i].newp = 200.12 + i;
            snap_data[i].vol  = 20000*k + i;
        }
        size += sis_disk_file_write_sdb(rwf, "k2", "snap", &snap_data[0], 10 * sizeof(s_snap));
        // printf("%zu\n", size);
        for (int i = 0; i < 10; i++)
        {
            snap_data[i].time = sno++;
            snap_data[i].newp = 400.12 + i;
            snap_data[i].vol  = 40000*k + i;
        }
        size += sis_disk_file_write_sdb(rwf, "k3", "snap", &snap_data[0], 10 * sizeof(s_snap));
    }
    return size;
}
void write_log(s_sis_disk_ctrl *rwf)
{
    sis_disk_file_write_start(rwf);
    
    // 先写
    sis_disk_class_write_key(rwf, keys, sis_strlen(keys));
    sis_disk_class_write_sdb(rwf, sdbs, sis_strlen(sdbs));

    // int count = 1*1000*1000;
    sis_disk_class_set_size(rwf, 400*1000000, 20*1000);
    // sis_disk_class_set_size(rwf, 0, 300*1000);    
    size_t size = sis_disk_file_write_sdb(rwf, "k1", "info", &info_data[0], sizeof(s_info));
    size += sis_disk_file_write_sdb(rwf, "k2", "info", &info_data[1], sizeof(s_info));
    size += sis_disk_file_write_sdb(rwf, "k3", "info", &info_data[2], sizeof(s_info));
    
    size+= write_hq(rwf);

    printf("style : %d, size = %zu\n", rwf->work_fps->main_head.style, size);

    sis_disk_file_write_stop(rwf);
    
    printf("write end\n");
}

void write_sdb(s_sis_disk_ctrl *rwf, char *keys_, char *sdbs_)
{
    sis_disk_file_write_start(rwf);

    sis_disk_class_write_key(rwf, keys_, sis_strlen(keys_));
    sis_disk_class_write_sdb(rwf, sdbs_, sis_strlen(sdbs_));

    // int count = 1*1000*1000;
    sis_disk_class_set_size(rwf, 400*1000, 20*1000);
    // sis_disk_class_set_size(rwf, 0, 300*1000);    
    size_t size = sis_disk_file_write_sdb(rwf, "k1", "info", &info_data[0], sizeof(s_info));
    size += sis_disk_file_write_sdb(rwf, "k2", "info", &info_data[1], sizeof(s_info));
    size += sis_disk_file_write_sdb(rwf, "k3", "info", &info_data[2], sizeof(s_info));
    
    size+= write_after(rwf);

    printf("%zu\n", size);

    sis_disk_file_write_stop(rwf);
    
    printf("write end\n");
}
void read_of_sub(s_sis_disk_ctrl *rwf)
{
    // 后读
    sis_disk_file_read_start(rwf);
    
    s_sis_disk_callback *callback = SIS_MALLOC(s_sis_disk_callback, callback);
    callback->source = rwf;
    callback->cb_begin = cb_begin1;
    callback->cb_key = cb_key1;
    callback->cb_sdb = cb_sdb1;
    callback->cb_read = cb_read1;
    callback->cb_end = cb_end1;

    s_sis_disk_reader *reader = sis_disk_reader_create(callback);
    sis_disk_reader_set_key(reader, "*");
    // sis_disk_reader_set_key(reader, "k1");
    // sis_disk_reader_set_sdb(reader, "snap");
    sis_disk_reader_set_sdb(reader, "*");
    // sis_disk_reader_set_sdb(reader, "info");
    // sis_disk_reader_set_stime(reader, 1000, 1500);

    __nums = 0;
    // sub 是一条一条的输出
    sis_disk_file_read_sub(rwf, reader);
    // get 是所有符合条件的一次性输出
    // sis_disk_file_read_get(rwf, reader);

    sis_disk_reader_destroy(reader);
    sis_free(callback);

    sis_disk_file_read_stop(rwf);
    
    printf("read end __nums = %d \n", __nums);

}

void rewrite_sdb(s_sis_disk_ctrl *rwf)
{
    // 先写

    printf("write ----1----. begin:\n");
    sis_disk_file_write_start(rwf);

    sis_disk_class_write_key(rwf, keys, sis_strlen(keys));
    sis_disk_class_write_sdb(rwf, sdbs, sis_strlen(sdbs));

    size_t size = 0;
    size += sis_disk_file_write_sdb(rwf, "k1", "info", &info_data[0], sizeof(s_info));
    size += sis_disk_file_write_sdb(rwf, "k2", "info", &info_data[1], sizeof(s_info));
    size += sis_disk_file_write_sdb(rwf, "k3", "info", &info_data[2], sizeof(s_info));
    // 只写单键值没问题
    // 必须对已经存在的文件 加载key和sdb后设置为 iswrite 
    size += sis_disk_file_write_any(rwf, "anykey", "my is dzd.", 10);
    sis_disk_file_write_stop(rwf);

    printf("write end. [%zu], read 1:\n", size);
    read_of_sub(rwf);
    printf("write ----2----. begin:\n");

    // sis_disk_file_write_start(rwf);
    // printf("write ----2----. begin:\n");
    // size = sis_disk_file_write_any(rwf, "anykey", "my is ding.", 11);
    // printf("write ----2----. begin:\n");
    // size += sis_disk_file_write_any(rwf, "anykey1", "my is xp.", 9);
    // printf("write ----2----. begin:\n");
    // sis_disk_file_write_stop(rwf);
    // printf("write end. [%zu], read 2:\n", size);

}
void pack_sdb(s_sis_disk_ctrl *src)
{
    s_sis_disk_ctrl *des = sis_disk_class_create(SIS_DISK_TYPE_SDB_NOTS ,"dbs", "20200101");  
    sis_disk_class_init(des, SIS_DISK_TYPE_SDB_NOTS, "debug1", "db");
    callback->cb_source(src, des);
    sis_disk_class_destroy(des);
}
int main(int argc, char **argv)
{
    sis_log_open(NULL, 10, 0);
    safe_memory_start();

// test stream
    // test_stream();
// test log
    // s_sis_disk_ctrl *rwf = sis_disk_class_create(SIS_DISK_TYPE_LOG ,"dbs", "20200101");  
    // write_log(rwf);
    // read_of_sub(rwf);
    // sis_disk_class_destroy(rwf);

// test sno
    // s_sis_disk_ctrl *rwf = sis_disk_class_create();//SIS_DISK_TYPE_SNO ,"dbs", "20200101");  
    // sis_disk_class_init(rwf, SIS_DISK_TYPE_SNO, "dbs", "20200101");
    // write_log(rwf);
    // read_of_sub(rwf);
    // sis_disk_class_destroy(rwf);

// test sdb
    s_sis_disk_ctrl *rwf = sis_disk_class_create(SIS_DISK_TYPE_SDB_NOTS ,"dbs", "20200101");  
    sis_disk_class_init(rwf, SIS_DISK_TYPE_SDB_NOTS, "debug", "db");
    // 测试写入后再写出错问题
    // rewrite_sdb(rwf);
    // pack_sdb(rwf);
    // write_sdb(rwf, keys, sdbs);  // sdb
    // write_sdb(rwf, NULL, sdbs);  // kdb
    // write_sdb(rwf, keys, NULL);  // key
    // write_sdb(rwf, NULL, NULL);  // any
    read_of_sub(rwf);
    sis_disk_class_destroy(rwf);
    
    safe_memory_stop();
    sis_log_close();
    return 0;
}
#endif

