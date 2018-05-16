

#include "sts_collect.h"

///////////////////////////////////////////////////////////////////////////
//------------------------s_sts_step_index --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sts_step_index *create_stepindex()
{
	s_sts_step_index *si = zmalloc(sizeof(s_sts_step_index));
	si->left = 0;
	si->right = 0;
	si->count = 0;
	si->step = 0;
	return si;
}
void destroy_stepindex(s_sts_step_index *si_)
{
	zfree(si_);
}
void stepindex_rebuild(s_sts_step_index *si_, uint64 left_, uint64 right_, int count_)
{
	si_->left = left_;
	si_->right = right_;
	si_->count = count_;
	si_->step = min((uint64)((right_ - left_) / count_ ), 1);

}
int stepindex_goto(s_sts_step_index *si_, uint64 curr_)
{
	if (curr_ < si_->left) {
		return 0;
	}
	if (curr_ > si_->right){
		return si_->count - 1;
	}
	int index = (int)((curr_ - si_->left) / si_->step);
	if (index > si_->count - 1) {
		return si_->count - 1;
	}
	return index;
}
///////////////////////////////////////////////////////////////////////////
//------------------------s_sts_collect_unit --------------------------------//
///////////////////////////////////////////////////////////////////////////


s_sts_collect_unit *create_sts_collect_unit(s_sts_table *tb_, const char *key_)
{
	s_sts_collect_unit *unit = zmalloc(sizeof(s_sts_collect_unit));
	unit->father = tb_;
	unit->step = create_stepindex();
	unit->value = sts_struct_list_create(sts_table_get_fields_size(unit->father), NULL, 0);
	return unit;
}
void destroy_sts_collect_unit(s_sts_collect_unit *unit_)
{
	sts_struct_list_destroy(unit_->value);
	destroy_stepindex(unit_->step);
	zfree(unit_);
}

