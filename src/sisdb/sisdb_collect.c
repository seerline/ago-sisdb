

#include <sisdb_collect.h>
#include <sisdb_fields.h>

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_step_index --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sis_step_index *sis_stepindex_create()
{
	s_sis_step_index *si = sis_malloc(sizeof(s_sis_step_index));
	memset(si, 0, sizeof(s_sis_step_index));
	si->left = 0;
	si->right = 0;
	si->count = 0;
	si->step = 0.0;

	return si;
}
void sis_stepindex_destroy(s_sis_step_index *si_)
{
	sis_free(si_);
}
void sis_stepindex_rebuild(s_sis_step_index *si_, uint64 left_, uint64 right_, int count_)
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
int stepindex_goto(s_sis_step_index *si_, uint64 curr_)
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
	if (si_->step > 0.000001)
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
//------------------------s_sis_collect_unit --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sis_collect_unit *sis_collect_unit_create(s_sis_table *tb_, const char *key_)
{
	s_sis_collect_unit *unit = sis_malloc(sizeof(s_sis_collect_unit));
	memset(unit, 0, sizeof(s_sis_collect_unit));
	unit->father = tb_;

	unit->stepinfo = sis_stepindex_create();
	int size = sis_table_get_fields_size(tb_);
	unit->value = sis_struct_list_create(size, NULL, 0);

	if (tb_->catch)
	{
		unit->front = sis_sdsnewlen(NULL, size);
		unit->lasted = sis_sdsnewlen(NULL, size);
	}
	if (tb_->zip)
	{
		unit->refer = sis_sdsnewlen(NULL, size);
	}

	return unit;
}
void sis_collect_unit_destroy(s_sis_collect_unit *unit_)
{
	// printf("delete in_collect = %p\n",unit_);
	if (unit_->value)
	{
		sis_struct_list_destroy(unit_->value);
	}
	if (unit_->stepinfo)
	{
		sis_stepindex_destroy(unit_->stepinfo);
	}
	if (unit_->father->catch)
	{
		sis_sdsfree(unit_->front);
		sis_sdsfree(unit_->lasted);
	}
	if (unit_->father->zip)
	{
		sis_sdsfree(unit_->refer);
	}
	sis_free(unit_);
}

uint64 sis_collect_unit_get_time(s_sis_collect_unit *unit_, int index_)
{

	uint64 tt = 0;
	void *val = sis_struct_list_get(unit_->value, index_);
	if (val)
	{
		tt = sis_fields_get_uint_from_key(unit_->father, "time", val);
	}
	return tt;
}
////////////////////////
//  delete
////////////////////////

int _sis_collect_unit_delete(s_sis_collect_unit *unit_, int start_, int count_)
{

	sis_struct_list_delete(unit_->value, start_, count_);
	sis_stepindex_rebuild(unit_->stepinfo,
						  sis_collect_unit_get_time(unit_, 0),
						  sis_collect_unit_get_time(unit_, unit_->value->count - 1),
						  unit_->value->count);
	return 0;
}
int sis_collect_unit_recs(s_sis_collect_unit *unit_)
{
	return unit_->value->count;
}

