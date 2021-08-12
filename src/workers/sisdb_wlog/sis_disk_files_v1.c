#include "sis_disk_v1.h"

///////////////////////////
//  s_sis_disk_v1_unit
///////////////////////////
s_sis_disk_v1_unit *sis_disk_v1_unit_create()
{
    s_sis_disk_v1_unit *o = SIS_MALLOC(s_sis_disk_v1_unit, o);
    o->wcatch = sis_memory_create_size(2 * SIS_MEMORY_SIZE);
    return o;
}
void sis_disk_v1_unit_destroy(void *unit_)
{
    s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)unit_;
    sis_memory_destroy(unit->wcatch);
    sis_free(unit);
}
///////////////////////////
//  s_sis_disk_v1
///////////////////////////
s_sis_disk_v1 *sis_disk_v1_create()
{
    s_sis_disk_v1 *o = SIS_MALLOC(s_sis_disk_v1, o);
    o->lists = sis_pointer_list_create();
    o->lists->vfree = sis_disk_v1_unit_destroy;
    o->zip_memory = sis_memory_create();
    sis_disk_v1_clear(o);
    return o;
}
void sis_disk_v1_destroy(void *in_)
{
    s_sis_disk_v1 *o = (s_sis_disk_v1 *)in_;
    // 需要关闭所有的文件
    sis_disk_v1_close(o);
    sis_pointer_list_destroy(o->lists);  
    sis_memory_destroy(o->zip_memory);
    sis_free(o);  
}
void sis_disk_v1_clear(s_sis_disk_v1 *in_)
{
    s_sis_disk_v1 *o = (s_sis_disk_v1 *)in_;
    sis_pointer_list_clear(o->lists);
    sis_memory_clear(o->zip_memory);

    o->access = SIS_DISK_ACCESS_NOLOAD;
    memset(&o->main_head, 0, sizeof(s_sis_disk_v1_main_head));
    memset(&o->main_tail, 0, sizeof(s_sis_disk_v1_main_tail));
    memset(&o->cur_name, 0, SIS_DISK_NAME_LEN);
    o->cur_unit = -1;
    o->max_file_size = 0;
    o->max_page_size = SIS_DISK_MAXLEN_MINPAGE;

    o->main_head.fin = 1;
    o->main_head.zip = 0;
    o->main_head.hid = SIS_DISK_HID_HEAD;
    o->main_head.version = SIS_DISK_SDB_VER;
    o->main_head.style = SIS_DISK_TYPE_STREAM;
    o->main_head.compress = SIS_DISK_ZIP_SNAPPY;
    memmove(o->main_head.sign, "SIS", 3);

    o->main_tail.fin = 1;
    o->main_tail.zip = 0;
    o->main_tail.hid = SIS_DISK_HID_TAIL;  
    o->main_tail.count = 1; 
}
void sis_disk_v1_close(s_sis_disk_v1 *cls_)
{
    for (int i = 0; i < cls_->lists->count; i++)
    {
        s_sis_disk_v1_unit *unit =(s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, i);
        if (unit->status != SIS_DISK_STATUS_CLOSED)
        {
            if (cls_->main_head.style != SIS_DISK_TYPE_STREAM && cls_->main_head.style != SIS_DISK_TYPE_LOG)
            {
                if (unit->status & SIS_DISK_STATUS_NOSTOP)
                {
                    cls_->main_tail.count = cls_->lists->count;
                    sis_write(unit->fp, (const char *)&cls_->main_tail, sizeof(s_sis_disk_v1_main_tail));
                    unit->offset += sizeof(s_sis_disk_v1_main_tail);
                    LOG(5)
                    ("write tail ok. count = %d\n", cls_->main_tail.count);
                }
            }
            // 关闭文件
            sis_close(unit->fp);
            unit->offset = 0;
            unit->status = SIS_DISK_STATUS_CLOSED;
            unit->fp = -1;
        }
    }
    // lists 在初始化中设置 所以这里不能清除
    // sis_pointer_list_clear(cls_->lists);
}

