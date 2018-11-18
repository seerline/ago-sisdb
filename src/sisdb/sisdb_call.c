
#include "sisdb_call.h"
#include "sisdb_fields.h"
#include "sisdb_map.h"

static struct s_sisdb_call _sisdb_call_table[] = {
	{"list", sisdb_call_list_command, "list all command"},
	{"init", sisdb_call_market_init, "init market info. : SH"},
	{"findcode", sisdb_call_match_code_sds, "match search of info. : {\"match\":\"YH\",\"count\":5}"},
	// format - json
	{"collects", sisdb_call_get_keys_sds, "get collects. : {\"table\":\"now\",\"format\":\"array\"}"},
	// format - json array
	{"getprice", sisdb_call_get_price_sds, "get mul stock price. : {\"codes\":\"SH600600,SZ000001\"}"},
	{"getright", sisdb_call_get_right_sds, "get exright val. : {\"code\":\"SH600603\",\"curr-date\":20180515,\"fixed-date\":20181010,\"close\":7.20}"}};

void sisdb_init_call_define(s_sis_map_pointer *map_)
{
	sis_map_pointer_clear(map_);
	int nums = sizeof(_sisdb_call_table) / sizeof(struct s_sisdb_call);

	for (int i = 0; i < nums; i++)
	{
		s_sis_sds key = sis_sdsnew(_sisdb_call_table[i].name);
		int rtn = sis_dict_add(map_, key, &_sisdb_call_table[i]);
		assert(rtn == DICT_OK);
	}
}

s_sisdb_call *sisdb_call_find_define(s_sis_map_pointer *map_, const char *name_)
{
	s_sisdb_call *val = NULL;
	if (map_)
	{
		s_sis_sds key = sis_sdsnew(name_);
		val = (s_sisdb_call *)sis_dict_fetch_value(map_, key);
		sis_sdsfree(key);
	}
	return val;
}

//////////////////////////////////////////////////////////////////////
//   下面是一些通用函数实现
//   返回数据分两种格式,一种是单个股票的,请求什么字段返回什么字段
//   一种是多个股票的,返回groups的多股票数组,每个股票最多只有一个数组元素,或者没有数组
//   fields：[close:0,code:1],groups:{{sh600600:[12,222]},{...}}
//   groups:{{sh600600:[12,222]},{...}}
//   collects:[sh600600,sh600601]
//////////////////////////////////////////////////////////////////////
s_sis_sds sisdb_call_list_command(s_sis_db *db_, const char *com_)
{
	int nums = sizeof(_sisdb_call_table) / sizeof(struct s_sisdb_call);
	s_sis_sds list = sis_sdsempty();
	for (int i = 0; i < nums; i++)
	{
		list = sdscatprintf(list, " %-10s : %s \n",
							_sisdb_call_table[i].name,
							_sisdb_call_table[i].explain);
	}
	return list;
}

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
	// 该函数需要锁定
	// 然后写aof。暂时不做
	return sis_sdsnewlong(o);
}

s_sis_sds sisdb_call_match_code_sds(s_sis_db *db_, const char *com_)
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
s_sis_sds sisdb_call_get_keys_sds(s_sis_db *db_, const char *com_) 
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


