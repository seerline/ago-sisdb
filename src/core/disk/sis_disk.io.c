
#include "sis_disk.h"

////////////////////////////////////////////////////////
// s_sis_disk_ctrl
////////////////////////////////////////////////////////
void _disk_ctrl_init(s_sis_disk_ctrl *o)
{
    o->work_fps->main_head.style = o->style;
    o->work_fps->main_head.index = 0;
    o->work_fps->main_head.wdate = o->open_date;

    o->net_count = 0;
    o->net_msec = 0;
    o->net_pages = 0;
    o->net_zipsize = 0;
    o->net_incrzip = NULL;

    o->sno_count = 0;
    o->sno_pages = 0;
    o->sno_msec = 0;
    o->sno_series = 0;
    o->sno_size = 0;
    o->sno_wcatch = NULL;
    o->sno_rctrl = NULL;

    char work_fn[1024];
    char widx_fn[1024];
    switch (o->style)
    {
    case SIS_DISK_TYPE_SIC: 
        {
            int nyear = o->open_date / 10000;
            sis_sprintf(work_fn, 1024, "%s/%s/%s.%d/%d.%s",
                    o->fpath, o->fname, SIS_DISK_SIC_CHAR, nyear, o->open_date, SIS_DISK_SIC_CHAR);
            // 这里虽然没有设置压缩方式 实际传入的数据就已经按增量压缩好了
            o->work_fps->main_head.index = 1;
            o->work_fps->max_page_size = SIS_DISK_MAXLEN_SICPAGE;
            o->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
            o->work_fps->main_head.zip = SIS_DISK_ZIP_INCRZIP;
            sis_sprintf(widx_fn, 1024, "%s/%s/%s.%d/%d.%s",
                    o->fpath, o->fname, SIS_DISK_SIC_CHAR, nyear, o->open_date, SIS_DISK_IDX_CHAR);
        }
        break;
    case SIS_DISK_TYPE_SNO: 
        {
            int nyear = o->open_date / 10000;
            sis_sprintf(work_fn, 1024, "%s/%s/%s.%d/%d.%s",
                    o->fpath, o->fname, SIS_DISK_SNO_CHAR, nyear, o->open_date, SIS_DISK_SNO_CHAR);
            // 这里虽然没有设置压缩方式 实际传入的数据就已经按增量压缩好了
            o->work_fps->main_head.index = 1;
            o->work_fps->max_page_size = SIS_DISK_MAXLEN_SNOPAGE;
            o->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
            o->work_fps->main_head.zip = SIS_DISK_ZIP_SNAPPY;
            sis_sprintf(widx_fn, 1024, "%s/%s/%s.%d/%d.%s",
                    o->fpath, o->fname, SIS_DISK_SNO_CHAR, nyear, o->open_date, SIS_DISK_IDX_CHAR);
        }
        break;
    case SIS_DISK_TYPE_SDB_YEAR:  // name/year/2000.sdb
        {
            // int nyear = o->open_date / 100000 * 10;
            // int open_date = (nyear + 0) * 10000 +  101;
            // int stop_date = (nyear + 9) * 10000 + 1231;
            // sis_sprintf(work_fn, 1024, "%s/%s/%s/%04d-%04d.%s",
            //         o->fpath, o->fname, SIS_DISK_YEAR_CHAR, open_date / 10000, stop_date / 10000, SIS_DISK_SDB_CHAR);
            // o->work_fps->main_head.index = 1;
            // o->work_fps->max_page_size = SIS_DISK_MAXLEN_SDBPAGE;
            // o->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
            // o->work_fps->main_head.zip = SIS_DISK_ZIP_SNAPPY;
            // sis_sprintf(widx_fn, 1024, "%s/%s/%s/%04d-%04d.%s",
            //         o->fpath, o->fname, SIS_DISK_YEAR_CHAR, open_date / 10000, stop_date / 10000, SIS_DISK_IDX_CHAR);
            int nyear = o->open_date / 10000;
            sis_sprintf(work_fn, 1024, "%s/%s/%s/%04d.%s",
                    o->fpath, o->fname, SIS_DISK_YEAR_CHAR, nyear, SIS_DISK_SDB_CHAR);
            o->work_fps->main_head.index = 1;
            o->work_fps->max_page_size = SIS_DISK_MAXLEN_SDBPAGE;
            o->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
            o->work_fps->main_head.zip = SIS_DISK_ZIP_SNAPPY;
            sis_sprintf(widx_fn, 1024, "%s/%s/%s/%04d.%s",
                    o->fpath, o->fname, SIS_DISK_YEAR_CHAR, nyear, SIS_DISK_IDX_CHAR);
        }
        break;
    case SIS_DISK_TYPE_SDB_DATE:  // name/date/2021/20210606.sdb
        {
            // 初始化小于日期时序的文件
            int nyear = o->open_date / 10000;
            sis_sprintf(work_fn, 1024, "%s/%s/%s/%d/%d.%s",
                    o->fpath, o->fname, SIS_DISK_DATE_CHAR, nyear, o->open_date, SIS_DISK_SDB_CHAR);
            o->work_fps->main_head.index = 1;
            o->work_fps->max_page_size = SIS_DISK_MAXLEN_SDBPAGE;
            o->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
            o->work_fps->main_head.zip = SIS_DISK_ZIP_SNAPPY;
            sis_sprintf(widx_fn, 1024, "%s/%s/%s/%d/%d.%s",
                    o->fpath, o->fname, SIS_DISK_DATE_CHAR, nyear, o->open_date, SIS_DISK_IDX_CHAR);
        }
        break;
    case SIS_DISK_TYPE_SDB_NOTS: // name/nots/name.sdb
        {
            sis_sprintf(work_fn, 1024, "%s/%s/%s/%s.%s",
                    o->fpath, o->fname, SIS_DISK_NOTS_CHAR, o->fname, SIS_DISK_SDB_CHAR);
            // 这里虽然没有设置压缩方式 实际传入的数据就已经按增量压缩好了
            o->work_fps->main_head.index = 1;
            o->work_fps->max_page_size = SIS_DISK_MAXLEN_SDBPAGE;
            o->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
            o->work_fps->main_head.zip = SIS_DISK_ZIP_SNAPPY;
            sis_sprintf(widx_fn, 1024, "%s/%s/%s/%s.%s",
                    o->fpath, o->fname, SIS_DISK_NOTS_CHAR, o->fname, SIS_DISK_IDX_CHAR);
        }
        break;
    case SIS_DISK_TYPE_SDB: // name/name.sdb
        {
            sis_sprintf(work_fn, 1024, "%s/%s/%s.%s",
                    o->fpath, o->fname, o->fname, SIS_DISK_MAP_CHAR);
            // 这里虽然没有设置压缩方式 实际传入的数据就已经按增量压缩好了
            o->work_fps->main_head.index = 0;
            o->work_fps->max_page_size = SIS_DISK_MAXLEN_SDBPAGE;
            o->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
            o->work_fps->main_head.zip = SIS_DISK_ZIP_NOZIP;
        }
        break;
    default: // SIS_DISK_TYPE_LOG
        // 其他类型的文件直接传入文件名 用户可自定义类型
        {
            sis_sprintf(work_fn, 1024, "%s/%s/%d.%s", o->fpath, o->fname, o->open_date, SIS_DISK_LOG_CHAR);
            o->work_fps->max_page_size = 0; // LOG文件有数据直接写
            o->work_fps->max_file_size = 0; // 顺序读写的文件 不需要设置最大值
            o->work_fps->main_head.zip = SIS_DISK_ZIP_NOZIP;
        }
        break;
    }
    // 工作文件和索引文件保持同样的随机码 应该创建时才生成    
    sis_disk_files_init(o->work_fps, work_fn);

    if (o->work_fps->main_head.index)
    {
       // 初始化无时序的索引文件
        o->widx_fps->main_head.style = SIS_DISK_TYPE_SDB_IDX;
        o->widx_fps->max_page_size = SIS_DISK_MAXLEN_IDXPAGE;
        o->widx_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
        o->widx_fps->main_head.zip = SIS_DISK_ZIP_NOZIP;
        o->widx_fps->main_head.wdate = o->work_fps->main_head.wdate;
        sis_disk_files_init(o->widx_fps, widx_fn);
    }
}

