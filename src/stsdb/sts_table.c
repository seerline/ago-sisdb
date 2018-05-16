

#include "sts_table.h"
#include "sts_db.h"
#include "sts_collect.h"
#include "sts_time.h"
#include "sts_json.h"

s_sts_table *sts_table_create(const char *name_, const char *command)
//command为一个json格式字段定义
{
	s_sts_table *tb = sts_db_get_table(name_);
	if (tb) {
		sts_table_destroy(tb);
	}
	tb = zmalloc(sizeof(s_sts_table ));
	tb->control.insert_mode = STS_INSERT_STS_CHECK;
	tb->control.limit_rows = 0;
	tb->control.version = (uint32)sts_time_get_now();
	tb->control.zip_mode = 0;//STS_ZIP_NO;

	tb->name = sdsnew(name_);
	tb->collect_map = sts_map_pointer_create();
	//处理字段定义
	tb->field_name = sts_string_list_create_w();
	tb->field_map = sts_map_pointer_create();
	sts_table_set_fields(tb, command);
	return tb;
}

void sts_table_destroy(s_sts_table *tb_)
//删除一个表
{
	//删除字段定义
	dictEntry *de;
	dictIterator *di = dictGetSafeIterator(tb_->field_map);
	while ((de = dictNext(di)) != NULL) {
		s_sts_field_unit *val = (s_sts_field_unit *)dictGetVal(de);
		sts_field_unit_destroy(val);
	}
	dictReleaseIterator(di);
	sts_map_pointer_destroy(tb_->field_map);
	sts_string_list_destroy(tb_->field_name);
	//删除数据区
	sts_table_clear(tb_);

	sdsfree(tb_->name);
	zfree(tb_);
}
void sts_table_clear(s_sts_table *tb_)
//清理一个表的所有数据
{
	dictEntry *de;
	dictIterator *di = dictGetSafeIterator(tb_->collect_map);
	while ((de = dictNext(di)) != NULL) {
		s_sts_collect_unit *val = (s_sts_collect_unit *)dictGetVal(de);
		destroy_sts_collect_unit(val);
	}
	dictReleaseIterator(di);
	sts_map_pointer_destroy(tb_->collect_map);
}
/////////////////////////////////////
//对数据库的各种属性设置
////////////////////////////////////
void sts_table_set_ver(s_sts_table *tb_, uint32_t ver_)
{
	tb_->control.version = ver_;
}
void sts_table_set_limit_rows(s_sts_table *tb_, uint32_t limits_)
{
	tb_->control.limit_rows = limits_;
}
void sts_table_set_zip_mode(s_sts_table *tb_, uint8_t zip_)
{
	tb_->control.zip_mode = 0;//STS_ZIP_NO;
}
void sts_table_set_insert_mode(s_sts_table *tb_, uint8_t insert_)
{
	tb_->control.insert_mode = STS_INSERT_STS_CHECK;
}

void sts_table_set_fields(s_sts_table *tb_, const char *command)
{
	s_sts_json_handle *handle = sts_json_load(command, strlen(command));
	if (!handle) { return; }
	s_sts_json_node *node = sts_json_cmp_child_node(handle->node, "fields");
	if (!node) { goto fail; }

	sts_map_pointer_clear(tb_->field_map);
	sts_string_list_clear(tb_->field_name);
	s_sts_json_node *item = sts_json_first_node(node);
	int index = 0;
	int offset = 0;
	while (item){
		const char * name = sts_json_get_str(item, "name");
		s_sts_field_unit *unit = sts_field_unit_create(
			index++,
			name,
			sts_json_get_str(item, "type"),
			sts_json_get_str(item, "zip"),
			sts_json_get_int(item, "len", 0),
			sts_json_get_int(item, "zoom", -100)
			);
		unit->offset = offset;
		offset += unit->flags.len;
		sts_map_pointer_set(tb_->field_map, name, unit);
		sts_string_list_push(tb_->field_name, name, strlen(name));
		item = sts_json_next_node(item);
	}
fail:
	sts_json_close(handle);
}