double _sis_from_now_get_price(s_sis_db *db_, const char *code_)
{
	char key[SIS_MAXLEN_KEY];
	sis_sprintf(key, SIS_MAXLEN_KEY, "%s.now", code_);
	s_sisdb_collect *collect = sisdb_get_collect(db_, key);
printf("===1==%s====\n",code_);

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
printf("===112==%s==%d==\n",code_, close);
	if (close == 0)
	{
		close = sisdb_field_get_uint_from_key(collect->db, "askp1", buffer);
	}
	else
	{
		goto success;
	}
printf("===12==%s==%d==\n",code_, close);

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
printf("===2==%d====\n",close);
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
/////////////////////////////////////////
// 传入格式为 code = "sh600600" ,
// fixed-date:20180101,curr-date:20180301, vol:1000, close:12.34}
// 由于用户传入的数据可能有多个价格,所以返回格式只有json一种格式,用户要什么就传回给什么
// 除vol外,其他支持open,high,low,close四个字段
// 默认vol为股,其他都是带小数点的,需要根据info的价格单位处理后再传入
/////////////////////////////////////////
int _sisdb_get_code_dot(s_sis_db *db_, const char *code_) 
{
	int o = 2;
	char key[SIS_MAXLEN_KEY];
	sis_sprintf(key, SIS_MAXLEN_KEY, "%s.%s", code_, SIS_TABLE_INFO);

	s_sis_sds info = sisdb_collect_get_sds(db_, key, SIS_QUERY_COM_INFO);
	if (!info)
	{
		return o;
	}
	s_sisdb_sys_info *info_ps = (s_sisdb_sys_info *)info;
	o = info_ps->dot;
	sis_sdsfree(info);
	return o;
}	
s_sis_sds sisdb_call_get_right_sds(s_sis_db *db_, const char *com_)
{
	s_sis_json_handle *handle = sis_json_load(com_, strlen(com_));
	if (!handle)
	{
		sis_out_log(3)("parse %s error.\n", com_);
		return NULL;
	}

	s_sis_sds o = NULL;

	const char *code = sis_json_get_str(handle->node, "code");
	if (!code)
	{
		sis_out_log(3)("no find code [%s].\n", com_);
		goto error;
	}

	int start = sis_json_get_int(handle->node, "curr-date", sis_time_get_idate(0));
	int stop = sis_json_get_int(handle->node, "fixed-date", sis_time_get_idate(0));

	char sql[64];
	if (start > stop)
	{
		sis_sprintf(sql, 64, SIS_QUERY_COM_SEARCH, stop, start);
	}
	else
	{
		sis_sprintf(sql, 64, SIS_QUERY_COM_SEARCH, start, stop);
	}
	char key[SIS_MAXLEN_KEY];
	sis_sprintf(key, SIS_MAXLEN_KEY, "%s.%s", code, SIS_TABLE_RIGHT);
	s_sis_sds right = sisdb_collect_get_sds(db_, key, sql);
	if (!right)
	{
		goto error;
	}

	s_sis_struct_list *right_list = sis_struct_list_create(sizeof(s_sisdb_right), right, sdslen(right));
	sis_sdsfree(right);

	int dot = _sisdb_get_code_dot(db_, code);

	uint32 ui = sis_json_get_int(handle->node, "vol", 0);
	if (ui > 0)
	{
		ui = sis_stock_exright_vol(start, stop, ui, right_list);
		sis_json_object_set_uint(handle->node, "vol", ui);
	}
	ui = sis_json_get_int(handle->node, "money", 0);
	if (ui > 0)
	{
		ui = sis_stock_exright_vol(start, stop, ui, right_list);
		sis_json_object_set_uint(handle->node, "money", ui);
	}
	// 得到股票的价格放大倍数
	int zoom = sis_zoom10(dot);
	double open = sis_json_get_double(handle->node, "open", 0.0001);
	if (open > 0.0001)
	{
		ui = (uint32)(open * zoom);
		ui = sis_stock_exright_price(start, stop, ui, zoom, right_list);
		sis_json_object_set_double(handle->node, "open", (double)ui / zoom, dot);
	}
	double high = sis_json_get_double(handle->node, "high", 0.0001);
	if (high > 0.0001)
	{
		ui = (uint32)(high * zoom);
		ui = sis_stock_exright_price(start, stop, ui, zoom, right_list);
		sis_json_object_set_double(handle->node, "high", (double)ui / zoom, dot);
	}
	double low = sis_json_get_double(handle->node, "low", 0.0001);
	if (low > 0.0001)
	{
		ui = (uint32)(low * zoom);
		ui = sis_stock_exright_price(start, stop, ui, zoom, right_list);
		sis_json_object_set_double(handle->node, "low", (double)ui / zoom, dot);
	}
	double close = sis_json_get_double(handle->node, "close", 0.0001);
	if (close > 0.0001)
	{
		ui = (uint32)(close * zoom);
		ui = sis_stock_exright_price(start, stop, ui, zoom, right_list);
		// printf("5 close: %d\n", ui);
		sis_json_object_set_double(handle->node, "close", (double)ui / zoom, dot);
	}
	sis_struct_list_destroy(right_list);

	size_t olen;
	char *str = sis_json_output_zip(handle->node, &olen);
	o = sis_sdsnewlen(str, olen);
	sis_free(str);
	sis_json_close(handle);
	return o;
error:
	o = sdsnewlen(com_, strlen(com_));
	sis_json_close(handle);

	return o;
}
