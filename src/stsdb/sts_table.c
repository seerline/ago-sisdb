

#include "sts_table.h"
#include "sts_collect.h"
#include "sts_db.h"

//command为一个json格式字段定义
s_sts_table *sts_table_create(s_sts_db *db_, const char *name_, s_sts_json_node *command)
{
	s_sts_table *tb = sts_db_get_table(db_, name_);
	if (tb)
	{
		sts_table_destroy(tb);
	}
	// 先加载默认配置
	tb = sts_malloc(sizeof(s_sts_table));
	memset(tb, 0, sizeof(s_sts_table));

	tb->control.data_type = STS_DATA_STRUCT; // 默认保存的目前都是struct，
	tb->control.time_scale = STS_SCALE_SECOND;
	tb->control.limit_rows = sts_json_get_int(command, "limit", 0);
	tb->control.isinit = sts_json_get_int(command, "isinit", 0);
	// printf("=====%s limit %d\n", name_, tb->control.limit_rows);
	tb->control.insert_mode = STS_OPTION_ALWAYS;
	tb->control.insert_mode = STS_OPTION_ALWAYS;
	tb->control.version = (uint32)sts_time_get_now();

	sts_dict_add(db_->db, sts_sdsnew(name_), tb);
	tb->father = db_;

	// 加载实际配置
	s_sts_map_define *map = NULL;
	const char *strval = NULL;

	strval = sts_json_get_str(command, "scale");
	map = sts_db_find_map_define(db_, strval, STS_MAP_DEFINE_SCALE);
	if (map)
	{
		tb->control.time_scale = map->uid;
	}
	strval = sts_json_get_str(command, "insert-mode");
	map = sts_db_find_map_define(db_, strval, STS_MAP_DEFINE_OPTION_MODE);
	if (map)
	{
		tb->control.insert_mode = map->uid;
	}
	strval = sts_json_get_str(command, "update-mode");
	map = sts_db_find_map_define(db_, strval, STS_MAP_DEFINE_OPTION_MODE);
	if (map)
	{
		tb->control.update_mode = map->uid;
	}

	tb->name = sts_sdsnew(name_);
	tb->collect_map = sts_map_pointer_create();
	//处理链接数据表名
	tb->links = sts_string_list_create_w();

	if (sts_json_cmp_child_node(command, "links"))
	{
		strval = sts_json_get_str(command, "links");
		sts_string_list_load(tb->links, strval, strlen(strval), ",");
	}

	//处理字段定义
	tb->field_name = sts_string_list_create_w();
	tb->field_map = sts_map_pointer_create();

	// 顺序不能变，必须最后
	sts_table_set_fields(tb, sts_json_cmp_child_node(command, "fields"));
	// int count = sts_string_list_getsize(tb->field_name);
	// for (int i = 0; i < count; i++)
	// {
	// 	printf("---111  %s\n",sts_string_list_get(tb->field_name, i));
	// }

	s_sts_json_node *cache = sts_json_cmp_child_node(command, "fields-cache");
	if (cache)
	{
		tb->catch = true;
		s_sts_json_node *child = cache->child;
		while (child)
		{
			s_sts_field_unit *fu = sts_field_get_from_key(tb, child->key);
			if (fu)
			{
				fu->catch_method = STS_FIELD_METHOD_COVER;
				map = sts_db_find_map_define(db_, sts_json_get_str(child, "0"), STS_MAP_DEFINE_FIELD_METHOD);
				if (map)
				{
					fu->catch_method = map->uid;
				}
				sts_strcpy(fu->catch_initfield, STS_FIELD_MAXLEN, sts_json_get_str(child, "1"));
				// printf("%s==%d  %s--- %s\n", child->key, fu->catch_method, fu->catch_initfield,fu->name);
			}
			// printf("[%s] %s=====%p\n", tb->name, child->key, fu);
			child = child->next;
		}
	}

	s_sts_json_node *zip = sts_json_cmp_child_node(command, "zip-method");
	if (zip)
	{
		tb->zip = true;
	}
	return tb;
}

