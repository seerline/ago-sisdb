
#include "sis_disk.h"

////////////////////////////////////////////////////////
// s_sis_disk_ctrl
////////////////////////////////////////////////////////
void _disk_ctrl_init(s_sis_disk_ctrl *o)
{
    o->work_fps->main_head.style = o->style;
    o->work_fps->main_head.index = 0;
    o->work_fps->main_head.wdate = o->open_date;

    o->sno_count = 0;
    o->sno_msec = 0;
    o->sno_zipsize = 0;
    o->sno_incrzip = NULL;

    char work_fn[1024];
    char widx_fn[1024];
    switch (o->style)
    {
    case SIS_DISK_TYPE_SNO: 
        {
            int nyear = o->open_date / 10000;
            sis_sprintf(work_fn, 1024, "%s/%s/%d/%d.%s",
                    o->fpath, o->fname, nyear, o->open_date, SIS_DISK_SNO_CHAR);
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
                    o->fpath, o->fname, o->open_date / 10000, o->stop_date / 10000, SIS_DISK_SDB_CHAR);
            o->work_fps->main_head.index = 1;
            o->work_fps->max_page_size = SIS_DISK_MAXLEN_SDBPAGE;
            o->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
            sis_sprintf(widx_fn, 1024, "%s/%s/%d-%d.%s",
                    o->fpath, o->fname, o->open_date / 10000, o->stop_date / 10000, SIS_DISK_IDX_CHAR);
        }
        break;
    case SIS_DISK_TYPE_SDB_DATE:  // name/2021/20210606.sdb
        {
            // 初始化小于日期时序的文件
            int nyear = o->open_date / 10000;
            sis_sprintf(work_fn, 1024, "%s/%s/%d/%d.%s",
                    o->fpath, o->fname, nyear, o->open_date, SIS_DISK_SDB_CHAR);
            o->work_fps->main_head.index = 1;
            o->work_fps->max_page_size = SIS_DISK_MAXLEN_SDBPAGE;
            o->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
            sis_sprintf(widx_fn, 1024, "%s/%s/%d/%d.%s",
                    o->fpath, o->fname, nyear, o->open_date, SIS_DISK_IDX_CHAR);
        }
        break;
    case SIS_DISK_TYPE_SDB_NONE: // name/name.sdb
        {
            sis_sprintf(work_fn, 1024, "%s/%s/%s.%s",
                    o->fpath, o->fname, o->fname, SIS_DISK_SDB_CHAR);
            // 这里虽然没有设置压缩方式 实际传入的数据就已经按增量压缩好了
            o->work_fps->main_head.index = 1;
            o->work_fps->max_page_size = SIS_DISK_MAXLEN_SDBPAGE;
            o->work_fps->max_file_size = SIS_DISK_MAXLEN_FILE;
            sis_sprintf(widx_fn, 1024, "%s/%s/%s.%s",
                    o->fpath, o->fname, o->fname, SIS_DISK_IDX_CHAR);
        }
        break;
    default: // SIS_DISK_TYPE_LOG
        // 其他类型的文件直接传入文件名 用户可自定义类型
        {
            sis_sprintf(work_fn, 1024, "%s/%s/%d.%s", o->fpath, o->fname, o->open_date, SIS_DISK_LOG_CHAR);
            o->work_fps->max_page_size = 0; // LOG文件有数据直接写
            o->work_fps->max_file_size = 0; // 顺序读写的文件 不需要设置最大值
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

    o->wcatch = sis_disk_wcatch_create(NULL, NULL);
    o->rcatch = sis_disk_rcatch_create(NULL);

    o->work_fps = sis_disk_files_create();

    o->sdb_incrzip = sis_incrzip_class_create();
    if (o->style != SIS_DISK_TYPE_LOG)
    {
        o->map_idxs = sis_map_list_create(sis_disk_idx_destroy);
        o->widx_fps = sis_disk_files_create();
    }
    
    _disk_ctrl_init(o);

    return 0;
}

void sis_disk_ctrl_destroy(void *cls_)
{
    s_sis_disk_ctrl *ctrl = (s_sis_disk_ctrl *)cls_;
    sis_sdsfree(ctrl->fpath);
    sis_sdsfree(ctrl->fname);

    sis_map_list_destroy(ctrl->map_kdicts);
    sis_map_list_destroy(ctrl->map_sdicts);

    sis_disk_wcatch_destroy(ctrl->wcatch);
    sis_disk_rcatch_destroy(ctrl->rcatch);

    sis_disk_files_destroy(ctrl->work_fps);

    sis_incrzip_class_destroy(ctrl->sdb_incrzip);
    if (ctrl->style != SIS_DISK_TYPE_LOG)
    {
        sis_map_list_destroy(ctrl->map_idxs);
        sis_disk_files_destroy(ctrl->widx_fps);
    }
    sis_free(ctrl);
}

void sis_disk_ctrl_clear(s_sis_disk_ctrl *cls_)
{
    // 创建时的信息不能删除 其他信息可清理
    sis_map_list_clear(cls_->map_kdicts);
    sis_map_list_clear(cls_->map_sdicts);

    sis_disk_wcatch_clear(cls_->wcatch);
    sis_disk_rcatch_clear(cls_->rcatch);

    sis_disk_files_clear(cls_->work_fps);

    sis_incrzip_class_clear(cls_->sdb_incrzip);
    if (cls_->style != SIS_DISK_TYPE_LOG)
    {
        sis_map_list_clear(cls_->map_idxs);
        sis_disk_files_clear(cls_->widx_fps);
    }    
    _disk_ctrl_init(cls_);
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
            sis_disk_ctrl_unzip_work(cls_, cls_->rcatch);
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
            sis_disk_ctrl_unzip_work(cls_, cls_->rcatch);
            sis_disk_reader_set_sdict(cls_->map_sdicts, sis_memory(cls_->rcatch->memory), sis_memory_get_size(cls_->rcatch->memory));
        }
    }
    return 0;
}
s_sis_disk_kdict *sis_disk_ctrl_add_kdict(s_sis_disk_ctrl *cls_, const char *kname_)
{
    s_sis_disk_kdict *kdict = sis_disk_kdict_create(kname_);
    kdict->index = sis_map_list_set(cls_->map_kdicts, kname_, kdict);
    return kdict;
}