s_sis_disk_ctrl *sis_disk_ctrl_create(int style_, const char *fpath_, const char *fname_, int wdate_)
{
    s_sis_disk_ctrl *o = SIS_MALLOC(s_sis_disk_ctrl, o);
    o->fpath = sis_sdsnew(fpath_);
    o->fname = sis_sdsnew(fname_);
    o->style = style_;
    o->open_date = wdate_ == 0 ? sis_time_get_idate(0) : wdate_;
    o->stop_date = o->open_date;
    o->status = SIS_DISK_STATUS_CLOSED;    
    o->isstop  = false;

    o->map_kdicts = sis_map_list_create(sis_disk_kdict_destroy);
    o->map_sdicts = sis_map_list_create(sis_disk_sdict_destroy);
    
    o->new_kinfos = sis_pointer_list_create();
    o->new_kinfos->vfree = sis_object_destroy;
    o->new_sinfos = sis_pointer_list_create();
    o->new_sinfos->vfree = sis_dynamic_db_destroy;

    o->wcatch = sis_disk_wcatch_create(NULL, NULL);
    o->rcatch = sis_disk_rcatch_create(NULL);

    o->work_fps = sis_disk_files_create();

    o->widx_fps = sis_disk_files_create();
    o->map_idxs = sis_map_list_create(sis_disk_idx_destroy);

    o->sdb_incrzip = sis_incrzip_class_create();

    o->map_maps = sis_map_list_create(sis_disk_map_destroy);
    _disk_ctrl_init(o);

    return o;
}

void sis_disk_ctrl_destroy(void *cls_)
{
    s_sis_disk_ctrl *ctrl = (s_sis_disk_ctrl *)cls_;
    sis_sdsfree(ctrl->fpath);
    sis_sdsfree(ctrl->fname);

    sis_map_list_destroy(ctrl->map_kdicts);
    sis_map_list_destroy(ctrl->map_sdicts);

    sis_pointer_list_destroy(ctrl->new_kinfos);
    sis_pointer_list_destroy(ctrl->new_sinfos);

    sis_disk_wcatch_destroy(ctrl->wcatch);
    sis_disk_rcatch_destroy(ctrl->rcatch);

    sis_disk_files_destroy(ctrl->work_fps);

    sis_incrzip_class_destroy(ctrl->sdb_incrzip);

    sis_map_list_destroy(ctrl->map_idxs);
    sis_disk_files_destroy(ctrl->widx_fps);

    sis_map_list_destroy(ctrl->map_maps);

    sis_free(ctrl);
}

void sis_disk_ctrl_clear(s_sis_disk_ctrl *cls_)
{
    // 创建时的信息不能删除 其他信息可清理
    sis_map_list_clear(cls_->map_kdicts);
    sis_map_list_clear(cls_->map_sdicts);

    sis_pointer_list_clear(cls_->new_kinfos);
    sis_pointer_list_clear(cls_->new_sinfos);

    sis_disk_wcatch_clear(cls_->wcatch);
    sis_disk_rcatch_clear(cls_->rcatch);

    sis_disk_files_clear(cls_->work_fps);

    sis_incrzip_class_clear(cls_->sdb_incrzip);

    sis_map_list_clear(cls_->map_idxs);
    sis_disk_files_clear(cls_->widx_fps);

    _disk_ctrl_init(cls_);
}

int sis_disk_ctrl_work_zipmode(s_sis_disk_ctrl *cls_)
{
    return cls_->work_fps ? cls_->work_fps->main_head.zip : SIS_DISK_ZIP_NOZIP;
}
int sis_disk_ctrl_widx_zipmode(s_sis_disk_ctrl *cls_)
{
    return cls_->widx_fps ? cls_->widx_fps->main_head.zip != SIS_DISK_ZIP_NOZIP ? SIS_DISK_ZIP_SNAPPY : SIS_DISK_ZIP_NOZIP : SIS_DISK_ZIP_NOZIP;
}
void sis_disk_ctrl_set_size(s_sis_disk_ctrl *cls_,size_t fsize_, size_t psize_)
{
    cls_->work_fps->max_file_size = fsize_;
    cls_->work_fps->max_page_size = psize_;
    if (cls_->style == SIS_DISK_TYPE_LOG)
    {
        cls_->work_fps->max_file_size = 0;
    }
}

