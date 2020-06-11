

#include <sisdb_collect.h>
#include <sisdb_io.h>
#include <sisdb.h>

// 必须找到一个相等值，否则返回-1
int sisdb_collect_search(s_sisdb_collect *collect_, uint64 finder_)
{
	int index = sisdb_stepindex_goto(collect_->stepinfo, finder_);
	if (index < 0)
	{
		return -1;
	}
	int i = index;
	int dir = 0;
	while (i >= 0 && i < sisdb_collect_recs(collect_))
	{
		uint64 ts = sisdb_collect_get_mindex(collect_, i);
		if (finder_ > ts)
		{
			if (dir == -1)
			{
				return -1;
			}
			dir = 1;
			i += dir;
			continue;
		}
		else if (finder_ < ts)
		{
			if (dir == 1)
			{
				return -1;
			}
			dir = -1;
			i += dir;
			continue;
		}
		else
		{
			return i;
		}
	}
	return -1;
}

// 从更早的时间开始检索
int sisdb_collect_search_left(s_sisdb_collect *collect_, uint64 finder_, int *mode_)
{
	*mode_ = SIS_SEARCH_NONE;
	int index = sisdb_stepindex_goto(collect_->stepinfo, finder_);
	if (index < 0)
	{
		return -1;
	}
	int i = index;
	int dir = 0;
	int size = sisdb_collect_recs(collect_);
	while (i >= 0 && i < size)
	{
		uint64 ts = sisdb_collect_get_mindex(collect_, i);
		// printf("  %lld --- %lld  mode= %d\n", finder_, ts, *mode_);
		if (finder_ > ts)
		{
			if (dir == -1)
			{
				*mode_ = SIS_SEARCH_LEFT;
				return i;
			}
			dir = 1;
			i += dir;
			continue;
		}
		else if (finder_ < ts)
		{
			if (dir == 1)
			{
				*mode_ = SIS_SEARCH_LEFT;
				return i - 1;
			}
			dir = -1;
			i += dir;
			continue;
		}
		else
		{
			*mode_ = SIS_SEARCH_OK;
			int k = i + 1;
			for (; k < size; k++)
			{
				ts = sisdb_collect_get_mindex(collect_, k);
				if (finder_ != ts)
				{
					break;
				}
				else
				{
					i = k;
				}
			}
			return i;
		}
	}
	if (dir == 1)
	{
		*mode_ = SIS_SEARCH_LEFT;
		return collect_->value->count - 1;
	}
	else
	{
		return -1;
	}
}
// 找到finder的位置，并返回index和相对位置（左还是右）没有记录设置 SIS_SEARCH_NONE
int sisdb_collect_search_right(s_sisdb_collect *collect_, uint64 finder_, int *mode_)
{
	*mode_ = SIS_SEARCH_NONE;
	int index = sisdb_stepindex_goto(collect_->stepinfo, finder_);
	if (index < 0)
	{
		return -1;
	}
	int i = index;
	int dir = 0;
	while (i >= 0 && i < sisdb_collect_recs(collect_))
	{
		uint64 ts = sisdb_collect_get_mindex(collect_, i);
		if (finder_ > ts)
		{
			if (dir == -1)
			{
				*mode_ = SIS_SEARCH_RIGHT;
				return i + 1;
			}
			dir = 1;
			i += dir;
			continue;
		}
		else if (finder_ < ts)
		{
			if (dir == 1)
			{
				*mode_ = SIS_SEARCH_RIGHT;
				return i;
			}
			dir = -1;
			i += dir;
			continue;
		}
		else
		{
			*mode_ = SIS_SEARCH_OK;
			// 如果相等，需要继续向前检索
			int k = i - 1;
			for (; k >= 0; k--)
			{
				ts = sisdb_collect_get_mindex(collect_, k);
				if (finder_ != ts)
				{
					break;
				}
				else
				{
					i = k;
				}
			}
			return i;
		}
	}

	if (dir == 1)
	{
		return -1;
	}
	else
	// if (dir == -1)
	{
		*mode_ = SIS_SEARCH_RIGHT;
		return 0;
	}
	// *mode_ = SIS_SEARCH_NONE;
	// return 0;
}

