

#include <sisdb_collect.h>
#include <sisdb_io.h>

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_step_index --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sis_step_index *sisdb_stepindex_create()
{
	s_sis_step_index *si = SIS_MALLOC(s_sis_step_index, si);
	return si;
}
void sisdb_stepindex_destroy(s_sis_step_index *si_)
{
	sis_free(si_);
}
void sisdb_stepindex_clear(s_sis_step_index *si_)
{
	si_->left = 0;
	si_->right = 0;
	si_->count = 0;
	si_->step = 0.0;
}
void sisdb_stepindex_rebuild(s_sis_step_index *si_, uint64 left_, uint64 right_, int count_)
{
	si_->left = left_;
	si_->right = right_;
	si_->count = count_;
	si_->step = 0.0;
	if (count_ > 0)
	{
		si_->step = (right_ - left_) / count_;
	}
}
int sisdb_stepindex_goto(s_sis_step_index *si_, uint64 curr_)
{
	if (si_->count < 1)
	{
		return -1;
	}
	if (curr_ <= si_->left)
	{
		return 0;
	}
	if (curr_ >= si_->right)
	{
		return si_->count - 1;
	}
	// printf("goto %f\n", si_->step);
	int index = 0;
	if (si_->step > 1.000001)
	{
		index = (int)((curr_ - si_->left) / si_->step);
	}
	if (index > si_->count - 1)
	{
		return si_->count - 1;
	}
	return index;
}
///////////////////////////////////////////////////////////////////////////
//------------------------s_sisdb_collect --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sisdb_collect *sisdb_get_collect(s_sisdb_cxt *sisdb_, char *key_)
{
	s_sisdb_collect *collect = NULL;
	if (sisdb_->collects)
	{
		collect = sis_map_pointer_get(sisdb_->collects, key_);
	}
	return collect;
}
s_sisdb_table *sisdb_get_table(s_sisdb_cxt *sisdb_, char *dbname_)
{
	s_sisdb_table *tb = (s_sisdb_table *)sis_map_list_get(sisdb_->sdbs, dbname_);
	return tb;
}

s_sisdb_collect *sisdb_collect_create(s_sisdb_cxt *sisdb_, char *key_)
{

	char code[128];
	char dbname[128];
    sis_str_divide(key_, '.', code, dbname);

	s_sisdb_table *sdb = sisdb_get_table(sisdb_, dbname);
	if (!sdb)
	{
		return NULL;
	}
	s_sisdb_collect *o = SIS_MALLOC(s_sisdb_collect, o);
	sis_map_pointer_set(sisdb_->collects, key_, o);

	o->father = sisdb_;
	o->sdb = sdb;

	o->stepinfo = sisdb_stepindex_create();

	o->value = sis_struct_list_create(sdb->db->size);

	return o;
}

void sisdb_collect_destroy(void *collect_)
{
	s_sisdb_collect *collect = (s_sisdb_collect *)collect_;
	{
		sis_struct_list_destroy(collect->value);
	}
	if (collect->stepinfo)
	{
		sisdb_stepindex_destroy(collect->stepinfo);
	}
	sis_free(collect);
}

void sisdb_collect_clear(s_sisdb_collect *collect_)
{
	sis_struct_list_clear(collect_->value);
	sisdb_stepindex_clear(collect_->stepinfo);
}

////////////////////////
//
////////////////////////
msec_t sisdb_collect_get_time(s_sisdb_collect *collect_, int index_)
{
	return sis_dynamic_db_gettime(collect_->sdb, index_, 
		sis_struct_list_first(collect_->value), 
		collect_->value->count * collect_->sdb->size);
}

int sisdb_collect_recs(s_sisdb_collect *collect_)
{
	if (!collect_ || !collect_->value)
	{
		return 0;
	}
	return collect_->value->count;
}

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
		uint64 ts = sisdb_collect_get_time(collect_, i);
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
		uint64 ts = sisdb_collect_get_time(collect_, i);
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
				ts = sisdb_collect_get_time(collect_, k);
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
		uint64 ts = sisdb_collect_get_time(collect_, i);
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
				ts = sisdb_collect_get_time(collect_, k);
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
		uint64 ts = sisdb_collect_get_time(collect_, i);
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

