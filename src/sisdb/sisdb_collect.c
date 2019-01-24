

#include <sisdb_collect.h>
#include <sisdb_fields.h>
#include <sisdb_map.h>
#include <sisdb_sys.h>
#include <sisdb_io.h>
#include <sisdb_file.h>
///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_step_index --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sis_step_index *sisdb_stepindex_create()
{
	s_sis_step_index *si = sis_malloc(sizeof(s_sis_step_index));
	memset(si, 0, sizeof(s_sis_step_index));
	sisdb_stepindex_clear(si);

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
int _stepindex_goto(s_sis_step_index *si_, uint64 curr_)
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
s_sisdb_collect *sisdb_collect_create(s_sis_db *db_, const char *key_)
{
	char code[SIS_MAXLEN_CODE];
	char dbname[SIS_MAXLEN_TABLE];
	sis_str_substr(code, SIS_MAXLEN_CODE, key_, '.', 0);
	sis_str_substr(dbname, SIS_MAXLEN_TABLE, key_, '.', 1);

	s_sisdb_table *tb = sisdb_get_table(db_, dbname);
	if (!tb)
	{
		sis_out_log(3)("table define no find.\n");
		return NULL;
	}

	s_sisdb_collect *unit = sis_malloc(sizeof(s_sisdb_collect));
	memset(unit, 0, sizeof(s_sisdb_collect));
	sis_map_buffer_set(db_->collects, key_, unit);

	unit->db = tb;
	if (tb->control.issys)
	{
		sis_string_list_push_only(tb->collect_list, code, strlen(code));
	}
	// if (db_->special) // 专用表
	{
		// 返回的指针，实体存放在 sys_exchs 中，
		unit->spec_info = sisdb_sys_create_info(db_, code);
		unit->spec_exch = sisdb_sys_create_exch(db_, code);
	}

	unit->stepinfo = sisdb_stepindex_create();
	// 仅仅在这里计算了记录长度，
	int size = sisdb_table_get_fields_size(tb);
	unit->value = sis_struct_list_create(size, NULL, 0);

	if (tb->control.issubs)
	{
		unit->front = sis_sdsnewlen(NULL, size);
		unit->lasted = sis_sdsnewlen(NULL, size);
	}
	if (tb->control.iszip)
	{
		unit->refer = sis_sdsnewlen(NULL, size);
	}

	return unit;
}

void sisdb_collect_destroy(s_sisdb_collect *unit_)
{
	// printf("delete in_collect = %p\n",unit_);
	if (unit_->value)
	{
		sis_struct_list_destroy(unit_->value);
	}
	if (unit_->stepinfo)
	{
		sisdb_stepindex_destroy(unit_->stepinfo);
	}
	if (unit_->db->control.issubs)
	{
		sis_sdsfree(unit_->front);
		sis_sdsfree(unit_->lasted);
	}
	if (unit_->db->control.iszip)
	{
		sis_sdsfree(unit_->refer);
	}
	sis_free(unit_);
}
void sisdb_collect_clear_subs(s_sisdb_collect *unit_)
{
	if (unit_->db->control.issubs)
	{
		memset(unit_->front, 0, sis_sdslen(unit_->front));
		memset(unit_->lasted, 0, sis_sdslen(unit_->lasted));
	}
}
void sisdb_collect_clear(s_sisdb_collect *unit_)
{
	sis_struct_list_clear(unit_->value);
	sisdb_stepindex_clear(unit_->stepinfo);
	sisdb_collect_clear_subs(unit_);
	if (unit_->db->control.iszip)
	{
		memset(unit_->lasted, 0, sis_sdslen(unit_->lasted));
	}
}
//获取股票的数据集合，
s_sisdb_collect *sisdb_get_collect(s_sis_db *db_, const char *key_)
{
	s_sisdb_collect *val = NULL;
	if (db_->collects)
	{
		s_sis_sds key = sis_sdsnew(key_);
		val = (s_sisdb_collect *)sis_dict_fetch_value(db_->collects, key);
		sis_sdsfree(key);
	}
	return val;
}

////////////////////////
//
////////////////////////
uint64 _sisdb_collect_get_time(s_sisdb_collect *unit_, int index_)
{
	uint64 tt = 0;
	void *val = sis_struct_list_get(unit_->value, index_);
	if (val)
	{
		tt = sisdb_field_get_uint_from_key(unit_->db, "time", val);
	}
	return tt;
}

int sisdb_collect_recs(s_sisdb_collect *unit_)
{
	if (!unit_ || !unit_->value)
	{
		return 0;
	}
	return unit_->value->count;
}

// 必须找到一个相等值，否则返回-1
int sisdb_collect_search(s_sisdb_collect *unit_, uint64 finder_)
{
	int index = _stepindex_goto(unit_->stepinfo, finder_);
	if (index < 0)
	{
		return -1;
	}
	int i = index;
	int dir = 0;
	while (i >= 0 && i < sisdb_collect_recs(unit_))
	{
		uint64 ts = _sisdb_collect_get_time(unit_, i);
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
int sisdb_collect_search_left(s_sisdb_collect *unit_, uint64 finder_, int *mode_)
{
	*mode_ = SIS_SEARCH_NONE;
	int index = _stepindex_goto(unit_->stepinfo, finder_);
	if (index < 0)
	{
		return -1;
	}
	int i = index;
	int dir = 0;
	int size = sisdb_collect_recs(unit_);
	while (i >= 0 && i < size)
	{
		uint64 ts = _sisdb_collect_get_time(unit_, i);
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
				ts = _sisdb_collect_get_time(unit_, k);
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
		return unit_->value->count - 1;
	}
	else
	{
		return -1;
	}
}
// 找到finder的位置，并返回index和相对位置（左还是右）没有记录设置 SIS_SEARCH_NONE
int sisdb_collect_search_right(s_sisdb_collect *unit_, uint64 finder_, int *mode_)
{
	*mode_ = SIS_SEARCH_NONE;
	int index = _stepindex_goto(unit_->stepinfo, finder_);
	if (index < 0)
	{
		return -1;
	}
	int i = index;
	int dir = 0;
	while (i >= 0 && i < sisdb_collect_recs(unit_))
	{
		uint64 ts = _sisdb_collect_get_time(unit_, i);
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
				ts = _sisdb_collect_get_time(unit_, k);
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
uint64 _sisdb_collect_get_time_fast(s_sisdb_collect *unit_, int index_, s_sisdb_field *fu_)
{
	uint64 tt = 0;
	void *val = sis_struct_list_get(unit_->value, index_);
	if (val)
	{
		tt = sisdb_field_get_uint(fu_, val);
	}
	return tt;
}

// 最后一个匹配的时间
// 12355579  查5返回5，查4返回3

int sisdb_collect_search_last(s_sisdb_collect *unit_, uint64 finder_, int *mode_)
{
	*mode_ = SIS_SEARCH_NONE;
	int index = _stepindex_goto(unit_->stepinfo, finder_);
	if (index < 0)
	{
		return -1;
	}
	int i = index;
	int dir = 0;

	s_sisdb_field *tfield = sisdb_field_get_from_key(unit_->db, "time");
	int count = sisdb_collect_recs(unit_);
	while (i >= 0 && i < count)
	{
		uint64 ts = _sisdb_collect_get_time_fast(unit_, i, tfield);
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

bool _sisdb_trans_of_range(s_sisdb_collect *unit_, int *start_, int *stop_)
{
	int llen = sisdb_collect_recs(unit_);

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
bool _sisdb_trans_of_count(s_sisdb_collect *unit_, int *start_, int *count_)
{
	if (*count_ == 0)
	{
		return false;
	}
	int llen = sisdb_collect_recs(unit_);

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

s_sis_sds sisdb_collect_get_of_range_sds(s_sisdb_collect *unit_, int start_, int stop_)
{
	bool o = _sisdb_trans_of_range(unit_, &start_, &stop_);
	if (!o)
	{
		return NULL;
	}
	int count = (stop_ - start_) + 1;
	return sis_sdsnewlen(sis_struct_list_get(unit_->value, start_), count * unit_->value->len);
}
s_sis_sds sisdb_collect_get_of_count_sds(s_sisdb_collect *unit_, int start_, int count_)
{
	bool o = _sisdb_trans_of_count(unit_, &start_, &count_);
	if (!o)
	{
		return NULL;
	}
	return sis_sdsnewlen(sis_struct_list_get(unit_->value, start_), count_ * unit_->value->len);
}
s_sis_sds sisdb_collect_get_last_sds(s_sisdb_collect *unit_)
{
	return sis_sdsnewlen(sis_struct_list_get(unit_->value, unit_->value->count - 1), unit_->value->len);
}

////////////////////////////////////////////
// format trans
///////////////////////////////////////////
s_sis_sds sisdb_collect_struct_filter_sds(s_sisdb_collect *unit_, s_sis_sds in_, s_sis_string_list *fields_)
{
	// 一定不是全字段
	s_sis_sds o = NULL;

	int count = (int)(sis_sdslen(in_) / unit_->value->len);
	char *val = in_;
	for (int k = 0; k < count; k++)
	{
		for (int i = 0; i < sis_string_list_getsize(fields_); i++)
		{
			const char *key = sis_string_list_get(fields_, i);
			s_sisdb_field *fu = sisdb_field_get_from_key(unit_->db, key);
			if (!fu)
			{
				continue;
			}
			if (!o)
			{
				o = sis_sdsnewlen(val + fu->offset, fu->flags.len);
			}
			else
			{
				o = sis_sdscatlen(o, val + fu->offset, fu->flags.len);
			}
		}
		val += unit_->value->len;
	}
	sis_string_list_destroy(fields_);
	return o;
}
s_sis_json_node *_sis_struct_to_array(s_sisdb_collect *unit_, s_sis_sds val_, s_sis_string_list *fields_)
{
	s_sis_json_node *o = sis_json_create_array();
	for (int i = 0; i < sis_string_list_getsize(fields_); i++)
	{
		const char *key = sis_string_list_get(fields_, i);
		// printf("----2----fields=%s\n",key);
		s_sisdb_field *fu = sisdb_field_get_from_key(unit_->db, key);
		if (!fu)
		{
			sis_json_array_add_string(o, " ", 1);
			continue;
		}
		const char *ptr = (const char *)val_;
		switch (fu->flags.type)
		{
		case SIS_FIELD_TYPE_CHAR:
			sis_json_array_add_string(o, ptr + fu->offset, fu->flags.len);
			break;
		case SIS_FIELD_TYPE_INT:
			// printf("ptr= %p, name=%s offset=%d\n", ptr, key, fu->offset);
			sis_json_array_add_int(o, sisdb_field_get_int(fu, ptr));
			break;
		case SIS_FIELD_TYPE_UINT:
		case SIS_FIELD_TYPE_VOLUME:
		case SIS_FIELD_TYPE_AMOUNT:
		case SIS_FIELD_TYPE_MSEC:
		case SIS_FIELD_TYPE_SECOND:
		case SIS_FIELD_TYPE_DATE:
			sis_json_array_add_uint(o, sisdb_field_get_uint(fu, ptr));
			break;
		case SIS_FIELD_TYPE_FLOAT:
			sis_json_array_add_double(o, sisdb_field_get_float(fu, ptr), fu->flags.dot);
			break;
		case SIS_FIELD_TYPE_PRICE:
			// 通用数据表没有该类型
			sis_json_array_add_double(o, sisdb_field_get_price(fu, ptr, unit_->spec_info->dot), unit_->spec_info->dot);
			break;
		default:
			sis_json_array_add_string(o, " ", 1);
			break;
		}
	}
	return o;
}

s_sis_sds sisdb_collect_struct_to_json_sds(s_sisdb_collect *unit_, s_sis_sds in_,
										   s_sis_string_list *fields_, bool zip_)
{
	s_sis_sds o = NULL;

	char *str;
	s_sis_json_node *jone = sis_json_create_object();
	s_sis_json_node *jtwo = sis_json_create_array();
	s_sis_json_node *jval = NULL;
	s_sis_json_node *jfields = sis_json_create_object();

	// 先处理字段
	for (int i = 0; i < sis_string_list_getsize(fields_); i++)
	{
		const char *key = sis_string_list_get(fields_, i);
		sis_json_object_add_uint(jfields, key, i);
	}
	sis_json_object_add_node(jone, SIS_JSON_KEY_FIELDS, jfields);

	int count = (int)(sis_sdslen(in_) / unit_->value->len);
	char *val = in_;
	for (int k = 0; k < count; k++)
	{
		sis_out_binary("get", val, sis_sdslen(in_));
		jval = _sis_struct_to_array(unit_, val, fields_);
		if (unit_->db->control.limits != 1)
		{
			sis_json_array_add_node(jtwo, jval);
		}
		else
		{
			break;
		}
		val += unit_->value->len;
	}

	if (unit_->db->control.limits != 1)
	{
		sis_json_object_add_node(jone, SIS_JSON_KEY_ARRAYS, jtwo);
	}
	else
	{
		sis_json_object_add_node(jone, SIS_JSON_KEY_ARRAY, jval);
	}
	// size_t ll;
	// printf("jone = %s\n", sis_json_output(jone, &ll));
	// 输出数据
	// printf("1112111 [%d]\n",tb->control.limits);

	size_t olen;
	if (zip_)
	{
		str = sis_json_output_zip(jone, &olen);
	}
	else
	{
		str = sis_json_output(jone, &olen);
	}
	o = sis_sdsnewlen(str, olen);
	sis_free(str);
	sis_json_delete_node(jone);

	return o;
}

s_sis_sds sisdb_collect_struct_to_array_sds(s_sisdb_collect *unit_, s_sis_sds in_,
											s_sis_string_list *fields_, bool zip_)
{
	s_sis_sds o = NULL;

	char *str;
	s_sis_json_node *jone = sis_json_create_array();
	s_sis_json_node *jval = NULL;

	int count = (int)(sis_sdslen(in_) / unit_->value->len);
	char *val = in_;
	for (int k = 0; k < count; k++)
	{
		sis_out_binary("get", val, sis_sdslen(in_));
		jval = _sis_struct_to_array(unit_, val, fields_);
		sis_json_array_add_node(jone, jval);
		if (unit_->db->control.limits == 1)
		{
			break;
		}
		val += unit_->value->len;
	}
	size_t olen;

	if (unit_->db->control.limits != 1)
	{
		// printf("jone = %s\n", sis_json_output(jone, &olen));
		if (zip_)
		{
			str = sis_json_output_zip(jone, &olen);
		}
		else
		{
			str = sis_json_output(jone, &olen);
		}
	}
	else
	{
		if (zip_)
		{
			str = sis_json_output_zip(jval, &olen);
		}
		else
		{
			str = sis_json_output(jval, &olen);
		}
		// printf("jval = %s\n", sis_json_output(jval, &olen));
	}
	// 输出数据
	// printf("1112111 [%d]\n",unit_->db->control.limits);
	o = sis_sdsnewlen(str, olen);
	sis_free(str);
	sis_json_delete_node(jone);

	return o;
}
s_sis_sds _sis_struct_to_csv(s_sis_sds str_, s_sisdb_collect *unit_, s_sis_sds val_, s_sis_string_list *fields_)
{
	int size = sis_string_list_getsize(fields_);
	for (int i = 0; i < size; i++)
	{
		const char *key = sis_string_list_get(fields_, i);
		s_sisdb_field *fu = sisdb_field_get_from_key(unit_->db, key);
		if (!fu)
		{
			if (i < size - 1)
			{
				str_ = sis_sdscat(str_, ",");
			}
			continue;
		}
		const char *ptr = (const char *)val_;
		char val[SIS_MAXLEN_STRING];
		int len = 0;
		switch (fu->flags.type)
		{
		case SIS_FIELD_TYPE_CHAR:
			len = sis_min(fu->flags.len, strlen(ptr + fu->offset));
			if (fu->flags.len == 1)
			{
				val[0] = *(ptr + fu->offset);
				val[1] = 0;
			}
			else
			{
				// 这里应判断如果字符串中有引号该怎么处理，标准处理方式是两个引号即可
				sis_sprintf(val, SIS_MAXLEN_STRING, "\"%*s\"", len, ptr + fu->offset);
			}
			break;
		case SIS_FIELD_TYPE_INT:
			sis_sprintf(val, SIS_MAXLEN_STRING, "%d", (int32)sisdb_field_get_int(fu, ptr));
			break;
		case SIS_FIELD_TYPE_MSEC:
			sis_sprintf(val, SIS_MAXLEN_STRING, "%lld", sisdb_field_get_uint(fu, ptr));
			break;
		case SIS_FIELD_TYPE_UINT:
		case SIS_FIELD_TYPE_VOLUME:
		case SIS_FIELD_TYPE_AMOUNT:
		case SIS_FIELD_TYPE_SECOND:
		case SIS_FIELD_TYPE_DATE:
			sis_sprintf(val, SIS_MAXLEN_STRING, "%d", (uint32)sisdb_field_get_uint(fu, ptr));
			break;
		case SIS_FIELD_TYPE_FLOAT:
			sis_sprintf(val, SIS_MAXLEN_STRING, "%.*f", fu->flags.dot, sisdb_field_get_float(fu, ptr));
			break;
		case SIS_FIELD_TYPE_PRICE:
			sis_sprintf(val, SIS_MAXLEN_STRING, "%.*f", unit_->spec_info->dot, sisdb_field_get_price(fu, ptr, unit_->spec_info->dot));
			break;
		}
		str_ = sis_sdscatfmt(str_, "%s", val);
		if (i < size - 1)
		{
			str_ = sis_sdscat(str_, ",");
		}
	}
	return str_;
}
s_sis_sds sisdb_collect_struct_to_csv_sds(s_sisdb_collect *unit_, s_sis_sds in_,
										  s_sis_string_list *fields_, bool zip_)
{
	s_sis_sds o = sis_sdsempty();

	// 先处理字段
	for (int i = 0; i < sis_string_list_getsize(fields_); i++)
	{
		const char *key = sis_string_list_get(fields_, i);
		if (i < 1)
		{
			o = sis_sdscatfmt(o, "%s", key);
		}
		else
		{
			o = sis_sdscatfmt(o, ",%s", key);
		}
	}
	o = sis_sdscat(o, "\n");
	int count = (int)(sis_sdslen(in_) / unit_->value->len);
	char *val = in_;

	for (int k = 0; k < count; k++)
	{
		o = _sis_struct_to_csv(o, unit_, val, fields_);
		o = sis_sdscat(o, "\n");
		// printf("--- [%d] =%s", k, o);
		val += unit_->value->len;
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
s_sis_sds sisdb_collect_get_original_sds(s_sisdb_collect *collect, s_sis_json_handle *handle)
{
	// 检查取值范围，没有就全部取
	s_sis_sds o = NULL;

	int64 min, max;
	int start, stop;
	int count = 0;
	int maxX, minX;
	int offset = 0;

	s_sis_json_node *search = sis_json_cmp_child_node(handle->node, "search");
	if (!search)
	{
		o = sisdb_collect_get_of_range_sds(collect, 0, -1);
		return o;
	}
	bool by_time = sis_json_cmp_child_node(search, "min") != NULL;
	bool by_region = sis_json_cmp_child_node(search, "start") != NULL;
	if (by_time)
	{
		min = sis_json_get_int(search, "min", 0);
		offset = sis_json_get_int(search, "offset", 0);
		// if (sis_json_cmp_child_node(search, "offset"))
		// {
		// 	offset = sis_json_get_int(search, "offset", 0);
		// }
		if (sis_json_cmp_child_node(search, "count"))
		{
			count = sis_json_get_int(search, "count", 1);
			start = sisdb_collect_search_right(collect, min, &minX);
			if (start >= 0)
			{
				start += offset;
				start = sis_max(0, start);
				if (offset < 0)
				{
					count -= offset;
				}
				// printf("---- %d %d  %d %d %d\n",start, collect->value->count, offset, min, minX);
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
					// if (offset < 0)
					// {
					// 	count -= offset;
					// }
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
				// start = sisdb_collect_search(collect, min);
				// if (start >= 0)
				// {
				// 	start += offset;
				// 	start = sis_max(0, start);
				// 	if (offset < 0)
				// 	{
				// 		count -= offset;
				// 	}
				// 	o = sisdb_collect_get_of_count_sds(collect, start, 1);
				// }
			}
		}
		return o;
	}
	if (by_region)
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
s_sis_sds sisdb_collect_fastget_sds(s_sis_db *db_, const char *key_)
{
	s_sisdb_table *tb = sisdb_get_table_from_key(db_, key_);
	if (!tb)
	{
		return NULL;
	}
	s_sisdb_collect *collect = sisdb_get_collect(db_, key_);
	if (!collect)
	{
		sis_out_log(3)("no find %s key.\n", key_);
		return NULL;
	}
	int start = 0;
	int count = collect->value->count;

	if (count * collect->value->len > SISDB_MAXLEN_RETURN)
	{
		count = SISDB_MAXLEN_RETURN / collect->value->len;
		start = -1 * count;
	}
	// printf("%d,%d\n",start,count);
	s_sis_sds out = sisdb_collect_get_of_count_sds(collect, start, count);

	if (!out)
	{
		return NULL;
	}
	// 最后转数据格式
	s_sis_sds other = sisdb_collect_struct_to_json_sds(collect, out, tb->field_name, true);

	sis_sdsfree(out);
	return other;
}

s_sis_sds sisdb_collect_get_sds(s_sis_db *db_, const char *key_, const char *com_)
{
	s_sisdb_table *tb = sisdb_get_table_from_key(db_, key_);
	if (!tb)
	{
		return NULL;
	}
	s_sisdb_collect *collect = sisdb_get_collect(db_, key_);
	if (!collect)
	{
		sis_out_log(3)("no find %s key.\n", key_);
		return NULL;
	}
	s_sis_json_handle *handle = sis_json_load(com_, strlen(com_));
	if (!handle)
	{
		return NULL;
	}
	s_sis_sds out = sisdb_collect_get_original_sds(collect, handle);

	if (!out)
	{
		sis_json_close(handle);
		return NULL;
	}
	// 最后转数据格式
	// 取出数据返回格式，没有就默认为二进制结构数据
	// int iformat = SIS_DATA_TYPE_STRUCT;
	int iformat = sis_from_node_get_format(db_, handle->node);

	// printf("iformat = %c\n", iformat);
	// 取出字段定义，没有就默认全部字段

	s_sis_sds fields = NULL;
	if (sis_json_cmp_child_node(handle->node, "fields"))
	{
		fields = sis_sdsnew(sis_json_get_str(handle->node, "fields"));
	}
	else
	{
		fields = sis_sdsnew("*");
	}
	// printf("query fields = %s\n", fields);

	s_sis_string_list *field_list = tb->field_name; //取得全部的字段定义
	if (!sisdb_field_is_whole(fields))
	{
		field_list = sis_string_list_create_w();
		sis_string_list_load(field_list, fields, strlen(fields), ",");
	}
	printf("out: %d\n", sisdb_get_server()->switch_output);
	// 判断是否写盘
	if (sisdb_get_server()->switch_output)
	{

		sisdb_file_get_outdisk(key_, iformat, out);
	}

	s_sis_sds other = NULL;
	switch (iformat)
	{
	case SIS_DATA_TYPE_STRUCT:
		if (!sisdb_field_is_whole(fields))
		{
			other = sisdb_collect_struct_filter_sds(collect, out, field_list);
			sis_sdsfree(out);
			out = other;
		}
		break;
	case SIS_DATA_TYPE_JSON:
		other = sisdb_collect_struct_to_json_sds(collect, out, field_list, true);
		sis_sdsfree(out);
		out = other;
		break;
	case SIS_DATA_TYPE_ARRAY:
		other = sisdb_collect_struct_to_array_sds(collect, out, field_list, true);
		sis_sdsfree(out);
		out = other;
		break;
	}
	if (!sisdb_field_is_whole(fields))
	{
		sis_string_list_destroy(field_list);
	}
	if (fields)
	{
		sis_sdsfree(fields);
	}
	sis_json_close(handle);

	return out;
}

// ///////////////////////////////////////////////////////////////////////////////
// //取数据,读表中代码为key的数据，key为*表示所有股票数据，由 com_ 定义数据范围和字段范围
// ///////////////////////////////////////////////////////////////////////////////

s_sis_json_node *sisdb_collect_groups_json_init(s_sis_string_list *fields_)
{
	s_sis_json_node *jone = sis_json_create_object();
	s_sis_json_node *jtwo = sis_json_create_object();
	s_sis_json_node *jfields = sis_json_create_object();
	// 先处理字段
	for (int i = 0; i < sis_string_list_getsize(fields_); i++)
	{
		const char *key = sis_string_list_get(fields_, i);
		// printf("----0----tb->field_name=%s\n",sis_string_list_get(tb->field_name, i));
		// printf("----1----fields=%s\n",key);
		sis_json_object_add_uint(jfields, key, i);
	}
	sis_json_object_add_node(jone, SIS_JSON_KEY_FIELDS, jfields);
	sis_json_object_add_node(jone, SIS_JSON_KEY_GROUPS, jtwo);

	return jone;
}
void sisdb_collect_groups_json_push(s_sis_json_node *node_, char *code, s_sisdb_collect *unit_, s_sis_sds in_, s_sis_string_list *fields_)
{
	s_sis_json_node *val = _sis_struct_to_array(unit_, in_, fields_);
	s_sis_json_node *groups = sis_json_cmp_child_node(node_, SIS_JSON_KEY_GROUPS);
	// printf("groups[%p]: key=%s\n ", groups,code);
	sis_json_object_add_node(groups, code, val);
}
s_sis_sds sisdb_collect_groups_json_sds(s_sis_json_node *node_)
{
	if (!node_)
		return NULL;
	size_t olen;
	char *str = sis_json_output_zip(node_, &olen);
	s_sis_sds o = sis_sdsnewlen(str, olen);
	sis_free(str);
	return o;
}

s_sis_sds sisdb_collects_get_last_sds(s_sis_db *db_, const char *dbname_, const char *com_)
{
	s_sisdb_table *tb = sisdb_get_table(db_, dbname_);
	if (!tb)
	{
		return NULL;
	}

	s_sis_json_handle *handle = sis_json_load(com_, strlen(com_));
	// printf("com_ = %s -- %lu -- %p\n", com_,strlen(com_),handle);
	if (!handle)
	{
		return NULL;
	}

	// s_sis_string_list *codes = sis_string_list_create_w();
	const char *codes = sis_json_get_str(handle->node, "codes");
	// if (!codes)
	// {
	// 	sis_out_log(3)("no find codes [%s].\n", com_);
	// 	goto error;
	// }
	// sis_string_list_load(codes, codes, strlen(codes), ",");

	// int count = sis_string_list_getsize(codes);
	// if (count < 1)
	// {
	// 	sis_out_log(3)("no find codes [%s].\n", com_);
	// 	goto error;
	// }

	int iformat = sis_from_node_get_format(db_, handle->node);

	printf("iformat = %c\n", iformat);
	// 取出字段定义，没有就默认全部字段
	s_sis_sds fields = NULL;
	if (sis_json_cmp_child_node(handle->node, "fields"))
	{
		fields = sis_sdsnew(sis_json_get_str(handle->node, "fields"));
	}
	else
	{
		fields = sis_sdsnew("*");
	}
	printf("query fields = %s\n", fields);

	s_sis_string_list *field_list = tb->field_name; //取得全部的字段定义
	if (!sisdb_field_is_whole(fields))
	{
		field_list = sis_string_list_create_w();
		sis_string_list_load(field_list, fields, strlen(fields), ",");
	}

	s_sis_sds out = NULL;
	s_sis_sds other = NULL;
	s_sis_json_node *node = NULL;
	// 只取最后一条记录
	char code[SIS_MAXLEN_CODE];
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(db_->collects);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
		if (sis_strcasecmp(collect->db->name, dbname_))
			continue;

		sis_str_substr(code, SIS_MAXLEN_CODE, sis_dict_getkey(de), '.', 0);
		if (codes)
		{
			// 如果定义了代码，就检查一下代码是否存在
			if (sis_str_subcmp(code, codes, ',') < 0)
			{
				continue;
			}
		}

		// printf("collect = %s db = %s %s\n", (char *)sis_dict_getkey(de), collect->db->name, dbname_);
		s_sis_sds val = sisdb_collect_get_of_count_sds(collect, -1, 1);

		// printf("out = %lu -- %lu\n", sis_sdslen(out),sis_sdslen(val));
		switch (iformat)
		{
		case SIS_DATA_TYPE_STRUCT:
			if (!out)
			{
				out = sis_sdsnewlen(code, SIS_MAXLEN_CODE);
			}
			else
			{
				out = sis_sdscatlen(out, code, SIS_MAXLEN_CODE);
			}
			if (!sisdb_field_is_whole(fields))
			{
				other = sisdb_collect_struct_filter_sds(collect, val, field_list);
				out = sis_sdscatlen(out, other, sis_sdslen(other));
				sis_sdsfree(other);
			}
			else
			{
				out = sis_sdscatlen(out, val, sis_sdslen(val));
			}
			break;
		case SIS_DATA_TYPE_JSON:
		default:
			if (!node)
			{
				node = sisdb_collect_groups_json_init(field_list);
			}
			sisdb_collect_groups_json_push(node, code, collect, val, field_list);
			break;
		}
		sis_sdsfree(val);
	}
	sis_dict_iter_free(di);

	if (iformat != SIS_DATA_TYPE_STRUCT)
	{
		out = sisdb_collect_groups_json_sds(node);
		sis_json_delete_node(node);
	}
	// 下面需要重写----因为比collect多出来个代码，所以只能用JSON和STRUCT结构来输出，这里会用到 groups 关键字
	// json中注意字段定义只需要一个，然后就是
	// fields :{}
	// groups :
	// {
	// 	sh600600:[],
	// 	sh600600:[],
	// 	sh600600:[]
	// }
	if (!sisdb_field_is_whole(fields))
	{
		sis_string_list_destroy(field_list);
	}
	if (fields)
	{
		sis_sdsfree(fields);
	}
	sis_json_close(handle);
	return out;
}

//////////////////////////////////////////////////
//  delete
//////////////////////////////////////////////////
int _sisdb_collect_delete(s_sisdb_collect *unit_, int start_, int count_)
{
	sis_struct_list_delete(unit_->value, start_, count_);
	sisdb_stepindex_rebuild(unit_->stepinfo,
							_sisdb_collect_get_time(unit_, 0),
							_sisdb_collect_get_time(unit_, unit_->value->count - 1),
							unit_->value->count);
	return 0;
}
int sisdb_collect_delete_of_range(s_sisdb_collect *unit_, int start_, int stop_)
{
	bool o = _sisdb_trans_of_range(unit_, &start_, &stop_);
	if (!o)
	{
		return 0;
	}
	int count = (stop_ - start_) + 1;
	return _sisdb_collect_delete(unit_, start_, count);
}

int sisdb_collect_delete_of_count(s_sisdb_collect *unit_, int start_, int count_)
{
	bool o = _sisdb_trans_of_count(unit_, &start_, &count_);
	if (!o)
	{
		return 0;
	}
	return _sisdb_collect_delete(unit_, start_, count_);
}

int sisdb_collect_delete(s_sis_db *db_, const char *key_, const char *com_)
{
	s_sisdb_collect *collect = sisdb_get_collect(db_, key_);
	if (!collect)
	{
		sis_out_log(3)("no find %s key.\n", key_);
		return 0;
	}
	s_sis_json_handle *handle = sis_json_load(com_, strlen(com_));
	if (!handle)
	{
		return 0;
	}

	uint64 min, max;
	int start, stop;
	int count = 0;
	int out = 0;
	int minX, maxX;

	s_sis_json_node *search = sis_json_cmp_child_node(handle->node, "search");
	if (!search)
	{
		goto exit;
	}
	bool by_time = sis_json_cmp_child_node(search, "min") != NULL;
	bool by_region = sis_json_cmp_child_node(search, "start") != NULL;

	if (by_time)
	{
		min = sis_json_get_int(search, "min", 0);
		if (sis_json_cmp_child_node(search, "count"))
		{
			count = sis_json_get_int(search, "count", 1);
			start = sisdb_collect_search(collect, min);
			if (start >= 0)
			{
				out = sisdb_collect_delete_of_count(collect, start, count);
			}
		}
		else
		{
			if (sis_json_cmp_child_node(search, "max"))
			{
				max = sis_json_get_int(search, "max", 0);
				start = sisdb_collect_search_right(collect, min, &minX);
				stop = sisdb_collect_search_left(collect, max, &maxX);
				if (minX != SIS_SEARCH_NONE && maxX != SIS_SEARCH_NONE)
				{
					out = sisdb_collect_delete_of_range(collect, start, stop);
				}
			}
			else
			{
				start = sisdb_collect_search(collect, min);
				if (start >= 0)
				{
					out = sisdb_collect_delete_of_count(collect, start, 1);
				}
			}
		}
		goto exit;
	}
	if (by_region)
	{
		start = sis_json_get_int(search, "start", -1); // -1 为最新一条记录
		if (sis_json_cmp_child_node(search, "count"))
		{
			count = sis_json_get_int(search, "count", 1);
			out = sisdb_collect_delete_of_count(collect, start, count); // 定位后按数量删除
		}
		else
		{
			if (sis_json_cmp_child_node(search, "stop"))
			{
				stop = sis_json_get_int(search, "stop", -1);			   // -1 为最新一条记录
				out = sisdb_collect_delete_of_range(collect, start, stop); // 定位后删除
			}
			else
			{
				out = sisdb_collect_delete_of_count(collect, start, 1); // 定位后按数量删除
			}
		}
		goto exit;
	}
exit:
	sis_json_close(handle);
	return out;
}

////////////////////////
//  update
////////////////////////

s_sis_sds sisdb_collect_json_to_struct_sds(s_sisdb_collect *unit_, const char *in_, size_t ilen_)
{
	// 取最后一条记录的数据
	const char *src = sis_struct_list_last(unit_->value);
	s_sis_sds o = sis_sdsnewlen(src, unit_->value->len);

	s_sis_json_handle *handle = sis_json_load(in_, ilen_);
	if (!handle)
	{
		return NULL;
	}

	s_sisdb_table *tb = unit_->db;

	int fields = sis_string_list_getsize(tb->field_name);

	for (int k = 0; k < fields; k++)
	{
		const char *key = sis_string_list_get(tb->field_name, k);
		s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, key);
		if (!fu)
		{
			continue;
		}
		// printf("key=%s i=%d offset=%d\n", key, k, fu->offset);
		int dot = unit_->db->father->special ? unit_->spec_info->dot : fu->flags.dot;
		sisdb_field_json_to_struct(o, fu, (char *)key, handle->node, dot);
	}

	sis_json_close(handle);
	return o;
}

s_sis_sds sisdb_collect_array_to_struct_sds(s_sisdb_collect *unit_, const char *in_, size_t ilen_)
{
	// 字段个数一定要一样
	s_sis_json_handle *handle = sis_json_load(in_, ilen_);
	if (!handle)
	{
		return NULL;
	}
	s_sisdb_table *tb = unit_->db;
	// 获取字段个数
	int fields = sis_string_list_getsize(tb->field_name);
	s_sis_json_node *jval;

	int count = 0;
	if (handle->node->child->type == SIS_JSON_ARRAY)
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

	s_sis_sds o = sis_sdsnewlen(NULL, count * unit_->value->len);
	int index = 0;
	while (jval)
	{
		int size = sis_json_get_size(jval);
		if (size != fields)
		{
			sis_out_log(3)("input fields[%d] count error [%d].\n", size, fields);
			jval = jval->next;
			continue;
		}

		for (int k = 0; k < fields; k++)
		{
			const char *fname = sis_string_list_get(tb->field_name, k);
			s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, fname);
			// printf("key=%s size=%d offset=%d\n", fname, fields, fu->offset);
			if (!fu)
			{
				continue;
			}
			char key[16];
			sis_sprintf(key, 10, "%d", k);
			int dot = unit_->db->father->special ? unit_->spec_info->dot : fu->flags.dot;
			sisdb_field_json_to_struct(o + index * unit_->value->len, fu, key, jval, dot);
		}
		index++;
		jval = jval->next;
	}

	sis_json_close(handle);
	return o;
}
// //检查是否增加记录，只和最后一条记录做比较，返回4个，一是当日最新记录，一是新记录，一是老记录, 一个为当前记录
// int _sisdb_collect_check_lasttime(s_sisdb_collect *unit_, uint64 finder_)
// {
// 	if (unit_->value->count < 1)
// 	{
// 		return SIS_CHECK_LASTTIME_INIT;
// 	}

// 	uint64 ts = _sisdb_collect_get_time(unit_, unit_->value->count - 1);
// 	if (finder_  ==  ts)
// 	{
// 		return SIS_CHECK_LASTTIME_OK;
// 	}
// 	else if (finder_ > ts)
// 	{
// 		if (sis_time_get_idate(finder_) > sis_time_get_idate(ts))
// 		{
// 			return SIS_CHECK_LASTTIME_INIT;
// 		}
// 		else
// 		{
// 			return SIS_CHECK_LASTTIME_NEW;
// 		}
// 	}
// 	else // if (finder_ < ts)
// 	{
// 		// 应该判断量，如果量小就返回错误
// 		return SIS_CHECK_LASTTIME_OLD;
// 	}
// }

s_sis_sds _sisdb_make_catch_inited_sds(s_sisdb_collect *unit_, const char *in_)
{
	s_sisdb_table *tb = unit_->db;
	s_sis_sds o = sis_sdsnewlen(in_, sis_sdslen(unit_->front));
	int fields = sis_string_list_getsize(tb->field_name);

	s_sis_collect_method_buffer obj;
	obj.init = true;
	obj.in = in_;
	obj.out = o;
	obj.collect = unit_;

	for (int k = 0; k < fields; k++)
	{
		const char *key = sis_string_list_get(tb->field_name, k);
		s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, key);
		if (!fu || !fu->subscribe_method)
			continue;
		if (!sisdb_field_is_integer(fu))
			continue;

		obj.field = fu;
		fu->subscribe_method->obj = &obj;
		sis_method_class_execute(fu->subscribe_method);
	}
	// printf("o=%lld\n",sisdb_field_get_uint_from_key(tb,"time",o));
	// printf("in_=%lld\n",sisdb_field_get_uint_from_key(tb,"time",in_));
	return o;
}
s_sis_sds sisdb_make_catch_moved_sds(s_sisdb_collect *unit_, const char *in_)
{
	s_sisdb_table *tb = unit_->db;
	// 获取最后一条记录
	s_sis_sds moved = sis_struct_list_last(unit_->value);

	// 保存源数据
	s_sis_sds o = sis_sdsnewlen(in_, sis_sdslen(unit_->front));
	int fields = sis_string_list_getsize(tb->field_name);

	s_sis_collect_method_buffer obj;
	obj.init = false;
	obj.in = in_;
	obj.last = moved;
	obj.out = o;
	obj.collect = unit_;

	for (int k = 0; k < fields; k++)
	{
		const char *key = sis_string_list_get(tb->field_name, k);
		s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, key);

		if (!fu || !fu->subscribe_method)
			continue;
		if (!sisdb_field_is_integer(fu))
			continue;

		obj.field = fu;
		fu->subscribe_method->obj = &obj;
		sis_method_class_execute(fu->subscribe_method);
	}
	return o;
}

int _sisdb_collect_update_alone_check_solely(s_sisdb_collect *unit_, const char *in_)
{
	s_sisdb_table *tb = unit_->db;
	// 采用直接内存比较

	for (int i = 0; i < unit_->value->count; i++)
	{
		const char *val = sis_struct_list_get(unit_->value, i);
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
			sis_struct_list_update(unit_->value, i, (void *)in_);
			return 0;
		}
	}
	sis_struct_list_push(unit_->value, (void *)in_);
	return 1;
}

int _sisdb_collect_update_alone_check_solely_time(s_sisdb_collect *unit_, const char *in_, int index_, uint32 time_)
{
	s_sisdb_table *tb = unit_->db;
	// 采用直接内存比较
	s_sisdb_field *field = sisdb_field_get_from_key(tb, "time");
	for (int i = index_; i >= 0; i--)
	{
		const char *val = sis_struct_list_get(unit_->value, i);
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
			sis_struct_list_update(unit_->value, i, (void *)in_);
			return 0;
		}
	}
	if (index_ == unit_->value->count - 1)
	{
		sis_struct_list_push(unit_->value, (void *)in_);
	}
	else
	{
		sis_struct_list_insert(unit_->value, index_, (void *)in_);
	}
	return 1;
}
// 检查是否增加记录，只和最后一条记录做比较，返回4个，一是当日最新记录，一是新记录，一是老记录, 一个为当前记录
int _sisdb_collect_check_lasttime(s_sisdb_collect *unit_, uint64 finder_)
{
	if (unit_->value->count < 1)
	{
		return SIS_CHECK_LASTTIME_INIT;
	}
	s_sisdb_table *tb = unit_->db;

	s_sisdb_field *field = sisdb_field_get_from_key(tb, "time");
	const char *last = sis_struct_list_last(unit_->value);
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
void _sisdb_collect_check_lastdate(s_sisdb_collect *unit_, const char *in_)
{
	s_sisdb_table *tb = unit_->db;
	s_sisdb_field *field = sisdb_field_get_from_key(tb, "time");
	if (!field)
		return;

	uint64 finder_ = sisdb_field_get_uint(field, in_); // 得到时间序列值
	const char *last = sis_struct_list_last(unit_->value);
	uint64 last_time = sisdb_field_get_uint(field, last); // 得到时间序列值

	if (field->flags.type == SIS_FIELD_TYPE_SECOND &&
		sis_time_get_idate(finder_) != sis_time_get_idate(last_time))
	{
		if (tb->control.isinit)
		{
			sisdb_collect_clear(unit_);
		}
		else if (tb->control.ispubs)
		{
			sisdb_collect_clear_subs(unit_);
		}
		// printf(">>> %s : [%d] %d %d %d %d\n", tb->name, unit_->value->count,
		// field->flags.type, tb->control.isinit, sis_time_get_idate(finder_),
		// sis_time_get_idate(last_time));
	}
	if (field->flags.type == SIS_FIELD_TYPE_UINT && tb->control.scale == SIS_TIME_SCALE_INCR &&
		finder_ < last_time)
	{
		if (tb->control.isinit)
		{
			sisdb_collect_clear(unit_);
		}
		else if (tb->control.ispubs)
		{
			sisdb_collect_clear_subs(unit_);
		}
		// printf(">>>>> %s : [%d] %d %d %d %d\n", tb->name, unit_->value->count,
		// field->flags.type, tb->control.isinit, finder_, last_time);
	}
}
int _sisdb_collect_update_alone(s_sisdb_collect *unit_, const char *in_, bool ispub_)
{
	s_sisdb_table *tb = unit_->db;
	tb->control.ispubs = (ispub_ & tb->control.issubs);
	if (tb->control.isinit || tb->control.ispubs)
	{
		_sisdb_collect_check_lastdate(unit_, in_);
	}
	if (tb->write_style == SIS_WRITE_ALWAYS)
	{
		// 不做任何检查直接写入
		sis_struct_list_push(unit_->value, (void *)in_);
		return 1;
	}
	// 先检查数据是否合法，合法就往下，不合法就直接返回
	if (tb->write_method)
	{
		s_sis_collect_method_buffer obj;
		obj.in = in_;
		obj.collect = unit_;
		tb->write_method->obj = &obj;

		bool *ok = sis_method_class_execute(tb->write_method);
		if (!*ok)
		{
			// s_sisdb_field *field = sisdb_field_get_from_key(tb, "time");
			// if (field)
			// {
			// 	uint64 in_time = sisdb_field_get_uint(field, in_); // 得到时间序列值
			// 	uint64 last_time = sisdb_field_get_uint(field, sis_struct_list_last(unit_->value)); // 得到时间序列值
			// 	uint64 sub_time = sisdb_field_get_uint(field, unit_->lasted);
			// 	printf("xxxx time %s %lld %lld %lld\n", tb->name, last_time, in_time, sub_time);
			// }
			// field = sisdb_field_get_from_key(tb, "vol");
			// if (field)
			// {
			// 	uint64 in_time= sisdb_field_get_uint(field, in_); // 得到时间序列值
			// 	uint64 last_time = sisdb_field_get_uint(field, sis_struct_list_last(unit_->value)); // 得到时间序列值
			// 	uint64 sub_time = sisdb_field_get_uint(field, unit_->lasted);
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
			sis_struct_list_push(unit_->value, (void *)in_);
			return 1;
		}
		else
		{
			return _sisdb_collect_update_alone_check_solely(unit_, in_);
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
	int search_mode = _sisdb_collect_check_lasttime(unit_, in_time);
	// if (search_mode==SIS_CHECK_LASTTIME_INIT)
	// printf("mode=%d tt= %lld  %d\n", search_mode, in_time, sis_time_get_idate(in_time));
	// sis_out_binary("update", in_, size);

	// printf("----=%d tt= %lld index=%d\n", mode, tt, index);
	if (search_mode == SIS_CHECK_LASTTIME_NEW)
	{
		if (tb->control.ispubs)
		{
			if (memcmp(unit_->lasted + offset, in_ + offset, unit_->value->len - offset))
			{
				sisdb_field_copy(unit_->front, unit_->lasted, unit_->value->len);
				// -- 先根据in -- tb->front生成新数据
				s_sis_sds in = _sisdb_make_catch_inited_sds(unit_, in_);
				// -- 再覆盖老数据
				sis_struct_list_push(unit_->value, (void *)in);
				sisdb_field_copy(unit_->lasted, in_, unit_->value->len);
				sis_sdsfree(in);
			}
			else
			{
				return 0;
			}
		}
		else
		{
			sis_struct_list_push(unit_->value, (void *)in_);
		}
	}
	else if (search_mode == SIS_CHECK_LASTTIME_INIT)
	{
		// 1. 初始化
		if (tb->control.ispubs)
		{
			sisdb_field_copy(unit_->front, NULL, unit_->value->len); // 保存的是全量，成交量使用
			sisdb_field_copy(unit_->lasted, in_, unit_->value->len); //
		}
		// 2. 写入数据
		sis_struct_list_push(unit_->value, (void *)in_);
		// sis_out_binary("set push", in_, size);
	}
	else if (search_mode == SIS_CHECK_LASTTIME_OK)
	{
		// 如果发现记录，保存原始值到lasted，然后计算出实际要写入的值
		// 1. 初始化
		int index = unit_->value->count - 1;
		if (tb->control.ispubs)
		{
			// -- 先根据in -- tb->front生成新数据
			if (memcmp(unit_->lasted + offset, in_ + offset, unit_->value->len - offset))
			{
				s_sis_sds in = sisdb_make_catch_moved_sds(unit_, in_);
				// if (tb->write_style & SIS_WRITE_SOLE_TIME)
				// {
				sis_struct_list_update(unit_->value, index, (void *)in);
				// }
				// else
				// {
				// 	sis_struct_list_push(unit_->value, (void *)in_);
				// }
				sisdb_field_copy(unit_->lasted, in_, unit_->value->len);
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
				sis_struct_list_update(unit_->value, index, (void *)in_);
			}
			else if (tb->write_style & SIS_WRITE_SOLE_MULS || tb->write_style & SIS_WRITE_SOLE_OTHER)
			{
				_sisdb_collect_update_alone_check_solely_time(unit_, in_, index, in_time);
			}
			else
			{
				sis_struct_list_push(unit_->value, (void *)in_);
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
			int index = sisdb_collect_search_last(unit_, in_time, &set);
			if (set == SIS_SEARCH_OK)
			{
				printf("index =%d write_style= %d \n", index, (tb->write_style & SIS_WRITE_SOLE_TIME));
				if (tb->write_style & SIS_WRITE_SOLE_TIME)
				{
					sis_struct_list_update(unit_->value, index, (void *)in_);
				}
				else if (tb->write_style & SIS_WRITE_SOLE_MULS || tb->write_style & SIS_WRITE_SOLE_OTHER)
				{
					_sisdb_collect_update_alone_check_solely_time(unit_, in_, index, in_time);
				}
				else
				{
					sis_struct_list_insert(unit_->value, index + 1, (void *)in_);
				}
			}
			else
			{
				sis_struct_list_insert(unit_->value, index, (void *)in_);
			}
		}
		// printf("count=%d\n",unit_->value->count);
	}

	return 1;
}
///////////////////////

// int _sisdb_collect_update_alone(s_sisdb_collect *unit_, const char *in_)
// {
// 	s_sisdb_table *tb = unit_->db;

// 	if (tb->append_method == SIS_ADD_METHOD_ALWAYS)
// 	{
// 		sis_struct_list_push(unit_->value, (void *)in_);
// 		return 1;
// 	}
// 	if (tb->append_method & SIS_ADD_METHOD_VOL)
// 	{
// 		uint64 vol = sisdb_field_get_uint_from_key(tb, "vol", in_); // 得到成交量序列值
// 		if (vol == 0)												// 开市前没有成交的直接返回
// 		{
// 			return 0;
// 		}
// 		if (tb->control.ispubs)
// 		{
// 			uint64 last_vol = sisdb_field_get_uint_from_key(tb, "vol", unit_->lasted);
// 			if (vol <= last_vol)
// 			{
// 				return 0;
// 			}
// 		}
// 	}
// 	if (tb->append_method & SIS_ADD_METHOD_CODE)
// 	{
// 		size_t len ;
// 		const char *code_src = sisdb_field_get_char_from_key(tb, "code", in_, &len);
// 		printf("%s [%d]\n", code_src, (int)len);
// 		for(int i = 0; i < unit_->value->count; i++)
// 		{
// 			const char *val = sis_struct_list_get(unit_->value, i);
// 			const char *code_des = sisdb_field_get_char_from_key(tb, "code", val, &len);
// 			printf("-- %s [%d]\n", code_des, (int)len);
// 			if (!sis_strncasecmp(code_src, code_des, len))
// 			{
// 				sis_struct_list_update(unit_->value, i, (void *)in_);
// 				return 0;
// 			}
// 		}
// 		sis_struct_list_push(unit_->value, (void *)in_);
// 		printf("no find %s [%d]\n", code_src, unit_->value->count);
// 		return 1;
// 		// 先取code
// 		// 检索code的记录
// 		// 如果全部记录都没有该code，就设置ok，否则就no
// 	}

// 	if (tb->append_method & SIS_ADD_METHOD_TIME || tb->append_method & SIS_ADD_METHOD_SORT)
// 	{
// 		uint64 tt = sisdb_field_get_uint_from_key(tb, "time", in_); // 得到时间序列值
// 		int offset = sisdb_field_get_offset(tb, "time");
// 		int index = unit_->value->count - 1;
// 		// 检查待插入的时间位置
// 		int search_mode = _sisdb_collect_check_lasttime(unit_, tt);
// 		// printf("mode=%d tt= %lld index=%d\n", mode, tt, index);
// 		// sis_out_binary("update", in_, size);

// 		if (search_mode == SIS_CHECK_LASTTIME_OLD)
// 		{
// 			// 时间是很早以前的数据，那就重新定位数据
// 			int set = SIS_SEARCH_NONE;
// 			index = sisdb_collect_search_right(unit_, tt, &set);
// 			if (set == SIS_SEARCH_OK)
// 			{
// 				if (tb->append_method & SIS_ADD_METHOD_SORT)
// 				{
// 					sis_struct_list_insert(unit_->value, index, (void *)in_);
// 				}
// 				else
// 				{
// 					sis_struct_list_update(unit_->value, index, (void *)in_);
// 				}
// 			}
// 			else
// 			{
// 				sis_struct_list_insert(unit_->value, index, (void *)in_);
// 			}
// 			// printf("count=%d\n",unit_->value->count);
// 		}
// 		// printf("----=%d tt= %lld index=%d\n", mode, tt, index);
// 		else if (search_mode == SIS_CHECK_LASTTIME_INIT)
// 		{
// 			// 1. 初始化
// 			if (tb->control.ispubs)
// 			{
// 				sisdb_field_copy(unit_->front, NULL, unit_->value->len); // 保存的是全量，成交量使用
// 				sisdb_field_copy(unit_->lasted, in_, unit_->value->len); //
// 			}
// 			// 2. 写入数据
// 			sis_struct_list_push(unit_->value, (void *)in_);
// 			// sis_out_binary("set push", in_, size);
// 		}
// 		else if (search_mode == SIS_CHECK_LASTTIME_NEW)
// 		{
// 			if (tb->control.ispubs)
// 			{
// 				if (memcmp(unit_->lasted + offset, in_ + offset, unit_->value->len - offset))
// 				{
// 					sisdb_field_copy(unit_->front, unit_->lasted, unit_->value->len);
// 					// -- 先根据in -- tb->front生成新数据
// 					s_sis_sds in = _sisdb_make_catch_inited_sds(unit_, in_);
// 					// -- 再覆盖老数据
// 					sis_struct_list_push(unit_->value, (void *)in);
// 					sisdb_field_copy(unit_->lasted, in_, unit_->value->len);
// 					sis_sdsfree(in);
// 				}
// 				else
// 				{
// 					return 0;
// 				}
// 			}
// 			else
// 			{
// 				sis_struct_list_push(unit_->value, (void *)in_);
// 			}
// 		}
// 		else
// 		// if (mode == SIS_CHECK_LASTTIME_OK)
// 		{
// 			//如果发现记录，保存原始值到lasted，然后计算出实际要写入的值
// 			// 1. 初始化
// 			if (tb->control.ispubs)
// 			{
// 				// -- 先根据in -- tb->front生成新数据
// 				if (memcmp(unit_->lasted + offset, in_ + offset, unit_->value->len - offset))
// 				{
// 					s_sis_sds in = sisdb_make_catch_moved_sds(unit_, in_);
// 					if (tb->append_method & SIS_ADD_METHOD_SORT)
// 					{
// 						sis_struct_list_push(unit_->value, (void *)in_);
// 					}
// 					else
// 					{
// 						sis_struct_list_update(unit_->value, index, (void *)in_);
// 					}
// 					// sis_struct_list_update(unit_->value, index, (void *)in);
// 					sisdb_field_copy(unit_->lasted, in_, unit_->value->len);
// 					sis_sdsfree(in);
// 				}
// 				else
// 				{
// 					return 0;
// 				}
// 			}
// 			else
// 			{
// 				if (tb->append_method & SIS_ADD_METHOD_SORT)
// 				{
// 					sis_struct_list_push(unit_->value, (void *)in_);
// 				}
// 				else
// 				{
// 					sis_struct_list_update(unit_->value, index, (void *)in_);
// 				}
// 			}
// 		}
// 	}
// 	return 1;
// }

int sisdb_collect_update(s_sisdb_collect *unit_, s_sis_sds in_, bool ispub_)
{
	int ilen = sis_sdslen(in_);
	if (ilen < 1)
	{
		return 0;
	}

	int count = 0;
	count = (int)(ilen / unit_->value->len);
	//这里应该判断数据完整性
	if (count * unit_->value->len != ilen)
	{
		sis_out_log(3)("source format error [%d*%d!=%u]\n", count, unit_->value->len, ilen);
		return 0;
	}
	// printf("-----count =%d len=%d:%d\n", count, ilen, unit_->value->len);
	const char *ptr = in_;
	for (int i = 0; i < count; i++)
	{
		_sisdb_collect_update_alone(unit_, ptr, ispub_);
		ptr += unit_->value->len;
	}
	// 处理记录个数

	s_sisdb_table *tb = unit_->db;
	if (tb->control.limits > 0)
	{
		sis_struct_list_limit(unit_->value, tb->control.limits);
	}
	// 重建索引
	sisdb_stepindex_rebuild(unit_->stepinfo,
							_sisdb_collect_get_time(unit_, 0),
							_sisdb_collect_get_time(unit_, unit_->value->count - 1),
							unit_->value->count);

	return count;
}
////////////////

//////////////////////////////////////////////////////////////////////////////////
// 同时修改多个数据库，key_为股票代码或市场编号，value_为二进制结构化数据或json数据
//////////////////////////////////////////////////////////////////////////////////
uint64 _sisdb_fields_trans_time(uint64 in_, s_sisdb_collect *inunit_, s_sisdb_collect *outunit_)
{
	uint64 o = in_;
	bool allow = 0;

	if (inunit_->db->control.scale == SIS_TIME_SCALE_DATE && outunit_->db->control.scale >= SIS_TIME_SCALE_DATE)
	{
		allow = 2;
	}
	if (inunit_->db->control.scale == SIS_TIME_SCALE_SECOND && outunit_->db->control.scale >= SIS_TIME_SCALE_SECOND)
	{
		allow = 1;
	}
	if (inunit_->db->control.scale == SIS_TIME_SCALE_MSEC)
	{
		if (outunit_->db->control.scale > SIS_TIME_SCALE_MSEC)
		{
			allow = 1;
			o = (uint64)(o / 1000);
		}
		else
		{
			return o;
		}
	}
	if (!allow)
	{
		return o;
	}
	int date = sis_time_get_idate(o);
	switch (outunit_->db->control.scale)
	{
	case SIS_TIME_SCALE_SECOND:
		return o;
	case SIS_TIME_SCALE_INCR:
		o = sisdb_ttime_to_trade_index(o, outunit_->spec_exch);
		break;
	case SIS_TIME_SCALE_MIN1:
		o = sisdb_ttime_to_trade_index(o, outunit_->spec_exch);
		o = sisdb_trade_index_to_ttime(date, o, outunit_->spec_exch);
		break;
	case SIS_TIME_SCALE_MIN5:
		o = sisdb_ttime_to_trade_index(o, outunit_->spec_exch);
		o = sisdb_trade_index_to_ttime(date, ((int)(o / 5) + 1) * 5 - 1, outunit_->spec_exch);
		break;
	case SIS_TIME_SCALE_HOUR:
		o = sisdb_ttime_to_trade_index(o, outunit_->spec_exch);
		o = sisdb_trade_index_to_ttime(date, ((int)(o / 60) + 1) * 60 - 1, outunit_->spec_exch);
		break;
	case SIS_TIME_SCALE_DATE:
	case SIS_TIME_SCALE_WEEK:
	case SIS_TIME_SCALE_MONTH:
	case SIS_TIME_SCALE_YEAR:
		if (allow == 1)
		{
			o = sis_time_get_idate(o);
		}
		break;
	}
	return o;
}

void _sisdb_collect_struct_trans_alone(s_sis_sds ins_, s_sisdb_field *infu_, s_sisdb_collect *inunit_,
									   s_sis_sds outs_, s_sisdb_field *outfu_, s_sisdb_collect *outunit_)
{

	int64 i64 = 0;
	double f64 = 0.0;

	switch (infu_->flags.type)
	{
	case SIS_FIELD_TYPE_CHAR:
		memmove(outs_ + outfu_->offset, ins_ + infu_->offset, outfu_->flags.len);
		break;
	case SIS_FIELD_TYPE_UINT:
	case SIS_FIELD_TYPE_VOLUME:
	case SIS_FIELD_TYPE_AMOUNT:
		i64 = sisdb_field_get_int(infu_, ins_);
		sisdb_field_set_uint(outfu_, outs_, (uint64)i64);
		break;
	case SIS_FIELD_TYPE_MSEC:
	case SIS_FIELD_TYPE_SECOND:
	case SIS_FIELD_TYPE_DATE:
		i64 = sisdb_field_get_int(infu_, ins_);
		i64 = (int64)_sisdb_fields_trans_time(i64, inunit_, outunit_);
		sisdb_field_set_uint(outfu_, outs_, (uint64)i64);
		break;
	case SIS_FIELD_TYPE_INT:
		i64 = sisdb_field_get_int(infu_, ins_);
		sisdb_field_set_int(outfu_, outs_, i64);
		break;
	case SIS_FIELD_TYPE_FLOAT:
		f64 = sisdb_field_get_float(infu_, ins_);
		sisdb_field_set_float(outfu_, outs_, f64);
		break;
	case SIS_FIELD_TYPE_PRICE:
		f64 = sisdb_field_get_price(infu_, ins_, inunit_->spec_info->dot);
		sisdb_field_set_price(outfu_, outs_, f64, outunit_->spec_info->dot);
		break;
	default:
		break;
	}
}

s_sis_sds _sisdb_collect_struct_trans(s_sisdb_collect *in_unit_, s_sis_sds ins_, s_sisdb_collect *out_unit_)
{
	// const char *src = sis_struct_list_get(out_unit_->value, out_unit_->value->count - 1);
	int count = (int)(sis_sdslen(ins_) / in_unit_->value->len);
	s_sis_sds outs_ = sis_sdsnewlen(NULL, count * out_unit_->value->len);

	s_sis_sds ins = ins_;
	s_sis_sds outs = outs_;

	// printf("table=%p:%p coll=%p,field=%p \n",
	// 			out_tb, out_unit_->father, out_unit_,
	// 			out_tb->field_name);
	int fields = sis_string_list_getsize(out_unit_->db->field_name);

	for (int k = 0; k < count; k++)
	{
		for (int m = 0; m < fields; m++)
		{
			const char *key = sis_string_list_get(out_unit_->db->field_name, m);
			s_sisdb_field *out_fu = (s_sisdb_field *)sis_map_buffer_get(out_unit_->db->field_map, key);
			s_sisdb_field *in_fu = (s_sisdb_field *)sis_map_buffer_get(in_unit_->db->field_map, key);
			if (!in_fu)
			{
				continue;
			}
			_sisdb_collect_struct_trans_alone(ins, in_fu, in_unit_, outs, out_fu, out_unit_);
		}
		ins += in_unit_->value->len;
		outs += out_unit_->value->len;
		// 下一块数据
	}
	return outs_;
}

int sisdb_collect_update_publish(s_sisdb_collect *unit_, s_sis_sds val_, const char *code_)
{
	int count = sis_string_list_getsize(unit_->db->publishs);
	// printf("publishs=%d\n", count);
	s_sis_db *db = unit_->db->father;
	s_sisdb_table *pub_table;

	s_sis_sds pub_val = NULL;
	char pub_key[SIS_MAXLEN_KEY];
	s_sisdb_collect *pub_collect;

	for (int k = 0; k < count; k++)
	{
		const char *pub_db_name = sis_string_list_get(unit_->db->publishs, k);
		pub_table = sisdb_get_table(db, pub_db_name);
		// printf("publishs=%d  db=%s  fields=%p %p \n", k, link_db,link_table,link_table->field_name);
		// for (int f=0; f<sis_string_list_getsize(link_table->field_name); f++){
		// 	printf("  fields=%s\n", sis_string_list_get(link_table->field_name,f));
		// }
		if (!pub_table)
		{
			continue;
		}
		// 时间跨度大的不能转到跨度小的数据表
		if (unit_->db->control.scale > pub_table->control.scale)
		{
			continue;
		}
		sis_sprintf(pub_key, SIS_MAXLEN_KEY, "%s.%s", code_, pub_db_name);

		pub_collect = sisdb_get_collect(db, pub_key);
		if (!pub_collect)
		{
			pub_collect = sisdb_collect_create(db, pub_key);
			if (!pub_collect)
			{
				continue;
			} // printf("new......\n");
		}

		// printf("publishs=%d  db=%s.%s in= %p table=%p:%p coll=%p,field=%p \n", k, link_db,key_,
		// 		collect_->father,
		// 		link_table, link_collect->father, link_collect,
		// 		link_table->field_name);
		//---- 转换到其他数据库的数据格式 ----//
		pub_val = _sisdb_collect_struct_trans(unit_, val_, pub_collect);
		//---- 修改数据 ----//
		if (pub_val)
		{
			// printf("table_=%lld\n",sisdb_field_get_uint_from_key(table_,"time",in_val));
			// printf("link_table=%lld\n",sisdb_field_get_uint_from_key(link_table,"time",link_val));
			sisdb_collect_update(pub_collect, pub_val, true);

			// sisdb_sys_check_write(db, pub_collect); // publish出去的库，不会影响config信息

			sis_sdsfree(pub_val);
		}
	}
	return count;
}

// 从磁盘中整块写入，不逐条进行校验
int sisdb_collect_update_block(s_sisdb_collect *unit_, const char *in_, size_t ilen_)
{
	if (ilen_ < 1)
	{
		return 0;
	}
	int count = 0;

	count = (int)(ilen_ / unit_->value->len);
	//这里应该判断数据完整性
	if (count * unit_->value->len != ilen_)
	{
		sis_out_log(3)("source format error [%d*%d!=%lu]\n", count, unit_->value->len, ilen_);
		return 0;
	}
	// printf("-[%s]----count =%d len=%ld:%d\n", unit_->father->name, count, ilen_, unit_->value->len);
	for (int i = 0; i < count; i++)
	{
		sis_struct_list_push(unit_->value, (void *)(in_ + i * unit_->value->len));
	}

	sisdb_stepindex_rebuild(unit_->stepinfo,
							_sisdb_collect_get_time(unit_, 0),
							_sisdb_collect_get_time(unit_, unit_->value->count - 1),
							unit_->value->count);
	return count;
}