// 检查文件是否有效
int sis_disk_ctrl_valid_work(s_sis_disk_ctrl *cls_)
{
    // 通常判断work和index的尾部是否一样 一样表示完整 否则
    // 检查work file 是否完整 如果不完整就设置最后一个块的位置 并重建索引
    // 如果work file 完整 就检查索引文件是否完整 不完整就重建索引
    // 重建失败就返回 1
    return SIS_DISK_CMD_OK;
}

int sis_disk_ctrl_valid_widx(s_sis_disk_ctrl *cls_)
{
    if (cls_->work_fps->main_head.index)
    {
        if (!sis_file_exists(cls_->widx_fps->cur_name))
        {
            LOG(5)("idxfile no exists.[%s]\n", cls_->widx_fps->cur_name);
            return SIS_DISK_CMD_NO_EXISTS_IDX;
        }
        return SIS_DISK_CMD_OK;
    }
    return SIS_DISK_CMD_NO_IDX;
}

int sis_disk_ctrl_read_kdict(s_sis_disk_ctrl *cls_, s_sis_disk_idx *node_)
{
    if (!node_)
    {
        return -1;
    }
    for (int k = 0; k < node_->idxs->count; k++)
    {
        s_sis_disk_idx_unit *unit = sis_disk_idx_get_unit(node_, k);
        sis_disk_rcatch_init_of_idx(cls_->rcatch, unit);
        if (sis_disk_files_read_fromidx(cls_->work_fps, cls_->rcatch) > 0)
        {
            sis_disk_ctrl_unzip_sdb(cls_, cls_->rcatch);
            sis_disk_reader_set_kdict(cls_->map_kdicts, sis_memory(cls_->rcatch->memory), sis_memory_get_size(cls_->rcatch->memory));
        }
    }
    return 0;
}

int sis_disk_ctrl_read_sdict(s_sis_disk_ctrl *cls_, s_sis_disk_idx *node_)
{
    if (!node_)
    {
        return -1;
    }
    for (int k = 0; k < node_->idxs->count; k++)
    {
        s_sis_disk_idx_unit *unit = sis_disk_idx_get_unit(node_, k);
        sis_disk_rcatch_init_of_idx(cls_->rcatch, unit);
        if (sis_disk_files_read_fromidx(cls_->work_fps, cls_->rcatch) > 0)
        {
            sis_disk_ctrl_unzip_sdb(cls_, cls_->rcatch);
            sis_disk_reader_set_sdict(cls_->map_sdicts, sis_memory(cls_->rcatch->memory), sis_memory_get_size(cls_->rcatch->memory));
        }
    }
    return 0;
}
void sis_disk_ctrl_cmp_kdict(s_sis_disk_ctrl *munit_, s_sis_disk_ctrl *sunit_)
{
    // printf("==kdict==, %d\n", sunit_->new_kinfos->count);
    sis_pointer_list_clear(sunit_->new_kinfos);
    int count = sis_map_list_getsize(munit_->map_kdicts);
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_kdict *mkdict = sis_map_list_geti(munit_->map_kdicts, i);
        sis_disk_ctrl_set_kdict(sunit_, SIS_OBJ_SDS(mkdict->name));
    }  
    // printf("==kdict==, %d %d %d\n", sunit_->work_fps->main_head.style, sunit_->new_kinfos->count, sis_map_list_getsize(sunit_->map_kdicts));
}
// 和上级munit_ 全字典比较 sunit 把需要更新的放入new中等待写盘
// 只检查sdict 因为 kdict 会随时写入 也就是说每个文件的key字典都不会相同 读取时根据自己的字典得到数据
// 上层表只是起到检索某个key有没有 某个结构有没有的作用
void sis_disk_ctrl_cmp_sdict(s_sis_disk_ctrl *munit_, s_sis_disk_ctrl *sunit_)
{
    // printf("==sdict==, %d\n", sunit_->new_sinfos->count);
    sis_pointer_list_clear(sunit_->new_sinfos);
    int count = sis_map_list_getsize(munit_->map_sdicts);
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_sdict *msdict = sis_map_list_geti(munit_->map_sdicts, i);
        s_sis_dynamic_db *msdb = sis_disk_sdict_last(msdict);
        int scale = sis_disk_get_sdb_scale(msdb);
        if (scale == SIS_SDB_SCALE_YEAR)
        {
            if (sunit_->work_fps->main_head.style == SIS_DISK_TYPE_SDB_YEAR)
            {
                sis_disk_ctrl_set_sdict(sunit_, msdb);
            }
        }
        else if (scale == SIS_SDB_SCALE_DATE )
        {
            if (sunit_->work_fps->main_head.style == SIS_DISK_TYPE_SDB_DATE)
            {
                sis_disk_ctrl_set_sdict(sunit_, msdb);
            }
        }
        else // if (scale == SIS_SDB_SCALE_NOTS)
        {
            if (sunit_->work_fps->main_head.style == SIS_DISK_TYPE_SDB_NOTS)
            {
                sis_disk_ctrl_set_sdict(sunit_, msdb);
            }
        }
    } 
    // printf("==sdict==,%d %d %d\n", sunit_->work_fps->main_head.style, sunit_->new_sinfos->count,sis_map_list_getsize(sunit_->map_sdicts));   
}
// 写字典
size_t sis_disk_io_write_dict(s_sis_disk_ctrl *cls_, s_sis_disk_wcatch *wcatch_)
{
    size_t osize = 0;
    switch (cls_->work_fps->main_head.style)
    {
    case SIS_DISK_TYPE_SIC:
        osize = sis_disk_io_write_sic_work(cls_, wcatch_);
        break;
    case SIS_DISK_TYPE_SNO:
        osize = sis_disk_io_write_sdb_work(cls_, wcatch_);
        break;
    case SIS_DISK_TYPE_SDB:
    case SIS_DISK_TYPE_SDB_NOTS:
    case SIS_DISK_TYPE_SDB_YEAR:
    case SIS_DISK_TYPE_SDB_DATE:
        osize = sis_disk_io_write_sdb_work(cls_, wcatch_);
        break;
    default:
        break;
    }
    return osize;
}

