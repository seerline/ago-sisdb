#include "sis_disk.io.h"

///////////////////////////
//  s_sis_disk_files_unit
///////////////////////////
s_sis_disk_files_unit *sis_disk_files_unit_create()
{
    s_sis_disk_files_unit *o = SIS_MALLOC(s_sis_disk_files_unit, o);
    o->fcatch = sis_memory_create_size(SIS_MEMORY_SIZE + SIS_MEMORY_SIZE);
    return o;
}
void sis_disk_files_unit_destroy(void *unit_)
{
    s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)unit_;
    sis_sdsfree(unit->fn);
    sis_memory_destroy(unit->fcatch);
    sis_free(unit);
}
///////////////////////////
//  s_sis_disk_files
///////////////////////////
s_sis_disk_files *sis_disk_files_create()
{
    s_sis_disk_files *o = SIS_MALLOC(s_sis_disk_files, o);
    o->lists = sis_pointer_list_create();
    o->lists->vfree = sis_disk_files_unit_destroy;
    sis_disk_files_clear(o);
    return o;
}
void sis_disk_files_destroy(void *in_)
{
    s_sis_disk_files *o = (s_sis_disk_files *)in_;
    // 需要关闭所有的文件
    sis_disk_files_close(o);
    sis_sdsfree(o->cur_name);
    sis_pointer_list_destroy(o->lists);  
    sis_free(o);  
}
void sis_disk_files_clear(s_sis_disk_files *in_)
{
    s_sis_disk_files *o = (s_sis_disk_files *)in_;
    sis_pointer_list_clear(o->lists);

    o->access = SIS_DISK_ACCESS_NOLOAD;
    memset(&o->main_head, 0, sizeof(s_sis_disk_main_head));
    memset(&o->main_tail, 0, sizeof(s_sis_disk_main_tail));
    sis_sdsfree(o->cur_name);  o->cur_name = NULL;
    o->cur_unit = -1;
    o->max_file_size = 0;  // = 0 不限制文件大小
    o->max_page_size = 0;

    o->main_head.fin = 1;
    o->main_head.zip = 0;
    o->main_head.hid = SIS_DISK_HID_HEAD;
    o->main_head.version = SIS_DISK_SDB_VER;
    o->main_head.style = SIS_DISK_TYPE_SDB_IDX;
    memmove(o->main_head.sign, "SIS", 3);

    o->main_tail.fin = 1;
    o->main_tail.zip = 0;
    o->main_tail.hid = SIS_DISK_HID_TAIL;  
    o->main_tail.fcount = 1; 
}
void sis_disk_files_close(s_sis_disk_files *cls_)
{
    for (int i = 0; i < cls_->lists->count; i++)
    {
        s_sis_disk_files_unit *unit =(s_sis_disk_files_unit *)sis_pointer_list_get(cls_->lists, i);
        if (unit->status != SIS_DISK_STATUS_CLOSED)
        {
            if (cls_->main_head.style != SIS_DISK_TYPE_LOG)
            {
                if (unit->status & SIS_DISK_STATUS_NOSTOP)
                {
                    cls_->main_tail.fcount = cls_->lists->count;
                    cls_->main_tail.novalid = unit->novalid;
                    cls_->main_tail.validly = unit->validly;
                    LOG(5)("write tail ok. %p %zu %zu count = %d | valid: %d %d\n", unit->fp_1, 
                        sis_file_seek(unit->fp_1, 0, SEEK_CUR), unit->offset, 
                        cls_->main_tail.fcount, cls_->main_tail.novalid, cls_->main_tail.validly);
                    sis_file_write(unit->fp_1, (const char *)&cls_->main_tail, sizeof(s_sis_disk_main_tail));
                    unit->offset += sizeof(s_sis_disk_main_tail);
                    LOG(5)("write tail ok. %p %zu %zu count = %d\n", unit->fp_1, 
                        sis_file_seek(unit->fp_1, 0, SEEK_CUR), unit->offset, 
                        cls_->main_tail.fcount);
                }
            }
            // 关闭文件
            sis_file_close(unit->fp_1);
            unit->offset = 0;
            unit->status = SIS_DISK_STATUS_CLOSED;
            unit->fp_1 = NULL;
            unit->novalid = 0;
            unit->validly = 0;
        }
    }
}

