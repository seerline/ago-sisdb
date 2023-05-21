
#include "sis_disk.h"

///////////////////////////
//  s_sis_disk_reader_unit
///////////////////////////
void sis_disk_reader_unit_close(s_sis_disk_reader_unit *unit_);

s_sis_disk_reader_unit *sis_disk_reader_unit_create(s_sis_disk_reader *reader_, int style_)
{
    s_sis_disk_reader_unit *o = SIS_MALLOC(s_sis_disk_reader_unit, o);
    o->reader = reader_;
    o->style = style_;
    return o;
}
void sis_disk_reader_unit_destroy(void *unit_)
{
    s_sis_disk_reader_unit *unit = (s_sis_disk_reader_unit *)unit_;
    sis_disk_reader_unit_close(unit);
    sis_free(unit);
}
int sis_disk_reader_unit_open(s_sis_disk_reader_unit *unit_)
{
    sis_disk_reader_unit_close(unit_);
    unit_->ctrl = sis_disk_ctrl_create(unit_->style, unit_->reader->fpath, unit_->reader->fname, unit_->idate);
    int o = sis_disk_ctrl_read_start(unit_->ctrl);
    if (o == SIS_DISK_CMD_OK)
    {
        return 0;
    }
    else
    {
        sis_disk_reader_unit_close(unit_);
    }
    return o;
}
void sis_disk_reader_unit_close(s_sis_disk_reader_unit *unit_)
{
    if (unit_->ctrl)
    {
        sis_disk_ctrl_read_stop(unit_->ctrl);
        sis_disk_ctrl_destroy(unit_->ctrl);
        unit_->ctrl = NULL;
    }
}
///////////////////////////
//  s_sis_disk_reader_sub
///////////////////////////
s_sis_disk_reader_sub *sis_disk_reader_sub_create(const char *skey_)
{
    s_sis_disk_reader_sub *o = SIS_MALLOC(s_sis_disk_reader_sub, o);
    o->kidxs = sis_pointer_list_create();
    o->units = sis_pointer_list_create();
    o->skey = sis_sdsnew(skey_);
    return o;
}
void sis_disk_reader_sub_destroy(void *sub_)
{
    s_sis_disk_reader_sub *sub = (s_sis_disk_reader_sub *)sub_;
    sis_pointer_list_destroy(sub->kidxs);
    sis_pointer_list_destroy(sub->units);
    sis_sdsfree(sub->skey);
    sis_free(sub);
}
void sis_disk_reader_sub_push(s_sis_disk_reader_sub *sub_, s_sis_disk_reader_unit *unit_, s_sis_disk_idx *kidx_)
{
    if (!unit_ || !kidx_)
    {
        return ;
    }
    sis_pointer_list_push(sub_->kidxs, kidx_);
    sis_pointer_list_push(sub_->units, unit_);
}
int sis_disk_reader_sub_getsize(s_sis_disk_reader_sub *sub_)
{
    return sub_->kidxs->count;
}
///////////////////////////
//  s_sis_disk_reader
///////////////////////////

/**
 * @brief 
 * @param path_ 
 * @param name_ 
 * @param style_ 
 * @param cb_ 读文件的回调函数结构体，包含了一组相关的回调函数
 * @return s_sis_disk_reader* 
 */
s_sis_disk_reader *sis_disk_reader_create(const char *path_, const char *name_, int style_, s_sis_disk_reader_cb *cb_)
{
    s_sis_disk_reader *o = SIS_MALLOC(s_sis_disk_reader, o);
    o->callback = cb_;
    o->style = style_;
    o->fpath = path_ ? sis_sdsnew(path_) : sis_sdsnew("./"); 
    o->fname = name_ ? sis_sdsnew(name_) : sis_sdsnew("sisdb");

    o->munit = NULL;
    o->sunits = sis_pointer_list_create();
    o->sunits->vfree = sis_disk_reader_unit_destroy;
    o->subidxs = sis_map_list_create(sis_disk_reader_sub_destroy);

    o->issub = 1;

    o->status_open = 0;
    o->status_sub = 0;
    return o;
}
void sis_disk_reader_destroy(void *reader_)
{
    s_sis_disk_reader *reader = (s_sis_disk_reader *)reader_;
    sis_disk_reader_close(reader);
    sis_pointer_list_destroy(reader->sunits);
    sis_map_list_destroy(reader->subidxs);
    sis_sdsfree(reader->sub_keys);
    sis_sdsfree(reader->sub_sdbs);
    sis_sdsfree(reader->fpath);
    sis_sdsfree(reader->fname);
    sis_free(reader);
}
void _disk_reader_close(s_sis_disk_reader *reader_)
{
    if (reader_->status_open == 1)
    {
        sis_disk_ctrl_read_stop(reader_->munit);
        sis_disk_ctrl_destroy(reader_->munit);
        reader_->munit = NULL;
        reader_->status_open = 0;
    }
}
// 打开 准备读 首先加载主文件到内存中 就知道目录下支持哪些数据了 LOG SNO SDB
int _disk_reader_open(s_sis_disk_reader *reader_, int style_, int idate_)
{
    if (reader_->status_open != 0)
    {
        _disk_reader_close(reader_);
    }
    if (reader_->style != style_)
    {
        LOG(5)("style no same : %d != %d.\n", style_, reader_->style);
        return -1;        
    }
    if (reader_->status_sub != 0)
    {
        LOG(5)("cannot sub %s.\n", reader_->fname);
        return -2;
    }
    
    reader_->munit = sis_disk_ctrl_create(reader_->style, reader_->fpath, reader_->fname, idate_);
    int o = sis_disk_ctrl_read_start(reader_->munit);
    // 这里已经读取了索引和字典
    if (o == SIS_DISK_CMD_OK || 
       (o == SIS_DISK_CMD_NO_IDX && (style_ == SIS_DISK_TYPE_SDB || style_ == SIS_DISK_TYPE_LOG)))
    {
        reader_->status_open = 1;
        return 0;
    }
    sis_disk_ctrl_destroy(reader_->munit);
    return o;
}

