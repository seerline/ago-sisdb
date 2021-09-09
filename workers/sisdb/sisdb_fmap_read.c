

#include <sisdb_fmap.h>
#include <sisdb_io.h>
#include <sisdb.h>

//////////////////////////////////////////////////
//  read
//////////////////////////////////////////////////

int sisdb_fmap_cxt_tsdb_read(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, s_sisdb_fmap_cmd *cmd_)
{
	s_sis_struct_list *slist = (s_sis_struct_list *)unit_->value;
	s_sisdb_fmap_cmp ans = { 0, -1, 0}; 
	// 根据 cmd 信息计算针对列表的开始索引和数量
	if (cmd_->cmpmode == SISDB_FMAP_CMP_SAME) // 完全匹配 
	{
		if (sisdb_fmap_cmp_same(unit_, cmd_->start, &ans) < 0)
		{
			return 0;
		}
	}
	else // 尽量匹配
	{
		if (sisdb_fmap_cmp_range(unit_, cmd_->start, cmd_->stop, &ans) < 0)
		{
			return 0;
		}
	}
	if (ans.ocount > 0 && ans.oindex >= 0)
	{
		if (cmd_->cb_fmap_read)
		{
			for (int i = ans.oindex, n = cmd_->count; n >= 0 && i < ans.oindex + ans.ocount; i++, n--)
			{
				char *mem = sis_struct_list_get(slist, i);
				cmd_->cb_fmap_read(cmd_->cb_source, mem, slist->len, n);
			}
		}
		else
		{
			// 这里直接返回数据指针 外部获取数据需要拷贝一份
			cmd_->imem = sis_struct_list_get(slist, ans.oindex);
			cmd_->isize = ans.ocount * slist->len;
		}
	}
	return ans.ocount;
}


int _fmap_cxt_getdata_read(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, int start)
{
	s_sis_msec_pair pair; 
	if (unit_->scale == SIS_SDB_SCALE_YEAR)
	{
		pair.start = (msec_t)sis_time_make_time(start / 10000 * 10000 + 101, 0) * 1000;
		pair.stop = (msec_t)sis_time_make_time(start / 10000 * 10000 + 1231, 235959) * 1000 + 999;
	}
	else
	{
		pair.start = (msec_t)sis_time_make_time(start, 0) * 1000;
		pair.stop = (msec_t)sis_time_make_time(start, 235959) * 1000 + 999;
	}
	s_sis_object *obj = sis_disk_reader_get_obj(cxt_->freader, SIS_OBJ_GET_CHAR(unit_->kname), SIS_OBJ_GET_CHAR(unit_->sname), &pair);
	if (!obj)
	{  
		// 没数据就增加一个空的索引 下次可以不用再读
		return 0;
	}
	// 有数据就把读到的数据插入数据区 并增加一个新的有数据的索引
	s_sis_struct_list *slist = (s_sis_struct_list *)unit_->value;
	// s_sisdb_fmap_cmp ans = { 0, -1, 0};


	// s_sis_struct_list *slist = (s_sis_struct_list *)unit_->value;
	// count = SIS_OBJ_GET_SIZE(obj) / unit_->sdb->size;
	// sis_struct_list_pushs(slist, SIS_OBJ_GET_CHAR(obj), count);
	// sis_object_destroy(obj);

	// 找到和start匹配的数据索引
	// push 一定可以成功
	// sisdb_fmap_cmp_find(unit_, start, &ans);
	
	// // 先整理镜像索引
	// int ostart = ans.ostart;
	// if (ostart == 0)
	// {
	// 	ostart = sis_time_unit_convert(unit_->sdb->field_mindex->style, SIS_DYNAMIC_TYPE_DATE, start);
	// }

	// sisdb_fmap_unit_set_idx(unit_, ostart);

	// int count = isize / unit_->sdb->size;
	// if (ans.oindex < 0)
	// {
	// 	sis_struct_list_inserts(slist, 0, imem, count);
	// }
	// else if (ans.oindex >= slist->count)
	// {
	// 	sis_struct_list_pushs(slist, imem, count);
	// }
	// else 
	// {
	// 	sis_struct_list_inserts(slist, ans.oindex + ans.ocount, imem, count);
	// }
	// // 重建索引
	// sisdb_fmap_unit_reidx(unit_);
	return 0;
}
// 返回加载的数据数量
int _fmap_cxt_getdata_year(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, int openyear, int stopyear)
{
	// 根据cmd中时间 匹配索引加载数据 已经加载过的不再加载
	// 数据加载后更新 
	int count = 0;
	int open_year = openyear; 
	int stop_year = stopyear;
	int index = 0;
	while (open_year <= stop_year)
	{
		// s_sisdb_fmap_idx *pidx = sis_struct_list_get(unit_->fidxs, index);
		// if (open_year == pidx->start)
		// {
		// 	if (pidx->start == -1)
		// 	{
		// 		// 读取数据并插入到合适的位置
		// 		// _fmap_cxt_getdata_set(unit_, pidx, )
		// 		pidx->start = 1;
		// 	}
		// 	else
		// 	{
		// 		open_year++;
		// 		index++;
		// 	}
		// }
		// else 
		// {
		// 	if (open_year < pidx->start)
		// 	{
		// 		sis_struct_list_insert(unit_->fidxs, index, &fidx);
		// 		open_year++;
		// 		index++;
		// 	}
		// 	else // if (open_year > pidx->start)
		// 	{
		// 		if (index < unit_->fidxs->count)
		// 		{
		// 			index++;
		// 		}
		// 		else
		// 		{
		// 			// 新读数据
		// 			sis_struct_list_push(unit_->fidxs, &fidx);
		// 			open_year++;
		// 			index++;
		// 		}
		// 	}
		// }
		// 	// 增
	}
	

	return count;
}