void sis_disk_ctrl_write_kdict(s_sis_disk_ctrl *cls_)
{
    s_sis_sds msg = NULL;
    for (int i = 0; i < cls_->new_kinfos->count; i++)
    {
        s_sis_object *obj = sis_pointer_list_get(cls_->new_kinfos, i);
        if (msg)
        {
            msg = sis_sdscatfmt(msg, ",%S", SIS_OBJ_SDS(obj));
        }
        else
        {
            msg = sis_sdsnew(SIS_OBJ_SDS(obj));
        }     
    }
    if (msg)
    {
        s_sis_object *mapobj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(SIS_DISK_SIGN_KEY));
        s_sis_disk_wcatch *wcatch = sis_disk_wcatch_create(mapobj, NULL);
        sis_memory_cat(wcatch->memory, msg, sis_sdslen(msg));
        wcatch->head.hid = SIS_DISK_HID_DICT_KEY;
        // wcatch->head.zip = SIS_DISK_ZIP_SNAPPY;
        sis_disk_io_write_dict(cls_, wcatch);
        sis_disk_wcatch_destroy(wcatch);
        sis_object_destroy(mapobj);
        sis_sdsfree(msg);
    }
    sis_pointer_list_clear(cls_->new_kinfos);
}
void sis_disk_ctrl_write_sdict(s_sis_disk_ctrl *cls_)
{
    if (cls_->new_sinfos->count < 1)
    {
        return ;
    }
    // ??? 这里可能有同名的结构体存在 以后有时间再处理
    // 可以一条一条存 也可以把重名的留到第二次再写 直到没有
    s_sis_sds msg = NULL;
    {
        s_sis_json_node *sdbs_node = sis_json_create_object();
        for (int i = 0; i < cls_->new_sinfos->count; i++)
        {
            s_sis_dynamic_db *sdb = sis_pointer_list_get(cls_->new_sinfos, i);
            sis_json_object_add_node(sdbs_node, sdb->name, sis_sdbinfo_to_json(sdb)); 
        }
        msg = sis_json_to_sds(sdbs_node, true);
        sis_json_delete_node(sdbs_node);
    }
    if (msg)
    {
        s_sis_object *mapobj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(SIS_DISK_SIGN_SDB));
        s_sis_disk_wcatch *wcatch = sis_disk_wcatch_create(mapobj, NULL);
        sis_memory_cat(wcatch->memory, msg, sis_sdslen(msg));
        wcatch->head.hid = SIS_DISK_HID_DICT_SDB;
        // wcatch->head.zip = SIS_DISK_ZIP_SNAPPY;
        sis_disk_io_write_dict(cls_, wcatch);
        sis_disk_wcatch_destroy(wcatch);
        sis_object_destroy(mapobj);
        sis_sdsfree(msg);
    }  
    sis_pointer_list_clear(cls_->new_sinfos);
}

// 读取keys信息
s_sis_sds sis_disk_ctrl_get_keys_sds(s_sis_disk_ctrl *cls_)
{
    s_sis_sds msg = NULL;
    int count = sis_map_list_getsize(cls_->map_kdicts);
    for(int i = 0; i < count; i++)
    {
        s_sis_disk_kdict *kdict = (s_sis_disk_kdict *)sis_map_list_geti(cls_->map_kdicts, i);
        if (msg)
        {
            msg = sis_sdscatfmt(msg, ",%S", SIS_OBJ_SDS(kdict->name));
        }
        else
        {
            msg = sis_sdsnew(SIS_OBJ_SDS(kdict->name));
        }    
    }
    return msg;
}
// 读取sdbs信息
s_sis_sds sis_disk_ctrl_get_sdbs_sds(s_sis_disk_ctrl *cls_)
{
    int count = sis_map_list_getsize(cls_->map_sdicts);
    s_sis_json_node *sdbs_node = sis_json_create_object();
    {
        for(int i = 0; i < count; i++)
        {
            s_sis_disk_sdict *sdict = (s_sis_disk_sdict *)sis_map_list_geti(cls_->map_sdicts, i);
            s_sis_dynamic_db *sdb = sis_disk_sdict_last(sdict);
            sis_json_object_add_node(sdbs_node, SIS_OBJ_SDS(sdict->name), sis_sdbinfo_to_json(sdb));
        }
    }
    s_sis_sds msg = sis_json_to_sds(sdbs_node, true);
    sis_json_delete_node(sdbs_node);
    return msg;
}

s_sis_disk_kdict *sis_disk_ctrl_set_kdict(s_sis_disk_ctrl *cls_, const char *kname_)
{
    s_sis_disk_kdict *kdict = sis_disk_map_get_kdict(cls_->map_kdicts, kname_);
    if (!kdict)
    {
        kdict = sis_disk_kdict_create(kname_);
        kdict->index = sis_map_list_set(cls_->map_kdicts, kname_, kdict);
        sis_object_incr(kdict->name);
        sis_pointer_list_push(cls_->new_kinfos, kdict->name);
    }
    return kdict;
}