// static size_t sis_seek(int fp, __off_t offset, int set)
// {
//     printf("lseek ..\n");
//     return lseek(fp, offset, set);
// }
void sis_disk_v1_init(s_sis_disk_v1 *cls_, char *fn_)
{
    sis_strcpy(cls_->cur_name, SIS_DISK_NAME_LEN, fn_);
    // 此时需要初始化list 设置一些
    sis_pointer_list_clear(cls_->lists);
    int count = 1;
    // 流式文件 和 LOG 文件没有尾部 只有一个文件
    if (cls_->main_head.style != SIS_DISK_TYPE_STREAM && cls_->main_head.style != SIS_DISK_TYPE_LOG)
    {
        if(sis_file_exists(cls_->cur_name))
        {
            // SIS_FILE_IO_RSYNC | 
            int fp = sis_open(cls_->cur_name, SIS_FILE_IO_BINARY | SIS_FILE_IO_READ , 0);
            s_sis_disk_v1_main_tail tail;
            sis_seek(fp, -1 * (int)sizeof(s_sis_disk_v1_main_tail), SEEK_END);  
            sis_read(fp, (char *)&tail, sizeof(s_sis_disk_v1_main_tail));
            count = tail.count;
            LOG(3)("init files [%s] %d count = %d\n", cls_->cur_name, tail.hid, count);
            sis_close(fp);      
        }
    }
    for (int i = 0; i < count; i++)
    {
        sis_disk_v1_inc_unit(cls_);
    }
}    

int sis_disk_v1_inc_unit(s_sis_disk_v1 *cls_)
{
    s_sis_disk_v1_unit *unit = sis_disk_v1_unit_create();
    unit->fp = -1;
    // 有新增 一定是有新文件产生
    if(cls_->lists->count < 1)
    {
        sis_strcpy(unit->fn, SIS_DISK_NAME_LEN, cls_->cur_name);
    }
    else
    {
        sis_sprintf(unit->fn, SIS_DISK_NAME_LEN, "%s.%d", cls_->cur_name, cls_->lists->count);
    }
    cls_->cur_unit = cls_->lists->count;
    return sis_pointer_list_push(cls_->lists, unit);
}
int sis_disk_v1_open_create(s_sis_disk_v1 *cls_, s_sis_disk_v1_unit *unit)
{
    if (!unit)
    {
        return -1;
    }
    sis_check_path(unit->fn); 
    // SIS_FILE_IO_DSYNC   
    unit->fp = sis_open(unit->fn, SIS_FILE_IO_TRUNC | SIS_FILE_IO_RDWR | SIS_FILE_IO_CREATE, SIS_FILE_MODE_NORMAL);
    if (unit->fp < 0)
    {
        return -2;
    }
    sis_write(unit->fp, (const char *)&cls_->main_head, sizeof(s_sis_disk_v1_main_head));
    // printf("-2--ss-- %d\n", cls_->main_head.wtime);
    unit->offset = sizeof(s_sis_disk_v1_main_head);
    unit->status = SIS_DISK_STATUS_CREATE | SIS_DISK_STATUS_NOSTOP;
    return 0;
}
int sis_disk_v1_open_append(s_sis_disk_v1 *cls_, s_sis_disk_v1_unit *unit)
{
    if (!unit)
    {
        return -1;
    }
    // SIS_FILE_IO_DSYNC
    unit->fp = sis_open(unit->fn, SIS_FILE_IO_BINARY | SIS_FILE_IO_RDWR, 0 );
    if (unit->fp < 0)
    {
        return -2;
    }
    s_sis_disk_v1_main_head head;
    size_t bytes = sis_read(unit->fp, (char *)&head, sizeof(s_sis_disk_v1_main_head));
    if (bytes != sizeof(s_sis_disk_v1_main_head) || head.style != cls_->main_head.style)
    {
        sis_disk_v1_close(cls_);
        return -3;
    }     
    if (cls_->main_head.style != SIS_DISK_TYPE_STREAM && cls_->main_head.style != SIS_DISK_TYPE_LOG)
    {
        unit->offset = sis_seek(unit->fp, -1 * (int)sizeof(s_sis_disk_v1_main_tail), SEEK_END);
    }
    else
    {
        unit->offset = sis_seek(unit->fp, 0, SEEK_END);
    }
    // ??? 这里可能有问题 日期不同的log这里会有问题
    memmove(&cls_->main_head, &head, sizeof(s_sis_disk_v1_main_head));
    unit->status = SIS_DISK_STATUS_APPEND | SIS_DISK_STATUS_NOSTOP;
    return 0;
}

