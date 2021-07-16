
#include "sis_disk.h"

int sis_disk_get_sdb_scale(s_sis_dynamic_db *db_)
{
    if (!db_->field_time)
    {
        return SIS_SDB_SCALE_NONE;
    }
    if (db_->field_time->style == SIS_DYNAMIC_TYPE_DATE)
    {
        return SIS_SDB_SCALE_YEAR;
    }
//  SIS_DYNAMIC_TYPE_WSEC   'W'  // 微秒 8  
//  SIS_DYNAMIC_TYPE_MSEC   'T'  // 毫秒 8  
//  SIS_DYNAMIC_TYPE_TSEC    'S'  // 秒   4 8  
//  SIS_DYNAMIC_TYPE_MINU   'M'  // 分钟 4 time_t/60
    return SIS_SDB_SCALE_DATE;
}

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

    o->status = 0;
    o->munit = NULL;
    o->units = sis_map_list_create(sis_disk_ctrl_destroy);;
    o->map_keys = sis_map_list_create(sis_sdsfree_call);;
    o->map_sdbs = sis_map_list_create(sis_dynamic_db_destroy);
    o->map_year = sis_map_list_create(NULL);
    o->map_date = sis_map_list_create(NULL);
    return o;
}
void sis_disk_writer_destroy(void *writer_)
{
    s_sis_disk_writer *writer = (s_sis_disk_writer *)writer_;
    if (writer->status)
    {
        sis_disk_writer_close(writer);
    }
    if (writer->munit)
    {
        sis_disk_ctrl_destroy(writer->munit);
    }
    sis_map_list_destroy(writer->units);
    sis_map_list_destroy(writer->map_year);
    sis_map_list_destroy(writer->map_date);
    sis_map_list_destroy(writer->map_sdbs);
    sis_map_list_destroy(writer->map_keys);

    sis_sdsfree(writer->fpath);
    sis_sdsfree(writer->fname);
    sis_free(writer);
}

int sis_disk_writer_open(s_sis_disk_writer *writer_, int idate_)
{
    if (writer_->status)
    {
        return 0;
    }
    switch (writer_->style)
    {
    case SIS_DISK_TYPE_SNO:
        writer_->munit = sis_disk_ctrl_create(SIS_DISK_TYPE_SNO, writer_->fpath, writer_->fname, idate_);
        break;   
    case SIS_DISK_TYPE_SDB:
        writer_->munit = sis_disk_ctrl_create(SIS_DISK_TYPE_SDB_NONE, writer_->fpath, writer_->fname, idate_);
        break;   
    default: // SIS_DISK_TYPE_LOG
        writer_->munit = sis_disk_ctrl_create(SIS_DISK_TYPE_LOG, writer_->fpath, writer_->fname, idate_);
        break;
    }
    if (sis_disk_ctrl_write_start(writer_->munit))
    {
        sis_disk_ctrl_destroy(writer_->munit);
        writer_->munit = NULL;
        return 0;
    }
    writer_->status = 1;
    return 1;
}
// 关闭所有文件 重写索引
void sis_disk_writer_close(s_sis_disk_writer *writer_)
{
    if (writer_->status && writer_->munit)
    {
        sis_disk_ctrl_write_stop(writer_->munit);
        int count = sis_map_list_getsize(writer_->units);
        for (int i = 0; i < count; i++)
        {
            s_sis_disk_ctrl *ctrl = sis_map_list_geti(writer_->units, i);
            sis_disk_ctrl_write_stop(ctrl);
        }
       writer_->status = 0;
    }
}

void sis_disk_writer_kdict_changed(s_sis_disk_writer *writer_, const char *kname_)
{
    if(!writer_->units)
    {
        return ;
    }
    // 向子文件传递
    int count = sis_map_list_getsize(writer_->units);
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_ctrl *unit = (s_sis_disk_ctrl *)sis_map_list_geti(writer_->units, i);
        s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(unit->map_kdicts, kname_);
        if (!kdict)
        {
            sis_disk_ctrl_add_kdict(unit, kname_);
        }
    }
}
void sis_disk_writer_sdict_changed(s_sis_disk_writer *writer_, const char *sname_, s_sis_dynamic_db *db_)
{
    if(!writer_->units)
    {
        return ;
    }
    // 向其他子文件传递
    int count = sis_map_list_getsize(writer_->units);
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_ctrl *unit = (s_sis_disk_ctrl *)sis_map_list_geti(writer_->units, i);
        s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(unit->map_sdicts, sname_);
        if (!sdict)
        {
            sis_disk_ctrl_add_sdict(unit, sname_, db_);
        }
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
    s_sis_string_list *klist = sis_string_list_create();
    sis_string_list_load(klist, in_, ilen_, ",");
    int count = sis_string_list_getsize(klist);
    for (int i = 0; i < count; i++)
    {
        const char *key = sis_string_list_get(klist, i);
        s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(writer_->munit->map_kdicts, key);
        if (!kdict)
        {
            sis_disk_ctrl_add_kdict(writer_->munit, key);
            // 向打开的其他文件传递 
            sis_disk_writer_kdict_changed(writer_, key);
        }
    }
    sis_string_list_destroy(klist);
    return  sis_map_list_getsize(writer_->munit->map_kdicts);  
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
    s_sis_json_node *innode = sis_json_first_node(injson->node); 
    while (innode)
    {
        s_sis_dynamic_db *newdb = sis_dynamic_db_create(innode);
        if (newdb)
        {
            s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(writer_->munit->map_sdicts, innode->key);
            if (!sdict)
            {
                sis_disk_ctrl_add_sdict(writer_->munit, innode->key, newdb);
                // 向打开的其他文件传递 
                sis_disk_writer_sdict_changed(writer_, innode->key, newdb);
            }
            else 
            {
                s_sis_dynamic_db *agodb = sis_pointer_list_get(sdict->sdbs, sdict->sdbs->count - 1);
                if (!sis_dynamic_dbinfo_same(agodb, newdb))
                {
                    sis_pointer_list_push(sdict->sdbs, newdb);
                    sis_disk_writer_sdict_changed(writer_, innode->key, newdb);
                }
                else
                {
                    sis_dynamic_db_destroy(newdb);
                }
            }
        }  
        innode = sis_json_next_node(innode);
    }
    sis_json_close(injson);
    return  sis_map_list_getsize(writer_->munit->map_sdicts);    
}
//////////////////////////////////////////
//   log 
//////////////////////////////////////////
// 写入数据 仅支持 LOG 不管数据多少 直接写盘 
size_t sis_disk_writer_log(s_sis_disk_writer *writer_, void *in_, size_t ilen_)
{
    return  sis_disk_io_write_log(writer_->munit, in_, ilen_);
}
//////////////////////////////////////////
//   sno 
//////////////////////////////////////////