bool _sisdb_trans_of_range(s_sisdb_collect *collect_, int *start_, int *stop_)
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
bool _sisdb_trans_of_count(s_sisdb_collect *collect_, int *start_, int *count_)
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
	bool o = _sisdb_trans_of_range(collect_, &start_, &stop_);
	if (!o)
	{
		return NULL;
	}
	int count = (stop_ - start_) + 1;
	return sis_sdsnewlen(sis_struct_list_get(collect_->value, start_), count * collect_->value->len);
}
s_sis_sds sisdb_collect_get_of_count_sds(s_sisdb_collect *collect_, int start_, int count_)
{
	bool o = _sisdb_trans_of_count(collect_, &start_, &count_);
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
// format trans
///////////////////////////////////////////
s_sis_json_node *sis_collect_get_fields_of_json(s_sisdb_collect *collect_, s_sis_string_list *fields_)
{
	s_sis_json_node *node = sis_json_create_object();

	// 先处理字段
	int  sno = 0;
	char sonkey[64];
	int count = sis_string_list_getsize(fields_);
	for (int i = 0; i < count; i++)
	{
		const char *key = sis_string_list_get(fields_, i);
		s_sisdb_field *fu = sis_dynamic_db_get_field(collect_->sdb->db, NULL, key);
		if(!fu) continue;
		if (fu->count > 1)
		{
			for(int index = 0; index < fu->count; index++)
			{
				sis_sprintf(sonkey, 64, "%s.%d", key, index);
				sis_json_object_add_uint(node, sonkey, sno);
				sno++;
			}
		}
		else
		{
			sis_json_object_add_uint(node, key, sno);
			sno++;
		}		
	}
	// sis_json_object_add_node(jone, SIS_JSON_KEY_FIELDS, jfields);
	return node;
}

s_sis_sds sis_collect_get_fields_of_csv(s_sisdb_collect *collect_, s_sis_string_list *fields_)
{
	s_sis_sds *o = sis_sdsempty();

	// 先处理字段
	int  sno = 0;
	char sonkey[64];
	int count = sis_string_list_getsize(fields_);
	for (int i = 0; i < count; i++)
	{
		const char *key = sis_string_list_get(fields_, i);
		s_sisdb_field *fu = sis_dynamic_db_get_field(collect_->sdb->db, NULL, key);
		if(!fu) continue;
		if (fu->count > 1)
		{
			for(int index = 0; index < fu->count; index++)
			{
				sis_sprintf(sonkey, 64, "%s.%d", key, index);
				o = sis_csv_make_str(o, sonkey, sis_strlen(sonkey));
				sno++;
			}
		}
		else
		{
			o = sis_csv_make_str(o, key, sis_strlen(key));
			sno++;
		}		
	}
	o = sis_csv_make_end(o);
	return o;
}

s_sis_sds sisdb_collect_struct_to_sds(s_sisdb_collect *collect_, s_sis_sds in_, s_sis_string_list *fields_)
{
	// 一定不是全字段
	s_sis_sds o = NULL;

	int count = (int)(sis_sdslen(in_) / collect_->value->len);
	char *val = in_;
	for (int k = 0; k < count; k++)
	{
		for (int i = 0; i < sis_string_list_getsize(fields_); i++)
		{
			int index = 0;
			s_sisdb_field *fu = sis_dynamic_db_get_field(collect_->sdb->db, &index, sis_string_list_get(fields_, i));
			if (!fu)
			{
				continue;
			}
			if (!o)
			{
				o = sis_sdsnewlen(val + fu->offset + index * fu->len, fu->len);
			}
			else
			{
				o = sis_sdscatlen(o, val + fu->offset + index * fu->len, fu->len);
			}
		}
		val += collect_->sdb->db->size;
	}
	return o;
}

s_sis_sds sisdb_collect_struct_to_json_sds(s_sisdb_collect *collect_, s_sis_sds in_,
										   const char *key_, s_sis_string_list *fields_,  bool isfields_, bool zip_)
{
	
	int fnums = sis_string_list_getsize(fields_);

	s_sis_json_node *jone = sis_json_create_object();
	
	if (isfields_)
	{
		s_sis_json_node *jfields = sis_json_create_object();
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(collect_->sdb->fields, sis_string_list_get(fields_, i));
			sis_json_object_add_uint(jfields, inunit->name, i);
		}
		sis_json_object_add_node(jone, "fields", jfields);
	}

	s_sis_json_node *jtwo = sis_json_create_array();
	const char *val = in_;
	int count = (int)(sis_sdslen(in_) / collect_->sdb->db->size);
	for (int k = 0; k < count; k++)
	{
		s_sis_json_node *jval = sis_json_create_array();
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(collect_->sdb->fields, sis_string_list_get(fields_, i));
			sis_dynamic_field_to_array(jval, inunit, val);
		}
		sis_json_array_add_node(jtwo, jval);
		val += collect_->sdb->db->size;
	}
	sis_json_object_add_node(jone, key_? key_ : "values", jtwo);

	char *str;
	size_t olen;
	if (zip_)
	{
		str = sis_json_output_zip(jone, &olen);
	}
	else
	{
		str = sis_json_output(jone, &olen);
	}
	s_sis_sds o = sis_sdsnewlen(str, olen);
	sis_free(str);
	sis_json_delete_node(jone);	
	return o;
}