int sis_disk_v1_open_rdonly(s_sis_disk_v1 *cls_, s_sis_disk_v1_unit *unit)
{
    if (!unit)
    {
        return -1;
    }
    // SIS_FILE_IO_RSYNC
    unit->fp = sis_open(unit->fn, SIS_FILE_IO_BINARY | SIS_FILE_IO_RSYNC | SIS_FILE_IO_READ, 0 );
    if (unit->fp < 0)
    {
        return -2;
    }
    s_sis_disk_v1_main_head head;
    size_t bytes = sis_read(unit->fp, (char *)&head, sizeof(s_sis_disk_v1_main_head));
    // sis_out_binary("head", (char *)&head, bytes);
    if (bytes != sizeof(s_sis_disk_v1_main_head) || head.style != cls_->main_head.style)
    {
        LOG(5)("style fail. [%d %d] [%d %d]\n", (int)bytes , (int)sizeof(s_sis_disk_v1_main_head), head.style, cls_->main_head.style);
        sis_disk_v1_close(cls_);
        return -4;
    }  
    // 这里要更新init设置的头信息
    memmove(&cls_->main_head, &head, sizeof(s_sis_disk_v1_main_head));
    // printf("-3--ss-- %d\n", cls_->main_head.wtime);
    unit->status = SIS_DISK_STATUS_RDOPEN;

    return 0;
}
int sis_disk_v1_delete(s_sis_disk_v1 *cls_)
{
    for (int  i = 0; i < cls_->lists->count; i++)
    {
        s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, i);
        LOG(8)("delete file %s\n", unit->fn);
        sis_file_delete(unit->fn);
    }
    return cls_->lists->count;
}
int sis_disk_v1_open(s_sis_disk_v1 *cls_, int access_)
{
    cls_->access = access_;
    // printf("-3.1--ss-- %d\n", cls_->main_head.wtime);
    LOG(3)("sis_disk_v1_open count = %d \n", cls_->lists->count);
    switch (cls_->access)
    {
    case SIS_DISK_ACCESS_CREATE:
        {
            for (int  i = 0; i < cls_->lists->count; i++)
            {
                s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, i);
                sis_file_delete(unit->fn);
            }
            sis_pointer_list_delete(cls_->lists, 1, cls_->lists->count - 1);
            s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, 0);
            if (cls_->lists->count < 1)
            {
                return -1;
            }
            cls_->cur_unit = 0;
            sis_disk_v1_open_create(cls_, unit);
        }
        break;
    case SIS_DISK_ACCESS_APPEND:
        {
            cls_->cur_unit = cls_->lists->count - 1;
            s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
            if (cls_->lists->count < 1)
            {
                return -2;
            }
            return sis_disk_v1_open_append(cls_, unit);
        }
        break;    
    default: 
        {
            for (int  i = 0; i < cls_->lists->count; i++)
            {
                s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, i);
                int o = sis_disk_v1_open_rdonly(cls_, unit);
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
    // printf("-3--ss-- %d\n", cls_->main_head.wtime);
    return 0;
}
// int sis_disk_v1_open(s_sis_disk_v1 *cls_, int access_)
// {
//     cls_->access = access_;