// 需要判断时序
s_sis_disk_sdict *sis_disk_ctrl_set_sdict(s_sis_disk_ctrl *cls_, s_sis_dynamic_db *sdb_)
{
    s_sis_disk_sdict *sdict = sis_disk_map_get_sdict(cls_->map_sdicts, sdb_->name);
    if (!sdict)
    {
        sdict = sis_disk_sdict_create(sdb_->name);
        sdict->index = sis_map_list_set(cls_->map_sdicts, sdb_->name, sdict);
    }
    if (sis_disk_sdict_isnew(sdict, sdb_))
    {
        sis_dynamic_db_incr(sdb_);
        sis_pointer_list_push(sdict->sdbs, sdb_);
        // 先去重 这里只处理了未写数据前重复问题 默认一写数据就更新写入结构体
        int index = 0;
        while(index < cls_->new_sinfos->count)
        {
            s_sis_dynamic_db *sdb = (s_sis_dynamic_db *)sis_pointer_list_get(cls_->new_sinfos, index);
            if (!sis_strcasecmp(sdb->name, sdb_->name))
            {
                sis_pointer_list_delete(cls_->new_sinfos, index, 1);
            }
            else
            {
                index++;
            }
        }
        sis_dynamic_db_incr(sdb_);        
        sis_pointer_list_push(cls_->new_sinfos, sdb_);
    }
    return sdict;   
}
size_t sis_disk_ctrl_unzip(s_sis_disk_ctrl *cls_, s_sis_disk_head *head_, char *imem_, size_t isize_, s_sis_memory *out_)
{
    sis_memory_clear(out_);
    size_t size = 0;
    if (head_->zip == SIS_DISK_ZIP_NOZIP)
    {
        sis_memory_cat(out_, imem_, isize_);
        return sis_memory_get_size(out_);
    }
    if (head_->zip == SIS_DISK_ZIP_SNAPPY)
    {
        if(sis_snappy_uncompress(imem_, isize_, out_))
        {
            size = sis_memory_get_size(out_);
        }
        else
        {
            LOG(5)("snappy_uncompress fail. %d %zu \n", head_->hid, isize_);
        }
    }
    return size;
}
size_t sis_disk_ctrl_unzip_sdb(s_sis_disk_ctrl *cls_, s_sis_disk_rcatch *rcatch_)
{
    size_t size = 0;
    if (rcatch_->head.zip == SIS_DISK_ZIP_NOZIP)
    {
        return sis_memory_get_size(rcatch_->memory);
    }
    if (rcatch_->head.zip == SIS_DISK_ZIP_SNAPPY)
    {
        s_sis_memory *memory = sis_memory_create();
        if(sis_snappy_uncompress(sis_memory(rcatch_->memory), sis_memory_get_size(rcatch_->memory), memory))
        {
            size = sis_memory_get_size(memory);
            sis_memory_swap(rcatch_->memory, memory);
        }
        else
        {
            LOG(5)("snappy_uncompress fail. %d %zu \n", rcatch_->head.hid, sis_memory_get_size(rcatch_->memory));
        }
        sis_memory_destroy(memory);
        // switch (rcatch_->rinfo->ktype)
        // {
        // case SIS_SDB_STYLE_MUL:
        // {
        //     if (!rcatch_->mlist)
        //     {
        //         rcatch_->mlist = sis_pointer_list_create();
        //         rcatch_->mlist->vfree = sis_sdsfree_call;
        //     }
        //     sis_pointer_list_clear(rcatch_->mlist);
        //     rcatch_->kidx = sis_memory_get_ssize(rcatch_->memory);
        //     int count = sis_memory_get_ssize(rcatch_->memory);
        //     for (int i = 0; i < count; i++)
        //     {
        //         int size = sis_memory_get_ssize(rcatch_->memory);
        //         s_sis_sds item = sis_sdsnewlen(sis_memory(rcatch_->memory), size);
        //         sis_memory_move(rcatch_->memory, size);
        //         sis_pointer_list_push(rcatch_->mlist, item);
        //     }
        //     size = count;
        // }
        // break;
        // case SIS_SDB_STYLE_ONE:
        // {
        //     rcatch_->kidx = sis_memory_get_ssize(rcatch_->memory);
        //     size = sis_memory_get_ssize(rcatch_->memory);
        //     if (size != sis_memory_get_size(rcatch_->memory))
        //     {
        //         size = 0;
        //     }
        //     // 已经移动到数据区
        // }
        // break;
        // default: // SIS_SDB_STYLE_NON
        // {
        //     sis_out_binary("unzip_fromidx", sis_memory(rcatch_->memory), sis_memory_get_size(rcatch_->memory));
        //     rcatch_->kidx = sis_memory_get_ssize(rcatch_->memory);
        //     rcatch_->sidx = sis_memory_get_ssize(rcatch_->memory);
        //     sis_out_binary("unzip_fromidx", sis_memory(rcatch_->memory), sis_memory_get_size(rcatch_->memory));
        //     // 已经移动到数据区
        //     size = sis_memory_get_size(rcatch_->memory);
        // }
        // break;
        // }
        return size;
    }
    return size;
}

// 只读数据
int sis_disk_ctrl_read_start(s_sis_disk_ctrl *cls_)
{
    if (!cls_->style)
    {
        return SIS_DISK_CMD_NO_INIT;
    }
    // 对已经存在的文件进行合法性检查 如果文件不完整 就打开失败 由外部程序来处理异常，这样相对安全
    if (!sis_file_exists(cls_->work_fps->cur_name))
    {
        LOG(5)("workfile no exists.[%s]\n", cls_->work_fps->cur_name);
        return SIS_DISK_CMD_NO_EXISTS;
    }
    if (cls_->work_fps->main_head.index)
    {
        if (!sis_file_exists(cls_->widx_fps->cur_name))
        {
            LOG(5)("idxfile no exists.[%s]\n", cls_->widx_fps->cur_name);
            return SIS_DISK_CMD_NO_EXISTS_IDX;
        }
    }
    // printf("-7--ss-- %d %d\n", cls_->work_fps->main_head.wdate, SIS_DISK_IS_SDB(cls_->style));
    int o = sis_disk_files_open(cls_->work_fps, SIS_DISK_ACCESS_RDONLY);
    if (o)
    {
        LOG(5)("open file fail.[%s:%d]\n", cls_->work_fps->cur_name, o);
        return SIS_DISK_CMD_NO_OPEN;
    }
    if (SIS_DISK_IS_SDB(cls_->style))
    {
        o = sis_disk_io_read_sdb_widx(cls_);
        int count = sis_map_list_getsize(cls_->map_idxs);
        for (int i = 0; i < count; i++)
        {
            s_sis_disk_idx *rnode = (s_sis_disk_idx *)sis_map_list_geti(cls_->map_idxs, i);
            printf("::::: %d %s %s\n", i, SIS_OBJ_GET_CHAR(rnode->kname), SIS_OBJ_GET_CHAR(rnode->sname));
            for (int k = 0; k < rnode->idxs->count; k++)
            {
                s_sis_disk_idx_unit *unit = (s_sis_disk_idx_unit *)sis_struct_list_get(rnode->idxs, k);
                printf(":::== %d %lld %lld\n", k, unit->offset, unit->size); 
            }     
        } 
        if (o != SIS_DISK_CMD_OK)
        {
            return o;
        }
    }
    else if (cls_->style == SIS_DISK_TYPE_SDB)
    {
        // 读取key和sdb定义信息
        o = sis_disk_io_read_sdb_map(cls_);
        if (o != SIS_DISK_CMD_NO_IDX)
        {
            return o;
        }
    }    
    else if (cls_->style == SIS_DISK_TYPE_SIC)
    {
        o = sis_disk_io_read_sic_widx(cls_);
        if (o != SIS_DISK_CMD_OK)
        {
            return o;
        }
    } 
    else if (cls_->style == SIS_DISK_TYPE_SNO)
    {
        o = sis_disk_io_read_sno_widx(cls_);
        if (o != SIS_DISK_CMD_OK)
        {
            return o;
        }
    }    
    cls_->status = SIS_DISK_STATUS_OPENED;
    return SIS_DISK_CMD_OK;
}