int sis_disk_files_init(s_sis_disk_files *cls_, char *fname_)
{
    sis_sdsfree(cls_->cur_name);
    cls_->cur_name = sis_sdsnew(fname_);
    // 此时需要初始化list 设置最少1个文件
    sis_pointer_list_clear(cls_->lists);
    // 初始化基础文件
    sis_disk_files_inc_unit(cls_);
    
    int count = 1;
    // LOG 文件没有尾部
    if (cls_->main_head.style != SIS_DISK_TYPE_LOG)
    {
        char fn[1024];
        while (1)
        {
            sis_sprintf(fn, 1024, "%s.%d", cls_->cur_name, count);
            if(sis_file_exists(fn))
            {
                count++;
                sis_disk_files_inc_unit(cls_);
            }
            else
            {
                break;
            }
        }
        if(sis_file_exists(cls_->cur_name))
        {
            LOG(8)("init files [%s] count = %d\n", cls_->cur_name, count);
        }
    }
    return count;
}    

int sis_disk_files_inc_unit(s_sis_disk_files *cls_)
{
    s_sis_disk_files_unit *unit = sis_disk_files_unit_create();
    unit->fp_1 = NULL;
    // 有新增 一定是有新文件产生
    sis_sdsfree(unit->fn);
    unit->fn = sis_sdsdup(cls_->cur_name);
    if(cls_->lists->count >= 1)
    {
        unit->fn = sis_sdscatfmt(unit->fn, ".%u", cls_->lists->count);
    }
    cls_->cur_unit = cls_->lists->count;
    return sis_pointer_list_push(cls_->lists, unit);
}
int sis_disk_files_open_create(s_sis_disk_files *cls_, s_sis_disk_files_unit *unit)
{
    if (!unit)
    {
        return -1;
    }
    sis_check_path(unit->fn); 
    // SIS_FILE_IO_DSYNC   
    unit->fp_1 = sis_file_open(unit->fn, SIS_FILE_IO_TRUNC | SIS_FILE_IO_RDWR | SIS_FILE_IO_CREATE, SIS_FILE_MODE_NORMAL);
    if (!unit->fp_1)
    {
        return -2;
    }
    sis_file_write(unit->fp_1, (const char *)&cls_->main_head, sizeof(s_sis_disk_main_head));
    unit->offset = sizeof(s_sis_disk_main_head);
    unit->status = SIS_DISK_STATUS_CREATE | SIS_DISK_STATUS_NOSTOP;
    return 0;
}
size_t sis_disk_files_seek_log(s_sis_disk_files_unit *unit)
{ 
    size_t offset = 0;

    s_sis_memory *imem = sis_memory_create_size(2 * SIS_MEMORY_SIZE);
    bool             FILEEND = false;
    bool             LINEEND = true;
    size_t           size = 0;
    s_sis_disk_head  head;   
    // 从头开始读
    sis_file_seek(unit->fp_1, sizeof(s_sis_disk_main_head), SEEK_SET);
    offset += sizeof(s_sis_disk_main_head);
    size_t ssize = 0;
    while (!FILEEND)
    {
        size_t bytes = sis_memory_readfile(imem, unit->fp_1, SIS_MEMORY_SIZE);
        if (bytes <= 0)
        {
            FILEEND = true; // 文件读完了, 但要处理完数据
        }
        // 缓存不够大就继续读
        if (sis_memory_get_size(imem) < size)
        {
            continue;
        }
        while (sis_memory_get_size(imem) >= 2)
        {   
            if (LINEEND)
            {
                memmove(&head, sis_memory(imem), sizeof(s_sis_disk_head));
                sis_memory_move(imem, sizeof(s_sis_disk_head));
                if (head.hid != SIS_DISK_HID_MSG_LOG)
                {
                    FILEEND = true; 
                    break;
                }
                // 读取数据区长度
                ssize = (size_t)sis_memory(imem);
                size = sis_memory_get_ssize(imem);
                ssize =  (size_t)sis_memory(imem) - ssize; 
                if (sis_memory_get_size(imem) < size)
                {
                    LINEEND = false;
                    break;
                }
            }
            sis_memory_move(imem, size);
            // printf("%zu %zu %zu\n", offset, ssize, size);
            offset += sizeof(s_sis_disk_head) + ssize + size;
            // printf("--- %zu %zu %zu\n", offset, ssize, size);
            size = 0;
            LINEEND = true;
        } // while SIS_DISK_MIN_RSIZE
    } // while
    LOG(5)("move offset: %zu %zu\n", offset, sis_file_seek(unit->fp_1, 0, SEEK_END));
    sis_memory_destroy(imem);
    return offset;
}
int sis_disk_files_open_append(s_sis_disk_files *cls_, s_sis_disk_files_unit *unit)
{
    if (!unit)
    {
        return -1;
    }
    // LOG(5)("open ok..1..: %s\n", unit->fn);
    // SIS_FILE_IO_DSYNC
    unit->fp_1 = sis_file_open(unit->fn, SIS_FILE_IO_BINARY | SIS_FILE_IO_RDWR, 0 );
    if (!unit->fp_1)
    {
        return -2;
    }
    s_sis_disk_main_head head;
    size_t bytes = sis_file_read(unit->fp_1, (char *)&head, sizeof(s_sis_disk_main_head));
    if (bytes != sizeof(s_sis_disk_main_head) || head.style != cls_->main_head.style)
    {
        LOG(5)("style fail. [%d %d] [%d %d]\n", (int)bytes , (int)sizeof(s_sis_disk_main_head), head.style, cls_->main_head.style);
        sis_disk_files_close(cls_);
        return -3;
    }     
    if (cls_->main_head.style != SIS_DISK_TYPE_LOG)
    {
        unit->offset = sis_file_seek(unit->fp_1, -1 * (int)sizeof(s_sis_disk_main_tail), SEEK_END);
        sis_file_read(unit->fp_1, (char *)&cls_->main_tail, sizeof(s_sis_disk_main_tail));
        // 再移动回去
        // LOG(5)("open ok: %zu %zu | %lld\n", unit->offset, sizeof(s_sis_disk_main_tail), sis_file_seek(unit->fp_1, 0, SEEK_CUR));
        sis_file_seek(unit->fp_1, -1 * (int)sizeof(s_sis_disk_main_tail), SEEK_END);
        // LOG(5)("open ok: %zu %zu | %lld\n", unit->offset, sizeof(s_sis_disk_main_tail), sis_file_seek(unit->fp_1, 0, SEEK_CUR));
        unit->validly = cls_->main_tail.validly;
        unit->novalid = cls_->main_tail.novalid;
        // ??? 可以在这里检查文件是否合法
    }
    else
    {
        // unit->offset = sis_file_seek(unit->fp_1, 0, SEEK_END);
        unit->offset = sis_disk_files_seek_log(unit);
        ftruncate(fileno(unit->fp_1), unit->offset);
        // ??? 可以在这里检查文件是否完整 不完整需要从头遍历到完整的位置
    }
    // LOG(5)("open ok..2..: %zu %d\n", unit->offset, cls_->main_head.style);
    // ??? 不同日期的LOG文件可能会有问题
    memmove(&cls_->main_head, &head, sizeof(s_sis_disk_main_head));
    unit->status = SIS_DISK_STATUS_APPEND | SIS_DISK_STATUS_NOSTOP;
    return 0;
}