// 开始写入数据 后面的数据只有符合条件才会写盘 仅支持 SNO SDB
int sis_disk_writer_start(s_sis_disk_writer *writer_)
{
    if (writer_->style != SIS_DISK_TYPE_SNO) 
    {
        return -1;
    }   
    return sis_disk_io_write_sno_start(writer_->munit);
}
// 数据传入结束 剩余全部写盘 仅支持 SNO SDB
void sis_disk_writer_stop(s_sis_disk_writer *writer_)
{
    if (writer_->style != SIS_DISK_TYPE_SNO) 
    {
        return;
    } 
    sis_disk_io_write_sno_stop(writer_->munit);
}

// 写入数据 仅支持 SNO 
int sis_disk_writer_sno(s_sis_disk_writer *writer_, const char *kname_, const char *sname_, void *in_, size_t ilen_)
{
    if (writer_->style != SIS_DISK_TYPE_SNO) 
    {
        return -1;
    }   
    s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(writer_->munit->map_sdicts, sname_);
    if (!sdict)
    {
        return 0;
    }
    s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(writer_->munit->map_kdicts, kname_);
    if (!kdict)
    {
        kdict = sis_disk_ctrl_add_kdict(writer_->munit, kname_);
    }
    return sis_disk_io_write_sno(writer_->munit, kdict, sdict, in_, ilen_);
}

//////////////////////////////////////////
//   sdb 
//////////////////////////////////////////
int _disk_writer_is_year10(int open_, int stop_)
{
    int open_year = open_ / 100000 * 10;
    int stop_year = stop_ / 100000 * 10;
    if (open_year == stop_year)
    {
        return 1;
    }
    return  0;
}
s_sis_disk_ctrl *_disk_writer_get_year(s_sis_disk_writer *writer_, int idate_)
{
    int idate = idate_ / 100000 * 10;
    char sdate[32];
    sis_llutoa(idate, sdate, 32, 10);
    s_sis_disk_ctrl *ctrl = sis_map_list_get(writer_->units, sdate);
    if (!ctrl)
    {
        ctrl = sis_disk_ctrl_create(SIS_DISK_TYPE_SDB_YEAR, writer_->fpath, writer_->fname, idate);
        if (sis_disk_ctrl_write_start(ctrl))
        {
            sis_disk_ctrl_destroy(ctrl);
            return NULL;
        }
        sis_map_list_set(writer_->units, sdate, ctrl);
    }
    return ctrl;
}
s_sis_disk_ctrl *_disk_writer_get_date(s_sis_disk_writer *writer_, int idate_)
{
    char sdate[32];
    sis_llutoa(idate_, sdate, 32, 10);
    s_sis_disk_ctrl *ctrl = sis_map_list_get(writer_->units, sdate);
    if (!ctrl)
    {
        ctrl = sis_disk_ctrl_create(SIS_DISK_TYPE_SDB_DATE, writer_->fpath, writer_->fname, idate_);
        if (sis_disk_ctrl_write_start(ctrl))
        {
            sis_disk_ctrl_destroy(ctrl);
            return NULL;
        }
        sis_map_list_set(writer_->units, sdate, ctrl);
    }
    return ctrl;
}
size_t sis_disk_writer_sdb_year(s_sis_disk_writer *writer_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, void *in_, size_t ilen_)
{
    size_t osize = 0;
    // 根据数据定位文件名 并修改units 
    s_sis_dynamic_db *sdb = sis_pointer_list_get(sdict_->sdbs, sdict_->sdbs->count - 1);
    int count = ilen_ / sdb->size; 
    s_sis_disk_ctrl *ctrl = NULL;
    int start = sis_dynamic_db_get_time(sdb, 0, in_, ilen_);
    start = sis_time_unit_convert(sdb->field_time->style, SIS_DYNAMIC_TYPE_DATE, start);
    int stop = sis_dynamic_db_get_time(sdb, count - 1, in_, ilen_);
    stop = sis_time_unit_convert(sdb->field_time->style, SIS_DYNAMIC_TYPE_DATE, stop);
    if (_disk_writer_is_year10(start, stop))
    {
        ctrl = _disk_writer_get_year(writer_, start);
        if (ctrl)
        {
            s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(ctrl->map_kdicts, SIS_OBJ_SDS(kdict_->name));
            s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(ctrl->map_kdicts, SIS_OBJ_SDS(sdict_->name));
            osize += sis_disk_io_write_sdb(ctrl, kdict, sdict, in_, ilen_);
        }
    }
    else
    {
        int nowday = start;
        int nowidx = 0;
        int nowrec = 1;
        for (int i = 1; i < count; i++)
        {
            int curr = sis_dynamic_db_get_time(sdb, i, in_, ilen_);
            curr = sis_time_unit_convert(sdb->field_time->style, SIS_DYNAMIC_TYPE_DATE, curr);
            if (!_disk_writer_is_year10(curr, nowday))
            {
                ctrl = _disk_writer_get_year(writer_, nowday);
                // write 
                if (ctrl)
                {
                    s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(ctrl->map_kdicts, SIS_OBJ_SDS(kdict_->name));
                    s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(ctrl->map_kdicts, SIS_OBJ_SDS(sdict_->name));
                    osize += sis_disk_io_write_sdb(ctrl, kdict, sdict, ((char *)in_) + nowidx * sdb->size, nowrec * sdb->size);
                }
                nowrec = 1;
                nowday = curr;
                nowidx = i;
            }
            else
            {
                nowrec++;
            }
        }
        ctrl = _disk_writer_get_year(writer_, nowday);
        // write
        if (ctrl)
        {
            s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(ctrl->map_kdicts, SIS_OBJ_SDS(kdict_->name));
            s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(ctrl->map_kdicts, SIS_OBJ_SDS(sdict_->name));
            osize += sis_disk_io_write_sdb(ctrl, kdict, sdict, ((char *)in_) + nowidx * sdb->size, nowrec * sdb->size);
        }
    }
    return osize;
}
size_t sis_disk_writer_sdb_date(s_sis_disk_writer *writer_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, void *in_, size_t ilen_)
{
    size_t osize = 0;
    // 根据数据定位文件名 并修改units 
    s_sis_dynamic_db *sdb = sis_pointer_list_get(sdict_->sdbs, sdict_->sdbs->count - 1);
    int count = ilen_ / sdb->size; 
    s_sis_disk_ctrl *ctrl = NULL;
    int start = sis_dynamic_db_get_time(sdb, 0, in_, ilen_);
    start = sis_time_unit_convert(sdb->field_time->style, SIS_DYNAMIC_TYPE_DATE, start);
    int stop = sis_dynamic_db_get_time(sdb, count - 1, in_, ilen_);
    stop = sis_time_unit_convert(sdb->field_time->style, SIS_DYNAMIC_TYPE_DATE, stop);
    if (start ==  stop)
    {
        ctrl = _disk_writer_get_date(writer_, start);
        if (ctrl)
        {
            s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(ctrl->map_kdicts, SIS_OBJ_SDS(kdict_->name));
            s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(ctrl->map_kdicts, SIS_OBJ_SDS(sdict_->name));
            osize += sis_disk_io_write_sdb(ctrl, kdict, sdict, in_, ilen_);
        }
    }
    else
    {
        int nowday = start;
        int nowidx = 0;
        int nowrec = 1;
        for (int i = 1; i < count; i++)
        {
            int curr = sis_dynamic_db_get_time(sdb, i, in_, ilen_);
            curr = sis_time_unit_convert(sdb->field_time->style, SIS_DYNAMIC_TYPE_DATE, curr);
            if (nowday != curr)
            {
                ctrl = _disk_writer_get_date(writer_, nowday);
                // write 
                if (ctrl)
                {
                    s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(ctrl->map_kdicts, SIS_OBJ_SDS(kdict_->name));
                    s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(ctrl->map_kdicts, SIS_OBJ_SDS(sdict_->name));
                    osize += sis_disk_io_write_sdb(ctrl, kdict, sdict, ((char *)in_) + nowidx * sdb->size, nowrec * sdb->size);
                }
                nowrec = 1;
                nowday = curr;
                nowidx = i;
            }
            else
            {
                nowrec++;
            }
        }
        ctrl = _disk_writer_get_date(writer_, nowday);
        // write
        if (ctrl)
        {
            s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(ctrl->map_kdicts, SIS_OBJ_SDS(kdict_->name));
            s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(ctrl->map_kdicts, SIS_OBJ_SDS(sdict_->name));
            osize += sis_disk_io_write_sdb(ctrl, kdict, sdict, ((char *)in_) + nowidx * sdb->size, nowrec * sdb->size);
        }
    }
    return osize;
}