// 顺序读取 仅支持 LOG  通过回调的 cb_original 返回数据
int sis_disk_reader_sub_log(s_sis_disk_reader *reader_, int idate_)
{
    if (_disk_reader_open(reader_, SIS_DISK_TYPE_LOG, idate_))
    {
        LOG(5)("no open %s.\n", reader_->fname);
        return 0;
    }
    reader_->status_sub = 1;
    // 按顺序输出 
    sis_disk_io_sub_log(reader_->munit, reader_->callback);
    // 订阅结束
    reader_->status_sub = 0;

    _disk_reader_close(reader_);
    return 0;
}

// 顺序读取 仅支持 NET  通过回调的 cb_original 或 cb_bytedata 返回数据
// 如果定义了 cb_bytedata 就解压数据再返回
// 可支持多个key和sdb订阅 k1,k2,k3  db1,db2,db3
int sis_disk_reader_sub_sic(s_sis_disk_reader *reader_, const char *keys_, const char *sdbs_, int idate_)
{
    int o = _disk_reader_open(reader_, SIS_DISK_TYPE_SIC, idate_);
    if (o)
    {
        LOG(5)("no open %s. %d\n", reader_->fname, o);
        return 0;
    }
    reader_->status_sub = 1;
    // 按顺序输出 keys_ sdbs_ = NULL 实际表示 *
    sis_disk_io_sub_sic(reader_->munit, keys_, sdbs_, NULL, reader_->callback);
    // 订阅结束
    reader_->status_sub = 0;

    _disk_reader_close(reader_);
    return 0;
}
// 读取 仅支持 SNO  
// 如果定义了 cb_bytedata cb_chardata 就解压数据再返回
// 可支持多个key和sdb订阅 k1,k2,k3  db1,db2,db3
/**
 * @brief 读取SNO文件
 * @param reader_ 
 * @param keys_ 需要读取行情的股票列表
 * @param sdbs_ 行情格式，JSON字符串
 * @param idate_ 需要读取的行情日期
 * @return int 
 */
int sis_disk_reader_sub_sno(s_sis_disk_reader *reader_, const char *keys_, const char *sdbs_, int idate_)
{
    int o = _disk_reader_open(reader_, SIS_DISK_TYPE_SNO, idate_);
    if (o)
    {
        // 通知订阅者读取或订阅结束
        if (reader_->callback->cb_stop)
        {
            reader_->callback->cb_stop(reader_->callback->cb_source, idate_);
        }
        LOG(5)("no open %s. %d\n", reader_->fname, o);
        return o;
    }
    reader_->status_sub = 1;
    // 按顺序输出 keys_ sdbs_ = NULL 实际表示 *
    sis_disk_io_sub_sno(reader_->munit, keys_, sdbs_, NULL, reader_->callback);
    // 订阅结束
    reader_->status_sub = 0;

    _disk_reader_close(reader_);
    return 0;
}
// 取消一个正在订阅的任务 只有处于非订阅状态下才能订阅 避免重复订阅
void sis_disk_reader_unsub(s_sis_disk_reader *reader_)
{
    if (reader_->status_sub == 1)
    {
        reader_->status_sub = 2;
        // 中断读取动作
        reader_->munit->isstop = true;
        for (int i = 0; i < reader_->sunits->count; i++)
        {
            s_sis_disk_reader_unit *unit = sis_pointer_list_get(reader_->sunits, i);
            if (unit->ctrl)
            {
                unit->ctrl->isstop = true;
            }
        }
    }
}
/////////////////////////////////////////////////////////////////
//  打开sdb类型文件 先打开主文件 然后建立需要的文件列表在一个一个打开
////////////////////////////////////////////////////////////////
int sis_disk_reader_open(s_sis_disk_reader *reader_)
{
    if (reader_->status_open != 0)
    {
        return 0;
    }
    if (reader_->style != SIS_DISK_TYPE_SDB)
    {
        LOG(5)("style no same : %d != %d.\n", SIS_DISK_TYPE_SDB, reader_->style);
        return -1;        
    }    
    reader_->munit = sis_disk_ctrl_create(reader_->style, reader_->fpath, reader_->fname, 0);
    //  if (reader_->style != SIS_DISK_TYPE_SDB && reader_->style != SIS_DISK_TYPE_SNO)
    // {
    //     LOG(5)("style no same : %d != %d.\n", SIS_DISK_TYPE_SDB, reader_->style);
    //     return -1;        
    // }    
    // int wdate = 0;
    // if (reader_->style == SIS_DISK_TYPE_SNO)
    // {
    //     wdate = sis_time_get_idate(reader_->search_msec.start / 1000);
    // }
    // reader_->munit = sis_disk_ctrl_create(reader_->style, reader_->fpath, reader_->fname, wdate);
    int o = sis_disk_ctrl_read_start(reader_->munit);
    if (o == SIS_DISK_CMD_OK || o == SIS_DISK_CMD_NO_IDX)
    {
        // 读取了索引和字典信息
        reader_->status_open = 1;
        return 0;
    }
    else
    {
        sis_disk_ctrl_read_stop(reader_->munit);
        sis_disk_ctrl_destroy(reader_->munit);
        reader_->munit = NULL;
    }
    return o;
}
void sis_disk_reader_close(s_sis_disk_reader *reader_)
{
    sis_disk_reader_unsub(reader_);
    while (reader_->status_sub != 0)
    {
        sis_sleep(30);
    }
    if (reader_->status_open == 1)
    {
        if (reader_->munit)
        {
            sis_disk_ctrl_read_stop(reader_->munit);
            sis_disk_ctrl_destroy(reader_->munit);
            reader_->munit = NULL;
        }
        // 清除可能
        sis_map_list_clear(reader_->subidxs);
        sis_pointer_list_clear(reader_->sunits);
        reader_->status_open = 0;
    }
}