int sis_disk_files_open_rdonly(s_sis_disk_files *cls_, s_sis_disk_files_unit *unit)
{
    if (!unit)
    {
        return -1;
    }
    // SIS_FILE_IO_RSYNC
    // unit->fp_1 = sis_open(unit->fn, SIS_FILE_IO_BINARY | SIS_FILE_IO_RSYNC | SIS_FILE_IO_READ, 0 );
    unit->fp_1 = sis_file_open(unit->fn, SIS_FILE_IO_BINARY | SIS_FILE_IO_READ, 0 );
    if (!unit->fp_1)
    {
        return -2;
    }
    s_sis_disk_main_head head;
    size_t bytes = sis_file_read(unit->fp_1, (char *)&head, sizeof(s_sis_disk_main_head));
    // sis_out_binary("head", (char *)&head, bytes);
    if (bytes != sizeof(s_sis_disk_main_head) || head.style != cls_->main_head.style)
    {
        LOG(5)("style fail. [%d %d] [%d %d]\n", (int)bytes , (int)sizeof(s_sis_disk_main_head), head.style, cls_->main_head.style);
        sis_disk_files_close(cls_);
        return -3;
    }  
    // 这里要更新init设置的头信息
    memmove(&cls_->main_head, &head, sizeof(s_sis_disk_main_head));
    // printf("-3--ss-- %d\n", cls_->main_head.wtime);
    unit->status = SIS_DISK_STATUS_RDOPEN;

    return 0;
}
int sis_disk_files_remove(s_sis_disk_files *cls_)
{
    for (int  i = 0; i < cls_->lists->count; i++)
    {
        s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->lists, i);
        LOG(8)("delete file %s\n", unit->fn);
        sis_file_delete(unit->fn);
    }
    return cls_->lists->count;
}
/**
 * @brief 
 * @param cls_ 
 * @param access_ 
 * SIS_DISK_ACCESS_RDONLY    1  文件只读打开,
 * SIS_DISK_ACCESS_APPEND    2  文件可写打开 从文件尾写入数据,
 * SIS_DISK_ACCESS_CREATE    3  新文件打开 从第一个文件写入
 * @return int 
 */