//获取数据库的各种值
s_sts_field_unit *sts_table_get_field(s_sts_table *tb_, const char *name_)
{
	if (!name_) { return NULL; }
	s_sts_field_unit *val = (s_sts_field_unit *)sts_map_pointer_get(tb_->field_map, name_);
//	s_sts_field_unit *val = (s_sts_field_unit *)dictFetchValue(tb_->field_map, name_);
	return val;
}
int sts_table_get_fields_size(s_sts_table *tb_){
	int len = 0;
	dictEntry *de;
	dictIterator *di = dictGetSafeIterator(tb_->field_map);
	while ((de = dictNext(di)) != NULL) {
		s_sts_field_unit *val = (s_sts_field_unit *)dictGetVal(de);
		len += val->flags.len;
	}
	dictReleaseIterator(di);
	return len;
}
uint64 _sts_table_get_integer(s_sts_field_unit *fu_, void *val_){
	uint64 out = 0;
	char *ptr = val_;
	uint8  *u8;
	uint16 *u16;
	uint32 *u32;
	uint64 *u64;

	switch (fu_->flags.type)
	{
	case STS_FIELD_INDEX:
	case STS_FIELD_SECOND:
	case STS_FIELD_MINUTE:
	case STS_FIELD_DAY:
	case STS_FIELD_MONTH:
	case STS_FIELD_INT4:
	case STS_FIELD_INTZ:
		u32 = (uint32 *)(ptr + fu_->offset);
		out = *u32;
		break;
	case STS_FIELD_YEAR:
	case STS_FIELD_INT2:
		u16 = (uint16 *)(ptr + fu_->offset);
		out = *u16;
		break;
	case STS_FIELD_INT1:
		u8 = (uint8 *)(ptr + fu_->offset);
		out = *u8;
		break;
	case STS_FIELD_CODE:
	case STS_FIELD_TIME:
	case STS_FIELD_INT8:
		u64 = (uint64 *)(ptr + fu_->offset);
		out = *u64;
		break;
	case STS_FIELD_PRC:
	case STS_FIELD_VOL:
		if (fu_->flags.len == 4) {
			u32 = (uint32 *)(ptr + fu_->offset);
			out = *u32;
		}
		if (fu_->flags.len == 8) {
			u64 = (uint64 *)(ptr + fu_->offset);
			out = *u64;
		}
	default:
		break;
	}
	return out;
}
double _sts_table_get_double(s_sts_field_unit *fu_, void *val_){
	double out = 0.0;
	char  *ptr = val_;
	float   *f32;
	double  *f64;

	switch (fu_->flags.type)
	{
	case STS_FIELD_FLOAT:
		f32 = (float  *)(ptr + fu_->offset);
		out = *f32;
		break;
	case STS_FIELD_DOUBLE:
		f64 = (double  *)(ptr + fu_->offset);
		out = *f64;
		break;
	default:
		break;
	}
	return out;
}
uint64 sts_table_get_times(s_sts_table *tb_, void *val_){
	uint64 out = 0;
	int count = sts_string_list_getsize(tb_->field_name);
	for (int i = 0; i < count; i++){
		s_sts_field_unit *fu = (s_sts_field_unit *)sts_map_buffer_get(tb_->field_map, sts_string_list_get(tb_->field_name, i));
		if (!fu) { continue;  }
		if (sts_field_is_times(fu->flags.type)){
			out = _sts_table_get_integer(fu, val_);
			break;
		}
	}
	return out;
}
sds sts_table_get_string_m(s_sts_table *tb_, void *val_, const char *name_){
	
	s_sts_field_unit *fu = (s_sts_field_unit *)sts_map_buffer_get(tb_->field_map, name_);
	if (!fu || fu->flags.type != STS_FIELD_CHAR) { return NULL; }
	sds str = sdsnewlen((const char *)val_ + fu->offset, fu->flags.len);
	return str;
}

//////////////////////////////////////////////////////////////////////////////////
//修改数据，key_为股票代码或市场编号，value_为二进制结构化数据或json数据
//////////////////////////////////////////////////////////////////////////////////
void sts_table_update(s_sts_table *tb_, const char *key_, sds value_)
{
	s_sts_collect_unit *collect = sts_map_buffer_get(tb_->collect_map, key_);
	if (!collect){
		collect = create_sts_collect_unit(tb_, key_);
		sts_map_buffer_set(tb_->collect_map, key_, collect);
	}
	sts_collect_unit_update(collect, value_, sdslen(value_));
}
//////////////////////////
//删除数据
//////////////////////////
int sts_table_delete(s_sts_table *tb_, const char *key_, const char *command)
{
	s_sts_collect_unit *collect = sts_map_buffer_get(tb_->collect_map, key_);
	if (!collect){ return 0; }

	s_sts_json_handle *handle = sts_json_load(command, strlen(command));
	if (!handle) { return 0; }

	uint64 min, max;
	int start, stop;
	int count = 0;
	int rtn = 0;
	int minX, maxX;

	s_sts_json_node *search = sts_json_cmp_child_node(handle->node, "search");
	if (search) { 
		if (!sts_json_cmp_child_node(search, "min")) { goto exit; }
		min = sts_json_get_int(search, "min", 0);
		if (sts_json_cmp_child_node(search, "count")){
			count = sts_json_get_int(search, "count", 1);
			start = sts_collect_unit_search(collect, min);
			if (start >= 0) {
				rtn = sts_collect_unit_delete_of_count(collect, start, count);
			}
		}
		else {
			if (sts_json_cmp_child_node(search, "max")) {
				max = sts_json_get_int(search, "max", 0);
				start = sts_collect_unit_search_right(collect, min, &minX);
				stop = sts_collect_unit_search_left(collect, max, &maxX);
				if (minX != STS_SEARCH_NONE && maxX != STS_SEARCH_NONE) {
					rtn = sts_collect_unit_delete_of_range(collect, start, stop);
				}
			}
			else {
				start = sts_collect_unit_search(collect, min);
				if (start >= 0) {
					rtn = sts_collect_unit_delete_of_count(collect, start, 1);
				}
			}
		}
		goto exit;
	}
	// 按界限操作
	s_sts_json_node *range = sts_json_cmp_child_node(handle->node, "range");
	if (!range) { goto exit; }

	if (!sts_json_cmp_child_node(range, "start")) { goto exit; }
	start = sts_json_get_int(range, "start", -1); // -1 为最新一条记录
	if (sts_json_cmp_child_node(range, "count")){
		count = sts_json_get_int(range, "count", 1);
		rtn = sts_collect_unit_delete_of_count(collect, start, count); // 定位后按数量删除
	}
	else {
		if (sts_json_cmp_child_node(range, "stop")) {
			stop = sts_json_get_int(range, "stop", -1);// -1 为最新一条记录
			rtn = sts_collect_unit_delete_of_range(collect, start, stop); // 定位后删除
		}
		else {
			rtn = sts_collect_unit_delete_of_count(collect, start, 1); // 定位后按数量删除
		}
	}
exit:
	sts_json_close(handle);
	return rtn;
}

