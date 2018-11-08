
#include "sisdb_table.h"
#include "sisdb_collect.h"
#include "sisdb_map.h"
#include "sisdb.h"

//com_为一个json格式字段定义
s_sisdb_table *sisdb_table_create(s_sis_db *db_, const char *name_, s_sis_json_node *com_)
{
	s_sisdb_table *tb = sisdb_get_table(db_, name_);
	if (tb)
	{
		sisdb_table_destroy(tb);
	}
	// 先加载默认配置
	tb = sis_malloc(sizeof(s_sisdb_table));
	memset(tb, 0, sizeof(s_sisdb_table));

	tb->control.type = SIS_TABLE_TYPE_STS; // 默认保存的目前都是struct，
	tb->control.scale = SIS_TIME_SCALE_SECOND;
	tb->control.limits = sis_json_get_int(com_, "limit", 0);
	tb->control.isinit = sis_json_get_int(com_, "isinit", 0);
	tb->control.issubs = 0;
	tb->control.iszip = 0;
	tb->control.isloading = 0;

	tb->version = (uint32)sis_time_get_now();
	tb->name = sis_sdsnew(name_);
	tb->father = db_;
	tb->append_method = SIS_ADD_METHOD_ALWAYS;

	sis_dict_add(db_->dbs, sis_sdsnew(name_), tb);

	// 加载实际配置
	s_sis_map_define *mm = NULL;
	const char *strval = NULL;

	strval = sis_json_get_str(com_, "scale");
	mm = sisdb_find_map_define(db_->map, strval, SIS_MAP_DEFINE_TIME_SCALE);
	if (mm)
	{
		tb->control.scale = mm->uid;
	}

	strval = sis_json_get_str(com_, "append-method");
	int nums = sis_str_substr_nums(strval, ',');
	if (nums < 1 && tb->control.limits == 0)
	{
		tb->control.limits = 1;
	}
	for (int i=0; i < nums; i++) 
	{
		char mode[32];
		sis_str_substr(mode, 32, strval, ',', i);
		mm = sisdb_find_map_define(db_->map, mode, SIS_MAP_DEFINE_ADD_METHOD);
		if (mm)
		{
			tb->append_method |= mm->uid;
		}
	}
	//处理链接数据表名
	tb->publishs = sis_string_list_create_w();

	if (sis_json_cmp_child_node(com_, "publishs"))
	{
		strval = sis_json_get_str(com_, "publishs");
		sis_string_list_load(tb->publishs, strval, strlen(strval), ",");
	}

	//处理字段定义
	tb->field_name = sis_string_list_create_w();
	tb->field_map = sis_map_pointer_create();

	// 顺序不能变，必须最后
	sisdb_table_set_fields(tb, sis_json_cmp_child_node(com_, "fields"));
	// int count = sis_string_list_getsize(tb->field_name);
	// for (int i = 0; i < count; i++)
	// {
	// 	printf("---111  %s\n",sis_string_list_get(tb->field_name, i));
	// }

	s_sis_json_node *subs = sis_json_cmp_child_node(com_, "subscribe-method");
	if (subs)
	{
		tb->control.issubs = 1;
		s_sis_json_node *child = subs->child;
		while (child)
		{
			s_sisdb_field *fu = sisdb_field_get_from_key(tb, child->key);
			if (fu)
			{
				fu->subscribe_method = SIS_SUBS_METHOD_COPY;
				map = sisdb_find_map_define(db_->map, sis_json_get_str(child, "0"), SIS_MAP_DEFINE_SUBS_METHOD);
				if (map)
				{
					fu->subscribe_method = map->uid;
				}
				sis_strcpy(fu->subscribe_refer_fields, SIS_FIELD_MAXLEN, sis_json_get_str(child, "1"));
				// printf("%s==%d  %s--- %s\n", child->key, fu->subscribe_method, fu->subscribe_refer_fields,fu->name);
			}
			// printf("[%s] %s=====%p\n", tb->name, child->key, fu);
			child = child->next;
		}
	}

	s_sis_json_node *zip = sis_json_cmp_child_node(com_, "zip-method");
	if (zip)
	{
		tb->control.iszip = 1;
	}
	return tb;
}

