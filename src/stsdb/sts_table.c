

#include "sts_table.h"
#include "sts_db.h"
#include "sts_collect.h"
#include "lw_time.h"
#include "lw_json.h"

sts_table *create_sts_table(const char *name_, const char *command)
//command为一个json格式字段定义
{
	sts_table *tb = sts_db_get_table(name_);
	if (tb) {
		destroy_sts_table(tb);
	}
	tb = zmalloc(sizeof(sts_table));
	tb->control.insert_mode = STS_INSERT_TS_CHECK;
	tb->control.limit_rows = 0;
	tb->control.version = (uint32)dp_nowtime();
	tb->control.zip_mode = STS_ZIP_NO;

	tb->name = sdsnew(name_);
	tb->collect_map = create_map_pointer();
	//处理字段定义
	tb->field_name = create_string_list_w();
	tb->field_map = create_map_pointer();
	sts_table_set_fields(tb, command);
	return tb;
}

void destroy_sts_table(sts_table *tb_)
//删除一个表
{
	//删除字段定义
	dictEntry *de;
	dictIterator *di = dictGetSafeIterator(tb_->field_map);
	while ((de = dictNext(di)) != NULL) {
		sts_field_unit *val = (sts_field_unit *)dictGetVal(de);
		destroy_sts_field_unit(val);
	}
	dictReleaseIterator(di);
	destroy_map_pointer(tb_->field_map);
	destroy_string_list(tb_->field_name);
	//删除数据区
	clear_sts_table(tb_);

	sdsfree(tb_->name);
	zfree(tb_);
}
void clear_sts_table(sts_table *tb_)
//清理一个表的所有数据
{
	dictEntry *de;
	dictIterator *di = dictGetSafeIterator(tb_->collect_map);
	while ((de = dictNext(di)) != NULL) {
		sts_collect_unit *val = (sts_collect_unit *)dictGetVal(de);
		destroy_sts_collect_unit(val);
	}
	dictReleaseIterator(di);
	destroy_map_pointer(tb_->collect_map);
}
/////////////////////////////////////
//对数据库的各种属性设置
////////////////////////////////////
void sts_table_set_ver(sts_table *tb_, uint32_t ver_)
{
	tb_->control.version = ver_;
}
void sts_table_set_limit_rows(sts_table *tb_, uint32_t limits_)
{
	tb_->control.limit_rows = limits_;
}
void sts_table_set_zip_mode(sts_table *tb_, uint8_t zip_)
{
	tb_->control.zip_mode = STS_ZIP_NO;
}
void sts_table_set_insert_mode(sts_table *tb_, uint8_t insert_)
{
	tb_->control.insert_mode = STS_INSERT_TS_CHECK;
}

void sts_table_set_fields(sts_table *tb_, const char *command)
{
	dp_json_read_handle *handle = dp_json_load(command);
	if (!handle) { return; }
	s_json_node *node = dp_json_object_get_item(handle->node, "fields");
	if (!node) { goto fail; }

	clear_map_pointer(tb_->field_map);
	clear_string_list(tb_->field_name);
	s_json_node *item = dp_json_array_get_item(node, 0);
	int index = 0;
	int offset = 0;
	while (item){
		const char * name = dp_json_get_str(item, "name");
		sts_field_unit *unit = create_sts_field_unit(
			index++,
			name,
			dp_json_get_str(item, "type"),
			dp_json_get_str(item, "zip"),
			dp_json_get_int(item, "len", 0),
			dp_json_get_int(item, "zoom", -100)
			);
		unit->offset = offset;
		offset += unit->flags.len;
		set_map_pointer(tb_->field_map, name, unit);
		string_list_push(tb_->field_name, name, strlen(name));
		item = dp_json_next_item(item);
	}
fail:
	dp_json_close(handle);
}

