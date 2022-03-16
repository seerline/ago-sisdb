//******************************************************
// Copyright (C) 2018, coollyer <seerlinecoin@gmail.com>
//*******************************************************

#include "sisdb_io.h"
#include "sis_net.msg.h"

int sisdb_io_create(s_sisdb_cxt *sisdb_, const char *sname_, s_sis_json_node *node_)
{
    s_sis_dynamic_db *newdb = sis_dynamic_db_create(node_);
    if (newdb)
    {
        sis_dynamic_db_setname(newdb, sname_);

        int o = sisdb_fmap_cxt_setdb(sisdb_->work_fmap_cxt, newdb);

        sis_dynamic_db_destroy(newdb);
        return o;  
    }
    return -1;
}

static int _init_cmd_from_get(s_sisdb_fmap_cmd *cmd_,  const char *key_, s_sis_json_node *node_)
{
    cmd_->cmpmode = SISDB_FMAP_CMP_RANGE;
    cmd_->start = -1;
    cmd_->stop = 0;
    cmd_->offset = 0;
    cmd_->count = 1;
    cmd_->key = key_;
    if (!node_)
    {
        return -1;
    }
    s_sis_json_node *range = sis_json_cmp_child_node(node_, "range");
    if (range)
    {
        if (sis_json_cmp_child_node(range, "offset"))
        {
            cmd_->offset = sis_json_get_int(range, "offset", 0);;
        }
        if (sis_json_cmp_child_node(range, "start"))
        {
            cmd_->start = sis_json_get_int(range, "start", 0);;
        }
        else
        {
            return -2;
        }
        cmd_->stop = sis_json_get_int(range, "stop", 0);;
        cmd_->ifprev = sis_json_get_int(range, "ifprev", 0);
    }
    else 
    {
        s_sis_json_node *where = sis_json_cmp_child_node(node_, "where");
        if (where)
        {
            cmd_->cmpmode = SISDB_FMAP_CMP_WHERE;
            cmd_->stop = 0;
            // where 不支持偏移 通常用于定位查找
            if (sis_json_cmp_child_node(where, "offset"))
            {
                cmd_->offset = sis_json_get_int(where, "offset", 0);;
            }
            if (sis_json_cmp_child_node(where, "start"))
            {
                cmd_->start = sis_json_get_int(where, "start", 0);;
            }
            else
            {
                return -3;
            }
        }
    }
    printf("start = %lld %lld\n", cmd_->start, cmd_->stop);
    return 0;
}
// 返回NULL表示全部字段
static s_sis_string_list *_read_fields( s_sis_json_node *node_)
{
	if (!node_ || !sis_json_cmp_child_node(node_, "fields"))
	{
		return NULL;
	}
    const char *fields = sis_json_get_str(node_, "fields");
    if (!sis_strncmp(fields, "*", 1))
    {
        return NULL;
    }
    s_sis_string_list *flist = sis_string_list_create();
    sis_string_list_load(flist, fields, sis_strlen(fields), ",");
    return flist;
}

// 为保证最快速度，尽量不加参数
// 默认返回最后不超过 64K 的数据，以json格式
s_sis_sds sisdb_io_get_sds(s_sisdb_cxt *sisdb_, const char *key_, s_sis_json_node *node_)
{
    s_sisdb_fmap_cmd cmd = {0};
    _init_cmd_from_get(&cmd, key_, node_);  // 获取参数

    if (sisdb_fmap_cxt_read(sisdb_->work_fmap_cxt, &cmd) < 0)
    {
        return NULL;
    };
    if (cmd.ktype == SIS_SDB_STYLE_MUL)
    {
        // 暂时不处理
        return NULL;
    }
    if (cmd.ktype == SIS_SDB_STYLE_ONE)
    {
        return sis_sdsnewlen(cmd.imem, cmd.isize); 
    }
    s_sis_string_list *fields = _read_fields(node_);

    if (!fields)
    {
        return sis_sdsnewlen(cmd.imem, cmd.isize); 
    }
    s_sis_sds o = sisdb_sdb_struct_to_sds(cmd.unit->sdb, cmd.imem, cmd.isize, fields);
    sis_string_list_destroy(fields);
    return o;
}

