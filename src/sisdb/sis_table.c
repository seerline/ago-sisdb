

#include "sis_table.h"
#include "sis_collect.h"
#include "sis_db.h"

//com_为一个json格式字段定义
s_sis_table *sis_table_create(s_sis_db *db_, const char *name_, s_sis_json_node *com_)
{
	s_sis_table *tb = sis_db_get_table(db_, name_);
	if (tb)
	{
		sis_table_destroy(tb);
	}
	// 先加载默认配置
	tb = sis_malloc(sizeof(s_sis_table));
	memset(tb, 0, sizeof(s_sis_table));

	tb->control.data_type = SIS_DATA_STRUCT; // 默认保存的目前都是struct，
	tb->control.time_scale = SIS_SCALE_SECOND;
	tb->control.limit_rows = sis_json_get_int(com_, "limit", 0);
	tb->control.isinit = sis_json_get_int(com_, "isinit", 0);
	// printf("=====%s limit %d\n", name_, tb->control.limit_rows);
	tb->control.insert_mode = SIS_OPTION_ALWAYS;
	tb->control.version = (uint32)sis_time_get_now();

	sis_dict_add(db_->db, sis_sdsnew(name_), tb);
	tb->father = db_;

	// 加载实际配置
	s_sis_map_define *map = NULL;
	const char *strval = NULL;

	strval = sis_json_get_str(com_, "scale");
	map = sis_db_find_map_define(db_, strval, SIS_MAP_DEFINE_SCALE);
	if (map)
	{
		tb->control.time_scale = map->uid;
	}

	strval = sis_json_get_str(com_, "insert-mode");
	int nums = sis_str_substr_nums(strval, ',');
	for (int i=0; i < nums; i++) 
	{
		char mode[32];
		sis_str_substr(mode, 32, strval, ',', i);
		map = sis_db_find_map_define(db_, mode, SIS_MAP_DEFINE_OPTION_MODE);
		if (map)
		{
			tb->control.insert_mode |= map->uid;
		}
	}
	// strval = sis_json_get_str(com_, "update-mode");
	// map = sis_db_find_map_define(db_, strval, SIS_MAP_DEFINE_OPTION_MODE);
	// if (map)
	// {
	// 	tb->control.update_mode = map->uid;
	// }

	tb->name = sis_sdsnew(name_);
	tb->collect_map = sis_map_pointer_create();
	//处理链接数据表名
	tb->links = sis_string_list_create_w();

	if (sis_json_cmp_child_node(com_, "links"))
	{
		strval = sis_json_get_str(com_, "links");
		sis_string_list_load(tb->links, strval, strlen(strval), ",");
	}

	//处理字段定义
	tb->field_name = sis_string_list_create_w();
	tb->field_map = sis_map_pointer_create();

	// 顺序不能变，必须最后
	sis_table_set_fields(tb, sis_json_cmp_child_node(com_, "fields"));
	// int count = sis_string_list_getsize(tb->field_name);
	// for (int i = 0; i < count; i++)
	// {
	// 	printf("---111  %s\n",sis_string_list_get(tb->field_name, i));
	// }

	s_sis_json_node *cache = sis_json_cmp_child_node(com_, "fields-cache");
	if (cache)
	{
		tb->catch = true;
		s_sis_json_node *child = cache->child;
		while (child)
		{
			s_sis_field_unit *fu = sis_field_get_from_key(tb, child->key);
			if (fu)
			{
				fu->catch_method = SIS_FIELD_METHOD_COVER;
				map = sis_db_find_map_define(db_, sis_json_get_str(child, "0"), SIS_MAP_DEFINE_FIELD_METHOD);
				if (map)
				{
					fu->catch_method = map->uid;
				}
				sis_strcpy(fu->catch_initfield, SIS_FIELD_MAXLEN, sis_json_get_str(child, "1"));
				// printf("%s==%d  %s--- %s\n", child->key, fu->catch_method, fu->catch_initfield,fu->name);
			}
			// printf("[%s] %s=====%p\n", tb->name, child->key, fu);
			child = child->next;
		}
	}

	s_sis_json_node *zip = sis_json_cmp_child_node(com_, "zip-method");
	if (zip)
	{
		tb->zip = true;
	}
	return tb;
}

