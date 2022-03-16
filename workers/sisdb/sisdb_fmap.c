

#include <sisdb_fmap.h>
#include <sisdb_io.h>
#include <sisdb.h>

///////////////////////////////////////////////////////////////////////////
//------------------------s_sisdb_fmap_cmd --------------------------------//
///////////////////////////////////////////////////////////////////////////
// 修正无时序的 count 
int _fmap_calc_nots_count(s_sisdb_fmap_cmd *cmd_, int inums_, int *start_, int *count_)
{
	if (cmd_->start < 0)
	{
		*start_ = sis_max(inums_ + cmd_->start, 0);
	}
	if (*start_ >= inums_)
	{
		return 0;
	}
	if ((*start_ + cmd_->count) > inums_)
	{
		*count_ = inums_ - *start_;
	}
	else
	{
		*count_ = cmd_->count;
	}
	return 1;
}

int _fmap_calc_nots_start(int start_, int inums_)
{
	int start = start_;
	if (start_ < 0)
	{
		start = sis_max(inums_ + start_, 0);
	}
	if (start >= inums_)
	{
		return -1;
	}
	return start;
}

///////////////////////////////////////////////////////////////////////////
//------------------------s_sisdb_fmap_cxt --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sisdb_fmap_cxt *sisdb_fmap_cxt_create(const char *wpath_, const char *wname_)
{
	s_sisdb_fmap_cxt *o = SIS_MALLOC(s_sisdb_fmap_cxt, o);
    // 数据集合 
    o->work_keys = sis_map_pointer_create();
	o->work_keys->type->vfree = sisdb_fmap_unit_destroy;
    // 加载本地的所有数据结构
    o->sdbs_defmap = sis_map_list_create(sis_dynamic_db_destroy);
	o->sdbs_writed = 0;
	o->work_path = sis_sdsnew(wpath_);
	o->work_name = sis_sdsnew(wname_);
	o->freader = sis_disk_reader_create(o->work_path, o->work_name, SIS_DISK_TYPE_SDB, NULL);
	return o;
}
void sisdb_fmap_cxt_destroy(s_sisdb_fmap_cxt *cxt_)
{
	if (cxt_->freader)
	{
		sis_disk_reader_destroy(cxt_->freader);
	}
	sis_sdsfree(cxt_->work_path);
	sis_sdsfree(cxt_->work_name);
	sis_map_pointer_destroy(cxt_->work_keys);
	sis_map_list_destroy(cxt_->sdbs_defmap);
	sis_free(cxt_);
}

static void _fmap_cxt_new_of_map(s_sisdb_fmap_cxt *cxt_, s_sis_disk_map *dmap_)
{
	s_sisdb_fmap_unit *unit = NULL;
	char name[255];
	switch (dmap_->ktype)
	{
	case SIS_SDB_STYLE_ONE:
	case SIS_SDB_STYLE_MUL:
		{
			unit = sisdb_fmap_unit_create(dmap_->kname, NULL, dmap_->ktype, NULL);
			sis_sprintf(name, 255, "%s", SIS_OBJ_SDS(dmap_->kname));
		}		
		break;
	default:
		{
			s_sis_dynamic_db *table = sis_map_list_get(cxt_->sdbs_defmap, SIS_OBJ_SDS(dmap_->sname));
			unit = sisdb_fmap_unit_create(dmap_->kname, dmap_->sname, dmap_->ktype, table);
			sis_sprintf(name, 255, "%s.%s", SIS_OBJ_SDS(dmap_->kname), SIS_OBJ_SDS(dmap_->sname));
		}
		break;
	}

	if (unit)
	{
		int count = sis_sort_list_getsize(dmap_->sidxs);
		for (int i = 0; i < count; i++)
		{
			s_sis_disk_map_unit *uidx = sis_sort_list_get(dmap_->sidxs, i);
			s_sisdb_fmap_idx fidx = {0};
			fidx.isign = uidx->idate;
			fidx.moved = uidx->active == 0 ? 1 : 0;
			fidx.start = fidx.moved ? 0 : -1;  // 此时不读盘
			sis_struct_list_push(unit->fidxs, &fidx);
		}
		sis_map_pointer_set(cxt_->work_keys, name, unit);
	}
}
// 初始化 加载磁盘中map的所有数据
int sisdb_fmap_cxt_init(s_sisdb_fmap_cxt *cxt_)
{
	// 每天存盘后重新打开一次 
	// 如果读文件存在就先关闭一次
	if (cxt_->freader)
	{
		sis_disk_reader_destroy(cxt_->freader);
	}
	sis_map_list_clear(cxt_->sdbs_defmap);
	sis_map_pointer_clear(cxt_->work_keys);

	cxt_->freader = sis_disk_reader_create(cxt_->work_path, cxt_->work_name, SIS_DISK_TYPE_SDB, NULL);
	if (sis_disk_reader_open(cxt_->freader))
	{
		// 文件不存在 先写数据到内存 
		sis_disk_reader_destroy(cxt_->freader);
		cxt_->freader = NULL;
		LOG(5)("open %s/%s fail.\n", cxt_->work_path, cxt_->work_name);	
		return -1;
	}
	// 加载数据结构
	{
		int count = sis_map_list_getsize(cxt_->freader->munit->map_sdicts);
		for (int i = 0; i < count; i++)
		{
			s_sis_disk_sdict *sdict = sis_map_list_geti(cxt_->freader->munit->map_sdicts, i);
			sisdb_fmap_cxt_setdb(cxt_, sis_disk_sdict_last(sdict));
		}	
	}
	// 加载数据索引信息 读时直接使用初始化时的索引
	{
		int count = sis_map_list_getsize(cxt_->freader->munit->map_maps);
		for (int i = 0; i < count; i++)
		{
			s_sis_disk_map *dmap = sis_map_list_geti(cxt_->freader->munit->map_maps, i);
			_fmap_cxt_new_of_map(cxt_, dmap);
		}	
	}	
	return 0;
}