s_sis_sds sisdb_io_get_chars_sds(s_sisdb_cxt *sisdb_, const char *key_, int rfmt_, s_sis_json_node *node_)
{
    s_sisdb_fmap_cmd cmd = {0};
    _init_cmd_from_get(&cmd, key_, node_);  // 获取参数

    if (sisdb_fmap_cxt_read(sisdb_->work_fmap_cxt, &cmd) < 0)
    {
        return NULL;
    };
    if (cmd.ktype == SIS_SDB_STYLE_MUL)
    {
        // 暂时不处理
        return NULL;
    }
    if (cmd.ktype == SIS_SDB_STYLE_ONE)
    {
        return sis_sdsnewlen(cmd.imem, cmd.isize); 
    }
    s_sis_sds o = NULL;
    s_sis_string_list *fields = _read_fields(node_);
    printf("===10 ==== %d\n", sisdb_fmap_unit_count(cmd.unit));
    if (!fields)
    {
		switch (rfmt_)
		{
		case SISDB_FORMAT_JSON:
			o = sis_sdb_to_array_sds(cmd.unit->sdb, key_, cmd.imem, cmd.isize); 
			break;
		case SISDB_FORMAT_ARRAY:
			o = sis_sdb_to_array_sds(cmd.unit->sdb, key_, cmd.imem, cmd.isize); 
			break;
		case SISDB_FORMAT_CSV:
			o = sis_sdb_to_csv_sds(cmd.unit->sdb, cmd.imem, cmd.isize); 
			break;
		}		
    }
    else
    {
		switch (rfmt_)
		{
		case SISDB_FORMAT_JSON:
			o = sis_sdb_fields_to_json_sds(cmd.unit->sdb, cmd.imem, cmd.isize, key_, fields, true, true);
			break;
		case SISDB_FORMAT_ARRAY:
			o = sis_sdb_fields_to_array_sds(cmd.unit->sdb, cmd.imem, cmd.isize, fields, true);
			break;
		case SISDB_FORMAT_CSV:
			o = sis_sdb_fields_to_csv_sds(cmd.unit->sdb, cmd.imem, cmd.isize, fields, true);
			break;
		}
		sis_string_list_destroy(fields);     
    }
    return o;    
}

static int _init_cmd_from_set(s_sisdb_fmap_cmd *cmd_, const char *key_, s_sis_sds ask_)
{
    cmd_->cmpmode = SISDB_FMAP_CMP_WHERE;
    cmd_->key = key_;
    cmd_->isize = sis_sdslen(ask_);
    cmd_->imem = ask_;
    // cmd_->start = sis_dynamic_db_get_mindex(unit->sdb, 0, cmd_->imem, cmd_->isize);
    // cmd_->start = 0;
    cmd_->stop = 0;
    // cmd_->offset = 0;
    // cmd_->count = 1;
    // if (!node_)
    // {
    //     return -1;
    // }

    return 0;
}
// 返回修改成功的数量
int sisdb_io_update(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds imem_)
{
    s_sisdb_fmap_cmd cmd = {0};
    _init_cmd_from_set(&cmd, key_, imem_);  // 获取参数

    int  o = sisdb_fmap_cxt_update(sisdb_->work_fmap_cxt, &cmd);

    return o;
}
int sisdb_io_set_chars(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds vmem_)
{
    if(!vmem_ || sis_sdslen(vmem_) < 1 || (vmem_[0] != '{' && vmem_[0] != '['))
    {
        return -1;
    }
    s_sis_dynamic_db *curdb = sisdb_fmap_cxt_getdb_of_key(sisdb_->work_fmap_cxt, key_);
    // 到这里已经有了collect
    s_sis_sds bytes = NULL;
    if (vmem_[0] == '{')
    {
        bytes = sis_json_to_struct_sds(curdb, vmem_, NULL);
    }
    if (vmem_[0] == '[')
    {
        bytes = sis_array_to_struct_sds(curdb, vmem_);
    }
    if (!bytes)
    {
        return -3;
    }
    // printf("%p\n",bytes);
    // sis_out_binary("bytes:",  bytes, sis_sdslen(bytes));
    int o = sisdb_io_update(sisdb_, key_, bytes);

    sis_sdsfree(bytes);

    return o;
}

