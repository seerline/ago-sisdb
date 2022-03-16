
#include "sis_disk.h"

///////////////////////////
//  s_sis_disk_writer
///////////////////////////
// 如果目录下已经有不同类型文件 返回错误
s_sis_disk_writer *sis_disk_writer_create(const char *path_, const char *name_, int style_)
{
    s_sis_disk_writer *o = SIS_MALLOC(s_sis_disk_writer, o);
    o->style = style_;
    o->fpath = path_ ? sis_sdsnew(path_) : sis_sdsnew("./"); 
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
    case SIS_DISK_TYPE_SIC:
        writer_->munit = sis_disk_ctrl_create(SIS_DISK_TYPE_SIC, writer_->fpath, writer_->fname, idate_);
        break;   
    case SIS_DISK_TYPE_SNO:
        writer_->munit = sis_disk_ctrl_create(SIS_DISK_TYPE_SNO, writer_->fpath, writer_->fname, idate_);
        break;   
    case SIS_DISK_TYPE_SDB:
        writer_->munit = sis_disk_ctrl_create(SIS_DISK_TYPE_SDB, writer_->fpath, writer_->fname, idate_);
        break;   
    default: // SIS_DISK_TYPE_LOG
        writer_->munit = sis_disk_ctrl_create(SIS_DISK_TYPE_LOG, writer_->fpath, writer_->fname, idate_);
        break;
    }
    if (sis_disk_ctrl_write_start(writer_->munit) < 0)
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
        if (writer_->style == SIS_DISK_TYPE_SDB)
        {
            // printf("%d %d\n", writer_->munit->new_kinfos->count, writer_->munit->new_sinfos->count);
            sis_disk_ctrl_write_kdict(writer_->munit);
            sis_disk_ctrl_write_sdict(writer_->munit);
        }
        sis_disk_ctrl_write_stop(writer_->munit);
        int count = sis_map_list_getsize(writer_->units);
        for (int i = 0; i < count; i++)
        {
            s_sis_disk_ctrl *ctrl = sis_map_list_geti(writer_->units, i);
            sis_disk_ctrl_write_stop(ctrl);
        }
    }
    writer_->status = 0;
}

// 所有基础类和子类文件key保持最新一致 但仍然 有可能老的子类文件不一致
void sis_disk_writer_kdict_changed(s_sis_disk_writer *writer_, const char *kname_)
{
    // 向打开的子文件传递
    int count = sis_map_list_getsize(writer_->units);
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_ctrl *unit = (s_sis_disk_ctrl *)sis_map_list_geti(writer_->units, i);
        sis_disk_ctrl_set_kdict(unit, kname_);
    }
}
// 基础类为结构全集，子类文件只保留当前时序的结构说明
void sis_disk_writer_sdict_changed(s_sis_disk_writer *writer_, s_sis_dynamic_db *db_)
{
    int count = sis_map_list_getsize(writer_->units);
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_ctrl *unit = (s_sis_disk_ctrl *)sis_map_list_geti(writer_->units, i);
        int scale = sis_disk_get_sdb_scale(db_);
        if (scale == SIS_SDB_SCALE_YEAR)
        {
            if (unit->work_fps->main_head.style != SIS_DISK_TYPE_SDB_YEAR)
            {
                continue;
            }
        }
        if (scale == SIS_SDB_SCALE_DATE)
        {
            if (unit->work_fps->main_head.style != SIS_DISK_TYPE_SDB_DATE)
            {
                continue;
            }
        }
        if (scale == SIS_SDB_SCALE_NOTS)
        {
            if (unit->work_fps->main_head.style != SIS_DISK_TYPE_SDB_NOTS)
            {
                continue;
            }
        }
        sis_disk_ctrl_set_sdict(unit, db_);
    }
}