s_sisdb_fmap_unit *sisdb_fmap_cxt_new(s_sisdb_fmap_cxt *cxt_, const char *key_, int ktype_)
{
	s_sis_sds kname = NULL; 
	s_sis_sds sname = NULL; 

    sis_str_divide_sds(key_, '.', &kname, &sname);
	
	s_sis_object *kobj = NULL;
	s_sis_object *sobj = NULL;

	if (kname)
	{
		kobj = sis_object_create(SIS_OBJECT_SDS, kname);
	}
	if (sname)
	{
		sobj = sis_object_create(SIS_OBJECT_SDS, sname);
	}

	s_sisdb_fmap_unit *unit = NULL;
	switch (ktype_)
	{
	case SIS_SDB_STYLE_ONE:
	case SIS_SDB_STYLE_MUL:
		{
			unit = sisdb_fmap_unit_create(kobj, NULL, ktype_, NULL);
		}		
		break;
	default:
		{
			s_sis_dynamic_db *table = sis_map_list_get(cxt_->sdbs_defmap, sname);
			// printf("create... %p, %s %d\n", table, sname, sis_map_list_getsize(cxt_->sdbs_defmap));
			unit = sisdb_fmap_unit_create(kobj, sobj, ktype_, table);
		}
		break;
	}
	if (kname)
	{
		sis_object_destroy(kobj);
	}
	if (sname)
	{
		sis_object_destroy(sobj);
	}
	if (unit)
	{
		sis_map_pointer_set(cxt_->work_keys, key_, unit);
	}
	return unit;
}
// 读索引
// 由于按时间来读数据 可以在主索引不存在的情况下 也可以定位数据文件 
// 因此即便没有主索引 也会根据 start 尝试获取数据 并存入索引中 
// 这样处理可以解决只拷贝了目标数据文件 未重建索引的情况 
// ***经过权衡 必须通过map中的主索引来获取数据 否则效率太低
s_sisdb_fmap_unit *sisdb_fmap_cxt_get(s_sisdb_fmap_cxt *cxt_, const char *key_)
{
	return sis_map_pointer_get(cxt_->work_keys, key_);
}
int sisdb_fmap_cxt_setdb(s_sisdb_fmap_cxt *cxt_, s_sis_dynamic_db *sdb_)
{
	s_sis_dynamic_db *agodb = (s_sis_dynamic_db *)sis_map_list_get(cxt_->sdbs_defmap, sdb_->name);
	if (!agodb || !sis_dynamic_dbinfo_same(agodb, sdb_))
	{
		cxt_->sdbs_writed = 1;
		sis_dynamic_db_incr(sdb_);
		sis_map_list_set(cxt_->sdbs_defmap, sdb_->name, sdb_);	
		return 0;
	}
	return 1;
}
s_sis_dynamic_db *sisdb_fmap_cxt_getdb(s_sisdb_fmap_cxt *cxt_, const char *sname_)
{
	return (s_sis_dynamic_db *)sis_map_list_get(cxt_->sdbs_defmap, sname_);	
}
s_sis_dynamic_db *sisdb_fmap_cxt_getdb_of_key(s_sisdb_fmap_cxt *cxt_, const char *key_)
{
    s_sisdb_fmap_unit *unit = sisdb_fmap_cxt_get(cxt_, key_);
    if (!unit)
    {
        unit = sisdb_fmap_cxt_new(cxt_, key_, SIS_SDB_STYLE_SDB);  
    }	
	return unit->sdb;
}
int sisdb_fmap_cxt_get_key_count(s_sisdb_fmap_cxt *cxt_)
{
	return sis_map_pointer_getsize(cxt_->work_keys);
}