// 写入标准数据 kname 如果没有就新增 sname 必须字典已经有了数据 
// 需要根据数据的时间字段 确定对应的文件
size_t sis_disk_writer_sdb(s_sis_disk_writer *writer_, const char *kname_, const char *sname_, void *in_, size_t ilen_)
{
    if (writer_->style != SIS_DISK_TYPE_SDB)
    //  || !kname_ || !sname_ || !in_ || ilen_ == 0) 
    {
        return 0;
    }   
    s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(writer_->munit->map_sdicts, sname_);
    if (!sdict)
    {
        return 0;
    }
    s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(writer_->munit->map_kdicts, kname_);
    if (!kdict)
    {
        kdict = sis_disk_ctrl_add_kdict(writer_->munit, kname_);
        // 向所有打开的子类发送变化
        sis_disk_writer_kdict_changed(writer_, kname_);
    }
    s_sis_dynamic_db *sdb = sis_pointer_list_get(sdict->sdbs, sdict->sdbs->count - 1);

    size_t osize = 0;
    int scale = sis_disk_get_sdb_scale(sdb);
    switch (scale)
    {
    case SIS_SDB_SCALE_YEAR:
        osize = sis_disk_writer_sdb_year(writer_, kdict, sdict, in_, ilen_);
        break;
    case SIS_SDB_SCALE_DATE:
        osize = sis_disk_writer_sdb_date(writer_, kdict, sdict, in_, ilen_);
        break;
    default: // SIS_SDB_SCALE_NONE
        osize = sis_disk_io_write_non(writer_->munit, kdict, sdict, in_, ilen_);
        break;
    }
    return osize;
}
// // 按索引写入数据 kidx_ sidx_ 必须都有效
// size_t sis_disk_writer_sdb_idx(s_sis_disk_writer *writer_, int kidx_, int sidx_, void *in_, size_t ilen_)
// {
//     if (writer_->style != SIS_DISK_TYPE_SDB) 
//     {
//         return 0;
//     }
//     if (kidx_ < 0 || kidx_ > sis_map_list_getsize(writer_->wdict->map_keys) - 1 || 
//         sidx_ < 0 || sidx_ > sis_map_list_getsize(writer_->wdict->map_sdbs) - 1)
//     {
//         return 0;
//     }
//     return  _disk_writer_sdb(writer_, kidx_, sidx_, in_, ilen_);
// }

size_t sis_disk_writer_sdb_one(s_sis_disk_writer *writer_, const char *kname_, void *in_, size_t ilen_)
{
    if (writer_->style != SIS_DISK_TYPE_SDB)
    {
        return 0;
    }
    s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(writer_->munit->map_kdicts, kname_);
    return  sis_disk_io_write_one(writer_->munit, kdict, in_, ilen_);
}
size_t sis_disk_writer_mul(s_sis_disk_writer *writer_, const char *kname_, s_sis_pointer_list *inlist_)
{
    if (writer_->style != SIS_DISK_TYPE_SDB)
    {
        return 0;
    }
    s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(writer_->munit->map_kdicts, kname_);
    return  sis_disk_io_write_mul(writer_->munit, kdict, inlist_);
}

// ///////////////////////////
// //  s_sis_disk_reader
// ///////////////////////////
// s_sis_disk_reader *sis_disk_reader_create(const char *path_, const char *name_, s_sis_disk_reader_cb *cb_);
// void sis_disk_reader_destroy(void *);

// // 打开 准备读 首先加载IDX到内存中 就知道目录下支持哪些数据了 LOG SNO SDB
// int sis_disk_reader_open(s_sis_disk_reader *);
// // 关闭所有文件 设置了不同订阅条件后可以重新
// void sis_disk_reader_close(s_sis_disk_reader *);

