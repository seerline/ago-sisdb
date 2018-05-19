

#include "sts_collect.h"
#include "sts_fields.h"

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
	if ( si_->count < 1) {
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
	printf("goto %llu\n", si_->step);
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
	unit->stepinfo = sts_stepindex_create();
	unit->value = sts_struct_list_create(sts_table_get_fields_size(tb_), NULL, 0);
	return unit;
}
void sts_collect_unit_destroy(s_sts_collect_unit *unit_)
{
	if (unit_->value)
	{
		sts_struct_list_destroy(unit_->value);
	}
	if (unit_->stepinfo)
	{
		sts_stepindex_destroy(unit_->stepinfo);
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
	sts_stepindex_rebuild(unit_->stepinfo,
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
	int index = stepindex_goto(unit_->stepinfo, index_);
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
	int index = stepindex_goto(unit_->stepinfo, index_);
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

	int index = stepindex_goto(unit_->stepinfo, index_);
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
	return sdsnewlen(sts_struct_list_get(unit_->value, start_), count * unit_->value->len);
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
	sts_stepindex_rebuild(unit_->stepinfo,
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
void _sts_collect_struct(sds in_, s_sts_field_unit *fu_, char *key_, s_sts_json_node *node_)
{
	int8 i8 = 0;
	int16 i16 = 0;
	int32 i32 = 0;
	int64 i64, zoom = 1;
	double f64 = 0.0;
	const char *str;
	switch (fu_->flags.type)
	{
	case STS_FIELD_STRING:
		str = sts_json_get_str(node_, key_);
		if (str)
		{
			memmove(in_ + fu_->offset, str, fu_->flags.len);
		}
		break;
	case STS_FIELD_INT:
	case STS_FIELD_UINT:
		if (sts_json_find_node(node_, key_))
		{
			i64 = sts_json_get_int(node_, key_, 0);
			printf("name=%s offset=%d : %d ii=%lld\n", key_, fu_->offset, fu_->flags.len, i64);
			if (fu_->flags.io && fu_->flags.zoom > 0)
			{
				zoom = sts_zoom10(fu_->flags.zoom);
			}
			if (fu_->flags.len == 1)
			{
				i8 = (int8)(i64 / zoom);
				memmove(in_ + fu_->offset, &i8, fu_->flags.len);
			}
			else if (fu_->flags.len == 2)
			{
				i16 = (int16)(i64 / zoom);
				memmove(in_ + fu_->offset, &i16, fu_->flags.len);
			}
			else if (fu_->flags.len == 4)
			{
				i32 = (int32)(i64 / zoom);
				memmove(in_ + fu_->offset, &i32, fu_->flags.len);
			}
			else
			{
				i64 = (int64)(i64 / zoom);
				memmove(in_ + fu_->offset, &i64, fu_->flags.len);
			}
		}
		break;
	case STS_FIELD_FLOAT:
	case STS_FIELD_DOUBLE:
		if (sts_json_find_node(node_, key_))
		{
			f64 = sts_json_get_double(node_, key_, 0.0);
			if (!fu_->flags.io && fu_->flags.zoom > 0)
			{
				zoom = sts_zoom10(fu_->flags.zoom);
			}
			if (fu_->flags.len == 4)
			{
				i32 = (int32)(f64 * zoom);
				memmove(in_ + fu_->offset, &i32, fu_->flags.len);
			}
			else
			{
				i64 = (int64)(f64 * zoom);
				memmove(in_ + fu_->offset, &i64, fu_->flags.len);
			}
		}
		break;
	default:
		break;
	}

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

	int fields = sts_string_list_getsize(tb->field_name);

	for (int k = 0; k < fields; k++)
	{
		const char *key = sts_string_list_get(tb->field_name, k);
		s_sts_field_unit *fu = (s_sts_field_unit *)sts_map_buffer_get(tb->field_map, key);
		printf("key=%s size=%d offset=%d\n", key, fields, fu->offset);
		if (!fu)
		{
			continue;
		}

		_sts_collect_struct(o, fu, (char *)key, handle->node);
	}

	sts_json_close(handle);
	return o;
}

sds sts_collect_array_to_struct(s_sts_collect_unit *unit_, const char *in_, size_t ilen_)
{
	// 字段个数一定要一样
	s_sts_json_handle *handle = sts_json_load(in_, ilen_);
	if (!handle)
	{
		return NULL;
	}
	s_sts_table *tb = unit_->father;
	// 获取字段个数
	int fields = sts_string_list_getsize(tb->field_name);
	s_sts_json_node *jval;

	int count = 0;
	if (handle->node->child->type == STS_JSON_ARRAY)
	{ // 表示二维数组
		jval = handle->node->child;
		count = sts_json_get_size(handle->node);
	}
	else
	{
		count = 1;
		jval = handle->node;
	}
	if (count<1) return NULL;
	
	sds o = sdsnewlen(NULL, count * unit_->value->len);
	int index = 0;
	while (jval)
	{
		int size = sts_json_get_size(jval);
		if (size != fields)
		{
			sts_out_error(3)("input fields[%d] count error [%d].\n", size, fields);
			jval = jval->next;
			continue;
		}

		for (int k = 0; k < fields; k++)
		{
			const char *fname = sts_string_list_get(tb->field_name, k);
			s_sts_field_unit *fu = (s_sts_field_unit *)sts_map_buffer_get(tb->field_map, fname);
			printf("key=%s size=%d offset=%d\n", fname, fields, fu->offset);
			if (!fu)
			{
				continue;
			}
			char key[16];
			sts_sprintf(key, 10, "%d", k);
			_sts_collect_struct(o + index * unit_->value->len, fu, key, jval);
		}
		index++;
		jval = jval->next;
	}

	sts_json_close(handle);
	return o;
}
////////////////

sds sts_collect_struct_filter(s_sts_collect_unit *unit_, sds in_, const char *fields_)
{
	sds out = NULL;

	s_sts_table *tb = unit_->father;
	s_sts_string_list *field_list = sts_string_list_create_w();
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
		field_list = sts_string_list_create_w();
		sts_string_list_load(field_list, fields_, strlen(fields_), ",");
	}

	char *str;
	s_sts_json_node *jone = sts_json_create_object();
	s_sts_json_node *jtwo = sts_json_create_array();
	s_sts_json_node *jval = NULL;
	s_sts_json_node *jfields = sts_json_create_object();

	// 先处理字段
	for (int i = 0; i < sts_string_list_getsize(field_list); i++)
	{
		const char *key = sts_string_list_get(field_list, i);
		sts_json_object_add_uint(jfields, key, i);
	}
	sts_json_object_add_node(jone, STS_JSON_KEY_FIELDS, jfields);

	printf("========%s rows=%d\n", tb->name, tb->control.limit_rows);

	int dot = 0; //小数点位数
	int skip_len = sts_table_get_fields_size(tb);
	int count = (int)(sdslen(in_) / skip_len);
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
			const char *ptr = (const char *)val;
			switch (fu->flags.type)
			{
			case STS_FIELD_STRING:
				sts_json_array_add_string(jval, ptr + fu->offset, fu->flags.len);
				break;
			case STS_FIELD_INT:
				printf("ptr= %p, name=%s offset=%d\n", ptr, key, fu->offset);
				sts_json_array_add_int(jval, sts_fields_get_int(fu, ptr));
				break;
			case STS_FIELD_UINT:
				sts_json_array_add_uint(jval, sts_fields_get_uint(fu, ptr));
				break;
			case STS_FIELD_FLOAT:
			case STS_FIELD_DOUBLE:
				if (!fu->flags.io && fu->flags.zoom > 0)
				{
					dot = fu->flags.zoom;
				}
				sts_json_array_add_double(jval, sts_fields_get_double(fu, ptr), dot);
				break;
			default:
				sts_json_array_add_string(jval, " ", 1);
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
		val += skip_len;
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

	size_t olen;
	str = sts_json_output_zip(jone, &olen);
	out = sdsnewlen(str, olen);
	sts_free(str);
	sts_json_delete_node(jone);

	if (!sts_check_fields_all(fields_))
	{
		sts_string_list_destroy(field_list);
	}
	return out;
}

sds sts_collect_struct_to_array(s_sts_collect_unit *unit_, sds in_, const char *fields_)
{
	sds out = NULL;

	s_sts_table *tb = unit_->father;
	s_sts_string_list *field_list = tb->field_name; //取得全部的字段定义
	if (!sts_check_fields_all(fields_))
	{
		field_list = sts_string_list_create_w();
		sts_string_list_load(field_list, fields_, strlen(fields_), ",");
	}

	char *str;
	s_sts_json_node *jone = sts_json_create_array();
	s_sts_json_node *jval = NULL;

	int dot = 0; //小数点位数
	int skip_len = sts_table_get_fields_size(tb);
	int count = (int)(sdslen(in_) / skip_len);
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
			const char *ptr = (const char *)val;
			switch (fu->flags.type)
			{
			case STS_FIELD_STRING:
				sts_json_array_add_string(jval, ptr + fu->offset, fu->flags.len);
				break;
			case STS_FIELD_INT:
				printf("ptr= %p, name=%s offset=%d\n", ptr, key, fu->offset);
				sts_json_array_add_int(jval, sts_fields_get_int(fu, ptr));
				break;
			case STS_FIELD_UINT:
				sts_json_array_add_uint(jval, sts_fields_get_uint(fu, ptr));
				break;
			case STS_FIELD_FLOAT:
			case STS_FIELD_DOUBLE:
				if (!fu->flags.io && fu->flags.zoom > 0)
				{
					dot = fu->flags.zoom;
				}
				sts_json_array_add_double(jval, sts_fields_get_double(fu, ptr), dot);
				break;
			default:
				sts_json_array_add_string(jval, " ", 1);
				break;
			}
		}
		sts_json_array_add_node(jone, jval);
		if (tb->control.limit_rows == 1)
		{
			break;
		}
		val += skip_len;
	}
	size_t olen;

	if (tb->control.limit_rows != 1)
	{
		printf("jone = %s\n", sts_json_output(jone, &olen));
		str = sts_json_output_zip(jone, &olen);
	}
	else
	{
		printf("jval = %s\n", sts_json_output(jval, &olen));
		str = sts_json_output_zip(jval, &olen);
	}
	// 输出数据
	// printf("1112111 [%d]\n",tb->control.limit_rows);
	out = sdsnewlen(str, olen);
	sts_free(str);
	sts_json_delete_node(jone);

	if (!sts_check_fields_all(fields_))
	{
		sts_string_list_destroy(field_list);
	}
	return out;
}