// 最后一个匹配的时间
// 12355579  查5返回5，查4返回3
int sisdb_collect_search_last(s_sisdb_collect *collect_, uint64 finder_, int *mode_)
{
	*mode_ = SIS_SEARCH_NONE;
	int index = sisdb_stepindex_goto(collect_->stepinfo, finder_);
	if (index < 0)
	{
		return -1;
	}
	int i = index;
	int dir = 0;
	int count = sisdb_collect_recs(collect_);
	while (i >= 0 && i < count)
	{
		uint64 ts = sisdb_collect_get_mindex(collect_, i);
		if (finder_ > ts)
		{
			if (dir == -1)
			{
				*mode_ = SIS_SEARCH_RIGHT;
				return i + 1;
			}
			dir = 1;
			i += dir;
			continue;
		}
		else if (finder_ < ts)
		{
			if (*mode_ == SIS_SEARCH_OK)
			{
				return i - 1;
			}
			if (dir == 1)
			{
				*mode_ = SIS_SEARCH_RIGHT;
				return i;
			}
			dir = -1;
			i += dir;
			continue;
		}
		else
		{
			*mode_ = SIS_SEARCH_OK;
			if (dir == -1)
			{
				return i;
			}
			else if (dir == 1)
			{
				i += dir;
			}
			else
			{
				dir = 1;
				i += dir;
			}
		}
	}
	if (*mode_ == SIS_SEARCH_OK)
	{
		return count - 1;
	}
	if (dir == 1)
	{
		return -1;
	}
	else
	// if (dir == -1)
	{
		*mode_ = SIS_SEARCH_RIGHT;
		return 0;
	}
	// *mode_ = SIS_SEARCH_NONE;
	// return 0;
}

//////////////////////////////////////////////////////
//  get  --  先默认二进制格式，全部字段返回数据
//////////////////////////////////////////////////////

bool sisdb_collect_trans_of_range(s_sisdb_collect *collect_, int *start_, int *stop_)
{
	int llen = sisdb_collect_recs(collect_);

	if (*start_ < 0)
	{
		*start_ = llen + *start_;
	}
	if (*stop_ < 0)
	{
		*stop_ = llen + *stop_;
	}
	if (*start_ < 0)
	{
		*start_ = 0;
	}
	if (*start_ > *stop_ || *start_ >= llen)
	{
		return false;
	}
	if (*stop_ >= llen)
	{
		*stop_ = llen - 1;
	}
	return true;
}
bool sisdb_collect_trans_of_count(s_sisdb_collect *collect_, int *start_, int *count_)
{
	if (*count_ == 0)
	{
		return false;
	}
	int llen = sisdb_collect_recs(collect_);

	if (*start_ < 0)
	{
		*start_ = llen + *start_;
	}
	if (*start_ < 0)
	{
		*start_ = 0;
	}
	if (*start_ >= llen)
	{
		return false;
	}
	if (*count_ < 0)
	{
		*count_ = abs(*count_);
		if (*count_ > (*start_ + 1))
		{
			*count_ = *start_ + 1;
		}
		*start_ -= (*count_ - 1);
	}
	else // count_ > 0
	{
		if ((*start_ + *count_) > llen)
		{
			*count_ = llen - *start_;
		}
	}
	return true;
}

s_sis_sds sisdb_collect_get_of_range_sds(s_sisdb_collect *collect_, int start_, int stop_)
{
	bool o = sisdb_collect_trans_of_range(collect_, &start_, &stop_);
	if (!o)
	{
		return NULL;
	}
	int count = (stop_ - start_) + 1;
	return sis_sdsnewlen(sis_struct_list_get(collect_->value, start_), count * collect_->value->len);
}
s_sis_sds sisdb_collect_get_of_count_sds(s_sisdb_collect *collect_, int start_, int count_)
{
	bool o = sisdb_collect_trans_of_count(collect_, &start_, &count_);
	if (!o)
	{
		return NULL;
	}
	return sis_sdsnewlen(sis_struct_list_get(collect_->value, start_), count_ * collect_->value->len);
}
s_sis_sds sisdb_collect_get_last_sds(s_sisdb_collect *collect_)
{
	if (!collect_->value||collect_->value->count < 1)
	{
		return NULL;
	}
	return sis_sdsnewlen(sis_struct_list_get(collect_->value, collect_->value->count - 1), collect_->value->len);
}