s_sis_dynamic_db *sis_disk_reader_getdb(s_sis_disk_reader *reader_, const char *sname_)
{
    s_sis_dynamic_db *db = NULL;
    if (reader_->style == SIS_DISK_TYPE_SDB)
    {
        s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(reader_->munit->map_sdicts, sname_);
        if (sdict)
        {
            db = sis_disk_sdict_last(sdict);
        }
    }
    if (reader_->style == SIS_DISK_TYPE_SNO)
    {
        s_sis_disk_reader_unit *munit = sis_pointer_list_get(reader_->sunits, 0);
        if (munit)
        {
            s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(munit->ctrl->map_sdicts, sname_);
            if (sdict)
            {
                db = sis_disk_sdict_last(sdict);
            }
        }
    }
    return db;
}


void sis_disk_reader_init(s_sis_disk_reader *reader_, const char *kname_, const char *sname_, s_sis_msec_pair *smsec_, int issub_)
{
    sis_map_list_clear(reader_->subidxs);
    sis_pointer_list_clear(reader_->sunits);

    sis_sdsfree(reader_->sub_keys);
    reader_->sub_keys = kname_ ? sis_sdsnew(kname_) : NULL;
    sis_sdsfree(reader_->sub_sdbs);
    reader_->sub_sdbs = sname_ ? sis_sdsnew(sname_) : NULL;

    reader_->issub = issub_;
    sis_disk_set_search_msec(smsec_, &reader_->search_msec);
}

