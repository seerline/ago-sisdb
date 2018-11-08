﻿

#include <sisdb_collect.h>
#include <sisdb_fields.h>

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
	char table[SIS_MAXLEN_TABLE];
	if(!sis_str_carve(key_, code, SIS_MAXLEN_CODE, table, SIS_MAXLEN_TABLE, '.'))
	{
		sis_out_log(3)("key [%s] error.\n", key_);
		return NULL;
	}

	s_sisdb_table *tb = sisdb_get_db(db_, table);
	if (!tb) {
		sis_out_log(3)("table define no find.\n");
		return NULL;
	}
	s_sisdb_sysinfo *info = sisdb_get_sysinfo(db_, code);
	if (!info) {
		// 需要在info表中创建一条记录，记录使用默认值
		info = sisdb_sysinfo_create(db_, code);
		if (!info) {
			sis_out_log(3)("cannot get info.\n");
			return NULL;
		}
	}

	s_sisdb_collect *unit = sis_malloc(sizeof(s_sisdb_collect));
	memset(unit, 0, sizeof(s_sisdb_collect));
	sis_map_buffer_set(db_->collects, key_, unit);

	unit->db = tb;
	unit->info = info;

	unit->stepinfo = sisdb_stepindex_create();
	// 仅仅在这里计算了记录长度，
	int size = sisdb_table_get_fields_size(tb);
	unit->value = sis_struct_list_create(size, NULL, 0);

	if (tb_->control.issubs)
	{
		unit->front = sis_sdsnewlen(NULL, size);
		unit->lasted = sis_sdsnewlen(NULL, size);
	}
	if (tb_->control.iszip)
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

void sisdb_collect_clear(s_sisdb_collect *unit_)
{
	sis_struct_list_clear(unit_->value);
	sisdb_stepindex_clear(unit_->stepinfo);
	if (unit_->db->control.issubs)
	{
		sis_sdsclear(unit_->front);
		sis_sdsclear(unit_->lasted);
	}
	if (unit_->db->control.iszip)
	{
		sis_sdsclear(unit_->refer);
	}

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
	while (i >= 0 && i < sisdb_collect_recs(unit_))
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
	if (*count_ <= 0)
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

s_sis_sds sisdb_collect_get_of_count_sds(s_sisdb_collect *unit_, int start_, int count_)
{
	bool o = _sisdb_trans_of_count(unit_, &start_, &count_);
	if (!o)
	{
		return NULL;
	}	
	return sis_sdsnewlen(sis_struct_list_get(unit_->value, start_), count_ * unit_->value->len);
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


////////////////////////////////////////////
// format trans
///////////////////////////////////////////
s_sis_sds sisdb_collect_struct_filter_sds(s_sisdb_collect *unit_, s_sis_sds in_, const char *fields_)
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
	sis_string_list_destroy(field_list);
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
			sis_json_array_add_double(o, sisdb_field_get_float(fu, ptr), unit_->info->dot);
			break;
		default:
			sis_json_array_add_string(o, " ", 1);
			break;
		}
	}	
	return o;
}

s_sis_sds sisdb_collect_struct_to_json_sds(s_sisdb_collect *unit_, s_sis_sds in_, s_sis_string_list *fields_)
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
	str = sis_json_output_zip(jone, &olen);
	o = sis_sdsnewlen(str, olen);
	sis_free(str);
	sis_json_delete_node(jone);

	return o;
}