s_sis_sds sisdb_fmap_cxt_get_keys(s_sisdb_fmap_cxt *cxt_)
{
	s_sis_string_list *slist = sis_string_list_create();
    {
        s_sis_dict_entry *de;
        s_sis_dict_iter *di = sis_dict_get_iter(cxt_->work_keys);
        while ((de = sis_dict_next(di)) != NULL)
        {
            s_sisdb_fmap_unit *funit = (s_sisdb_fmap_unit *)sis_dict_getval(de);
            // 虽然效率低 但是这里要去除重复的代码
            sis_string_list_push_only(slist, SIS_OBJ_GET_CHAR(funit->kname), SIS_OBJ_GET_SIZE(funit->kname));
        }
        sis_dict_iter_free(di);
    }  
    s_sis_sds msg = sis_string_list_sds(slist);
    sis_string_list_destroy(slist);
    return msg;  
}

s_sis_sds sisdb_fmap_cxt_get_sdbs(s_sisdb_fmap_cxt *cxt_, int isname_, int iszip_)
{
    int count = sis_map_list_getsize(cxt_->sdbs_defmap);
	if (count < 1)
    {
        return NULL;
    }
	s_sis_json_node *jone = sis_json_create_object();
    s_sis_json_node *jdbs = sis_json_create_object();
	for(int i = 0; i < count; i++)
	{
		s_sis_dynamic_db *table = (s_sis_dynamic_db *)sis_map_list_geti(cxt_->sdbs_defmap, i);
		if (isname_)
		{
			sis_json_object_add_node(jdbs, table->name, sis_sdbinfo_to_json(table));
		}
		else
		{
			sis_json_object_add_node(jone, table->name, sis_sdbinfo_to_json(table));
		}
	}
	if (isname_)
	{
		sis_json_object_add_node(jone, cxt_->work_name, jdbs);
	}
    s_sis_sds msg = sis_json_to_sds(jone, iszip_);
    // printf("sdbs = %s\n", msg);
	if (!isname_)
	{
		sis_json_delete_node(jdbs);
	}
    sis_json_delete_node(jone);
    return msg;
}

// 以下函数都需要读取文件信息
// 读取数据
// 先得到该键值的 unit
// 如果没有 就拆分key 然后根据时间去磁盘读取 key + sdb 的数据 如果数据读到
// 就增加一个unit 并把数据写入内存 如果为时序数据就建立索引 再返回严格的对应数据 给函数调用者
// 如果没有数据就返回空
int sisdb_fmap_cxt_read(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_)
{
	s_sisdb_fmap_unit *unit = sisdb_fmap_cxt_get(cxt_, cmd_->key);
	if (!unit)
	{
		return -1;
	}
	else
	{
		printf("===11 ==== %s %lld %lld %d\n", cmd_->key, cmd_->start, cmd_->stop, cmd_->offset);
		sisdb_fmap_cxt_read_data(cxt_, unit, cmd_->key, cmd_->start, cmd_->stop);
		printf("===11 ==== %d\n", ((s_sis_struct_list *)unit->value)->count);
	}
	cmd_->unit = unit;
	cmd_->ktype = unit->ktype;
	int count = 0;
	unit->reads++;
	unit->rmsec = sis_time_get_now_msec();
	switch (unit->ktype)
	{
	case SIS_SDB_STYLE_ONE:
		{
			count = 1;
			s_sis_sds str = (s_sis_sds)unit->value;
			if (cmd_->cb_fmap_read)
			{
				cmd_->cb_fmap_read(cmd_->cb_source, str, sis_sdslen(str), 0);
			}
			else
			{
				cmd_->imem = str;
				cmd_->isize = sis_sdslen(str);
			}
		}
		break;
	case SIS_SDB_STYLE_MUL:
		{
			s_sis_node *snode = (s_sis_node *)unit->value;
			int nums = sis_node_get_size(snode);
			int start = 0;
			if (_fmap_calc_nots_count(cmd_, nums, &start, &count))
			{
				s_sis_node *node = sis_node_get(snode, start);
				if (cmd_->cb_fmap_read)
				{
					for (int n = count; n >= 0; n--)
					{
						s_sis_sds str = (s_sis_sds)node->value;
						cmd_->cb_fmap_read(cmd_->cb_source, str, sis_sdslen(str), n);
						node = sis_node_next(node);
					}				
				}
				else
				{
					cmd_->imem = node;
					cmd_->isize = count;
				}
			}
		}
		break;
	case SIS_SDB_STYLE_NON:
		{
			s_sis_struct_list *slist = (s_sis_struct_list *)unit->value;
			int start = 0;
			// printf("===12 ==== %zu %d %p\n", cmd_->isize, slist->count, cmd_->cb_fmap_read);
			if (_fmap_calc_nots_count(cmd_, slist->count, &start, &count))
			{
				if (cmd_->cb_fmap_read)
				{
					for (int i = start, n = count; n >= 0; i++, n--)
					{
						char *mem = sis_struct_list_get(slist, i);
						cmd_->cb_fmap_read(cmd_->cb_source, mem, slist->len, n);
					}
				}
				else
				{
					cmd_->imem = sis_struct_list_get(slist, start);
					cmd_->isize = count * slist->len;
				}
			}
		}
		break;
	default:
		{
			count = sisdb_fmap_cxt_tsdb_read(cxt_, unit, cmd_);
		}
		break;
	}
	return count;
}