s_sis_disk_sdict *sis_disk_ctrl_add_sdict(s_sis_disk_ctrl *cls_, const char *sname_, s_sis_dynamic_db *sdb_)
{
    s_sis_disk_sdict *sdict = sis_disk_sdict_create(sname_);
    sdict->index = sis_map_list_set(cls_->map_sdicts, sname_, sdict);
    return sdict;   
}

size_t sis_disk_ctrl_unzip_work(s_sis_disk_ctrl *cls_, s_sis_disk_rcatch *rcatch_)
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
        switch (rcatch_->rinfo->style)
        {
        case SIS_SDB_STYLE_MUL:
        {
            if (!rcatch_->mlist)
            {
                rcatch_->mlist = sis_pointer_list_create();
                rcatch_->mlist->vfree = sis_sdsfree_call;
            }
            sis_pointer_list_clear(rcatch_->mlist);
            rcatch_->kidx = sis_memory_get_ssize(rcatch_->memory);
            int count = sis_memory_get_ssize(rcatch_->memory);
            for (int i = 0; i < count; i++)
            {
                int size = sis_memory_get_ssize(rcatch_->memory);
                s_sis_sds item = sis_sdsnewlen(sis_memory(rcatch_->memory), size);
                sis_memory_move(rcatch_->memory, size);
                sis_pointer_list_push(rcatch_->mlist, item);
            }
            size = count;
        }
        break;
        case SIS_SDB_STYLE_ONE:
        {
            rcatch_->kidx = sis_memory_get_ssize(rcatch_->memory);
            size = sis_memory_get_ssize(rcatch_->memory);
            if (size != sis_memory_get_size(rcatch_->memory))
            {
                size = 0;
            }
            // 已经移动到数据区
        }
        break;
        default: // SIS_SDB_STYLE_NON
        {
            rcatch_->kidx = sis_memory_get_ssize(rcatch_->memory);
            rcatch_->sidx = sis_memory_get_ssize(rcatch_->memory);
            // 已经移动到数据区
            size = sis_memory_get_size(rcatch_->memory);
        }
        break;
        }
        return size;
    }
    if (rcatch_->head.zip == SIS_DISK_ZIP_INCRZIP)
    {
        if (rcatch_->rinfo->style == SIS_SDB_STYLE_SDB)
        {
            rcatch_->kidx = sis_memory_get_ssize(rcatch_->memory);
            rcatch_->sidx = sis_memory_get_ssize(rcatch_->memory);
            s_sis_disk_kdict *kdict = (s_sis_disk_kdict *)sis_map_list_geti(cls_->map_kdicts, rcatch_->kidx);
            s_sis_disk_sdict *sdict = (s_sis_disk_sdict *)sis_map_list_geti(cls_->map_sdicts, rcatch_->sidx);
            if (!kdict || !sdict)
            {
                LOG(8)("no find .kid = %d : %d  sid = %d : %d\n", 
                    rcatch_->kidx, sis_map_list_getsize(cls_->map_kdicts), 
                    rcatch_->sidx, sis_map_list_getsize(cls_->map_sdicts));
                return 0;
            }             
            s_sis_dynamic_db *db = sis_disk_sdict_get(sdict, rcatch_->rinfo->sdict); 
            if (!db)
            {
                db = sis_disk_sdict_last(sdict); 
            }
            s_sis_memory *unzipmemory = sis_memory_create();
            sis_incrzip_class_clear(cls_->sdb_incrzip);
            sis_incrzip_set_sdb(cls_->sdb_incrzip, db);
            if(sis_incrzip_uncompress(cls_->sdb_incrzip, sis_memory(rcatch_->memory), sis_memory_get_size(rcatch_->memory), unzipmemory) > 0)
            {
                sis_memory_swap(rcatch_->memory, unzipmemory);
                size = sis_memory_get_size(rcatch_->memory);
            }
            else
            {
                LOG(8)("incrzip uncompress fail.\n"); 
            }
            sis_memory_destroy(unzipmemory); 
        }
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
        return SIS_DISK_CMD_NO_EXISTS;
    }
    if (cls_->work_fps->main_head.index)
    {
        if (!sis_file_exists(cls_->widx_fps->cur_name))
        {
            LOG(5)
            ("idxfile no exists.[%s]\n", cls_->widx_fps->cur_name);
            return SIS_DISK_CMD_NO_EXISTS_IDX;
        }
    }
    // printf("-7--ss-- %d\n", cls_->work_fps->main_head.wtime);
    int o = sis_disk_files_open(cls_->work_fps, SIS_DISK_ACCESS_RDONLY);
    if (o)
    {
        LOG(5)
        ("open file fail.[%s:%d]\n", cls_->work_fps->cur_name, o);
        return SIS_DISK_CMD_NO_OPEN;
    }
    if (SIS_DISK_IS_SDB(cls_->style))
    {
        o = sis_disk_io_read_sdb_widx(cls_);
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
    // printf("-5--ss-- %d\n", cls_->work_fps->main_head.wtime);
    if (access == SIS_DISK_ACCESS_CREATE)
    {
        // 工作文件和索引文件保持同样的随机码 应该创建时才生成
        // 只有创建时重新生成
        sis_str_get_random(cls_->work_fps->main_tail.crc, 16);
        memmove(cls_->widx_fps->main_tail.crc, cls_->work_fps->main_tail.crc, 16);
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
        // printf("-6--ss-- %d\n", cls_->work_fps->main_head.wtime);
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
            int o = sis_disk_io_read_sdb_widx(cls_);
            if (o != SIS_DISK_CMD_OK)
            {
                return o;
            }
        }
        else if (cls_->style == SIS_DISK_TYPE_SNO)
        {
            int o = sis_disk_io_read_sno_widx(cls_);
            if (o != SIS_DISK_CMD_OK)
            {
                return o;
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


void sis_disk_ctrl_delete(s_sis_disk_ctrl *cls_)
{
    int count = sis_disk_files_delete(cls_->work_fps);
    if (cls_->widx_fps)
    {
        count += sis_disk_files_delete(cls_->widx_fps);
    }
    LOG(5)("delete file count = %d.\n", count);
}

int sis_disk_ctrl_pack(s_sis_disk_ctrl *src_, s_sis_disk_ctrl *des_)
{
    // 开始新文件
    sis_disk_ctrl_delete(des_);
    if (src_->style == SIS_DISK_TYPE_LOG || src_->style == SIS_DISK_TYPE_SNO)
    {
        return 0;
    }
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

//??? 这里以后要判断是否改名成功 如果一个失败就全部恢复出来
int sis_disk_ctrl_move(s_sis_disk_ctrl *cls_, const char *path_)
{
    char agofn[255];
    char newfn[255];
    int o = 0;
    for (int  i = 0; i < cls_->work_fps->lists->count; i++)
    {
        s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->work_fps->lists, i);
        sis_file_getname(unit->fn, agofn, 255);
        sis_sprintf(newfn, 255, "%s/%s", path_, agofn);
        sis_file_rename(unit->fn, newfn);
    }
    if (cls_->work_fps->main_head.index)
    {
        for (int  i = 0; i < cls_->widx_fps->lists->count; i++)
        {
            s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->widx_fps->lists, i);
            sis_file_getname(unit->fn, agofn, 255);
            sis_sprintf(newfn, 255, "%s/%s", path_, agofn);
            sis_file_rename(unit->fn, newfn);
        }
    }
    return o;
}