void sis_table_destroy(s_sis_table *tb_)
//删除一个表
{
	// //删除字段定义
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(tb_->field_map);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sis_field_unit *val = (s_sis_field_unit *)sis_dict_getval(de);
		sis_field_unit_destroy(val);
	}
	sis_dict_iter_free(di);
	sis_map_pointer_destroy(tb_->field_map);

	sis_string_list_destroy(tb_->links);

	sis_string_list_destroy(tb_->field_name);
	//删除数据区
	sis_table_collect_clear(tb_);
	sis_map_pointer_destroy(tb_->collect_map);
	
	sis_sdsfree(tb_->name);
	sis_free(tb_);
}
void sis_table_collect_clear(s_sis_table *tb_)
//清理一个表的所有数据
{
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(tb_->collect_map);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sis_collect_unit *val = (s_sis_collect_unit *)sis_dict_getval(de);
		sis_collect_unit_destroy(val);
	}
	sis_dict_iter_free(di);
	// sis_map_buffer_clear(tb_->collect_map);
	// sis_map_pointer_destroy(tb_->collect_map);
}
//取数据和写数据
s_sis_table *sis_db_get_table(s_sis_db *db_, const char *name_)
{
	s_sis_table *val = NULL;
	if (db_->db)
	{
		s_sis_sds key = sis_sdsnew(name_);
		val = (s_sis_table *)sis_dict_fetch_value(db_->db, key);
		sis_sdsfree(key);
	}
	return val;
}

/////////////////////////////////////
//对数据库的各种属性设置
////////////////////////////////////
void sis_table_set_ver(s_sis_table *tb_, uint32 ver_)
{
	tb_->control.version = ver_;
}
void sis_table_set_limit_rows(s_sis_table *tb_, uint32 limits_)
{
	tb_->control.limit_rows = limits_;
}
void sis_table_set_insert_mode(s_sis_table *tb_, uint16 insert_)
{
	tb_->control.insert_mode = insert_;
}

void sis_table_set_fields(s_sis_table *tb_, s_sis_json_node *fields_)
{
	if (!fields_)
	{
		return;
	}

	sis_map_pointer_clear(tb_->field_map);
	sis_string_list_clear(tb_->field_name);

	s_sis_json_node *node = sis_json_first_node(fields_);

	s_sis_fields_flags flags;
	s_sis_map_define *map = NULL;
	int index = 0;
	int offset = 0;
	while (node)
	{
		// size_t ss;
		// printf("node=%s\n", sis_json_output(node, &ss));
		const char *name = sis_json_get_str(node, "0");
		flags.type = SIS_FIELD_INT;
		flags.len = sis_json_get_int(node, "2", 4);
		flags.io = sis_json_get_int(node, "3", 0);
		flags.zoom = sis_json_get_int(node, "4", 0);
		flags.ziper = 0; // 暂时不压缩
		flags.refer = 0;

		const char *val = sis_json_get_str(node, "1");
		map = sis_db_find_map_define(tb_->father, val, SIS_MAP_DEFINE_FIELD_TYPE);
		if (map)
		{
			flags.type = map->uid;
		}

		s_sis_field_unit *unit =
			sis_field_unit_create(index++, name, &flags);
		unit->offset = offset;
		offset += unit->flags.len;
		// printf("[%d:%d] name=%s len=%d\n", index, offset, name, flags.len);
		sis_map_pointer_set(tb_->field_map, name, unit);
		sis_string_list_push(tb_->field_name, name, strlen(name));

		node = sis_json_next_node(node);
	}
}

int sis_table_get_fields_size(s_sis_table *tb_)
{
	int len = 0;
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(tb_->field_map);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sis_field_unit *val = (s_sis_field_unit *)sis_dict_getval(de);
		len += val->flags.len;
	}
	sis_dict_iter_free(di);
	return len;
}

// uint64 sis_table_get_times(s_sis_table *tb_, void *val_)
// {
// 	uint64 out = 0;
// 	int count = sis_string_list_getsize(tb_->field_name);
// 	for (int i = 0; i < count; i++)
// 	{
// 		s_sis_field_unit *fu = (s_sis_field_unit *)sis_map_buffer_get(tb_->field_map, sis_string_list_get(tb_->field_name, i));
// 		if (!fu)
// 		{
// 			continue;
// 		}
// 		if (sis_field_is_time(fu))
// 		{
// 			out = sis_fields_get_uint(fu, (const char *)val_);
// 			break;
// 		}
// 	}
// 	return out;
// }