// // 从对应文件中获取数据 拼成完整的数据返回 只支持 SDB 单键单表
// // 多表按时序输出通过该函数获取全部数据后 排序输出
// s_sis_object *sis_disk_reader_get_obj(s_sis_disk_reader *, const char *kname_, const char *sname_, s_sis_msec_pair *smsec_);

// // 以流的方式读取文件 从文件中一条一条发出 按时序 无时序的会最先发出 只支持 SDB 
// // 可支持多个key和sdb订阅 k1,k2,k3  db1,db2,db3
// // 按磁盘存储的块 所有键无时序的先发 然后有时序读取第一块 然后排序返回 依次回调 cb_realdate 直到所有数据发送完毕 
// int sis_disk_reader_sub(s_sis_disk_reader *, const char *keys_, const char *sdbs_, s_sis_msec_pair *smsec_);

// // 顺序读取 仅支持 LOG  通过回调的 cb_original 返回数据
// int sis_disk_reader_sub_log(s_sis_disk_reader *, int idate_);

// // 顺序读取 仅支持 SNO  通过回调的 cb_original 或 cb_realdate 返回数据
// // 如果定义了 cb_realdate 就解压数据再返回
// // 可支持多个key和sdb订阅 k1,k2,k3  db1,db2,db3
// int sis_disk_reader_sub_sno(s_sis_disk_reader *, const char *keys_, const char *sdbs_, int idate_);

// // 取消一个正在订阅的任务 只有处于非订阅状态下才能订阅 避免重复订阅
// int sis_disk_reader_unsub(s_sis_disk_reader *);


// ///////////////////////////
// //  s_sis_disk_reader
// ///////////////////////////
// s_sis_disk_reader *sis_disk_reader_create(s_sis_disk_callback *cb_)
// {
//     s_sis_disk_reader *o = SIS_MALLOC(s_sis_disk_reader, o);
//     o->callback = cb_;
//     o->issub = 1;
//     o->whole = 0;
//     o->memory = sis_memory_create();
//     return o;
// }
// void sis_disk_reader_destroy(void *cls_)
// {
//     s_sis_disk_reader *info = (s_sis_disk_reader *)cls_;
//     sis_sdsfree(cls_->sub_keys);
//     sis_sdsfree(cls_->sub_sdbs);
//     sis_memory_destroy(info->memory);
//     sis_free(info);
// }
// void sis_disk_reader_init(s_sis_disk_reader *cls_)
// {
//     cls_->rhead.fin = 1;
//     cls_->rhead.hid = 0;
//     cls_->rhead.zip = 0;
//     cls_->rinfo     = NULL;
//     sis_memory_clear(cls_->memory);
// }
// void sis_disk_reader_clear(s_sis_disk_reader *cls_)
// {
//     sis_sdsfree(cls_->sub_keys);
//     cls_->sub_keys = NULL;
//     sis_sdsfree(cls_->sub_sdbs);
//     cls_->sub_sdbs = NULL;

//     cls_->issub = 1;
//     cls_->whole = 0;
    
//     cls_->search_msec.start = 0;
//     cls_->search_msec.stop  = 0;

//     sis_disk_reader_init(cls_);
// }
// void sis_disk_reader_set_key(s_sis_disk_reader *cls_, const char *in_)
// {
//     sis_sdsfree(cls_->sub_keys);
//     cls_->sub_keys = sis_sdsnew(in_);
// }
// void sis_disk_reader_set_sdb(s_sis_disk_reader *cls_, const char *in_)
// {
//     sis_sdsfree(cls_->sdbs);
//     cls_->sdbs = sis_sdsnew(in_);
// }

// void sis_disk_reader_set_stime(s_sis_disk_reader *cls_, msec_t start_, msec_t stop_)
// {
//     cls_->search_msec.start = start_;
//     cls_->search_msec.stop = stop_;
// }

// int sis_reader_sub_filters(s_sis_disk_ctrl *cls_, s_sis_disk_reader *reader_, s_sis_pointer_list *list_)
// {
//     if (!reader_ || !reader_->issub || !list_ || !reader_->keys || sis_sdslen(reader_->keys) < 1)
//     {
//         return 0;
//     }
//     sis_pointer_list_clear(list_);
//     if(!sis_strcasecmp(reader_->keys, "*") && !sis_strcasecmp(reader_->sdbs, "*"))
//     {
//         // 如果是全部订阅
//         for (int k = 0; k < sis_map_list_getsize(cls_->map_idxs); k++)
//         {
//             s_sis_disk_idx *node = (s_sis_disk_idx *)sis_map_list_geti(cls_->map_idxs, k);
//             if (node)
//             {
//                 sis_pointer_list_push(list_, node);
//             }
//         }  
//         return list_->count;     
//     }

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
//                 // printf("%s %s\n", info, reader_->sdbs);            
//                 s_sis_disk_idx *node = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, info);
//                 if (node)
//                 {
//                     // printf("%s : %s\n",node? SIS_OBJ_SDS(node->key) : "nil", info);
//                     sis_pointer_list_push(list_, node);
//                 }
//             }
//         }
//         else
//         {

//             s_sis_disk_idx *node = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, sis_string_list_get(keys, i));
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



// ///////////////////////////
// //  tools
// ///////////////////////////


// s_sis_disk_idx *sis_disk_idx_get(s_sis_map_list *map_, s_sis_object *kname_, s_sis_object *sname_)
// {
//     if (!kname_||!sname_)
//     {
//         return NULL;
//     }
//     s_sis_sds key = sis_sdsnew(SIS_OBJ_SDS(kname_));
//     key = sis_sdscatfmt(key, ".%s", SIS_OBJ_SDS(sname_));
//     LOG(0)("merge : %s\n", key);
//     s_sis_disk_idx *node = (s_sis_disk_idx *)sis_map_list_get(map_, key);
//     if (!node)
//     {
//         node = sis_disk_idx_create(key_, sdb_);
//         sis_map_list_set(map_, key, node);
//     }
//     sis_sdsfree(key);
//     return node;
// }


