
#include "sis_disk.io.h"

////////////////////////////////////////////////////////
// s_sis_disk_map
////////////////////////////////////////////////////////

s_sis_disk_map *sis_disk_map_create()
{
    s_sis_disk_map *o = SIS_MALLOC(s_sis_disk_map, o);
    o->map_keys = sis_map_list_create(sis_sdsfree_call);;
    o->map_sdbs = sis_map_list_create(sis_dynamic_db_destroy);
    o->map_year = sis_map_list_create(NULL);
    o->map_date = sis_map_list_create(NULL);
    return o;
}
void sis_disk_map_destroy(void *map_)
{
    s_sis_disk_map *map = (s_sis_disk_map *)map_;
    sis_map_list_destroy(map->map_year);
    sis_map_list_destroy(map->map_date);
    sis_map_list_destroy(map->map_sbds);
    sis_map_list_destroy(map->map_keys);
    sis_free(map);
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
    return (s_sis_dynamic_db *)sis_pointer_list_get(dict_->sdbs, dict_->units->count - 1);
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
const char *sis_disk_kdict_get_name(s_sis_map_list *map_kdict_, int keyi_)
{
    return SIS_OBJ_SDS(((s_sis_disk_kdict *)sis_map_list_geti(map_kdict_, keyi_))->name);
}
const char *sis_disk_sdict_get_name(s_sis_map_list *map_sdict_, int sdbi_)
{
    return SIS_OBJ_SDS(((s_sis_disk_sdict *)sis_map_list_geti(map_sdict_, sdbi_))->name);
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