////////////////////////////////////////////////////////////////////////////////////////////////
//取数据,读表中代码为key的数据，key为*表示所有股票数据，由command定义数据范围和字段范围
////////////////////////////////////////////////////////////////////////////////////////////////

sds sts_table_get_m(s_sts_table *tb_, const char *key_, const char *command)
{
	s_sts_collect_unit *collect = sts_map_buffer_get(tb_->collect_map, key_);
	if (!collect){ return NULL; }

	s_sts_json_handle *handle = sts_json_load(command, strlen(command));
	if (!handle) { return NULL; }

	sds out = NULL;

	// 取出数据返回格式，没有就默认为数组
	int out_format = STS_DATA_ARRAY;
	s_sts_json_node *format = sts_json_cmp_child_node(handle->node, "format");
	if (format) {
		s_sts_map_define *smd = sts_db_find_map_define(format->value);
		if (smd) { out_format = smd->uid; }
	} 

	// 取出字段定义，没有就默认全部字段
	sds out_fields = NULL;
	s_sts_json_node *fields = sts_json_cmp_child_node(handle->node, "fields");
	if (fields) {
		if (strcmp(fields->value, "*")){
			out_fields = sdsnew(fields->value);
		}
	}
	// 检查取值范围，没有就全部取
	uint64 min, max;
	int start, stop;
	int count = 0;
	int maxX, minX;

	s_sts_json_node *search = sts_json_cmp_child_node(handle->node, "search");
	s_sts_json_node *range = sts_json_cmp_child_node(handle->node, "range");
	if (!search && !range){
		out = sts_collect_unit_get_of_range_m(collect, 0, -1, out_format, out_fields);
		goto exit;
	}
	if (search) {
		if (!sts_json_cmp_child_node(search, "min")) { goto exit; }
		min = sts_json_get_int(search, "min", 0);
		if (sts_json_cmp_child_node(search, "count")){
			count = sts_json_get_int(search, "count", 1);
			start = sts_collect_unit_search(collect, min);
			if (start >= 0) {
				out = sts_collect_unit_get_of_count_m(collect, start, count, out_format, out_fields);
			}
		}
		else {
			if (sts_json_cmp_child_node(search, "max")) {
				max = sts_json_get_int(search, "max", 0);
				start = sts_collect_unit_search_right(collect, min, &minX);
				stop = sts_collect_unit_search_left(collect, max, &maxX);
				if (minX != STS_SEARCH_NONE && maxX != STS_SEARCH_NONE) {
					out = sts_collect_unit_get_of_range_m(collect, start, stop, out_format, out_fields);
				}
			}
			else {
				start = sts_collect_unit_search(collect, min);
				if (start >= 0) {
					out = sts_collect_unit_get_of_count_m(collect, start, 1, out_format, out_fields);
				}
			}
		}
		goto exit;
	}
	if (range) {
		if (!sts_json_cmp_child_node(range, "start")) { goto exit; }
		start = sts_json_get_int(range, "start", -1); // -1 为最新一条记录
		if (sts_json_cmp_child_node(range, "count")){
			count = sts_json_get_int(range, "count", 1);
			out = sts_collect_unit_get_of_count_m(collect, start, count, out_format, out_fields);
		}
		else {
			if (sts_json_cmp_child_node(range, "stop")) {
				stop = sts_json_get_int(range, "stop", -1);// -1 为最新一条记录
				out = sts_collect_unit_get_of_range_m(collect, start, stop, out_format, out_fields);
			}
			else {
				out = sts_collect_unit_get_of_count_m(collect, start, 1, out_format, out_fields);
			}
		}
		goto exit;
	}
exit:
	sts_json_close(handle);
	if (out_fields) {
		sdsfree(out_fields);
	}
	return out;

}