void sts_table_destroy(s_sts_table *tb_)
//删除一个表
{
	// //删除字段定义
	s_sts_dict_entry *de;
	s_sts_dict_iter *di = sts_dict_get_iter(tb_->field_map);
	while ((de = sts_dict_next(di)) != NULL)
	{
		s_sts_field_unit *val = (s_sts_field_unit *)sts_dict_getval(de);
		sts_field_unit_destroy(val);
	}
	sts_dict_iter_free(di);
	sts_map_pointer_destroy(tb_->field_map);

	sts_string_list_destroy(tb_->links);

	sts_string_list_destroy(tb_->field_name);
	//删除数据区
	sts_table_collect_clear(tb_);

	sts_sdsfree(tb_->name);
	sts_free(tb_);
}
void sts_table_collect_clear(s_sts_table *tb_)
//清理一个表的所有数据
{
	s_sts_dict_entry *de;
	s_sts_dict_iter *di = sts_dict_get_iter(tb_->collect_map);
	while ((de = sts_dict_next(di)) != NULL)
	{
		s_sts_collect_unit *val = (s_sts_collect_unit *)sts_dict_getval(de);
		sts_collect_unit_destroy(val);
	}
	sts_dict_iter_free(di);
	// sts_map_buffer_clear(tb_->collect_map);
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
		map = sts_db_find_map_define(tb_->father, val, STS_MAP_DEFINE_FIELD_TYPE);
		if (map)
		{
			flags.type = map->uid;
		}

		s_sts_field_unit *unit =
			sts_field_unit_create(index++, name, &flags);
		unit->offset = offset;
		offset += unit->flags.len;
		// printf("[%d:%d] name=%s len=%d\n", index, offset, name, flags.len);
		sts_map_pointer_set(tb_->field_map, name, unit);
		sts_string_list_push(tb_->field_name, name, strlen(name));

		node = sts_json_next_node(node);
	}
}

int sts_table_get_fields_size(s_sts_table *tb_)
{
	int len = 0;
	s_sts_dict_entry *de;
	s_sts_dict_iter *di = sts_dict_get_iter(tb_->field_map);
	while ((de = sts_dict_next(di)) != NULL)
	{
		s_sts_field_unit *val = (s_sts_field_unit *)sts_dict_getval(de);
		len += val->flags.len;
	}
	sts_dict_iter_free(di);
	return len;
}

// uint64 sts_table_get_times(s_sts_table *tb_, void *val_)
// {
// 	uint64 out = 0;
// 	int count = sts_string_list_getsize(tb_->field_name);
// 	for (int i = 0; i < count; i++)
// 	{
// 		s_sts_field_unit *fu = (s_sts_field_unit *)sts_map_buffer_get(tb_->field_map, sts_string_list_get(tb_->field_name, i));
// 		if (!fu)
// 		{
// 			continue;
// 		}
// 		if (sts_field_is_time(fu))
// 		{
// 			out = sts_fields_get_uint(fu, (const char *)val_);
// 			break;
// 		}
// 	}
// 	return out;
// }