s_sis_sds sisdb_collect_struct_to_array_sds(s_sisdb_collect *collect_, s_sis_sds in_,
											s_sis_string_list *fields_, bool zip_)
{
	s_sis_json_node *jone = sis_json_create_array();
	int fnums = sis_string_list_getsize(fields_);

	const char *val = in_;
	int count = (int)(sis_sdslen(in_) / collect_->sdb->db->size);
	for (int k = 0; k < count; k++)
	{
		s_sis_json_node *jval = sis_json_create_array();
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(collect_->sdb->fields, sis_string_list_get(fields_, i));
			sis_dynamic_field_to_array(jval, inunit, val);
		}
		sis_json_array_add_node(jone, jval);
		val += collect_->sdb->db->size;
	}
	char *str;
	size_t olen;
	if (zip_)
	{
		str = sis_json_output_zip(jone, &olen);
	}
	else
	{
		str = sis_json_output(jone, &olen);
	}
	s_sis_sds o = sis_sdsnewlen(str, olen);
	sis_free(str);
	sis_json_delete_node(jone);	
	return o;
}

s_sis_sds sisdb_collect_struct_to_csv_sds(s_sisdb_collect *collect_, s_sis_sds in_,
										  s_sis_string_list *fields_, bool isfields_)
{
	s_sis_sds o = sis_sdsempty();
	int fnums = sis_string_list_getsize(fields_);
	if (isfields_)
	{
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(collect_->sdb->fields, sis_string_list_get(fields_, i));
			o = sis_csv_make_str(o, inunit->name, sis_sdslen(inunit->name));
		}
		o = sis_csv_make_end(o);
	}

	char *val = in_;
	int count = (int)(sis_sdslen(in_) / collect_->value->len);

	for (int k = 0; k < count; k++)
	{
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(collect_->sdb->fields, sis_string_list_get(fields_, i));
			o = sis_dynamic_field_to_csv(o, inunit, val);
			val += collect_->value->len;
		}
		o = sis_csv_make_end(o);
		val += collect_->sdb->db->size;
	}
	return o;
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

	char *fields = NULL;
	if (sis_json_cmp_child_node(node_, "fields"))
	{
		fields = sis_json_get_str(node_, "fields");
	}

	s_sis_string_list *field_list = collect_->sdb->fields; //取得全部的字段定义
	if (!sisdb_field_is_whole(fields))
	{
		field_list = sis_string_list_create_w();
		sis_string_list_load(field_list, fields, strlen(fields), ",");
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

	char *fields = NULL;
	if (sis_json_cmp_child_node(node_, "fields"))
	{
		fields = sis_json_get_str(node_, "fields");
	}

	s_sis_string_list *field_list = collect_->sdb->fields; //取得全部的字段定义
	if (!sisdb_field_is_whole(fields))
	{
		field_list = sis_string_list_create_w();
		sis_string_list_load(field_list, fields, strlen(fields), ",");
	}

	int fnums = sis_string_list_getsize(field_list);
	// case SISDB_FORMAT_JSON:
	s_sis_json_node *jval = sis_json_create_array();
	for (int i = 0; i < fnums; i++)
	{
		s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(collect_->sdb->fields, sis_string_list_get(field_list, i));
		sis_dynamic_field_to_array(jval, inunit, val);
	}
	
	sis_sdsfree(val);
	if (!sisdb_field_is_whole(fields))
	{
		sis_string_list_destroy(field_list);
	}

	return jval;
}

//////////////////////////////////////////////////
//  delete
//////////////////////////////////////////////////
int _sisdb_collect_delete(s_sisdb_collect *collect_, int start_, int count_)
{
	sis_struct_list_delete(collect_->value, start_, count_);
	sisdb_stepindex_rebuild(collect_->stepinfo,
							sisdb_collect_get_time(collect_, 0),
							sisdb_collect_get_time(collect_, collect_->value->count - 1),
							collect_->value->count);
	return 0;
}
int sisdb_collect_delete_of_range(s_sisdb_collect *collect_, int start_, int stop_)
{
	bool o = _sisdb_trans_of_range(collect_, &start_, &stop_);
	if (!o)
	{
		return 0;
	}
	int count = (stop_ - start_) + 1;
	return _sisdb_collect_delete(collect_, start_, count);
}

int sisdb_collect_delete_of_count(s_sisdb_collect *collect_, int start_, int count_)
{
	bool o = _sisdb_trans_of_count(collect_, &start_, &count_);
	if (!o)
	{
		return 0;
	}
	return _sisdb_collect_delete(collect_, start_, count_);
}