//获取数据库的各种值
sts_field_unit *sts_table_get_field(sts_table *tb_, const char *name_)
{
	if (!name_) { return NULL; }
	sts_field_unit *val = (sts_field_unit *)get_map_pointer(tb_->field_map, name_);
//	sts_field_unit *val = (sts_field_unit *)dictFetchValue(tb_->field_map, name_);
	return val;
}
int sts_table_get_fields_size(sts_table *tb_){
	int len = 0;
	dictEntry *de;
	dictIterator *di = dictGetSafeIterator(tb_->field_map);
	while ((de = dictNext(di)) != NULL) {
		sts_field_unit *val = (sts_field_unit *)dictGetVal(de);
		len += val->flags.len;
	}
	dictReleaseIterator(di);
	return len;
}
uint64 __sts_table_get_integer(sts_field_unit *fu_, void *val_){
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
double __sts_table_get_double(sts_field_unit *fu_, void *val_){
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
uint64 sts_table_get_times(sts_table *tb_, void *val_){
	uint64 out = 0;
	int count = string_list_getsize(tb_->field_name);
	for (int i = 0; i < count; i++){
		sts_field_unit *fu = (sts_field_unit *)get_map_buffer(tb_->field_map, string_list_get(tb_->field_name, i));
		if (!fu) { continue;  }
		if (sts_field_is_times(fu->flags.type)){
			out = __sts_table_get_integer(fu, val_);
			break;
		}
	}
	return out;
}
sds sts_table_get_string_m(sts_table *tb_, void *val_, const char *name_){
	
	sts_field_unit *fu = (sts_field_unit *)get_map_buffer(tb_->field_map, name_);
	if (!fu || fu->flags.type != STS_FIELD_CHAR) { return NULL; }
	sds str = sdsnewlen((const char *)val_ + fu->offset, fu->flags.len);
	return str;
}

//////////////////////////////////////////////////////////////////////////////////
//修改数据，key_为股票代码或市场编号，value_为二进制结构化数据或json数据
//////////////////////////////////////////////////////////////////////////////////
void sts_table_update(sts_table *tb_, const char *key_, sds value_)
{
	sts_collect_unit *collect = get_map_buffer(tb_->collect_map, key_);
	if (!collect){
		collect = create_sts_collect_unit(tb_, key_);
		set_map_buffer(tb_->collect_map, key_, collect);
	}
	sts_collect_unit_update(collect, value_, sdslen(value_));
}
//////////////////////////
//删除数据
//////////////////////////
int sts_table_delete(sts_table *tb_, const char *key_, const char *command)
{
	sts_collect_unit *collect = get_map_buffer(tb_->collect_map, key_);
	if (!collect){ return 0; }

	dp_json_read_handle *handle = dp_json_load(command);
	if (!handle) { return 0; }

	uint64 min, max;
	int start, stop;
	int count = 0;
	int rtn = 0;
	int minX, maxX;

	s_json_node *search = dp_json_object_get_item(handle->node, "search");
	if (search) { 
		if (!dp_json_object_get_item(search, "min")) { goto exit; }
		min = dp_json_get_int(search, "min", 0);
		if (dp_json_object_get_item(search, "count")){
			count = dp_json_get_int(search, "count", 1);
			start = sts_collect_unit_search(collect, min);
			if (start >= 0) {
				rtn = sts_collect_unit_delete_of_count(collect, start, count);
			}
		}
		else {
			if (dp_json_object_get_item(search, "max")) {
				max = dp_json_get_int(search, "max", 0);
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
	s_json_node *range = dp_json_object_get_item(handle->node, "range");
	if (!range) { goto exit; }

	if (!dp_json_object_get_item(range, "start")) { goto exit; }
	start = dp_json_get_int(range, "start", -1); // -1 为最新一条记录
	if (dp_json_object_get_item(range, "count")){
		count = dp_json_get_int(range, "count", 1);
		rtn = sts_collect_unit_delete_of_count(collect, start, count); // 定位后按数量删除
	}
	else {
		if (dp_json_object_get_item(range, "stop")) {
			stop = dp_json_get_int(range, "stop", -1);// -1 为最新一条记录
			rtn = sts_collect_unit_delete_of_range(collect, start, stop); // 定位后删除
		}
		else {
			rtn = sts_collect_unit_delete_of_count(collect, start, 1); // 定位后按数量删除
		}
	}
exit:
	dp_json_close(handle);
	return rtn;
}

////////////////////////////////////////////////////////////////////////////////////////////////
//取数据,读表中代码为key的数据，key为*表示所有股票数据，由command定义数据范围和字段范围
////////////////////////////////////////////////////////////////////////////////////////////////

sds sts_table_get_m(sts_table *tb_, const char *key_, const char *command)
{
	sts_collect_unit *collect = get_map_buffer(tb_->collect_map, key_);
	if (!collect){ return NULL; }

	dp_json_read_handle *handle = dp_json_load(command);
	if (!handle) { return NULL; }

	sds out = NULL;

	// 取出数据返回格式，没有就默认为数组
	int out_format = STS_DATA_ARRAY;
	s_json_node *format = dp_json_object_get_item(handle->node, "format");
	if (format) {
		sts_map_define *smd = sts_db_find_map_define(format->value_r);
		if (smd) { out_format = smd->uid; }
	} 

	// 取出字段定义，没有就默认全部字段
	sds out_fields = NULL;
	s_json_node *fields = dp_json_object_get_item(handle->node, "fields");
	if (fields) {
		if (strcmp(fields->value_r, "*")){
			out_fields = sdsnew(fields->value_r);
		}
	}
	// 检查取值范围，没有就全部取
	uint64 min, max;
	int start, stop;
	int count = 0;
	int maxX, minX;

	s_json_node *search = dp_json_object_get_item(handle->node, "search");
	s_json_node *range = dp_json_object_get_item(handle->node, "range");
	if (!search && !range){
		out = sts_collect_unit_get_of_range_m(collect, 0, -1, out_format, out_fields);
		goto exit;
	}
	if (search) {
		if (!dp_json_object_get_item(search, "min")) { goto exit; }
		min = dp_json_get_int(search, "min", 0);
		if (dp_json_object_get_item(search, "count")){
			count = dp_json_get_int(search, "count", 1);
			start = sts_collect_unit_search(collect, min);
			if (start >= 0) {
				out = sts_collect_unit_get_of_count_m(collect, start, count, out_format, out_fields);
			}
		}
		else {
			if (dp_json_object_get_item(search, "max")) {
				max = dp_json_get_int(search, "max", 0);
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
		if (!dp_json_object_get_item(range, "start")) { goto exit; }
		start = dp_json_get_int(range, "start", -1); // -1 为最新一条记录
		if (dp_json_object_get_item(range, "count")){
			count = dp_json_get_int(range, "count", 1);
			out = sts_collect_unit_get_of_count_m(collect, start, count, out_format, out_fields);
		}
		else {
			if (dp_json_object_get_item(range, "stop")) {
				stop = dp_json_get_int(range, "stop", -1);// -1 为最新一条记录
				out = sts_collect_unit_get_of_range_m(collect, start, stop, out_format, out_fields);
			}
			else {
				out = sts_collect_unit_get_of_count_m(collect, start, 1, out_format, out_fields);
			}
		}
		goto exit;
	}
exit:
	dp_json_close(handle);
	if (out_fields) {
		sdsfree(out_fields);
	}
	return out;

}


