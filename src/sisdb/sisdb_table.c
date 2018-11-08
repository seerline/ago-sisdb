
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

// uint64 sisdb_table_struct_trans_time(uint64 in_, int inscale_, s_sisdb_table *out_tb_, int outscale_)
// {
// 	if (inscale_ > outscale_)
// 	{
// 		return in_;
// 	}
// 	if (inscale_ >= SIS_TIME_SCALE_DATE)
// 	{
// 		return in_;
// 	}
// 	uint64 o = in_;
// 	int date = sis_time_get_idate(o);
// 	if (inscale_ == SIS_TIME_SCALE_MSEC)
// 	{
// 		o = (uint64)(o / 1000);
// 	}
// 	switch (outscale_)
// 	{
// 	case SIS_TIME_SCALE_MSEC:
// 		return in_;
// 	case SIS_TIME_SCALE_SECOND:
// 		return o;
// 	case SIS_TIME_SCALE_INCR:
// 		o = sisdb_ttime_to_trade_index(o, out_tb_->father->trade_time);
// 		break;
// 	case SIS_TIME_SCALE_MIN1:
// 		o = sisdb_ttime_to_trade_index(o, out_tb_->father->trade_time);
// 		o = sisdb_trade_index_to_ttime(date, o, out_tb_->father->trade_time);
// 		break;
// 	case SIS_TIME_SCALE_MIN5:
// 		o = sisdb_ttime_to_trade_index(o, out_tb_->father->trade_time);
// 		o = sisdb_trade_index_to_ttime(date, ((int)(o / 5) + 1) * 5 - 1, out_tb_->father->trade_time);
// 		break;
// 	case SIS_TIME_SCALE_HOUR:
// 		o = sisdb_ttime_to_trade_index(o, out_tb_->father->trade_time);
// 		o = sisdb_trade_index_to_ttime(date, ((int)(o / 30) + 1) * 30 - 1, out_tb_->father->trade_time);
// 		break;
// 	case SIS_TIME_SCALE_DATE:
// 		o = date;
// 		break;
// 	}
// 	return o;
// }
// s_sis_sds _sisdb_table_struct_trans(s_sisdb_collect *in_unit_, s_sis_sds ins_, s_sisdb_collect *out_unit_)
// {
// 	// const char *src = sis_struct_list_get(out_unit_->value, out_unit_->value->count - 1);

// 	s_sisdb_table *in_tb = in_unit_->father;
// 	s_sisdb_table *out_tb = out_unit_->father;

// 	int count = (int)(sis_sdslen(ins_) / in_unit_->value->len);
// 	s_sis_sds outs_ = sis_sdsnewlen(NULL, count * out_unit_->value->len);

// 	s_sis_sds ins = ins_;
// 	s_sis_sds outs = outs_;

// 	// printf("table=%p:%p coll=%p,field=%p \n",
// 	// 			out_tb, out_unit_->father, out_unit_,
// 	// 			out_tb->field_name);
// 	int fields = sis_string_list_getsize(out_tb->field_name);

// 	for (int k = 0; k < count; k++)
// 	{
// 		for (int m = 0; m < fields; m++)
// 		{
// 			const char *key = sis_string_list_get(out_tb->field_name, m);
// 			s_sisdb_field *out_fu = (s_sisdb_field *)sis_map_buffer_get(out_tb->field_map, key);
// 			// printf("key=%s size=%d offset=%d\n", key, fields, fu->offset);
// 			s_sisdb_field *in_fu = (s_sisdb_field *)sis_map_buffer_get(in_tb->field_map, key);
// 			if (!in_fu)
// 			{
// 				continue;
// 			}
// 			sisdb_collect_struct_trans(ins, in_fu, in_tb, outs, out_fu, out_tb);
// 		}
// 		ins += in_unit_->value->len;
// 		outs += out_unit_->value->len;
// 		// 下一块数据
// 	}
// 	return outs_;
// }


// //////////////////////////////////////////////////////////////////////////////////
// // 同时修改多个数据库，key_为股票代码或市场编号，value_为二进制结构化数据或json数据
// //////////////////////////////////////////////////////////////////////////////////
// int _sisdb_table_update_links(s_sisdb_table *table_,const char *key_, s_sisdb_collect *collect_,s_sis_sds val_)
// {
// 	int count = sis_string_list_getsize(table_->publishs);
// 	// printf("publishs=%d\n", count);