void sisdb_table_destroy(s_sisdb_table *tb_)
//删除一个表
{
	// //删除字段定义
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(tb_->field_map);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sisdb_field *val = (s_sisdb_field *)sis_dict_getval(de);
		sisdb_field_destroy(val);
	}
	sis_dict_iter_free(di);
	sis_map_pointer_destroy(tb_->field_map);

	sis_string_list_destroy(tb_->publishs);

	sis_string_list_destroy(tb_->field_name);
	
	sis_sdsfree(tb_->name);
	sis_free(tb_);
}

/////////////////////////////////////
//对数据库的各种属性设置
////////////////////////////////////

int sisdb_table_set_fields(s_sisdb_table *tb_, s_sis_json_node *fields_)
{
	int o = 0
	if (!fields_)
	{
		return o;
	}

	sis_map_pointer_clear(tb_->field_map);
	sis_string_list_clear(tb_->field_name);

	s_sis_json_node *node = sis_json_first_node(fields_);

	s_sisdb_field_flags flags;
	s_sis_map_define *map = NULL;
	int index = 0;
	int offset = 0;
	while (node)
	{
		// size_t ss;
		// printf("node=%s\n", sis_json_output(node, &ss));
		const char *name = sis_json_get_str(node, "0");

		flags.type = SIS_FIELD_TYPE_INT;
		const char *val = sis_json_get_str(node, "1");
		map = sisdb_find_map_define(tb_->father->map, val, SIS_MAP_DEFINE_FIELD_TYPE);
		if (map)
		{
			flags.type = map->uid;
		}

		flags.len = sis_json_get_int(node, "2", 4);
		if(flags.type==SIS_FIELD_TYPE_STRING||flags.type==SIS_FIELD_TYPE_JSON)
		{
			flags.len = sizeof(void *);
		}
		flags.dot = sis_json_get_int(node, "3", 0);

		s_sisdb_field *unit = sisdb_field_create(index++, name, &flags);
		unit->offset = offset;
		offset += unit->flags.len;
		// printf("[%d:%d] name=%s len=%d\n", index, offset, name, flags.len);
		sis_map_pointer_set(tb_->field_map, name, unit);
		sis_string_list_push(tb_->field_name, name, strlen(name));

		node = sis_json_next_node(node);
	}
	return o;
}

int sisdb_table_get_fields_size(s_sisdb_table *tb_)
{
	int len = 0;
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(tb_->field_map);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sisdb_field *val = (s_sisdb_field *)sis_dict_getval(de);
		len += val->flags.len;
	}
	sis_dict_iter_free(di);
	return len;
}






// int sisdb_table_update_load(int type_, s_sisdb_table *table_, const char *key_, const char *in_, size_t ilen_)
// {
// 	s_sisdb_collect *in_collect = sis_map_buffer_get(table_->collect_map, key_);
// 	if (!in_collect)
// 	{
// 		in_collect = sisdb_collect_create(table_, key_);
// 		sis_map_buffer_set(table_->collect_map, key_, in_collect);
// 	}

// 	// 先储存上一次的数据，
// 	int o = sisdb_collect_update_block(in_collect, in_, ilen_);

// 	return o;
// }
// //////////////////////////////////////////////////////////////////////////////////
// //修改数据，key_为股票代码或市场编号，in_为二进制结构化数据或json数据
// //////////////////////////////////////////////////////////////////////////////////




// s_sis_sds sisdb_table_get_search_sds(s_sisdb_table *tb_, const char *code_, int min_,int max_)
// {
// 	s_sisdb_collect *collect = sis_map_buffer_get(tb_->collect_map, code_);
// 	if (!collect)
// 	{
// 		return NULL;
// 	}

// 	s_sis_sds o = NULL;

// 	int start, stop;
// 	int maxX, minX;

// 	start = sisdb_collect_search_right(collect, min_, &minX);
// 	stop = sisdb_collect_search_left(collect, max_, &maxX);
// 	if (minX != SIS_SEARCH_NONE && maxX != SIS_SEARCH_NONE)
// 	{
// 		o = sisdb_collect_get_of_range_sds(collect, start, stop);
// 	}	
// 	return o;
// }