// 写入键值信息 - 可以多次写 新增的添加到末尾 仅支持 SNO SDB NET
// 打开一个文件后 强制同步一下当前的KS,然后每次更新KS都同步
// 以保证后续写入数据时MAP是最新的
int sis_disk_writer_set_kdict(s_sis_disk_writer *writer_, const char *in_, size_t ilen_)
{
    if (writer_->status == 0 || writer_->style == SIS_DISK_TYPE_LOG || !in_ || ilen_ < 1)
    {
        return 0;
    }
    s_sis_string_list *klist = sis_string_list_create();
    sis_string_list_load(klist, in_, ilen_, ",");
    int count = sis_string_list_getsize(klist);
    for (int i = 0; i < count; i++)
    {
        const char *key = sis_string_list_get(klist, i);
        sis_disk_ctrl_set_kdict(writer_->munit, key);
        sis_disk_writer_kdict_changed(writer_, key);
    }
    sis_string_list_destroy(klist);
    // 写盘
    {
        sis_disk_ctrl_write_kdict(writer_->munit);
        int count = sis_map_list_getsize(writer_->units);
        for (int i = 0; i < count; i++)
        {
            s_sis_disk_ctrl *unit = (s_sis_disk_ctrl *)sis_map_list_geti(writer_->units, i);
            sis_disk_ctrl_write_kdict(unit);
        }
    }
    return  sis_map_list_getsize(writer_->munit->map_kdicts);  
}    
// 设置表结构体 - 根据不同的时间尺度设置不同的标记 仅支持 SNO SDB NET
// 只传递增量和变动的DB
int sis_disk_writer_set_sdict(s_sis_disk_writer *writer_, const char *in_, size_t ilen_)
{
    if (writer_->status == 0 || writer_->style == SIS_DISK_TYPE_LOG || !in_ || ilen_ < 16) // {k:{fields:[[]]}}
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
            sis_disk_ctrl_set_sdict(writer_->munit, newdb);
            sis_disk_writer_sdict_changed(writer_, newdb);
            sis_dynamic_db_decr(newdb);
        }  
        innode = sis_json_next_node(innode);
    }
    sis_json_close(injson);
    {
        sis_disk_ctrl_write_sdict(writer_->munit);
        int count = sis_map_list_getsize(writer_->units);
        for (int i = 0; i < count; i++)
        {
            s_sis_disk_ctrl *unit = (s_sis_disk_ctrl *)sis_map_list_geti(writer_->units, i);
            sis_disk_ctrl_write_sdict(unit);
        }
    }
    return  sis_map_list_getsize(writer_->munit->map_sdicts);    
}
//////////////////////////////////////////
//   log 
//////////////////////////////////////////
// 写入数据 仅支持 LOG 不管数据多少 直接写盘 
size_t sis_disk_writer_log(s_sis_disk_writer *writer_, void *in_, size_t ilen_)
{
    // if (!writer_->munit || !in_ || ilen_ < 1)
    // {
    //     return 0;
    // }
    return  sis_disk_io_write_log(writer_->munit, in_, ilen_);
}
//////////////////////////////////////////
//   net sno
//////////////////////////////////////////