// void sis_disk_ctrl_move(s_sis_disk_ctrl *cls_, const char *path_)
// {
//     char fn[255];
//     char newfn[255];
//     for (int  i = 0; i < cls_->work_fps->lists->count; i++)
//     {
//         s_sis_disk_ctrls_unit *unit = (s_sis_disk_ctrls_unit *)sis_pointer_list_get(cls_->work_fps->lists, i);
//         sis_file_getname(unit->fn, fn, 255);
//         sis_sprintf(newfn, 255, "%s/%s", path_, fn);
//         sis_file_rename(unit->fn, newfn);
//     }
//     if (cls_->work_fps->main_head.index)
//     {
//         for (int  i = 0; i < cls_->widx_fps->lists->count; i++)
//         {
//             s_sis_disk_ctrls_unit *unit = (s_sis_disk_ctrls_unit *)sis_pointer_list_get(cls_->widx_fps->lists, i);
//             sis_file_getname(unit->fn, fn, 255);
//             sis_sprintf(newfn, 255, "%s/%s", path_, fn);
//             sis_file_rename(unit->fn, newfn);
//         }
//     }
// }
// int sis_disk_ctrl_valid_widx(s_sis_disk_ctrl *cls_)
// {
//     if (cls_->work_fps->main_head.index)
//     {
//         if (!sis_file_exists(cls_->widx_fps->cur_name))
//         {
//             LOG(5)
//             ("idxfile no exists.[%s]\n", cls_->widx_fps->cur_name);
//             return SIS_DISK_CMD_NO_EXISTS_IDX;
//         }
//         return SIS_DISK_CMD_OK;
//     }
//     return SIS_DISK_CMD_NO_IDX;
// }

// // 检查文件是否有效
// int sis_disk_ctrl_valid_work(s_sis_disk_ctrl *cls_)
// {
//     // 通常判断work和index的尾部是否一样 一样表示完整 否则
//     // 检查work file 是否完整 如果不完整就设置最后一个块的位置 并重建索引
//     // 如果work file 完整 就检查索引文件是否完整 不完整就重建索引
//     // 重建失败就返回 1
//     return SIS_DISK_CMD_OK;
// }



// ////////////////////
// //  write
// ///////////////////

// // 到这里的写入 已经保证了数据在当前时间范围 不用再去判断时间区域问题 
// size_t sis_disk_ctrl_write_sdbi(s_sis_disk_ctrl *cls_, int keyi_, int sdbi_, void *in_, size_t ilen_)
// {
//     if (!in_ || !ilen_)
//     {
//         return 0;
//     }
//     int size = 0;
//     s_sis_disk_dict *key = (s_sis_disk_dict *)sis_map_list_geti(cls_->keys, keyi_);
//     s_sis_disk_dict *sdb = (s_sis_disk_dict *)sis_map_list_geti(cls_->sdbs, sdbi_);
//     if (key && sdb)
//     {
//         switch (cls_->work_fps->main_head.style)
//         {
//         case SIS_DISK_TYPE_SNO:
//             size = sis_disk_ctrl_write_sdb_sno(cls_, key_, sdb_, in_, ilen_);
//             break;
//         case SIS_DISK_TYPE_SDB_YEAR:
//         case SIS_DISK_TYPE_SDB_DATE:
//             size = sis_disk_ctrl_write_sdb_tsno(cls_, key_, sdb_, in_, ilen_);
//             break;
//         case SIS_DISK_TYPE_SDB_NOTS:
//             size = sis_disk_ctrl_write_sdb_nots(cls_, key_, sdb_, in_, ilen_);
//             break;
//         default: // 其他结构不支持
//             break;
//         }
//     } 
//     return size;    
// }

// // newkeys
// s_sis_sds sis_disk_ctrl_get_keys(s_sis_disk_ctrl *cls_, bool onlyincr_)
// {
//     s_sis_sds msg = sis_sdsempty();
//     int nums = 0;
//     {
//         int count = sis_map_list_getsize(cls_->keys);
//         for(int i = 0; i < count; i++)
//         {
//             s_sis_disk_dict *info = (s_sis_disk_dict *)sis_map_list_geti(cls_->keys, i);
//             for(int k = 0; k < info->units->count; k++)
//             {
//                 s_sis_disk_dict_unit *unit = sis_disk_dict_get(info, k);
//                 // LOG(1)("w writed = %s %d\n", SIS_OBJ_GET_CHAR(info->name), unit->writed);
//                 if (onlyincr_ && unit->writed)
//                 {
//                     continue;
//                 }
//                 if (nums > 0)
//                 {
//                     msg = sis_sdscatfmt(msg, ",%s", SIS_OBJ_SDS(info->name));
//                 }
//                 else
//                 {
//                     msg = sis_sdscatfmt(msg, "%s", SIS_OBJ_SDS(info->name));
//                 }
//                 nums++;
//                 if (onlyincr_) 
//                 {
//                     unit->writed = 1;
//                 }      
//             }
//         }
//     }
//     return msg;
// }
// size_t sis_disk_ctrl_write_key_dict(s_sis_disk_ctrl *cls_)
// {
//     size_t size = 0;
//     // 写 键表
//     s_sis_sds msg = sis_disk_ctrl_get_keys(cls_, true);
//     if (sis_sdslen(msg) > 2)
//     {
//         s_sis_object *mapobj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(SIS_DISK_SIGN_KEY));
//         s_sis_disk_wcatch *wcatch = sis_disk_wcatch_create(mapobj, NULL);
//         sis_memory_cat(wcatch->memory, msg, sis_sdslen(msg));
//         wcatch->head.hid = SIS_DISK_HID_DICT_KEY;
//         size = sis_disk_write_work(cls_, wcatch);
//         sis_disk_wcatch_destroy(wcatch);
//         sis_object_destroy(mapobj);
//     }
//     sis_sdsfree(msg);
//     return size;
// }
// s_sis_sds sis_disk_ctrl_get_sdbs(s_sis_disk_ctrl *cls_, bool onlyincr_)
// {
//     int count = sis_map_list_getsize(cls_->sdbs);
//     s_sis_json_node *sdbs_node = sis_json_create_object();
//     {
//         for(int i = 0; i < count; i++)
//         {
//             s_sis_disk_dict *info = (s_sis_disk_dict *)sis_map_list_geti(cls_->sdbs, i);
//             // printf("+++++ %d %d %s\n",count, info->units->count, SIS_OBJ_SDS(info->name));
//             for(int k = 0; k < info->units->count; k++)
//             {
//                 s_sis_disk_dict_unit *unit = sis_disk_dict_get(info, k);
//                 // LOG(1)("w writed = %s %d %d\n", SIS_OBJ_GET_CHAR(info->name), unit->writed, unit->db->size);
//                 if (onlyincr_ && unit->writed)
//                 {
//                     continue;
//                 }
//                 if (onlyincr_) 
//                 {
//                     unit->writed = 1;
//                 }
//                 sis_json_object_add_node(sdbs_node, SIS_OBJ_SDS(info->name), sis_dynamic_dbinfo_to_json(unit->db));
//             }
//         }
//     }
//     s_sis_sds msg = sis_json_to_sds(sdbs_node, true);
//     sis_json_delete_node(sdbs_node);
//     return msg;
// }

