#include "sis_disk.h"

///////////////////////////
//  write
///////////////////////////



////////////////////
//  write
///////////////////

size_t sis_disk_file_write_log(s_sis_disk_ctrl *cls_, void *in_, size_t ilen_)
{
    size_t size = 0;
    
    if (cls_->work_fps->main_head.style == SIS_DISK_TYPE_LOG)
    {
        s_sis_disk_wcatch *wcatch = cls_->wcatch;
        sis_disk_wcatch_init(wcatch);
        sis_memory_cat(wcatch->memory, (char *)in_, ilen_);
        // 流式文件不压缩 每次必写盘
        wcatch->head.hid = SIS_DISK_HID_MSG_LOG;
        size = sis_disk_files_write_saveidx(cls_->work_fps, wcatch); 
    }
    return size;
}

size_t sis_disk_file_write_sdb_sno(s_sis_disk_ctrl *cls_,
                                   s_sis_disk_dict *key, s_sis_disk_dict *sdb, void *in_, size_t ilen_)
{
    s_sis_disk_dict_unit *unit = sis_disk_dict_last(sdb);
    if (!unit && (ilen_ % unit->db->size) != 0)
    {
        return 0;
    }
    if (cls_->wzipsize > cls_->work_fps->max_page_size)
    {
        s_sis_disk_wcatch *wcatch = cls_->wcatch;
        wcatch->head.hid = SIS_DISK_HID_SNO_END;
        cls_->wcount++;
        sis_memory_cat_ssize(wcatch->memory, cls_->wcount); 
        sis_disk_write_work(cls_, wcatch);
        sis_disk_wcatch_init(wcatch);
        cls_->wzipsize = 0;
        sis_incrzip_compress_restart(cls_->wincrzip);
    }
    int count = ilen_ / unit->db->size;
    for (int i = 0; i < count; i++)
    {
        sis_incrzip_compress_step(cls_->wincrzip, key->index, sdb->index, (char *)in_ + i * unit->db->size, unit->db->size);
    }
    return 0;
}

static int sis_write_sdb_merge(s_sis_disk_ctrl *cls_, s_sis_disk_wcatch *wcatch_)
{
    s_sis_disk_idx *node = sis_disk_idx_get(cls_->map_idxs, wcatch_->key, wcatch_->sdb);
    // 判断来源数据是否时序的数据
    // 如果来源数据不是序列化的就直接删除老的 
    if ((wcatch_->winfo.start == 0 && wcatch_->winfo.stop == 0) || 
         wcatch_->winfo.start > wcatch_->winfo.stop)
    {
        sis_struct_list_clear(node->index);
    }   
    else
    {
        // 暂时处理为时间有重叠就覆盖老的
        int idx = 0;
        while(idx < node->index->count)
        {
            s_sis_disk_idx_unit *unit = (s_sis_disk_idx_unit *)sis_struct_list_get(node->index, idx);
            if (sis_is_mixed(wcatch_->winfo.start, wcatch_->winfo.stop, unit->start, unit->stop))
            {
                sis_struct_list_delete(node->index, idx, 1);
                continue;
            }
            idx++;
        }   
    }     
    return 0;
}