s_sis_sds sisdb_collect_struct_to_array_sds(s_sisdb_collect *unit_, s_sis_sds in_, s_sis_string_list *fields_)
{
	s_sis_sds o = NULL;

	char *str;
	s_sis_json_node *jone = sis_json_create_array();
	s_sis_json_node *jval = NULL;

	int dot = 0; //小数点位数
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
		str = sis_json_output_zip(jone, &olen);
	}
	else
	{
		// printf("jval = %s\n", sis_json_output(jval, &olen));
		str = sis_json_output_zip(jval, &olen);
	}
	// 输出数据
	// printf("1112111 [%d]\n",unit_->db->control.limits);
	o = sis_sdsnewlen(str, olen);
	sis_free(str);
	sis_json_delete_node(jone);

	return o;
}
////////////////////////////////////////////
// main get
///////////////////////////////////////////
s_sis_sds sisdb_collect_get_sds(s_sis_db *db_,const char *key_, const char *com_)
{
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
	s_sis_sds out = NULL;

	// 检查取值范围，没有就全部取
	int64 min, max;
	int start, stop;
	int count = 0;
	int maxX, minX;

	s_sis_json_node *search = sis_json_cmp_child_node(handle->node, "search");
	if (!search)
	{
		out = sisdb_collect_get_of_range_sds(collect, 0, -1);
		goto filter;
	}
	bool by_time = sis_json_cmp_child_node(search, "min") != NULL;
	bool by_region = sis_json_cmp_child_node(search, "start") != NULL;
	if (by_time)
	{
		min = sis_json_get_int(search, "min", 0);
		if (sis_json_cmp_child_node(search, "count"))
		{
			count = sis_json_get_int(search, "count", 1);
			start = sisdb_collect_search_right(collect, min, &minX);
			if (start >= 0)
			{
				out = sisdb_collect_get_of_count_sds(collect, start, count);
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
					out = sisdb_collect_get_of_range_sds(collect, start, stop);
				}
			}
			else
			{
				start = sisdb_collect_search(collect, min);
				if (start >= 0)
				{
					out = sisdb_collect_get_of_count_sds(collect, start, 1);
				}
			}
		}
		goto filter;
	}
	if (by_region)
	{
		start = sis_json_get_int(search, "start", -1); // -1 为最新一条记录
		if (sis_json_cmp_child_node(search, "count"))
		{
			count = sis_json_get_int(search, "count", 1);
			out = sisdb_collect_get_of_count_sds(collect, start, count);
		}
		else
		{
			if (sis_json_cmp_child_node(search, "stop"))
			{
				stop = sis_json_get_int(search, "stop", -1); // -1 为最新一条记录
				out = sisdb_collect_get_of_range_sds(collect, start, stop);
			}
			else
			{
				out = sisdb_collect_get_of_count_sds(collect, start, 1);
			}
		}
		goto filter;
	}
filter:
	if (!out)
	{
		goto nodata;
	}
	// 最后转数据格式
	// 取出数据返回格式，没有就默认为二进制结构数据
	// int iformat = SIS_DATA_TYPE_STRUCT;
	int iformat = sis_from_node_get_format(db_, handle->node);

	// printf("iformat = %c\n", iformat);
	// 取出字段定义，没有就默认全部字段

	s_sis_sds fields = NULL;
	if(sis_json_cmp_child_node(handle->node, "fields")) {
		fields = sis_sdsnew(sis_json_get_str(handle->node, "fields"));
	} else {
		fields = sis_sdsnew("*");
	}
	printf("query fields = %s\n", fields);

	s_sisdb_table *tb = sisdb_get_db_from_key(db_,key_);
	
	s_sis_string_list *field_list = tb->field_name; //取得全部的字段定义
	if (!sisdb_field_is_whole(fields_))
	{
		field_list = sis_string_list_create_w();
		sis_string_list_load(field_list, fields_, strlen(fields_), ",");
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
		other = sisdb_collect_struct_to_json_sds(collect, out, field_list);
		sis_sdsfree(out);
		out = other;
		break;
	case SIS_DATA_TYPE_ARRAY:
		other = sisdb_collect_struct_to_array_sds(collect, out, field_list);
		sis_sdsfree(out);
		out = other;
		break;
	}
	if (!sisdb_field_is_whole(fields_))
	{
		sis_string_list_destroy(field_list);
	}	
	if (fields)
	{
		sis_sdsfree(fields);
	}