uint64 sts_collect_unit_get_time(s_sts_collect_unit *unit_, int index_)
{
	time_t tt = 0;
	void * val = sts_struct_list_get(unit_->value, index_);
	if (val) {
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
	stepindex_rebuild(unit_->step,
		sts_collect_unit_get_time(unit_, 0),
		sts_collect_unit_get_time(unit_, unit_->value->count - 1), 
		unit_->value->count);
	return 0;
}
int	sts_collect_unit_recs(s_sts_collect_unit *unit_)
{
	return unit_->value->count;
}
int sts_collect_unit_search_left(s_sts_collect_unit *unit_, uint64 index_, int *mode_)
{
	*mode_ = STS_SEARCH_NONE;
	int index = stepindex_goto(unit_->step, index_);
	if (index < 0) {
		return -1;  // 没有任何数据
	}
	int i = index;
	int dir = 0;
	while (i >= 0 && i < unit_->value->count){
		uint64 ts = sts_collect_unit_get_time(unit_, i);
		if (index_ > ts){
			if (dir == -1) {
				*mode_ = STS_SEARCH_LEFT;
				return i;
			}
			dir = 1;
			i += dir;
			continue;
		}
		else if (index_ < ts){
			if (dir == 1) {
				*mode_ = STS_SEARCH_LEFT;
				return i - 1;
			}
			dir = -1;
			i += dir;
			continue;
		}
		else {
			*mode_ = STS_SEARCH_OK;
			return i;
		}
	}
	if (dir == 1) {
		*mode_ = STS_SEARCH_LEFT;
		return unit_->value->count - 1;
	}
	else {
		return -1;
	}
}
int sts_collect_unit_search_right(s_sts_collect_unit *unit_, uint64 index_, int *mode_)
{
	*mode_ = STS_SEARCH_NONE;
	int index = stepindex_goto(unit_->step, index_);
	if (index < 0) {
		return -1;  // 没有任何数据
	}
	int i = index;
	int dir = 0;
	while (i >= 0 && i < unit_->value->count){
		uint64 ts = sts_collect_unit_get_time(unit_, i);
		if (index_ > ts){
			if (dir == -1) {
				*mode_ = STS_SEARCH_RIGHT;
				return i + 1;
			}
			dir = 1;
			i += dir;
			continue;
		}
		else if (index_ < ts){
			if (dir == 1) {
				*mode_ = STS_SEARCH_RIGHT;
				return i;
			}
			dir = -1;
			i += dir;
			continue;
		}
		else {
			*mode_ = STS_SEARCH_OK;
			return i;
		}
	}
	
	if (dir == 1) {
		return -1;
	}
	else {
		*mode_ = STS_SEARCH_RIGHT;
		return 0;
	}	
}
int sts_collect_unit_search(s_sts_collect_unit *unit_, uint64 index_)
{
	int index = stepindex_goto(unit_->step, index_);
	if (index < 0) {
		return -1;  // 没有任何数据
	}
	int i = index;
	int dir = 0;
	while (i >= 0 && i < unit_->value->count){
		uint64 ts = sts_collect_unit_get_time(unit_, i);
		if (index_ > ts){
			if (dir == -1) {
				return i-1;
			}
			dir = 1;
			i += dir;
			continue;
		}
		else if (index_ < ts){
			if (dir == 1) {
				return -1;
			}
			dir = -1;
			i += dir;
			continue;
		}
		else {
			return i;
		}
	}
	return -1;
}

int	sts_collect_unit_delete_of_range(s_sts_collect_unit *unit_, int start_, int stop_)
{
	int llen, count;

	llen = sts_collect_unit_recs(unit_);

	if (start_ < 0) start_ = llen + start_;
	if (stop_ < 0) stop_ = llen + stop_;
	if (start_ < 0) start_ = 0;

	if (start_ > stop_ || start_ >= llen) {
		return 0;
	}
	if (stop_ >= llen) stop_ = llen - 1;
	count = (stop_ - start_) + 1;

	return _sts_collect_unit_delete(unit_, start_, count);
}
int	sts_collect_unit_delete_of_count(s_sts_collect_unit *unit_, int start_, int count_)
{
	int llen;
	llen = sts_collect_unit_recs(unit_);

	if (start_ < 0) start_ = llen + start_;
	if (start_ < 0) start_ = 0;

	if (start_ >= llen) {
		return 0;
	}
	if (start_ + count_ > llen) count_ = llen - start_;

	return _sts_collect_unit_delete(unit_, start_, count_);
}
//////////////////////////////////////////////////////
//  get  --  先默认二进制格式，全部字段返回数据
//////////////////////////////////////////////////////

//#define STS_DATA_ZIP     'R'   // 后面开始为数据
//#define STS_DATA_STRUCT     'B'   // 后面开始为数据
//#define STS_DATA_STRING  'S'   // 后面开始为数据
//#define STS_DATA_JSON    '{'   // 直接传数据
//#define STS_DATA_ARRAY   '['   // 直接传数据
#define check_fields_all(f) (!f||!strncmp(f,"*",1))

sds _sts_collect_unit_get_data(s_sts_collect_unit *unit_, int no_, const char *fieldname_, sds out_)
{
	s_sts_field_unit *fu = sts_table_get_field(unit_->father, fieldname_);
	if (!fu) { return out_; }
	char *data = (char *)sts_struct_list_get(unit_->value, no_);
	if (!data) { return out_; }
	if (!out_){
		out_ = sdsnewlen(data + fu->offset, fu->flags.len);
	}
	else {
		out_ = sdscatlen(out_, data + fu->offset, fu->flags.len);
	}
	return out_;
}
sds _sts_collect_unit_get_of_count(s_sts_collect_unit *unit_, int start_, int count_, int format_, const char *fields_)
{
	sds out = NULL;
	switch (format_)
	{
	case STS_DATA_STRUCT:
		if (check_fields_all(fields_)){
			out = sdsnewlen(sts_struct_list_get(unit_->value, start_), count_ * unit_->value->len);
		}
		else {
			s_sts_string_list *list = sts_string_list_create_r();
			sts_string_list_load(list, fields_, strlen(fields_), ",");
			for (int no = start_; no < start_ + count_; no++){
				for (int i = 0; i < sts_string_list_getsize(list); i++){
					const char *key = sts_string_list_get(list, i);
					out = _sts_collect_unit_get_data(unit_, no, key, out); // 直接写入out中
				}
			}
			sts_string_list_destroy(list);
		}
		break;
	case STS_DATA_ZIP:
	case STS_DATA_STRING:
		break;
	case STS_DATA_JSON:
	case STS_DATA_ARRAY:
		break;
	default:
		break;
	}
	return out;
}

sds sts_collect_unit_get_of_count_m(s_sts_collect_unit *unit_, int start_, int count_, int format_, const char *fields_)
{
	if (count_ == 0) {
		return NULL;
	}
	int llen = sts_collect_unit_recs(unit_);

	if (start_ < 0) { start_ = llen + start_; }
	if (start_ < 0) { start_ = 0; }
	if (count_ < 0) {
		count_ = abs(count_);
		if (count_ >(start_ + 1)){
			count_ = start_ + 1;
		}
		start_ -= (count_ - 1);
	}
	else {
		if ((start_ + count_) > llen) {
			count_ = llen - start_;
		}
	}
	return _sts_collect_unit_get_of_count(unit_, start_, count_, format_, fields_);
}

sds sts_collect_unit_get_of_range_m(s_sts_collect_unit *unit_, int start_, int stop_, int format_, const char *fields_)
{
	int llen = sts_collect_unit_recs(unit_);

	if (start_ < 0) { start_ = llen + start_; }
	if (stop_ < 0)  { stop_ = llen + stop_; }
	if (start_ < 0) { start_ = 0; }

	if (start_ > stop_ || start_ >= llen) {
		return NULL;
	}
	if (stop_ >= llen) { stop_ = llen - 1; }

	int count = (stop_ - start_) + 1;
	return _sts_collect_unit_get_of_count(unit_, start_, count, format_, fields_);
}
////////////////////////
//  update
////////////////////////
int _sts_collect_unit_update(s_sts_collect_unit *unit_, const char *in_)
{
	uint64 tt;
	switch (unit_->father->control.insert_mode)
	{
	case STS_INSERT_PUSH:
		if (unit_->father->control.limit_rows == 1){
			sts_struct_list_update(unit_->value, 0, (void *)in_);
			unit_->value->count = 1;
		}
		else {
			sts_struct_list_push(unit_->value, (void *)in_);
			if (unit_->father->control.limit_rows > 1){
				sts_struct_list_limit(unit_->value, unit_->father->control.limit_rows);
			}
		}
		break;
	case STS_INSERT_MUL_CHECK:
		break;
	default: // STS_INSERT_STS_CHECK
		tt = sts_table_get_times(unit_->father, (void *)in_);  // 得到时间序列值
		int mode;
		int index = sts_collect_unit_search_left(unit_, tt, &mode);
		if (mode == STS_SEARCH_NONE) {
			sts_struct_list_push(unit_->value, (void *)in_);
		}
		else if (mode == STS_SEARCH_OK) {
			sts_struct_list_update(unit_->value, index, (void *)in_);
		}
		else {
			sts_struct_list_insert(unit_->value, index + 1, (void *)in_);
		}
		if (unit_->father->control.limit_rows > 0){
			sts_struct_list_limit(unit_->value, unit_->father->control.limit_rows);
		}
		break;
	}
	stepindex_rebuild(unit_->step,
		sts_collect_unit_get_time(unit_, 0),
		sts_collect_unit_get_time(unit_, unit_->value->count - 1),
		unit_->value->count);
	return 1;
}
int sts_collect_unit_update(s_sts_collect_unit *unit_, const char *in_, size_t inLen_)
{
	if (inLen_ < 1) return 0;
	char dt = in_[0]; // 取类型
	int count = 0;
	switch (dt)
	{
	case STS_DATA_STRUCT:
		count = (int)(inLen_ / unit_->value->len);
		for (int i = 0; i < count; i++){
			_sts_collect_unit_update(unit_, in_ + i * unit_->value->len);
		}
		break;
	case STS_DATA_ZIP:
	case STS_DATA_STRING:
		break;
	case STS_DATA_JSON:
	case STS_DATA_ARRAY:
		break;
	default:
		break;
	}
	return count;
}
