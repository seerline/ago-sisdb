

#include "sts_collect.h"

///////////////////////////////////////////////////////////////////////////
//------------------------s_sts_step_index --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sts_step_index *sts_stepindex_create()
{
	s_sts_step_index *si = zmalloc(sizeof(s_sts_step_index));
	si->left = 0;
	si->right = 0;
	si->count = 0;
	si->step = 0;
	return si;
}
void sts_stepindex_destroy(s_sts_step_index *si_)
{
	zfree(si_);
}
void sts_stepindex_rebuild(s_sts_step_index *si_, uint64 left_, uint64 right_, int count_)
{
	si_->left = left_;
	si_->right = right_;
	si_->count = count_;
	si_->step = min((uint64)((right_ - left_) / count_), 1);
}
int stepindex_goto(s_sts_step_index *si_, uint64 curr_)
{
	if (curr_ < si_->left)
	{
		return 0;
	}
	if (curr_ > si_->right)
	{
		return si_->count - 1;
	}
	int index = (int)((curr_ - si_->left) / si_->step);
	if (index > si_->count - 1)
	{
		return si_->count - 1;
	}
	return index;
}
///////////////////////////////////////////////////////////////////////////
//------------------------s_sts_collect_unit --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sts_collect_unit *sts_collect_unit_create(s_sts_table *tb_, const char *key_)
{
	s_sts_collect_unit *unit = zmalloc(sizeof(s_sts_collect_unit));
	memset(unit, 0, sizeof(s_sts_collect_unit));
	unit->father = tb_;
	unit->step = sts_stepindex_create();
	unit->value = sts_struct_list_create(sts_table_get_fields_size(tb_), NULL, 0);
	return unit;
}
void sts_collect_unit_destroy(s_sts_collect_unit *unit_)
{
	if (unit_->value)
	{
		sts_struct_list_destroy(unit_->value);
	}
	if (unit_->step)
	{
		sts_stepindex_destroy(unit_->step);
	}
	zfree(unit_);
}

uint64 sts_collect_unit_get_time(s_sts_collect_unit *unit_, int index_)
{

	time_t tt = 0;
	void *val = sts_struct_list_get(unit_->value, index_);
	if (val)
	{
		tt = sts_table_get_times(unit_->father, val);
	}
	return tt;
}
////////////////////////
//  delete
////////////////////////

int _sts_collect_unit_delete(s_sts_collect_unit *unit_, int start_, int count_)
{

	sts_struct_list_delete(unit_->value, start_, count_);
	sts_stepindex_rebuild(unit_->step,
						  sts_collect_unit_get_time(unit_, 0),
						  sts_collect_unit_get_time(unit_, unit_->value->count - 1),
						  unit_->value->count);
	return 0;
}
int sts_collect_unit_recs(s_sts_collect_unit *unit_)
{
	return unit_->value->count;
}