int sisdb_collect_delete(s_sisdb_collect  *collect_, s_sis_json_node *jsql)
{
	s_sis_json_node *search = sis_json_cmp_child_node(jsql, "search");
	if (!search)
	{
		return collect_->value->count;
	}
	uint64 min, max;
	int start, stop;
	int count = 0;
	int out = 0;
	int minX, maxX;

	bool by_time = sis_json_cmp_child_node(search, "min") != NULL;
	bool by_region = sis_json_cmp_child_node(search, "start") != NULL;
	if (by_time)
	{
		min = sis_json_get_int(search, "min", 0);
		if (sis_json_cmp_child_node(search, "count"))
		{
			count = sis_json_get_int(search, "count", 1);
			start = sisdb_collect_search(collect_, min);
			if (start >= 0)
			{
				out = sisdb_collect_delete_of_count(collect_, start, count);
			}
		}
		else
		{
			if (sis_json_cmp_child_node(search, "max"))
			{
				max = sis_json_get_int(search, "max", 0);
				start = sisdb_collect_search_right(collect_, min, &minX);
				stop = sisdb_collect_search_left(collect_, max, &maxX);
				if (minX != SIS_SEARCH_NONE && maxX != SIS_SEARCH_NONE)
				{
					out = sisdb_collect_delete_of_range(collect_, start, stop);
				}
			}
			else
			{
				start = sisdb_collect_search(collect_, min);
				if (start >= 0)
				{
					out = sisdb_collect_delete_of_count(collect_, start, 1);
				}
			}
		}
		return collect_->value->count;
	}
	if (by_region)
	{
		start = sis_json_get_int(search, "start", -1); // -1 为最新一条记录
		if (sis_json_cmp_child_node(search, "count"))
		{
			count = sis_json_get_int(search, "count", 1);
			out = sisdb_collect_delete_of_count(collect_, start, count); // 定位后按数量删除
		}
		else
		{
			if (sis_json_cmp_child_node(search, "stop"))
			{
				stop = sis_json_get_int(search, "stop", -1);			   // -1 为最新一条记录
				out = sisdb_collect_delete_of_range(collect_, start, stop); // 定位后删除
			}
			else
			{
				out = sisdb_collect_delete_of_count(collect_, start, 1); // 定位后按数量删除
			}
		}
	}
	return collect_->value->count;
}

////////////////////////
//  update
////////////////////////

s_sis_sds sisdb_collect_json_to_struct_sds(s_sisdb_collect *collect_, const char *in_, size_t ilen_)
{
	s_sis_json_handle *handle = sis_json_load(in_, ilen_);
	if (!handle)
	{
		return NULL;
	}
	// 取最后一条记录的数据作为底
	const char *src = sis_struct_list_last(collect_->value);
	s_sis_sds o = sis_sdsnewlen(src, collect_->value->len);

	s_sisdb_table *tb = collect_->sdb;

	int index = 0;
	int fnums = sis_string_list_getsize(tb->fields);
	char key[64];
	for (int k = 0; k < fnums; k++)
	{
		s_sisdb_field *fu = (s_sisdb_field *)sis_map_list_geti(tb->db->fields, k);
		for (int i = 0; i < fu->count; i++)
		{
			sis_sprintf(key, 64, "%s.%d", fu->name, i);
			sis_dynamic_field_json_to_struct(o + index * collect_->value->len, fu, i, key, handle->node);
		}
	}
	sis_json_close(handle);
	return o;
}

s_sis_sds sisdb_collect_array_to_struct_sds(s_sisdb_collect *collect_, const char *in_, size_t ilen_)
{
	// 字段个数一定要一样
	s_sis_json_handle *handle = sis_json_load(in_, ilen_);
	if (!handle)
	{
		return NULL;
	}
	// 获取字段个数
	s_sis_json_node *jval = NULL;

	int count = 0;
	if (handle->node->child && handle->node->child->type == SIS_JSON_ARRAY)
	{ // 表示二维数组
		jval = handle->node->child;
		count = sis_json_get_size(handle->node);
	}
	else
	{
		count = 1;
		jval = handle->node;
	}
	if (count < 1)
	{
		sis_json_close(handle);
		return NULL;
	}

	int fnums = sis_string_list_getsize(collect_->sdb->fields);
	s_sis_sds o = sis_sdsnewlen(NULL, count * collect_->value->len);
	int index = 0;
	while (jval)
	{
		int size = sis_json_get_size(jval);
		if (size != fnums)
		{
			LOG(3)("input fields[%d] count error [%d].\n", size, fnums);
			jval = jval->next;
			continue;
		}
		char key[16];
		int fidx = 0;
		for (int k = 0; k < fnums; k++)
		{
			s_sisdb_field *fu = (s_sisdb_field *)sis_map_list_geti(collect_->sdb->db->fields, k);
			for (int i = 0; i < fu->count; i++)
			{
				sis_sprintf(key, 10, "%d", fidx);
				sis_dynamic_field_json_to_struct(o + index * collect_->value->len, fu, fidx, key, jval);
				fidx++;
			}
		}
		index++;
		jval = jval->next;
	}

	sis_json_close(handle);
	return o;
}