size_t sis_disk_file_write_sdb_tsno(s_sis_disk_ctrl *cls_,
                                    s_sis_disk_dict *key, s_sis_disk_dict *sdb, void *in_, size_t ilen_)
{
    size_t size = 0;

    s_sis_disk_dict_unit *unit = sis_disk_dict_last(sdb);
    if (unit && (ilen_ % unit->db->size) == 0)
    {
        s_sis_disk_wcatch *wcatch = sis_disk_wcatch_create(key->name, sdb->name);

        int count = ilen_ / unit->db->size;
        wcatch->winfo.start = sis_dynamic_db_get_time(unit->db, 0, in_, ilen_);
        wcatch->winfo.stop = sis_dynamic_db_get_time(unit->db, count - 1, in_, ilen_);
        // printf("write : %d %d %d \n", count, wcatch->winfo.start , wcatch->winfo.stop);
        wcatch->winfo.kdict = key->units->count;
        wcatch->winfo.sdict = sdb->units->count;
        // 这里可以判断新的块和老的块是否有冲突 甚至老的和新的数据结构不同也可以在这里处理
        sis_write_sdb_merge(cls_, wcatch);

        sis_memory_cat_ssize(wcatch->memory, key->index);
        sis_memory_cat_ssize(wcatch->memory, sdb->index);
        // 这里压缩数据开始
        s_sis_memory *zipmem = sis_memory_create();
        sis_incrzip_clear(cls_->wincrzip);
        sis_incrzip_set_sdb(cls_->wincrzip, unit->db);
        if(sis_incrzip_compress(cls_->wincrzip, in_, ilen_, zipmem) > 0)
        {
            wcatch->head.zip = SIS_DISK_ZIP_INCRZIP;
            sis_memory_cat(wcatch->memory, sis_memory(zipmem), sis_memory_get_size(zipmem));
        }
        else
        {
            wcatch->head.zip = SIS_DISK_ZIP_NOZIP;
            sis_memory_cat(wcatch->memory, in_, ilen_);
        }
        sis_memory_destroy(zipmem);
        // 这里压缩数据结束
        wcatch->head.hid = SIS_DISK_HID_MSG_SDB;
        size = sis_disk_write_work(cls_, wcatch);
        sis_disk_wcatch_destroy(wcatch);
    }
    return size;
}
size_t sis_disk_file_write_sdb_nots(s_sis_disk_ctrl *cls_,
                                    s_sis_disk_dict *key, s_sis_disk_dict *sdb, void *in_, size_t ilen_)
{
    size_t size = 0;

    s_sis_disk_dict_unit *unit = sis_disk_dict_last(sdb);
    if (unit && (ilen_ % unit->db->size) == 0)
    {
        s_sis_disk_wcatch *wcatch = sis_disk_wcatch_create(key->name, sdb->name);

        int count = ilen_ / unit->db->size;
        wcatch->winfo.start = sis_dynamic_db_get_time(unit->db, 0, in_, ilen_);
        wcatch->winfo.stop = sis_dynamic_db_get_time(unit->db, count - 1, in_, ilen_);
        // printf("write : %d %d %d \n", count, wcatch->winfo.start , wcatch->winfo.stop);
        wcatch->winfo.kdict = key->units->count;
        wcatch->winfo.sdict = sdb->units->count;
        // 这里可以判断新的块和老的块是否有冲突 甚至老的和新的数据结构不同也可以在这里处理
        sis_write_sdb_merge(cls_, wcatch);

        sis_memory_cat_ssize(wcatch->memory, key->index);
        sis_memory_cat_ssize(wcatch->memory, sdb->index);
        // 这里压缩数据开始
        s_sis_memory *zipmem = sis_memory_create();
        if(sis_snappy_compress(in_, ilen_, zipmem))
        {
            wcatch->head.zip = SIS_DISK_ZIP_SNAPPY;
            sis_memory_cat(wcatch->memory, sis_memory(zipmem), sis_memory_get_size(zipmem));
        }
        else
        {
            wcatch->head.zip = SIS_DISK_ZIP_NOZIP;
            sis_memory_cat(wcatch->memory, in_, ilen_);
        }
        sis_memory_destroy(zipmem);
        // 这里压缩数据结束
        wcatch->head.hid = SIS_DISK_HID_MSG_NON;
        size = sis_disk_write_work(cls_, wcatch);
        sis_disk_wcatch_destroy(wcatch);
    }
    return size;
}
// 到这里的写入 已经保证了数据在当前时间范围 不用再去判断时间区域问题 
size_t sis_disk_file_write_sdbi(s_sis_disk_ctrl *cls_, int keyi_, int sdbi_, void *in_, size_t ilen_)
{
    if (!in_ || !ilen_)
    {
        return 0;
    }
    int size = 0;
    s_sis_disk_dict *key = (s_sis_disk_dict *)sis_map_list_geti(cls_->keys, keyi_);
    s_sis_disk_dict *sdb = (s_sis_disk_dict *)sis_map_list_geti(cls_->sdbs, sdbi_);
    if (key && sdb)
    {
        switch (cls_->work_fps->main_head.style)
        {
        case SIS_DISK_TYPE_SNO:
            size = sis_disk_file_write_sdb_sno(cls_, key_, sdb_, in_, ilen_);
            break;
        case SIS_DISK_TYPE_SDB_YEAR:
        case SIS_DISK_TYPE_SDB_DATE:
            size = sis_disk_file_write_sdb_tsno(cls_, key_, sdb_, in_, ilen_);
            break;
        case SIS_DISK_TYPE_SDB_NOTS:
            size = sis_disk_file_write_sdb_nots(cls_, key_, sdb_, in_, ilen_);
            break;
        default: // 其他结构不支持
            break;
        }
    } 
    return size;    
}