int sis_disk_files_open(s_sis_disk_files *cls_, int access_)
{
    cls_->access = access_;
    // printf("-3.1--ss-- %d\n", cls_->main_head.wtime);
    LOG(5)("ready open file count = %d \n", cls_->lists->count);
    switch (cls_->access)
    {
    case SIS_DISK_ACCESS_CREATE:
        {
            sis_disk_files_remove(cls_);
            sis_pointer_list_delete(cls_->lists, 1, cls_->lists->count - 1);
            s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->lists, 0);
            if (cls_->lists->count < 1)
            {
                return -1;
            }
            cls_->cur_unit = 0;
            sis_disk_files_open_create(cls_, unit);
        }
        break;
    case SIS_DISK_ACCESS_APPEND:
        {
            cls_->cur_unit = cls_->lists->count - 1;
            s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
            if (cls_->lists->count < 1)
            {
                return -2;
            }
            sis_disk_files_open_append(cls_, unit);
        }
        break;    
    default:  // SIS_DISK_ACCESS_RDONLY
        {
            for (int  i = 0; i < cls_->lists->count; i++)
            {
                s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->lists, i);
                int o = sis_disk_files_open_rdonly(cls_, unit);
                if (o)
                {
                    LOG(3)("open rdonly fail. [%d]\n", o);
                    return -5;
                }
            }      
            if (cls_->lists->count < 1)
            {
                LOG(3)("open fail. count = %d \n", cls_->lists->count);
                return -3;
            }   
            cls_->cur_unit = 0;   
        }
        break;
    }
    return 0;
}