s_sis_sds _sisdb_make_catch_inited_sds(s_sisdb_collect *collect_, const char *in_)
{
	s_sisdb_table *tb = collect_->db;
	s_sis_sds o = sis_sdsnewlen(in_, sis_sdslen(collect_->front));
	int fields = sis_string_list_getsize(tb->field_name);

	s_sis_collect_method_buffer obj;
	obj.init = true;
	obj.in = in_;
	obj.out = o;
	obj.collect = collect_;

	for (int k = 0; k < fields; k++)
	{
		const char *key = sis_string_list_get(tb->field_name, k);
		s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, key);
		if (!fu || !fu->subscribe_method)
		{
			continue;
		}
		if (!sisdb_field_is_integer(fu))
		{
			continue;
		}

		obj.field = fu;
		fu->subscribe_method->obj = &obj;
		sis_method_class_execute(fu->subscribe_method);
	}
	// printf("o=%lld\n",sisdb_field_get_uint_from_key(tb,"time",o));
	// printf("in_=%lld\n",sisdb_field_get_uint_from_key(tb,"time",in_));
	return o;
}
s_sis_sds sisdb_make_catch_moved_sds(s_sisdb_collect *collect_, const char *in_)
{
	s_sisdb_table *tb = collect_->db;
	// 获取最后一条记录
	s_sis_sds moved = sis_struct_list_last(collect_->value);

	// 保存源数据
	s_sis_sds o = sis_sdsnewlen(in_, sis_sdslen(collect_->front));
	int fields = sis_string_list_getsize(tb->field_name);

	s_sis_collect_method_buffer obj;
	obj.init = false;
	obj.in = in_;
	obj.last = moved;
	obj.out = o;
	obj.collect = collect_;

	for (int k = 0; k < fields; k++)
	{
		const char *key = sis_string_list_get(tb->field_name, k);
		s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, key);

		if (!fu || !fu->subscribe_method)
		{
			continue;
		}
		if (!sisdb_field_is_integer(fu))
		{
			continue;
		}

		obj.field = fu;
		fu->subscribe_method->obj = &obj;
		sis_method_class_execute(fu->subscribe_method);
	}
	return o;
}

int _sisdb_collect_update_alone_check_solely(s_sisdb_collect *collect_, const char *in_)
{
	s_sisdb_table *tb = collect_->db;
	// 采用直接内存比较

	for (int i = 0; i < collect_->value->count; i++)
	{
		const char *val = sis_struct_list_get(collect_->value, i);
		bool ok = true;
		for (int k = 0; k < tb->write_solely->count; k++)
		{
			s_sisdb_field *fu = (s_sisdb_field *)sis_pointer_list_get(tb->write_solely, k);
			// sis_out_binary("in :", in_+fu->offset, fu->flags.len);
			// sis_out_binary("val:", val+fu->offset, fu->flags.len);
			if (memcmp(in_ + fu->offset, val + fu->offset, fu->flags.len))
			{
				ok = false;
				break;
			}
		}
		// printf("ok=%d\n", ok);
		if (ok) //  所有字段都相同
		{
			sis_struct_list_update(collect_->value, i, (void *)in_);
			return 0;
		}
	}
	sis_struct_list_push(collect_->value, (void *)in_);
	return 1;
}