// 到这里 sub_keys_ sub_sdbs_ 必须有值
int sis_disk_reader_filters(s_sis_disk_reader *reader_, s_sis_disk_reader_unit *unit_)
{
    int count = 0;
    if (unit_->reader->isone)
    {
        char info[255];
        sis_sprintf(info, 255, "%s.%s", unit_->reader->sub_keys, unit_->reader->sub_sdbs);    
        s_sis_disk_idx *subidx = (s_sis_disk_idx *)sis_map_list_get(unit_->ctrl->map_idxs, info);
        if (subidx)
        {
            s_sis_disk_reader_sub  *rsub = sis_map_list_get(reader_->subidxs, info);
            if (!rsub)
            {
                rsub = sis_disk_reader_sub_create(info);
                sis_map_list_set(reader_->subidxs, info, rsub);
            }
            // printf("== add : %d %s %s\n", count, SIS_OBJ_SDS(subidx->kname), SIS_OBJ_SDS(subidx->sname));
            sis_disk_reader_sub_push(rsub, unit_, subidx);
            count++;
        }
    }
    else
    {
        int nums = sis_map_list_getsize(unit_->ctrl->map_idxs);
        for (int k = 0; k < nums; k++)
        {
            s_sis_disk_idx *subidx = (s_sis_disk_idx *)sis_map_list_geti(unit_->ctrl->map_idxs, k);
            if (!subidx->kname || !subidx->sname) //表示为key sdb 或 sno net
            {
                continue;
            }
            printf("== filter : %d %p %p\n", nums, subidx->kname, subidx->sname);
            if ((!sis_strcasecmp(unit_->reader->sub_sdbs, "*") || sis_str_subcmp_strict(SIS_OBJ_SDS(subidx->sname), unit_->reader->sub_sdbs, ',') >= 0) &&
                (!sis_strcasecmp(unit_->reader->sub_keys, "*") || sis_str_subcmp(SIS_OBJ_SDS(subidx->kname), unit_->reader->sub_keys, ',') >= 0))
            {
                char info[255];
                sis_sprintf(info, 255, "%s.%s", SIS_OBJ_SDS(subidx->kname), SIS_OBJ_SDS(subidx->sname));   
                s_sis_disk_reader_sub  *rsub = sis_map_list_get(reader_->subidxs, info);
                if (!rsub)
                {
                    rsub = sis_disk_reader_sub_create(info);
                    sis_map_list_set(reader_->subidxs, info, rsub);
                }
                // printf("== adda : %d %p %p\n", count, subidx->kname, subidx->sname);
                sis_disk_reader_sub_push(rsub, unit_, subidx);
                count++;
            }
        }  
    }
    return count;
}
// 在 reader 中 应该生成 年和日的 kint map 表 
// 文件打得开的放到 map 表中 下次读取时 就可以不用按顺序打开了 
// 以后再优化
void _disk_reader_make_sdb_from_file(s_sis_disk_reader *reader_)
{
    // 2.检查日上时序数据
    int open_year = sis_time_get_idate(reader_->search_msec.start / 1000) / 10000;
    int stop_year = sis_time_get_idate(reader_->search_msec.stop / 1000) / 10000;
    // open_year = open_year / 10 * 10;
    // stop_year = stop_year / 10 * 10;
    // ??? 以下的过滤器未来要改成根据 map 的信息获取数据
    while(open_year <= stop_year)
    {
        LOG(5)("year: %d %d\n", open_year, stop_year);
        int isok = true;
        s_sis_disk_reader_unit *unit = sis_disk_reader_unit_create(reader_, SIS_DISK_TYPE_SDB_YEAR);
        unit->idate = open_year * 10000 + 101;
        if (!sis_disk_reader_unit_open(unit))
        {
            if (sis_disk_reader_filters(reader_, unit) == 0)
            {
                // 没有相关订阅记录
                isok = false;
            }
        }
        else
        {
            isok = false;
        }
        if (isok)
        {
            sis_pointer_list_push(reader_->sunits, unit);
        }
        else
        {
            sis_disk_reader_unit_destroy(unit);
        }
        // open_year += 10;
        open_year += 1;
    }
    if (reader_->isone && sis_map_list_getsize(reader_->subidxs) > 0)
    {
        return;
    }
    
    // 3.检查日下时序数据
    {
        int open = sis_time_get_idate(reader_->search_msec.start / 1000);
        int stop = sis_time_get_idate(reader_->search_msec.stop / 1000);
        while (open <= stop)
        {
            int isok = true;
            s_sis_disk_reader_unit *unit = sis_disk_reader_unit_create(reader_, SIS_DISK_TYPE_SDB_DATE);
            unit->idate = open;
            if (!sis_disk_reader_unit_open(unit))
            {
                if (sis_disk_reader_filters(reader_, unit) == 0)
                {
                    // 没有相关订阅记录
                    isok = false;
                }
            }
            else
            {
                isok = false;
            }
            if (isok)
            {
                sis_pointer_list_push(reader_->sunits, unit);
            }
            else
            {
                sis_disk_reader_unit_destroy(unit);
            }
            open = sis_time_next_work_day(open, 1);
        }
    }
}
void sis_disk_reader_make_sdb(s_sis_disk_reader *reader_)
{
    // 时间 = 0 表示取最近的数据块 
    // 先确定时间的日期区间 找到对应时区的文件 打开不成功就下一个
    // 成功就读取索引 并做匹配 匹配到的就加入索引列表
    // 索引列表不为空该文件有效 添加到文件列表 并且每个文件都是打开状态
    // 1.检查无时序结构化数据
    {   // 检查无时序数据
        int isok = true;
        s_sis_disk_reader_unit *unit = sis_disk_reader_unit_create(reader_, SIS_DISK_TYPE_SDB_NOTS);
        if (!sis_disk_reader_unit_open(unit))
        {
            if (sis_disk_reader_filters(reader_, unit) == 0)
            {
                // 没有相关订阅记录
                isok = false;
            }
        }
        else
        {
            isok = false;
        }
        if (isok)
        {
            sis_pointer_list_push(reader_->sunits, unit);
        }
        else
        {
            sis_disk_reader_unit_destroy(unit);
        }
    }
    if (reader_->isone && sis_map_list_getsize(reader_->subidxs) > 0)
    {
        return;
    }
    if (sis_map_list_getsize(reader_->munit->map_maps) > 0)
    {
        // _disk_reader_make_sdb_from_fmap(reader_);
        _disk_reader_make_sdb_from_file(reader_);
    }
    else
    {
        _disk_reader_make_sdb_from_file(reader_);
    }

}