bool sis_disk_files_seek(s_sis_disk_files *cls_)
{ 
    s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
    if (!unit || !unit->fp_1)
    {
        return false;
    }
    // 定位写入的位置 每次定位写盘速度慢
    sis_file_seek(unit->fp_1, unit->offset, SEEK_SET);
    return true;
}
size_t sis_disk_files_offset(s_sis_disk_files *cls_)
{
    s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
    if (!unit || !unit->fp_1)
    {
        return 0;
    }
    return unit->offset;
}
size_t sis_disk_files_write_sync(s_sis_disk_files *cls_)
{
    size_t size = 0;
    s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
    if (!unit || !unit->fp_1)
    {
        return 0;
    }
    // LOG(5)("sync: %p %zu %zu\n", unit->fp_1, sis_file_seek(unit->fp_1, 0, SEEK_END), sis_memory_get_size(unit->fcatch));
    if (sis_memory_get_size(unit->fcatch) > 0)
    {
        size = sis_file_write(unit->fp_1, sis_memory(unit->fcatch), sis_memory_get_size(unit->fcatch));
        sis_memory_clear(unit->fcatch);
    }
    sis_file_fsync(unit->fp_1);
    LOG(5)("sync: %p %zu\n", unit->fp_1, sis_file_seek(unit->fp_1, 0, SEEK_CUR));
    return size;
}
// 有数据就写 只有LOG才同步每一次写入 
// 同步写非常慢 
// static size_t sis_safe_write(int __t__,s_sis_memory *__m__,s_sis_handle __f__,
//     const char *__v__,size_t __z__)
// {
//     size_t _osize_ = 0;   
//     size_t _curz_ = sis_memory_get_size(__m__); 
//     if (_curz_ > 0) { 
//         _osize_ = sis_write(__f__, sis_memory(__m__), _curz_); 
//         sis_memory_clear(__m__); 
//     } 
//     if (__z__ > 0)
//     {
//         _osize_ += sis_write(__f__,__v__,__z__);  
//     }
//     if (__t__ == SIS_DISK_HID_MSG_LOG) 
//     {       
//         sis_fsync(__f__); 
//     } 
//     return _osize_; 
// } 
static inline size_t sis_safe_write(s_sis_disk_head *__h__,s_sis_memory *__m__,s_sis_file_handle __f__,
    const char *__v__, size_t __z__)
{
    size_t _osize_ = 0;  
    if (__z__ > 0)
    {
        _osize_ = sis_memory_cat(__m__, (char *)__h__, sizeof(s_sis_disk_head));
        _osize_ += sis_memory_cat_ssize(__m__, __z__);
        size_t _curz_ = sis_memory_get_size(__m__); 
        if ((_curz_ + __z__) > SIS_MEMORY_SIZE) { 
            sis_file_write(__f__, sis_memory(__m__), _curz_); 
            sis_memory_clear(__m__); 
            _osize_ += sis_file_write(__f__,__v__,__z__);  
        } 
        else
        {
            _osize_ += sis_memory_cat(__m__, (char *)__v__, __z__); 
        }

    }
    else
    {
        _osize_ = sis_memory_cat(__m__, (char *)__h__, sizeof(s_sis_disk_head));
        _osize_ += sis_memory_cat_ssize(__m__, __z__);
    }
    return _osize_; 
} 
size_t sis_disk_files_write(s_sis_disk_files *cls_, s_sis_disk_head  *head_, void *in_, size_t ilen_)
{
    s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
    if (!unit || !unit->fp_1)
    {
        return 0;
    }
    if (cls_->main_head.style == SIS_DISK_TYPE_LOG) 
    {
        sis_memory_clear(unit->fcatch); 
        sis_memory_cat(unit->fcatch, (char *)head_, sizeof(s_sis_disk_head));
        sis_memory_cat_ssize(unit->fcatch, ilen_);
        size_t size = sis_file_write(unit->fp_1, sis_memory(unit->fcatch), sis_memory_get_size(unit->fcatch)); 
        sis_memory_clear(unit->fcatch);
        size += sis_file_write(unit->fp_1, in_, ilen_);
        sis_file_fsync(unit->fp_1); 
        // printf("%lld %lld\n", sis_file_seek(unit->fp_1, 0, SEEK_END), sis_memory_get_size(unit->fcatch));
        // printf("%lld %lld\n", sis_file_seek(unit->fp_1, 0, SEEK_END), sis_memory_get_size(unit->fcatch));
        return size; 
    }

    size_t size = sis_safe_write(head_, unit->fcatch, unit->fp_1, in_, ilen_);
    unit->offset += size;
    return size;
}