// 关闭文件
int sis_disk_ctrl_read_stop(s_sis_disk_ctrl *cls_)
{
   if (cls_->status == SIS_DISK_STATUS_CLOSED)
    {
        return 0;
    }
    // 根据文件类型写索引，并关闭文件
    sis_disk_files_close(cls_->work_fps);
    if (cls_->work_fps->main_head.index)
    {
        sis_disk_files_close(cls_->widx_fps);
    }
    sis_disk_ctrl_clear(cls_);
    cls_->status = SIS_DISK_STATUS_CLOSED; 

    return 0;
}

int sis_disk_ctrl_write_start(s_sis_disk_ctrl *cls_)
{
    if (!cls_->style)
    {
        return SIS_DISK_CMD_NO_INIT;
    }
    int access = SIS_DISK_ACCESS_APPEND;
    if (!sis_file_exists(cls_->work_fps->cur_name))
    {
        access = SIS_DISK_ACCESS_CREATE;
    }
    // printf("-5--ss-- %s %d\n", cls_->work_fps->cur_name, cls_->open_date);
    if (access == SIS_DISK_ACCESS_CREATE)
    {
        // 工作文件和索引文件保持同样的随机码 应该创建时才生成
        // 只有创建时重新生成
        sis_str_get_random(cls_->work_fps->main_tail.crc, 16);
        if (cls_->widx_fps)
        {
            memmove(cls_->widx_fps->main_tail.crc, cls_->work_fps->main_tail.crc, 16);
        }
        // 以新建新文件的方式打开文件 如果以前有文件直接删除创建新的
        if (sis_disk_files_open(cls_->work_fps, SIS_DISK_ACCESS_CREATE))
        {
            LOG(2)("create file fail. [%s]\n", cls_->work_fps->cur_name);
            return SIS_DISK_CMD_NO_CREATE;
        }
    }
    else
    {
        // 以在文件后面追加数据的方式打开文件
        // 对已经存在的文件进行合法性检查 如果文件不完整 就打开失败 由外部程序来处理异常，这样相对安全
        int vo = sis_disk_ctrl_valid_work(cls_);
        if ( vo != SIS_DISK_CMD_OK)
        {
            LOG(5)("work is no valid.[%s]\n", cls_->work_fps->cur_name);
            return SIS_DISK_CMD_NO_VAILD;
        }
        if (sis_disk_files_open(cls_->work_fps, SIS_DISK_ACCESS_APPEND))
        {
            LOG(5)("open file fail.[%s]\n", cls_->work_fps->cur_name);
            return SIS_DISK_CMD_NO_OPEN;
        }    
        if (SIS_DISK_IS_SDB(cls_->style))
        {
            vo = sis_disk_io_read_sdb_widx(cls_);
            if (vo != SIS_DISK_CMD_OK)
            {
                return vo;
            }
        }
        else if (cls_->style == SIS_DISK_TYPE_SDB)
        {
            // 读取key和sdb定义信息
            vo = sis_disk_io_read_sdb_map(cls_);
            if (vo != SIS_DISK_CMD_NO_IDX)
            {
                return vo;
            }
        } 
        else if (cls_->style == SIS_DISK_TYPE_SIC)
        {
            vo = sis_disk_io_read_sic_widx(cls_);
            if (vo != SIS_DISK_CMD_OK)
            {
                return vo;
            }
        } 
        else if (cls_->style == SIS_DISK_TYPE_SNO)
        {
            vo = sis_disk_io_read_sno_widx(cls_);
            if (vo != SIS_DISK_CMD_OK)
            {
                return vo;
            }
        } 
    } 
    // 打开已有文件会先加载字典 写字典表如果发现和加载的字典表一样就不重复写入

    // 写文件前强制修正一次写入位置
    if (!sis_disk_files_seek(cls_->work_fps))
    {
        LOG(5)("seek file fail.[%s]\n", cls_->work_fps->cur_name);
        return -8;
    }
    cls_->status = SIS_DISK_STATUS_OPENED;
    return 0;
}