void sis_disk_reader_make_alone(s_sis_disk_reader *reader_, const char *kname_)
{
    int isok = true;
    s_sis_disk_reader_sub  *rsub = sis_disk_reader_sub_create(kname_);
    s_sis_disk_reader_unit *unit = sis_disk_reader_unit_create(reader_, SIS_DISK_TYPE_SDB_NOTS);
    if (!sis_disk_reader_unit_open(unit))
    {
        s_sis_disk_idx *subidx = (s_sis_disk_idx *)sis_map_list_get(unit->ctrl->map_idxs, kname_);
        if (subidx)
        {
            sis_disk_reader_sub_push(rsub, unit, subidx);
            printf("subwork-0-: %s = %d:%d %p \n", rsub->skey, rsub->kidxs->count, rsub->units->count, subidx->kname);
        } 
        else
        {
            isok = false;
        }
    }
    else
    {
        isok = false;
    }
    if (isok)
    {
        sis_pointer_list_push(reader_->sunits, unit);
        sis_map_list_set(reader_->subidxs, kname_, rsub);
    }
    else
    {
        sis_disk_reader_sub_destroy(rsub);
        sis_disk_reader_unit_destroy(unit);
    }
}
s_sis_object *sis_disk_reader_get_one(s_sis_disk_reader *reader_, const char *kname_)
{
    if (reader_->status_open == 0 || reader_->status_sub == 1 || !kname_)
    {
        return NULL;
    }
    reader_->isone = 1;
    reader_->issub = 0;
    sis_map_list_clear(reader_->subidxs);
    sis_pointer_list_clear(reader_->sunits);
    sis_disk_reader_make_alone(reader_, kname_);

    if (sis_map_list_getsize(reader_->subidxs) > 0)
    {
        s_sis_memory *memory = sis_memory_create();
        s_sis_object * obj = sis_object_create(SIS_OBJECT_MEMORY, memory);
        reader_->status_sub = 1;
        // 只有一个键
        s_sis_disk_reader_sub *subwork = (s_sis_disk_reader_sub *)sis_map_list_geti(reader_->subidxs, 0);
        int count = sis_disk_reader_sub_getsize(subwork);
        for (int j = 0; j < count; j++)
        {
            s_sis_disk_reader_unit *runit = (s_sis_disk_reader_unit *)sis_pointer_list_get(subwork->units, j);
            s_sis_disk_rcatch *rcatch = runit->ctrl->rcatch;
            s_sis_disk_idx *subidx = (s_sis_disk_idx *)sis_pointer_list_get(subwork->kidxs, j);
            for (int k = 0; k < subidx->idxs->count; k++)
            {
                s_sis_disk_idx_unit *idxunit = (s_sis_disk_idx_unit *)sis_struct_list_get(subidx->idxs, k);
                sis_disk_rcatch_init_of_idx(rcatch, idxunit);
                sis_disk_io_read_sdb(runit->ctrl, rcatch);
                if (rcatch->head.hid == SIS_DISK_HID_MSG_ONE)
                {
                    // 跳掉长度
                    sis_memory_get_ssize(rcatch->memory);
                    // printf("%lld %lld \n", rcatch->rinfo->offset, rcatch->rinfo->size);
                    // sis_out_binary(".out.",sis_memory(rcatch->memory), sis_memory_get_size(rcatch->memory));
                    sis_memory_cat(memory, sis_memory(rcatch->memory), sis_memory_get_size(rcatch->memory));
                }
            }
        }
        // 订阅结束
        reader_->status_sub = 0;
        if (sis_memory_get_size(memory) > 0)
        {
            return obj;
        }
        sis_object_destroy(obj);
    }
    return NULL;
}
// 返回值 释放类型为 s_sis_memory
s_sis_node *sis_disk_reader_get_mul(s_sis_disk_reader *reader_, const char *kname_)
{
    if (reader_->status_open == 0 || reader_->status_sub == 1 || !kname_)
    {
        return NULL;
    }
    reader_->isone = 1;
    reader_->issub = 0;
    sis_map_list_clear(reader_->subidxs);
    sis_pointer_list_clear(reader_->sunits);
    sis_disk_reader_make_alone(reader_, kname_);

    s_sis_node *obj = sis_node_create();
    if (sis_map_list_getsize(reader_->subidxs) > 0)
    {
        reader_->status_sub = 1;
        // 只有一个键
        s_sis_disk_reader_sub *subwork = (s_sis_disk_reader_sub *)sis_map_list_geti(reader_->subidxs, 0);
        int count = sis_disk_reader_sub_getsize(subwork);
        for (int j = 0; j < count; j++)
        {
            s_sis_disk_reader_unit *runit = (s_sis_disk_reader_unit *)sis_pointer_list_get(subwork->units, j);
            s_sis_disk_rcatch *rcatch = runit->ctrl->rcatch;
            s_sis_disk_idx *subidx = (s_sis_disk_idx *)sis_pointer_list_get(subwork->kidxs, j);
            for (int k = 0; k < subidx->idxs->count; k++)
            {
                s_sis_disk_idx_unit *idxunit = (s_sis_disk_idx_unit *)sis_struct_list_get(subidx->idxs, k);
                sis_disk_rcatch_init_of_idx(rcatch, idxunit);
                sis_disk_io_read_sdb(runit->ctrl, rcatch);
                if (rcatch->head.hid == SIS_DISK_HID_MSG_MUL)
                {
                    int nums = sis_memory_get_ssize(rcatch->memory);
                    for (int i = 0; i < nums; i++)
                    {
                        int insize = sis_memory_get_ssize(rcatch->memory);
                        s_sis_sds in = sis_sdsnewlen(sis_memory(rcatch->memory), insize);
                        sis_memory_move(rcatch->memory, insize);
                        sis_node_push(obj, in);
                    }
                }
            }
        }
        // 订阅结束
        reader_->status_sub = 0;
    }
    if (sis_node_get_size(obj) > 0)
    {
        return obj;
    }
    sis_node_destroy(obj);
    return NULL;
}
// 时间范围 日期以上可以多日 日期以下只能当日
// 多表按时序输出通过该函数获取全部数据后 排序输出
s_sis_object *_disk_reader_get_sdb_obj(s_sis_disk_reader *reader_, const char *kname_, const char *sname_, s_sis_msec_pair *smsec_)
{
    // printf("==1== %s %s %d %d\n", kname_, sname_, reader_->status_sub, reader_->status_open);
    if (reader_->status_open == 0 || reader_->status_sub == 1 || !kname_ || !sname_)
    {
        return NULL;
    }
    // printf("==2== %s %s \n", kname_, sname_);
    if (sis_is_multiple_sub(kname_, sis_strlen(kname_)) || sis_is_multiple_sub(sname_, sis_strlen(sname_)))
    {
        LOG(5)("no mul key or sdb: %s\n", kname_, sname_);
        return NULL;
    }
    // printf("==3== %s %s \n", kname_, sname_);
    reader_->isone = 1;
    sis_disk_reader_init(reader_, kname_, sname_, smsec_, 0);

    // 只读结构化数据
    sis_disk_reader_make_sdb(reader_);

    // printf("==4== %d \n", sis_map_list_getsize(reader_->subidxs));
    if (sis_map_list_getsize(reader_->subidxs) > 0)
    {
        s_sis_memory *memory = sis_memory_create();
        s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, memory);
        reader_->status_sub = 1;
        // 只有一个键
        s_sis_disk_reader_sub *subwork = (s_sis_disk_reader_sub *)sis_map_list_geti(reader_->subidxs, 0);
        int count = sis_disk_reader_sub_getsize(subwork);
        for (int j = 0; j < count; j++)
        {
            s_sis_disk_reader_unit *runit = (s_sis_disk_reader_unit *)sis_pointer_list_get(subwork->units, j);
            s_sis_disk_rcatch *rcatch = runit->ctrl->rcatch;
            s_sis_disk_idx *subidx = (s_sis_disk_idx *)sis_pointer_list_get(subwork->kidxs, j);
            for (int k = 0; k < subidx->idxs->count; k++)
            {
                s_sis_disk_idx_unit *idxunit = (s_sis_disk_idx_unit *)sis_struct_list_get(subidx->idxs, k);
                sis_disk_rcatch_init_of_idx(rcatch, idxunit);
                
                sis_disk_io_read_sdb(runit->ctrl, rcatch);
                // printf("%d %llu | %zu\n", rcatch->rinfo->offset, rcatch->rinfo->size, sis_memory_get_size(rcatch->memory));
                // sis_out_binary(".out.",sis_memory(rcatch->memory), sis_memory_get_size(rcatch->memory));
                sis_memory_cat(memory, sis_memory(rcatch->memory), sis_memory_get_size(rcatch->memory));
            }
        }
        // 订阅结束
        reader_->status_sub = 0;
        if (sis_memory_get_size(memory) > 0)
        {
            return obj;
        }
        sis_object_destroy(obj);
    }
    return NULL;
}