int _fmap_cxt_getdata_date(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, int start_, int stop_)
{
	// sis_time_unit_convert(unit_->sdb->field_mindex->style, SIS_DYNAMIC_TYPE_DATE, start);
	return 0;
}
// 从磁盘中读实际数据
// 仅仅把相关的键值 相关的日期的数据加载到内存 其他什么事情也不做
// 为了加快速度 如果请求数据在 unit 的时间区间内就直接返回 
int sisdb_fmap_cxt_read_data(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, const char *key_, int start_, int stop_)
{
	// 这里需要根据索引 去对应文件读取数据
	// 传入的 cmd 中的 start stop 是原始数据 需要根据数据类型转换后 再去匹配索引的信息
	if (!unit_)
	{
		return 0;
	}
	int count = 0;
	switch (unit_->ktype)
	{
	case SISDB_FMAP_TYPE_ONE:
		{
			if (!unit_->ready)
			{
				sis_disk_reader_open(cxt_->freader);
				s_sis_object *obj = sis_disk_reader_get_one(cxt_->freader, key_);
				if(obj)
				{
					s_sis_sds *str = (s_sis_sds)unit_->value;
					sis_sdsclear(str);
					unit_->value = sis_sdscatlen(str, SIS_OBJ_GET_CHAR(obj), SIS_OBJ_GET_SIZE(obj));
					sis_object_destroy(obj);
					count = 1;
				}
				sis_disk_reader_close(cxt_->freader);
				unit_->ready = 1;
			}
		}
		break;
	case SISDB_FMAP_TYPE_MUL:
		{
			if (!unit_->ready)
			{
				sis_disk_reader_open(cxt_->freader);
				s_sis_node *obj = sis_disk_reader_get_mul(cxt_->freader, key_);
				if(obj)
				{
					if (unit_->value) 
					{
						sis_node_destroy(unit_->value);
					}
					unit_->value = obj;
					count = sis_node_get_size(obj);
				}
				sis_disk_reader_close(cxt_->freader);
				unit_->ready = 1;
			}
		}
		break;
	case SISDB_FMAP_TYPE_NON:
		{
			if (!unit_->ready)
			{
				sis_disk_reader_open(cxt_->freader);
				s_sis_object *obj = sis_disk_reader_get_obj(cxt_->freader, SIS_OBJ_GET_CHAR(unit_->kname), SIS_OBJ_GET_CHAR(unit_->sname), NULL);
				if (obj)
				{
					s_sis_struct_list *slist = (s_sis_struct_list *)unit_->value;
					sis_struct_list_clear(slist);
					count = SIS_OBJ_GET_SIZE(obj) / unit_->sdb->size;
					sis_struct_list_pushs(slist, SIS_OBJ_GET_CHAR(obj), count);
					sis_object_destroy(obj);
				}
				sis_disk_reader_close(cxt_->freader);
				unit_->ready = 1;
			}
		}
		break;
	default:
		{
			if (start_ < unit_->start || stop_ > unit_->stop || unit_->start == 0 || unit_->stop == 0)
			{
				sis_disk_reader_open(cxt_->freader);
				if (unit_->scale == SIS_SDB_SCALE_YEAR)
				{
					// 根据cmd中时间 匹配索引加载数据 已经加载过的不再加载
					// 数据加载后更新 count
					// 传入年份 如果已经读过了就直接返回 如果没有读过就去读
					count = _fmap_cxt_getdata_year(cxt_, unit_, start_ / 10000, stop_ / 10000);
				}
				else
				{
					int start = sis_time_unit_convert(unit_->sdb->field_mindex->style, SIS_DYNAMIC_TYPE_DATE, start_);
					int stop = sis_time_unit_convert(unit_->sdb->field_mindex->style, SIS_DYNAMIC_TYPE_DATE, stop_);
					// 无论是何类型都转换为日 要加载就加载一天 
					count = _fmap_cxt_getdata_date(cxt_, unit_, start, stop);
				}
				sis_disk_reader_close(cxt_->freader);
			}
		}
		break;
	}
	return count;
}

int sisdb_fmap_cxt_init_data(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, const char *key_, int start_, int stop_)
{
}
