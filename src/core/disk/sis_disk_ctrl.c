
#include "sis_disk.io.h"

///////////////////////////
//  s_sis_disk_idx
///////////////////////////
s_sis_disk_idx *sis_disk_idx_create(s_sis_object *key_, s_sis_object *sdb_)
{
    s_sis_disk_idx *o = SIS_MALLOC(s_sis_disk_idx, o);
    if (key_)
    {
        o->kname = sis_object_incr(key_);
    }
    if (sdb_)
    {
        o->sname = sis_object_incr(sdb_);
    }
    o->idxs = sis_struct_list_create(sizeof(s_sis_disk_idx_unit));
    return o;
}
void sis_disk_idx_destroy(void *in_)
{
    s_sis_disk_idx *o = (s_sis_disk_idx *)in_;
    sis_object_decr(o->kname);
    sis_object_decr(o->sname);
    sis_struct_list_destroy(o->idxs);
    sis_free(o);
}
int sis_disk_idx_set_unit(s_sis_disk_idx *cls_, s_sis_disk_idx_unit *unit_)
{
    return sis_struct_list_push(cls_->idxs, unit_);
}

s_sis_disk_idx_unit *sis_disk_idx_get_unit(s_sis_disk_idx *cls_, int index_)
{
    return (s_sis_disk_idx_unit *)sis_struct_list_get(cls_->idxs, index_);
}

s_sis_disk_idx *sis_disk_idx_get(s_sis_map_list *map_, s_sis_object *kname_,  s_sis_object *sname_)
{
    if (!kname_)
    {
        return NULL;
    }
    s_sis_sds key = sis_sdsnew(SIS_OBJ_SDS(kname_));
    if (sname_)
    {
        key = sis_sdscatfmt(key, ".%s", SIS_OBJ_SDS(sname_));
    }
    s_sis_disk_idx *node = (s_sis_disk_idx *)sis_map_list_get(map_, key);
    if (!node)
    {
        node = sis_disk_idx_create(kname_, sname_);
        sis_map_list_set(map_, key, node);
    }
    sis_sdsfree(key);
    return node;
}

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
    if (in_->kname)
    {
        sis_object_decr(in_->kname);
        in_->kname = sis_object_incr(kname_);
    }
    if (in_->sname)
    {
        sis_object_decr(in_->sname);
        in_->sname = sis_object_incr(sname_);
    }
}
////////////////////////////////////////////////////////
// s_sis_disk_rcatch
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
    sis_sdsfree(o->sub_keys);
    sis_sdsfree(o->sub_sdbs);
    sis_free(o);
}
void sis_disk_rcatch_clear(s_sis_disk_rcatch *in_)
{
    in_->head.fin = 1;
    in_->head.hid = 0;
    in_->head.zip = 0;
    sis_memory_clear(in_->memory);
    in_->rinfo = NULL;
    sis_sdsfree(in_->sub_keys);  in_->sub_keys = NULL;
    sis_sdsfree(in_->sub_sdbs);  in_->sub_sdbs = NULL;
    in_->callback = NULL;
    memset(&in_->search_msec, 0, sizeof(s_sis_msec_pair));
}
void sis_disk_rcatch_init_of_idx(s_sis_disk_rcatch *in_, s_sis_disk_idx_unit *rinfo_)
{
    in_->head.fin = 1;
    in_->head.hid = 0;
    in_->head.zip = 0;
    sis_memory_clear(in_->memory);
    in_->rinfo = rinfo_;

    sis_sdsfree(in_->sub_keys);  in_->sub_keys = NULL;
    sis_sdsfree(in_->sub_sdbs);  in_->sub_sdbs = NULL;
    in_->callback = NULL;
    memset(&in_->search_msec, 0, sizeof(s_sis_msec_pair));
}
void sis_disk_rcatch_init_of_sub(s_sis_disk_rcatch *in_, const char *subkeys_, const char *subsdbs_,
    s_sis_msec_pair *search_, void *cb_)
{
    sis_disk_rcatch_init_of_idx(in_, NULL);
    if (subkeys_)
    {
        in_->sub_keys = sis_sdsnew(subkeys_);
    }
    else
    {
        in_->sub_keys = sis_sdsnew("*");
    }
    if (subsdbs_)
    {
        in_->sub_sdbs = sis_sdsnew(subsdbs_);
    }
    else
    {
        in_->sub_sdbs = sis_sdsnew("*");
    }
    in_->iswhole = 0;
    if (!sis_strcasecmp(in_->sub_keys, "*") && !sis_strcasecmp(in_->sub_sdbs, "*"))
    {
        in_->iswhole = 1;
    }
    in_->callback = cb_;
    if (search_)
    {
        memmove(&in_->search_msec, search_, sizeof(s_sis_msec_pair));
    }
}

////////////////////////////////////////////////////////
// s_sis_disk_kdict
////////////////////////////////////////////////////////