size_t sis_disk_files_write_saveidx(s_sis_disk_files *cls_, s_sis_disk_wcatch *wcatch_)
{ 
    // 传进来的数据 hid == SIS_DISK_HID_MSG_LOG 立即写盘
    s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
    if (!unit || !unit->fp_1)
    {
        return 0;
    }
    size_t insize = sis_memory_get_size(wcatch_->memory);
    // 压缩要在外部 到这里只管写数据 新进的块如果过大就开新文件
    if (cls_->max_file_size > 0 && (unit->offset + insize + SIS_DISK_MIN_WSIZE) > cls_->max_file_size)
    {
        LOG(5)("new file: %d %zu %zu\n", cls_->cur_unit, unit->offset, cls_->max_file_size);
        sis_disk_files_write_sync(cls_);
        sis_disk_files_inc_unit(cls_);
        // 创建文件并打开
        unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
        if (sis_disk_files_open_create(cls_, unit))
        {
            LOG(5)("grow file fail. %s\n", unit->fn);
            return 0;
        } 
    }
    // 如果块大于 PAGE 就会分块写入 fin = 0 直到 fin = 1 才是一个完整块
    size_t size = sis_safe_write(&wcatch_->head, unit->fcatch, unit->fp_1, sis_memory(wcatch_->memory), insize);
    wcatch_->winfo.fidx = cls_->cur_unit;
    wcatch_->winfo.offset = unit->offset;
    wcatch_->winfo.size = size; // 因为是索引的长度 因此这里的长度包含头和size
    unit->offset += size;
    // LOG(8)("%d hid = %d zip = %d size = %zu | %zu \n", unit->fp_1, wcatch_->head.hid, wcatch_->head.zip, size, unit->offset);
    return size;
}
//////////////////////////
//  以下是全文件顺序读取的函数
//////////////////////////
// #define _READ_SPEED_  
size_t sis_disk_files_read_fulltext(s_sis_disk_files *cls_, void *source_, cb_sis_disk_files_read *callback)
{ 
    if (!callback)
    {
        return 0;
    }
    bool READSTOP = false;
    s_sis_memory *imem = sis_memory_create_size(2 * SIS_MEMORY_SIZE);
    for (int i = 0; i < cls_->lists->count && !READSTOP; i++)
    {
        s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->lists, i); 
        sis_memory_clear(imem);
        bool             FILEEND = false;
        bool             LINEEND = true;
        size_t           size = 0;
        s_sis_disk_head  head;   
        // 从头开始读
        sis_file_seek(unit->fp_1, sizeof(s_sis_disk_main_head), SEEK_SET);
#ifdef  _READ_SPEED_
        msec_t _start_msec = sis_time_get_now_msec();
        size_t _mem_size = 0;
#endif
        while (!FILEEND && !READSTOP)
        {
            size_t bytes = sis_memory_readfile(imem, unit->fp_1, SIS_MEMORY_SIZE);
            if (bytes <= 0)
            {
                FILEEND = true; // 文件读完了, 但要处理完数据
            }
#ifdef  _READ_SPEED_
            _mem_size+=bytes;
            // continue;
#endif
            // 缓存不够大就继续读
            if (sis_memory_get_size(imem) < size)
            {
                continue;
            }
            while (sis_memory_get_size(imem) >= 4 && !READSTOP)
            {
                if (LINEEND)
                {
                    memmove(&head, sis_memory(imem), sizeof(s_sis_disk_head));
                    sis_memory_move(imem, sizeof(s_sis_disk_head));
                    if (head.hid == SIS_DISK_HID_TAIL)
                    {
                        // 结束
                        size = SIS_DISK_MIN_RSIZE - 1;
                        FILEEND = true; 
                        break;
                    }
                    // 读取数据区长度
                    size = sis_memory_get_ssize(imem);
                    if (sis_memory_get_size(imem) < size)
                    {
                        LINEEND = false;
                        break;
                    }
                }
                if (head.hid != SIS_DISK_HID_NONE)
                {
                    int ret = callback(source_, &head, sis_memory(imem), size);
                    // sis_out_binary("ok:", sis_memory(imem), size) ;
                    if (ret == -2)
                    {
                    //     // 文坏块
                        sis_out_binary("mem:", sis_memory(imem) - 128, size + 128) ;
                    }
                    // else 
                    if (ret < 0)
                    {
                        // 回调返回 -1 表示已经没有读者了
                        LOG(5)("user jump ok.\n");
                        READSTOP = true;
                        break;
                    }
                }
                else
                {
                    sis_out_binary("zero:", sis_memory(imem) - 128, size + 128) ;
                    // 出错
                    LOG(5)("file fail.\n");
                    READSTOP = true;
                    break;
                }
                sis_memory_move(imem, size);
                size = 0;
                LINEEND = true;
            } // while SIS_DISK_MIN_RSIZE
        } // while
#ifdef  _READ_SPEED_
        // 读4G文件约60秒 MAC 1秒
        // 解压缩 约 40秒 MAC 21秒
        // 只解析数据 约 160秒 
        // 排序花费时间 840秒- 2050秒 
        // 优化后 300秒
        LOG(5)("%zu cost = %d\n", _mem_size, sis_time_get_now_msec() - _start_msec);
#endif
    }
    sis_memory_destroy(imem);
    return 0;
}

