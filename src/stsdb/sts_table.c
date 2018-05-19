

#include "sts_table.h"
#include "sts_collect.h"
#include "sts_db.h"

//command为一个json格式字段定义
s_sts_table *sts_table_create(const char *name_, s_sts_json_node *command)
{
	s_sts_table *tb = sts_db_get_table(name_);
	if (tb)
	{
		sts_table_destroy(tb);
	}
	// 先加载默认配置
	tb = zmalloc(sizeof(s_sts_table));
	tb->control.data_type = STS_DATA_STRUCT;
	tb->control.time_scale = STS_FIELD_SECOND;
	tb->control.limit_rows = sts_json_get_int(command, "limit", 0);
	// printf("=====%s limit %d\n", name_, tb->control.limit_rows);
	tb->control.insert_mode = STS_OPTION_ALWAYS;
	tb->control.insert_mode = STS_OPTION_ALWAYS;
	tb->control.version = (uint32)sts_time_get_now();
	// 加载实际配置
	s_sts_map_define *map = NULL;
	const char *strval = NULL;

	strval = sts_json_get_str(command, "scale");
	map = sts_db_find_map_define(strval, STS_MAP_DEFINE_SCALE);
	if (map)
	{
		tb->control.time_scale = map->uid;
	}
	strval = sts_json_get_str(command, "insert-mode");
	map = sts_db_find_map_define(strval, STS_MAP_DEFINE_OPTION_MODE);
	if (map)
	{
		tb->control.insert_mode = map->uid;
	}
	strval = sts_json_get_str(command, "update-mode");
	map = sts_db_find_map_define(strval, STS_MAP_DEFINE_OPTION_MODE);
	if (map)
	{
		tb->control.update_mode = map->uid;
	}

	tb->name = sdsnew(name_);
	tb->collect_map = sts_map_pointer_create();
	//处理字段定义
	tb->field_name = sts_string_list_create_w();
	tb->field_map = sts_map_pointer_create();
	sts_table_set_fields(tb, sts_json_cmp_child_node(command, "fields"));
	return tb;
}

void sts_table_destroy(s_sts_table *tb_)
//删除一个表
{
	//删除字段定义
	dictEntry *de;
	dictIterator *di = dictGetSafeIterator(tb_->field_map);
	while ((de = dictNext(di)) != NULL)
	{
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
	while ((de = dictNext(di)) != NULL)
	{
		s_sts_collect_unit *val = (s_sts_collect_unit *)dictGetVal(de);
		sts_collect_unit_destroy(val);
	}
	dictReleaseIterator(di);
	sts_map_pointer_destroy(tb_->collect_map);
}
/////////////////////////////////////
//对数据库的各种属性设置
////////////////////////////////////
void sts_table_set_ver(s_sts_table *tb_, uint32 ver_)
{
	tb_->control.version = ver_;
}
void sts_table_set_limit_rows(s_sts_table *tb_, uint32 limits_)
{
	tb_->control.limit_rows = limits_;
}
void sts_table_set_insert_mode(s_sts_table *tb_, uint8_t insert_)
{
	tb_->control.insert_mode = insert_;
}

void sts_table_set_fields(s_sts_table *tb_, s_sts_json_node *fields_)
{
	if (!fields_)
	{
		return;
	}

	sts_map_pointer_clear(tb_->field_map);
	sts_string_list_clear(tb_->field_name);

	s_sts_json_node *node = sts_json_first_node(fields_);

	s_sts_fields_flags flags;
	s_sts_map_define *map = NULL;
	int index = 0;
	int offset = 0;
	while (node)
	{
		// size_t ss;
		// printf("node=%s\n", sts_json_output(node, &ss));
		const char *name = sts_json_get_str(node, "0");
		flags.type = STS_FIELD_INT;
		flags.len = sts_json_get_int(node, "2", 4);
		flags.io = sts_json_get_int(node, "3", 0);
		flags.zoom = sts_json_get_int(node, "4", 0);
		flags.ziper = 0; // 暂时不压缩
		flags.refer = 0;

		const char *val = sts_json_get_str(node, "1");
		map = sts_db_find_map_define(val, STS_MAP_DEFINE_FIELD_TYPE);
		if (map)
		{
			flags.type = map->uid;
		}

		s_sts_field_unit *unit =
			sts_field_unit_create(index++, name, &flags);
		unit->offset = offset;
		offset += unit->flags.len;
		printf("[%d:%d] name=%s len=%d\n", index, offset, name, flags.len);
		sts_map_pointer_set(tb_->field_map, name, unit);
		sts_string_list_push(tb_->field_name, name, strlen(name));

		node = sts_json_next_node(node);
	}
}

//获取数据库的各种值
s_sts_field_unit *sts_table_get_field(s_sts_table *tb_, const char *name_)
{
	if (!name_)
	{
		return NULL;
	}
	s_sts_field_unit *val = (s_sts_field_unit *)sts_map_pointer_get(tb_->field_map, name_);
	//	s_sts_field_unit *val = (s_sts_field_unit *)dictFetchValue(tb_->field_map, name_);
	return val;
}

int sts_table_get_fields_size(s_sts_table *tb_)
{
	int len = 0;
	dictEntry *de;
	dictIterator *di = dictGetSafeIterator(tb_->field_map);
	while ((de = dictNext(di)) != NULL)
	{
		s_sts_field_unit *val = (s_sts_field_unit *)dictGetVal(de);
		len += val->flags.len;
	}
	dictReleaseIterator(di);
	return len;
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
			out = sts_fields_get_uint(fu, (const char *)val_);
			break;
		}
	}
	return out;
}
//////////////////////////////////////////////////////////////////////////////////
//修改数据，key_为股票代码或市场编号，value_为二进制结构化数据或json数据
//////////////////////////////////////////////////////////////////////////////////