// newkeys
s_sis_sds sis_disk_file_get_keys(s_sis_disk_ctrl *cls_, bool onlyincr_)
{
    s_sis_sds msg = sis_sdsempty();
    int nums = 0;
    {
        int count = sis_map_list_getsize(cls_->keys);
        for(int i = 0; i < count; i++)
        {
            s_sis_disk_dict *info = (s_sis_disk_dict *)sis_map_list_geti(cls_->keys, i);
            for(int k = 0; k < info->units->count; k++)
            {
                s_sis_disk_dict_unit *unit = sis_disk_dict_get(info, k);
                // LOG(1)("w writed = %s %d\n", SIS_OBJ_GET_CHAR(info->name), unit->writed);
                if (onlyincr_ && unit->writed)
                {
                    continue;
                }
                if (nums > 0)
                {
                    msg = sis_sdscatfmt(msg, ",%s", SIS_OBJ_SDS(info->name));
                }
                else
                {
                    msg = sis_sdscatfmt(msg, "%s", SIS_OBJ_SDS(info->name));
                }
                nums++;
                if (onlyincr_) 
                {
                    unit->writed = 1;
                }      
            }
        }
    }
    return msg;
}
size_t sis_disk_file_write_key_dict(s_sis_disk_ctrl *cls_)
{
    size_t size = 0;
    // 写 键表
    s_sis_sds msg = sis_disk_file_get_keys(cls_, true);
    if (sis_sdslen(msg) > 2)
    {
        s_sis_object *mapobj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(SIS_DISK_SIGN_KEY));
        s_sis_disk_wcatch *wcatch = sis_disk_wcatch_create(mapobj, NULL);
        sis_memory_cat(wcatch->memory, msg, sis_sdslen(msg));
        wcatch->head.hid = SIS_DISK_HID_DICT_KEY;
        size = sis_disk_write_work(cls_, wcatch);
        sis_disk_wcatch_destroy(wcatch);
        sis_object_destroy(mapobj);
    }
    sis_sdsfree(msg);
    return size;
}
s_sis_sds sis_disk_file_get_sdbs(s_sis_disk_ctrl *cls_, bool onlyincr_)
{
    int count = sis_map_list_getsize(cls_->sdbs);
    s_sis_json_node *sdbs_node = sis_json_create_object();
    {
        for(int i = 0; i < count; i++)
        {
            s_sis_disk_dict *info = (s_sis_disk_dict *)sis_map_list_geti(cls_->sdbs, i);
            // printf("+++++ %d %d %s\n",count, info->units->count, SIS_OBJ_SDS(info->name));
            for(int k = 0; k < info->units->count; k++)
            {
                s_sis_disk_dict_unit *unit = sis_disk_dict_get(info, k);
                // LOG(1)("w writed = %s %d %d\n", SIS_OBJ_GET_CHAR(info->name), unit->writed, unit->db->size);
                if (onlyincr_ && unit->writed)
                {
                    continue;
                }
                if (onlyincr_) 
                {
                    unit->writed = 1;
                }
                sis_json_object_add_node(sdbs_node, SIS_OBJ_SDS(info->name), sis_dynamic_dbinfo_to_json(unit->db));
            }
        }
    }
    s_sis_sds msg = sis_json_to_sds(sdbs_node, true);
    sis_json_delete_node(sdbs_node);
    return msg;
}

size_t sis_disk_file_write_sdb_dict(s_sis_disk_ctrl *cls_)
{
    size_t size = 0;
    s_sis_sds msg = sis_disk_file_get_sdbs(cls_, true);
    printf("new sdb:%s\n", msg);
    if (sis_sdslen(msg) > 2)
    {
        s_sis_object *mapobj = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(SIS_DISK_SIGN_SDB));
        s_sis_disk_wcatch *wcatch = sis_disk_wcatch_create(mapobj, NULL);
        sis_memory_cat(wcatch->memory, msg, sis_sdslen(msg));
        wcatch->head.hid = SIS_DISK_HID_DICT_SDB;
        size = sis_disk_write_work(cls_, wcatch);
        sis_disk_wcatch_destroy(wcatch);
        sis_object_destroy(mapobj);
    }
    sis_sdsfree(msg);
    return size;
}