////////////////////////////////////////////
// main get
// 定义默认的检索工具 还需要定义
// 方法万能方法，不过速度慢，用于检索其他非排序字段的内容
// 有以下运算方法
// scope -- field min max 字段内容在min和max之间 【数值】
// same -- field val 字段和变量值相等 【字符串和数值】 根据字段类型比较，不区分大小写
// match -- first val 字段值有val内容 【字符串】val为子集 val:"sh600"
// in -- first val 字段值有val内容 【字符串】val为母集
// contain -- field set 字段内容包含在set集合中 set:"600600,600601" val为母集，相等才返回真【字符串或数值】
//
// $same : { field: code, value:600600 }
// $same : { field: code, value:600600, $scope:{ field: in_time, min: 111, max:222}} //复合条件查询
///////////////////////////////////////////
s_sis_sds sisdb_collect_get_original_sds(s_sisdb_collect *collect, s_sis_json_node *node_)
{
	// 检查取值范围，没有就全部取
	s_sis_sds o = NULL;

	int64 min, max;
	int start, stop;
	int count = 0;
	int maxX, minX;
	int offset = 0;

	s_sis_json_node *search = sis_json_cmp_child_node(node_, "search");
	if (!search)
	{
		o = sisdb_collect_get_of_range_sds(collect, 0, -1);
		return o;
	}
	int style = 0;
	if (sis_json_cmp_child_node(search, "min"))
	{
		style = SIS_SEARCH_MIN;
	} 
	else
	{
		if (sis_json_cmp_child_node(search, "max"))
		{
			style = SIS_SEARCH_MAX;
		} 
		else
		{
			if (sis_json_cmp_child_node(search, "start"))
			{
				style = SIS_SEARCH_START;
			}
		}	
	}
	
	if (style == SIS_SEARCH_MIN || style == SIS_SEARCH_MAX)
	{
		if (style == SIS_SEARCH_MIN)
		{
			min = sis_json_get_int(search, "min", 0);
		} 
		else
		{
			min = sis_json_get_int(search, "max", 0);
		}
		offset = sis_json_get_int(search, "offset", 0);

		if (sis_json_cmp_child_node(search, "count"))
		{
			count = sis_json_get_int(search, "count", 1);
			if (style == SIS_SEARCH_MIN)
			{
				start = sisdb_collect_search_right(collect, min, &minX);	
			} 
			else
			{
				start = sisdb_collect_search_left(collect, min, &minX);
				if (minX!=SIS_SEARCH_OK)
				{
					if (offset < 0)
					{
						offset++;
						count++;
					}
				}
			}
			if (start >= 0)
			{
				start += offset;
				start = sis_max(0, start);
				if (offset < 0)
				{
					count -= offset;
				}
				o = sisdb_collect_get_of_count_sds(collect, start, count);
			}
		}
		else
		{
			if (sis_json_cmp_child_node(search, "max"))
			{
				max = sis_json_get_int(search, "max", 0);
				start = sisdb_collect_search_right(collect, min, &minX);
				stop = sisdb_collect_search_left(collect, max, &maxX);
				// printf("%d---%d\n", start, stop);
				if (minX != SIS_SEARCH_NONE && maxX != SIS_SEARCH_NONE)
				{
					start += offset;
					start = sis_max(0, start);
					o = sisdb_collect_get_of_range_sds(collect, start, stop);
				}
			}
			else
			{
				start = sisdb_collect_search_right(collect, min, &minX);
				stop = sisdb_collect_search_left(collect, min, &maxX);
				// printf("%d---%d\n", start, stop);
				if (minX != SIS_SEARCH_NONE && maxX != SIS_SEARCH_NONE)
				{
					start += offset;
					start = sis_max(0, start);
					o = sisdb_collect_get_of_range_sds(collect, start, stop);
				}
			}
		}
		return o;
	}
	if (style == SIS_SEARCH_START)
	{
		start = sis_json_get_int(search, "start", -1); // -1 为最新一条记录
		if (sis_json_cmp_child_node(search, "count"))
		{
			count = sis_json_get_int(search, "count", 1);
			o = sisdb_collect_get_of_count_sds(collect, start, count);
		}
		else
		{
			if (sis_json_cmp_child_node(search, "stop"))
			{
				stop = sis_json_get_int(search, "stop", -1); // -1 为最新一条记录
				o = sisdb_collect_get_of_range_sds(collect, start, stop);
			}
			else
			{
				o = sisdb_collect_get_of_count_sds(collect, start, 1);
			}
		}
	}
	return o;
}