void sis_disk_reader_make_sno(s_sis_disk_reader *reader_)
{
    // 时间 = 0 表示取最近的数据块 
    // 先确定时间的日期区间 找到对应时区的文件 打开不成功就下一个
    // 成功就读取索引 并做匹配 匹配到的就加入索引列表
    // 索引列表不为空该文件有效 添加到文件列表 并且每个文件都是打开状态
    {
        int open = sis_time_get_idate(reader_->search_msec.start / 1000);
        int stop = sis_time_get_idate(reader_->search_msec.stop / 1000);
        while (open <= stop)
        {
            int isok = true;
            s_sis_disk_reader_unit *unit = sis_disk_reader_unit_create(reader_, SIS_DISK_TYPE_SNO);
            unit->idate = open;
            if (!sis_disk_reader_unit_open(unit))
            {
                if (sis_disk_reader_filters(reader_, unit) == 0)
                {
                    // 没有相关订阅记录
                    isok = false;
                }
            }
            else
            {
                isok = false;
            }
            if (isok)
            {
                sis_pointer_list_push(reader_->sunits, unit);
            }
            else
            {
                sis_disk_reader_unit_destroy(unit);
            }
            open = sis_time_next_work_day(open, 1);
        }
    }
}

s_sis_object *_disk_reader_get_sno_obj(s_sis_disk_reader *reader_, const char *kname_, const char *sname_, s_sis_msec_pair *smsec_)
{
    if (sis_is_multiple_sub(kname_, sis_strlen(kname_)) || sis_is_multiple_sub(sname_, sis_strlen(sname_)))
    {
        LOG(5)("no mul key or sdb: %s\n", kname_, sname_);
        return NULL;
    }
    if (reader_->status_sub == 1 || !kname_ || !sname_)
    {
        return NULL;
    }   
    reader_->search_msec.start = smsec_->start;
    reader_->search_msec.stop  = smsec_->stop;
    
    reader_->isone = 1;
    sis_disk_reader_init(reader_, kname_, sname_, smsec_, 0);

    // 只读结构化数据
    sis_disk_reader_make_sno(reader_);

    s_sis_dynamic_db *sdb = sis_disk_reader_getdb(reader_, sname_);

    if (sis_map_list_getsize(reader_->subidxs) > 0)
    {
        s_sis_memory *memory = sis_memory_create();
        s_sis_object * obj = sis_object_create(SIS_OBJECT_MEMORY, memory);
        reader_->status_sub = 1;
        // 只有一个键
        s_sis_disk_reader_sub *subwork = (s_sis_disk_reader_sub *)sis_map_list_geti(reader_->subidxs, 0);
        int count = sis_disk_reader_sub_getsize(subwork);
        for (int j = 0; j < count; j++)
        {
            s_sis_disk_reader_unit *runit = (s_sis_disk_reader_unit *)sis_pointer_list_get(subwork->units, j);
            s_sis_disk_rcatch *rcatch = runit->ctrl->rcatch;
            s_sis_disk_idx *subidx = (s_sis_disk_idx *)sis_pointer_list_get(subwork->kidxs, j);
            for (int k = 0; k < subidx->idxs->count; k++)
            {
                s_sis_disk_idx_unit *idxunit = (s_sis_disk_idx_unit *)sis_struct_list_get(subidx->idxs, k);
                sis_disk_rcatch_init_of_idx(rcatch, idxunit);
                sis_disk_io_read_sno(runit->ctrl, rcatch);
                // sis_out_binary(".out.",sis_memory(rcatch->memory), sis_memory_get_size(rcatch->memory));
                while(sis_memory_get_size(rcatch->memory) > 0)
                {
                    // size_t size = 
                    sis_memory_get_ssize(rcatch->memory);
                    // printf("[%d] %llu %llu | %d %d %zu\n", subidx->idxs->count, rcatch->rinfo->offset, rcatch->rinfo->size, rcatch->kidx, rcatch->sidx, size);
                    sis_memory_cat(memory, sis_memory(rcatch->memory), sdb->size);
                    sis_memory_move(rcatch->memory, sdb->size);
                }
            }
        }
        // 订阅结束
        reader_->status_sub = 0;
        if (sis_memory_get_size(memory) > 0)
        {
            return obj;
        }
        sis_object_destroy(obj);
    }
    return NULL;
}
// 从对应文件中获取数据 拼成完整的数据返回 只支持 SNO SDB 单键单表 
s_sis_object *sis_disk_reader_get_obj(s_sis_disk_reader *reader_, const char *kname_, const char *sname_, s_sis_msec_pair *smsec_)
{

    s_sis_object * obj = NULL;
    if (reader_->style == SIS_DISK_TYPE_SNO)
    {
        // 只支持根据日期获取
        obj = _disk_reader_get_sno_obj(reader_, kname_, sname_, smsec_);
    }
    else if (reader_->style == SIS_DISK_TYPE_SDB)
    {
        obj = _disk_reader_get_sdb_obj(reader_, kname_, sname_, smsec_);
    }
    return obj;
}
// 获取sno键值
s_sis_object *sis_disk_reader_get_keys(s_sis_disk_reader *reader_, int idate)
{
    return NULL;
}
s_sis_sds sis_map_sdict_as_sdbs(s_sis_map_list *map_sdicts_)
{
    s_sis_json_node *innode = sis_json_create_object();
    int count = sis_map_list_getsize(map_sdicts_);
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_sdict *sdict = sis_map_list_geti(map_sdicts_, i);
        s_sis_dynamic_db *db = sis_disk_sdict_last(sdict);
		sis_json_object_add_node(innode, db->name, sis_sdbinfo_to_json(db));
    }
    s_sis_sds o = sis_json_to_sds(innode, 1);
	sis_json_delete_node(innode);
	return o;
}
// 获取sno数据库
s_sis_object *sis_disk_reader_get_sdbs(s_sis_disk_reader *reader_, int idate)
{
    if (reader_->style == SIS_DISK_TYPE_SNO)
    {
        reader_->munit = sis_disk_ctrl_create(reader_->style, reader_->fpath, reader_->fname, idate);
    }
    else if (reader_->style == SIS_DISK_TYPE_SDB)
    {
        reader_->munit = sis_disk_ctrl_create(reader_->style, reader_->fpath, reader_->fname, 0);
    }
    else
    {
        return NULL;
    }
    s_sis_object * obj = NULL;
    int o = sis_disk_ctrl_read_start(reader_->munit);
    if (o == SIS_DISK_CMD_OK || o == SIS_DISK_CMD_NO_IDX)
    {
        obj = sis_object_create(SIS_OBJECT_SDS, sis_map_sdict_as_sdbs(reader_->munit->map_sdicts));
    }
    sis_disk_ctrl_read_stop(reader_->munit);
    sis_disk_ctrl_destroy(reader_->munit);
    reader_->munit = NULL;
    return obj;
}