//     s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
//     if (!unit)
//     {
//         return -1;
//     }
//     printf("access: %d | %d %d\n", cls_->access, cls_->lists->count, cls_->cur_unit);
//     if (cls_->access == SIS_DISK_ACCESS_CREATE)
//     {
//         sis_check_path(unit->fn);    
//         unit->fp = sis_open(unit->fn, SIS_FILE_IO_TRUNC | SIS_FILE_IO_RDWR | SIS_FILE_IO_CREATE, SIS_FILE_MODE_NORMAL);
//     }
//     else if (cls_->access == SIS_DISK_ACCESS_APPEND)
//     {
//         unit->fp = sis_open(unit->fn, SIS_FILE_IO_RDWR , 0);
//     }
//     else
//     {
//         unit->fp = sis_open(unit->fn, | SIS_FILE_IO_READ , 0);
//     } 
//     if (unit->fp < 0)
//     {
//         return -2;
//     }
//     if (cls_->access == SIS_DISK_ACCESS_CREATE)
//     {
//         sis_write(unit->fp, (const char *)&cls_->main_head, sizeof(s_sis_disk_v1_main_head));
//         unit->offset = sizeof(s_sis_disk_v1_main_head);
//         unit->status = SIS_DISK_STATUS_CREATE | SIS_DISK_STATUS_NOSTOP;
//     }
//     else if (cls_->access == SIS_DISK_ACCESS_APPEND)
//     {
//         s_sis_disk_v1_main_head head;
//         size_t bytes = sis_read(unit->fp, (char *)&head, sizeof(s_sis_disk_v1_main_head));
//         if (bytes != sizeof(s_sis_disk_v1_main_head) || head.style != cls_->main_head.style)
//         {
//             sis_disk_v1_close(cls_);
//             return -3;
//         }     
//         unit->offset = sis_seek(unit->fp, -1 * sizeof(s_sis_disk_v1_main_tail), SEEK_END);
//         unit->status = SIS_DISK_STATUS_APPEND | SIS_DISK_STATUS_NOSTOP;
//     }
//     else
//     {
//         s_sis_disk_v1_main_head head;
//         size_t bytes = sis_read(unit->fp, (char *)&head, sizeof(s_sis_disk_v1_main_head));
//         if (bytes != sizeof(s_sis_disk_v1_main_head) || head.style != cls_->main_head.style)
//         {
//             sis_disk_v1_close(cls_);
//             return -4;
//         }     
//         unit->status = SIS_DISK_STATUS_RDOPEN;
//     } 
//     return 0;
// }
bool sis_disk_v1_seek(s_sis_disk_v1 *cls_)
{ 
    s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
    if (!unit->fp)
    {
        return false;
    }
    // 定位写入的位置 每次定位写盘速度慢
    sis_seek(unit->fp, unit->offset, SEEK_SET);
    return true;
}
size_t sis_disk_v1_offset(s_sis_disk_v1 *cls_)
{
    s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
    if (!unit->fp)
    {
        return 0;
    }
    return unit->offset;
}
size_t sis_disk_v1_write_sync(s_sis_disk_v1 *cls_)
{
    size_t size = 0;
    s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
    if (unit->fp && sis_memory_get_size(unit->wcatch) > 0)
    {
        size = sis_write(unit->fp, sis_memory(unit->wcatch), sis_memory_get_size(unit->wcatch));
        sis_memory_clear(unit->wcatch);
    }
    sis_fsync(unit->fp);
    return size;
}
// 有数据就写 只有stream才同步每一次写入
static size_t sis_safe_write(int __t__,s_sis_memory *__m__,s_sis_handle __f__,
    const char *__v__,size_t __z__)
{
    size_t _osize_ = 0;   
    size_t _curz_ = sis_memory_get_size(__m__); 
    if (_curz_ > 0) { 
        _osize_ = sis_write(__f__, sis_memory(__m__), _curz_); 
        sis_memory_clear(__m__); 
    } 
    _osize_ += sis_write(__f__,__v__,__z__);  
    if (__t__ == SIS_DISK_HID_STREAM) 
    {       
        sis_fsync(__f__); 
    } 
    return _osize_; 
} 