// size_t sis_disk_ctrl_write_sdb_dict(s_sis_disk_ctrl *cls_)
// {
//     size_t size = 0;
//     s_sis_sds msg = sis_disk_ctrl_get_sdbs(cls_, true);
//     printf("new sdb:%s\n", msg);
//     if (sis_sdslen(msg) > 2)
//     {
//         s_sis_object *mapobj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(SIS_DISK_SIGN_SDB));
//         s_sis_disk_wcatch *wcatch = sis_disk_wcatch_create(mapobj, NULL);
//         sis_memory_cat(wcatch->memory, msg, sis_sdslen(msg));
//         wcatch->head.hid = SIS_DISK_HID_DICT_SDB;
//         size = sis_disk_write_work(cls_, wcatch);
//         sis_disk_wcatch_destroy(wcatch);
//         sis_object_destroy(mapobj);
//     }
//     sis_sdsfree(msg);
//     return size;
// }


// #if 0

// int __nums = 0;
// size_t __size = 0;

// static void cb_begin1(void *src, msec_t tt)
// {
//     printf("%s : %llu\n", __func__, tt);
// }
// static void cb_key1(void *src, void *key_, size_t size) 
// {
//     __size += size;
//     s_sis_sds info = sis_sdsnewlen((char *)key_, size);
//     printf("%s %d :%s\n", __func__, (int)size, info);
//     sis_sdsfree(info);
// }
// static void cb_sdb1(void *src, void *sdb_, size_t size)  
// {
//     __size += size;
//     s_sis_sds info = sis_sdsnewlen((char *)sdb_, size);
//     printf("%s %d : %s\n", __func__, (int)size, info);
//     sis_sdsfree(info);
// }

// static void cb_read_stream1(void *src, const char *key_, const char *sdb_, s_sis_object *obj_)
// {
//     __nums++;
//     __size += sis_memory_get_size((s_sis_memory *)obj_->ptr);
//     // if (__nums < 2 || __nums % 1000 == 0)
//     {
//         printf("%s : %d %zu\n", __func__, __nums, __size);
//         sis_out_binary(" ", sis_memory((s_sis_memory *)obj_->ptr), 16);
//                 // sis_memory_get_size((s_sis_memory *)obj_->ptr));
//     }
// } 

// static void cb_read1(void *src, const char *key_, const char *sdb_, s_sis_object *obj_)
// {
//     __nums++;
//     __size += sis_memory_get_size((s_sis_memory *)obj_->ptr);
//     // if (__nums < 2 || __nums % 100 == 0)
//     {
//         printf(" %s : [%d] %zu ",__func__, __nums, __size);
//         if (key_) printf(" %s ", key_);
//         if (sdb_) printf(" %s ", sdb_);
        
//         s_sis_disk_ctrl *rwf = (s_sis_disk_ctrl *)src;
//         s_sis_disk_dict *sdb = (s_sis_disk_dict *)sis_map_list_get(rwf->sdbs, sdb_);
//         if (sdb)
//         {
//             s_sis_disk_dict_unit *unit =  sis_disk_dict_last(sdb);
//             s_sis_sds info = sis_dynamic_db_to_csv_sds(unit->db, SIS_OBJ_GET_CHAR(obj_), SIS_OBJ_GET_SIZE(obj_)); 
//             printf(" %s \n", info);
//             sis_sdsfree(info);
//         }
//         else
//         {
//             printf("\n");
//             sis_out_binary("data:", SIS_OBJ_GET_CHAR(obj_), SIS_OBJ_GET_SIZE(obj_));
//         }
        
//     }
// } 
// static void cb_end1(void *src, msec_t tt)
// {
//     printf("%s : %llu\n", __func__, tt);
// }
// // test stream
// int test_stream()
// {
//     // s_sis_sds s = sis_sdsempty();
//     // char c = 'A';
//     // s = sis_sdscatfmt(s,"%s", &c);
//     // printf("%c %s\n", c, s);
//     // return 0;
//     sis_log_open(NULL, 10, 0);
//     safe_memory_start();
//     s_sis_disk_ctrl *rwf = sis_disk_ctrl_create();
//     // 先写
//     sis_disk_class_init(rwf, SIS_DISK_TYPE_STREAM, "dbs", "1111.aof");

//     sis_disk_ctrl_write_start(rwf);

//     int count = 1*1000*1000;
//     sis_disk_class_set_size(rwf, 400*1000, 20*1000);
//     // sis_disk_class_set_size(rwf, 0, 300*1000);
//     for (int i = 0; i < count; i++)
//     {
//         sis_disk_ctrl_write_log(rwf, "012345678987654321", 18);
//     }  
//     sis_disk_ctrl_write_stop(rwf);
//     // 后读
//     printf("write end .\n");

//     sis_disk_ctrl_read_start(rwf);
//     s_sis_disk_callback *callback = SIS_MALLOC(s_sis_disk_callback, callback);
//     callback->source = rwf;
//     callback->cb_begin = cb_begin1;
//     callback->cb_read = cb_read_stream1;
//     callback->cb_end = cb_end1;
//     __nums = 0;

//     s_sis_disk_reader *reader = sis_disk_reader_create(callback);
//     sis_disk_ctrl_read_sub(rwf, reader);

//     sis_disk_ctrl_read_stop(rwf);
//     sis_disk_reader_destroy(reader);
//     sis_free(callback);
//     printf("read end __nums = %d \n", __nums);

//     sis_disk_ctrl_destroy(rwf);
//     safe_memory_stop();
//     sis_log_close();
//     return 0;
// }