//////////////////////////////////
// 订阅的相关函数
/////////////////////////////////
static int cb_sub_start(void *reader_, void *avgv_)
{
    s_sis_disk_reader *reader = (s_sis_disk_reader *)reader_; 
    if (reader->callback->cb_start)
    {
        reader->callback->cb_start(reader->callback->cb_source, sis_time_get_idate(reader->search_msec.start / 1000));
    }
	return 0;
}
static int cb_sub_stop(void *reader_, void *avgv_)
{
    s_sis_disk_reader *reader = (s_sis_disk_reader *)reader_; 
    if (reader->status_sub == 2)
    {
        if (reader->callback->cb_break)
        {
            reader->callback->cb_break(reader->callback->cb_source, sis_time_get_idate(reader->search_msec.start / 1000));
        }
    }
    else
    {
        if (reader->callback->cb_stop)
        {
            reader->callback->cb_stop(reader->callback->cb_source, sis_time_get_idate(reader->search_msec.start / 1000));
        }
    }
	return 0;
}
static int cb_key_stop(void *reader_, void *avgv_)
{
    s_sis_disk_reader *reader = (s_sis_disk_reader *)reader_; 
    if (reader->status_sub == 2)
    {
        return -1;
    }
    s_sis_db_chars *chars = (s_sis_db_chars *)avgv_;
    char skey[255];
    sis_sprintf(skey, 255, "%s.%s", chars->kname, chars->sname);
    s_sis_disk_reader_sub *subwork = (s_sis_disk_reader_sub *)sis_map_list_get(reader->subidxs, skey);
    if (!subwork)
    {   
        return 0;
    }
_sub_next:
    {
        s_sis_disk_reader_unit *runit = (s_sis_disk_reader_unit *)sis_pointer_list_get(subwork->units, subwork->unit_cursor);
        s_sis_disk_idx *subidx = (s_sis_disk_idx *)sis_pointer_list_get(subwork->kidxs, subwork->unit_cursor);
        if (!runit || !subidx)
        {
            return 0;
        }
        subwork->kidx_cursor++;
        s_sis_disk_idx_unit *idxunit = (s_sis_disk_idx_unit *)sis_struct_list_get(subidx->idxs, subwork->kidx_cursor);
        if (idxunit)
        {
            s_sis_disk_rcatch *rcatch = runit->ctrl->rcatch;
            sis_disk_rcatch_init_of_idx(rcatch, idxunit);
            sis_disk_io_read_sdb(runit->ctrl, rcatch);
            sis_subdb_cxt_push_sdbs(reader->subcxt, skey, sis_memory(rcatch->memory), sis_memory_get_size(rcatch->memory));
        }
        else
        {
            subwork->unit_cursor++;
            subwork->kidx_cursor = -1;
            goto _sub_next;
        }
    }
    return 0;
}
static int cb_key_chars(void *reader_, void *avgv_)
{
    s_sis_disk_reader *reader = (s_sis_disk_reader *)reader_; 
	s_sis_db_chars *chars = (s_sis_db_chars *)avgv_;

    // 因为取消订阅而退出
    if (reader->status_sub == 2)
    {
        return -1;
    }
    if (reader->callback->cb_chardata)
    {
        reader->callback->cb_chardata(reader->callback->cb_source,
            chars->kname, chars->sname, 
            chars->data, chars->size);
    } 
	return 0;
}