// 索引每次都重写一遍
size_t sis_disk_file_write_index(s_sis_disk_ctrl *cls_)
{
    if (!sis_disk_files_seek(cls_->widx_fps))
    {
        return 0;
    }
    s_sis_disk_wcatch *wcatch = cls_->wcatch;
    sis_disk_wcatch_init(wcatch);

    size_t size = 0;
    // 到这里头已经写了 尾不用写，直接写中间的内容
    s_sis_memory *memory = wcatch->memory;
    sis_memory_clear(memory);
    s_sis_disk_idx *keynode = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, SIS_DISK_SIGN_KEY);
    if (keynode)
    {
        sis_memory_cat_ssize(memory, keynode->index->count);
        for (int k = 0; k < keynode->index->count; k++)
        {
            s_sis_disk_idx_unit *unit = sis_disk_idx_get_unit(keynode, k);
            sis_memory_cat_ssize(memory, unit->fidx);
            sis_memory_cat_ssize(memory, unit->offset);
            sis_memory_cat_ssize(memory, unit->size);
        }
        wcatch->head.hid = SIS_DISK_HID_INDEX_KEY;
        size += sis_disk_files_write_saveidx(cls_->widx_fps, wcatch);
    }
    sis_memory_clear(memory);
    s_sis_disk_idx *sdbnode = (s_sis_disk_idx *)sis_map_list_get(cls_->map_idxs, SIS_DISK_SIGN_SDB);
    if (sdbnode)
    {
        sis_memory_cat_ssize(memory, sdbnode->index->count);
        for (int k = 0; k < sdbnode->index->count; k++)
        {
            s_sis_disk_idx_unit *unit = sis_disk_idx_get_unit(sdbnode, k);
            sis_memory_cat_ssize(memory, unit->fidx);
            sis_memory_cat_ssize(memory, unit->offset);
            sis_memory_cat_ssize(memory, unit->size);
        }
        wcatch->head.hid = SIS_DISK_HID_INDEX_SDB;
        size += sis_disk_files_write_saveidx(cls_->widx_fps, wcatch);
    }
    int count = sis_map_list_getsize(cls_->map_idxs);
    sis_memory_clear(memory);
    for (int i = 0; i < count; i++)
    {
        s_sis_disk_idx *node = (s_sis_disk_idx *)sis_map_list_geti(cls_->map_idxs, i);
        // 排除key和sdb
        if (node == sdbnode || node == keynode)
        {
            continue;
        }
        // printf("write_index %s %d \n", SIS_OBJ_SDS(node->key), node->index->count);
        size_t klen = sis_sdslen(SIS_OBJ_SDS(node->key));
        sis_memory_cat_ssize(memory, klen);
        sis_memory_cat(memory, SIS_OBJ_SDS(node->key), klen);
        if (node->sdb)
        {
            size_t slen = sis_sdslen(SIS_OBJ_SDS(node->sdb));
            sis_memory_cat_ssize(memory, slen);
            sis_memory_cat(memory, SIS_OBJ_SDS(node->sdb), slen);
        }
        else
        {
            sis_memory_cat_ssize(memory, 0);
        }        
        sis_memory_cat_ssize(memory, node->index->count);
        for (int k = 0; k < node->index->count; k++)
        {
            s_sis_disk_idx_unit *unit = (s_sis_disk_idx_unit *)sis_struct_list_get(node->index, k);
            sis_memory_cat_byte(memory, unit->active, 1);
            sis_memory_cat_byte(memory, unit->kdict, 1);
            sis_memory_cat_byte(memory, unit->sdict, 1);
            sis_memory_cat_ssize(memory, unit->fidx);
            sis_memory_cat_ssize(memory, unit->offset);
            sis_memory_cat_ssize(memory, unit->size);
            sis_memory_cat_ssize(memory, unit->start);
            sis_memory_cat_ssize(memory, unit->stop - unit->start);
        }
        if (sis_memory_get_size(memory) > cls_->widx_fps->max_page_size)
        {
            wcatch->head.hid = SIS_DISK_HID_INDEX_MSG;
            size += sis_disk_files_write_saveidx(cls_->widx_fps, wcatch);    
            sis_memory_clear(memory);
        }   
    }
    if (sis_memory_get_size(memory) > 0)
    {
        wcatch->head.hid = SIS_DISK_HID_INDEX_MSG;
        size += sis_disk_files_write_saveidx(cls_->widx_fps, wcatch);    
        sis_memory_clear(memory);
    }   
    sis_disk_files_write_sync(cls_->widx_fps);
    LOG(5)("write_index end %zu \n", size);
    return size;
}