int sisdb_io_set_one_chars(s_sisdb_cxt *sisdb_, const char *key_, s_sis_sds argv_)
{
    // 单键值 暂时不支持
    return 0;
}


// 必须带参数 否则不执行删除操作
int sisdb_io_del(s_sisdb_cxt *sisdb_, const char *key_, s_sis_json_node *node_)
{
    s_sisdb_fmap_cmd cmd = {0};
    
    if (!node_)
    {
        return 0;
    }
    cmd.cmpmode = SISDB_FMAP_CMP_WHERE;
    s_sis_json_node *range = sis_json_cmp_child_node(node_, "range");
    if (range)
    {
        cmd.cmpmode = SISDB_FMAP_CMP_RANGE;
        if (sis_json_cmp_child_node(range, "start"))
        {
            cmd.start = sis_json_get_int(range, "start", 0);;
        }
        else
        {
            return 0;
        }
        cmd.stop = sis_json_get_int(range, "stop", cmd.start);
    }
    else 
    {
        s_sis_json_node *same = sis_json_cmp_child_node(node_, "where");
        if (same)
        {
            cmd.stop = 0; // 暂时只支持定位
            if (sis_json_cmp_child_node(same, "start"))
            {
                cmd.start = sis_json_get_int(same, "start", 0);;
            }
            else
            {
                return 0;
            }
        }
    }    
    cmd.key = key_;

    return sisdb_fmap_cxt_del(sisdb_->work_fmap_cxt, &cmd);

}
int sisdb_io_del_one(s_sisdb_cxt *sisdb_, const char *key_, s_sis_json_node *node_)
{
    // 单键值 暂时不支持
    return 0;
}

int sisdb_io_drop(s_sisdb_cxt *sisdb_, const char *sname_)
{
    s_sis_dynamic_db *curdb = sisdb_fmap_cxt_getdb(sisdb_->work_fmap_cxt, sname_);
    if (!curdb)
    {
        return -2;
    }
    // 暂时不支持
    return -1;
}

// 多单键取值只处理字符串 非字符串不处理
s_sis_sds sisdb_io_gets_one_sds(s_sisdb_cxt *sisdb_, const char *keys_)
{
    // s_sis_json_node *jone = sis_json_create_object();
    // if (!sis_strcasecmp(keys_, "*"))
    // {
    //     s_sis_dict_entry *de;
    //     s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->work_fmap_cxt->work_keys);
    //     while ((de = sis_dict_next(di)) != NULL)
    //     {
    //         s_sisdb_fmap_unit *unit = (s_sisdb_fmap_unit *)sis_dict_getval(de);
    //         if (unit->ktype == SIS_SDB_STYLE_ONE)
    //         {
    //             sis_json_object_add_string(jone, sis_dict_getkey(de), SIS_OBJ_GET_CHAR(collect->obj), SIS_OBJ_GET_SIZE(collect->obj));
    //         }
    //     }
    //     sis_dict_iter_free(di);
    // }
    // else
    // {
    //     s_sis_string_list *klist = sis_string_list_create();
    //     sis_string_list_load(klist, keys_, sis_strlen(keys_), ",");

    //     int count = sis_string_list_getsize(klist);
    //     for (int i = 0; i < count; i++)
    //     {
    //         const char *key = sis_string_list_get(klist, i);
    //         s_sisdb_collect *collect = sis_map_pointer_get(sisdb_->work_keys, key);
    //         if (collect && collect->style == SISDB_COLLECT_TYPE_CHARS)
    //         {
    //             sis_json_object_add_string(jone, key, SIS_OBJ_GET_CHAR(collect->obj), SIS_OBJ_GET_SIZE(collect->obj));
    //         }
    //     }
    //     sis_string_list_destroy(klist);
    // }
    // s_sis_sds o = sis_json_to_sds(jone, false);
    // sis_json_delete_node(jone);
    // return o;
    return NULL;
}


