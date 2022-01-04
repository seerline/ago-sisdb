

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
	if (cmd_->cmpmode == SISDB_FMAP_CMP_WHERE) // 完全匹配 
	{
		if (sisdb_fmap_cmp_where(unit_, cmd_->start, cmd_->offset, &ans) < 0)
		{
			return 0;
		}
	}
	else // 尽量匹配
	{
		if (sisdb_fmap_cmp_range(unit_, cmd_->start, cmd_->stop, cmd_->ifprev, &ans) < 0)
		{
			return 0;
		}
	}
	printf("read tsdb :%lld %lld %d %d | %d %d\n", cmd_->start, cmd_->stop, cmd_->offset, slist->count, ans.ocount, ans.oindex);
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


int _fmap_cxt_getdata_from_disk(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, int index, int start)
{
	s_sis_msec_pair pair; 
	if (unit_->scale == SIS_SDB_SCALE_YEAR)
	{
		pair.start = (msec_t)sis_time_make_time(start * 10000 + 101, 0) * 1000;
		pair.stop = (msec_t)sis_time_make_time(start * 10000 + 1231, 235959) * 1000 + 999;
	}
	else
	{
		pair.start = (msec_t)sis_time_make_time(start, 0) * 1000;
		pair.stop = (msec_t)sis_time_make_time(start, 235959) * 1000 + 999;
	}
	s_sisdb_fmap_idx *pidx = sis_struct_list_get(unit_->fidxs, index);
	pidx->count = 0;
	pidx->start = 0;
	s_sis_object *obj = sis_disk_reader_get_obj(cxt_->freader, SIS_OBJ_GET_CHAR(unit_->kname), SIS_OBJ_GET_CHAR(unit_->sname), &pair);
	if (!obj)
	{  
		// 没数据就增加一个空的索引 下次可以不用再读
		return 0;
	}
	int count = SIS_OBJ_GET_SIZE(obj) / unit_->sdb->size;
	pidx->count = count;
	// 有数据就把读到的数据插入数据区 并更新 start 的值
	if (count > 0)
	{
		s_sis_struct_list *slist = (s_sis_struct_list *)unit_->value;
		msec_t search = pair.start;
		search = sis_time_unit_convert(SIS_DYNAMIC_TYPE_MSEC, unit_->sdb->field_mindex->style, search);
		int oindex = sisdb_fmap_cmp_find_head(unit_, search);
		// 修改之后的数据
		for (int i = index + 1; i < unit_->fidxs->count; i++)
		{
			s_sisdb_fmap_idx *fpidx = sis_struct_list_get(unit_->fidxs, i);
			if (fpidx->count > 0 && fpidx->start >= 0)
			{
				fpidx->start += count;
			}
		}
		if (oindex >= slist->count)
		{
			pidx->start = slist->count;
			sis_struct_list_pushs(slist, SIS_OBJ_GET_CHAR(obj), count);
		}
		else 
		{
			if (oindex < 0)
			{
				oindex = 0;
			}
			pidx->start = oindex;
			sis_struct_list_inserts(slist, oindex, SIS_OBJ_GET_CHAR(obj), count);
		}
	}
	// 重建索引
	sisdb_fmap_unit_reidx(unit_);
	sis_object_destroy(obj);
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
	// printf("==511==, %d %d %d\n", unit_->fidxs->count, openyear, stopyear);
	while (open_year <= stop_year && index < unit_->fidxs->count)
	{
		s_sisdb_fmap_idx *pidx = sis_struct_list_get(unit_->fidxs, index);
		printf("==512==, %d %d\n", open_year, pidx->isign);
		if (open_year == pidx->isign)
		{
			if (pidx->start == -1)
			{
				// 读取数据并插入到合适的位置
				count = _fmap_cxt_getdata_from_disk(cxt_, unit_, index, open_year);
			}
			open_year++;
			index++;
		}
		else 
		{
			if (open_year < pidx->isign)
			{
				open_year++;
			}
			else // if (open_year > pidx->isign)
			{
				index++;
			}
		}
	}
	return count;
}