int sisdb_fmap_cxt_update(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_)
{
	s_sisdb_fmap_unit *unit = sisdb_fmap_cxt_get(cxt_, cmd_->key);

    // printf("sisdb_fmap_cxt_update ==== %d %p\n", sisdb_fmap_cxt_get_key_count(cxt_), unit);

	if (!unit)
	{
		unit = sisdb_fmap_cxt_new(cxt_, cmd_->key, cmd_->ktype);
	}
	else
	{
		// 这里计算 start stop 的值
		if (unit->ktype == SIS_SDB_STYLE_SDB)
		{
			int count = cmd_->isize / unit->sdb->size;
			cmd_->start = sis_dynamic_db_get_mindex(unit->sdb, 0, cmd_->imem, cmd_->isize);
			cmd_->stop  = sis_dynamic_db_get_mindex(unit->sdb, count - 1, cmd_->imem, cmd_->isize);
		}
		// printf("unit count == %d | %lld %lld\n", ((s_sis_struct_list *)unit->value)->count, cmd_->start, cmd_->stop);
		sisdb_fmap_cxt_read_data(cxt_, unit, cmd_->key, cmd_->start, cmd_->stop);
	}
	if (!unit)
	{
		LOG(5)("no create fmap : %s %lld %lld %d\n", cmd_->key, cmd_->start, cmd_->stop, cmd_->ktype);
		return 0;
	}
	// printf("unit count == %d\n", ((s_sis_struct_list *)unit->value)->count);
	switch (unit->ktype)
	{
	case SIS_SDB_STYLE_ONE:
		{
			s_sis_sds str = (s_sis_sds)unit->value;
			sis_sdsclear(str);
			unit->value = sis_sdscatlen(str, cmd_->imem, cmd_->isize);
		}
		break;
	case SIS_SDB_STYLE_MUL:
		{
			s_sis_node *node = (s_sis_node *)unit->value;
			if (cmd_->cmpmode == SISDB_FMAP_CMP_NONE)
			{
				s_sis_sds str = sis_sdsnewlen(cmd_->imem, cmd_->isize);
				sis_node_push(node, str);
			}
			else
			{
				int nums = sis_node_get_size(node);			
				int start = _fmap_calc_nots_start(cmd_->start, nums);
				if (start >= 0)
				{
					s_sis_sds ago = sis_node_set(node, start, sis_sdsnewlen(cmd_->imem, cmd_->isize));
					sis_sdsfree(ago);
				}
			}
		}
		break;
	case SIS_SDB_STYLE_NON:
		{
			int count = cmd_->isize / unit->sdb->size;
			//这里应该判断数据完整性
			if (count * unit->sdb->size != cmd_->isize)
			{
				LOG(3)("source format error [%d*%d != %zu]\n", count, unit->sdb->size, cmd_->isize);
				return 0;
			}
			s_sis_struct_list *slist = (s_sis_struct_list *)unit->value;
			// printf("unit == %d %zu %d %d %d\n", count, cmd_->isize, unit->sdb->size, slist->count, unit->sdb->field_solely->count);
			if (slist->count < 1 || unit->sdb->field_solely->count == 0)
			{
				sis_struct_list_pushs(slist, cmd_->imem, count);
			}
			else
			{
				count = sisdb_fmap_cxt_solely_update(cxt_, unit, cmd_);
			}
			// 无索引
			if (count > 0)
			{
				unit->writed = 1;
			}
		}
		break;		
	default:
		{
			int count = cmd_->isize / unit->sdb->size;
			//这里应该判断数据完整性
			if (count * unit->sdb->size != cmd_->isize)
			{
				LOG(3)("source format error [%d*%d != %zu]\n", count, unit->sdb->size, cmd_->isize);
				return 0;
			}
			s_sis_struct_list *slist = (s_sis_struct_list *)unit->value;
			printf("unit == %d %d\n", slist->count, count);
			if (slist->count < 1)
			{
				// ??? 这里没有更新字典索引
				sis_struct_list_pushs(slist, cmd_->imem, count);
			}
			else
			{
				if (unit->sdb->field_solely->count > 0)
				{
					count = sisdb_fmap_cxt_solely_update(cxt_, unit, cmd_);
				}
				else
				{
					count = sisdb_fmap_cxt_mindex_update(cxt_, unit, cmd_);
				}
			}
			if (count > 0)
			{
				sisdb_fmap_unit_reidx(unit);
			}
		}
		break;
	}
	return 1;
}
// 删除数据 如果数据块删空 或者键值下没有数据 需要做标记
// 删除数据必须设置时间 才能定位数据 
int sisdb_fmap_cxt_del(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_)
{
	s_sisdb_fmap_unit *unit = sisdb_fmap_cxt_get(cxt_, cmd_->key);
	if (!unit)
	{
		return 0;
	}
	else
	{
		if (unit->ktype != SIS_SDB_STYLE_ONE)
		{
			sisdb_fmap_cxt_read_data(cxt_, unit, cmd_->key, cmd_->start, cmd_->stop);
		}
	}
	int count = 0;
	switch (unit->ktype)
	{
	case SIS_SDB_STYLE_ONE:
		{
			sis_sdsclear((s_sis_sds)unit->value);
			unit->moved = 1;
			count = 1;
		}
		break;
	case SIS_SDB_STYLE_MUL:
		{
			s_sis_node *node = (s_sis_node *)unit->value;
			int nums = sis_node_get_size(node);
			int start = 0;
			if (_fmap_calc_nots_count(cmd_, nums, &start, &count))
			{
				for (int i = count; i >= 0; i--)
				{
					s_sis_sds ago = sis_node_del(node, start);
					sis_sdsfree(ago);
				}
			}
			if (sis_node_get_size(node) == 0)
			{
				unit->moved = 1;
			}
		}
		break;
	case SIS_SDB_STYLE_NON:
		{
			s_sis_struct_list *slist = (s_sis_struct_list *)unit->value;
			int start = 0;
			if (_fmap_calc_nots_count(cmd_, slist->count, &start, &count))
			{
				if (count > 0)
				{
					sis_struct_list_delete(slist, start, count);
				}
			}
			if (slist->count == 0)
			{
				unit->moved = 1;
			}
		}
		break;		
	default:
		{
			count = sisdb_fmap_cxt_tsdb_del(cxt_, unit, cmd_);
		}
		break;
	}
	return count;
}

// 删除键值 需要做标记
int sisdb_fmap_cxt_remove(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_)
{
	// 对于时序表 如果没有索引 移除键值实际上不能移除未加载到内存的数据块
	// 因此移除键值必须设置时间 在getidx时加载索引到内存 再做move才能保证数据干净的被清理
	s_sisdb_fmap_unit *unit = sisdb_fmap_cxt_get(cxt_, cmd_->key);
	if (!unit)
	{
		return 0;
	}
	switch (unit->ktype)
	{
	case SIS_SDB_STYLE_ONE:
		sis_sdsclear(unit->value);
		break;
	case SIS_SDB_STYLE_MUL:
		{
			s_sis_node *node = (s_sis_node *)unit->value;
			s_sis_node *next = sis_node_get(node, 0);
			while (next)
			{
				sis_sdsfree((s_sis_sds)next->value);
				next = sis_node_next(next);
			}
			sis_node_clear(node);
		}
		break;
	default:
		{
			sisdb_fmap_cxt_tsdb_remove(cxt_, unit);
		}
		break;
	}
	unit->reads = 0;
	unit->rmsec = 0;
	unit->moved = 1;
	return 1;
}