// 	s_sis_sds link_val = NULL;
// 	s_sisdb_table *link_table;
// 	s_sisdb_collect *link_collect;
// 	for (int k = 0; k < count; k++)
// 	{
// 		const char *link_db = sis_string_list_get(table_->publishs, k);
// 		link_table = sisdb_get_table(table_->father, link_db);
// 		// printf("publishs=%d  db=%s  fields=%p %p \n", k, link_db,link_table,link_table->field_name);
// 		// for (int f=0; f<sis_string_list_getsize(link_table->field_name); f++){
// 		// 	printf("  fields=%s\n", sis_string_list_get(link_table->field_name,f));
// 		// }
// 		if (!link_table)
// 		{
// 			continue;
// 		}
// 		// 时间跨度大的不能转到跨度小的数据表
// 		if (table_->control.scale > link_table->control.scale)
// 		{
// 			continue;
// 		}
// 		link_collect = sis_map_buffer_get(link_table->collect_map, key_);
// 		if (!link_collect)
// 		{
// 			// printf("new......\n");
// 			link_collect = sisdb_collect_create(link_table, key_);
// 			sis_map_buffer_set(link_table->collect_map, key_, link_collect);
// 		}
// 		// printf("publishs=%d  db=%s.%s in= %p table=%p:%p coll=%p,field=%p \n", k, link_db,key_,
// 		// 		collect_->father,
// 		// 		link_table, link_collect->father, link_collect,
// 		// 		link_table->field_name);
// 		//---- 转换到其他数据库的数据格式 ----//
// 		link_val = _sisdb_table_struct_trans(collect_, val_, link_collect);
// 		//---- 修改数据 ----//
// 		if (link_val)
// 		{
// 			// printf("table_=%lld\n",sisdb_field_get_uint_from_key(table_,"time",in_val));
// 			// printf("link_table=%lld\n",sisdb_field_get_uint_from_key(link_table,"time",link_val));
// 			sisdb_collect_update(link_collect, link_val, sis_sdslen(link_val));
// 			sis_sdsfree(link_val);
// 		}
// 	}
// 	return count;
// }
// int sisdb_table_update_and_pubs(int type_, s_sisdb_table *table_, const char *key_, const char *in_, size_t ilen_)
// {
// 	// 1 . 先把来源数据，转换为srcdb的二进制结构数据集合
// 	s_sisdb_collect *in_collect = sis_map_buffer_get(table_->collect_map, key_);
// 	if (!in_collect)
// 	{
// 		in_collect = sisdb_collect_create(table_, key_);
// 		sis_map_buffer_set(table_->collect_map, key_, in_collect);
// 		//if (!sis_strcasecmp(table_->name,"min")&&!sis_strcasecmp(key_,"SH600000"))
// 		// printf("[%s.%s]new in_collect = %p = %p | %p\n",key_, table_->name,in_collect->father,table_,in_collect);
// 	}

// 	s_sis_sds in_val = NULL;

// 	switch (type_)
// 	{
// 	case SIS_DATA_TYPE_JSON:
// 		// 取json中字段，如果目标没有记录，新建或者取最后一条记录的信息为模版
// 		// 用新的数据进行覆盖，然后返回数据
// 		in_val = sisdb_collect_json_to_struct_sds(in_collect, in_, ilen_);
// 		break;
// 	case SIS_DATA_TYPE_ARRAY:
// 		in_val = sisdb_collect_array_to_struct_sds(in_collect, in_, ilen_);
// 		break;
// 	default:
// 		// 这里应该不用申请新的内存
// 		in_val = sis_sdsnewlen(in_, ilen_);
// 	}
// 	// sis_out_binary("update 0 ", in_, ilen_);

// 	// ------- val 就是hi已经转换好的完整的结构化数据 -----//
// 	// 2. 先修改自己
// 	if (!in_val)
// 	{
// 		return 0;
// 	}

// 	// 先储存上一次的数据，
// 	int o = sisdb_collect_update(in_collect, in_val, sis_sdslen(in_val));

// 	// 3. 顺序改其他数据表，
// 	// **** 修改其他数据表时应该和单独修改min不同，需要修正vol和money两个字段*****
// 	if(!table_->loading) {
// 		_sisdb_table_update_links(table_, key_, in_collect, in_val);
// 	}

// 	// 5. 释放内存
// 	// 不要重复释放
// 	// if (type_!=SIS_DATA_TYPE_JSON&&type_!=SIS_DATA_TYPE_ARRAY)
// 	if (in_val)
// 	{
// 		sis_sdsfree(in_val);
// 	}

// 	return o;
// }

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