int _fmap_cxt_getdata_date(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, int opendate, int stopdate)
{
	// 根据cmd中时间 匹配索引加载数据 已经加载过的不再加载
	// 数据加载后更新 
	int count = 0;
	int open_date = opendate; 
	int stop_date = stopdate;
	int index = 0;
	while (open_date <= stop_date && index < unit_->fidxs->count)
	{
		s_sisdb_fmap_idx *pidx = sis_struct_list_get(unit_->fidxs, index);
		if (open_date == pidx->isign)
		{
			if (pidx->start == -1)
			{
				// 读取数据并插入到合适的位置
				count = _fmap_cxt_getdata_from_disk(cxt_, unit_, index, open_date);
			}
			open_date = sis_time_get_offset_day(open_date, 1);
			index++;
		}
		else 
		{
			if (open_date < pidx->isign)
			{
				open_date = sis_time_get_offset_day(open_date, 1);
			}
			else // if (open_date > pidx->isign)
			{
				index++;
			}
		}
	}
	return count;
}
int _fmap_cxt_get_head_sign(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_)
{
	s_sisdb_fmap_idx *pidx = sis_struct_list_first(unit_->fidxs);
	if (pidx)
	{
		return pidx->isign;
	}
	if (unit_->scale == SIS_SDB_SCALE_YEAR)
	{
		return sis_time_get_idate(0) / 10000;
	}
	return sis_time_get_idate(0);
}
int _fmap_cxt_get_tail_sign(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_)
{
	s_sisdb_fmap_idx *pidx = sis_struct_list_last(unit_->fidxs);
	if (pidx)
	{
		return pidx->isign;
	}
	if (unit_->scale == SIS_SDB_SCALE_YEAR)
	{
		return sis_time_get_idate(0) / 10000;
	}
	return sis_time_get_idate(0);
}
// 从磁盘中读实际数据
// 仅仅把相关的键值 相关的日期的数据加载到内存 其他什么事情也不做
// 为了加快速度 如果请求数据在 unit 的时间区间内就直接返回 
int sisdb_fmap_cxt_read_data(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, const char *key_, int64 start_, int64 stop_)
{
	// 这里需要根据索引 去对应文件读取数据
	// 传入的 cmd 中的 start stop 是原始数据 需要根据数据类型转换后 再去匹配索引的信息
	// printf("==21==, %p %p %d %d\n", unit_,  cxt_->freader, unit_->ktype, unit_->scale);
	if (!unit_ || !cxt_->freader)
	{
		return 0;
	}
	int count = 0;
	switch (unit_->ktype)
	{
	case SIS_SDB_STYLE_ONE:
		{
			if (unit_->reads == 0)
			{
				s_sis_object *obj = sis_disk_reader_get_one(cxt_->freader, key_);
				if(obj)
				{
					s_sis_sds str = (s_sis_sds)unit_->value;
					sis_sdsclear(str);
					unit_->value = sis_sdscatlen(str, SIS_OBJ_GET_CHAR(obj), SIS_OBJ_GET_SIZE(obj));
					sis_object_destroy(obj);
					count = 1;
				}
			}
		}
		break;
	case SIS_SDB_STYLE_MUL:
		{
			if (unit_->reads == 0)
			{
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
			}
		}
		break;
	case SIS_SDB_STYLE_NON:
		{
			printf("==22==, %d %d\n", unit_->reads,  unit_->sdb->size);
			if (unit_->reads == 0)
			{
				s_sis_object *obj = sis_disk_reader_get_obj(cxt_->freader, SIS_OBJ_GET_CHAR(unit_->kname), SIS_OBJ_GET_CHAR(unit_->sname), NULL);
				if (obj)
				{
					s_sis_struct_list *slist = (s_sis_struct_list *)unit_->value;
					sis_struct_list_clear(slist);
					count = SIS_OBJ_GET_SIZE(obj) / unit_->sdb->size;
					printf("==23==, %d %d  %zu %d\n", slist->count, count, SIS_OBJ_GET_SIZE(obj),  unit_->sdb->size);
					sis_struct_list_pushs(slist, SIS_OBJ_GET_CHAR(obj), count);
					printf("==24==, %d %d\n", slist->count, count);
					sis_object_destroy(obj);
				}
			}
		}
		break;
	default:
		{
			// if (start_ < unit_->start || stop_ > unit_->stop || unit_->start == 0 || unit_->stop == 0)
			{
				if (unit_->scale == SIS_SDB_SCALE_YEAR)
				{
					// 根据cmd中时间 匹配索引加载数据 已经加载过的不再加载
					// 数据加载后更新 count
					// 传入年份 如果已经读过了就直接返回 如果没有读过就去读
					int start = 0;
					int stop  = 0;
					if (start_ <= 0 || stop_ <= 0)
					{
						int head_date = _fmap_cxt_get_head_sign(cxt_, unit_);
						int tail_date = _fmap_cxt_get_tail_sign(cxt_, unit_);
						start = start_ <= 0 ? start_ < 0 ? tail_date : head_date : start_;
						stop  = stop_ < 0 ? tail_date : stop_;
						if (stop_ == 0)
						{
							stop = start; 
						}
						// printf("==512==, %d %d %d %d\n", start,  stop, count, unit_->fidxs->count);
					}
					else
					{
						start = sis_time_unit_convert(unit_->sdb->field_mindex->style, SIS_DYNAMIC_TYPE_YEAR, start_);
						stop = sis_time_unit_convert(unit_->sdb->field_mindex->style, SIS_DYNAMIC_TYPE_YEAR, stop_);
					}
					count = _fmap_cxt_getdata_year(cxt_, unit_, start, stop);
					printf("==52==, %lld %lld | %d %d %d %d\n", start_, stop_, start,  stop, count, unit_->fidxs->count);
					// for (int i = 0; i < unit_->fidxs->count; i++)
					// {
					// 	s_sisdb_fmap_idx *fpidx = sis_struct_list_get(unit_->fidxs, i);
					// 	printf("==52==, %d\n", fpidx->isign);
					// }
					
				}
				else
				{
					int start = 0;
					int stop  = 0;
					if (start_ <= 0 || stop_ <= 0)
					{
						int head_date = _fmap_cxt_get_head_sign(cxt_, unit_);
						int tail_date = _fmap_cxt_get_tail_sign(cxt_, unit_);
						start = start_ <= 0 ? head_date : start_;
						stop  = stop_ < 0 ? tail_date : stop_;
						if (stop_ == 0)
						{
							stop = start; 
						}
					}
					else
					{
						start = sis_time_unit_convert(unit_->sdb->field_mindex->style, SIS_DYNAMIC_TYPE_DATE, start_);
						stop = sis_time_unit_convert(unit_->sdb->field_mindex->style, SIS_DYNAMIC_TYPE_DATE, stop_);
					}
					// 无论是何类型都转换为日 要加载就加载一天 
					count = _fmap_cxt_getdata_date(cxt_, unit_, start, stop);
					printf("==152==, %d %d %d %d\n", start,  stop, count, unit_->fidxs->count);
				}
			}
		}
		break;
	}
	return count;
}