// 为保证最快速度，尽量不加参数
s_sis_sds sisdb_collect_fastget_sds(s_sisdb_collect *collect_,const char *key_)
{
	int start = 0;
	int count = collect_->value->count;

	if (count * collect_->sdb->db->size > SISDB_MAX_GET_SIZE)
	{
		count = SISDB_MAX_GET_SIZE / collect_->sdb->db->size;
		start = -1 * count;
	}
	// printf("%d,%d\n",start,count);
	s_sis_sds out = sisdb_collect_get_of_count_sds(collect_, start, count);

	if (!out)
	{
		return NULL;
	}
	// 最后转数据格式
	s_sis_sds other = sisdb_collect_struct_to_json_sds(collect_, out, key_, collect_->sdb->fields, true, true);

	sis_sdsfree(out);
	return other;
}

#define sisdb_field_is_whole(f) (!f || !sis_strncmp(f, "*", 1))

s_sis_sds sisdb_collect_get_sds(s_sisdb_collect *collect_, const char *key_, int iformat_, s_sis_json_node *node_)
{
	s_sis_sds out = sisdb_collect_get_original_sds(collect_, node_);
	if (!out)
	{
		return NULL;
	}

	const char *fields = NULL;
	if (sis_json_cmp_child_node(node_, "fields"))
	{
		fields = sis_json_get_str(node_, "fields");
	}

	s_sis_string_list *field_list = collect_->sdb->fields; //取得全部的字段定义
	if (!sisdb_field_is_whole(fields))
	{
		field_list = sis_string_list_create_w();
		sis_string_list_load(field_list, fields, sis_strlen(fields), ",");
	}

	s_sis_sds other = NULL;
	switch (iformat_)
	{
	case SISDB_FORMAT_BYTES:
		if (!sisdb_field_is_whole(fields))
		{
			other = sisdb_collect_struct_to_sds(collect_, out, field_list);
			sis_sdsfree(out);
			out = other;
		}
		break;
	case SISDB_FORMAT_JSON:
		other = sisdb_collect_struct_to_json_sds(collect_, out, key_, field_list, true, true);
		sis_sdsfree(out);
		out = other;
		break;
	case SISDB_FORMAT_ARRAY:
		other = sisdb_collect_struct_to_array_sds(collect_, out, field_list, true);
		sis_sdsfree(out);
		out = other;
		break;
	case SISDB_FORMAT_CSV:
		other = sisdb_collect_struct_to_csv_sds(collect_, out, field_list, false);
		sis_sdsfree(out);
		out = other;
		break;
	}
	if (!sisdb_field_is_whole(fields))
	{
		sis_string_list_destroy(field_list);
	}
	return out;
}

///////////////////////////////////////////////////////////////////////////////
//取数据,读表中代码为key的数据，key为*表示所有股票数据，由 com_ 定义数据范围和字段范围
///////////////////////////////////////////////////////////////////////////////

s_sis_json_node *sisdb_collects_get_last_node(s_sisdb_collect *collect_, s_sis_json_node *node_)
{
	s_sis_sds val = sisdb_collect_get_of_count_sds(collect_, -1, 1);
	if (!val)
	{
		return NULL;
	}

	const char *fields = NULL;
	if (sis_json_cmp_child_node(node_, "fields"))
	{
		fields = sis_json_get_str(node_, "fields");
	}

	s_sis_string_list *field_list = collect_->sdb->fields; //取得全部的字段定义
	if (!sisdb_field_is_whole(fields))
	{
		field_list = sis_string_list_create_w();
		sis_string_list_load(field_list, fields, sis_strlen(fields), ",");
	}

	int fnums = sis_string_list_getsize(field_list);
	// case SISDB_FORMAT_JSON:
	s_sis_json_node *jval = sis_json_create_array();
	for (int i = 0; i < fnums; i++)
	{
		s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(collect_->sdb->db->fields, sis_string_list_get(field_list, i));
		sis_dynamic_field_to_array(jval, inunit, val);
	}
	
	sis_sdsfree(val);
	if (!sisdb_field_is_whole(fields))
	{
		sis_string_list_destroy(field_list);
	}

	return jval;
}