//time_t 返回０－２３９
// 不允许返回-1
uint16 _sis_ttime_to_trade_index(uint64 ttime_, s_sis_struct_list *tradetime_)
{
	int tt = sis_time_get_iminute(ttime_);

	int nowmin = 0;
	int out = 0;

	s_sis_time_pair *pair;
	for (int i = 0; i < tradetime_->count; i++)
	{
		pair = (s_sis_time_pair *)sis_struct_list_get(tradetime_, i);
		if (tt > pair->first && tt < pair->second) //9:31:00--11:29:59  13:01:00--14:59:59
		{
			out = nowmin + sis_time_get_iminute_offset_i(pair->first, tt);
			break;
		}
		if (tt <= pair->first && i == 0)
		{ // 8:00:00---9:30:59秒前都=0
			return 0;
		}
		if (tt <= pair->first && (tt > sis_time_get_iminute_minnum(pair->first, -5)))
		{ //12:55:59--13:00:59秒
			return nowmin;
		}

		nowmin += sis_time_get_iminute_offset_i(pair->first, pair->second);

		if (tt >= pair->second && i == tradetime_->count - 1)
		{ //15:00:00秒后
			return nowmin - 1;
		}
		// if (tt >= pair->second && (tt < sis_time_get_iminute_minnum(pair->second, 5)))
		// { //11:30:00--11:34:59秒
		// 	return nowmin - 1;
		// }
		if (tt >= pair->second)
		{ //11:30:00--11:34:59秒
			if (tt < sis_time_get_iminute_minnum(pair->second, 5))
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
uint64 _sis_trade_index_to_ttime(int date_, int nIndex_, s_sis_struct_list *tradetime_)
{
	int tt = nIndex_;
	int nowmin = 0;
	s_sis_time_pair *pair = (s_sis_time_pair *)sis_struct_list_last(tradetime_);
	uint64 out = sis_time_make_time(date_, pair->second * 100);
	for (int i = 0; i < tradetime_->count; i++)
	{
		pair = (s_sis_time_pair *)sis_struct_list_get(tradetime_, i);
		nowmin = sis_time_get_iminute_offset_i(pair->first, pair->second);
		if (tt < nowmin)
		{
			int min = sis_time_get_iminute_minnum(pair->first, tt + 1);
			out = sis_time_make_time(date_, min * 100);
			break;
		}
		tt -= nowmin;
	}
	return out;
}
uint64 sis_table_struct_trans_time(uint64 in_, int inscale_, s_sis_table *out_tb_, int outscale_)
{
	if (inscale_ > outscale_)
	{
		return in_;
	}
	if (inscale_ >= SIS_SCALE_DAY)
	{
		return in_;
	}
	uint64 o = in_;
	int date = sis_time_get_idate(o);
	if (inscale_ == SIS_SCALE_MSEC)
	{
		o = (uint64)(o / 1000);
	}
	switch (outscale_)
	{
	case SIS_SCALE_MSEC:
		return in_;
	case SIS_SCALE_SECOND:
		return o;
	case SIS_SCALE_INDEX:
		o = _sis_ttime_to_trade_index(o, out_tb_->father->trade_time);
		break;
	case SIS_SCALE_MIN1:
		o = _sis_ttime_to_trade_index(o, out_tb_->father->trade_time);
		o = _sis_trade_index_to_ttime(date, o, out_tb_->father->trade_time);
		break;
	case SIS_SCALE_MIN5:
		o = _sis_ttime_to_trade_index(o, out_tb_->father->trade_time);
		o = _sis_trade_index_to_ttime(date, ((int)(o / 5) + 1) * 5 - 1, out_tb_->father->trade_time);
		break;
	case SIS_SCALE_MIN30:
		o = _sis_ttime_to_trade_index(o, out_tb_->father->trade_time);
		o = _sis_trade_index_to_ttime(date, ((int)(o / 30) + 1) * 30 - 1, out_tb_->father->trade_time);
		break;
	case SIS_SCALE_DAY:
		o = date;
		break;
	}
	return o;
}
s_sis_sds _sis_table_struct_trans(s_sis_collect_unit *in_unit_, s_sis_sds ins_, s_sis_collect_unit *out_unit_)
{
	// const char *src = sis_struct_list_get(out_unit_->value, out_unit_->value->count - 1);

	s_sis_table *in_tb = in_unit_->father;
	s_sis_table *out_tb = out_unit_->father;

	int count = (int)(sis_sdslen(ins_) / in_unit_->value->len);
	s_sis_sds outs_ = sis_sdsnewlen(NULL, count * out_unit_->value->len);

	s_sis_sds ins = ins_;
	s_sis_sds outs = outs_;

	// printf("table=%p:%p coll=%p,field=%p \n",
	// 			out_tb, out_unit_->father, out_unit_,
	// 			out_tb->field_name);
	int fields = sis_string_list_getsize(out_tb->field_name);

	for (int k = 0; k < count; k++)
	{
		for (int m = 0; m < fields; m++)
		{
			const char *key = sis_string_list_get(out_tb->field_name, m);
			s_sis_field_unit *out_fu = (s_sis_field_unit *)sis_map_buffer_get(out_tb->field_map, key);
			// printf("key=%s size=%d offset=%d\n", key, fields, fu->offset);
			s_sis_field_unit *in_fu = (s_sis_field_unit *)sis_map_buffer_get(in_tb->field_map, key);
			if (!in_fu)
			{
				continue;
			}
			sis_collect_struct_trans(ins, in_fu, in_tb, outs, out_fu, out_tb);
		}
		ins += in_unit_->value->len;
		outs += out_unit_->value->len;
		// 下一块数据
	}
	return outs_;
}

// s_sis_sds _sis_table_struct_trans_incr(s_sis_collect_unit *in_unit_, s_sis_sds ins_, s_sis_sds dbs_, s_sis_collect_unit *out_unit_, s_sis_string_list *fields_)
// {
// 	// 需要求差值然后根据情况不同累计到目标数据表中
// 	s_sis_table *in_tb = in_unit_->father;
// 	s_sis_table *out_tb = out_unit_->father;

// 	int count = (int)(sis_sdslen(ins_) / in_unit_->value->len);
// 	s_sis_sds outs_ = sis_sdsnewlen(NULL, count * out_unit_->value->len);

// 	s_sis_sds ins = ins_;
// 	s_sis_sds dbs = dbs_;
// 	s_sis_sds outs = outs_;

// 	int fields = sis_string_list_getsize(out_tb->field_name);

// 	for (int k = 0; k < count; k++)
// 	{
// 		for (int m = 0; m < fields; m++)
// 		{
// 			const char *key = sis_string_list_get(out_tb->field_name, m);
// 			s_sis_field_unit *out_fu = (s_sis_field_unit *)sis_map_buffer_get(out_tb->field_map, key);
// 			// printf("key=%s size=%d offset=%d\n", key, fields, fu->offset);
// 			s_sis_field_unit *in_fu = (s_sis_field_unit *)sis_map_buffer_get(in_tb->field_map, key);
// 			if (!in_fu)
// 			{
// 				continue;
// 			}
// 			if (sis_string_list_indexof(fields_,key)) // 需要做增量计算
// 			{
// 				sis_collect_struct_trans_incr(ins, dbs, in_fu, in_tb, outs, out_fu, out_tb);
// 			} else {
// 				sis_collect_struct_trans(ins, in_fu, in_tb, outs, out_fu, out_tb);
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
int _sis_table_update_links(s_sis_table *table_,const char *key_, s_sis_collect_unit *collect_,s_sis_sds val_)
{
	int count = sis_string_list_getsize(table_->links);
	// printf("links=%d\n", count);

	s_sis_sds link_val = NULL;
	s_sis_table *link_table;
	s_sis_collect_unit *link_collect;
	for (int k = 0; k < count; k++)
	{
		const char *link_db = sis_string_list_get(table_->links, k);
		link_table = sis_db_get_table(table_->father, link_db);
		// printf("links=%d  db=%s  fields=%p %p \n", k, link_db,link_table,link_table->field_name);
		// for (int f=0; f<sis_string_list_getsize(link_table->field_name); f++){
		// 	printf("  fields=%s\n", sis_string_list_get(link_table->field_name,f));
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
		link_collect = sis_map_buffer_get(link_table->collect_map, key_);
		if (!link_collect)
		{
			// printf("new......\n");
			link_collect = sis_collect_unit_create(link_table, key_);
			sis_map_buffer_set(link_table->collect_map, key_, link_collect);
		}
		// printf("links=%d  db=%s.%s in= %p table=%p:%p coll=%p,field=%p \n", k, link_db,key_,
		// 		collect_->father,
		// 		link_table, link_collect->father, link_collect,
		// 		link_table->field_name);
		//---- 转换到其他数据库的数据格式 ----//
		link_val = _sis_table_struct_trans(collect_, val_, link_collect);
		//---- 修改数据 ----//
		if (link_val)
		{
			// printf("table_=%lld\n",sis_fields_get_uint_from_key(table_,"time",in_val));
			// printf("link_table=%lld\n",sis_fields_get_uint_from_key(link_table,"time",link_val));
			sis_collect_unit_update(link_collect, link_val, sis_sdslen(link_val));
			sis_sdsfree(link_val);
		}
	}
	return count;
}
int sis_table_update_mul(int type_, s_sis_table *table_, const char *key_, const char *in_, size_t ilen_)
{
	// 1 . 先把来源数据，转换为srcdb的二进制结构数据集合
	s_sis_collect_unit *in_collect = sis_map_buffer_get(table_->collect_map, key_);
	if (!in_collect)
	{
		in_collect = sis_collect_unit_create(table_, key_);
		sis_map_buffer_set(table_->collect_map, key_, in_collect);
		//if (!sis_strcasecmp(table_->name,"min")&&!sis_strcasecmp(key_,"SH600000"))
		// printf("[%s.%s]new in_collect = %p = %p | %p\n",key_, table_->name,in_collect->father,table_,in_collect);
	}

	s_sis_sds in_val = NULL;

	switch (type_)
	{
	case SIS_DATA_JSON:
		// 取json中字段，如果目标没有记录，新建或者取最后一条记录的信息为模版
		// 用新的数据进行覆盖，然后返回数据
		in_val = sis_collect_json_to_struct_sds(in_collect, in_, ilen_);
		break;
	case SIS_DATA_ARRAY:
		in_val = sis_collect_array_to_struct_sds(in_collect, in_, ilen_);
		break;
	default:
		// 这里应该不用申请新的内存
		in_val = sis_sdsnewlen(in_, ilen_);
	}
	// sis_out_binary("update 0 ", in_, ilen_);

	// ------- val 就是hi已经转换好的完整的结构化数据 -----//
	// 2. 先修改自己
	if (!in_val)
	{
		return 0;
	}

	// 先储存上一次的数据，
	int o = sis_collect_unit_update(in_collect, in_val, sis_sdslen(in_val));

	// 3. 顺序改其他数据表，
	// **** 修改其他数据表时应该和单独修改min不同，需要修正vol和money两个字段*****
	if(!table_->loading) {
		_sis_table_update_links(table_, key_, in_collect, in_val);
	}

	// 5. 释放内存
	// 不要重复释放
	// if (type_!=SIS_DATA_JSON&&type_!=SIS_DATA_ARRAY)
	if (in_val)
	{
		sis_sdsfree(in_val);
	}

	return o;
}

int sis_table_update_load(int type_, s_sis_table *table_, const char *key_, const char *in_, size_t ilen_)
{
	s_sis_collect_unit *in_collect = sis_map_buffer_get(table_->collect_map, key_);
	if (!in_collect)
	{
		in_collect = sis_collect_unit_create(table_, key_);
		sis_map_buffer_set(table_->collect_map, key_, in_collect);
	}

	// 先储存上一次的数据，
	int o = sis_collect_unit_update_block(in_collect, in_, ilen_);

	return o;
}
//////////////////////////////////////////////////////////////////////////////////
//修改数据，key_为股票代码或市场编号，in_为二进制结构化数据或json数据
//////////////////////////////////////////////////////////////////////////////////

// int sis_table_update(int type_, s_sis_table *tb_, const char *key_, const char *in_, size_t ilen_)
// {
// 	s_sis_collect_unit *collect = sis_map_buffer_get(tb_->collect_map, key_);
// 	// printf("---collect %p---%p--%d\n", collect, tb_->field_map, sis_table_get_fields_size(tb_));
// 	if (!collect)
// 	{
// 		collect = sis_collect_unit_create(tb_, key_);
// 		sis_map_buffer_set(tb_->collect_map, key_, collect);
// 	}
// 	// printf("---collect %p---\n", collect);
// 	int o = 0;
// 	s_sis_sds val = NULL;
// 	switch (type_)
// 	{
// 	case SIS_DATA_JSON:
// 		// 取json中字段，如果目标没有记录，新建或者取最后一条记录的信息为模版
// 		// 用新的数据进行覆盖，然后返回数据
// 		val = sis_collect_json_to_struct_sds(collect, in_, ilen_);
// 		if (val)
// 		{
// 			o = sis_collect_unit_update(collect, val, sis_sdslen(val));
// 			sis_sdsfree(val);
// 		}
// 		break;
// 	case SIS_DATA_ARRAY:
// 		val = sis_collect_array_to_struct_sds(collect, in_, ilen_);
// 		if (val)
// 		{
// 			o = sis_collect_unit_update(collect, val, sis_sdslen(val));
// 			sis_sdsfree(val);
// 		}
// 		break;
// 	default:
// 		o = sis_collect_unit_update(collect, in_, ilen_);
// 	}
// 	return o;
// }
//////////////////////////
//删除数据
//////////////////////////
int sis_table_delete(s_sis_table *tb_, const char *key_, const char *com_)
{
	s_sis_collect_unit *collect = sis_map_buffer_get(tb_->collect_map, key_);
	if (!collect)
	{
		return 0;
	}

	s_sis_json_handle *handle = sis_json_load(com_, strlen(com_));
	if (!handle)
	{
		return 0;
	}

	uint64 min, max;
	int start, stop;
	int count = 0;
	int rtn = 0;
	int minX, maxX;

	s_sis_json_node *search = sis_json_cmp_child_node(handle->node, "search");
	if (search)
	{
		if (!sis_json_cmp_child_node(search, "min"))
		{
			goto exit;
		}
		min = sis_json_get_int(search, "min", 0);
		if (sis_json_cmp_child_node(search, "count"))
		{
			count = sis_json_get_int(search, "count", 1);
			start = sis_collect_unit_search(collect, min);
			if (start >= 0)
			{
				rtn = sis_collect_unit_delete_of_count(collect, start, count);
			}
		}
		else
		{
			if (sis_json_cmp_child_node(search, "max"))
			{
				max = sis_json_get_int(search, "max", 0);
				start = sis_collect_unit_search_right(collect, min, &minX);
				stop = sis_collect_unit_search_left(collect, max, &maxX);
				if (minX != SIS_SEARCH_NONE && maxX != SIS_SEARCH_NONE)
				{
					rtn = sis_collect_unit_delete_of_range(collect, start, stop);
				}
			}
			else
			{
				start = sis_collect_unit_search(collect, min);
				if (start >= 0)
				{
					rtn = sis_collect_unit_delete_of_count(collect, start, 1);
				}
			}
		}
		goto exit;
	}
	// 按界限操作
	s_sis_json_node *range = sis_json_cmp_child_node(handle->node, "range");
	if (!range)
	{
		goto exit;
	}

	if (!sis_json_cmp_child_node(range, "start"))
	{
		goto exit;
	}
	start = sis_json_get_int(range, "start", -1); // -1 为最新一条记录
	if (sis_json_cmp_child_node(range, "count"))
	{
		count = sis_json_get_int(range, "count", 1);
		rtn = sis_collect_unit_delete_of_count(collect, start, count); // 定位后按数量删除
	}
	else
	{
		if (sis_json_cmp_child_node(range, "stop"))
		{
			stop = sis_json_get_int(range, "stop", -1);					  // -1 为最新一条记录
			rtn = sis_collect_unit_delete_of_range(collect, start, stop); // 定位后删除
		}
		else
		{
			rtn = sis_collect_unit_delete_of_count(collect, start, 1); // 定位后按数量删除
		}
	}
exit:
	sis_json_close(handle);
	return rtn;
}
int sis_from_node_get_format(s_sis_db *db_, s_sis_json_node *node_)
{
	int o = SIS_DATA_JSON;
	s_sis_json_node *format = sis_json_cmp_child_node(node_, "format");
	if (format)
	{
		s_sis_map_define *smd = sis_db_find_map_define(db_, format->value, SIS_MAP_DEFINE_DATA_TYPE);
		if (smd)
		{
			o = smd->uid;
		}
	}
	return o;
}

s_sis_sds sis_table_get_search_sds(s_sis_table *tb_, const char *code_, int min_,int max_)
{
	s_sis_collect_unit *collect = sis_map_buffer_get(tb_->collect_map, code_);
	if (!collect)
	{
		return NULL;
	}

	s_sis_sds o = NULL;

	int start, stop;
	int maxX, minX;

	start = sis_collect_unit_search_right(collect, min_, &minX);
	stop = sis_collect_unit_search_left(collect, max_, &maxX);
	if (minX != SIS_SEARCH_NONE && maxX != SIS_SEARCH_NONE)
	{
		o = sis_collect_unit_get_of_range_sds(collect, start, stop);
	}	
	return o;
}
///////////////////////////////////////////////////////////////////////////////
//取数据,读表中代码为key的数据，key为*表示所有股票数据，由 com_ 定义数据范围和字段范围
///////////////////////////////////////////////////////////////////////////////
s_sis_sds sis_table_get_sds(s_sis_table *tb_, const char *key_, const char *com_)
{
	if (strlen(key_)<1) {
		printf("get all data....\n");
		return sis_table_get_table_sds(tb_,com_);
	} 
	return sis_table_get_code_sds(tb_,key_,com_);
}

s_sis_sds sis_table_struct_filter_sds(s_sis_table *tb_, s_sis_sds in_, const char *fields_)
{
	// 一定不是全字段
	s_sis_sds o = NULL;

	s_sis_table *tb = tb_;
	s_sis_string_list *field_list = sis_string_list_create_w();
	sis_string_list_load(field_list, fields_, strlen(fields_), ",");

	int len = sis_table_get_fields_size(tb) + SIS_CODE_MAXLEN;
	int count = (int)(sis_sdslen(in_) / len);
	char *val = in_;
	for (int k = 0; k < count; k++)
	{
		if (!o)
		{
			o = sis_sdsnewlen(val, SIS_CODE_MAXLEN);
		}
		else
		{
			o = sis_sdscatlen(o, val, SIS_CODE_MAXLEN);
		}
		for (int i = 0; i < sis_string_list_getsize(field_list); i++)
		{
			const char *key = sis_string_list_get(field_list, i);
			s_sis_field_unit *fu = sis_field_get_from_key(tb, key);
			if (!fu)
			{
				continue;
			}
			o = sis_sdscatlen(o, val + SIS_CODE_MAXLEN + fu->offset, fu->flags.len);
		}
		val += len;
	}
	sis_string_list_destroy(field_list);
	return o;
}
s_sis_sds sis_table_struct_to_json_sds(s_sis_table *tb_, s_sis_sds in_, const char *fields_)
{
	s_sis_sds o = NULL;

	s_sis_table *tb = tb_;
	s_sis_string_list *field_list = tb->field_name; //取得全部的字段定义
	
	if (!sis_check_fields_all(fields_))
	{
		field_list = sis_string_list_create_w();
		sis_string_list_load(field_list, fields_, strlen(fields_), ",");
	}

	char *str;
	s_sis_json_node *jone = sis_json_create_object();
	s_sis_json_node *jtwo = sis_json_create_object();
	s_sis_json_node *jval = NULL;
	s_sis_json_node *jfields = sis_json_create_object();

	// 先处理字段
	for (int i = 0; i < sis_string_list_getsize(field_list); i++)
	{
		const char *key = sis_string_list_get(field_list, i);
		sis_json_object_add_uint(jfields, key, i);
	}
	sis_json_object_add_node(jone, SIS_JSON_KEY_FIELDS, jfields);

	// printf("========%s rows=%d\n", tb->name, tb->control.limit_rows);

	int dot = 0; //小数点位数
	int skip_len = sis_table_get_fields_size(tb) + SIS_CODE_MAXLEN;
	int count = (int)(sis_sdslen(in_) / skip_len);
	char *val = in_;
	char code[SIS_CODE_MAXLEN];
	for (int k = 0; k < count; k++)
	{
		sis_strncpy(code, SIS_CODE_MAXLEN, val, SIS_CODE_MAXLEN);
		// sis_out_binary("get", val, sis_sdslen(in_));
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
			const char *ptr = (const char *)val + SIS_CODE_MAXLEN;
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
		sis_json_object_add_node(jtwo, code, jval);
		val += skip_len;
	}
	sis_json_object_add_node(jone, SIS_JSON_KEY_GROUPS, jtwo);

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

s_sis_sds sis_table_get_table_sds(s_sis_table *tb_, const char *com_)
{
	s_sis_json_handle *handle = sis_json_load(com_, strlen(com_));
	// printf("com_ = %s -- %lu -- %p\n", com_,strlen(com_),handle);
	if (!handle)
	{
		return NULL;
	}
	s_sis_sds out = NULL;
	// 只取最后一条记录
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(tb_->collect_map);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sis_collect_unit *collect = (s_sis_collect_unit *)sis_dict_getval(de);
		s_sis_sds val = sis_collect_unit_get_of_count_sds(collect, -1, 1);
		if (!out)
		{
			out = sis_sdsnewlen(sis_dict_getkey(de), SIS_CODE_MAXLEN);
		} else {
			out = sis_sdscatlen(out, sis_dict_getkey(de), SIS_CODE_MAXLEN);
		}
		// printf("out = %lu -- %lu\n", sis_sdslen(out),sis_sdslen(val));
		out = sis_sdscatlen(out, val, sis_sdslen(val));
		sis_sdsfree(val);
	}
	sis_dict_iter_free(di);

	if (!out)
	{
		goto nodata;
	}
	int iformat = sis_from_node_get_format(tb_->father, handle->node);

	printf("iformat = %c\n", iformat);
	// 取出字段定义，没有就默认全部字段
	s_sis_sds sds_fields = NULL;
	s_sis_json_node *fields = sis_json_cmp_child_node(handle->node, "fields");
	if (fields)
	{
		sds_fields = sis_sdsnew(fields->value);
	}
	// printf("sds_fields = %s\n", sds_fields);
	s_sis_sds other = NULL;
	switch (iformat)
	{
	case SIS_DATA_STRUCT:
		if (!sis_check_fields_all(sds_fields))
		{
			other = sis_table_struct_filter_sds(tb_, out, sds_fields);
			sis_sdsfree(out);
			out = other;
		}
		break;
	case SIS_DATA_JSON:
		other = sis_table_struct_to_json_sds(tb_, out, sds_fields);
		sis_sdsfree(out);
		out = other;
		break;
	// case SIS_DATA_ARRAY:
	// 	other = sis_table_struct_to_array_sds(tb_, out, sds_fields);
	// 	sis_sdsfree(out);
	// 	out = other;
	// 	break;
	default :
		sis_sdsfree(out);
		out = NULL;
	}
	if (sds_fields)
	{
		sis_sdsfree(sds_fields);
	}
nodata:
	sis_json_close(handle);
	return out;
}

s_sis_sds sis_table_get_collects_sds(s_sis_table *tb_, const char *com_)
{
	s_sis_json_handle *handle = sis_json_load(com_, strlen(com_));
	printf("com_ = %s -- %lu -- %p\n", com_,strlen(com_),handle);
	if (!handle)
	{
		return NULL;
	}
	s_sis_sds out = NULL;
	// 只取最后一条记录
	int count = 0;
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(tb_->collect_map);
	while ((de = sis_dict_next(di)) != NULL)
	{
		if (!out)
		{
			out = sis_sdsnewlen(sis_dict_getkey(de), SIS_CODE_MAXLEN);
		} else {
			out = sis_sdscatlen(out, sis_dict_getkey(de), SIS_CODE_MAXLEN);
		}
		count++;
	}
	sis_dict_iter_free(di);

	if (!out)
	{
		goto end;
	}
	int iformat = sis_from_node_get_format(tb_->father, handle->node);

	printf("iformat = %c\n", iformat);
	// 取出字段定义，没有就默认全部字段
	if (iformat == SIS_DATA_STRUCT) {
		goto end;
	}
	char *str;
	s_sis_json_node *jone = NULL;
	s_sis_json_node *jval = sis_json_create_array();

	for(int i=0;i<count;i++){
		sis_json_array_add_string(jval, out + i*SIS_CODE_MAXLEN, SIS_CODE_MAXLEN);
	}
	if (iformat == SIS_DATA_ARRAY) {
		jone = jval;
	} else {
		jone = sis_json_create_object();
		sis_json_object_add_node(jone, SIS_JSON_KEY_COLLECTS, jval);
	}

	size_t olen;
	str = sis_json_output_zip(jone, &olen);
	sis_sdsfree(out);
	out = sis_sdsnewlen(str, olen);
	sis_free(str);
	sis_json_delete_node(jone);

end:
	sis_json_close(handle);
	return out;

}
s_sis_sds sis_table_get_code_sds(s_sis_table *tb_, const char *key_, const char *com_)
{
	s_sis_json_handle *handle = sis_json_load(com_, strlen(com_));
	if (!handle)
	{
		return NULL;
	}

	s_sis_collect_unit *collect = sis_map_buffer_get(tb_->collect_map, key_);
	if (!collect)
	{
		return NULL;
	}

	s_sis_sds out = NULL;

	// 检查取值范围，没有就全部取
	int64 min, max;
	int start, stop;
	int count = 0;
	int maxX, minX;

	s_sis_json_node *search = sis_json_cmp_child_node(handle->node, "search");
	s_sis_json_node *range = sis_json_cmp_child_node(handle->node, "range");
	if (!search && !range) // 这两个互斥，以search为首发
	{
		out = sis_collect_unit_get_of_range_sds(collect, 0, -1);
		goto filter;
	}
	if (search)
	{
		if (!sis_json_cmp_child_node(search, "min"))
		{
			goto filter;
		}
		min = sis_json_get_int(search, "min", 0);
		if (sis_json_cmp_child_node(search, "count"))
		{
			count = sis_json_get_int(search, "count", 1);
			start = sis_collect_unit_search_right(collect, min, &minX);
			if (start >= 0)
			{
				out = sis_collect_unit_get_of_count_sds(collect, start, count);
			}
		}
		else
		{
			if (sis_json_cmp_child_node(search, "max"))
			{
				max = sis_json_get_int(search, "max", 0);
				start = sis_collect_unit_search_right(collect, min, &minX);
				stop = sis_collect_unit_search_left(collect, max, &maxX);
				if (minX != SIS_SEARCH_NONE && maxX != SIS_SEARCH_NONE)
				{
					out = sis_collect_unit_get_of_range_sds(collect, start, stop);
				}
			}
			else
			{
				start = sis_collect_unit_search(collect, min);
				if (start >= 0)
				{
					out = sis_collect_unit_get_of_count_sds(collect, start, 1);
				}
			}
		}
		goto filter;
	}
	if (range)
	{
		if (!sis_json_cmp_child_node(range, "start"))
		{
			goto filter;
		}
		start = sis_json_get_int(range, "start", -1); // -1 为最新一条记录
		if (sis_json_cmp_child_node(range, "count"))
		{
			count = sis_json_get_int(range, "count", 1);
			out = sis_collect_unit_get_of_count_sds(collect, start, count);
		}
		else
		{
			if (sis_json_cmp_child_node(range, "stop"))
			{
				stop = sis_json_get_int(range, "stop", -1); // -1 为最新一条记录
				out = sis_collect_unit_get_of_range_sds(collect, start, stop);
			}
			else
			{
				out = sis_collect_unit_get_of_count_sds(collect, start, 1);
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
	// int iformat = SIS_DATA_STRUCT;
	int iformat = sis_from_node_get_format(tb_->father, handle->node);

	printf("iformat = %c\n", iformat);
	// 取出字段定义，没有就默认全部字段
	s_sis_sds sds_fields = NULL;
	s_sis_json_node *fields = sis_json_cmp_child_node(handle->node, "fields");
	if (fields)
	{
		sds_fields = sis_sdsnew(fields->value);
	}
	printf("sds_fields = %s\n", sds_fields);
	s_sis_sds other = NULL;
	switch (iformat)
	{
	case SIS_DATA_STRUCT:
		if (!sis_check_fields_all(sds_fields))
		{
			other = sis_collect_struct_filter_sds(collect, out, sds_fields);
			sis_sdsfree(out);
			out = other;
		}
		break;
	case SIS_DATA_JSON:
		other = sis_collect_struct_to_json_sds(collect, out, sds_fields);
		sis_sdsfree(out);
		out = other;
		break;
	case SIS_DATA_ARRAY:
		other = sis_collect_struct_to_array_sds(collect, out, sds_fields);
		sis_sdsfree(out);
		out = other;
		break;
	}
	if (sds_fields)
	{
		sis_sdsfree(sds_fields);
	}
nodata:
	sis_json_close(handle);
	return out;
}

