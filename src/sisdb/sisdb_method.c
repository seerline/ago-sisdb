
#include "sisdb_call.h"
#include "sisdb_fields.h"
#include "sisdb_map.h"
#include "sisdb_io.h"

static struct s_sis_method _sis_method_table[] = {
	// 默认行为，对有scale的time字段表，必须按time进行排序，没有scale的表不排序
	{"only",    "append", method_append_only},     // 字段列表中单独唯一
	// 例如 "time,code" 表示time和code不能同时一样，单独一样可以
	{"incr",    "update", method_append_incr},     // 数值必须增长且不为零
	{"nonzero", "append", method_append_nonzero},  // 仅仅非零

	// {"incr",    "update", method_update_incr},     // 数值必须增长且不为零

	{"once",    "subscribe", method_subscribe_once},  // 只对 0 值赋值一次，后面不再赋值
	{"min",     "subscribe", method_subscribe_min},   // 求最小值
	{"max",     "subscribe", method_subscribe_max},   // 求最大值
	{"gap",     "subscribe", method_subscribe_gap},  // 和上一笔的差值
};

s_sis_map_pointer *sisdb_method_define_create(s_sis_map_pointer *map_)
{
	int nums = sizeof(_sis_method_table) / sizeof(struct s_sis_method);
	s_sis_map_pointer *map = sis_method_map_create(map_, &_sis_method_table, nums);
	return map;
}
void sisdb_method_define_destroy(s_sis_map_pointer *map_)
{
	sis_method_map_destroy(map_);
}

//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////

s_sis_sds sisdb_call_market_init(s_sis_db *db_, const char *market_)
{
	if (!market_) return NULL;
	int o = 0;
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(db_->collects);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sisdb_collect *val = (s_sisdb_collect *)sis_dict_getval(de);
		if (val->db->control.isinit && !val->db->control.issys &&
			!sis_strncasecmp(sis_dict_getkey(de), market_, strlen(market_)))
		{
			// 只是设置记录数为0
			sisdb_collect_clear(val);
			o++;
		}
	}
	sis_dict_iter_free(di);
	
	// 该函数执行完毕需要写盘，这样就不需要处理aof文件了
	sisdb_save();
	
	return sis_sdsnewlong(o);
}

s_sis_sds sisdb_call_get_code_sds(s_sis_db *db_, const char *com_)
{
	s_sis_json_handle *handle = sis_json_load(com_, strlen(com_));
	if (!handle)
	{
		sis_out_log(3)("parse %s error.\n", com_);
		return NULL;
	}

	s_sis_sds o = NULL;

	const char *match = sis_json_get_str(handle->node, "match");
	if (!match)
	{
		sis_out_log(3)("no find match [%s].\n", com_);
		goto error;
	}
	if (strlen(match) < 2)
	{
		sis_out_log(3)("match too short [%s].\n", com_);
		goto error;
	}

	s_sisdb_table *tb = sisdb_get_table(db_, SIS_TABLE_INFO);
	if (!tb)
	{
		sis_out_log(3)("no info table.\n");
		goto error;
	}	
	s_sis_sds buffer = sisdb_collects_get_last_sds(db_, SIS_TABLE_INFO, SIS_QUERY_COM_NORMAL);
	if (!buffer)
	{
		sis_out_log(3)("no info data.\n");
		goto error;
	}

	s_sis_json_node *jfields = sis_json_create_object();
	// 先处理字段
	sis_json_object_add_uint(jfields, "code", 0);
	sis_json_object_add_uint(jfields, "name", 1);
	sis_json_object_add_node(handle->node, SIS_JSON_KEY_FIELDS, jfields);

	//开始检索数据
	int size = sisdb_table_get_fields_size(tb);
	int count = sis_sdslen(buffer) / (size + SIS_MAXLEN_CODE);
	int set;
	
	s_sis_struct_list *list = sis_pointer_list_create();
	
	sis_str_to_upper((char *)match);
	char *ptr = buffer;
	size_t len;
	for (int i = 0; i < count; i++)
	{
		const char *search = sisdb_field_get_char_from_key(tb, "search", ptr + SIS_MAXLEN_CODE, &len);
		set = sis_str_match(match, search, ',');
		if (set == 0)
		{
			sis_pointer_list_insert(list, 0, ptr);
		}
		else if (set == 1)
		{
			sis_pointer_list_push(list, ptr);
		}
		ptr+= (size + SIS_MAXLEN_CODE);
	}
	sis_sdsfree(buffer);
	
	s_sis_json_node *jone = sis_json_create_array();
	s_sis_json_node *jval = NULL;

	count = sis_json_get_int(handle->node, "count", 20);
	count = sis_between(count, 5, 200);
	char code[SIS_MAXLEN_CODE];
	for (int i = 0; i < list->count && i < count; i++)
	{
		ptr = (char *)sis_struct_list_get(list, i);
		jval = sis_json_create_array();
		sis_strncpy(code, SIS_MAXLEN_CODE, ptr, SIS_MAXLEN_CODE);
		sis_json_array_add_string(jval, code, strlen(code));

		const char *name=sisdb_field_get_char_from_key(tb, "name", ptr + SIS_MAXLEN_CODE, &len);
		sis_json_array_add_string(jval, name, len);

		sis_json_array_add_node(jone, jval);
	}
	sis_json_object_add_node(handle->node, SIS_JSON_KEY_ARRAYS, jone);
	sis_pointer_list_destroy(list);

	sis_json_delete_node(sis_json_cmp_child_node(handle->node, "match"));
	sis_json_delete_node(sis_json_cmp_child_node(handle->node, "count"));

	size_t olen;
	char *str = sis_json_output_zip(handle->node, &olen);
	o = sis_sdsnewlen(str, olen);
	sis_free(str);

error:
	sis_json_close(handle);

	return o;
}