int sis_collect_unit_search_left(s_sis_collect_unit *unit_, uint64 finder_, int *mode_)
{
	*mode_ = SIS_SEARCH_NONE;
	int index = stepindex_goto(unit_->stepinfo, finder_);
	if (index < 0)
	{
		return -1;
	}
	int i = index;
	int dir = 0;
	while (i >= 0 && i < unit_->value->count)
	{
		uint64 ts = sis_collect_unit_get_time(unit_, i);
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
int sis_collect_unit_search_right(s_sis_collect_unit *unit_, uint64 finder_, int *mode_)
{
	*mode_ = SIS_SEARCH_NONE;
	int index = stepindex_goto(unit_->stepinfo, finder_);
	if (index < 0)
	{
		return -1;
	}
	int i = index;
	int dir = 0;
	while (i >= 0 && i < unit_->value->count)
	{
		uint64 ts = sis_collect_unit_get_time(unit_, i);
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
// 必须找到一个相等值，否则返回-1
int sis_collect_unit_search(s_sis_collect_unit *unit_, uint64 finder_)
{
	int index = stepindex_goto(unit_->stepinfo, finder_);
	if (index < 0)
	{
		return -1;
	}
	int i = index;
	int dir = 0;
	while (i >= 0 && i < unit_->value->count)
	{
		uint64 ts = sis_collect_unit_get_time(unit_, i);
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
int sis_collect_unit_search_check(s_sis_collect_unit *unit_, uint64 finder_)
{
	if (unit_->value->count < 1)
	{
		return SIS_SEARCH_CHECK_INIT;
	}

	uint64 ts = sis_collect_unit_get_time(unit_, unit_->value->count - 1);
	if (finder_ > ts)
	{
		if (sis_time_get_idate(finder_) > sis_time_get_idate(ts))
		{
			return SIS_SEARCH_CHECK_INIT;
		}
		else
		{
			return SIS_SEARCH_CHECK_NEW;
		}
	}
	else if (finder_ < ts)
	{
		// 应该判断量，如果量小就返回错误
		return SIS_SEARCH_CHECK_OLD;
	}
	else
	{
		return SIS_SEARCH_CHECK_OK;
	}
}

bool sis_trans_of_range(s_sis_collect_unit *unit_, int *start_, int *stop_)
{
	int llen = sis_collect_unit_recs(unit_);

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
int sis_collect_unit_delete_of_range(s_sis_collect_unit *unit_, int start_, int stop_)
{
	// int llen, count;

	// llen = sis_collect_unit_recs(unit_);

	// if (start_ < 0)
	// 	start_ = llen + start_;
	// if (stop_ < 0)
	// 	stop_ = llen + stop_;
	// if (start_ < 0)
	// 	start_ = 0;

	// if (start_ > stop_ || start_ >= llen)
	// {
	// 	return 0;
	// }
	// if (stop_ >= llen)
	// 	stop_ = llen - 1;
	bool o = sis_trans_of_range(unit_, &start_, &stop_);
	if (!o)
		return 0;
	int count = (stop_ - start_) + 1;
	return _sis_collect_unit_delete(unit_, start_, count);
}
bool sis_trans_of_count(s_sis_collect_unit *unit_, int *start_, int *count_)
{
	if (*count_ <= 0)
	{
		return false;
	}
	int llen = sis_collect_unit_recs(unit_);

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
int sis_collect_unit_delete_of_count(s_sis_collect_unit *unit_, int start_, int count_)
{
	bool o = sis_trans_of_count(unit_, &start_, &count_);
	if (!o)
		return 0;
	// int llen;
	// llen = sis_collect_unit_recs(unit_);

	// if (start_ < 0)
	// 	start_ = llen + start_;
	// if (start_ < 0)
	// 	start_ = 0;

	// if (start_ >= llen)
	// {
	// 	return 0;
	// }
	// if (start_ + count_ > llen)
	// 	count_ = llen - start_;

	return _sis_collect_unit_delete(unit_, start_, count_);
}

//////////////////////////////////////////////////////
//  get  --  先默认二进制格式，全部字段返回数据
//////////////////////////////////////////////////////

s_sis_sds sis_collect_unit_get_of_count_sds(s_sis_collect_unit *unit_, int start_, int count_)
{
	bool o = sis_trans_of_count(unit_, &start_, &count_);
	if (!o)
		return NULL;
	// if (count_ == 0)
	// {
	// 	return NULL;
	// }
	// int llen = sis_collect_unit_recs(unit_);

	// if (start_ < 0)
	// {
	// 	start_ = llen + start_;
	// }
	// if (start_ < 0)
	// {
	// 	start_ = 0;
	// }
	// if (count_ < 0)
	// {
	// 	count_ = abs(count_);
	// 	if (count_ > (start_ + 1))
	// 	{
	// 		count_ = start_ + 1;
	// 	}
	// 	start_ -= (count_ - 1);
	// }
	// else
	// {
	// 	if ((start_ + count_) > llen)
	// 	{
	// 		count_ = llen - start_;
	// 	}
	// }
	return sis_sdsnewlen(sis_struct_list_get(unit_->value, start_), count_ * unit_->value->len);
}
s_sis_sds sis_table_get_of_range_sds(s_sis_table *tb_, const char *code_, int start_, int stop_)
{
	s_sis_collect_unit *collect = sis_map_buffer_get(tb_->collect_map, code_);
	if (!collect)
	{
		return 0;
	}
	return sis_collect_unit_get_of_range_sds(collect, start_, stop_);
}
s_sis_sds sis_collect_unit_get_of_range_sds(s_sis_collect_unit *unit_, int start_, int stop_)
{
	bool o = sis_trans_of_range(unit_, &start_, &stop_);
	if (!o)
		return NULL;

	// int llen = sis_collect_unit_recs(unit_);

	// if (start_ < 0)
	// {
	// 	start_ = llen + start_;
	// }
	// if (stop_ < 0)
	// {
	// 	stop_ = llen + stop_;
	// }
	// if (start_ < 0)
	// {
	// 	start_ = 0;
	// }

	// if (start_ > stop_ || start_ >= llen)
	// {
	// 	return NULL;
	// }
	// if (stop_ >= llen)
	// {
	// 	stop_ = llen - 1;
	// }

	int count = (stop_ - start_) + 1;
	return sis_sdsnewlen(sis_struct_list_get(unit_->value, start_), count * unit_->value->len);
}
////////////////////////
//  update
////////////////////////
s_sis_sds sis_make_catch_inited_sds(s_sis_collect_unit *unit_, const char *in_)
{
	s_sis_table *tb = unit_->father;
	s_sis_sds o = sis_sdsnewlen(in_, sis_sdslen(unit_->front));
	int fields = sis_string_list_getsize(tb->field_name);
	for (int k = 0; k < fields; k++)
	{
		const char *key = sis_string_list_get(tb->field_name, k);
		s_sis_field_unit *fu = (s_sis_field_unit *)sis_map_buffer_get(tb->field_map, key);
		if (!fu || fu->catch_method == SIS_FIELD_METHOD_COVER)
		{
			continue;
		}
		uint64 u64 = 0;
		// printf("fixed--key=%s size=%d offset=%d, method=%d\n", key, fields, fu->offset,fu->catch_method);
		if (fu->catch_method == SIS_FIELD_METHOD_INCR)
		{
			// printf("----inited incr key=%s new=%lld front=%lld\n", key, sis_fields_get_uint(fu,in_), sis_fields_get_uint(fu,unit_->front));
			u64 = sis_fields_get_uint(fu, in_) - sis_fields_get_uint(fu, unit_->front);
		}
		else
		{ //SIS_FIELD_METHOD_INIT&SIS_FIELD_METHOD_MIN&SIS_FIELD_METHOD_MAX
			s_sis_field_unit *sfu = (s_sis_field_unit *)sis_map_buffer_get(tb->field_map, fu->catch_initfield);
			if (sfu)
			{
				u64 = sis_fields_get_uint(sfu, in_);
			}
		}
		sis_fields_set_uint(fu, o, u64);
	}
	// printf("o=%lld\n",sis_fields_get_uint_from_key(tb,"time",o));
	// printf("in_=%lld\n",sis_fields_get_uint_from_key(tb,"time",in_));
	return o;
}
s_sis_sds sis_make_catch_moved_sds(s_sis_collect_unit *unit_, const char *in_)
{
	s_sis_table *tb = unit_->father;
	// 获取最后一条记录
	s_sis_sds moved = sis_struct_list_last(unit_->value);

	// 保存源数据
	s_sis_sds o = sis_sdsnewlen(in_, sis_sdslen(unit_->front));
	int fields = sis_string_list_getsize(tb->field_name);
	for (int k = 0; k < fields; k++)
	{
		const char *key = sis_string_list_get(tb->field_name, k);
		s_sis_field_unit *fu = (s_sis_field_unit *)sis_map_buffer_get(tb->field_map, key);
		// printf("fixed--key=%s size=%d offset=%d\n", key, fields, fu->offset);
		if (!fu || fu->catch_method == SIS_FIELD_METHOD_COVER)
		{
			continue;
		}
		// in_.vol - front.vol
		// in_.money - front.money
		// in.close = in_.close
		uint64 u64, u64_front, u64_moved, u64_inited;
		// close 的字段

		if (fu->catch_method == SIS_FIELD_METHOD_INCR)
		{
			printf("---- incr key=%s new=%lld front=%lld\n", key, sis_fields_get_uint(fu, in_), sis_fields_get_uint(fu, unit_->front));
			u64 = sis_fields_get_uint(fu, in_) - sis_fields_get_uint(fu, unit_->front);
		}
		else if (fu->catch_method == SIS_FIELD_METHOD_MAX)
		{
			// in_.high > front.high || !front.high => in.high = in_.high
			//     else in_.close > moved.high => in.high = in_.close
			//		       else in.high = moved.high
			u64 = sis_fields_get_uint(fu, in_);
			u64_front = sis_fields_get_uint(fu, unit_->front);
			u64_moved = sis_fields_get_uint(fu, moved);
			s_sis_field_unit *sfu = (s_sis_field_unit *)sis_map_buffer_get(tb->field_map, fu->catch_initfield);
			u64_inited = sis_fields_get_uint(sfu, in_);
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
		else if (fu->catch_method == SIS_FIELD_METHOD_MIN)
		{
			// in_.low < front.low || !front.low => in.low = in_.low
			//		else in_.close < moved.low => in.low = in_.close
			//		       else in.low = moved.low
			u64 = sis_fields_get_uint(fu, in_);
			u64_front = sis_fields_get_uint(fu, unit_->front);
			u64_moved = sis_fields_get_uint(fu, moved);
			s_sis_field_unit *sfu = (s_sis_field_unit *)sis_map_buffer_get(tb->field_map, fu->catch_initfield);
			u64_inited = sis_fields_get_uint(sfu, in_);
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
		{ //SIS_FIELD_METHOD_INIT
			// 恢复open
			u64 = sis_fields_get_uint(fu, moved);
			if (u64 == 0) // 如果是开始第一笔，就直接取对应字段值
			{
				u64 = sis_fields_get_uint(fu, in_);
			}
		}
		sis_fields_set_uint(fu, o, u64);
	}

	return o;
}
// int _sis_collect_unit_update_one(s_sis_collect_unit *unit_, const char *in_)
// {
// 	s_sis_table *tb = unit_->father;

// 	uint64 tt;
// 	uint64 vol;
// 	switch (tb->control.insert_mode)
// 	{
// 	case SIS_OPTION_ALWAYS:
// 		if (tb->control.limit_rows == 1)
// 		{
// 			if (sis_struct_list_update(unit_->value, 0, (void *)in_) < 0)
// 			{
// 				sis_struct_list_push(unit_->value, (void *)in_);
// 			}
// 		}
// 		else
// 		{
// 			sis_struct_list_push(unit_->value, (void *)in_);
// 			if (tb->control.limit_rows > 1)
// 			{
// 				sis_struct_list_limit(unit_->value, tb->control.limit_rows);
// 			}
// 		}
// 		break;
// 	case SIS_OPTION_MUL_CHECK:
// 		break;
// 	case SIS_OPTION_VOL:
// 		//
// 		vol = sis_fields_get_uint_from_key(tb, "vol", in_); // 得到成交量序列值
// 		if(vol == 0) // 开市前没有成交的判断
// 		{
// 			break;
// 		}
// 	case SIS_OPTION_TIME:
// 	default:
// 		tt = sis_fields_get_uint_from_key(tb, "time", in_); // 得到时间序列值
// 		int offset = sis_field_get_offset(tb, "time");
// 		int index = unit_->value->count - 1;
// 		int mode = sis_collect_unit_search_check(unit_, tt);
// 		int size = sis_table_get_fields_size(tb);
// 		// printf("mode=%d tt= %lld index=%d\n", mode, tt, index);
// 		// sis_out_binary("update", in_, size);

// 		if (mode == SIS_SEARCH_CHECK_OLD) {
// 			// 时间是很早以前的数据，那就重新定位数据
// 			int set = SIS_SEARCH_NONE;
// 			index = sis_collect_unit_search_right(unit_, tt, &set);
// 			// printf("mode=%d set=%d tt= %lld index=%d\n", mode, set, tt, index);

// 			if (set == SIS_SEARCH_OK) {
// 				sis_struct_list_update(unit_->value, index, (void *)in_);
// 			}
// 			else
// 			{
// 				sis_struct_list_insert(unit_->value, index, (void *)in_);
// 			}
// 			// printf("count=%d\n",unit_->value->count);
// 		}
// 		// printf("----=%d tt= %lld index=%d\n", mode, tt, index);
// 		else if (mode == SIS_SEARCH_CHECK_INIT)
// 		{
// 			// 1. 初始化
// 			if (tb->catch)
// 			{
// 				sis_fields_copy(unit_->front, NULL, size); // 保存的是全量，成交量使用
// 				sis_fields_copy(unit_->lasted, in_, size); //
// 			}
// 			// 2. 写入数据
// 			sis_struct_list_push(unit_->value, (void *)in_);
// 			// sis_out_binary("set push", in_, size);
// 		}
// 		else if (mode == SIS_SEARCH_CHECK_NEW)
// 		{
// 			// vol1=vol2,求和vol1的差值
// 			if (tb->catch)
// 			{
// 				if (memcmp(unit_->lasted+offset, in_+offset, size - offset))
// 				{
// 					sis_fields_copy(unit_->front, unit_->lasted, size);
// 				// -- 先根据in -- tb->front生成新数据
// 					s_sis_sds in = sis_make_catch_inited_sds(unit_, in_);
// 				// in_.vol - front.vol
// 				// in_.money - front.money
// 				// in.open = in_.close;
// 				// in_.high = in_.close;
// 				// in_.low = in_.close;
// 				// in.close = in_.close
// 				// -- 再覆盖老数据
// 					sis_struct_list_push(unit_->value, (void *)in);
// 					sis_fields_copy(unit_->lasted, in_, size);
// 				}
// 				else
// 				{
// 					return 1;
// 				}
// 			}
// 			else
// 			{
// 				sis_struct_list_push(unit_->value, (void *)in_);
// 			}
// 		}
// 		else
// 		// if (mode == SIS_SEARCH_CHECK_OK)
// 		{
// 			//如果发现记录，保存原始值到lasted，然后计算出实际要写入的值
// 			// 1. 初始化
// 			if (tb->catch)
// 			{
// 				// -- 先根据in -- tb->front生成新数据
// 				if (memcmp(unit_->lasted+offset, in_+offset, size - offset))
// 				{
// 					s_sis_sds in = sis_make_catch_moved_sds(unit_, in_);
// 					sis_struct_list_update(unit_->value, index, (void *)in);
// 					sis_fields_copy(unit_->lasted, in_, size);
// 				} else {
// 					return 1;
// 				}
// 			}
// 			else
// 			{
// 				sis_struct_list_update(unit_->value, index, (void *)in_);
// 			}
// 		}

// 		if (tb->control.limit_rows > 0)
// 		{
// 			sis_struct_list_limit(unit_->value, tb->control.limit_rows);
// 		}
// 		break;
// 	}
// 	sis_stepindex_rebuild(unit_->stepinfo,
// 						  sis_collect_unit_get_time(unit_, 0),
// 						  sis_collect_unit_get_time(unit_, unit_->value->count - 1),
// 						  unit_->value->count);
// 	return 1;
// }
int _sis_collect_update_one(s_sis_collect_unit *unit_, const char *in_)
{
	s_sis_table *tb = unit_->father;

	if (tb->control.insert_mode == SIS_OPTION_ALWAYS)
	{
		sis_struct_list_push(unit_->value, (void *)in_);
		return 1;
	}
	if (tb->control.insert_mode & SIS_OPTION_VOL)
	{
		uint64 vol = sis_fields_get_uint_from_key(tb, "vol", in_); // 得到成交量序列值
		if (vol == 0)											   // 开市前没有成交的直接返回
		{
			return 0;
		}
		if (tb->catch)
		{
			uint64 last_vol = sis_fields_get_uint_from_key(tb, "vol", unit_->lasted);
			if (vol <= last_vol)
			{
				return 0;
			}
		}
	}
	if (tb->control.insert_mode & SIS_OPTION_CODE)
	{
		// 先取code
		// 检索code的记录
		// 如果全部记录都没有该code，就设置ok，否则就no
	}

	if (tb->control.insert_mode & SIS_OPTION_TIME || tb->control.insert_mode & SIS_OPTION_SORT)
	{
		uint64 tt = sis_fields_get_uint_from_key(tb, "time", in_); // 得到时间序列值
		int offset = sis_field_get_offset(tb, "time");
		int index = unit_->value->count - 1;
		// 检查待插入的时间位置
		int search_mode = sis_collect_unit_search_check(unit_, tt);
		int size = sis_table_get_fields_size(tb);
		// printf("mode=%d tt= %lld index=%d\n", mode, tt, index);
		// sis_out_binary("update", in_, size);

		if (search_mode == SIS_SEARCH_CHECK_OLD)
		{
			// 时间是很早以前的数据，那就重新定位数据
			int set = SIS_SEARCH_NONE;
			index = sis_collect_unit_search_right(unit_, tt, &set);
			if (set == SIS_SEARCH_OK)
			{
				if (tb->control.insert_mode & SIS_OPTION_SORT)
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
		else if (search_mode == SIS_SEARCH_CHECK_INIT)
		{
			// 1. 初始化
			if (tb->catch)
			{
				sis_fields_copy(unit_->front, NULL, size); // 保存的是全量，成交量使用
				sis_fields_copy(unit_->lasted, in_, size); //
			}
			// 2. 写入数据
			sis_struct_list_push(unit_->value, (void *)in_);
			// sis_out_binary("set push", in_, size);
		}
		else if (search_mode == SIS_SEARCH_CHECK_NEW)
		{
			if (tb->catch)
			{
				if (memcmp(unit_->lasted + offset, in_ + offset, size - offset))
				{
					sis_fields_copy(unit_->front, unit_->lasted, size);
					// -- 先根据in -- tb->front生成新数据
					s_sis_sds in = sis_make_catch_inited_sds(unit_, in_);
					// -- 再覆盖老数据
					sis_struct_list_push(unit_->value, (void *)in);
					sis_fields_copy(unit_->lasted, in_, size);
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
		// if (mode == SIS_SEARCH_CHECK_OK)
		{
			//如果发现记录，保存原始值到lasted，然后计算出实际要写入的值
			// 1. 初始化
			if (tb->catch)
			{
				// -- 先根据in -- tb->front生成新数据
				if (memcmp(unit_->lasted + offset, in_ + offset, size - offset))
				{
					s_sis_sds in = sis_make_catch_moved_sds(unit_, in_);
					if (tb->control.insert_mode & SIS_OPTION_SORT)
					{
						sis_struct_list_push(unit_->value, (void *)in_);
					}
					else
					{
						sis_struct_list_update(unit_->value, index, (void *)in_);
					}
					// sis_struct_list_update(unit_->value, index, (void *)in);
					sis_fields_copy(unit_->lasted, in_, size);
					sis_sdsfree(in);
				}
				else
				{
					return 0;
				}
			}
			else
			{
				if (tb->control.insert_mode & SIS_OPTION_SORT)
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
int sis_collect_unit_update(s_sis_collect_unit *unit_, const char *in_, size_t ilen_)
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
		sis_out_error(3)("source format error [%d*%d!=%lu]\n", count, unit_->value->len, ilen_);
		return 0;
	}
	// printf("-[%s]----count =%d len=%ld:%d\n", unit_->father->name, count, ilen_, unit_->value->len);
	const char *ptr = in_;
	for (int i = 0; i < count; i++)
	{
		_sis_collect_update_one(unit_, ptr);
		ptr += unit_->value->len;
	}
	// 处理记录个数

	s_sis_table *tb = unit_->father;
	if (tb->control.limit_rows > 0)
	{
		sis_struct_list_limit(unit_->value, tb->control.limit_rows);
	}
	// 重建索引
	sis_stepindex_rebuild(unit_->stepinfo,
						  sis_collect_unit_get_time(unit_, 0),
						  sis_collect_unit_get_time(unit_, unit_->value->count - 1),
						  unit_->value->count);

	return count;
}
int sis_collect_unit_update_block(s_sis_collect_unit *unit_, const char *in_, size_t ilen_)
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
		sis_out_error(3)("source format error [%d*%d!=%lu]\n", count, unit_->value->len, ilen_);
		return 0;
	}
	// printf("-[%s]----count =%d len=%ld:%d\n", unit_->father->name, count, ilen_, unit_->value->len);
	for (int i = 0; i < count; i++)
	{
		sis_struct_list_push(unit_->value, (void *)(in_ + i * unit_->value->len));
	}

	sis_stepindex_rebuild(unit_->stepinfo,
						  sis_collect_unit_get_time(unit_, 0),
						  sis_collect_unit_get_time(unit_, unit_->value->count - 1),
						  unit_->value->count);
	return count;
}
void _sis_fields_json_to_struct(s_sis_sds in_, s_sis_field_unit *fu_, char *key_, s_sis_json_node *node_)
{
	int8 i8 = 0;
	int16 i16 = 0;
	int32 i32 = 0;
	int64 i64, zoom = 1;
	double f64 = 0.0;
	const char *str;
	switch (fu_->flags.type)
	{
	case SIS_FIELD_STRING:
		str = sis_json_get_str(node_, key_);
		if (str)
		{
			memmove(in_ + fu_->offset, str, fu_->flags.len);
		}
		break;
	case SIS_FIELD_INT:
	case SIS_FIELD_UINT:
		if (sis_json_find_node(node_, key_))
		{
			i64 = sis_json_get_int(node_, key_, 0);
			// printf("name=%s offset=%d : %d ii=%lld\n", key_, fu_->offset, fu_->flags.len, i64);
			if (fu_->flags.io && fu_->flags.zoom > 0)
			{
				zoom = sis_zoom10(fu_->flags.zoom);
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
	case SIS_FIELD_DOUBLE:
		if (sis_json_find_node(node_, key_))
		{
			f64 = sis_json_get_double(node_, key_, 0.0);
			if (!fu_->flags.io && fu_->flags.zoom > 0)
			{
				zoom = sis_zoom10(fu_->flags.zoom);
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

void sis_collect_struct_trans(s_sis_sds ins_, s_sis_field_unit *infu_, s_sis_table *indb_,
							  s_sis_sds outs_, s_sis_field_unit *outfu_, s_sis_table *outdb_)
{
	int8 i8 = 0;
	int16 i16 = 0;
	int32 i32 = 0;
	int64 i64, zoom = 1;
	double f64 = 0.0;

	switch (infu_->flags.type)
	{
	case SIS_FIELD_STRING:
		memmove(outs_ + outfu_->offset, ins_ + infu_->offset, outfu_->flags.len);
		break;
	case SIS_FIELD_INT:
	case SIS_FIELD_UINT:
		i64 = sis_fields_get_int(infu_, ins_);

		if (outfu_->flags.io && outfu_->flags.zoom > 0)
		{
			zoom = sis_zoom10(outfu_->flags.zoom);
		}
		// 对时间进行转换
		if (sis_field_is_time(infu_))
		{
			i64 = (int64)sis_table_struct_trans_time(i64, indb_->control.time_scale,
													 outdb_, outdb_->control.time_scale);
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
	case SIS_FIELD_DOUBLE:
		f64 = sis_fields_get_double(infu_, ins_);
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
s_sis_sds sis_collect_json_to_struct_sds(s_sis_collect_unit *unit_, const char *in_, size_t ilen_)
{
	// 取最后一条记录的数据
	const char *src = sis_struct_list_last(unit_->value);
	s_sis_sds o = sis_sdsnewlen(src, unit_->value->len);

	s_sis_json_handle *handle = sis_json_load(in_, ilen_);
	if (!handle)
	{
		return NULL;
	}

	s_sis_table *tb = unit_->father;

	int fields = sis_string_list_getsize(tb->field_name);

	for (int k = 0; k < fields; k++)
	{
		const char *key = sis_string_list_get(tb->field_name, k);
		s_sis_field_unit *fu = (s_sis_field_unit *)sis_map_buffer_get(tb->field_map, key);
		if (!fu)
		{
			continue;
		}
		printf("key=%s size=%d offset=%d\n", key, fields, fu->offset);

		_sis_fields_json_to_struct(o, fu, (char *)key, handle->node);
	}

	sis_json_close(handle);
	return o;
}

s_sis_sds sis_collect_array_to_struct_sds(s_sis_collect_unit *unit_, const char *in_, size_t ilen_)
{
	// 字段个数一定要一样
	s_sis_json_handle *handle = sis_json_load(in_, ilen_);
	if (!handle)
	{
		return NULL;
	}
	s_sis_table *tb = unit_->father;
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
			sis_out_error(3)("input fields[%d] count error [%d].\n", size, fields);
			jval = jval->next;
			continue;
		}

		for (int k = 0; k < fields; k++)
		{
			const char *fname = sis_string_list_get(tb->field_name, k);
			s_sis_field_unit *fu = (s_sis_field_unit *)sis_map_buffer_get(tb->field_map, fname);
			printf("key=%s size=%d offset=%d\n", fname, fields, fu->offset);
			if (!fu)
			{
				continue;
			}
			char key[16];
			sis_sprintf(key, 10, "%d", k);
			_sis_fields_json_to_struct(o + index * unit_->value->len, fu, key, jval);
		}
		index++;
		jval = jval->next;
	}

	sis_json_close(handle);
	return o;
}
////////////////

s_sis_sds sis_collect_struct_filter_sds(s_sis_collect_unit *unit_, s_sis_sds in_, const char *fields_)
{
	// 一定不是全字段
	s_sis_sds o = NULL;

	s_sis_table *tb = unit_->father;
	s_sis_string_list *field_list = sis_string_list_create_w();
	sis_string_list_load(field_list, fields_, strlen(fields_), ",");

	int len = sis_table_get_fields_size(tb);
	int count = (int)(sis_sdslen(in_) / len);
	char *val = in_;
	for (int k = 0; k < count; k++)
	{
		for (int i = 0; i < sis_string_list_getsize(field_list); i++)
		{
			const char *key = sis_string_list_get(field_list, i);
			s_sis_field_unit *fu = sis_field_get_from_key(tb, key);
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
		val += len;
	}
	sis_string_list_destroy(field_list);
	return o;
}
s_sis_sds sis_collect_struct_to_json_sds(s_sis_collect_unit *unit_, s_sis_sds in_, const char *fields_)
{
	s_sis_sds o = NULL;

	s_sis_table *tb = unit_->father;
	s_sis_string_list *field_list = tb->field_name; //取得全部的字段定义

	if (!sis_check_fields_all(fields_))
	{
		field_list = sis_string_list_create_w();
		sis_string_list_load(field_list, fields_, strlen(fields_), ",");
	}

	char *str;
	s_sis_json_node *jone = sis_json_create_object();
	s_sis_json_node *jtwo = sis_json_create_array();
	s_sis_json_node *jval = NULL;
	s_sis_json_node *jfields = sis_json_create_object();

	// 先处理字段
	for (int i = 0; i < sis_string_list_getsize(field_list); i++)
	{
		const char *key = sis_string_list_get(field_list, i);
		// printf("----0----tb->field_name=%s\n",sis_string_list_get(tb->field_name, i));
		// printf("----1----fields=%s\n",key);
		sis_json_object_add_uint(jfields, key, i);
	}
	sis_json_object_add_node(jone, SIS_JSON_KEY_FIELDS, jfields);

	// printf("========%s rows=%d\n", tb->name, tb->control.limit_rows);

	int dot = 0; //小数点位数
	int skip_len = sis_table_get_fields_size(tb);
	int count = (int)(sis_sdslen(in_) / skip_len);
	char *val = in_;
	for (int k = 0; k < count; k++)
	{
		sis_out_binary("get", val, sis_sdslen(in_));
		jval = sis_json_create_array();
		for (int i = 0; i < sis_string_list_getsize(field_list); i++)
		{
			const char *key = sis_string_list_get(field_list, i);
			// printf("----2----fields=%s\n",key);
			s_sis_field_unit *fu = sis_field_get_from_key(tb, key);
			if (!fu)
			{
				sis_json_array_add_string(jval, " ", 1);
				continue;
			}
			const char *ptr = (const char *)val;
			switch (fu->flags.type)
			{
			case SIS_FIELD_STRING:
				sis_json_array_add_string(jval, ptr + fu->offset, fu->flags.len);
				break;
			case SIS_FIELD_INT:
				// printf("ptr= %p, name=%s offset=%d\n", ptr, key, fu->offset);
				sis_json_array_add_int(jval, sis_fields_get_int(fu, ptr));
				break;
			case SIS_FIELD_UINT:
				sis_json_array_add_uint(jval, sis_fields_get_uint(fu, ptr));
				break;
			case SIS_FIELD_DOUBLE:
				if (!fu->flags.io && fu->flags.zoom > 0)
				{
					dot = fu->flags.zoom;
				}
				sis_json_array_add_double(jval, sis_fields_get_double(fu, ptr), dot);
				break;
			default:
				sis_json_array_add_string(jval, " ", 1);
				break;
			}
		}
		if (tb->control.limit_rows != 1)
		{
			sis_json_array_add_node(jtwo, jval);
		}
		else
		{
			break;
		}
		val += skip_len;
	}

	if (tb->control.limit_rows != 1)
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
	// printf("1112111 [%d]\n",tb->control.limit_rows);

	size_t olen;
	str = sis_json_output_zip(jone, &olen);
	o = sis_sdsnewlen(str, olen);
	sis_free(str);
	sis_json_delete_node(jone);

	if (!sis_check_fields_all(fields_))
	{
		sis_string_list_destroy(field_list);
	}
	return o;
}

s_sis_sds sis_collect_struct_to_array_sds(s_sis_collect_unit *unit_, s_sis_sds in_, const char *fields_)
{
	s_sis_sds o = NULL;

	s_sis_table *tb = unit_->father;
	s_sis_string_list *field_list = tb->field_name; //取得全部的字段定义
	if (!sis_check_fields_all(fields_))
	{
		field_list = sis_string_list_create_w();
		sis_string_list_load(field_list, fields_, strlen(fields_), ",");
	}

	char *str;
	s_sis_json_node *jone = sis_json_create_array();
	s_sis_json_node *jval = NULL;

	int dot = 0; //小数点位数
	int skip_len = sis_table_get_fields_size(tb);
	int count = (int)(sis_sdslen(in_) / skip_len);
	char *val = in_;
	for (int k = 0; k < count; k++)
	{
		sis_out_binary("get", val, sis_sdslen(in_));
		jval = sis_json_create_array();
		for (int i = 0; i < sis_string_list_getsize(field_list); i++)
		{
			const char *key = sis_string_list_get(field_list, i);
			s_sis_field_unit *fu = sis_field_get_from_key(tb, key);
			if (!fu)
			{
				sis_json_array_add_string(jval, " ", 1);
				continue;
			}
			const char *ptr = (const char *)val;
			switch (fu->flags.type)
			{
			case SIS_FIELD_STRING:
				sis_json_array_add_string(jval, ptr + fu->offset, fu->flags.len);
				break;
			case SIS_FIELD_INT:
				// printf("ptr= %p, name=%s offset=%d\n", ptr, key, fu->offset);
				sis_json_array_add_int(jval, sis_fields_get_int(fu, ptr));
				break;
			case SIS_FIELD_UINT:
				sis_json_array_add_uint(jval, sis_fields_get_uint(fu, ptr));
				break;
			case SIS_FIELD_DOUBLE:
				if (!fu->flags.io && fu->flags.zoom > 0)
				{
					dot = fu->flags.zoom;
				}
				sis_json_array_add_double(jval, sis_fields_get_double(fu, ptr), dot);
				break;
			default:
				sis_json_array_add_string(jval, " ", 1);
				break;
			}
		}
		sis_json_array_add_node(jone, jval);
		if (tb->control.limit_rows == 1)
		{
			break;
		}
		val += skip_len;
	}
	size_t olen;

	if (tb->control.limit_rows != 1)
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
	// printf("1112111 [%d]\n",tb->control.limit_rows);
	o = sis_sdsnewlen(str, olen);
	sis_free(str);
	sis_json_delete_node(jone);

	if (!sis_check_fields_all(fields_))
	{
		sis_string_list_destroy(field_list);
	}
	return o;
}