void _disk_reader_sub_work(s_sis_disk_reader *reader_)
{
    int count = sis_map_list_getsize(reader_->subidxs);  
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_reader_sub *subwork = (s_sis_disk_reader_sub *)sis_map_list_geti(reader_->subidxs, i);
        subwork->kidx_cursor = 0;
        subwork->unit_cursor = 0;
        s_sis_disk_reader_unit *runit = (s_sis_disk_reader_unit *)sis_pointer_list_get(subwork->units, 0);
        s_sis_disk_idx *subidx = (s_sis_disk_idx *)sis_pointer_list_get(subwork->kidxs, 0);
        printf("subwork-1-: %d = %d:%d %p %p\n", count, subwork->kidxs->count, subwork->units->count, subidx->kname, subidx->sname);
            // subidx && subidx->kname ? SIS_OBJ_SDS(subidx->kname) : "nil",
            // subidx && subidx->sname ? SIS_OBJ_SDS(subidx->sname) : "nil");
        s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(runit->ctrl->map_sdicts, SIS_OBJ_SDS(subidx->sname));
        s_sis_dynamic_db *sdb = sis_disk_sdict_get(sdict, sdict->sdbs->count - 1);
        sis_subdb_cxt_set_sdbs(reader_->subcxt, sdb);
    }
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_reader_sub *subwork = (s_sis_disk_reader_sub *)sis_map_list_geti(reader_->subidxs, i);
        s_sis_disk_reader_unit *runit = (s_sis_disk_reader_unit *)sis_pointer_list_get(subwork->units, subwork->unit_cursor);
        s_sis_disk_rcatch *rcatch = runit->ctrl->rcatch;
        s_sis_disk_idx *subidx = (s_sis_disk_idx *)sis_pointer_list_get(subwork->kidxs, subwork->unit_cursor);
        printf("subwork-2-: %d:%d %s %s\n", subwork->kidxs->count, subwork->units->count, 
            subidx && subidx->kname ? SIS_OBJ_SDS(subidx->kname) : "nil",
            subidx && subidx->sname ? SIS_OBJ_SDS(subidx->sname) : "nil");
        s_sis_disk_idx_unit *idxunit = (s_sis_disk_idx_unit *)sis_struct_list_get(subidx->idxs, subwork->kidx_cursor);
        sis_disk_rcatch_init_of_idx(rcatch, idxunit);
        sis_disk_io_read_sdb(runit->ctrl, rcatch);
        sis_subdb_cxt_push_sdbs(reader_->subcxt, subwork->skey, sis_memory(rcatch->memory), sis_memory_get_size(rcatch->memory));
    }
    sis_subdb_cxt_sub_start(reader_->subcxt);
}

int sis_disk_reader_sub_sdb(s_sis_disk_reader *reader_, const char *keys_, const char *sdbs_, s_sis_msec_pair *smsec_)
{
    if (reader_->status_open == 0 || reader_->status_sub == 1)
    {
        return -1;
    }
    sis_disk_reader_init(reader_, keys_, sdbs_, smsec_, 1);
    if (!reader_->sub_keys)
    {
        reader_->sub_keys = sis_sdsnew("*");
    }
    if (!reader_->sub_sdbs)
    {
        reader_->sub_sdbs = sis_sdsnew("*");
    }
    reader_->isone = 0;
    if (!sis_is_multiple_sub(reader_->sub_keys, sis_strlen(reader_->sub_keys)) &&
        !sis_is_multiple_sub(reader_->sub_sdbs, sis_strlen(reader_->sub_sdbs)))
    {
        // 表示单键单表
        reader_->isone = 1;
    }
    // 只读结构化数据
    sis_disk_reader_make_sdb(reader_);
    if (sis_map_list_getsize(reader_->subidxs) > 0)
    {
        reader_->status_sub = 1;
        reader_->subcxt = sis_subdb_cxt_create(0); 
        reader_->subcxt->cb_source = reader_;
        reader_->subcxt->cb_sub_start = cb_sub_start;
        reader_->subcxt->cb_sub_stop = cb_sub_stop;
        reader_->subcxt->cb_key_stop = cb_key_stop;
        reader_->subcxt->cb_key_chars = cb_key_chars;

        _disk_reader_sub_work(reader_);

        sis_subdb_cxt_destroy(reader_->subcxt);
        // 订阅结束
        reader_->status_sub = 0;
    }
    return 0;
}