s_sis_disk_kdict *sis_disk_kdict_create(const char *name_)
{
    s_sis_disk_kdict *o = SIS_MALLOC(s_sis_disk_kdict, o);
    o->name = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(name_));
    return o;
}
void sis_disk_kdict_destroy(void *in_)
{
    s_sis_disk_kdict *o = (s_sis_disk_kdict *)in_;
    sis_object_destroy(o->name);
    sis_free(o);
}
////////////////////////////////////////////////////////
// s_sis_disk_sdict
////////////////////////////////////////////////////////

s_sis_disk_sdict *sis_disk_sdict_create(const char *name_)
{
    s_sis_disk_sdict *o = SIS_MALLOC(s_sis_disk_sdict, o);
    o->name = sis_object_create(SIS_OBJECT_SDS, sis_sdsnew(name_));
    o->sdbs = sis_pointer_list_create();
    o->sdbs->vfree = sis_dynamic_db_destroy;
    return o;
}

void sis_disk_sdict_destroy(void *in_)
{
    s_sis_disk_sdict *o = (s_sis_disk_sdict *)in_;
    sis_pointer_list_destroy(o->sdbs);
    sis_object_destroy(o->name);
    sis_free(o);
}

s_sis_dynamic_db *sis_disk_sdict_last(s_sis_disk_sdict *dict_)
{
    return (s_sis_dynamic_db *)sis_pointer_list_get(dict_->sdbs, dict_->sdbs->count - 1);
}

s_sis_dynamic_db *sis_disk_sdict_get(s_sis_disk_sdict *dict_, int index_)
{
    return (s_sis_dynamic_db *)sis_pointer_list_get(dict_->sdbs, index_);
}
// 设置新的结构体 返回-1 需要清理 db_
int sis_disk_sdict_set(s_sis_disk_sdict *dict_, s_sis_dynamic_db * db_)
{
    if (db_)
    {
        s_sis_dynamic_db *last = sis_disk_sdict_last(dict_);
        if (!last || !sis_dynamic_dbinfo_same(last, db_))
        {
            sis_pointer_list_push(dict_->sdbs, db_);
            return dict_->sdbs->count - 1;
        }
    }
    return -1;
}

////////////////////////////////////////////
s_sis_object *sis_disk_kdict_get_name(s_sis_map_list *map_kdict_, int keyi_)
{
    return ((s_sis_disk_kdict *)sis_map_list_geti(map_kdict_, keyi_))->name;
}
s_sis_object *sis_disk_sdict_get_name(s_sis_map_list *map_sdict_, int sdbi_)
{
    return ((s_sis_disk_sdict *)sis_map_list_geti(map_sdict_, sdbi_))->name;
}
int sis_disk_kdict_get_idx(s_sis_map_list *map_kdict_, const char *kname_)
{
    return ((s_sis_disk_kdict *)sis_map_list_get(map_kdict_, kname_))->index;
}
int sis_disk_sdict_get_idx(s_sis_map_list *map_sdict_, const char *sname_)
{
    return ((s_sis_disk_sdict *)sis_map_list_get(map_sdict_, sname_))->index;
}

int sis_disk_reader_set_kdict(s_sis_map_list *map_kdict_, const char *in_, size_t ilen_)
{
    if (!in_ || ilen_ < 1)
    {
        return 0;
    }
    s_sis_string_list *klist = sis_string_list_create();
    sis_string_list_load(klist, in_, ilen_, ",");
    int count = sis_string_list_getsize(klist);
    for (int i = 0; i < count; i++)
    {
        const char *key = sis_string_list_get(klist, i);
        s_sis_disk_kdict *kdict = sis_map_list_get(map_kdict_, key);
        if (!kdict)
        {
            kdict = sis_disk_kdict_create(key);
            kdict->index = sis_map_list_set(map_kdict_, key, kdict);
        }
    }
    sis_string_list_destroy(klist);
    return sis_map_list_getsize(map_kdict_);
}  

int sis_disk_reader_set_sdict(s_sis_map_list *map_sdict_, const char *in_, size_t ilen_)
{
    if (!in_ || ilen_ < 16)
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
            s_sis_disk_sdict *sdict = sis_map_list_get(map_sdict_, innode->key);
            if (!sdict)
            {
                sdict = sis_disk_sdict_create(innode->key);
                sdict->index = sis_map_list_set(map_sdict_, innode->key, sdict);
            }
            if (sis_disk_sdict_set(sdict, newdb) < 0)
            {
                sis_dynamic_db_destroy(newdb);
            }
        }  
        innode = sis_json_next_node(innode);
    }
    sis_json_close(injson); 
    return sis_map_list_getsize(map_sdict_);
}  

s_sis_disk_kdict *sis_disk_map_get_kdict(s_sis_map_list *map_kdict_, const char *kname_)
{
    s_sis_disk_kdict *kdict = sis_map_list_get(map_kdict_, kname_);
    return kdict;
}
 
s_sis_disk_sdict *sis_disk_map_get_sdict(s_sis_map_list *map_sdict_, const char *sname_)
{
    s_sis_disk_sdict *sdict = sis_map_list_get(map_sdict_, sname_);
    return sdict;
}