// 写入结束标记，并生成索引文件
// 这里要判断是否需要自动pack 当无效数据大于 60% 时就执行pack
int sis_disk_ctrl_write_stop(s_sis_disk_ctrl *cls_)
{
    if (cls_->status == SIS_DISK_STATUS_CLOSED)
    {
        return 0;
    }
    // 根据文件类型写索引，并关闭文件
    sis_disk_files_write_sync(cls_->work_fps);

    if (cls_->work_fps->main_head.index)
    {
        // printf("-6--ss-- %d %d\n", cls_->work_fps->main_head.wdate, cls_->style);
        cls_->widx_fps->main_head.workers = cls_->work_fps->lists->count;
        // 当前索引就1个文件 以后有需求再说
        // 难点是要先统计索引的时间和热度，以及可以加载内存的数据量 来计算出把索引分为几个
        // 具体思路是 设定内存中给索引 1G 那么根据索引未压缩的数据量 按热度先后排序
        // 保留经常访问的数据到0号文件 其他分别放到1或者2号....
        // open后就会写索引头
        // 索引文件每次都重新写
        if (sis_disk_files_open(cls_->widx_fps, SIS_DISK_ACCESS_CREATE))
        {
            LOG(5)("open idxfile fail.[%s] %d\n", cls_->widx_fps->cur_name, cls_->widx_fps->main_head.workers);
            return -2;
        }
        if (SIS_DISK_IS_SDB(cls_->style))
        {
            if (sis_disk_io_write_sdb_widx(cls_) == 0)
            {
                LOG(5)("write sdbidx fail.[%s]\n", cls_->widx_fps->cur_name);
                return -3;
            }
        }
        else if (cls_->style == SIS_DISK_TYPE_SIC)
        {
            if (sis_disk_io_write_sic_widx(cls_) == 0)
            {
                LOG(5)("write netidx fail.[%s]\n", cls_->widx_fps->cur_name);
                return -4;
            }
        } 
        else if (cls_->style == SIS_DISK_TYPE_SNO)
        {
            if (sis_disk_io_write_sno_widx(cls_) == 0)
            {
                LOG(5)("write snoidx fail.[%s]\n", cls_->widx_fps->cur_name);
                return -4;
            }
        } 
        sis_disk_files_close(cls_->widx_fps);
    }
    // 必须等索引写好才关闭工作句柄
    sis_disk_files_close(cls_->work_fps);

    sis_disk_ctrl_clear(cls_);

    cls_->status = SIS_DISK_STATUS_CLOSED;
    return 0;
}


void sis_disk_ctrl_remove(s_sis_disk_ctrl *cls_)
{
    int count = sis_disk_files_remove(cls_->work_fps);
    if (cls_->widx_fps)
    {
        count += sis_disk_files_remove(cls_->widx_fps);
    }
    LOG(5)("delete file count = %d.\n", count);
}

int sis_disk_ctrl_pack(s_sis_disk_ctrl *src_, s_sis_disk_ctrl *des_)
{
    if (src_->style == SIS_DISK_TYPE_LOG || src_->style == SIS_DISK_TYPE_SIC || src_->style == SIS_DISK_TYPE_SNO)
    {
        return 0;
    }
    // 开始新文件
    sis_disk_ctrl_remove(des_);
    // 先初始化原文件 读索引
    sis_disk_ctrl_read_start(src_);
    // 再初始化目标文件 准备工作
    sis_disk_ctrl_write_start(des_);
    // 然后遍历索引 读取一块就写入一块
    int count = sis_map_list_getsize(src_->map_idxs);
    s_sis_disk_rcatch *rcatch = src_->rcatch;
    s_sis_disk_wcatch *wcatch = des_->wcatch;
    sis_disk_wcatch_init(wcatch);
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_idx *rnode = sis_map_list_geti(src_->map_idxs, i);
        for (int k = 0; k < rnode->idxs->count; k++)
        {
            s_sis_disk_idx_unit *unit = (s_sis_disk_idx_unit *)sis_struct_list_get(rnode->idxs, k);
            sis_disk_rcatch_init_of_idx(rcatch, unit);
            sis_disk_io_read_sdb(src_, rcatch);
            memmove(&wcatch->head, &rcatch->head, sizeof(s_sis_disk_head));
            memmove(&wcatch->winfo, rcatch->rinfo, sizeof(s_sis_disk_idx_unit));
            sis_memory_swap(wcatch->memory, rcatch->memory);
            sis_disk_files_write_saveidx(des_->work_fps, wcatch); 
            // 更新索引
            s_sis_disk_idx *wnode = sis_disk_idx_get(des_->map_idxs, rnode->kname, rnode->sname);
            sis_disk_idx_set_unit(wnode, &wcatch->winfo);
        }        
    } 
    // 关闭原文件
    sis_disk_ctrl_read_stop(src_);
    // 写目标索引 和 文件尾 关闭文件
    sis_disk_ctrl_write_stop(des_);
    return 1;
}