int sts_collect_unit_search_left(s_sts_collect_unit *unit_, uint64 index_, int *mode_)
{
	*mode_ = STS_SEARCH_NONE;
	int index = stepindex_goto(unit_->step, index_);
	if (index < 0)
	{
		return -1; // 没有任何数据
	}
	int i = index;
	int dir = 0;
	while (i >= 0 && i < unit_->value->count)
	{
		uint64 ts = sts_collect_unit_get_time(unit_, i);
		if (index_ > ts)
		{
			if (dir == -1)
			{
				*mode_ = STS_SEARCH_LEFT;
				return i;
			}
			dir = 1;
			i += dir;
			continue;
		}
		else if (index_ < ts)
		{
			if (dir == 1)
			{
				*mode_ = STS_SEARCH_LEFT;
				return i - 1;
			}
			dir = -1;
			i += dir;
			continue;
		}
		else
		{
			*mode_ = STS_SEARCH_OK;
			return i;
		}
	}
	if (dir == 1)
	{
		*mode_ = STS_SEARCH_LEFT;
		return unit_->value->count - 1;
	}
	else
	{
		return -1;
	}
}
int sts_collect_unit_search_right(s_sts_collect_unit *unit_, uint64 index_, int *mode_)
{

	*mode_ = STS_SEARCH_NONE;
	int index = stepindex_goto(unit_->step, index_);
	if (index < 0)
	{
		return -1; // 没有任何数据
	}
	int i = index;
	int dir = 0;
	while (i >= 0 && i < unit_->value->count)
	{
		uint64 ts = sts_collect_unit_get_time(unit_, i);
		if (index_ > ts)
		{
			if (dir == -1)
			{
				*mode_ = STS_SEARCH_RIGHT;
				return i + 1;
			}
			dir = 1;
			i += dir;
			continue;
		}
		else if (index_ < ts)
		{
			if (dir == 1)
			{
				*mode_ = STS_SEARCH_RIGHT;
				return i;
			}
			dir = -1;
			i += dir;
			continue;
		}
		else
		{
			*mode_ = STS_SEARCH_OK;
			return i;
		}
	}

	if (dir == 1)
	{
		return -1;
	}
	else
	{
		*mode_ = STS_SEARCH_RIGHT;
		return 0;
	}
}
int sts_collect_unit_search(s_sts_collect_unit *unit_, uint64 index_)
{

	int index = stepindex_goto(unit_->step, index_);
	if (index < 0)
	{
		return -1; // 没有任何数据
	}
	int i = index;
	int dir = 0;
	while (i >= 0 && i < unit_->value->count)
	{
		uint64 ts = sts_collect_unit_get_time(unit_, i);
		if (index_ > ts)
		{
			if (dir == -1)
			{
				return i - 1;
			}
			dir = 1;
			i += dir;
			continue;
		}
		else if (index_ < ts)
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

int sts_collect_unit_delete_of_range(s_sts_collect_unit *unit_, int start_, int stop_)
{
	int llen, count;

	llen = sts_collect_unit_recs(unit_);

	if (start_ < 0)
		start_ = llen + start_;
	if (stop_ < 0)
		stop_ = llen + stop_;
	if (start_ < 0)
		start_ = 0;

	if (start_ > stop_ || start_ >= llen)
	{
		return 0;
	}
	if (stop_ >= llen)
		stop_ = llen - 1;
	count = (stop_ - start_) + 1;

	return _sts_collect_unit_delete(unit_, start_, count);
}
int sts_collect_unit_delete_of_count(s_sts_collect_unit *unit_, int start_, int count_)
{
	int llen;
	llen = sts_collect_unit_recs(unit_);

	if (start_ < 0)
		start_ = llen + start_;
	if (start_ < 0)
		start_ = 0;

	if (start_ >= llen)
	{
		return 0;
	}
	if (start_ + count_ > llen)
		count_ = llen - start_;

	return _sts_collect_unit_delete(unit_, start_, count_);
}
//////////////////////////////////////////////////////
//  get  --  先默认二进制格式，全部字段返回数据
//////////////////////////////////////////////////////

sds sts_collect_unit_get_of_count_m(s_sts_collect_unit *unit_, int start_, int count_)
{
	if (count_ == 0)
	{
		return NULL;
	}
	int llen = sts_collect_unit_recs(unit_);

	if (start_ < 0)
	{
		start_ = llen + start_;
	}
	if (start_ < 0)
	{
		start_ = 0;
	}
	if (count_ < 0)
	{
		count_ = abs(count_);
		if (count_ > (start_ + 1))
		{
			count_ = start_ + 1;
		}
		start_ -= (count_ - 1);
	}
	else
	{
		if ((start_ + count_) > llen)
		{
			count_ = llen - start_;
		}
	}
	return sdsnewlen(sts_struct_list_get(unit_->value, start_), count_ * unit_->value->len);
}

sds sts_collect_unit_get_of_range_m(s_sts_collect_unit *unit_, int start_, int stop_)
{
	int llen = sts_collect_unit_recs(unit_);

	if (start_ < 0)
	{
		start_ = llen + start_;
	}
	if (stop_ < 0)
	{
		stop_ = llen + stop_;
	}
	if (start_ < 0)
	{
		start_ = 0;
	}

	if (start_ > stop_ || start_ >= llen)
	{
		return NULL;
	}
	if (stop_ >= llen)
	{
		stop_ = llen - 1;
	}

	int count = (stop_ - start_) + 1;
	return sdsnewlen(sts_struct_list_get(unit_->value, start_), count_ * unit_->value->len);
}
////////////////////////
//  update
////////////////////////
int _sts_collect_unit_update(s_sts_collect_unit *unit_, const char *in_)
{
	uint64 tt;
	switch (unit_->father->control.insert_mode)
	{
	case STS_OPTION_ALWAYS:
		if (unit_->father->control.limit_rows == 1)
		{
			if (sts_struct_list_update(unit_->value, 0, (void *)in_) < 0)
			{
				sts_struct_list_push(unit_->value, (void *)in_);
			}
		}
		else
		{
			sts_struct_list_push(unit_->value, (void *)in_);
			if (unit_->father->control.limit_rows > 1)
			{
				sts_struct_list_limit(unit_->value, unit_->father->control.limit_rows);
			}
		}
		break;
	case STS_OPTION_MUL_CHECK:
		break;
	case STS_OPTION_TIME:
	case STS_OPTION_VOL:
	default:												  // STS_OPTION_STS_CHECK
		tt = sts_table_get_times(unit_->father, (void *)in_); // 得到时间序列值
		int mode;
		int index = sts_collect_unit_search_left(unit_, tt, &mode);
		if (mode == STS_SEARCH_NONE)
		{
			sts_struct_list_push(unit_->value, (void *)in_);
		}
		else if (mode == STS_SEARCH_OK)
		{
			sts_struct_list_update(unit_->value, index, (void *)in_);
		}
		else
		{
			sts_struct_list_insert(unit_->value, index + 1, (void *)in_);
		}
		if (unit_->father->control.limit_rows > 0)
		{
			sts_struct_list_limit(unit_->value, unit_->father->control.limit_rows);
		}
		break;
	}
	sts_stepindex_rebuild(unit_->step,
						  sts_collect_unit_get_time(unit_, 0),
						  sts_collect_unit_get_time(unit_, unit_->value->count - 1),
						  unit_->value->count);
	return 1;
}
int sts_collect_unit_update(s_sts_collect_unit *unit_, const char *in_, size_t ilen_)
{
	if (ilen_ < 1)
		return 0;
	int count = 0;

	count = (int)(ilen_ / unit_->value->len);
	// printf("-----count =%d len=%ld:%d\n", count, ilen_, unit_->value->len);
	for (int i = 0; i < count; i++)
	{
		_sts_collect_unit_update(unit_, in_ + i * unit_->value->len);
	}
	return count;
}

sds sts_collect_json_to_struct(s_sts_collect_unit *unit_, const char *in_, size_t ilen_)
{
	// 取最后一条记录的数据
	const char *src = sts_struct_list_get(unit_->value, unit_->value->count - 1);
	sds o = sdsnewlen(src, unit_->value->len);

	s_sts_json_handle *handle = sts_json_load(in_, ilen_);
	if (!handle)
	{
		return NULL;
	}
	s_sts_table *tb = unit_->father;
	s_sts_string_list *field_list = tb->field_name; //取得全部的字段定义

	for (int i = 0; i < sts_string_list_getsize(field_list); i++)
	{
		const char *key = sts_string_list_get(field_list, i);
		s_sts_field_unit *fu = (s_sts_field_unit *)sts_map_buffer_get(tb->field_map, key);
		printf("key=%s size=%d offset=%d\n", key, sts_string_list_getsize(field_list), fu->offset);
		if (!fu)
		{
			continue;
		}
		int8 i8 = 0;
		int16 i16 = 0;
		int32 i32 = 0;
		int64 i64, zoom;
		float f32 = 0.0;
		double f64 = 0.0;
		switch (fu->flags.type)
		{
		case STS_FIELD_STRING:
			src = sts_json_get_str(handle->node, key);
			if (src)
			{
				memmove(o + fu->offset, src, fu->flags.len);
			}
			break;
		case STS_FIELD_INT:
		case STS_FIELD_UINT:
			printf("ptr= %p, name=%s offset=%d\n", ptr, key, fu->offset);
			if (sts_json_find_node(handle->node, key))
			{
				i64 = sts_json_get_int(handle->node, key, 0);
				if (fu->flags.io && fu->flags.zoom > 0)
				{
					zoom = sts_zoom10(fu->flags.zoom);
				}
				if (fu->flags.len == 1)
				{
					i8 = (int8)(i64 / zoom);
					memmove(o + fu->offset, &i8, fu->flags.len);
				}
				else if (fu->flags.len == 2)
				{
					i16 = (int16)(i64 / zoom);
					memmove(o + fu->offset, &i16, fu->flags.len);
				}
				else if (fu->flags.len == 4)
				{
					i32 = (int32)(i64 / zoom);
					memmove(o + fu->offset, &i32, fu->flags.len);
				}
				else
				{
					i64 = (int64)(i64 / zoom);
					memmove(o + fu->offset, &i64, fu->flags.len);
				}
			}
			break;
		case STS_FIELD_FLOAT:
		case STS_FIELD_DOUBLE:
			if (sts_json_find_node(handle->node, key))
			{
				f64 = sts_json_get_double(handle->node, key, 0.0);
				if (!fu->flags.io && fu->flags.zoom > 0)
				{
					zoom = sts_zoom10(fu->flags.zoom);
				}
				if (fu->flags.len == 4)
				{
					i32 = (int32)(f64 * zoom);
					memmove(o + fu->offset, &i32, fu->flags.len);
				}
				else
				{
					i64 = (int64)(f64 * zoom);
					memmove(o + fu->offset, &i64, fu->flags.len);
				}
			}
			break;
		default:
			break;
		}
	}

	sts_json_close(handle);
	return o;
}

////////////////
uint64 sts_table_get_uint(s_sts_field_unit *fu_, const char *val_)
{
	uint64 out = 0;
	uint8 *u8;
	uint16 *u16;
	uint32 *u32;
	uint64 *u64;
	const char *ptr = val_;

	switch (fu_->flags.len)
	{
	case 1:
		u8 = (uint8 *)(ptr + fu_->offset);
		out = *u8;
		break;
	case 2:
		u16 = (uint16 *)(ptr + fu_->offset);
		out = *u16;
		break;
	case 4:
		u32 = (uint32 *)(ptr + fu_->offset);
		out = *u32;
		break;
	case 8:
		u64 = (uint64 *)(ptr + fu_->offset);
		out = *u64;
		break;
	default:
		break;
	}
	if (fu_->flags.io && fu_->flags.zoom > 0)
	{
		out *= sts_zoom10(fu_->flags.zoom);
	}
	return out;
}
int64 sts_table_get_int(s_sts_field_unit *fu_, const char *val_)
{
	int64 out = 0;
	int8 *u8;
	int16 *u16;
	int32 *u32;
	int64 *u64;
	const char *ptr = val_;

	switch (fu_->flags.len)
	{
	case 1:
		u8 = (int8 *)(ptr + fu_->offset);
		out = *u8;
		break;
	case 2:
		u16 = (int16 *)(ptr + fu_->offset);
		out = *u16;
		break;
	case 4:
		u32 = (int32 *)(ptr + fu_->offset);
		out = *u32;
		break;
	case 8:
		u64 = (int64 *)(ptr + fu_->offset);
		out = *u64;
		break;
	default:
		break;
	}
	if (fu_->flags.io && fu_->flags.zoom > 0)
	{
		out *= sts_zoom10(fu_->flags.zoom);
	}
	return out;
}
double sts_table_get_double(s_sts_field_unit *fu_, const char *val_)
{
	double out = 0.0;
	float *f32;
	double *f64;
	const char *ptr = val_;

	switch (fu_->flags.len)
	{
	case 4:
		f32 = (float *)(ptr + fu_->offset);
		out = *f32;
		break;
	case 8:
		f64 = (double *)(ptr + fu_->offset);
		out = *f64;
		break;
	default:
		break;
	}
	if (!fu_->flags.io && fu_->flags.zoom > 0)
	{
		out /= sts_zoom10(fu_->flags.zoom);
	}
	return out;
}

uint64 sts_table_get_times(s_sts_table *tb_, void *val_)
{
	uint64 out = 0;
	int count = sts_string_list_getsize(tb_->field_name);
	for (int i = 0; i < count; i++)
	{
		s_sts_field_unit *fu = (s_sts_field_unit *)sts_map_buffer_get(tb_->field_map, sts_string_list_get(tb_->field_name, i));
		if (!fu)
		{
			continue;
		}
		if (sts_field_is_times(fu->flags.type))
		{
			out = sts_table_get_uint(fu, (const char *)val_);
			break;
		}
	}
	return out;
}

sds sts_collect_struct_filter(s_sts_collect_unit *unit_, sds in_, const char *fields_)
{
	sds out = NULL;

	s_sts_table *tb = unit_->father;
	s_sts_string_list *field_list = sts_string_list_create_r();
	sts_string_list_load(field_list, fields_, strlen(fields_), ",");

	int len = sts_table_get_fields_size(tb);
	int count = (int)(sdslen(in_) / len);
	char *val = in_;
	for (int k = 0; k < count; k++)
	{
		for (int i = 0; i < sts_string_list_getsize(field_list); i++)
		{
			const char *key = sts_string_list_get(field_list, i);
			s_sts_field_unit *fu = sts_table_get_field(tb, key);
			if (!fu)
			{
				continue;
			}
			if (!out)
			{
				out = sdsnewlen(val + fu->offset, fu->flags.len);
			}
			else
			{
				out = sdscatlen(out, val + fu->offset, fu->flags.len);
			}
		}
		val += len;
	}
	sts_string_list_destroy(field_list);
	return out;
}
sds sts_collect_struct_to_json(s_sts_collect_unit *unit_, sds in_, const char *fields_)
{
	sds out = NULL;

	s_sts_table *tb = unit_->father;
	s_sts_string_list *field_list = tb->field_name; //取得全部的字段定义
	if (!sts_check_fields_all(fields_))
	{
		field_list = sts_string_list_create_r();
		sts_string_list_load(field_list, fields_, strlen(fields_), ",");
	}

	char *str;
	s_sts_json_node *jone = sts_json_create_object();
	s_sts_json_node *jtwo = sts_json_create_object();
	s_sts_json_node *jval = NULL;
	s_sts_json_node *jfields = sts_json_create_object();

	// 先处理字段
	
	sts_json_object_add_node(jone, STS_JSON_KEY_FIELDS, jfields);

	int dot = 0; //小数点位数
	int len = sts_table_get_fields_size(tb);
	int count = (int)(sdslen(in_) / len);
	char *val = in_;
	for (int k = 0; k < count; k++)
	{
		sts_out_binary("get", val, 30);
		jval = sts_json_create_array();
		for (int i = 0; i < sts_string_list_getsize(field_list); i++)
		{
			const char *key = sts_string_list_get(field_list, i);
			s_sts_field_unit *fu = sts_table_get_field(tb, key);
			if (!fu)
			{
				sts_json_array_add_string(jval, " ", 1);
				continue;
			}
			const char *ptr = (const char *)simple_val;
			switch (fu->flags.type)
			{
			case STS_FIELD_STRING:
				sts_json_array_add_string(jval, ptr + fu->offset, fu->flags.len);
				break;
			case STS_FIELD_INT:
				printf("ptr= %p, name=%s offset=%d\n", ptr, key, fu->offset);
				sts_json_array_add_int(jval, sts_table_get_int(fu, ptr));
				break;
			case STS_FIELD_UINT:
				sts_json_array_add_uint(jval, sts_table_get_uint(fu, ptr));
				break;
			case STS_FIELD_FLOAT:
			case STS_FIELD_DOUBLE:
				if (!fu->flags.io && fu->flags.zoom > 0)
				{
					dot = fu->flags.zoom;
				}
				sts_json_array_add_double(jval, sts_table_get_double(fu, ptr), dot);
				break;
			default:
				sts_json_array_add_string(jval, "", 0);
				break;
			}
		}
		if (tb->control.limit_rows != 1)
		{
			sts_json_array_add_node(jtwo, jval);
		}
		else
		{
			break;
		}
		val += len;
	}

	if (tb->control.limit_rows != 1)
	{
		sts_json_object_add_node(jone, STS_JSON_KEY_ARRAYS, jtwo);
	}
	else
	{
		sts_json_object_add_node(jone, STS_JSON_KEY_ARRAY, jval);
	}

	size_t ll;
	printf("jone = %s\n", sts_json_output(jone, &ll));
	// 输出数据
	// printf("1112111 [%d]\n",tb->control.limit_rows);

	size_t len;
	str = sts_json_output_zip(jone, &len);
	out = sdsnewlen(str, len);
	sts_free(str);
	sts_json_delete_node(jone);

	if (!sts_check_fields_all(fields_))
	{
		sts_string_list_destroy(field_list);
	}
	return out;
}

sds sts_collect_struct_to_array(s_sts_collect_unit *unit_, sds in_, const char *fields_);