int _sisdb_collect_update_alone_check_solely_time(s_sisdb_collect *collect_, const char *in_, int index_, uint32 time_)
{
	s_sisdb_table *tb = collect_->db;
	// 采用直接内存比较
	s_sisdb_field *field = sisdb_field_get_from_key(tb, "time");
	for (int i = index_; i >= 0; i--)
	{
		const char *val = sis_struct_list_get(collect_->value, i);
		if (sisdb_field_get_uint(field, val) != time_)
		{
			break;
		}
		bool ok = true;
		for (int k = 0; k < tb->write_solely->count; k++)
		{
			s_sisdb_field *fu = (s_sisdb_field *)sis_pointer_list_get(tb->write_solely, k);
			// sis_out_binary("in :", in_+fu->offset, fu->flags.len);
			// sis_out_binary("val:", val+fu->offset, fu->flags.len);
			if (memcmp(in_ + fu->offset, val + fu->offset, fu->flags.len))
			{
				ok = false;
				break;
			}
		}
		// printf("ok=%d\n", ok);
		if (ok) //  所有字段都相同
		{
			sis_struct_list_update(collect_->value, i, (void *)in_);
			return 0;
		}
	}
	if (index_ == collect_->value->count - 1)
	{
		sis_struct_list_push(collect_->value, (void *)in_);
	}
	else
	{
		sis_struct_list_insert(collect_->value, index_, (void *)in_);
	}
	return 1;
}
// 检查是否增加记录，只和最后一条记录做比较，返回4个，一是当日最新记录，一是新记录，一是老记录, 一个为当前记录
int _sisdb_collect_check_lasttime(s_sisdb_collect *collect_, uint64 finder_)
{
	if (collect_->value->count < 1)
	{
		return SIS_CHECK_LASTTIME_INIT;
	}
	s_sisdb_table *tb = collect_->db;

	s_sisdb_field *field = sisdb_field_get_from_key(tb, "time");
	const char *last = sis_struct_list_last(collect_->value);
	uint64 last_time = sisdb_field_get_uint(field, last); // 得到时间序列值

	if (finder_ == last_time)
	{
		return SIS_CHECK_LASTTIME_OK;
	}
	else if (finder_ > last_time)
	{
		// 仅仅当秒才起作用
		if (field->flags.type == SIS_FIELD_TYPE_SECOND &&
			sis_time_get_idate(finder_) > sis_time_get_idate(last_time))
		{
			printf(">>> %s : \n", tb->name);
			return SIS_CHECK_LASTTIME_INIT;
		}
		else
		{
			return SIS_CHECK_LASTTIME_NEW;
		}
	}
	else // if (finder_ < last_time)
	{
		// 应该判断量，如果量小就返回错误
		return SIS_CHECK_LASTTIME_OLD;
	}
}

