#include "sis_disk.h"

///////////////////////////
//  s_sis_files
///////////////////////////
s_sis_files *sis_files_create()
{
    s_sis_files *o = SIS_MALLOC(s_sis_files, o);
    o->lists = sis_struct_list_create(sizeof(s_sis_files_unit));
    o->zip_memory = sis_memory_create();
    sis_files_clear(o);
    return o;
}
void sis_files_destroy(void *in_)
{
    s_sis_files *o = (s_sis_files *)in_;
    // 需要关闭所有的文件
    sis_files_close(o);
    sis_struct_list_destroy(o->lists);  
    sis_memory_destroy(o->zip_memory);
    sis_free(o);  
}
void sis_files_clear(s_sis_files *in_)
{
    s_sis_files *o = (s_sis_files *)in_;
    sis_struct_list_clear(o->lists);
    sis_memory_clear(o->zip_memory);

    o->access = SIS_DISK_ACCESS_NOLOAD;
    memset(&o->main_head, 0, sizeof(s_sis_disk_main_head));
    memset(&o->main_tail, 0, sizeof(s_sis_disk_main_tail));
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
void sis_files_close(s_sis_files *cls_)
{
    for (int i = 0; i < cls_->lists->count; i++)
    {
        s_sis_files_unit *unit =(s_sis_files_unit *)sis_struct_list_get(cls_->lists, i);
        if (unit->status != SIS_DISK_STATUS_CLOSED)
        {
            if (cls_->main_head.style != SIS_DISK_TYPE_STREAM && cls_->main_head.style != SIS_DISK_TYPE_LOG)
            {
                if (unit->status & SIS_DISK_STATUS_NOSTOP)
                {
                    cls_->main_tail.count = cls_->lists->count;
                    sis_write(unit->fp, (const char *)&cls_->main_tail, sizeof(s_sis_disk_main_tail));
                    unit->offset += sizeof(s_sis_disk_main_tail);
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
    // sis_struct_list_clear(cls_->lists);
}

void sis_files_init(s_sis_files *cls_, char *fn_)
{
    sis_strcpy(cls_->cur_name, SIS_DISK_NAME_LEN, fn_);
    // 此时需要初始化list 设置一些
    sis_struct_list_clear(cls_->lists);
    int count = 1;
    // 流式文件 和 LOG 文件没有尾部 只有一个文件
    if (cls_->main_head.style != SIS_DISK_TYPE_STREAM && cls_->main_head.style != SIS_DISK_TYPE_LOG)
    {
        if(sis_file_exists(cls_->cur_name))
        {
            int fp = sis_open(cls_->cur_name, SIS_FILE_IO_BINARY | SIS_FILE_IO_RSYNC | SIS_FILE_IO_READ , 0);
            s_sis_disk_main_tail tail;
            sis_seek(fp, -1 * (int)sizeof(s_sis_disk_main_tail), SEEK_END);  
            sis_read(fp, (char *)&tail, sizeof(s_sis_disk_main_tail));
            count = tail.count;
            LOG(3)("init files [%s] %d count = %d\n", cls_->cur_name, tail.hid, count);
            sis_close(fp);      
        }
    }
    for (int i = 0; i < count; i++)
    {
        sis_files_inc_unit(cls_);
    }
}    

int sis_files_inc_unit(s_sis_files *cls_)
{
    s_sis_files_unit unit;
    memset(&unit, 0, sizeof(s_sis_files_unit));
    unit.fp = -1;
    // 有新增 一定是有新文件产生
    if(cls_->lists->count < 1)
    {
        sis_strcpy(unit.fn, SIS_DISK_NAME_LEN, cls_->cur_name);
    }
    else
    {
        sis_sprintf(unit.fn, SIS_DISK_NAME_LEN, "%s.%d", cls_->cur_name, cls_->lists->count);
    }
    cls_->cur_unit = cls_->lists->count;
    return sis_struct_list_push(cls_->lists, &unit);
}
int sis_files_open_create(s_sis_files *cls_, s_sis_files_unit *unit)
{
    if (!unit)
    {
        return -1;
    }
    sis_check_path(unit->fn);    
    unit->fp = sis_open(unit->fn, SIS_FILE_IO_DSYNC | SIS_FILE_IO_TRUNC | SIS_FILE_IO_RDWR | SIS_FILE_IO_CREATE, SIS_FILE_MODE_NORMAL);
    if (unit->fp < 0)
    {
        return -2;
    }
    sis_write(unit->fp, (const char *)&cls_->main_head, sizeof(s_sis_disk_main_head));
    unit->offset = sizeof(s_sis_disk_main_head);
    unit->status = SIS_DISK_STATUS_CREATE | SIS_DISK_STATUS_NOSTOP;
    return 0;
}
int sis_files_open_append(s_sis_files *cls_, s_sis_files_unit *unit)
{
    if (!unit)
    {
        return -1;
    }
    unit->fp = sis_open(unit->fn, SIS_FILE_IO_BINARY | SIS_FILE_IO_DSYNC | SIS_FILE_IO_RDWR, 0 );
    if (unit->fp < 0)
    {
        return -2;
    }
    s_sis_disk_main_head head;
    size_t bytes = sis_read(unit->fp, (char *)&head, sizeof(s_sis_disk_main_head));
    if (bytes != sizeof(s_sis_disk_main_head) || head.style != cls_->main_head.style)
    {
        sis_files_close(cls_);
        return -3;
    }     
    if (cls_->main_head.style != SIS_DISK_TYPE_STREAM && cls_->main_head.style != SIS_DISK_TYPE_LOG)
    {
        unit->offset = sis_seek(unit->fp, -1 * (int)sizeof(s_sis_disk_main_tail), SEEK_END);
    }
    else
    {
        unit->offset = sis_seek(unit->fp, 0, SEEK_END);
    }
    
    unit->status = SIS_DISK_STATUS_APPEND | SIS_DISK_STATUS_NOSTOP;
    return 0;
}

int sis_files_open_rdonly(s_sis_files *cls_, s_sis_files_unit *unit)
{
    if (!unit)
    {
        return -1;
    }
    unit->fp = sis_open(unit->fn, SIS_FILE_IO_BINARY | SIS_FILE_IO_RSYNC | SIS_FILE_IO_READ, 0 );
    if (unit->fp < 0)
    {
        return -2;
    }
    s_sis_disk_main_head head;
    size_t bytes = sis_read(unit->fp, (char *)&head, sizeof(s_sis_disk_main_head));
    // sis_out_binary("head", (char *)&head, bytes);
    if (bytes != sizeof(s_sis_disk_main_head) || head.style != cls_->main_head.style)
    {
        LOG(5)("style fail. [%d %d] [%d %d]\n", (int)bytes , (int)sizeof(s_sis_disk_main_head), head.style, cls_->main_head.style);
        sis_files_close(cls_);
        return -4;
    }     
    unit->status = SIS_DISK_STATUS_RDOPEN;

    return 0;
}
int sis_files_delete(s_sis_files *cls_)
{
    for (int  i = 0; i < cls_->lists->count; i++)
    {
        s_sis_files_unit *unit = (s_sis_files_unit *)sis_struct_list_get(cls_->lists, i);
        LOG(8)("delete file %s\n", unit->fn);
        sis_file_delete(unit->fn);
    }
    return cls_->lists->count;
}
int sis_files_open(s_sis_files *cls_, int access_)
{
    cls_->access = access_;
    LOG(3)("sis_files_open count = %d \n", cls_->lists->count);
    switch (cls_->access)
    {
    case SIS_DISK_ACCESS_CREATE:
        {
            for (int  i = 0; i < cls_->lists->count; i++)
            {
                s_sis_files_unit *unit = (s_sis_files_unit *)sis_struct_list_get(cls_->lists, i);
                sis_file_delete(unit->fn);
            }
            sis_struct_list_delete(cls_->lists, 1, cls_->lists->count - 1);
            s_sis_files_unit *unit = (s_sis_files_unit *)sis_struct_list_get(cls_->lists, 0);
            if (cls_->lists->count < 1)
            {
                return -1;
            }
            cls_->cur_unit = 0;
            sis_files_open_create(cls_, unit);
        }
        break;
    case SIS_DISK_ACCESS_APPEND:
        {
            cls_->cur_unit = cls_->lists->count - 1;
            s_sis_files_unit *unit = (s_sis_files_unit *)sis_struct_list_get(cls_->lists, cls_->cur_unit);
            if (cls_->lists->count < 1)
            {
                return -2;
            }
            return sis_files_open_append(cls_, unit);
        }
        break;    
    default: 
        {
            for (int  i = 0; i < cls_->lists->count; i++)
            {
                s_sis_files_unit *unit = (s_sis_files_unit *)sis_struct_list_get(cls_->lists, i);
                int o = sis_files_open_rdonly(cls_, unit);
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
// int sis_files_open(s_sis_files *cls_, int access_)
// {
//     cls_->access = access_;

//     s_sis_files_unit *unit = (s_sis_files_unit *)sis_struct_list_get(cls_->lists, cls_->cur_unit);
//     if (!unit)
//     {
//         return -1;
//     }
//     printf("access: %d | %d %d\n", cls_->access, cls_->lists->count, cls_->cur_unit);
//     if (cls_->access == SIS_DISK_ACCESS_CREATE)
//     {
//         sis_check_path(unit->fn);    
//         unit->fp = sis_open(unit->fn, SIS_FILE_IO_DSYNC | SIS_FILE_IO_TRUNC | SIS_FILE_IO_RDWR | SIS_FILE_IO_CREATE, SIS_FILE_MODE_NORMAL);
//     }
//     else if (cls_->access == SIS_DISK_ACCESS_APPEND)
//     {
//         unit->fp = sis_open(unit->fn, SIS_FILE_IO_DSYNC | SIS_FILE_IO_RDWR , 0);
//     }
//     else
//     {
//         unit->fp = sis_open(unit->fn, SIS_FILE_IO_RSYNC | SIS_FILE_IO_READ , 0);
//     } 
//     if (unit->fp < 0)
//     {
//         return -2;
//     }
//     if (cls_->access == SIS_DISK_ACCESS_CREATE)
//     {
//         sis_write(unit->fp, (const char *)&cls_->main_head, sizeof(s_sis_disk_main_head));
//         unit->offset = sizeof(s_sis_disk_main_head);
//         unit->status = SIS_DISK_STATUS_CREATE | SIS_DISK_STATUS_NOSTOP;
//     }
//     else if (cls_->access == SIS_DISK_ACCESS_APPEND)
//     {
//         s_sis_disk_main_head head;
//         size_t bytes = sis_read(unit->fp, (char *)&head, sizeof(s_sis_disk_main_head));
//         if (bytes != sizeof(s_sis_disk_main_head) || head.style != cls_->main_head.style)
//         {
//             sis_files_close(cls_);
//             return -3;
//         }     
//         unit->offset = sis_seek(unit->fp, -1 * sizeof(s_sis_disk_main_tail), SEEK_END);
//         unit->status = SIS_DISK_STATUS_APPEND | SIS_DISK_STATUS_NOSTOP;
//     }
//     else
//     {
//         s_sis_disk_main_head head;
//         size_t bytes = sis_read(unit->fp, (char *)&head, sizeof(s_sis_disk_main_head));
//         if (bytes != sizeof(s_sis_disk_main_head) || head.style != cls_->main_head.style)
//         {
//             sis_files_close(cls_);
//             return -4;
//         }     
//         unit->status = SIS_DISK_STATUS_RDOPEN;
//     } 
//     return 0;
// }

size_t sis_files_write(s_sis_files *cls_, int hid_, s_sis_disk_wcatch *wcatch_)
{ 
   // 传进来的数据一定是要写盘的了

    s_sis_files_unit *unit = (s_sis_files_unit *)sis_struct_list_get(cls_->lists, cls_->cur_unit);
    
    if (wcatch_) 
    { 
        printf("%s %s ",wcatch_->key ? SIS_OBJ_SDS(wcatch_->key) : "N", wcatch_->sdb ? SIS_OBJ_SDS(wcatch_->sdb) : "N");
    }
    printf("hid=%d, %p fidx = %d size = %zu\n", hid_, unit, cls_->cur_unit, wcatch_ ? sis_memory_get_size(wcatch_->memory) : 0);
    if (!unit->fp)
    {
        return 0;
    }
    // 定位写入的位置
    sis_seek(unit->fp, unit->offset, SEEK_SET);

    s_sis_disk_head head;
    head.fin = 1;
    head.hid = hid_;
    head.zip = 0;
    if (!wcatch_)
    {
        sis_write(unit->fp, (const char *)&head, 1);
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
        sis_files_inc_unit(cls_);
        // 创建文件并打开
        unit = (s_sis_files_unit *)sis_struct_list_get(cls_->lists, cls_->cur_unit);
        if (sis_files_open_create(cls_, unit))
        {
            return 0;
        } 
    }

    s_sis_memory *memory = sis_memory_create();
    sis_memory_cat(memory, (char *)&head, sizeof(s_sis_disk_head));
    sis_memory_cat_ssize(memory, size);
    sis_write(unit->fp, sis_memory(memory), sis_memory_get_size(memory));
    if (head.zip)
    {
      size = sis_write(unit->fp, sis_memory(cls_->zip_memory), size);
    }
    else
    {
      size = sis_write(unit->fp, sis_memory(wcatch_->memory), size);
    }  
    size += sis_memory_get_size(memory);
    sis_memory_destroy(memory); 

    wcatch_->winfo.fidx = cls_->cur_unit;
    wcatch_->winfo.offset = unit->offset;
    wcatch_->winfo.size = size;
    unit->offset += size;
    LOG(8)("%d zip = %d size = %zu %zu\n", unit->fp, head.zip, size, unit->offset);
    return size;
}

size_t sis_files_uncompress(s_sis_files *cls_, s_sis_disk_head *head_,
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
size_t sis_files_read_fulltext(s_sis_files *cls_, void *source_, cb_sis_files_read *callback)
{ 
    if (!callback)
    {
        return 0;
    }
    for (int i = 0; i < cls_->lists->count; i++)
    {
        s_sis_files_unit *unit = (s_sis_files_unit *)sis_struct_list_get(cls_->lists, i); 

        s_sis_memory    *memory = sis_memory_create();
        
        bool             FILEEND = false;
        bool             LINEEND = true;
        size_t           size = 0;
        s_sis_disk_head  head;   
        // 从头开始读
        sis_seek(unit->fp, sizeof(s_sis_disk_main_head), SEEK_SET);
        while (!FILEEND)
        {
            SIGNAL_EXIT_FAST
            size_t bytes = sis_memory_read(memory, unit->fp, SIS_MEMORY_SIZE);
            if (bytes <= 0)
            {
                FILEEND = true; // 文件读完了, 但要处理完数据
            }
            // 缓存不够大就继续读
            if (sis_memory_get_size(memory) < size)
            {
                continue;
            }
            while (sis_memory_get_size(memory) >= SIS_DISK_MIN_BUFFER)
            {
                if (LINEEND)
                {
                    memmove(&head, sis_memory(memory), sizeof(s_sis_disk_head));
                    sis_memory_move(memory, sizeof(s_sis_disk_head));
                    if (head.hid == SIS_DISK_HID_SNO_END)
                    {
                        callback(source_, &head, NULL);
                        LINEEND = true; 
                        continue;
                    }
                    if (head.hid == SIS_DISK_HID_TAIL)
                    {
                        // 结束
                        size = SIS_DISK_MIN_BUFFER - 1;
                        FILEEND = true; 
                        break;
                    }
                    // 读取数据区长度
                    size = sis_memory_get_ssize(memory);
                    if (sis_memory_get_size(memory) < size)
                    {
                        LINEEND = false;
                        break;
                    }
                }
                if (head.hid != SIS_DISK_HID_NONE)
                {
                    // printf("read----: %d \n", head.hid);
                    s_sis_object *obj = sis_object_create(SIS_OBJECT_MEMORY, sis_memory_create());
                    if (sis_files_uncompress(cls_, &head, sis_memory(memory), size, SIS_OBJ_MEMORY(obj)) > 0)
                    {
                        callback(source_, &head, obj);
                    }
                    sis_object_destroy(obj);
                }
                sis_memory_move(memory, size);
                size = 0;
                LINEEND = true;
            } // while SIS_DISK_MIN_BUFFER
        } // while
        sis_memory_destroy(memory);
    }
    return 0;
}

// 定位读去某一个块
size_t sis_files_read(s_sis_files *cls_, int fidx_, size_t offset_, size_t size_, uint8 *hid_, s_sis_memory *out_)
{
    s_sis_files_unit *unit = (s_sis_files_unit *)sis_struct_list_get(cls_->lists, fidx_); 
    printf("unit =%p, fidx = %d : %d | %zu  %zu\n",unit, fidx_, cls_->lists->count, offset_, size_);

    size_t           size = 0; 
    s_sis_disk_head  head;   

    s_sis_memory    *memory = sis_memory_create();

    sis_seek(unit->fp, offset_, SEEK_SET);
    size_t bytes = sis_memory_read(memory, unit->fp, size_);
    if (bytes == size_)
    {
        memmove(&head, sis_memory(memory), sizeof(s_sis_disk_head));
        if (hid_) 
        {
            *hid_ = head.hid;
        }
        sis_memory_move(memory, sizeof(s_sis_disk_head));
        size = sis_memory_get_ssize(memory);
        //结果放在 wcatch_->memory 中
        size = sis_files_uncompress(cls_, &head, sis_memory(memory), size, out_);
    }
    sis_memory_destroy(memory);  
    // 这里移动文件指针 效率低
    // sis_seek(unit->fp, unit->offset, SEEK_SET);
    return size;
}