int sts_table_update(s_sts_table *tb_, const char *key_, int type_, const char *value_, size_t len_)
{
	s_sts_collect_unit *collect = sts_map_buffer_get(tb_->collect_map, key_);
	// printf("---collect %p---%p--%d\n", collect, tb_->field_map, sts_table_get_fields_size(tb_));
	if (!collect)
	{
		collect = sts_collect_unit_create(tb_, key_);
		sts_map_buffer_set(tb_->collect_map, key_, collect);
	}
	// printf("---collect %p---\n", collect);
	int o = 0;
	sds val = NULL;
	switch (type_)
	{
	case STS_DATA_JSON:
		// 取json中字段，如果目标没有记录，新建或者取最后一条记录的信息为模版
		// 用新的数据进行覆盖，然后返回数据
		val = sts_collect_json_to_struct(collect, value_, len_);
		if (val)
		{
			o = sts_collect_unit_update(collect, val, sdslen(val));
			sdsfree(val);
		}
		break;
	case STS_DATA_ARRAY:
		val = sts_collect_array_to_struct(collect, value_, len_);
		if (val)
		{
			o = sts_collect_unit_update(collect, val, sdslen(val));
			sdsfree(val);
		}
		break;
	default:
		o = sts_collect_unit_update(collect, value_, len_);
	}
	return o;
}
//////////////////////////
//删除数据
//////////////////////////
int sts_table_delete(s_sts_table *tb_, const char *key_, const char *command)
{
	s_sts_collect_unit *collect = sts_map_buffer_get(tb_->collect_map, key_);
	if (!collect)
	{
		return 0;
	}

	s_sts_json_handle *handle = sts_json_load(command, strlen(command));
	if (!handle)
	{
		return 0;
	}

	uint64 min, max;
	int start, stop;
	int count = 0;
	int rtn = 0;
	int minX, maxX;

	s_sts_json_node *search = sts_json_cmp_child_node(handle->node, "search");
	if (search)
	{
		if (!sts_json_cmp_child_node(search, "min"))
		{
			goto exit;
		}
		min = sts_json_get_int(search, "min", 0);
		if (sts_json_cmp_child_node(search, "count"))
		{
			count = sts_json_get_int(search, "count", 1);
			start = sts_collect_unit_search(collect, min);
			if (start >= 0)
			{
				rtn = sts_collect_unit_delete_of_count(collect, start, count);
			}
		}
		else
		{
			if (sts_json_cmp_child_node(search, "max"))
			{
				max = sts_json_get_int(search, "max", 0);
				start = sts_collect_unit_search_right(collect, min, &minX);
				stop = sts_collect_unit_search_left(collect, max, &maxX);
				if (minX != STS_SEARCH_NONE && maxX != STS_SEARCH_NONE)
				{
					rtn = sts_collect_unit_delete_of_range(collect, start, stop);
				}
			}
			else
			{
				start = sts_collect_unit_search(collect, min);
				if (start >= 0)
				{
					rtn = sts_collect_unit_delete_of_count(collect, start, 1);
				}
			}
		}
		goto exit;
	}
	// 按界限操作
	s_sts_json_node *range = sts_json_cmp_child_node(handle->node, "range");
	if (!range)
	{
		goto exit;
	}

	if (!sts_json_cmp_child_node(range, "start"))
	{
		goto exit;
	}
	start = sts_json_get_int(range, "start", -1); // -1 为最新一条记录
	if (sts_json_cmp_child_node(range, "count"))
	{
		count = sts_json_get_int(range, "count", 1);
		rtn = sts_collect_unit_delete_of_count(collect, start, count); // 定位后按数量删除
	}
	else
	{
		if (sts_json_cmp_child_node(range, "stop"))
		{
			stop = sts_json_get_int(range, "stop", -1);					  // -1 为最新一条记录
			rtn = sts_collect_unit_delete_of_range(collect, start, stop); // 定位后删除
		}
		else
		{
			rtn = sts_collect_unit_delete_of_count(collect, start, 1); // 定位后按数量删除
		}
	}
exit:
	sts_json_close(handle);
	return rtn;
}