// 获得某个数据表所有的key键 com_ 即表名
s_sis_sds sisdb_call_get_collects_sds(s_sis_db *db_, const char *com_) 
{
	s_sis_json_handle *handle = sis_json_load(com_, strlen(com_));
	// printf("com_ = %s -- %lu -- %p\n", com_,strlen(com_),handle);
	if (!handle)
	{
		return NULL;
	}
	const char *dbname = sis_json_get_str(handle->node, "table");
	if (!dbname) 
	{
		dbname = SIS_TABLE_INFO;
	}

	int count = 0;
	char code[SIS_MAXLEN_CODE];

	s_sis_sds out = NULL;

	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(db_->collects);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sisdb_collect *collect = (s_sisdb_collect *)sis_dict_getval(de);
		if (!sis_strcasecmp(collect->db->name, dbname))
		{
			sis_str_substr(code, SIS_MAXLEN_CODE, sis_dict_getkey(de), '.', 0);
			if (!out)
			{
				out = sis_sdsnewlen(code, SIS_MAXLEN_CODE);
			}
			else
			{
				out = sis_sdscatlen(out, code, SIS_MAXLEN_CODE);
			}
			count++;
		}
	}
	sis_dict_iter_free(di);

	if (!out)
	{
		goto error;
	}
	int iformat = sis_from_node_get_format(db_, handle->node);

	printf("iformat = %c\n", iformat);
	// 取出字段定义,没有就默认全部字段
	if (iformat == SIS_DATA_TYPE_STRUCT)
	{
		goto error;
	}
	char *str;
	s_sis_json_node *jone = NULL;
	s_sis_json_node *jval = sis_json_create_array();

	for (int i = 0; i < count; i++)
	{
		sis_json_array_add_string(jval, out + i * SIS_MAXLEN_CODE, SIS_MAXLEN_CODE);
	}
	if (iformat == SIS_DATA_TYPE_ARRAY)
	{
		jone = jval;
	}
	else // SIS_DATA_TYPE_JSON
	{
		jone = sis_json_create_object();
		sis_json_object_add_node(jone, SIS_JSON_KEY_COLLECTS, jval);
	}

	size_t olen;
	str = sis_json_output_zip(jone, &olen);
	sis_sdsfree(out);
	out = sis_sdsnewlen(str, olen);
	sis_free(str);
	sis_json_delete_node(jone);

error:
	sis_json_close(handle);
	return out;
}


uint32 _sis_from_now_get_price(s_sis_db *db_, const char *code_)
{
	char key[SIS_MAXLEN_KEY];
	sis_sprintf(key, SIS_MAXLEN_KEY, "%s.now", code_);
	s_sisdb_collect *collect = sisdb_get_collect(db_, key);
	// printf("===1==%s====\n",code_);

	if (!collect) 
	{
		return  0;
	}
	s_sis_sds buffer = sisdb_collect_get_of_count_sds(collect, -1, 1);
	if (!buffer)
	{
		return 0;
	}
	sis_out_binary("out",buffer, sisdb_table_get_fields_size(collect->db));
	uint32 close = sisdb_field_get_uint_from_key(collect->db, "close", buffer);
	// printf("===112==%s==%d==\n",code_, close);
	if (close == 0)
	{
		close = sisdb_field_get_uint_from_key(collect->db, "askp1", buffer);
	}
	else
	{
		goto success;
	}
	// printf("===12==%s==%d==\n",code_, close);

	if (close == 0)
	{
		close = sisdb_field_get_uint_from_key(collect->db, "bidp1", buffer); 
	}
	else
	{
		goto success;
	}
	// if (close==0) {
	//     close = dynamic_p->open;
	// } else {
	//     goto success;
	// }
success:
	sis_sdsfree(buffer);
	return close;
}