int sis_disk_ctrl_merge(s_sis_disk_ctrl *src_)
{
    if (src_->style == SIS_DISK_TYPE_LOG || src_->style == SIS_DISK_TYPE_SIC || src_->style == SIS_DISK_TYPE_SNO)
    {
        return 0;
    }
    int nowdate = src_->open_date;
    char despath[255];
    sis_sprintf(despath, 255, "%s/tmp/", src_->fpath);
    s_sis_disk_ctrl *tmp_ = sis_disk_ctrl_create(src_->style, despath, src_->fname, nowdate);

    // 开始新文件
    sis_disk_ctrl_remove(tmp_);
    // 先初始化原文件 读索引
    sis_disk_ctrl_read_start(src_);
    // 再初始化目标文件 准备工作
    sis_disk_ctrl_write_start(tmp_);
    // 然后遍历索引 读取一块就写入一块
    int count = sis_map_list_getsize(src_->map_idxs);
    // printf("=== %s --> %s : %d count = %d\n", src_->fpath, despath, nowdate, count);
    s_sis_disk_rcatch *rcatch = src_->rcatch;
    s_sis_disk_wcatch *wcatch = tmp_->wcatch;
    sis_disk_wcatch_init(wcatch);
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_idx *rnode = sis_map_list_geti(src_->map_idxs, i);
        for (int k = 0; k < rnode->idxs->count; k++)
        {
            s_sis_disk_idx_unit *unit = (s_sis_disk_idx_unit *)sis_struct_list_get(rnode->idxs, k);
            sis_disk_rcatch_init_of_idx(rcatch, unit);
            // printf("=== read: %d %d\n", rcatch->rinfo->offset, rcatch->rinfo->size);
            sis_disk_io_read_sdb(src_, rcatch);
            // printf("=== read: %d %d\n", sis_memory_get_size(rcatch->memory), rcatch->rinfo->size);
            sis_disk_wcatch_setname(wcatch, rnode->kname, rnode->sname);
            memmove(&wcatch->head, &rcatch->head, sizeof(s_sis_disk_head));
            memmove(&wcatch->winfo, rcatch->rinfo, sizeof(s_sis_disk_idx_unit));
            // printf("=== write :%d %d | %zu\n", wcatch->head.hid, wcatch->head.zip, sis_memory_get_size(rcatch->memory));
            // sis_out_binary("..2..", sis_memory(rcatch->memory), sis_memory_get_size(rcatch->memory));
            switch (wcatch->head.hid)
            {
            case SIS_DISK_HID_DICT_KEY:
                sis_memory_cat(wcatch->memory, sis_memory(rcatch->memory), sis_memory_get_size(rcatch->memory));
                sis_disk_io_write_noidx(tmp_->work_fps, wcatch); 
                // sis_disk_files_write_saveidx(tmp_->work_fps, wcatch); 
                s_sis_disk_idx *node = sis_disk_idx_create(NULL, NULL);    
                sis_disk_idx_set_unit(node, &wcatch->winfo);    
                sis_map_list_set(tmp_->map_idxs, SIS_DISK_SIGN_KEY, node);  
                sis_memory_clear(wcatch->memory);              /* code */
                break;
            case SIS_DISK_HID_DICT_SDB:
                {
                    sis_memory_cat(wcatch->memory, sis_memory(rcatch->memory), sis_memory_get_size(rcatch->memory));
                    sis_disk_io_write_noidx(tmp_->work_fps, wcatch); 
                    // sis_disk_files_write_saveidx(tmp_->work_fps, wcatch); 
                    s_sis_disk_idx *node = sis_disk_idx_create(NULL, NULL);    
                    sis_disk_idx_set_unit(node, &wcatch->winfo);    
                    sis_map_list_set(tmp_->map_idxs, SIS_DISK_SIGN_SDB, node);  
                    sis_memory_clear(wcatch->memory);    
                }
                /* code */
                break; 
            case SIS_DISK_HID_MSG_SDB:
            case SIS_DISK_HID_MSG_NON:
                if (k == 0)
                {
                    int kidx = sis_disk_kdict_get_idx(src_->map_kdicts, SIS_OBJ_GET_CHAR(wcatch->kname));
                    int sidx = sis_disk_sdict_get_idx(src_->map_sdicts, SIS_OBJ_GET_CHAR(wcatch->sname));
                    // printf("--- : %d %d\n", kidx, sidx);
                    sis_memory_cat_ssize(wcatch->memory, kidx);
                    sis_memory_cat_ssize(wcatch->memory, sidx);
                }
                sis_memory_cat(wcatch->memory, sis_memory(rcatch->memory), sis_memory_get_size(rcatch->memory));
                break;  
            case SIS_DISK_HID_MSG_SIC:
            case SIS_DISK_HID_SIC_NEW:
            case SIS_DISK_HID_MSG_MAP:
            case SIS_DISK_HID_MSG_ONE:
            case SIS_DISK_HID_MSG_MUL:        
            default:
                break;
            }
        }
        // printf("=== write : %d %s %s %s %s %zu\n", wcatch->head.zip,
        //     SIS_OBJ_GET_CHAR(wcatch->kname), SIS_OBJ_GET_CHAR(wcatch->sname),
        //     SIS_OBJ_GET_CHAR(rnode->kname), SIS_OBJ_GET_CHAR(rnode->sname),
        //     sis_memory_get_size(wcatch->memory));

        // sis_out_binary("..1..", sis_memory(wcatch->memory), sis_memory_get_size(wcatch->memory));
        if (sis_memory_get_size(wcatch->memory) > 0)
        {
            // 更新索引
            sis_disk_io_write_noidx(tmp_->work_fps, wcatch); 
            // sis_disk_files_write_saveidx(tmp_->work_fps, wcatch); 
            s_sis_disk_idx *wnode = sis_disk_idx_get(tmp_->map_idxs, rnode->kname, rnode->sname);
            sis_disk_idx_set_unit(wnode, &wcatch->winfo);
            sis_memory_clear(wcatch->memory);
            // {
            //     printf("=== no idx : %s %s %s %s %zu\n", 
            //         SIS_OBJ_GET_CHAR(wcatch->kname), SIS_OBJ_GET_CHAR(wcatch->sname), 
            //         SIS_OBJ_GET_CHAR(rnode->kname), SIS_OBJ_GET_CHAR(rnode->sname), 
            //         sis_memory_get_size(wcatch->memory));
            // }
        }
    } 
    // 关闭原文件
    sis_disk_ctrl_read_stop(src_);
    // 写目标索引 和 文件尾 关闭文件
    sis_disk_ctrl_write_stop(tmp_);

    sis_disk_ctrl_remove(src_);
    sis_disk_ctrl_move(tmp_, src_->fpath);

    return 1;
}
//??? 这里以后要判断是否改名成功 如果一个失败就全部恢复出来
int sis_disk_ctrl_move(s_sis_disk_ctrl *cls_, const char *path_)
{
    char newfn[255];
    int o = 0;
    for (int  i = 0; i < cls_->work_fps->lists->count; i++)
    {
        s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->work_fps->lists, i);
        sis_str_change(newfn, 255, unit->fn, cls_->fpath, path_);
        // printf("work move :%s %s %s --> %s\n", unit->fn, cls_->fpath, path_, newfn);
        if (!sis_path_exists(newfn))
        {
            sis_path_mkdir(newfn);
        }
        sis_file_rename(unit->fn, newfn);
    }
    if (cls_->work_fps->main_head.index)
    {
        for (int  i = 0; i < cls_->widx_fps->lists->count; i++)
        {
            s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->widx_fps->lists, i);
            sis_str_change(newfn, 255, unit->fn, cls_->fpath, path_);
            // printf("widx move :%s --> %s\n", unit->fn, newfn);
            if (!sis_path_exists(newfn))
            {
                sis_path_mkdir(newfn);
            }
            sis_file_rename(unit->fn, newfn);
        }
    }
    return o;
}