///////////////////////////////////////////////////////////////////////////////
//取数据,读表中代码为key的数据，key为*表示所有股票数据，由command定义数据范围和字段范围
///////////////////////////////////////////////////////////////////////////////
sds sts_table_get_m(s_sts_table *tb_, const char *key_, const char *command)
{
	s_sts_json_handle *handle = sts_json_load(command, strlen(command));
	if (!handle)
	{
		return NULL;
	}

	s_sts_collect_unit *collect = sts_map_buffer_get(tb_->collect_map, key_);
	if (!collect)
	{
		return NULL;
	}

	// 检查取值范围，没有就全部取
	sds out = NULL;

	int64 min, max;
	int start, stop;
	int count = 0;
	int maxX, minX;

	s_sts_json_node *search = sts_json_cmp_child_node(handle->node, "search");
	s_sts_json_node *range = sts_json_cmp_child_node(handle->node, "range");
	if (!search && !range) // 这两个互斥，以search为首发
	{
		out = sts_collect_unit_get_of_range_m(collect, 0, -1);
		goto filter;
	}
	if (search)
	{
		if (!sts_json_cmp_child_node(search, "min"))
		{
			goto filter;
		}
		min = sts_json_get_int(search, "min", 0);
		if (sts_json_cmp_child_node(search, "count"))
		{
			count = sts_json_get_int(search, "count", 1);
			start = sts_collect_unit_search(collect, min);
			if (start >= 0)
			{
				out = sts_collect_unit_get_of_count_m(collect, start, count);
			}
		}
		else
		{
			if (sts_json_cmp_child_node(search, "max"))
			{
				max = sts_json_get_int(search, "max", 0);
				start = sts_collect_unit_search_right(collect, min, &minX);
				stop = sts_collect_unit_search_left(collect, max, &maxX);
				if (minX != STS_SEARCH_NONE && maxX != STS_SEARCH_NONE)
				{
					out = sts_collect_unit_get_of_range_m(collect, start, stop);
				}
			}
			else
			{
				start = sts_collect_unit_search(collect, min);
				if (start >= 0)
				{
					out = sts_collect_unit_get_of_count_m(collect, start, 1);
				}
			}
		}
		goto filter;
	}
	if (range)
	{
		if (!sts_json_cmp_child_node(range, "start"))
		{
			goto filter;
		}
		start = sts_json_get_int(range, "start", -1); // -1 为最新一条记录
		if (sts_json_cmp_child_node(range, "count"))
		{
			count = sts_json_get_int(range, "count", 1);
			out = sts_collect_unit_get_of_count_m(collect, start, count);
		}
		else
		{
			if (sts_json_cmp_child_node(range, "stop"))
			{
				stop = sts_json_get_int(range, "stop", -1); // -1 为最新一条记录
				out = sts_collect_unit_get_of_range_m(collect, start, stop);
			}
			else
			{
				out = sts_collect_unit_get_of_count_m(collect, start, 1);
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
	int iformat = STS_DATA_STRUCT;
	s_sts_json_node *format = sts_json_cmp_child_node(handle->node, "format");
	if (format)
	{
		s_sts_map_define *smd = sts_db_find_map_define(format->value, STS_MAP_DEFINE_DATA_TYPE);
		if (smd)
		{
			iformat = smd->uid;
		}
	}
	printf("iformat = %c\n", iformat);
	// 取出字段定义，没有就默认全部字段
	sds sds_fields = NULL;
	s_sts_json_node *fields = sts_json_cmp_child_node(handle->node, "fields");
	if (fields)
	{
		sds_fields = sdsnew(fields->value);
	}
	printf("sds_fields = %s\n", sds_fields);
	sds other = NULL;
	switch (iformat)
	{
	case STS_DATA_STRUCT:
		if (!sts_check_fields_all(sds_fields))
		{
			other = sts_collect_struct_filter(collect, out, sds_fields);
			sdsfree(out);
			out = other;
		}
		break;
	case STS_DATA_JSON:
		other = sts_collect_struct_to_json(collect, out, sds_fields);
		sdsfree(out);
		out = other;
		break;
	case STS_DATA_ARRAY:
		other = sts_collect_struct_to_array(collect, out, sds_fields);
		sdsfree(out);
		out = other;
		break;
	}
	if (sds_fields)
	{
		sdsfree(sds_fields);
	}
nodata:
	sts_json_close(handle);
	return out;
}