//time_t 返回０－２３９
// 不允许返回-1
uint16 _sts_ttime_to_trade_index(uint64 ttime_, s_sts_struct_list *tradetime_)
{
	int tt = sts_time_get_iminute(ttime_);

	int nowmin = 0;
	int out = 0;

	s_sts_time_pair *pair;
	for (int i = 0; i < tradetime_->count; i++)
	{
		pair = (s_sts_time_pair *)sts_struct_list_get(tradetime_, i);
		if (tt > pair->first && tt < pair->second) //9:31:00--11:29:59  13:01:00--14:59:59
		{
			out = nowmin + sts_time_get_iminute_offset_i(pair->first, tt);
			break;
		}
		if (tt <= pair->first && i == 0)
		{ // 8:00:00---9:30:59秒前都=0
			return 0;
		}
		if (tt <= pair->first && (tt > sts_time_get_iminute_minnum(pair->first, -5)))
		{ //12:55:59--13:00:59秒
			return nowmin;
		}

		nowmin += sts_time_get_iminute_offset_i(pair->first, pair->second);

		if (tt >= pair->second && i == tradetime_->count - 1)
		{ //15:00:00秒后
			return nowmin - 1;
		}
		// if (tt >= pair->second && (tt < sts_time_get_iminute_minnum(pair->second, 5)))
		// { //11:30:00--11:34:59秒
		// 	return nowmin - 1;
		// }
		if (tt >= pair->second)
		{ //11:30:00--11:34:59秒
			if (tt < sts_time_get_iminute_minnum(pair->second, 5))
			{
				return nowmin - 1;
			}
			else
			{
				out = nowmin;
			}
		}
	}
	return out;
}
uint64 _sts_trade_index_to_ttime(int date_, int nIndex_, s_sts_struct_list *tradetime_)
{
	int tt = nIndex_;
	int nowmin = 0;
	s_sts_time_pair *pair = (s_sts_time_pair *)sts_struct_list_last(tradetime_);
	uint64 out = sts_time_make_time(date_, pair->second * 100);
	for (int i = 0; i < tradetime_->count; i++)
	{
		pair = (s_sts_time_pair *)sts_struct_list_get(tradetime_, i);
		nowmin = sts_time_get_iminute_offset_i(pair->first, pair->second);
		if (tt < nowmin)
		{
			int min = sts_time_get_iminute_minnum(pair->first, tt + 1);
			out = sts_time_make_time(date_, min * 100);
			break;
		}
		tt -= nowmin;
	}
	return out;
}
uint64 sts_table_struct_trans_time(uint64 in_, int inscale_, s_sts_table *out_tb_, int outscale_)
{
	if (inscale_ > outscale_)
	{
		return in_;
	}
	if (inscale_ >= STS_SCALE_DAY)
	{
		return in_;
	}
	uint64 o = in_;
	int date = sts_time_get_idate(o);
	if (inscale_ == STS_SCALE_MSEC)
	{
		o = (uint64)(o / 1000);
	}
	switch (outscale_)
	{
	case STS_SCALE_MSEC:
		return in_;
	case STS_SCALE_SECOND:
		return o;
	case STS_SCALE_INDEX:
		o = _sts_ttime_to_trade_index(o, out_tb_->father->trade_time);
		break;
	case STS_SCALE_MIN1:
		o = _sts_ttime_to_trade_index(o, out_tb_->father->trade_time);
		o = _sts_trade_index_to_ttime(date, o, out_tb_->father->trade_time);
		break;
	case STS_SCALE_MIN5:
		o = _sts_ttime_to_trade_index(o, out_tb_->father->trade_time);
		o = _sts_trade_index_to_ttime(date, ((int)(o / 5) + 1) * 5 - 1, out_tb_->father->trade_time);
		break;
	case STS_SCALE_MIN30:
		o = _sts_ttime_to_trade_index(o, out_tb_->father->trade_time);
		o = _sts_trade_index_to_ttime(date, ((int)(o / 30) + 1) * 30 - 1, out_tb_->father->trade_time);
		break;
	case STS_SCALE_DAY:
		o = date;
		break;
	}
	return o;
}
s_sts_sds _sts_table_struct_trans(s_sts_collect_unit *in_unit_, s_sts_sds ins_, s_sts_collect_unit *out_unit_)
{
	// const char *src = sts_struct_list_get(out_unit_->value, out_unit_->value->count - 1);

	s_sts_table *in_tb = in_unit_->father;
	s_sts_table *out_tb = out_unit_->father;

	int count = (int)(sts_sdslen(ins_) / in_unit_->value->len);
	s_sts_sds outs_ = sts_sdsnewlen(NULL, count * out_unit_->value->len);

	s_sts_sds ins = ins_;
	s_sts_sds outs = outs_;

	// printf("table=%p:%p coll=%p,field=%p \n",
	// 			out_tb, out_unit_->father, out_unit_,
	// 			out_tb->field_name);
	int fields = sts_string_list_getsize(out_tb->field_name);

	for (int k = 0; k < count; k++)
	{
		for (int m = 0; m < fields; m++)
		{
			const char *key = sts_string_list_get(out_tb->field_name, m);
			s_sts_field_unit *out_fu = (s_sts_field_unit *)sts_map_buffer_get(out_tb->field_map, key);
			// printf("key=%s size=%d offset=%d\n", key, fields, fu->offset);
			s_sts_field_unit *in_fu = (s_sts_field_unit *)sts_map_buffer_get(in_tb->field_map, key);
			if (!in_fu)
			{
				continue;
			}
			sts_collect_struct_trans(ins, in_fu, in_tb, outs, out_fu, out_tb);
		}
		ins += in_unit_->value->len;
		outs += out_unit_->value->len;
		// 下一块数据
	}
	return outs_;
}