size_t sis_disk_v1_write(s_sis_disk_v1 *cls_, int hid_, s_sis_disk_v1_wcatch *wcatch_)
{ 
    // 传进来的数据 hid == SIS_DISK_HID_STREAM 立即写盘
    s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
    
    // if (wcatch_) 
    // { 
    //     printf("%s %s ",wcatch_->key ? SIS_OBJ_SDS(wcatch_->key) : "N", wcatch_->sdb ? SIS_OBJ_SDS(wcatch_->sdb) : "N");
    // }
    // printf("hid=%d, %p fidx = %d size = %zu\n", hid_, unit, cls_->cur_unit, wcatch_ ? sis_memory_get_size(wcatch_->memory) : 0);
    if (!unit->fp)
    {
        return 0;
    }
    // 定位写入的位置 每次定位写盘速度慢
    // sis_seek(unit->fp, unit->offset, SEEK_SET);

    s_sis_disk_v1_head head;
    head.fin = 1;
    head.hid = hid_;
    head.zip = 0;
    if (!wcatch_)
    {
        sis_safe_write(head.hid, unit->wcatch, unit->fp, (const char *)&head, 1);
        unit->offset += 1;
        return 1;
    }
    size_t size = sis_memory_get_size(wcatch_->memory);
    // 这里对数据压缩, 如果压缩后长度不减反增 就不压缩
    if (cls_->main_head.compress == SIS_DISK_ZIP_SNAPPY)
    {
        sis_memory_clear(cls_->zip_memory);
        if(sis_snappy_compress(sis_memory(wcatch_->memory), size, cls_->zip_memory))
        {
            head.zip = 1;
            size = sis_memory_get_size(cls_->zip_memory);
        }
    }
    if (cls_->max_file_size > 0 && (unit->offset + size) > cls_->max_file_size)
    {
        sis_disk_v1_write_sync(cls_);
        sis_disk_v1_inc_unit(cls_);
        // 创建文件并打开
        unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
        if (sis_disk_v1_open_create(cls_, unit))
        {
            return 0;
        } 
    }
    
    sis_memory_cat(unit->wcatch, (char *)&head, sizeof(s_sis_disk_v1_head));
    // int offset = 
    sis_memory_cat_ssize(unit->wcatch, size);
    if (head.zip)
    {
		size = sis_safe_write(head.hid, unit->wcatch, unit->fp, sis_memory(cls_->zip_memory), size);
    }
    else
    {
		size = sis_safe_write(head.hid, unit->wcatch, unit->fp, sis_memory(wcatch_->memory), size);
    }  
    ///
    // size += offset + sizeof(s_sis_disk_v1_head);
    wcatch_->winfo.fidx = cls_->cur_unit;
    wcatch_->winfo.offset = unit->offset;
    wcatch_->winfo.size = size;
    unit->offset += size;
    // LOG(8)("%d zip = %d size = %zu %zu\n", unit->fp, head.zip, size, unit->offset);
    return size;
}
// size_t sis_disk_v1_write(s_sis_disk_v1 *cls_, int hid_, s_sis_disk_v1_wcatch *wcatch_)
// { 
//     // 传进来的数据 hid == SIS_DISK_HID_STREAM 立即写盘
//     s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
    
//     // if (wcatch_) 
//     // { 
//     //     printf("%s %s ",wcatch_->key ? SIS_OBJ_SDS(wcatch_->key) : "N", wcatch_->sdb ? SIS_OBJ_SDS(wcatch_->sdb) : "N");
//     // }
//     // printf("hid=%d, %p fidx = %d size = %zu\n", hid_, unit, cls_->cur_unit, wcatch_ ? sis_memory_get_size(wcatch_->memory) : 0);
//     if (!unit->fp)
//     {
//         return 0;
//     }
//     // 定位写入的位置 每次定位写盘速度慢
//     // sis_seek(unit->fp, unit->offset, SEEK_SET);