// char *keys = "{\"k1\",\"k3\",\"k3\"}";
// // char *keys = "{\"k1\":{\"dp\":2},\"k2\":{\"dp\":2},\"k3\":{\"dp\":3}}";
// char *sdbs = "{\"info\":{\"fields\":{\"name\":[\"C\",10]}},\"snap\":{\"fields\":{\"time\":[\"U\",8],\"newp\":[\"F\",8,1,2],\"vol\":[\"I\",4]}}}";
// #pragma pack(push,1)
// typedef struct s_info
// {
//     char name[10];
// }s_info;
// typedef struct s_snap
// {
//     uint64 time;
//     double newp; 
//     uint32 vol;
// }s_snap;
// #pragma pack(pop)
// s_info info_data[3] = { 
//     { "k10000" },
//     { "k20000" },
//     { "k30000" },
// };
// s_snap snap_data[10] = { 
//     { 1000, 128.561, 3000},
//     { 2000, 129.562, 4000},
//     { 3000, 130.563, 5000},
//     { 4000, 121.567, 6000},
//     { 5000, 122.561,  7000},
//     { 6000, 123.562,  8000},
//     { 7000, 124.563,  9000},
//     { 8000, 125.564,  9900},
//     { 9000, 124.563,  9000},
//     { 10000, 125.564,  9900},
// };
// size_t write_hq_1(s_sis_disk_ctrl *rwf)
// {
//     size_t size = 0;
//     int count  = 1000;
//     for (int k = 0; k < count; k++)
//     {
//         for (int i = 0; i < 8; i++)
//         {
//             size += sis_disk_ctrl_write_sdb(rwf, "k1", "snap", &snap_data[i], sizeof(s_snap));
//         }
//         // printf("%zu\n", size);
//         for (int i = 0; i < 5; i++)
//         {
//             size += sis_disk_ctrl_write_sdb(rwf, "k2", "snap", &snap_data[i], sizeof(s_snap));
//         }
//         // printf("%zu\n", size);
//         for (int i = 0; i < 7; i++)
//         {
//             size += sis_disk_ctrl_write_sdb(rwf, "k3", "snap", &snap_data[i], sizeof(s_snap));
//         }
//     }
//     return size;
// }

// size_t write_hq(s_sis_disk_ctrl *rwf)
// {
//     size_t size = 0;
//     int count  = 3000;
//     s_snap snap;
//     int sno = 1;
//     for (int k = 0; k < count; k++)
//     {        
//         for (int i = 0; i < 10; i++)
//         {
//             snap.time = sno++;
//             snap.newp = 100.12 + i;
//             snap.vol  = 10000*k + i;
//             size += sis_disk_ctrl_write_sdb(rwf, "k1", "snap", &snap, sizeof(s_snap));
//         }
//         // printf("%zu\n", size);
//         for (int i = 0; i < 10; i++)
//         {
//             snap.time = sno++;
//             snap.newp = 200.12 + i;
//             snap.vol  = 20000*k + i;
//             size += sis_disk_ctrl_write_sdb(rwf, "k2", "snap", &snap, sizeof(s_snap));
//         }
//         // printf("%zu\n", size);
//         for (int i = 0; i < 10; i++)
//         {
//             snap.time = sno++;
//             snap.newp = 400.12 + i;
//             snap.vol  = 40000*k + i;
//             size += sis_disk_ctrl_write_sdb(rwf, "k3", "snap", &snap, sizeof(s_snap));
//         }
//     }
//     return size;
// }
// size_t write_after(s_sis_disk_ctrl *rwf)
// {
//     size_t size = 0;
//     int count  = 500;
//     int sno = 1;
//     for (int k = 0; k < count; k++)
//     {        
//         for (int i = 0; i < 10; i++)
//         {
//             snap_data[i].time = sno++;
//             snap_data[i].newp = 100.12 + i;
//             snap_data[i].vol  = 10000*k + i;
//         }
//         size += sis_disk_ctrl_write_sdb(rwf, "k1", "snap", &snap_data[0], 10 * sizeof(s_snap));
//         // printf("%zu\n", size);
//         for (int i = 0; i < 10; i++)
//         {
//             snap_data[i].time = sno++;
//             snap_data[i].newp = 200.12 + i;
//             snap_data[i].vol  = 20000*k + i;
//         }
//         size += sis_disk_ctrl_write_sdb(rwf, "k2", "snap", &snap_data[0], 10 * sizeof(s_snap));
//         // printf("%zu\n", size);
//         for (int i = 0; i < 10; i++)
//         {
//             snap_data[i].time = sno++;
//             snap_data[i].newp = 400.12 + i;
//             snap_data[i].vol  = 40000*k + i;
//         }
//         size += sis_disk_ctrl_write_sdb(rwf, "k3", "snap", &snap_data[0], 10 * sizeof(s_snap));
//     }
//     return size;
// }
// void write_log(s_sis_disk_ctrl *rwf)
// {
//     sis_disk_ctrl_write_start(rwf);
    
//     // 先写
//     sis_disk_class_write_key(rwf, keys, sis_strlen(keys));
//     sis_disk_class_write_sdb(rwf, sdbs, sis_strlen(sdbs));

//     // int count = 1*1000*1000;
//     sis_disk_class_set_size(rwf, 400*1000000, 20*1000);
//     // sis_disk_class_set_size(rwf, 0, 300*1000);    
//     size_t size = sis_disk_ctrl_write_sdb(rwf, "k1", "info", &info_data[0], sizeof(s_info));
//     size += sis_disk_ctrl_write_sdb(rwf, "k2", "info", &info_data[1], sizeof(s_info));
//     size += sis_disk_ctrl_write_sdb(rwf, "k3", "info", &info_data[2], sizeof(s_info));
    
//     size+= write_hq(rwf);

//     printf("style : %d, size = %zu\n", rwf->work_fps->main_head.style, size);

//     sis_disk_ctrl_write_stop(rwf);
    
//     printf("write end\n");
// }

// void write_sdb(s_sis_disk_ctrl *rwf, char *keys_, char *sdbs_)
// {
//     sis_disk_ctrl_write_start(rwf);

//     sis_disk_class_write_key(rwf, keys_, sis_strlen(keys_));
//     sis_disk_class_write_sdb(rwf, sdbs_, sis_strlen(sdbs_));

//     // int count = 1*1000*1000;
//     sis_disk_class_set_size(rwf, 400*1000, 20*1000);
//     // sis_disk_class_set_size(rwf, 0, 300*1000);    
//     size_t size = sis_disk_ctrl_write_sdb(rwf, "k1", "info", &info_data[0], sizeof(s_info));
//     size += sis_disk_ctrl_write_sdb(rwf, "k2", "info", &info_data[1], sizeof(s_info));
//     size += sis_disk_ctrl_write_sdb(rwf, "k3", "info", &info_data[2], sizeof(s_info));
    
