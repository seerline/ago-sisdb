
#include "sis_disk.h"

////////////////////////////////////////////////////////
// s_sis_disk_wcatch
////////////////////////////////////////////////////////

s_sis_disk_wcatch *sis_disk_wcatch_create(s_sis_object *kname_, s_sis_object *sname_)
{
    s_sis_disk_wcatch *o = SIS_MALLOC(s_sis_disk_wcatch, o);
    if (kname_)
    {
        o->kname = sis_object_incr(kname_);
    }
    if (sname_)
    {
        o->sname = sis_object_incr(sname_);
    }
    o->memory = sis_memory_create();
    return o;
}
void sis_disk_wcatch_destroy(void *in_)
{
    s_sis_disk_wcatch *o = (s_sis_disk_wcatch *)in_;
    sis_object_decr(o->kname);
    sis_object_decr(o->sname);
    sis_memory_destroy(o->memory);
    sis_free(o);
}
void sis_disk_wcatch_init(s_sis_disk_wcatch *in_)
{
    in_->head.fin = 1;
    in_->head.hid = 0;
    in_->head.zip = 0;
    sis_memory_clear(in_->memory);
}
void sis_disk_wcatch_clear(s_sis_disk_wcatch *in_)
{
    in_->head.fin = 1;
    in_->head.hid = 0;
    in_->head.zip = 0;
    sis_memory_clear(in_->memory);
    memset(&in_->winfo, 0, sizeof(s_sis_disk_idx_unit));
}
void sis_disk_wcatch_setname(s_sis_disk_wcatch *in_, s_sis_object *kname_, s_sis_object *sname_)
{
    if (o->kname)
    {
        sis_object_decr(o->kname);
        o->kname = sis_object_incr(kname_);
    }
    if (o->sname)
    {
        sis_object_decr(o->sname);
        o->sname = sis_object_incr(sname_);
    }
}
////////////////////////////////////////////////////////
// s_sis_disk_wcatch
////////////////////////////////////////////////////////

s_sis_disk_rcatch *sis_disk_rcatch_create(s_sis_disk_idx_unit *rinfo_)
{
    s_sis_disk_rcatch *o = SIS_MALLOC(s_sis_disk_rcatch, o);
    o->memory = sis_memory_create();
    o->rinfo = rinfo_;
    return o;
}
void sis_disk_rcatch_destroy(void *in_)
{
    s_sis_disk_rcatch *o = (s_sis_disk_rcatch *)in_;
    sis_memory_destroy(o->memory);
    sis_free(o);
}
void sis_disk_rcatch_init(s_sis_disk_rcatch *in_, s_sis_disk_idx_unit *rinfo_)
{
    in_->head.fin = 1;
    in_->head.hid = 0;
    in_->head.zip = 0;
    sis_memory_clear(in_->memory);
    in_->rinfo = rinfo_;
}

///////////////////////////
//  s_sis_disk_index
///////////////////////////
s_sis_disk_index *sis_disk_index_create(s_sis_object *key_, s_sis_object *sdb_)
{
    s_sis_disk_index *o = SIS_MALLOC(s_sis_disk_index, o);
    if (key_)
    {
        o->key = sis_object_incr(key_);
    }
    if (sdb_)
    {
        o->sdb = sis_object_incr(sdb_);
    }
    o->index = sis_struct_list_create(sizeof(s_sis_disk_index_unit));
    return o;
}
void sis_disk_index_destroy(void *in_)
{
    s_sis_disk_index *o = (s_sis_disk_index *)in_;
    sis_object_decr(o->key);
    sis_object_decr(o->sdb);
    sis_struct_list_destroy(o->index);
    sis_free(o);
}
int sis_disk_index_set_unit(s_sis_disk_index *cls_, s_sis_disk_index_unit *unit_)
{
    return sis_struct_list_push(cls_->index, unit_);
}

s_sis_disk_index_unit *sis_disk_index_get_unit(s_sis_disk_index *cls_, int index_)
{
    return (s_sis_disk_index_unit *)sis_struct_list_get(cls_->index, index_);
}

s_sis_disk_index *sis_disk_index_get(s_sis_map_list *map_, const char *kname_,  const char *sname_)
{
    if (!kname_)
    {
        return NULL;
    }
    char key[255];
    if (sname_)
    {
        sis_sprintf(key, 255, "%s.%s", kname_, sname_);
    }
    else
    {   
        sis_sprintf(key, 255, "%s", kname_);
    }
    s_sis_disk_index *node = (s_sis_disk_index *)sis_map_list_get(map_, key);
    if (!node)
    {
        node = sis_disk_index_create(kname_, kname_);
        sis_map_list_set(map_, key, node);
    }
    return node;
}

////////////////////////////////////////////////////////
// public function
////////////////////////////////////////////////////////


size_t sis_disk_io_uncompress(s_sis_disk_ctrl *cls_, s_sis_disk_rcatch *rcatch_)
{
    size_t size = 0;
    if (rcatch_->head.zip == SIS_DISK_ZIP_NOZIP)
    {
        return sis_memory_get_size(rcatch_->memory);
    }
    if (rcatch_->head->zip == SIS_DISK_ZIP_SNAPPY)
    {
        s_sis_memory *memory = sis_memory_create();
        if(sis_snappy_uncompress(sis_memory(rcatch_->memory), sis_memory_get_size(rcatch_->memory), memory))
        {
            size = sis_memory_get_size(memory);
            sis_memory_swap(rcatch_->memory, memory);
        }
        else
        {
            LOG(5)("snappy_uncompress fail. %d %zu \n", head_->hid, ilen_);
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
    if (rcatch_->head->zip == SIS_DISK_ZIP_INCRZIP)
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
            sis_incrzip_clear(cls_->wincrzip);
            sis_incrzip_set_sdb(cls_->wincrzip, db);
            if(sis_incrzip_uncompress(cls_->wincrzip, sis_memory(rcatch_->memory), sis_memory_get_size(rcatch_->memory), unzipmemory) > 0)
            {
                sis_memory_swap(rcatch_->memory, unzipmemory);
                size = sis_memory_get_size(rcatch_->memory);
            }
            else
            {
                LOG(8)("incrzip uncompress fail.\n", 
            }
            sis_memory_destroy(unzipmemory); 
        }
    }
    return size;
}