// 开始写入数据 后面的数据只有符合条件才会写盘 仅支持 SNO SDB NET
int sis_disk_writer_start(s_sis_disk_writer *writer_)
{
    int o = -1;
    switch (writer_->style)
    {
    case SIS_DISK_TYPE_SIC:
        sis_disk_ctrl_write_kdict(writer_->munit);
        sis_disk_ctrl_write_sdict(writer_->munit);
        o = sis_disk_io_write_sic_start(writer_->munit);
        break;
    case SIS_DISK_TYPE_SNO:
        sis_disk_ctrl_write_kdict(writer_->munit);
        sis_disk_ctrl_write_sdict(writer_->munit);
        o = sis_disk_io_write_sno_start(writer_->munit);
        break;
    case SIS_DISK_TYPE_SDB:
        break;
    default:
        break;
    }
    return o;
}
// 数据传入结束 剩余全部写盘 仅支持 SNO SDB NET
void sis_disk_writer_stop(s_sis_disk_writer *writer_)
{
    switch (writer_->style)
    {
    case SIS_DISK_TYPE_SIC:
        sis_disk_io_write_sic_stop(writer_->munit);
        break;
    case SIS_DISK_TYPE_SNO:
        sis_disk_io_write_sno_stop(writer_->munit);
        break;
    case SIS_DISK_TYPE_SDB:
        break;
    default:
        break;
    } 
}
// int __tempnums = 0;
// 写入数据 仅支持 NET 
int sis_disk_writer_sic(s_sis_disk_writer *writer_, const char *kname_, const char *sname_, void *in_, size_t ilen_)
{
    if (writer_->style != SIS_DISK_TYPE_SIC) 
    {
        return -1;
    }  
    // __tempnums++;
    // if (__tempnums % 1000 == 0)
    // {
    // printf("%d %d %d\n", __tempnums, sis_map_list_getsize(writer_->munit->map_sdicts), sis_map_list_getsize(writer_->munit->map_kdicts));
    // } 
    s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(writer_->munit->map_sdicts, sname_);
    if (!sdict)
    {
        return 0;
    }
    s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(writer_->munit->map_kdicts, kname_);
    if (!kdict)
    {
        LOG(8)("new key: %s\n", kname_);
        kdict = sis_disk_ctrl_set_kdict(writer_->munit, kname_);
        sis_disk_ctrl_write_kdict(writer_->munit);
        // 对于net新增代码后 必须要好好处理key增长的问题
        sis_incrzip_compress_addkey(writer_->munit->net_incrzip, 1);
    }
    return sis_disk_io_write_sic(writer_->munit, kdict, sdict, in_, ilen_);
}

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
        LOG(8)("new key: %s\n", kname_);
        kdict = sis_disk_ctrl_set_kdict(writer_->munit, kname_);
        sis_disk_ctrl_write_kdict(writer_->munit);
    }
    return sis_disk_io_write_sno(writer_->munit, kdict, sdict, in_, ilen_);
}
//////////////////////////////////////////
//   sdb 
//////////////////////////////////////////
// int _disk_writer_is_year10(int open_, int stop_)
// {
//     int open_year = open_ / 100000 * 10;
//     int stop_year = stop_ / 100000 * 10;
//     if (open_year == stop_year)
//     {
//         return 1;
//     }
//     return  0;
// }
int _disk_writer_is_year(int open_, int stop_)
{
    int open_year = open_ / 10000;
    int stop_year = stop_ / 10000;
    if (open_year == stop_year)
    {
        return 1;
    }
    return  0;
}
s_sis_disk_ctrl *_disk_writer_get_year(s_sis_disk_writer *writer_, int idate_)
{
    // int nyear = idate_ / 100000 * 10;
    int nyear = idate_ / 10000;
    char syear[32];
    sis_llutoa(nyear, syear, 32, 10);
    s_sis_disk_ctrl *ctrl = sis_map_list_get(writer_->units, syear);
    if (!ctrl)
    {
        ctrl = sis_disk_ctrl_create(SIS_DISK_TYPE_SDB_YEAR, writer_->fpath, writer_->fname, idate_);
        if (sis_disk_ctrl_write_start(ctrl))
        {
            sis_disk_ctrl_destroy(ctrl);
            return NULL;
        }
        sis_map_list_set(writer_->units, syear, ctrl);
    }
    if (ctrl)
    {
        // 这里要检查 字典表是否一致 不一致就要立即更新并写盘
        sis_disk_ctrl_cmp_kdict(writer_->munit, ctrl);
        sis_disk_ctrl_cmp_sdict(writer_->munit, ctrl);
        // 先写可能增加的字典信息
        sis_disk_ctrl_write_kdict(ctrl);
        sis_disk_ctrl_write_sdict(ctrl);
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
    if (ctrl)
    {
        // 这里要检查 字典表是否一致 不一致就要立即更新并写盘
        sis_disk_ctrl_cmp_kdict(writer_->munit, ctrl);
        sis_disk_ctrl_cmp_sdict(writer_->munit, ctrl);
        // 先写可能增加的字典信息
        sis_disk_ctrl_write_kdict(ctrl);
        sis_disk_ctrl_write_sdict(ctrl);
    }
    return ctrl;
}
s_sis_disk_ctrl *_disk_writer_get_nots(s_sis_disk_writer *writer_)
{
    s_sis_disk_ctrl *ctrl = sis_map_list_get(writer_->units, SIS_DISK_NOTS_CHAR);
    if (!ctrl)
    {
        ctrl = sis_disk_ctrl_create(SIS_DISK_TYPE_SDB_NOTS, writer_->fpath, writer_->fname, 0);
        if (sis_disk_ctrl_write_start(ctrl))
        {
            sis_disk_ctrl_destroy(ctrl);
            return NULL;
        }
        sis_map_list_set(writer_->units, SIS_DISK_NOTS_CHAR, ctrl);
    }
    if (ctrl)
    {
        // 这里要检查 字典表是否一致 不一致就要立即更新并写盘
        sis_disk_ctrl_cmp_kdict(writer_->munit, ctrl);
        sis_disk_ctrl_cmp_sdict(writer_->munit, ctrl);
        // 先写可能增加的字典信息
        sis_disk_ctrl_write_kdict(ctrl);
        sis_disk_ctrl_write_sdict(ctrl);
    }
    return ctrl;
}
static size_t _disk_writer_sunit(s_sis_disk_ctrl *ctrl, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, void *in_, size_t ilen_)
{
    s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(ctrl->map_kdicts, SIS_OBJ_SDS(kdict_->name));
    s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(ctrl->map_sdicts, SIS_OBJ_SDS(sdict_->name));
    return sis_disk_io_write_sdb(ctrl, kdict, sdict, in_, ilen_);
}
size_t sis_disk_writer_sdb_year(s_sis_disk_writer *writer_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, void *in_, size_t ilen_)
{
    size_t osize = 0;
    // 根据数据定位文件名 并修改 units 
    s_sis_dynamic_db *sdb = sis_pointer_list_get(sdict_->sdbs, sdict_->sdbs->count - 1);
    int count = ilen_ / sdb->size; 
    s_sis_disk_ctrl *ctrl = NULL;
    msec_t start = sis_dynamic_db_get_time(sdb, 0, in_, ilen_);
    start = sis_time_unit_convert(sdb->field_time->style, SIS_DYNAMIC_TYPE_DATE, start);
    msec_t stop = sis_dynamic_db_get_time(sdb, count - 1, in_, ilen_);
    stop = sis_time_unit_convert(sdb->field_time->style, SIS_DYNAMIC_TYPE_DATE, stop);
    if (_disk_writer_is_year(start, stop))
    {
        ctrl = _disk_writer_get_year(writer_, start);
        if (ctrl)
        {
            // 得到实际的字典
            osize += _disk_writer_sunit(ctrl, kdict_, sdict_, in_, ilen_);
            sis_disk_io_write_map(writer_->munit, kdict_, sdict_, SIS_SDB_STYLE_SDB, start / 10000, 0);
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
            if (!_disk_writer_is_year(curr, nowday))
            {
                ctrl = _disk_writer_get_year(writer_, nowday);
                // write 
                if (ctrl)
                {
                    osize += _disk_writer_sunit(ctrl, kdict_, sdict_, ((char *)in_) + nowidx * sdb->size, nowrec * sdb->size);
                    sis_disk_io_write_map(writer_->munit, kdict_, sdict_, SIS_SDB_STYLE_SDB, nowday / 10000, 0);
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
            osize += _disk_writer_sunit(ctrl, kdict_, sdict_, ((char *)in_) + nowidx * sdb->size, nowrec * sdb->size);
            sis_disk_io_write_map(writer_->munit, kdict_, sdict_, SIS_SDB_STYLE_SDB, nowday / 10000, 0);
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
    msec_t start = sis_dynamic_db_get_time(sdb, 0, in_, ilen_);
    start = sis_time_unit_convert(sdb->field_time->style, SIS_DYNAMIC_TYPE_DATE, start);
    msec_t stop = sis_dynamic_db_get_time(sdb, count - 1, in_, ilen_);
    stop = sis_time_unit_convert(sdb->field_time->style, SIS_DYNAMIC_TYPE_DATE, stop);
    if (start ==  stop)
    {
        ctrl = _disk_writer_get_date(writer_, start);
        if (ctrl)
        {
            osize += _disk_writer_sunit(ctrl, kdict_, sdict_, in_, ilen_);
            sis_disk_io_write_map(writer_->munit, kdict_, sdict_, SIS_SDB_STYLE_SDB, start, 0);
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
                    osize += _disk_writer_sunit(ctrl, kdict_, sdict_, ((char *)in_) + nowidx * sdb->size, nowrec * sdb->size);
                    sis_disk_io_write_map(writer_->munit, kdict_, sdict_, SIS_SDB_STYLE_SDB, nowday, 0);
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
            osize += _disk_writer_sunit(ctrl, kdict_, sdict_, ((char *)in_) + nowidx * sdb->size, nowrec * sdb->size);
            sis_disk_io_write_map(writer_->munit, kdict_, sdict_, SIS_SDB_STYLE_SDB, nowday, 0);
        }
    }
    return osize;
}

size_t sis_disk_writer_sdb_nots(s_sis_disk_writer *writer_, s_sis_disk_kdict *kdict_, s_sis_disk_sdict *sdict_, void *in_, size_t ilen_)
{
    size_t osize = 0;
    s_sis_disk_ctrl *ctrl = _disk_writer_get_nots(writer_);
    if (ctrl)
    {
        s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(ctrl->map_sdicts, SIS_OBJ_SDS(sdict_->name));
        s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(ctrl->map_kdicts, SIS_OBJ_SDS(kdict_->name));

        osize += sis_disk_io_write_non(ctrl, kdict, sdict, in_, ilen_);
        printf("sis_disk_writer_sdb_nots:  %s %s | %zu\n", 
                kdict ? SIS_OBJ_SDS(kdict->name) : "nil", 
                sdict ? SIS_OBJ_SDS(sdict->name) : "nil",
                osize);
        // s_sis_dynamic_db *sdb = sis_pointer_list_get(sdict_->sdbs, sdict_->sdbs->count - 1);
        // int count = ilen_ / sdb->size;
        sis_disk_io_write_map(writer_->munit, kdict_, sdict_, SIS_SDB_STYLE_NON, 0, 0);
    }
    return osize;
}

// 写入标准数据 kname 如果没有就新增 sname 必须字典已经有了数据 
// 需要根据数据的时间字段 确定对应的文件 并自动分离不同时间段的数据
size_t sis_disk_writer_sdb(s_sis_disk_writer *writer_, const char *kname_, const char *sname_, void *in_, size_t ilen_)
{
    if (writer_->style != SIS_DISK_TYPE_SDB)
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
        kdict = sis_disk_ctrl_set_kdict(writer_->munit, kname_);
        sis_disk_ctrl_write_kdict(writer_->munit);
        sis_disk_writer_kdict_changed(writer_, kname_);
    }
    // 对主文件来说只有一条记录
    s_sis_dynamic_db *sdb = sis_pointer_list_get(sdict->sdbs, sdict->sdbs->count - 1);

    size_t osize = 0;
    int scale = sis_disk_get_sdb_scale(sdb);
    printf("sis_disk_writer_sdb : %d scale = %d\n", writer_->style, scale);
    switch (scale)
    {
    case SIS_SDB_SCALE_YEAR:
        osize = sis_disk_writer_sdb_year(writer_, kdict, sdict, in_, ilen_);
        break;
    case SIS_SDB_SCALE_DATE:
        osize = sis_disk_writer_sdb_date(writer_, kdict, sdict, in_, ilen_);
        break;
    default: // SIS_SDB_SCALE_NOTS
        osize = sis_disk_writer_sdb_nots(writer_, kdict, sdict, in_, ilen_);
        break;
    }
    return osize;
}

size_t sis_disk_writer_one(s_sis_disk_writer *writer_, const char *kname_, void *in_, size_t ilen_)
{
    if (writer_->style != SIS_DISK_TYPE_SDB)
    {
        return 0;
    }
    s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(writer_->munit->map_kdicts, kname_);
    if (!kdict)
    {
        kdict = sis_disk_ctrl_set_kdict(writer_->munit, kname_);
        sis_disk_ctrl_write_kdict(writer_->munit);
    }
    size_t osize = 0;
    s_sis_disk_ctrl *ctrl = _disk_writer_get_nots(writer_);
    if (ctrl)
    {
        s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(ctrl->map_kdicts, kname_);
        osize += sis_disk_io_write_one(ctrl, kdict, in_, ilen_);
    }
    if (osize > 0)
    {
        sis_disk_io_write_map(writer_->munit, kdict, NULL, SIS_SDB_STYLE_ONE, 0, 0);
    }
    return osize;
}
size_t sis_disk_writer_mul(s_sis_disk_writer *writer_, const char *kname_, s_sis_pointer_list *inlist_)
{
    if (writer_->style != SIS_DISK_TYPE_SDB)
    {
        return 0;
    }
    s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(writer_->munit->map_kdicts, kname_);
    if (!kdict)
    {
        kdict = sis_disk_ctrl_set_kdict(writer_->munit, kname_);
        sis_disk_ctrl_write_kdict(writer_->munit);
    }
    size_t osize = 0;
    s_sis_disk_ctrl *ctrl = _disk_writer_get_nots(writer_);
    if (ctrl)
    {
        s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(ctrl->map_kdicts, kname_);
        osize += sis_disk_io_write_mul(ctrl, kdict, inlist_);
    }
    if (osize > 0)
    {
        sis_disk_io_write_map(writer_->munit, kdict, NULL, SIS_SDB_STYLE_MUL, 0, 0);
    }
    return osize;
}

// 结构化时序和无时序数据 
int sis_disk_writer_sdb_remove(s_sis_disk_writer *writer_, const char *kname_, const char *sname_, int isign_)
{
    if (writer_->style != SIS_DISK_TYPE_SDB)
    {
        return -1;
    }   
    s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(writer_->munit->map_sdicts, sname_);
    if (!sdict)
    {
        return -2;
    }
    s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(writer_->munit->map_kdicts, kname_);
    if (!kdict)
    {
        return -3;
    }
    s_sis_dynamic_db *sdb = sis_pointer_list_get(sdict->sdbs, sdict->sdbs->count - 1);
    int scale = sis_disk_get_sdb_scale(sdb);
    switch (scale)
    {
    case SIS_SDB_SCALE_YEAR: // isign_ :: 2021
        sis_disk_io_write_map(writer_->munit, kdict, sdict, SIS_SDB_STYLE_SDB, isign_, 1);
        break;
    case SIS_SDB_SCALE_DATE: // isign_ :: 20211210
        sis_disk_io_write_map(writer_->munit, kdict, sdict, SIS_SDB_STYLE_SDB, isign_, 1);
        break;
    default: // SIS_SDB_SCALE_NOTS
        sis_disk_io_write_map(writer_->munit, kdict, sdict, SIS_SDB_STYLE_NON, isign_, 1);
        break;
    }
    return 0;
}
// 单键值单记录数据 
int sis_disk_writer_one_remove(s_sis_disk_writer *writer_, const char *kname_)
{
    if (writer_->style != SIS_DISK_TYPE_SDB)
    {
        return -1;
    }   
    s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(writer_->munit->map_kdicts, kname_);
    if (!kdict)
    {
        return -3;
    }
    sis_disk_io_write_map(writer_->munit, kdict, NULL, SIS_SDB_STYLE_ONE, 0, 1);
    return 0;
}
// 单键值多记录数据 inlist_ : s_sis_sds 的列表
int sis_disk_writer_mul_remove(s_sis_disk_writer *writer_, const char *kname_)
{
    if (writer_->style != SIS_DISK_TYPE_SDB)
    {
        return -1;
    }   
    s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(writer_->munit->map_kdicts, kname_);
    if (!kdict)
    {
        return -3;
    }
    sis_disk_io_write_map(writer_->munit, kdict, NULL, SIS_SDB_STYLE_MUL, 0, 1);
    return 0;
}

///////////////////////////
//  s_sis_disk_control
///////////////////////////

// 这个函数专门清理 SBD 扩展类型的数据
// int sis_disk_control_remove_sdbs(const char *path_, const char *name_)
// {
//     // 暂时不支持
//     // 1.检索目录下面所有相关文件 生成文件列表
//     // 2.统计文件数据大小 和磁盘空闲大小 如果空闲足够 就继续 否则返回错误
//     // 3.挨个锁住文件和索引，mv文件到日期命名的safe目录
//     // 4.如果有文件移动失败 就等待继续 直到全部移动完成
//     // -- 不直接删除 避免误操作 数据库文件存储已经做到所有不同组文件分离 -- //
//     return 0;
// }
// 这里style_为以下7种类型 传入后得到相应文件名然后操作
// #define  SIS_DISK_TYPE_LOG         1  // name.20210121.log
// #define  SIS_DISK_TYPE_SNO         2  // name/2021/20210121.sno
// #define  SIS_DISK_TYPE_SDB         3  // name/name.sdb
// #define  SIS_DISK_TYPE_SDB_NOTS    4  // name/nots/name.sdb name.idx
// #define  SIS_DISK_TYPE_SDB_YEAR    5  // name/year/2010.sdb
// #define  SIS_DISK_TYPE_SDB_DATE    6  // name/date/2021/20210606.sdb
// #define  SIS_DISK_TYPE_NET         7  // name/2021/20210121.net

int sis_disk_control_remove(const char *path_, const char *name_, int style_, int idate_)
{
    s_sis_disk_ctrl *munit = sis_disk_ctrl_create(style_, path_, name_, idate_);
    sis_disk_ctrl_remove(munit);
    sis_disk_ctrl_destroy(munit);
    return 1;
}
int sis_disk_control_exist(const char *path_, const char *name_, int style_, int idate_)
{
    s_sis_disk_ctrl *munit = sis_disk_ctrl_create(style_, path_, name_, idate_);
    int o = sis_disk_ctrl_read_start(munit);
    sis_disk_ctrl_read_stop(munit);
    sis_disk_ctrl_destroy(munit);
    return (o == SIS_DISK_CMD_OK) ? 1 : 0;
}

// 移动文件至目标目录
int sis_disk_control_move(const char *srcpath_, const char *name_, int style_, int idate_, const char *dstpath_)
{
    s_sis_disk_ctrl *munit = sis_disk_ctrl_create(style_, srcpath_, name_, idate_);
    sis_disk_ctrl_move(munit, dstpath_);
    sis_disk_ctrl_destroy(munit);
    return 1;
}

// 复制文件至目标目录
int sis_disk_control_copy(const char *srcpath_, const char *name_, int style_, int idate_, const char *dstpath_)
{
    s_sis_disk_ctrl *unit = sis_disk_ctrl_create(style_, srcpath_, name_, idate_);
    // char agofn[255];
    // char newfn[512];
    // for (int  i = 0; i < unit->work_fps->lists->count; i++)
    // {
    //     s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->work_fps->lists, i);
    //     sis_file_getname(unit->fn, agofn, 255);

    //     sis_sprintf(newfn, 255, "%s/%s", path_, agofn);
    //     sis_file_copy(unit->fn, newfn);
    // }
    // if (unit->work_fps->main_head.index)
    // {
    //     for (int  i = 0; i < cls_->widx_fps->lists->count; i++)
    //     {
    //         s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->widx_fps->lists, i);
    //         sis_file_getname(unit->fn, agofn, 255);
    //         sis_sprintf(newfn, 255, "%s/%s", path_, agofn);
    //         sis_file_copy(unit->fn, newfn);
    //     }
    // }
    sis_disk_ctrl_destroy(unit);
    return 1;
}
int sis_disk_log_exist(const char *path_, const char *name_, int idate_)
{
    int isok = 0;
    s_sis_disk_ctrl *munit = sis_disk_ctrl_create(SIS_DISK_TYPE_LOG, path_, name_, idate_);
    int o = sis_disk_ctrl_read_start(munit);
    if (o == SIS_DISK_CMD_NO_IDX || o == SIS_DISK_CMD_OK)
    {
        isok = 1;
    }
    sis_disk_ctrl_destroy(munit);
    return isok;
}

int sis_disk_sic_exist(const char *path_, const char *name_, int idate_)
{
    int isok = 0;
    s_sis_disk_ctrl *munit = sis_disk_ctrl_create(SIS_DISK_TYPE_SIC, path_, name_, idate_);
    int o = sis_disk_ctrl_read_start(munit);
    if (o == SIS_DISK_CMD_OK)
    {
        isok = 1;
    }
    sis_disk_ctrl_destroy(munit);
    return isok;
}

int sis_disk_sno_exist(const char *path_, const char *name_, int idate_)
{
    int isok = 0;
    s_sis_disk_ctrl *munit = sis_disk_ctrl_create(SIS_DISK_TYPE_SNO, path_, name_, idate_);
    int o = sis_disk_ctrl_read_start(munit);
    if (o == SIS_DISK_CMD_OK)
    {
        isok = 1;
    }
    sis_disk_ctrl_destroy(munit);
    return isok;
}