nodata:
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
	sis_json_object_add_node(groups, code, val);
}
s_sis_sds sisdb_collect_groups_json_sds(s_sis_json_node *node_)
{
	if(!node_) return NULL;
	size_t olen;
	char *str = sis_json_output_zip(node_, &olen);
	s_sis_sds o = sis_sdsnewlen(str, olen);
	sis_free(str);
}

s_sis_sds sisdb_collects_get_last_sds(s_sis_db *db_,const char *dbname_, const char *com_)
{
	s_sis_json_handle *handle = sis_json_load(com_, strlen(com_));
	// printf("com_ = %s -- %lu -- %p\n", com_,strlen(com_),handle);
	if (!handle)
	{
		return NULL;
	}	
	int iformat = sis_from_node_get_format(db_, handle->node);

	printf("iformat = %c\n", iformat);
	// 取出字段定义，没有就默认全部字段
	s_sis_sds fields = NULL;
	if(sis_json_cmp_child_node(handle->node, "fields")) {
		fields = sis_sdsnew(sis_json_get_str(handle->node, "fields"));
	} else {
		fields = sis_sdsnew("*");
	}
	printf("query fields = %s\n", fields);

	s_sisdb_table *tb = sisdb_get_db(db_, dbname_);
	s_sis_string_list *field_list = tb->field_name; //取得全部的字段定义
	if (!sisdb_field_is_whole(fields_))
	{
		field_list = sis_string_list_create_w();
		sis_string_list_load(field_list, fields_, strlen(fields_), ",");
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
		if (!sis_strcasecmp(collect->db->name, dbname_)) 
		{
			s_sis_sds val = sisdb_collect_get_of_count_sds(collect, -1, 1);
        	sis_str_substr(code, SIS_MAXLEN_CODE, sis_dict_getkey(de), '.', 0);

			// printf("out = %lu -- %lu\n", sis_sdslen(out),sis_sdslen(val));
			switch (iformat)
			{
			case SIS_DATA_TYPE_STRUCT:
				if (!out)
				{
					out = sis_sdsnewlen(code, SIS_MAXLEN_CODE);
				} else {
					out = sis_sdscatlen(out, code, SIS_MAXLEN_CODE);
				}
				if (!sisdb_field_is_whole(fields))
				{
					other = sisdb_collect_struct_filter_sds(collect, val, field_list);
					out = sis_sdscatlen(out, other, sis_sdslen(other));
					sis_sdsfree(other);
				} else {
					out = sis_sdscatlen(out, val, sis_sdslen(val));
				}
				break;
			case SIS_DATA_TYPE_JSON:
			default :
				if (!node) {
					node = sisdb_collect_groups_json_init(field_list);
				} else {
					sisdb_collect_groups_json_push(node, code, collect, val, field_list);
				}
				break;
			}
			sis_sdsfree(val);
		}
	}
	sis_dict_iter_free(di);

	if(iformat!=SIS_DATA_TYPE_STRUCT) 
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
	if (!sisdb_field_is_whole(fields_))
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

s_sis_sds sisdb_collects_get_code_sds(s_sis_db *db_,const char *dbname_, const char *com_)  //返回数据需要释放
{
	int count = 0;
	char code[SIS_MAXLEN_CODE];

	s_sis_sds out = NULL;

	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(db_->collects);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
		if (!sis_strcasecmp(collect->db->name, dbname_)) 
		{
        	sis_str_substr(code, SIS_MAXLEN_CODE, sis_dict_getkey(de), '.', 0);
			if (!out)
			{
				out = sis_sdsnewlen(code, SIS_MAXLEN_CODE);
			} else {
				out = sis_sdscatlen(out, code, SIS_MAXLEN_CODE);
			}
			count++;
		}
	}
	sis_dict_iter_free(di);

	if (!out)
	{
		return NULL;
	}
	s_sis_json_handle *handle = sis_json_load(com_, strlen(com_));
	// printf("com_ = %s -- %lu -- %p\n", com_,strlen(com_),handle);
	if (!handle)
	{
		return NULL;
	}
	int iformat = sis_from_node_get_format(db_, handle->node);

	printf("iformat = %c\n", iformat);
	// 取出字段定义，没有就默认全部字段
	if (iformat == SIS_DATA_TYPE_STRUCT) 
	{
		goto end;
	}
	char *str;
	s_sis_json_node *jone = NULL;
	s_sis_json_node *jval = sis_json_create_array();

	for (int i = 0; i < count; i++)
	{
		sis_json_array_add_string(jval, out + i * SIS_MAXLEN_CODE, SIS_MAXLEN_CODE);
	}
	if (iformat == SIS_DATA_TYPE_ARRAY) 
	{
		jone = jval;
	} 
	else // SIS_DATA_TYPE_JSON
	{
		jone = sis_json_create_object();
		sis_json_object_add_node(jone, SIS_JSON_KEY_COLLECTS, jval);
	}

	size_t olen;
	str = sis_json_output_zip(jone, &olen);
	sis_sdsfree(out);
	out = sis_sdsnewlen(str, olen);
	sis_free(str);
	sis_json_delete_node(jone);

end:
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
        return NULL;
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
		start = sis_json_get_int(range, "start", -1); // -1 为最新一条记录
		if (sis_json_cmp_child_node(range, "count"))
		{
			count = sis_json_get_int(range, "count", 1);
			out = sisdb_collect_delete_of_count(collect, start, count); // 定位后按数量删除
		}
		else
		{
			if (sis_json_cmp_child_node(range, "stop"))
			{
				stop = sis_json_get_int(range, "stop", -1);					  // -1 为最新一条记录
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
		printf("key=%s size=%d offset=%d\n", key, fields, fu->offset);

		sisdb_field_json_to_struct(o, fu, (char *)key, handle->node, unit_->info);
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
			printf("key=%s size=%d offset=%d\n", fname, fields, fu->offset);
			if (!fu)
			{
				continue;
			}
			char key[16];
			sis_sprintf(key, 10, "%d", k);
			sisdb_field_json_to_struct(o + index * unit_->value->len, fu, key, jval);
		}
		index++;
		jval = jval->next;
	}

	sis_json_close(handle);
	return o;
}
//检查是否增加记录，只和最后一条记录做比较，返回3个，一是当日最新记录，一是新记录，一是老记录
int _sisdb_collect_check_lasttime(s_sisdb_collect *unit_, uint64 finder_)
{
	if (unit_->value->count < 1)
	{
		return SIS_CHECK_LASTTIME_INIT;
	}

	uint64 ts = _sisdb_collect_get_time(unit_, unit_->value->count - 1);
	if (finder_ > ts)
	{
		if (sis_time_get_idate(finder_) > sis_time_get_idate(ts))
		{
			return SIS_CHECK_LASTTIME_INIT;
		}
		else
		{
			return SIS_CHECK_LASTTIME_NEW;
		}
	}
	else if (finder_ < ts)
	{
		// 应该判断量，如果量小就返回错误
		return SIS_CHECK_LASTTIME_OLD;
	}
	else
	{
		return SIS_CHECK_LASTTIME_OK;
	}
}