//     size+= write_after(rwf);

//     printf("%zu\n", size);

//     sis_disk_ctrl_write_stop(rwf);
    
//     printf("write end\n");
// }
// void read_of_sub(s_sis_disk_ctrl *rwf)
// {
//     // 后读
//     sis_disk_ctrl_read_start(rwf);
    
//     s_sis_disk_callback *callback = SIS_MALLOC(s_sis_disk_callback, callback);
//     callback->source = rwf;
//     callback->cb_begin = cb_begin1;
//     callback->cb_key = cb_key1;
//     callback->cb_sdb = cb_sdb1;
//     callback->cb_read = cb_read1;
//     callback->cb_end = cb_end1;

//     s_sis_disk_reader *reader = sis_disk_reader_create(callback);
//     sis_disk_reader_set_key(reader, "*");
//     // sis_disk_reader_set_key(reader, "k1");
//     // sis_disk_reader_set_sdb(reader, "snap");
//     sis_disk_reader_set_sdb(reader, "*");
//     // sis_disk_reader_set_sdb(reader, "info");
//     // sis_disk_reader_set_stime(reader, 1000, 1500);

//     __nums = 0;
//     // sub 是一条一条的输出
//     sis_disk_ctrl_read_sub(rwf, reader);
//     // get 是所有符合条件的一次性输出
//     // sis_disk_ctrl_read_get(rwf, reader);

//     sis_disk_reader_destroy(reader);
//     sis_free(callback);

//     sis_disk_ctrl_read_stop(rwf);
    
//     printf("read end __nums = %d \n", __nums);

// }

// void rewrite_sdb(s_sis_disk_ctrl *rwf)
// {
//     // 先写

//     printf("write ----1----. begin:\n");
//     sis_disk_ctrl_write_start(rwf);

//     sis_disk_class_write_key(rwf, keys, sis_strlen(keys));
//     sis_disk_class_write_sdb(rwf, sdbs, sis_strlen(sdbs));

//     size_t size = 0;
//     size += sis_disk_ctrl_write_sdb(rwf, "k1", "info", &info_data[0], sizeof(s_info));
//     size += sis_disk_ctrl_write_sdb(rwf, "k2", "info", &info_data[1], sizeof(s_info));
//     size += sis_disk_ctrl_write_sdb(rwf, "k3", "info", &info_data[2], sizeof(s_info));
//     // 只写单键值没问题
//     // 必须对已经存在的文件 加载key和sdb后设置为 iswrite 
//     size += sis_disk_ctrl_write_any(rwf, "anykey", "my is dzd.", 10);
//     sis_disk_ctrl_write_stop(rwf);

//     printf("write end. [%zu], read 1:\n", size);
//     read_of_sub(rwf);
//     printf("write ----2----. begin:\n");

//     // sis_disk_ctrl_write_start(rwf);
//     // printf("write ----2----. begin:\n");
//     // size = sis_disk_ctrl_write_any(rwf, "anykey", "my is ding.", 11);
//     // printf("write ----2----. begin:\n");
//     // size += sis_disk_ctrl_write_any(rwf, "anykey1", "my is xp.", 9);
//     // printf("write ----2----. begin:\n");
//     // sis_disk_ctrl_write_stop(rwf);
//     // printf("write end. [%zu], read 2:\n", size);

// }
// void pack_sdb(s_sis_disk_ctrl *src)
// {
//     s_sis_disk_ctrl *des = sis_disk_ctrl_create(SIS_DISK_TYPE_SDB_NOTS ,"dbs", "20200101");  
//     sis_disk_class_init(des, SIS_DISK_TYPE_SDB_NOTS, "debug1", "db");
//     callback->cb_source(src, des);
//     sis_disk_ctrl_destroy(des);
// }
// int main(int argc, char **argv)
// {
//     sis_log_open(NULL, 10, 0);
//     safe_memory_start();

// // test stream
//     // test_stream();
// // test log
//     // s_sis_disk_ctrl *rwf = sis_disk_ctrl_create(SIS_DISK_TYPE_LOG ,"dbs", "20200101");  
//     // write_log(rwf);
//     // read_of_sub(rwf);
//     // sis_disk_ctrl_destroy(rwf);

// // test sno
//     // s_sis_disk_ctrl *rwf = sis_disk_ctrl_create();//SIS_DISK_TYPE_SNO ,"dbs", "20200101");  
//     // sis_disk_class_init(rwf, SIS_DISK_TYPE_SNO, "dbs", "20200101");
//     // write_log(rwf);
//     // read_of_sub(rwf);
//     // sis_disk_ctrl_destroy(rwf);

// // test sdb
//     s_sis_disk_ctrl *rwf = sis_disk_ctrl_create(SIS_DISK_TYPE_SDB_NOTS ,"dbs", "20200101");  
//     sis_disk_class_init(rwf, SIS_DISK_TYPE_SDB_NOTS, "debug", "db");
//     // 测试写入后再写出错问题
//     // rewrite_sdb(rwf);
//     // pack_sdb(rwf);
//     // write_sdb(rwf, keys, sdbs);  // sdb
//     // write_sdb(rwf, NULL, sdbs);  // kdb
//     // write_sdb(rwf, keys, NULL);  // key
//     // write_sdb(rwf, NULL, NULL);  // any
//     read_of_sub(rwf);
//     sis_disk_ctrl_destroy(rwf);
    
//     safe_memory_stop();
//     sis_log_close();
//     return 0;
// }
// #endif



///////////////////////////
//  s_sis_disk_control
///////////////////////////

// 不论该目录下有任何类型文件 全部删除
int sis_disk_control_clear(const char *path_, const char *name_)
{
    // 暂时不支持
    // 1.检索目录下面所有相关文件 生成文件列表
    // 2.统计文件数据大小 和磁盘空闲大小 如果空闲足够 就继续 否则返回错误
    // 3.挨个锁住文件和索引，mv文件到日期命名的safe目录
    // 4.如果有文件移动失败 就等待继续 直到全部移动完成
    // -- 不直接删除 避免误操作 数据库文件存储已经做到所有不同组文件分离 -- //
    return 0;
}