void _sisdb_collect_check_lastdate(s_sisdb_collect *collect_, const char *in_)
{
	s_sisdb_table *tb = collect_->db;
	s_sisdb_field *field = sisdb_field_get_from_key(tb, "time");
	if (!field)
		return;

	uint64 finder_ = sisdb_field_get_uint(field, in_); // 得到时间序列值
	const char *last = sis_struct_list_last(collect_->value);
	uint64 last_time = sisdb_field_get_uint(field, last); // 得到时间序列值

	if (field->flags.type == SIS_FIELD_TYPE_SECOND &&
		(sis_time_get_idate(finder_) != sis_time_get_idate(last_time)||
		finder_ < last_time))
	{
		if (tb->control.isinit)
		{
			sisdb_collect_clear(collect_);
		}
		else if (tb->control.ispubs)
		{
			sisdb_collect_clear_subs(collect_);
			// 还应该删除一些记录
			int mode;
			int start = sisdb_collect_search_right(collect_, finder_, &mode);
			time_t stoptime = sis_time_make_time(sis_time_get_idate(finder_), 235959);
			int stop = sisdb_collect_search_left(collect_, stoptime, &mode);
			// printf("start=%d, stop=%d count = %d\n", start, stop, collect_->value->count);
			if (start >=0 && stop >=0)
			{
				// 这里应该判断只删除一天的数据
				sisdb_collect_delete_of_range(collect_, start, stop);
			}
			else
			{
				sisdb_collect_clear(collect_);
			}
		}
		// printf(">>> %s : [%d] %d %d %d %d\n", tb->name, collect_->value->count,
		// field->flags.type, tb->control.isinit, sis_time_get_idate(finder_),
		// sis_time_get_idate(last_time));
	}
	if (field->flags.type == SIS_FIELD_TYPE_UINT && tb->control.scale == SIS_TIME_SCALE_INCR &&
		finder_ < last_time)
	{
		if (tb->control.isinit)
		{
			sisdb_collect_clear(collect_);
		}
		else if (tb->control.ispubs)
		{
			sisdb_collect_clear_subs(collect_);
		}

		// printf(">>>>> %s : [%d] %d %d %d %d\n", tb->name, collect_->value->count,
		// field->flags.type, tb->control.isinit, finder_, last_time);
	}
}
int _sisdb_collect_update_alone(s_sisdb_collect *collect_, const char *in_, bool ispub_)
{
	s_sisdb_table *tb = collect_->db;
	// printf("tb = %p\n", tb);
	tb->control.ispubs = (ispub_ & tb->control.issubs);
	if (tb->control.isinit || tb->control.ispubs)
	{
		_sisdb_collect_check_lastdate(collect_, in_);
	}
	if (tb->write_style == SIS_WRITE_ALWAYS)
	{
		// 不做任何检查直接写入
		sis_struct_list_push(collect_->value, (void *)in_);
		return 1;
	}
	// 先检查数据是否合法，合法就往下，不合法就直接返回
	if (tb->write_method)
	{
		s_sis_collect_method_buffer obj;
		obj.in = in_;
		obj.collect = collect_;
		tb->write_method->obj = &obj;

		bool *ok = sis_method_class_execute(tb->write_method);
		if (!*ok)
		{
			// s_sisdb_field *field = sisdb_field_get_from_key(tb, "time");
			// if (field)
			// {
			// 	uint64 in_time = sisdb_field_get_uint(field, in_); // 得到时间序列值
			// 	uint64 last_time = sisdb_field_get_uint(field, sis_struct_list_last(collect_->value)); // 得到时间序列值
			// 	uint64 sub_time = sisdb_field_get_uint(field, collect_->lasted);
			// 	printf("xxxx time %s %lld %lld %lld\n", tb->name, last_time, in_time, sub_time);
			// }
			// field = sisdb_field_get_from_key(tb, "vol");
			// if (field)
			// {
			// 	uint64 in_time= sisdb_field_get_uint(field, in_); // 得到时间序列值
			// 	uint64 last_time = sisdb_field_get_uint(field, sis_struct_list_last(collect_->value)); // 得到时间序列值
			// 	uint64 sub_time = sisdb_field_get_uint(field, collect_->lasted);
			// 	printf("xxxx vol %s %lld %lld %lld\n", tb->name, last_time, in_time, sub_time);
			// }
			// 数据检查不合法
			return 0;
		}
	}

	// 不需要排序，应该仅仅处理唯一字段即可
	if (tb->write_style & SIS_WRITE_SORT_NONE)
	{

		// printf("tb->write_style= %d %d \n", tb->write_style , tb->write_style & SIS_WRITE_SOLE_NONE);
		if (tb->write_style & SIS_WRITE_SOLE_NONE)
		{
			sis_struct_list_push(collect_->value, (void *)in_);
			return 1;
		}
		else
		{
			return _sisdb_collect_update_alone_check_solely(collect_, in_);
		}
	}
	// 需要检查按什么排序，目前暂时只能按time排序
	if (tb->write_style & SIS_WRITE_SORT_OTHER)
	{
		return 0;
	}
	/////////////////////////////////////////////////
	// 下面按 time 为索引操作数据，
	////////////////////////////////////////////////

	s_sisdb_field *field = sisdb_field_get_from_key(tb, "time");
	if (!field)
		return 0;
	uint64 in_time = sisdb_field_get_uint(field, in_); // 得到时间序列值

	int offset = field->offset + field->flags.len; // 得到实际数据区，用于数据比较

	// 检查待插入的时间位置
	int search_mode = _sisdb_collect_check_lasttime(collect_, in_time);
	// if (search_mode==SIS_CHECK_LASTTIME_INIT)
	// printf("mode=%d tt= %lld  %d\n", search_mode, in_time, sis_time_get_idate(in_time));
	// sis_out_binary("update", in_, size);

	// printf("----=%d tt= %lld index=%d\n", mode, tt, index);
	if (search_mode == SIS_CHECK_LASTTIME_NEW)
	{
		if (tb->control.ispubs)
		{
			if (memcmp(collect_->lasted + offset, in_ + offset, collect_->value->len - offset))
			{
				sisdb_field_copy(collect_->front, collect_->lasted, collect_->value->len);
				// -- 先根据in -- tb->front生成新数据
				s_sis_sds in = _sisdb_make_catch_inited_sds(collect_, in_);
				// -- 再覆盖老数据
				sis_struct_list_push(collect_->value, (void *)in);
				sisdb_field_copy(collect_->lasted, in_, collect_->value->len);
				sis_sdsfree(in);
			}
			else
			{
				return 0;
			}
		}
		else
		{
			sis_struct_list_push(collect_->value, (void *)in_);
		}
	}
	else if (search_mode == SIS_CHECK_LASTTIME_INIT)
	{
		// 1. 初始化
		if (tb->control.ispubs)
		{
			sisdb_field_copy(collect_->front, NULL, collect_->value->len); // 保存的是全量，成交量使用
			sisdb_field_copy(collect_->lasted, in_, collect_->value->len); //
		}
		// 2. 写入数据
		sis_struct_list_push(collect_->value, (void *)in_);
		// sis_out_binary("set push", in_, size);
	}
	else if (search_mode == SIS_CHECK_LASTTIME_OK)
	{
		// 如果发现记录，保存原始值到lasted，然后计算出实际要写入的值
		// 1. 初始化
		int index = collect_->value->count - 1;
		if (tb->control.ispubs)
		{
			// -- 先根据in -- tb->front生成新数据
			if (memcmp(collect_->lasted + offset, in_ + offset, collect_->value->len - offset))
			{
				s_sis_sds in = sisdb_make_catch_moved_sds(collect_, in_);
				// if (tb->write_style & SIS_WRITE_SOLE_TIME)
				// {
				sis_struct_list_update(collect_->value, index, (void *)in);
				// }
				// else
				// {
				// 	sis_struct_list_push(collect_->value, (void *)in_);
				// }
				sisdb_field_copy(collect_->lasted, in_, collect_->value->len);
				sis_sdsfree(in);
			}
			else
			{
				return 0;
			}
		}
		else
		{
			// printf("tb->write_style= %d \n", tb->write_style );
			if (tb->write_style & SIS_WRITE_SOLE_TIME)
			{
				sis_struct_list_update(collect_->value, index, (void *)in_);
			}
			else if (tb->write_style & SIS_WRITE_SOLE_MULS || tb->write_style & SIS_WRITE_SOLE_OTHER)
			{
				_sisdb_collect_update_alone_check_solely_time(collect_, in_, index, in_time);
			}
			else
			{
				sis_struct_list_push(collect_->value, (void *)in_);
			}
		}
	}
	else // if (search_mode == SIS_CHECK_LASTTIME_OLD)
	{
		if (tb->control.ispubs)
		{
			// 广播数据，不能插入，只能追加
		}
		else
		{
			// 时间是很早以前的数据，那就重新定位数据
			int set = SIS_SEARCH_NONE;
			int index = sisdb_collect_search_last(collect_, in_time, &set);
			if (set == SIS_SEARCH_OK)
			{
				// printf("index =%d write_style= %d \n", index, (tb->write_style & SIS_WRITE_SOLE_TIME));
				if (tb->write_style & SIS_WRITE_SOLE_TIME)
				{
					sis_struct_list_update(collect_->value, index, (void *)in_);
				}
				else if (tb->write_style & SIS_WRITE_SOLE_MULS || tb->write_style & SIS_WRITE_SOLE_OTHER)
				{
					_sisdb_collect_update_alone_check_solely_time(collect_, in_, index, in_time);
				}
				else
				{
					sis_struct_list_insert(collect_->value, index + 1, (void *)in_);
				}
			}
			else
			{
				sis_struct_list_insert(collect_->value, index, (void *)in_);
			}
		}
		// printf("count=%d\n",collect_->value->count);
	}

	return 1;
}