s_sis_sds _sisdb_make_catch_inited_sds(s_sisdb_collect *unit_, const char *in_)
{
	s_sisdb_table *tb = unit_->father;
	s_sis_sds o = sis_sdsnewlen(in_, sis_sdslen(unit_->front));
	int fields = sis_string_list_getsize(tb->field_name);
	for (int k = 0; k < fields; k++)
	{
		const char *key = sis_string_list_get(tb->field_name, k);
		s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, key);
		if (!fu || fu->subscribe_method == SIS_SUBS_METHOD_COPY)
		{
			continue;
		}
		uint64 u64 = 0;
		// printf("fixed--key=%s size=%d offset=%d, method=%d\n", key, fields, fu->offset,fu->subscribe_method);
		if (fu->subscribe_method == SIS_SUBS_METHOD_INCR)
		{
			// printf("----inited incr key=%s new=%lld front=%lld\n", key, sisdb_field_get_uint(fu,in_), sisdb_field_get_uint(fu,unit_->front));
			u64 = sisdb_field_get_uint(fu, in_) - sisdb_field_get_uint(fu, unit_->front);
		}
		else
		{ //SIS_SUBS_METHOD_INIT&SIS_SUBS_METHOD_MIN&SIS_SUBS_METHOD_MAX
			s_sisdb_field *sfu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, fu->subscribe_refer_fields);
			if (sfu)
			{
				u64 = sisdb_field_get_uint(sfu, in_);
			}
		}
		sisdb_field_set_uint(fu, o, u64);
	}
	// printf("o=%lld\n",sisdb_field_get_uint_from_key(tb,"time",o));
	// printf("in_=%lld\n",sisdb_field_get_uint_from_key(tb,"time",in_));
	return o;
}
s_sis_sds sisdb_make_catch_moved_sds(s_sisdb_collect *unit_, const char *in_)
{
	s_sisdb_table *tb = unit_->father;
	// 获取最后一条记录
	s_sis_sds moved = sis_struct_list_last(unit_->value);

	// 保存源数据
	s_sis_sds o = sis_sdsnewlen(in_, sis_sdslen(unit_->front));
	int fields = sis_string_list_getsize(tb->field_name);
	for (int k = 0; k < fields; k++)
	{
		const char *key = sis_string_list_get(tb->field_name, k);
		s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, key);
		// printf("fixed--key=%s size=%d offset=%d\n", key, fields, fu->offset);
		if (!fu || fu->subscribe_method == SIS_SUBS_METHOD_COPY)
		{
			continue;
		}
		// in_.vol - front.vol
		// in_.money - front.money
		// in.close = in_.close
		uint64 u64, u64_front, u64_moved, u64_inited;
		// close 的字段

		if (fu->subscribe_method == SIS_SUBS_METHOD_INCR)
		{
			printf("---- incr key=%s new=%lld front=%lld\n", key, sisdb_field_get_uint(fu, in_), sisdb_field_get_uint(fu, unit_->front));
			u64 = sisdb_field_get_uint(fu, in_) - sisdb_field_get_uint(fu, unit_->front);
		}
		else if (fu->subscribe_method == SIS_SUBS_METHOD_MAX)
		{
			// in_.high > front.high || !front.high => in.high = in_.high
			//     else in_.close > moved.high => in.high = in_.close
			//		       else in.high = moved.high
			u64 = sisdb_field_get_uint(fu, in_);
			u64_front = sisdb_field_get_uint(fu, unit_->front);
			u64_moved = sisdb_field_get_uint(fu, moved);
			s_sisdb_field *sfu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, fu->subscribe_refer_fields);
			u64_inited = sisdb_field_get_uint(sfu, in_);
			if (u64 > u64_front || !u64_front)
			{
			}
			else
			{
				if (u64_inited > u64_moved)
				{
					u64 = u64_inited;
				}
				else
				{
					u64 = u64_moved;
				}
			}
		}
		else if (fu->subscribe_method == SIS_SUBS_METHOD_MIN)
		{
			// in_.low < front.low || !front.low => in.low = in_.low
			//		else in_.close < moved.low => in.low = in_.close
			//		       else in.low = moved.low
			u64 = sisdb_field_get_uint(fu, in_);
			u64_front = sisdb_field_get_uint(fu, unit_->front);
			u64_moved = sisdb_field_get_uint(fu, moved);
			s_sisdb_field *sfu = (s_sisdb_field *)sis_map_buffer_get(tb->field_map, fu->subscribe_refer_fields);
			u64_inited = sisdb_field_get_uint(sfu, in_);
			if (u64 < u64_front || !u64_front)
			{
			}
			else
			{
				if (u64_inited < u64_moved)
				{
					u64 = u64_inited;
				}
				else
				{
					u64 = u64_moved;
				}
			}
		}
		else
		{ //SIS_SUBS_METHOD_INIT
			// 恢复open
			u64 = sisdb_field_get_uint(fu, moved);
			if (u64 == 0) // 如果是开始第一笔，就直接取对应字段值
			{
				u64 = sisdb_field_get_uint(fu, in_);
			}
		}
		sisdb_field_set_uint(fu, o, u64);
	}

	return o;
}
int _sisdb_collect_update_alone(s_sisdb_collect *unit_, const char *in_)
{
	s_sisdb_table *tb = unit_->db;

	if (tb->append_method == SIS_ADD_METHOD_ALWAYS)
	{
		sis_struct_list_push(unit_->value, (void *)in_);
		return 1;
	}
	if (tb->append_method & SIS_ADD_METHOD_VOL)
	{
		uint64 vol = sisdb_field_get_uint_from_key(tb, "vol", in_); // 得到成交量序列值
		if (vol == 0)											   // 开市前没有成交的直接返回
		{
			return 0;
		}
		if (tb->control.issubs)
		{
			uint64 last_vol = sisdb_field_get_uint_from_key(tb, "vol", unit_->lasted);
			if (vol <= last_vol)
			{
				return 0;
			}
		}
	}
	if (tb->append_method & SIS_ADD_METHOD_CODE)
	{
		// 先取code
		// 检索code的记录
		// 如果全部记录都没有该code，就设置ok，否则就no
	}

	if (tb->append_method & SIS_ADD_METHOD_TIME || tb->append_method & SIS_ADD_METHOD_SORT)
	{
		uint64 tt = sisdb_field_get_uint_from_key(tb, "time", in_); // 得到时间序列值
		int offset = sisdb_field_get_offset(tb, "time");
		int index = unit_->value->count - 1;
		// 检查待插入的时间位置
		int search_mode = _sisdb_collect_check_lasttime(unit_, tt);
		// printf("mode=%d tt= %lld index=%d\n", mode, tt, index);
		// sis_out_binary("update", in_, size);

		if (search_mode == SIS_CHECK_LASTTIME_OLD)
		{
			// 时间是很早以前的数据，那就重新定位数据
			int set = SIS_SEARCH_NONE;
			index = sisdb_collect_search_right(unit_, tt, &set);
			if (set == SIS_SEARCH_OK)
			{
				if (tb->append_method & SIS_ADD_METHOD_SORT)
				{
					sis_struct_list_insert(unit_->value, index, (void *)in_);
				}
				else
				{
					sis_struct_list_update(unit_->value, index, (void *)in_);
				}
			}
			else
			{
				sis_struct_list_insert(unit_->value, index, (void *)in_);
			}
			// printf("count=%d\n",unit_->value->count);
		}
		// printf("----=%d tt= %lld index=%d\n", mode, tt, index);
		else if (search_mode == SIS_CHECK_LASTTIME_INIT)
		{
			// 1. 初始化
			if (tb->control.issubs)
			{
				sisdb_field_copy(unit_->front, NULL, unit_->value->len); // 保存的是全量，成交量使用
				sisdb_field_copy(unit_->lasted, in_, unit_->value->len); //
			}
			// 2. 写入数据
			sis_struct_list_push(unit_->value, (void *)in_);
			// sis_out_binary("set push", in_, size);
		}
		else if (search_mode == SIS_CHECK_LASTTIME_NEW)
		{
			if (tb->control.issubs)
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
		else
		// if (mode == SIS_CHECK_LASTTIME_OK)
		{
			//如果发现记录，保存原始值到lasted，然后计算出实际要写入的值
			// 1. 初始化
			if (tb->control.issubs)
			{
				// -- 先根据in -- tb->front生成新数据
				if (memcmp(unit_->lasted + offset, in_ + offset, unit_->value->len - offset))
				{
					s_sis_sds in = sisdb_make_catch_moved_sds(unit_, in_);
					if (tb->append_method & SIS_ADD_METHOD_SORT)
					{
						sis_struct_list_push(unit_->value, (void *)in_);
					}
					else
					{
						sis_struct_list_update(unit_->value, index, (void *)in_);
					}
					// sis_struct_list_update(unit_->value, index, (void *)in);
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
				if (tb->append_method & SIS_ADD_METHOD_SORT)
				{
					sis_struct_list_push(unit_->value, (void *)in_);
				}
				else
				{
					sis_struct_list_update(unit_->value, index, (void *)in_);
				}
			}
		}
	}
	return 1;
}

int sisdb_collect_update(s_sisdb_collect *unit_, s_sis_sds in_)
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
		sis_out_log(3)("source format error [%d*%d!=%lu]\n", count, unit_->value->len, ilen);
		return 0;
	}
	// printf("-[%s]----count =%d len=%ld:%d\n", unit_->father->name, count, ilen_, unit_->value->len);
	const char *ptr = in_;
	for (int i = 0; i < count; i++)
	{
		_sisdb_collect_update_alone(unit_, ptr);
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


void sisdb_collect_struct_trans(s_sis_sds ins_, s_sisdb_field *infu_, s_sisdb_table *indb_,
							  s_sis_sds outs_, s_sisdb_field *outfu_, s_sisdb_table *outdb_)
{
	int8 i8 = 0;
	int16 i16 = 0;
	int32 i32 = 0;
	int64 i64, zoom = 1;
	double f64 = 0.0;

	switch (infu_->flags.type)
	{
	case SIS_FIELD_TYPE_CHAR:
		memmove(outs_ + outfu_->offset, ins_ + infu_->offset, outfu_->flags.len);
		break;
	case SIS_FIELD_TYPE_INT:
	case SIS_FIELD_TYPE_UINT:
		i64 = sisdb_field_get_int(infu_, ins_);

		if (outfu_->flags.io && outfu_->flags.zoom > 0)
		{
			zoom = sis_zoom10(outfu_->flags.zoom);
		}
		// 对时间进行转换
		if (sisdb_field_is_time(infu_))
		{
			i64 = (int64)sisdb_table_struct_trans_time(i64, indb_->control.scale,
													 outdb_, outdb_->control.scale);
		}
		// 对成交量进行转换，必须是当天写入的数据，才以min为基础进行转换，
		if (outfu_->flags.len == 1)
		{
			i8 = (int8)(i64 / zoom);
			memmove(outs_ + outfu_->offset, &i8, outfu_->flags.len);
		}
		else if (outfu_->flags.len == 2)
		{
			i16 = (int16)(i64 / zoom);
			memmove(outs_ + outfu_->offset, &i16, outfu_->flags.len);
		}
		else if (outfu_->flags.len == 4)
		{
			i32 = (int32)(i64 / zoom);
			memmove(outs_ + outfu_->offset, &i32, outfu_->flags.len);
		}
		else
		{
			i64 = (int64)(i64 / zoom);
			memmove(outs_ + outfu_->offset, &i64, outfu_->flags.len);
		}
		break;
	case SIS_FIELD_TYPE_FLOAT:
		f64 = sisdb_field_get_price(infu_, ins_);
		if (!outfu_->flags.io && outfu_->flags.zoom > 0)
		{
			zoom = sis_zoom10(outfu_->flags.zoom);
		}
		if (outfu_->flags.len == 4)
		{
			i32 = (int32)(f64 * zoom);
			memmove(outs_ + outfu_->offset, &i32, outfu_->flags.len);
		}
		else
		{
			i64 = (int64)(f64 * zoom);
			memmove(outs_ + outfu_->offset, &i64, outfu_->flags.len);
		}

		break;
	default:
		break;
	}
}

////////////////