// 定位读去某一个块
size_t sis_disk_files_read(s_sis_disk_files *cls_, int fidx_, size_t offset_, size_t size_, s_sis_disk_head *head_, s_sis_memory *omemory_)
{
    s_sis_disk_files_unit *unit = (s_sis_disk_files_unit *)sis_pointer_list_get(cls_->lists, fidx_); 
    if (!unit || !unit->fp_1)
    {
        return 0;
    }
    size_t  size = 0; 
    sis_file_seek(unit->fp_1, offset_, SEEK_SET);
    size_t bytes = sis_memory_readfile(omemory_, unit->fp_1, size_);
    if (bytes == size_)
    {
        memmove(head_, sis_memory(omemory_), sizeof(s_sis_disk_head));
        sis_memory_move(omemory_, sizeof(s_sis_disk_head));
        size = sis_memory_get_ssize(omemory_);
        if (size != sis_memory_get_size(omemory_))
        {
            LOG(5)("read data size error. %zu != %zu\n", size, sis_memory_get_size(omemory_));
        }
    }
    return size;
}

// 根据索引读取数据放到 rcatch_ 中 不解压 只读出原始数据 
size_t sis_disk_files_read_fromidx(s_sis_disk_files *cls_, s_sis_disk_rcatch *rcatch_)
{
    // 此函数是否需要保存文件的原始位置
    size_t size = sis_disk_files_read(cls_, rcatch_->rinfo->fidx, rcatch_->rinfo->offset, 
        rcatch_->rinfo->size, &rcatch_->head, rcatch_->memory);
    // sis_out_binary("read_fromidx", sis_memory(rcatch_->memory), sis_memory_get_size(rcatch_->memory));
    return size;
}

size_t sis_disk_io_unzip_widx(s_sis_disk_head  *head_, char *in_, size_t ilen_, s_sis_memory *memory_)
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