//     s_sis_disk_v1_head head;
//     head.fin = 1;
//     head.hid = hid_;
//     head.zip = 0;
//     if (!wcatch_)
//     {
//         sis_write(unit->fp, (const char *)&head, 1);
//         unit->offset += 1;
//         return 1;
//     }
//     size_t size = sis_memory_get_size(wcatch_->memory);
//     // 这里对数据压缩, 如果压缩后长度不减反增 就不压缩
//     if (cls_->main_head.compress == SIS_DISK_ZIP_SNAPPY)
//     {
//         sis_memory_clear(cls_->zip_memory);
//         if(sis_snappy_compress(sis_memory(wcatch_->memory), size, cls_->zip_memory))
//         {
//             head.zip = 1;
//             size = sis_memory_get_size(cls_->zip_memory);
//         }
//     }
//     if (cls_->max_file_size > 0 && (unit->offset + size) > cls_->max_file_size)
//     {
//         sis_disk_v1_inc_unit(cls_);
//         // 创建文件并打开
//         unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, cls_->cur_unit);
//         if (sis_disk_v1_open_create(cls_, unit))
//         {
//             return 0;
//         } 
//     }
    
//     s_sis_memory *memory = sis_memory_create();
//     sis_memory_cat(memory, (char *)&head, sizeof(s_sis_disk_v1_head));
//     sis_memory_cat_ssize(memory, size);
//     ////
//     ////
//     sis_write(unit->fp, sis_memory(memory), sis_memory_get_size(memory));
//     if (head.zip)
//     {
//       size = sis_write(unit->fp, sis_memory(cls_->zip_memory), size);
//     }
//     else
//     {
//       size = sis_write(unit->fp, sis_memory(wcatch_->memory), size);
//     }  
//     ///
//     size += sis_memory_get_size(memory);
//     sis_memory_destroy(memory); 

//     wcatch_->winfo.fidx = cls_->cur_unit;
//     wcatch_->winfo.offset = unit->offset;
//     wcatch_->winfo.size = size;
//     unit->offset += size;
//     LOG(8)("%d zip = %d size = %zu %zu\n", unit->fp, head.zip, size, unit->offset);
//     return size;
// }