// int _sisdb_get_filter(s_sisdb_cxt *sisdb_, s_sis_string_list *list_, const char *keys_, const char *sdbs_)
// {
//     sis_string_list_clear(list_);
//     if (!sis_strcasecmp(sdbs_, "*") && !sis_strcasecmp(keys_, "*"))
//     {
//         s_sis_dict_entry *de;
//         s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->work_keys);
//         while ((de = sis_dict_next(di)) != NULL)
//         {
//             sis_string_list_push(list_, sis_dict_getkey(de), sis_strlen(sis_dict_getkey(de)));
//         }
//         sis_dict_iter_free(di);
//         return sis_string_list_getsize(list_);
//     }
    
//     if (!sis_strcasecmp(keys_, "*"))
//     {
//         s_sis_dict_entry *de;
//         s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->work_keys);
//         while ((de = sis_dict_next(di)) != NULL)
//         {
//             s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
//             if (collect->style == SISDB_COLLECT_TYPE_TABLE && 
//                 sis_str_subcmp_strict(collect->sdb->name,  sdbs_, ',') >= 0)
//             {
//                 sis_string_list_push(list_, sis_dict_getkey(de), sis_strlen(sis_dict_getkey(de)));
//             }
//         }
//         sis_dict_iter_free(di);
//     }
//     else
//     {
//         s_sis_string_list *slists = sis_string_list_create();
//         if (!sis_strcasecmp(sdbs_, "*"))
//         {
//             int count = sis_map_list_getsize(sisdb_->work_sdbs);
//             for (int i = 0; i < count; i++)
//             {
//                 s_sis_dynamic_db *table = sis_map_list_geti(sisdb_->work_sdbs, i);
//                 sis_string_list_push(slists, table->name, sis_sdslen(table->name));
//             }  
//         }
//         else
//         {
//             sis_string_list_load(slists, sdbs_, sis_strlen(sdbs_), ",");  
//         }

//         s_sis_string_list *klists = sis_string_list_create();
//         sis_string_list_load(klists, keys_, sis_strlen(keys_), ",");  
//         int kcount = sis_string_list_getsize(klists);
//         int scount = sis_string_list_getsize(slists);
//         char key[128];
//         for (int k = 0; k < kcount; k++)
//         {
//             for (int i = 0; i < scount; i++)
//             {
//                 sis_sprintf(key, 128, "%s.%s", sis_string_list_get(klists, k), sis_string_list_get(slists, i));
//                 sis_string_list_push(list_, key, sis_strlen(key));
//             }         
//         }
//         sis_string_list_destroy(klists);
//         sis_string_list_destroy(slists);
//     }
//     return sis_string_list_getsize(list_);
// }

// 多键取值只返回字符串 并且只返回最后一条记录
s_sis_sds sisdb_io_gets_sds(s_sisdb_cxt *sisdb_, const char *key_, s_sis_json_node *node_)
{
    // s_sis_string_list *collects = sis_string_list_create();

    // int count = _sisdb_get_filter(sisdb_, collects, keys_, sdbs_);

    // s_sis_json_handle *handle = NULL;
    // if (argv_)
    // {
    //     handle = sis_json_load(argv_, sis_sdslen(argv_));
    // } 
    
    // s_sis_json_node *jone = sis_json_create_object();
    // for (int i = 0; i < count; i++)
    // {
    //     const char *key = sis_string_list_get(collects, i);
    //     s_sisdb_collect *collect = sis_map_pointer_get(sisdb_->work_keys, key);
    //     if (collect)
    //     {
    //         s_sis_json_node *jval = sisdb_collects_get_last_node(collect, handle ? handle->node : NULL);
    //         if (jval)
    //         {
    //             sis_json_object_add_node(jone, key, jval);
    //         }
    //     }
    // }
    // sis_string_list_destroy(collects);
    // if (handle)
    // {
    //     sis_json_close(handle);
    // }

    // s_sis_sds o = sis_json_to_sds(jone, true);
    // sis_json_delete_node(jone);
    // return o;
    return NULL;
}

