

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

int _fmap_calc_nots_start(s_sisdb_fmap_cmd *cmd_, int inums_, int *start_)
{
	if (cmd_->start < 0)
	{
		*start_ = sis_max(inums_ + cmd_->start, 0);
	}
	if (*start_ >= inums_)
	{
		return 0;
	}
	return 1;
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
    o->work_sdbs = sis_map_list_create(sis_dynamic_db_destroy);
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
	sis_map_list_destroy(cxt_->work_sdbs);
	sis_free(cxt_);
}
void sisdb_fmap_cxt_new_of_map(s_sisdb_fmap_cxt *cxt_, s_sis_disk_map *dmap_)
{
	s_sisdb_fmap_unit *unit = NULL;
	char name[255];
	switch (dmap_->ktype)
	{
	case SISDB_FMAP_TYPE_ONE:
	case SISDB_FMAP_TYPE_MUL:
		{
			unit = sisdb_fmap_unit_create(dmap_->kname, NULL, dmap_->ktype, NULL);
			sis_sprintf(name, 255, "%s", SIS_OBJ_SDS(dmap_->kname));
		}		
		break;
	default:
		{
			s_sis_dynamic_db *table = sis_map_list_get(cxt_->work_sdbs, SIS_OBJ_SDS(dmap_->sname));
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
			fidx.start = -1;
			fidx.moved = uidx->active == 0 ? 1 : 0;
			sis_struct_list_push(unit->fidxs, &fidx);
		}
		sis_map_pointer_set(cxt_->work_keys, name, unit);
	}
}
// 初始化 加载磁盘中map的所有数据
int sisdb_fmap_cxt_init(s_sisdb_fmap_cxt *cxt_)
{
	if (!cxt_->freader)
	{
		return -1;
	}
	// 初始化 加载磁盘中数据
	// SISDB_FMAP_TYPE_ONE  SISDB_FMAP_TYPE_MUL 类型的 所有 key 
	// SISDB_FMAP_TYPE_SDB  SISDB_FMAP_TYPE_NON 类型的 所有 sdb 结构 和 key 值
	// 如果map文件中有所有键值的索引信息 就方便了
	// 键值 nyear 有这些信息 查询速度就快了
	sis_map_list_clear(cxt_->work_sdbs);
	sis_map_pointer_clear(cxt_->work_keys);

    if (sis_disk_reader_open(cxt_->freader))
	{
		LOG(5)("open %s/%s fail.\n", cxt_->work_path, cxt_->work_name);
		
		return -2;
	}
	// 记载数据结构
	{
		int count = sis_map_list_getsize(cxt_->freader->munit->map_sdicts);
		for (int i = 0; i < count; i++)
		{
			s_sis_disk_sdict *sdict = sis_map_list_geti(cxt_->freader->munit->map_sdicts, i);
			sisdb_fmap_cxt_setdb(cxt_, sis_disk_sdict_last(sdict));
		}	
	}
	// 加载数据索引信息
	{
		int count = sis_map_list_getsize(cxt_->freader->munit->map_maps);
		for (int i = 0; i < count; i++)
		{
			s_sis_disk_map *dmap = sis_map_list_geti(cxt_->freader->munit->map_maps, i);
			sisdb_fmap_cxt_new_of_map(cxt_, dmap);
		}	
	}	
    sis_disk_reader_close(cxt_->freader);
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
	case SISDB_FMAP_TYPE_ONE:
	case SISDB_FMAP_TYPE_MUL:
		{
			unit = sisdb_fmap_unit_create(kobj, NULL, ktype_, NULL);
		}		
		break;
	default:
		{
			s_sis_dynamic_db *table = sis_map_list_get(cxt_->work_sdbs, sname);
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
s_sisdb_fmap_unit *sisdb_fmap_cxt_get(s_sisdb_fmap_cxt *cxt_, const char *key_)
{
	return sis_map_pointer_get(cxt_->work_keys, key_);
}
int sisdb_fmap_cxt_setdb(s_sisdb_fmap_cxt *cxt_, s_sis_dynamic_db *sdb_)
{
	s_sis_dynamic_db *agodb = (s_sis_dynamic_db *)sis_map_list_get(cxt_->work_sdbs, sdb_->name);
	if (!agodb || !sis_dynamic_dbinfo_same(agodb, sdb_))
	{
		sis_dynamic_db_incr(sdb_);
		sis_map_list_set(cxt_->work_sdbs, sdb_->name, sdb_);	
		return 0;
	}
	return 1;
}
s_sis_dynamic_db *sisdb_fmap_cxt_getdb(s_sisdb_fmap_cxt *cxt_, const char *sname_)
{
	return (s_sis_dynamic_db *)sis_map_list_get(cxt_->work_sdbs, sname_);	
}

// 以下函数都需要读取文件信息
// 读取数据
// 先得到该键值的 unit
// 如果没有 就拆分key 然后根据时间去磁盘读取 key + sdb 的数据 如果数据读到
// 就增加一个unit 并把数据写入内存 如果为时序数据就建立索引 再返回严格的对应数据 给函数调用者
// 如果没有数据就返回空
int sisdb_fmap_cxt_read(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_)
{
	s_sisdb_fmap_unit *unit = sisdb_fmap_cxt_getidx(cxt_, cmd_);
	if (!unit)
	{
		return -1;
	}
	else
	{
		sisdb_fmap_cxt_getdata(cxt_, unit, cmd_);
	}
	cmd_->ktype = unit->ktype;
	int count = 0;
	unit->reads++;
	unit->rmsec = sis_time_get_now_msec();
	switch (unit->ktype)
	{
	case SISDB_FMAP_TYPE_ONE:
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
	case SISDB_FMAP_TYPE_MUL:
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
	case SISDB_FMAP_TYPE_NON:
		{
			s_sis_struct_list *slist = (s_sis_struct_list *)unit->value;
			int start = 0;
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
		count = sisdb_fmap_cxt_tsdb_read(cxt_, unit, cmd_);
		break;
	}
	return count;
}
// 增加数据 直接写 不做安全判断 
int sisdb_fmap_cxt_push(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_)
{
	s_sisdb_fmap_unit *unit = sisdb_fmap_cxt_getidx(cxt_, cmd_);
	if (!unit)
	{
		// 没有数据 要增加新键值
		unit = sisdb_fmap_cxt_new(cxt_, cmd_->key, cmd_->ktype);
	}
	else
	{
		sisdb_fmap_cxt_getdata(cxt_, unit, cmd_);
	}
	int count = 1;
	switch (unit->ktype)
	{
	case SISDB_FMAP_TYPE_ONE:
		{
			s_sis_sds str = (s_sis_sds)unit->value;
			sis_sdsclear(str);
			unit->value = sis_sdscatlen(str, cmd_->imem, cmd_->isize);
		}
		break;
	case SISDB_FMAP_TYPE_MUL:
		{
			s_sis_sds str = sis_sdsnewlen(cmd_->imem, cmd_->isize);
			s_sis_node *node = (s_sis_node *)unit->value;
			sis_node_push(node, str);
		}
		break;
	case SISDB_FMAP_TYPE_NON:
		{
			s_sis_struct_list *slist = (s_sis_struct_list *)unit->value;
			count = cmd_->isize / unit->sdb->size;
			sis_struct_list_pushs(slist, cmd_->imem, count);
		}
		break;
	default:
		{
			// 如果有多条记录 需要根据索引字段的值 同样的值一起插入 毕竟要修改 索引表 所以不能一起插入
			count = cmd_->isize / unit->sdb->size;
			if (count > 1)
			{
				s_sisdb_fmap_cmd cmd;
				memset(&cmd, 0, sizeof(s_sisdb_fmap_cmd));
				int index = 0;
				cmd.start = sis_dynamic_db_get_mindex(unit->sdb, 0, cmd_->imem, cmd_->isize);
				for (int i = 1; i < count; i++)
				{
					msec_t start = sis_dynamic_db_get_mindex(unit->sdb, i, cmd_->imem, cmd_->isize);
					if (start != cmd.start)
					{
						cmd.imem = cmd_->imem + index * unit->sdb->size;
						cmd.isize = (i - index) * unit->sdb->size;
						sisdb_fmap_cxt_tsdb_push(cxt_, unit, &cmd);
						index = i;
						cmd.start = start;
					}
				}
				if (index < count)
				{
					cmd.imem = cmd_->imem + index * unit->sdb->size;
					cmd.isize = (count - index) * unit->sdb->size;
					sisdb_fmap_cxt_tsdb_push(cxt_, unit, &cmd);
				}
			}
			else
			{
				sisdb_fmap_cxt_tsdb_push(cxt_, unit, cmd_);
			}
		}
		break;
	}
	return count;
}

int sisdb_fmap_cxt_update(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_)
{
	s_sisdb_fmap_unit *unit = sisdb_fmap_cxt_getidx(cxt_, cmd_);
	if (!unit)
	{
		if (cmd_->ktype == SISDB_FMAP_TYPE_ONE)
		{
			unit = sisdb_fmap_cxt_new(cxt_, cmd_->key, cmd_->ktype);
		}
		else
		{
			return 0;
		}
	}
	else
	{
		sisdb_fmap_cxt_getdata(cxt_, unit, cmd_);
	}
	switch (unit->ktype)
	{
	case SISDB_FMAP_TYPE_ONE:
		{
			s_sis_sds str = (s_sis_sds)unit->value;
			sis_sdsclear(str);
			unit->value = sis_sdscatlen(str, cmd_->imem, cmd_->isize);
		}
		break;
	case SISDB_FMAP_TYPE_MUL:
		{
			s_sis_node *node = (s_sis_node *)unit->value;
			int nums = sis_node_get_size(node);
			int start = 0;
			if (_fmap_calc_nots_start(cmd_, nums, &start))
			{
				s_sis_sds ago = sis_node_set(node, start, sis_sdsnewlen(cmd_->imem, cmd_->isize));
				sis_sdsfree(ago);
			}
		}
		break;
	case SISDB_FMAP_TYPE_NON:
		{
			s_sis_struct_list *slist = (s_sis_struct_list *)unit->value;
			int start = 0;
			if (_fmap_calc_nots_start(cmd_, slist->count, &start))
			{
				sis_struct_list_update(slist, start, cmd_->imem);
			}
		}
		break;		
	default:
		sisdb_fmap_cxt_tsdb_update(cxt_, unit, cmd_);
		break;
	}
	return 1;
}
// 删除数据 如果数据块删空 或者键值下没有数据 需要做标记
// 删除数据必须设置时间 才能定位数据 
int sisdb_fmap_cxt_del(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_)
{
	s_sisdb_fmap_unit *unit = sisdb_fmap_cxt_getidx(cxt_, cmd_);
	if (!unit)
	{
		return 0;
	}
	else
	{
		sisdb_fmap_cxt_getdata(cxt_, unit, cmd_);
	}
	int count = 0;
	switch (unit->ktype)
	{
	case SISDB_FMAP_TYPE_ONE:
		{
			sis_sdsclear((s_sis_sds)unit->value);
			unit->moved = 1;
			count = 1;
		}
		break;
	case SISDB_FMAP_TYPE_MUL:
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
	case SISDB_FMAP_TYPE_NON:
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
int sisdb_fmap_cxt_move(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_)
{
	// 对于时序表 如果没有索引 移除键值实际上不能移除未加载到内存的数据块
	// 因此移除键值必须设置时间 在getidx时加载索引到内存 再做move才能保证数据干净的被清理
	s_sisdb_fmap_unit *unit = sisdb_fmap_cxt_getidx(cxt_, cmd_);
	if (!unit)
	{
		return 0;
	}
	switch (unit->ktype)
	{
	case SISDB_FMAP_TYPE_ONE:
		sis_sdsclear(unit->value);
		break;
	case SISDB_FMAP_TYPE_MUL:
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
		sisdb_fmap_cxt_tsdb_move(cxt_, unit);
		break;
	}
	unit->reads = 0;
	unit->rmsec = 0;
	unit->moved = 1;
	return 1;
}


// 读索引
// 由于按时间来读数据 可以在主索引不存在的情况下 也可以定位数据文件 
// 因此即便没有主索引 也会根据 start 尝试获取数据 并存入索引中 
// 这样处理可以解决只拷贝了目标数据文件 未重建索引的情况 
// ***经过权衡 必须通过map中的主索引来获取数据 否则效率太低
s_sisdb_fmap_unit *sisdb_fmap_cxt_getidx(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_cmd *cmd_)
{
	s_sisdb_fmap_unit *unit = sis_map_pointer_get(cxt_->work_keys, cmd_->key);
	return unit;
}

// 读实际数据
// 仅仅把相关的键值 相关的日期的数据加载到内存 其他什么事情也不做
int sisdb_fmap_cxt_getdata(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, s_sisdb_fmap_cmd *cmd_)
{
	// 这里需要根据索引 去对应文件读取数据
	// 传入的 cmd 中的 start stop 是原始数据 需要根据数据类型转换后 再去匹配索引的信息

	switch (unit_->ktype)
	{
	case SISDB_FMAP_TYPE_ONE:
		{
			if (!unit_->value)
			{
				
			}
			// s_sis_sds str = (s_sis_sds)unit->value;
			// sis_sdsclear(str);
			// unit->value = sis_sdscatlen(str, cmd_->imem, cmd_->isize);
		}
		break;
	case SISDB_FMAP_TYPE_MUL:
		{
			// s_sis_sds str = sis_sdsnewlen(cmd_->imem, cmd_->isize);
			// s_sis_node *node = (s_sis_node *)unit->value;
			// sis_node_push(node, str);
		}
		break;
	case SISDB_FMAP_TYPE_NON:
		{
			sis_disk_reader_open(cxt_->freader);
			char kname[128], sname[128];
			sis_str_divide(cmd_->key, '.', kname, sname);
			s_sis_object *obj = sis_disk_reader_get_obj(cxt_->freader, kname, sname, NULL);
			if (obj)
			{
				s_sis_struct_list *slist = (s_sis_struct_list *)unit_->value;
				int count = SIS_OBJ_GET_SIZE(obj) / unit_->sdb->size;
				sis_struct_list_pushs(slist, SIS_OBJ_GET_CHAR(obj), count);
				sis_object_destroy(obj);
			}
			sis_disk_reader_close(cxt_->freader);
		}
		break;
	default:
		{
			// 如果有多条记录 需要根据索引字段的值 同样的值一起插入 毕竟要修改 索引表 所以不能一起插入
			sis_disk_reader_open(cxt_->freader);
			if (unit_->scale == SIS_SDB_SCALE_YEAR)
			{
				// 根据cmd中时间 匹配索引加载数据 已经加载过的不再加载
				// 数据加载后更新 count
			}
			else
			{

			}
			sis_disk_reader_close(cxt_->freader);
		// 	count = cmd_->isize / unit->sdb->size;
		// 	if (count > 1)
		// 	{
		// 		s_sisdb_fmap_cmd cmd;
		// 		memset(&cmd, 0, sizeof(s_sisdb_fmap_cmd));
		// 		int index = 0;
		// 		cmd.start = sis_dynamic_db_get_mindex(unit->sdb, 0, cmd_->imem, cmd_->isize);
		// 		for (int i = 1; i < count; i++)
		// 		{
		// 			msec_t start = sis_dynamic_db_get_mindex(unit->sdb, i, cmd_->imem, cmd_->isize);
		// 			if (start != cmd.start)
		// 			{
		// 				cmd.imem = cmd_->imem + index * unit->sdb->size;
		// 				cmd.isize = (i - index) * unit->sdb->size;
		// 				sisdb_fmap_cxt_tsdb_push(cxt_, unit, &cmd);
		// 				index = i;
		// 				cmd.start = start;
		// 			}
		// 		}
		// 		if (index < count)
		// 		{
		// 			cmd.imem = cmd_->imem + index * unit->sdb->size;
		// 			cmd.isize = (count - index) * unit->sdb->size;
		// 			sisdb_fmap_cxt_tsdb_push(cxt_, unit, &cmd);
		// 		}
		// 	}
		// 	else
		// 	{
		// 		sisdb_fmap_cxt_tsdb_push(cxt_, unit, cmd_);
		// 	}
		}
		break;
	}
	return 0;
}