int sisdb_collect_update(s_sisdb_collect *collect_, s_sis_sds in_)
{
	int ilen = sis_sdslen(in_);
	if (ilen < 1)
	{
		return 0;
	}

	int count = 0;
	count = (int)(ilen / collect_->value->len);
	//这里应该判断数据完整性
	if (count * collect_->value->len != ilen)
	{
		LOG(3)("source format error [%d*%d!=%u]\n", count, collect_->value->len, ilen);
		return 0;
	}
	// printf("-----count =%d len=%d:%d\n", count, ilen, collect_->value->len);
	const char *ptr = in_;
	for (int i = 0; i < count; i++)
	{
		_sisdb_collect_update_alone(collect_, ptr, ispub_);
		ptr += collect_->value->len;
	}
	// 处理记录个数

	s_sisdb_table *tb = collect_->db;
	if (tb->control.limits > 0)
	{
		sis_struct_list_limit(collect_->value, tb->control.limits);
	}
	// 重建索引
	sisdb_stepindex_rebuild(collect_->stepinfo,
							sisdb_collect_get_time(collect_, 0),
							sisdb_collect_get_time(collect_, collect_->value->count - 1),
							collect_->value->count);

	return count;
}
////////////////


// 从磁盘中整块写入，不逐条进行校验
int sisdb_collect_update_block(s_sisdb_collect *collect_, const char *in_, size_t ilen_)
{
	if (ilen_ < 1)
	{
		return 0;
	}
	int count = 0;

	count = (int)(ilen_ / collect_->value->len);
	//这里应该判断数据完整性
	if (count * collect_->value->len != ilen_)
	{
		LOG(3)("source format error [%d*%d!=%lu]\n", count, collect_->value->len, ilen_);
		return 0;
	}
	sis_struct_list_pushs(collect_->value, in_, count);
	// printf("-[%s]----count =%d len=%ld:%d\n", collect_->father->name, count, ilen_, collect_->value->len);
	// for (int i = 0; i < count; i++)
	// {
	// 	sis_struct_list_push(collect_->value, (void *)(in_ + i * collect_->value->len));
	// }

	sisdb_stepindex_rebuild(collect_->stepinfo,
							sisdb_collect_get_time(collect_, 0),
							sisdb_collect_get_time(collect_, collect_->value->count - 1),
							collect_->value->count);
	return count;
}