size_t sis_disk_v1_uncompress(s_sis_disk_v1 *cls_, s_sis_disk_v1_head *head_,
    char *in_, size_t ilen_, s_sis_memory *out_)
{
    int    unzip = 0;
    size_t size = ilen_;
    if (head_->zip)
    {
        if (cls_->main_head.compress == SIS_DISK_ZIP_SNAPPY)
        {
            if(sis_snappy_uncompress(in_, ilen_, out_))
            {
                unzip = 1;
                size = sis_memory_get_size(out_);
            }
            else
            {
                printf("%d %zu \n",head_->hid, ilen_);
                LOG(8)("snappy_uncompress fail.\n");
                return 0;   
            }
        }
    }
    // 获取最终数据
    if (!unzip)
    {
        sis_memory_cat(out_, in_, size);
    }
    return size;
}
size_t sis_disk_v1_read_fulltext(s_sis_disk_v1 *cls_, void *source_, cb_sis_disk_v1_read *callback)
{ 
    if (!callback)
    {
        return 0;
    }
    bool isstop = false;
    s_sis_memory *imem = sis_memory_create_size(SIS_MEMORY_SIZE);
    s_sis_memory *omem = sis_memory_create();
    for (int i = 0; i < cls_->lists->count && !isstop; i++)
    {
        s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, i); 

        sis_memory_clear(imem);
        
        bool             FILEEND = false;
        bool             LINEEND = true;
        size_t           size = 0;
        s_sis_disk_v1_head  head;   
        // 从头开始读
        sis_seek(unit->fp, sizeof(s_sis_disk_v1_main_head), SEEK_SET);
        // msec_t _start_msec = sis_time_get_now_msec();
        size_t _mem_size = 0;
        while (!FILEEND && !isstop)
        {
            size_t bytes = sis_memory_read(imem, unit->fp, SIS_MEMORY_SIZE);
            if (bytes <= 0)
            {
                FILEEND = true; // 文件读完了, 但要处理完数据
            }
            _mem_size+=bytes;
            // sis_memory_clear(imem);
            // continue;
            // 缓存不够大就继续读
            if (sis_memory_get_size(imem) < size)
            {
                continue;
            }
            while (sis_memory_get_size(imem) >= SIS_DISK_V1_MIN_BUFFER && !isstop)
            {
                if (LINEEND)
                {
                    memmove(&head, sis_memory(imem), sizeof(s_sis_disk_v1_head));
                    sis_memory_move(imem, sizeof(s_sis_disk_v1_head));
                    if (head.hid == SIS_DISK_HID_SNO_END)
                    {
                        if (callback(source_, &head, NULL) < 0)
                        {
                            // 回调返回 -1 表示已经没有读者了
                            // printf("stop break. end\n");
                            isstop = true;
                            break;
                        }
                        LINEEND = true; 
                        continue;
                    }
                    if (head.hid == SIS_DISK_HID_TAIL)
                    {
                        // 结束
                        size = SIS_DISK_V1_MIN_BUFFER - 1;
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
                    // printf("read----: %d \n", head.hid);
                    // sis_memory_clear(omem); // 解压时会自动清理
                    if (sis_disk_v1_uncompress(cls_, &head, sis_memory(imem), size, omem) > 0)
                    {
                        if (callback(source_, &head, omem) < 0)
                        {
                            // 回调返回 -1 表示已经没有读者了
                            // printf("stop break. sno\n");
                            isstop = true;
                            break;
                        }
                    }
                }
                sis_memory_move(imem, size);
                size = 0;
                LINEEND = true;
            } // while SIS_DISK_V1_MIN_BUFFER
        } // while
        // 读4G文件约60秒 MAC 1秒
        // 解压缩 约 40秒 MAC 21秒
        // 只解析数据 约 160秒 
        // 排序花费时间 840秒- 2050秒 
        // 优化后 300秒
        // printf("%zu cost = %d\n", _mem_size, sis_time_get_now_msec() - _start_msec);
    }
    sis_memory_destroy(imem);
    sis_memory_destroy(omem);
    return 0;
}

// 定位读去某一个块
size_t sis_disk_v1_read(s_sis_disk_v1 *cls_, int fidx_, size_t offset_, size_t size_, uint8 *hid_, s_sis_memory *out_)
{
    s_sis_disk_v1_unit *unit = (s_sis_disk_v1_unit *)sis_pointer_list_get(cls_->lists, fidx_); 
    // printf("unit =%p, fidx = %d : %d | %zu  %zu\n",unit, fidx_, cls_->lists->count, offset_, size_);

    size_t           size = 0; 
    s_sis_disk_v1_head  head;   

    s_sis_memory    *memory = sis_memory_create();

    sis_seek(unit->fp, offset_, SEEK_SET);
    size_t bytes = sis_memory_read(memory, unit->fp, size_);
    if (bytes == size_)
    {
        memmove(&head, sis_memory(memory), sizeof(s_sis_disk_v1_head));
        if (hid_) 
        {
            *hid_ = head.hid;
        }
        sis_memory_move(memory, sizeof(s_sis_disk_v1_head));
        size = sis_memory_get_ssize(memory);
        //结果放在 wcatch_->memory 中
        size = sis_disk_v1_uncompress(cls_, &head, sis_memory(memory), size, out_);
    }
    sis_memory_destroy(memory);  
    // printf("--- hid_ = %d %p size = %zu %zu\n",head.hid, hid_, size, sis_memory_get_size(out_));
    // 这里移动文件指针 效率低
    // sis_seek(unit->fp, unit->offset, SEEK_SET);
    return size;
}