int sis_disk_file_write_start(s_sis_disk_ctrl *cls_)
{
    if (!cls_->style)
    {
        return -10;
    }
    int access = SIS_DISK_ACCESS_APPEND;
    if (!sis_file_exists(cls_->work_fps->cur_name))
    {
        access = SIS_DISK_ACCESS_CREATE;
    }
    // printf("-5--ss-- %d\n", cls_->work_fps->main_head.wtime);
    if (access == SIS_DISK_ACCESS_CREATE)
    {
        // 以新建新文件的方式打开文件 如果以前有文件直接删除创建新的
        if (sis_disk_files_open(cls_->work_fps, SIS_DISK_ACCESS_CREATE))
        {
            LOG(2)("create file fail. [%s]\n", cls_->work_fps->cur_name);
            return -5;
        }
    }
    else
    {
        // 以在文件后面追加数据的方式打开文件
        // 对已经存在的文件进行合法性检查 如果文件不完整 就打开失败 由外部程序来处理异常，这样相对安全
        int vo = sis_disk_file_valid_work(cls_);
        if ( vo != SIS_DISK_CMD_OK)
        {
            LOG(5)
            ("open is no valid.[%s]\n", cls_->work_fps->cur_name);
            return -2;
        }
        if (sis_disk_files_open(cls_->work_fps, SIS_DISK_ACCESS_APPEND))
        {
            LOG(5)
            ("open file fail.[%s]\n", cls_->work_fps->cur_name);
            return -3;
        }    
        if (cls_->work_fps->main_head.index && sis_disk_file_valid_widx(cls_) == SIS_DISK_CMD_OK)
        {
            sis_map_list_clear(cls_->map_idxs);
            if (sis_disk_files_open(cls_->widx_fps, SIS_DISK_ACCESS_RDONLY))
            {
                LOG(5)
                ("open idxfile fail.[%s]\n", cls_->widx_fps->cur_name);
                return -4;
            }
            // 加载索引项
            sis_disk_files_read_fulltext(cls_->widx_fps, cls_, cb_sis_disk_file_read_idx);
            sis_disk_file_read_dict(cls_);
            sis_disk_files_close(cls_->widx_fps);
        }        
    } 
    // 打开已有文件会先加载字典 写字典表如果发现和加载的字典表一样就不重复写入

    // 写文件前强制修正一次写入位置
    if (!sis_disk_files_seek(cls_->work_fps))
    {
        return -8;
    }
    // 到这里字典都加载完毕 需要根据文件类型初始化其他一些参数
    sis_disk_file_incrzip_wopen(cls_);

    cls_->status = SIS_DISK_STATUS_OPENED;
    return 0;
}

// 写入结束标记，并生成索引文件
int sis_disk_file_write_stop(s_sis_disk_ctrl *cls_)
{
    if (cls_->status == SIS_DISK_STATUS_CLOSED)
    {
        return 0;
    }
    sis_disk_file_incrzip_wstop(cls_);
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
            LOG(5)
            ("open idxfile fail.[%s] %d\n", cls_->widx_fps->cur_name, cls_->widx_fps->main_head.workers);
            return -2;
        }
        sis_disk_file_write_index(cls_);
        sis_disk_files_close(cls_->widx_fps);
    }
    // 必须等索引写好才关闭工作句柄
    sis_disk_files_close(cls_->work_fps);

    sis_disk_class_clear(cls_);

    cls_->status = SIS_DISK_STATUS_CLOSED;
    return 0;
}