// s_sts_sds _sts_table_struct_trans_incr(s_sts_collect_unit *in_unit_, s_sts_sds ins_, s_sts_sds dbs_, s_sts_collect_unit *out_unit_, s_sts_string_list *fields_)
// {
// 	// 需要求差值然后根据情况不同累计到目标数据表中
// 	s_sts_table *in_tb = in_unit_->father;
// 	s_sts_table *out_tb = out_unit_->father;

// 	int count = (int)(sts_sdslen(ins_) / in_unit_->value->len);
// 	s_sts_sds outs_ = sts_sdsnewlen(NULL, count * out_unit_->value->len);

// 	s_sts_sds ins = ins_;
// 	s_sts_sds dbs = dbs_;
// 	s_sts_sds outs = outs_;

// 	int fields = sts_string_list_getsize(out_tb->field_name);

// 	for (int k = 0; k < count; k++)
// 	{
// 		for (int m = 0; m < fields; m++)
// 		{
// 			const char *key = sts_string_list_get(out_tb->field_name, m);
// 			s_sts_field_unit *out_fu = (s_sts_field_unit *)sts_map_buffer_get(out_tb->field_map, key);
// 			// printf("key=%s size=%d offset=%d\n", key, fields, fu->offset);
// 			s_sts_field_unit *in_fu = (s_sts_field_unit *)sts_map_buffer_get(in_tb->field_map, key);
// 			if (!in_fu)
// 			{
// 				continue;
// 			}
// 			if (sts_string_list_indexof(fields_,key)) // 需要做增量计算
// 			{
// 				sts_collect_struct_trans_incr(ins, dbs, in_fu, in_tb, outs, out_fu, out_tb);
// 			} else {
// 				sts_collect_struct_trans(ins, in_fu, in_tb, outs, out_fu, out_tb);
// 			}
// 		}
// 		dbs = ins;  // 和上一条记录比较
// 		ins += in_unit_->value->len;
// 		outs += out_unit_->value->len;
// 		// 下一块数据
// 	}
// 	return outs_;
// }
//////////////////////////////////////////////////////////////////////////////////
// 同时修改多个数据库，key_为股票代码或市场编号，value_为二进制结构化数据或json数据
//////////////////////////////////////////////////////////////////////////////////
int _sts_table_update_links(s_sts_table *table_,const char *key_, s_sts_collect_unit *collect_,s_sts_sds val_)
{
	int count = sts_string_list_getsize(table_->links);
	// printf("links=%d\n", count);

	s_sts_sds link_val = NULL;
	s_sts_table *link_table;
	s_sts_collect_unit *link_collect;
	for (int k = 0; k < count; k++)
	{
		const char *link_db = sts_string_list_get(table_->links, k);
		link_table = sts_db_get_table(table_->father, link_db);
		// printf("links=%d  db=%s  fields=%p %p \n", k, link_db,link_table,link_table->field_name);
		// for (int f=0; f<sts_string_list_getsize(link_table->field_name); f++){
		// 	printf("  fields=%s\n", sts_string_list_get(link_table->field_name,f));
		// }
		if (!link_table)
		{
			continue;
		}
		// 时间跨度大的不能转到跨度小的数据表
		if (table_->control.time_scale > link_table->control.time_scale)
		{
			continue;
		}
		link_collect = sts_map_buffer_get(link_table->collect_map, key_);
		if (!link_collect)
		{
			// printf("new......\n");
			link_collect = sts_collect_unit_create(link_table, key_);
			sts_map_buffer_set(link_table->collect_map, key_, link_collect);
		}
		// printf("links=%d  db=%s.%s in= %p table=%p:%p coll=%p,field=%p \n", k, link_db,key_,
		// 		collect_->father,
		// 		link_table, link_collect->father, link_collect,
		// 		link_table->field_name);
		//---- 转换到其他数据库的数据格式 ----//
		link_val = _sts_table_struct_trans(collect_, val_, link_collect);
		//---- 修改数据 ----//
		if (link_val)
		{
			// printf("table_=%lld\n",sts_fields_get_uint_from_key(table_,"time",in_val));
			// printf("link_table=%lld\n",sts_fields_get_uint_from_key(link_table,"time",link_val));
			sts_collect_unit_update(link_collect, link_val, sts_sdslen(link_val));
			sts_sdsfree(link_val);
		}
	}
	return count;
}
int sts_table_update_mul(int type_, s_sts_table *table_, const char *key_, const char *in_, size_t ilen_)
{
	// 1 . 先把来源数据，转换为srcdb的二进制结构数据集合
	s_sts_collect_unit *in_collect = sts_map_buffer_get(table_->collect_map, key_);
	if (!in_collect)
	{
		in_collect = sts_collect_unit_create(table_, key_);
		sts_map_buffer_set(table_->collect_map, key_, in_collect);
		//if (!sts_strcasecmp(table_->name,"min")&&!sts_strcasecmp(key_,"SH600000"))
		// printf("[%s.%s]new in_collect = %p = %p | %p\n",key_, table_->name,in_collect->father,table_,in_collect);
	}

	s_sts_sds in_val = NULL;

	switch (type_)
	{
	case STS_DATA_JSON:
		// 取json中字段，如果目标没有记录，新建或者取最后一条记录的信息为模版
		// 用新的数据进行覆盖，然后返回数据
		in_val = sts_collect_json_to_struct_sds(in_collect, in_, ilen_);
		break;
	case STS_DATA_ARRAY:
		in_val = sts_collect_array_to_struct_sds(in_collect, in_, ilen_);
		break;
	default:
		in_val = sts_sdsnewlen(in_, ilen_);
	}
	// sts_out_binary("update 0 ", in_, ilen_);

	// ------- val 就是hi已经转换好的完整的结构化数据 -----//
	// 2. 先修改自己
	if (!in_val)
	{
		return 0;
	}
	// printf("[%s] in_collect->value = %p\n",in_collect->father->name,in_collect->value);
	// s_sts_sds db_val = sts_sdsnewlen(sts_struct_list_last(in_collect->value),
	// 					   in_collect->value->len);
	// 先储存上一次的数据，
	int o = sts_collect_unit_update(in_collect, in_val, sts_sdslen(in_val));

	// 3. 顺序改其他数据表，
	// **** 修改其他数据表时应该和单独修改min不同，需要修正vol和money两个字段*****
	if(!table_->loading) {
		// _sts_table_update_links(table_, key_, in_collect, in_val);
	}

	// 5. 释放内存
	if (in_val)
	{
		sts_sdsfree(in_val);
	}
	// if (db_val)
	// {
	// 	sts_sdsfree(db_val);
	// }
	return o;
}
//////////////////////////////////////////////////////////////////////////////////
//修改数据，key_为股票代码或市场编号，in_为二进制结构化数据或json数据
//////////////////////////////////////////////////////////////////////////////////

