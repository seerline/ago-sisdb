
#include "sis_disk.io.h"
void sis_disk_set_search_msec(s_sis_msec_pair *src_, s_sis_msec_pair *des_)
{
    des_->start = 0;
    des_->stop  = 0;
    if (src_)
    {
        des_->start = src_->start;
        des_->stop  = src_->stop;
    }
    des_->start = des_->start ? des_->start : sis_time_make_time(sis_time_get_idate(0), 0) * 1000;
    des_->stop = des_->stop ? des_->stop : sis_time_make_time(sis_time_get_idate(0), 235959) * 1000 + 999;
}
int sis_disk_get_sdb_scale(s_sis_dynamic_db *db_)
{
    if (!db_->field_time)
    {
        return SIS_SDB_SCALE_NOTS;
    }
    if (db_->field_time->style == SIS_DYNAMIC_TYPE_DATE)
    {
        return SIS_SDB_SCALE_YEAR;
    }
//  SIS_DYNAMIC_TYPE_WSEC   'W'  // 微秒 8  
//  SIS_DYNAMIC_TYPE_MSEC   'T'  // 毫秒 8  
//  SIS_DYNAMIC_TYPE_TSEC    'S'  // 秒   4 8  
//  SIS_DYNAMIC_TYPE_MINU   'M'  // 分钟 4 time_t/60
    return SIS_SDB_SCALE_DATE;
}

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
    sis_object_decr(in_->kname);
    if (kname_)
    {
        in_->kname = sis_object_incr(kname_);
    }
    else
    {
        in_->kname = NULL;
    }
    sis_object_decr(in_->sname);
    if (sname_)
    {
        in_->sname = sis_object_incr(sname_);
    }
    else
    {
        in_->sname = NULL;
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

/**
 * @brief 
 * @param in_ 
 * @param subkeys_ 需要读取行情的股票列表
 * @param subsdbs_ 需要读取行情的数据格式，JSON
 * @param search_ 
 * @param cb_ 回调函数组合
 */
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
    sis_disk_set_search_msec(search_, &in_->search_msec);

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
bool sis_disk_sdict_isnew(s_sis_disk_sdict *dict_, s_sis_dynamic_db * db_)
{
    if (db_)
    {
        s_sis_dynamic_db *last = sis_disk_sdict_last(dict_);
        if (!last || !sis_dynamic_dbinfo_same(last, db_))
        {
            return true;
        }
    }
    return false;
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
            if (sis_disk_sdict_isnew(sdict, newdb))
            {
                sis_dynamic_db_incr(newdb);
                sis_pointer_list_push(sdict->sdbs, newdb);
            }
            sis_dynamic_db_decr(newdb);
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


////////////////////////////////////////////////////////
// s_sis_disk_map
////////////////////////////////////////////////////////


s_sis_disk_map *sis_disk_map_create(s_sis_object *kname_, s_sis_object *sname_)
{
    s_sis_disk_map *o = SIS_MALLOC(s_sis_disk_map, o);
    if (kname_)
    {
        o->kname = sis_object_incr(kname_);
    }
    if (sname_)
    {
        o->sname = sis_object_incr(sname_);
    }
    o->sidxs = sis_sort_list_create(sizeof(s_sis_disk_map_unit));
    o->sidxs->isascend = 1;
    return o;
}
void sis_disk_map_destroy(void *in_)
{
    s_sis_disk_map *o = (s_sis_disk_map *)in_;
    sis_sort_list_destroy(o->sidxs);
    if (o->kname)
    {
        sis_object_decr(o->kname);
    }
    if (o->sname)
    {
        sis_object_decr(o->sname);
    }
    sis_free(o);  
}

int sis_disk_map_merge(s_sis_disk_map *agomap_, s_sis_disk_map *newmap_)
{
    int count = sis_sort_list_getsize(newmap_->sidxs);
    for (int k = 0; k < count; k++)
    {
        s_sis_disk_map_unit *newunit = sis_sort_list_get(newmap_->sidxs, k);
        sis_sort_list_set(agomap_->sidxs, newunit->idate, newunit);
    } 
    return 0;
}


// #if 0

// int __nums = 0;
// size_t __size = 0;

// static void cb_begin1(void *src, msec_t tt)
// {
//     printf("%s : %llu\n", __func__, tt);
// }
// static void cb_key1(void *src, void *key_, size_t size) 
// {
//     __size += size;
//     s_sis_sds info = sis_sdsnewlen((char *)key_, size);
//     printf("%s %d :%s\n", __func__, (int)size, info);
//     sis_sdsfree(info);
// }
// static void cb_sdb1(void *src, void *sdb_, size_t size)  
// {
//     __size += size;
//     s_sis_sds info = sis_sdsnewlen((char *)sdb_, size);
//     printf("%s %d : %s\n", __func__, (int)size, info);
//     sis_sdsfree(info);
// }

// static void cb_read_stream1(void *src, const char *key_, const char *sdb_, s_sis_object *obj_)
// {
//     __nums++;
//     __size += sis_memory_get_size((s_sis_memory *)obj_->ptr);
//     // if (__nums < 2 || __nums % 1000 == 0)
//     {
//         printf("%s : %d %zu\n", __func__, __nums, __size);
//         sis_out_binary(" ", sis_memory((s_sis_memory *)obj_->ptr), 16);
//                 // sis_memory_get_size((s_sis_memory *)obj_->ptr));
//     }
// } 

// static void cb_read1(void *src, const char *key_, const char *sdb_, s_sis_object *obj_)
// {
//     __nums++;
//     __size += sis_memory_get_size((s_sis_memory *)obj_->ptr);
//     // if (__nums < 2 || __nums % 100 == 0)
//     {
//         printf(" %s : [%d] %zu ",__func__, __nums, __size);
//         if (key_) printf(" %s ", key_);
//         if (sdb_) printf(" %s ", sdb_);
        
//         s_sis_disk_ctrl *rwf = (s_sis_disk_ctrl *)src;
//         s_sis_disk_dict *sdb = (s_sis_disk_dict *)sis_map_list_get(rwf->sdbs, sdb_);
//         if (sdb)
//         {
//             s_sis_disk_dict_unit *unit =  sis_disk_dict_last(sdb);
//             s_sis_sds info = sis_sdb_to_csv_sds(unit->db, SIS_OBJ_GET_CHAR(obj_), SIS_OBJ_GET_SIZE(obj_)); 
//             printf(" %s \n", info);
//             sis_sdsfree(info);
//         }
//         else
//         {
//             printf("\n");
//             sis_out_binary("data:", SIS_OBJ_GET_CHAR(obj_), SIS_OBJ_GET_SIZE(obj_));
//         }
        
//     }
// } 
// static void cb_end1(void *src, msec_t tt)
// {
//     printf("%s : %llu\n", __func__, tt);
// }
// // test stream
// int test_stream()
// {
//     // s_sis_sds s = sis_sdsempty();
//     // char c = 'A';
//     // s = sis_sdscatfmt(s,"%s", &c);
//     // printf("%c %s\n", c, s);
//     // return 0;
//     sis_log_open(NULL, 10, 0);
//     safe_memory_start();
//     s_sis_disk_ctrl *rwf = sis_disk_ctrl_create();
//     // 先写
//     sis_disk_class_init(rwf, SIS_DISK_TYPE_STREAM, "dbs", "1111.aof");

//     sis_disk_ctrl_write_start(rwf);

//     int count = 1*1000*1000;
//     sis_disk_class_set_size(rwf, 400*1000, 20*1000);
//     // sis_disk_class_set_size(rwf, 0, 300*1000);
//     for (int i = 0; i < count; i++)
//     {
//         sis_disk_ctrl_write_log(rwf, "012345678987654321", 18);
//     }  
//     sis_disk_ctrl_write_stop(rwf);
//     // 后读
//     printf("write end .\n");

//     sis_disk_ctrl_read_start(rwf);
//     s_sis_disk_callback *callback = SIS_MALLOC(s_sis_disk_callback, callback);
//     callback->source = rwf;
//     callback->cb_begin = cb_begin1;
//     callback->cb_read = cb_read_stream1;
//     callback->cb_end = cb_end1;
//     __nums = 0;

//     s_sis_disk_reader *reader = sis_disk_reader_create(callback);
//     sis_disk_ctrl_read_sub(rwf, reader);

//     sis_disk_ctrl_read_stop(rwf);
//     sis_disk_reader_destroy(reader);
//     sis_free(callback);
//     printf("read end __nums = %d \n", __nums);

//     sis_disk_ctrl_destroy(rwf);
//     safe_memory_stop();
//     sis_log_close();
//     return 0;
// }


// char *keys = "{\"k1\",\"k3\",\"k3\"}";
// // char *keys = "{\"k1\":{\"dp\":2},\"k2\":{\"dp\":2},\"k3\":{\"dp\":3}}";
// char *sdbs = "{\"info\":{\"fields\":{\"name\":[\"C\",10]}},\"snap\":{\"fields\":{\"time\":[\"U\",8],\"newp\":[\"F\",8,1,2],\"vol\":[\"I\",4]}}}";
// #pragma pack(push,1)
// typedef struct s_info
// {
//     char name[10];
// }s_info;
// typedef struct s_snap
// {
//     uint64 time;
//     double newp; 
//     uint32 vol;
// }s_snap;
// #pragma pack(pop)
// s_info info_data[3] = { 
//     { "k10000" },
//     { "k20000" },
//     { "k30000" },
// };
// s_snap snap_data[10] = { 
//     { 1000, 128.561, 3000},
//     { 2000, 129.562, 4000},
//     { 3000, 130.563, 5000},
//     { 4000, 121.567, 6000},
//     { 5000, 122.561,  7000},
//     { 6000, 123.562,  8000},
//     { 7000, 124.563,  9000},
//     { 8000, 125.564,  9900},
//     { 9000, 124.563,  9000},
//     { 10000, 125.564,  9900},
// };
// size_t write_hq_1(s_sis_disk_ctrl *rwf)
// {
//     size_t size = 0;
//     int count  = 1000;
//     for (int k = 0; k < count; k++)
//     {
//         for (int i = 0; i < 8; i++)
//         {
//             size += sis_disk_ctrl_write_sdb(rwf, "k1", "snap", &snap_data[i], sizeof(s_snap));
//         }
//         // printf("%zu\n", size);
//         for (int i = 0; i < 5; i++)
//         {
//             size += sis_disk_ctrl_write_sdb(rwf, "k2", "snap", &snap_data[i], sizeof(s_snap));
//         }
//         // printf("%zu\n", size);
//         for (int i = 0; i < 7; i++)
//         {
//             size += sis_disk_ctrl_write_sdb(rwf, "k3", "snap", &snap_data[i], sizeof(s_snap));
//         }
//     }
//     return size;
// }

// size_t write_hq(s_sis_disk_ctrl *rwf)
// {
//     size_t size = 0;
//     int count  = 3000;
//     s_snap snap;
//     int sno = 1;
//     for (int k = 0; k < count; k++)
//     {        
//         for (int i = 0; i < 10; i++)
//         {
//             snap.time = sno++;
//             snap.newp = 100.12 + i;
//             snap.vol  = 10000*k + i;
//             size += sis_disk_ctrl_write_sdb(rwf, "k1", "snap", &snap, sizeof(s_snap));
//         }
//         // printf("%zu\n", size);
//         for (int i = 0; i < 10; i++)
//         {
//             snap.time = sno++;
//             snap.newp = 200.12 + i;
//             snap.vol  = 20000*k + i;
//             size += sis_disk_ctrl_write_sdb(rwf, "k2", "snap", &snap, sizeof(s_snap));
//         }
//         // printf("%zu\n", size);
//         for (int i = 0; i < 10; i++)
//         {
//             snap.time = sno++;
//             snap.newp = 400.12 + i;
//             snap.vol  = 40000*k + i;
//             size += sis_disk_ctrl_write_sdb(rwf, "k3", "snap", &snap, sizeof(s_snap));
//         }
//     }
//     return size;
// }
// size_t write_after(s_sis_disk_ctrl *rwf)
// {
//     size_t size = 0;
//     int count  = 500;
//     int sno = 1;
//     for (int k = 0; k < count; k++)
//     {        
//         for (int i = 0; i < 10; i++)
//         {
//             snap_data[i].time = sno++;
//             snap_data[i].newp = 100.12 + i;
//             snap_data[i].vol  = 10000*k + i;
//         }
//         size += sis_disk_ctrl_write_sdb(rwf, "k1", "snap", &snap_data[0], 10 * sizeof(s_snap));
//         // printf("%zu\n", size);
//         for (int i = 0; i < 10; i++)
//         {
//             snap_data[i].time = sno++;
//             snap_data[i].newp = 200.12 + i;
//             snap_data[i].vol  = 20000*k + i;
//         }
//         size += sis_disk_ctrl_write_sdb(rwf, "k2", "snap", &snap_data[0], 10 * sizeof(s_snap));
//         // printf("%zu\n", size);
//         for (int i = 0; i < 10; i++)
//         {
//             snap_data[i].time = sno++;
//             snap_data[i].newp = 400.12 + i;
//             snap_data[i].vol  = 40000*k + i;
//         }
//         size += sis_disk_ctrl_write_sdb(rwf, "k3", "snap", &snap_data[0], 10 * sizeof(s_snap));
//     }
//     return size;
// }
// void write_log(s_sis_disk_ctrl *rwf)
// {
//     sis_disk_ctrl_write_start(rwf);
    
//     // 先写
//     sis_disk_class_write_key(rwf, keys, sis_strlen(keys));
//     sis_disk_class_write_sdb(rwf, sdbs, sis_strlen(sdbs));

//     // int count = 1*1000*1000;
//     sis_disk_class_set_size(rwf, 400*1000000, 20*1000);
//     // sis_disk_class_set_size(rwf, 0, 300*1000);    
//     size_t size = sis_disk_ctrl_write_sdb(rwf, "k1", "info", &info_data[0], sizeof(s_info));
//     size += sis_disk_ctrl_write_sdb(rwf, "k2", "info", &info_data[1], sizeof(s_info));
//     size += sis_disk_ctrl_write_sdb(rwf, "k3", "info", &info_data[2], sizeof(s_info));
    
//     size+= write_hq(rwf);

//     printf("style : %d, size = %zu\n", rwf->work_fps->main_head.style, size);

//     sis_disk_ctrl_write_stop(rwf);
    
//     printf("write end\n");
// }

// void write_sdb(s_sis_disk_ctrl *rwf, char *keys_, char *sdbs_)
// {
//     sis_disk_ctrl_write_start(rwf);

//     sis_disk_class_write_key(rwf, keys_, sis_strlen(keys_));
//     sis_disk_class_write_sdb(rwf, sdbs_, sis_strlen(sdbs_));

//     // int count = 1*1000*1000;
//     sis_disk_class_set_size(rwf, 400*1000, 20*1000);
//     // sis_disk_class_set_size(rwf, 0, 300*1000);    
//     size_t size = sis_disk_ctrl_write_sdb(rwf, "k1", "info", &info_data[0], sizeof(s_info));
//     size += sis_disk_ctrl_write_sdb(rwf, "k2", "info", &info_data[1], sizeof(s_info));
//     size += sis_disk_ctrl_write_sdb(rwf, "k3", "info", &info_data[2], sizeof(s_info));
    
//     size+= write_after(rwf);

//     printf("%zu\n", size);

//     sis_disk_ctrl_write_stop(rwf);
    
//     printf("write end\n");
// }
// void read_of_sub(s_sis_disk_ctrl *rwf)
// {
//     // 后读
//     sis_disk_ctrl_read_start(rwf);
    
//     s_sis_disk_callback *callback = SIS_MALLOC(s_sis_disk_callback, callback);
//     callback->source = rwf;
//     callback->cb_begin = cb_begin1;
//     callback->cb_key = cb_key1;
//     callback->cb_sdb = cb_sdb1;
//     callback->cb_read = cb_read1;
//     callback->cb_end = cb_end1;

//     s_sis_disk_reader *reader = sis_disk_reader_create(callback);
//     sis_disk_reader_set_key(reader, "*");
//     // sis_disk_reader_set_key(reader, "k1");
//     // sis_disk_reader_set_sdb(reader, "snap");
//     sis_disk_reader_set_sdb(reader, "*");
//     // sis_disk_reader_set_sdb(reader, "info");
//     // sis_disk_reader_set_stime(reader, 1000, 1500);

//     __nums = 0;
//     // sub 是一条一条的输出
//     sis_disk_ctrl_read_sub(rwf, reader);
//     // get 是所有符合条件的一次性输出
//     // sis_disk_ctrl_read_get(rwf, reader);

//     sis_disk_reader_destroy(reader);
//     sis_free(callback);

//     sis_disk_ctrl_read_stop(rwf);
    
//     printf("read end __nums = %d \n", __nums);

// }

// void rewrite_sdb(s_sis_disk_ctrl *rwf)
// {
//     // 先写

//     printf("write ----1----. begin:\n");
//     sis_disk_ctrl_write_start(rwf);

//     sis_disk_class_write_key(rwf, keys, sis_strlen(keys));
//     sis_disk_class_write_sdb(rwf, sdbs, sis_strlen(sdbs));

//     size_t size = 0;
//     size += sis_disk_ctrl_write_sdb(rwf, "k1", "info", &info_data[0], sizeof(s_info));
//     size += sis_disk_ctrl_write_sdb(rwf, "k2", "info", &info_data[1], sizeof(s_info));
//     size += sis_disk_ctrl_write_sdb(rwf, "k3", "info", &info_data[2], sizeof(s_info));
//     // 只写单键值没问题
//     // 必须对已经存在的文件 加载key和sdb后设置为 iswrite 
//     size += sis_disk_ctrl_write_any(rwf, "anykey", "my is dzd.", 10);
//     sis_disk_ctrl_write_stop(rwf);

//     printf("write end. [%zu], read 1:\n", size);
//     read_of_sub(rwf);
//     printf("write ----2----. begin:\n");

//     // sis_disk_ctrl_write_start(rwf);
//     // printf("write ----2----. begin:\n");
//     // size = sis_disk_ctrl_write_any(rwf, "anykey", "my is ding.", 11);
//     // printf("write ----2----. begin:\n");
//     // size += sis_disk_ctrl_write_any(rwf, "anykey1", "my is xp.", 9);
//     // printf("write ----2----. begin:\n");
//     // sis_disk_ctrl_write_stop(rwf);
//     // printf("write end. [%zu], read 2:\n", size);

// }
// void pack_sdb(s_sis_disk_ctrl *src)
// {
//     s_sis_disk_ctrl *des = sis_disk_ctrl_create(SIS_DISK_TYPE_SDB_NOTS ,"dbs", "20200101");  
//     sis_disk_class_init(des, SIS_DISK_TYPE_SDB_NOTS, "debug1", "db");
//     callback->cb_source(src, des);
//     sis_disk_ctrl_destroy(des);
// }
// int main(int argc, char **argv)
// {
//     sis_log_open(NULL, 10, 0);
//     safe_memory_start();

// // test stream
//     // test_stream();
// // test log
//     // s_sis_disk_ctrl *rwf = sis_disk_ctrl_create(SIS_DISK_TYPE_LOG ,"dbs", "20200101");  
//     // write_log(rwf);
//     // read_of_sub(rwf);
//     // sis_disk_ctrl_destroy(rwf);

// // test sno
//     // s_sis_disk_ctrl *rwf = sis_disk_ctrl_create();//SIS_DISK_TYPE_SNO ,"dbs", "20200101");  
//     // sis_disk_class_init(rwf, SIS_DISK_TYPE_SNO, "dbs", "20200101");
//     // write_log(rwf);
//     // read_of_sub(rwf);
//     // sis_disk_ctrl_destroy(rwf);

// // test sdb
//     s_sis_disk_ctrl *rwf = sis_disk_ctrl_create(SIS_DISK_TYPE_SDB_NOTS ,"dbs", "20200101");  
//     sis_disk_class_init(rwf, SIS_DISK_TYPE_SDB_NOTS, "debug", "db");
//     // 测试写入后再写出错问题
//     // rewrite_sdb(rwf);
//     // pack_sdb(rwf);
//     // write_sdb(rwf, keys, sdbs);  // sdb
//     // write_sdb(rwf, NULL, sdbs);  // kdb
//     // write_sdb(rwf, keys, NULL);  // key
//     // write_sdb(rwf, NULL, NULL);  // any
//     read_of_sub(rwf);
//     sis_disk_ctrl_destroy(rwf);
    
//     safe_memory_stop();
//     sis_log_close();
//     return 0;
// }
// #endif