////////////////////////////////////////////////////////////////
// 传入格式为 {"codes":"sh600600,sh600048,sh600601"}
// 返回格式为：{"sh600600":{"close":34.59},
//            "sh600048":{"close":10.32},
//			  "sh600601":{"close":3.72}}
////////////////////////////////////////////////////////////////
s_sis_sds sisdb_call_get_price_sds(s_sis_db *db_, const char *com_)
{
	s_sis_json_handle *handle = sis_json_load(com_, strlen(com_));
	if (!handle)
	{
		sis_out_log(3)("parse %s error.\n", com_);
		return NULL;
	}
	s_sis_sds o = NULL;

	s_sis_string_list *codes = sis_string_list_create_w();
	const char *co = sis_json_get_str(handle->node, "codes");
	if (!co)
	{
		sis_out_log(3)("no find codes [%s].\n", com_);
		goto error;
	}
	sis_string_list_load(codes, co, strlen(co), ",");

	int count = sis_string_list_getsize(codes);
	if (count < 1)
	{
		sis_out_log(3)("no find codes [%s].\n", com_);
		goto error;
	}
	const char *fmt = sis_json_get_str(handle->node, "format");
	if(fmt)
	{
		if (!sis_strcasecmp(fmt, "struct"))
		{
			o = sis_sdsnewlen(NULL, count*sizeof(uint32));
			uint32 *close = (uint32 *)o;
			char key[SIS_MAXLEN_KEY];
			for (int i = 0; i < count; i++)
			{
				close = (uint32 *)o + i;
				*close = 0;
				const char *code = sis_string_list_get(codes, i);

				sis_sprintf(key, SIS_MAXLEN_KEY, "%s.%s", code, SIS_TABLE_INFO);
				s_sisdb_collect *collect = sisdb_get_collect(db_, key);
				if (!collect) 
				{
					continue;
				}
				s_sis_sds info = sisdb_collect_get_of_count_sds(collect, -1, 1);
				if (!info)
				{
					continue;
				}
				*close = _sis_from_now_get_price(db_, code);
				// printf("===2==%d====\n",close);
				if (*close == 0)
				{
					*close = (uint32)sisdb_field_get_uint_from_key(collect->db, "before", info);
				}
				sis_sdsfree(info);
			}	
			goto error;
		}
	}
	///  json 格式
	s_sis_json_node *jone = sis_json_create_object();
	s_sis_json_node *jtwo = sis_json_create_object();
	s_sis_json_node *jval = NULL;
	s_sis_json_node *jfields = sis_json_create_object();
	// 先处理字段
	sis_json_object_add_uint(jfields, "close", 0);
	sis_json_object_add_node(jone, SIS_JSON_KEY_FIELDS, jfields);

	int dot = 2;
	uint32 close;
	char key[SIS_MAXLEN_KEY];
	for (int i = 0; i < count; i++)
	{
		const char *code = sis_string_list_get(codes, i);

		sis_sprintf(key, SIS_MAXLEN_KEY, "%s.%s", code, SIS_TABLE_INFO);
		s_sisdb_collect *collect = sisdb_get_collect(db_, key);
		if (!collect) 
		{
			continue;
		}
		s_sis_sds info = sisdb_collect_get_of_count_sds(collect, -1, 1);
		if (!info)
		{
			continue;
		}
		dot = sisdb_field_get_uint_from_key(collect->db, "dot", info);

		close = _sis_from_now_get_price(db_, code);
		// printf("===2==%d====\n",close);
		if (close == 0)
		{
			close = sisdb_field_get_uint_from_key(collect->db, "before", info);
		}
		jval = sis_json_create_array();
		sis_json_array_add_double(jval, (double)close / sis_zoom10(dot), dot);
		sis_json_object_add_node(jtwo, code, jval);
		sis_sdsfree(info);
	}
	sis_json_object_add_node(jone, SIS_JSON_KEY_GROUPS, jtwo);

	size_t olen;
	char *str = sis_json_output_zip(jone, &olen);
	o = sis_sdsnewlen(str, olen);
	sis_free(str);
	sis_json_delete_node(jone);

error:
	sis_json_close(handle);
	sis_string_list_destroy(codes);

	return o;
}