// 只获取单键数值
s_sis_sds sisdb_io_keys_one_sds(s_sisdb_cxt *sisdb_, const char *keys_)
{
    s_sis_sds o = NULL;
    // s_sis_dict_entry *de;
    // s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->work_keys);
    // while ((de = sis_dict_next(di)) != NULL)
    // {
    //     s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
    //     if (collect->style != SISDB_COLLECT_TYPE_TABLE)
    //     {
    //         if (!o)
    //         {
    //             o = sis_sdsnew(sis_dict_getkey(de));
    //         }
    //         else
    //         {
    //             o = sis_sdscatfmt(o, ",%s", sis_dict_getkey(de));
    //         }
    //     }
    // }
    // sis_dict_iter_free(di);
    return o;
}
s_sis_sds sisdb_io_keys_sds(s_sisdb_cxt *sisdb_, const char *keys_, s_sis_json_node *node_)
{
    s_sis_sds o = NULL;
    // s_sis_map_int *keys = sis_map_int_create();
    // {
    //     char kname[128], sname[128]; 
    //     s_sis_dict_entry *de;
    //     s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->work_keys);
    //     while ((de = sis_dict_next(di)) != NULL)
    //     {
    //         s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
    //         // printf("%s\n", (char *)sis_dict_getkey(de));
    //         if (collect->style == SISDB_COLLECT_TYPE_TABLE)
    //         {
    //             int cmds = sis_str_divide(sis_dict_getkey(de), '.', kname, sname);
    //             if (cmds == 2)
    //             {
    //                 if (sis_map_int_get(keys, kname) == -1)
    //                 {
    //                     sis_map_int_set(keys, kname, 1);
    //                 }
    //             }
    //         }
    //     }
    //     sis_dict_iter_free(di);
    // }
    // {
    //     s_sis_dict_entry *de;
    //     s_sis_dict_iter *di = sis_dict_get_iter(keys);
    //     while ((de = sis_dict_next(di)) != NULL)
    //     {
    //         if (!o)
    //         {
    //             o = sis_sdsnew(sis_dict_getkey(de));
    //         }
    //         else
    //         {
    //             o = sis_sdscatfmt(o, ",%s", sis_dict_getkey(de));
    //         }
    //     }
    //     sis_dict_iter_free(di);
    // }
    // sis_map_int_destroy(keys);

    return o;
}


int sisdb_io_dels_one(s_sisdb_cxt *sisdb_, const char *key_)
{
    int o = 0;
    // if (!sis_strcasecmp(keys_, "*"))
    // {
    //     // sis_map_pointer_clear(sisdb_->work_keys);
    //     s_sis_dict_entry *de;
    //     s_sis_dict_iter *di = sis_dict_get_iter(sisdb_->work_keys);
    //     while ((de = sis_dict_next(di)) != NULL)
    //     {
    //         s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
    //         if (collect->style == SISDB_COLLECT_TYPE_CHARS || collect->style == SISDB_COLLECT_TYPE_BYTES)
    //         {
    //             sis_map_pointer_del(sisdb_->work_keys, sis_dict_getkey(de));
    //             o++;
    //         }
    //     }
    //     sis_dict_iter_free(di);
    // }
    // else
    // {
    //     s_sis_string_list *klist = sis_string_list_create();
    //     sis_string_list_load(klist, keys_, sis_strlen(keys_), ",");

    //     int count = sis_string_list_getsize(klist);
    //     for (int i = 0; i < count; i++)
    //     {
    //         sis_map_pointer_del(sisdb_->work_keys, sis_string_list_get(klist, i));
    //         o++;
    //     }
    //     sis_string_list_destroy(klist);
    // }
    return o;
}

int sisdb_io_dels(s_sisdb_cxt *sisdb_, const char *key_, s_sis_json_node *node_)
{
    // s_sis_string_list *collects = sis_string_list_create();
    // int count = _sisdb_get_filter(sisdb_, collects, keys_, sdbs_);
    // for (int i = 0; i < count; i++)
    // {
    //     sisdb_del(sisdb_, sis_string_list_get(collects, i), argv_);
    // }
    // sis_string_list_destroy(collects);
    return 0;    
}