// int sts_table_update(int type_, s_sts_table *tb_, const char *key_, const char *in_, size_t ilen_)
// {
// 	s_sts_collect_unit *collect = sts_map_buffer_get(tb_->collect_map, key_);
// 	// printf("---collect %p---%p--%d\n", collect, tb_->field_map, sts_table_get_fields_size(tb_));
// 	if (!collect)
// 	{
// 		collect = sts_collect_unit_create(tb_, key_);
// 		sts_map_buffer_set(tb_->collect_map, key_, collect);
// 	}
// 	// printf("---collect %p---\n", collect);
// 	int o = 0;
// 	s_sts_sds val = NULL;
// 	switch (type_)
// 	{
// 	case STS_DATA_JSON:
// 		// 取json中字段，如果目标没有记录，新建或者取最后一条记录的信息为模版
// 		// 用新的数据进行覆盖，然后返回数据
// 		val = sts_collect_json_to_struct_sds(collect, in_, ilen_);
// 		if (val)
// 		{
// 			o = sts_collect_unit_update(collect, val, sts_sdslen(val));
// 			sts_sdsfree(val);
// 		}
// 		break;
// 	case STS_DATA_ARRAY:
// 		val = sts_collect_array_to_struct_sds(collect, in_, ilen_);
// 		if (val)
// 		{
// 			o = sts_collect_unit_update(collect, val, sts_sdslen(val));
// 			sts_sdsfree(val);
// 		}
// 		break;
// 	default:
// 		o = sts_collect_unit_update(collect, in_, ilen_);
// 	}
// 	return o;
// }
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
s_sts_sds sts_table_get_sds(s_sts_table *tb_, const char *key_, const char *command)
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
	s_sts_sds out = NULL;

	int64 min, max;
	int start, stop;
	int count = 0;
	int maxX, minX;

	s_sts_json_node *search = sts_json_cmp_child_node(handle->node, "search");
	s_sts_json_node *range = sts_json_cmp_child_node(handle->node, "range");
	if (!search && !range) // 这两个互斥，以search为首发
	{
		out = sts_collect_unit_get_of_range_sds(collect, 0, -1);
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
				out = sts_collect_unit_get_of_count_sds(collect, start, count);
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
					out = sts_collect_unit_get_of_range_sds(collect, start, stop);
				}
			}
			else
			{
				start = sts_collect_unit_search(collect, min);
				if (start >= 0)
				{
					out = sts_collect_unit_get_of_count_sds(collect, start, 1);
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
			out = sts_collect_unit_get_of_count_sds(collect, start, count);
		}
		else
		{
			if (sts_json_cmp_child_node(range, "stop"))
			{
				stop = sts_json_get_int(range, "stop", -1); // -1 为最新一条记录
				out = sts_collect_unit_get_of_range_sds(collect, start, stop);
			}
			else
			{
				out = sts_collect_unit_get_of_count_sds(collect, start, 1);
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
		s_sts_map_define *smd = sts_db_find_map_define(tb_->father, format->value, STS_MAP_DEFINE_DATA_TYPE);
		if (smd)
		{
			iformat = smd->uid;
		}
	}
	printf("iformat = %c\n", iformat);
	// 取出字段定义，没有就默认全部字段
	s_sts_sds sds_fields = NULL;
	s_sts_json_node *fields = sts_json_cmp_child_node(handle->node, "fields");
	if (fields)
	{
		sds_fields = sts_sdsnew(fields->value);
	}
	printf("sds_fields = %s\n", sds_fields);
	s_sts_sds other = NULL;
	switch (iformat)
	{
	case STS_DATA_STRUCT:
		if (!sts_check_fields_all(sds_fields))
		{
			other = sts_collect_struct_filter_sds(collect, out, sds_fields);
			sts_sdsfree(out);
			out = other;
		}
		break;
	case STS_DATA_JSON:
		other = sts_collect_struct_to_json_sds(collect, out, sds_fields);
		sts_sdsfree(out);
		out = other;
		break;
	case STS_DATA_ARRAY:
		other = sts_collect_struct_to_array_sds(collect, out, sds_fields);
		sts_sdsfree(out);
		out = other;
		break;
	}
	if (sds_fields)
	{
		sts_sdsfree(sds_fields);
	}
nodata:
	sts_json_close(handle);
	return out;
}
