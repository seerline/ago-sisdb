
#include "sts_time.h"
#include "sts_json.h"
#include "sts_str.h"
#include "digger_tools.h"

static struct s_sts_command _sts_command_table[] = {
	{"findcode",  sts_command_find_code_sds},
    {"getprice",  sts_command_get_price_sds},
    {"getright",  sts_command_get_right_sds}
};

s_sts_map_pointer *sts_command_create()
{
    s_sts_map_pointer *map = sts_map_pointer_create();
	int nums = sizeof(_sts_command_table) / sizeof(struct s_sts_command);

	for (int i = 0; i < nums; i++)
	{
		struct s_sts_command *c = _sts_command_table + i;
		int o = sts_dict_add(map, sts_sdsnew(c->name), c);
		assert(o == DICT_OK);
	}    
    return map;
}
void sts_command_destroy(s_sts_map_pointer *map_)
{
    if (map_){
        sts_map_pointer_destroy(map_);
    }
}

s_sts_command *sts_command_get(s_sts_map_pointer *map_, const char *name_)
{
	s_sts_command *val = NULL;
	if (map_)
	{
		s_sts_sds func = sts_sdsnew(name_);
		val = (s_sts_command *)sts_dict_fetch_value(map_, func);
		sts_sdsfree(func);
	}
	return val;
}

//////////////////////////////////////////////////////////////////////
//   下面是函数实现
//   返回数据分两种格式，一种是单个股票的，请求什么字段返回什么字段
//   一种是多个股票的，返回groups的多股票数组，每个股票最多只有一个数组元素，或者没有数组
//   fields：[close:0,code:1],groups:{{sh600600:[12,222]},{...}}
//   groups:{{sh600600:[12,222]},{...}}
//   collects:[sh600600,sh600601]
//////////////////////////////////////////////////////////////////////

uint32 _sts_from_now_get_price(s_digger_server *s_, const char *code_)
{
	s_stock_dynamic *dynamic_p;
	s_sts_sds info = get_stsdb_info_sds(s_, code_, "now", STS_QUERY_COM_LAST);
	if(!info) {
		return 0;
	}
	dynamic_p = (s_stock_dynamic *)info;
	// sts_out_binary("dynamic_p",info, 30);
	// printf("[%lu:%lu] close=%d,=%d,=%d\n",sizeof(s_stock_dynamic),sts_sdslen(info),dynamic_p->close,dynamic_p->buy1,dynamic_p->sell1);

	uint32 close = dynamic_p->close;
    if (close==0) {
        close = dynamic_p->buy1;
    } else {
        goto success;
    }
    if (close==0) {
        close = dynamic_p->sell1;
    } else {
        goto success;
    }
	// if (close==0) {
    //     close = dynamic_p->open;
    // } else {
    //     goto success;
    // }
success:
    sts_sdsfree(info);
	return close;
}

////////////////////////////////////////////////////////////////
// 传入格式为 {"codes":"sh600600,sh600048,sh600601"}
// 返回格式为：{"sh600600":{"close":34.59},
//            "sh600048":{"close":10.32},
//			  "sh600601":{"close":3.72}}
////////////////////////////////////////////////////////////////
s_sts_sds sts_command_get_price_sds(s_digger_server *s_,const char *com_)
{
	s_sts_json_handle *handle = sts_json_load(com_, strlen(com_));
	if (!handle)
	{
        sts_out_error(3)("parse %s error.\n", com_);
		return NULL;
	}
    s_sts_sds o = NULL;

    s_sts_string_list *codes = sts_string_list_create_w();
	const char *co = sts_json_get_str(handle->node, "codes");
	if (!co) {
		sts_out_error(3)("no find codes [%s].\n", com_);
		goto error;
	}
	sts_string_list_load(codes,co,strlen(co),",");

	int count = sts_string_list_getsize(codes);
    if(count < 1) {
        sts_out_error(3)("no find codes [%s].\n", com_);
		goto error;
    } 

	s_sts_json_node *jone = sts_json_create_object();
	s_sts_json_node *jtwo = sts_json_create_object();
	s_sts_json_node *jval = NULL;
	s_sts_json_node *jfields = sts_json_create_object();
	// 先处理字段
	sts_json_object_add_uint(jfields, "close", 0);
	sts_json_object_add_node(jone, STS_JSON_KEY_FIELDS, jfields);

	int dot = 2;
    uint32 close;
	s_stock_info *info_ps;

    for (int i=0; i < count; i++){
        const char *code = sts_string_list_get(codes, i);

		s_sts_sds info = get_stsdb_info_sds(s_, code, "info", STS_QUERY_COM_NORMAL);
		if (!info) {
			continue;
		}
		info_ps = (s_stock_info *)info;
		// sts_out_binary("info_ps",info, 30);
		// printf("[%lu:%lu] dot=%d,before=%d\n",sizeof(s_stock_info),sts_sdslen(info),info_ps->coinunit,info_ps->coinunit);
		dot = info_ps->coinunit;

		close = _sts_from_now_get_price(s_, code);
		if (close==0) {
			close = info_ps->coinunit;
		}
		jval = sts_json_create_array();
		sts_json_array_add_double(jval, (double)close/sts_zoom10(dot), dot);
		sts_json_object_add_node(jtwo, code, jval);
		sts_sdsfree(info);
    }
	sts_json_object_add_node(jone, STS_JSON_KEY_GROUPS, jtwo);
	
	size_t olen;
	char *str = sts_json_output_zip(jone, &olen);
	o = sts_sdsnewlen(str, olen);
	sts_free(str);
	sts_json_delete_node(jone);

error:	
	sts_json_close(handle);
	sts_string_list_destroy(codes);

   	return o;
}
/////////////////////////////////////////
// 传入格式为 code = "sh600600" , 
// fixed-date:20180101,curr-date:20180301, vol:1000, close:12.34}
// 由于用户传入的数据可能有多个价格，所以返回格式只有json一种格式，用户要什么就传回给什么
// 除vol外，其他支持open，high，low，close四个字段
// 默认vol为股，其他都是带小数点的，需要根据info的价格单位处理后再传入
/////////////////////////////////////////

s_sts_sds sts_command_get_right_sds(s_digger_server *s_,const char *com_)
{
	s_sts_json_handle *handle = sts_json_load(com_, strlen(com_));
	if (!handle)
	{
        sts_out_error(3)("parse %s error.\n", com_);
		return NULL;
	}
    
	s_sts_sds o = NULL;

	const char *code = sts_json_get_str(handle->node, "code");
    if(!code) {
        sts_out_error(3)("no find code [%s].\n", com_);
		goto error;
    } 

	int start = sts_json_get_int(handle->node, "curr-date", sts_time_get_idate(0));
	int stop = sts_json_get_int(handle->node, "fixed-date", sts_time_get_idate(0));
	char query[64];
	if(start > stop) {
		sts_snprintf(query,64, STS_QUERY_COM_SEARCH, stop, start);
	} else {
		sts_snprintf(query,64, STS_QUERY_COM_SEARCH, start, stop);
	}
	s_sts_sds right = get_stsdb_info_sds(s_, code, "right", query);
	if (!right) {
		o = sdsnewlen(com_,strlen(com_));
		return o;
	}
	s_sts_struct_list *right_list = sts_struct_list_create(sizeof(s_stock_right), right, sdslen(right));
	sts_sdsfree(right);	

	int dot = 2;
	s_stock_info *info_ps;
	s_sts_sds info = get_stsdb_info_sds(s_, code, "info", STS_QUERY_COM_NORMAL);
	if (!info) {
		goto error;
	}
	info_ps = (s_stock_info *)info;
	dot = info_ps->coinunit;
	sts_sdsfree(info);

	uint32 ui = sts_json_get_int(handle->node, "vol", 0);
	if(ui>0) {
		ui = sts_stock_exright_vol(start, stop, ui, right_list);
		sts_json_object_set_uint(handle->node, "vol", ui);
	}
	// 得到股票的价格放大倍数
	int zoom = sts_zoom10(dot);
	double open = sts_json_get_double(handle->node, "open", 0.0001);
	if(open>0.0001) {
		ui = (uint32)(open*zoom);
		ui = sts_stock_exright_price(start, stop, ui, zoom, right_list);
		sts_json_object_set_double(handle->node, "open", (double)ui/zoom, dot);
	}
	double high= sts_json_get_double(handle->node, "high", 0.0001);
	if(high > 0.0001) {
		ui = (uint32)(high*zoom);
		ui = sts_stock_exright_price(start, stop, ui, zoom, right_list);
		sts_json_object_set_double(handle->node, "high", (double)ui/zoom, dot);
	}
	double low= sts_json_get_double(handle->node, "low", 0.0001);
	if(low > 0.0001) {
		ui = (uint32)(low*zoom);
		ui = sts_stock_exright_price(start, stop, ui, zoom, right_list);
		sts_json_object_set_double(handle->node, "low", (double)ui/zoom, dot);
	}
	double close= sts_json_get_double(handle->node, "close", 0.0001);
	if(close > 0.0001) {
		ui = (uint32)(close*zoom);
		ui = sts_stock_exright_price(start, stop, ui, zoom, right_list);
		// printf("5 close: %d\n", ui);
		sts_json_object_set_double(handle->node, "close", (double)ui/zoom, dot);
	}	
	sts_struct_list_destroy(right_list);

	size_t olen;
	char *str = sts_json_output_zip(handle->node, &olen);
	o = sts_sdsnewlen(str, olen);
	sts_free(str);

error:	
	sts_json_close(handle);

   	return o;
}

s_sts_sds sts_command_find_code_sds(s_digger_server *s_,const char *com_)
{
	s_sts_json_handle *handle = sts_json_load(com_, strlen(com_));
	if (!handle)
	{
        sts_out_error(3)("parse %s error.\n", com_);
		return NULL;
	}
    
	s_sts_sds o = NULL;

	const char *match = sts_json_get_str(handle->node, "match");
    if(!match) {
        sts_out_error(3)("no find match [%s].\n", com_);
		goto error;
    } 
	if(strlen(match) < 2) {
        sts_out_error(3)("match too short [%s].\n", com_);
		goto error;
    } 

	s_sts_json_node *jfields = sts_json_create_object();
	// 先处理字段
	sts_json_object_add_uint(jfields, "code", 0);
	sts_json_object_add_uint(jfields, "name", 1);
	sts_json_object_add_node(handle->node, STS_JSON_KEY_FIELDS, jfields);

	s_sts_sds info = get_stsdb_info_sds(s_, NULL, "info", STS_QUERY_COM_NORMAL);
	if(!info) {
        sts_out_error(3)("no info data.\n");
		goto error;
	}
	//开始检索数据

	int count = sts_sdslen(info)/sizeof(s_stock_info_db);
	s_stock_info_db *info_ps = (s_stock_info_db *)info;
	int set;
	s_sts_struct_list *list = sts_pointer_list_create();
	for (int i=0; i < count; i++){
		sts_str_to_upper((char *)match);
		set = sts_str_match(match, info_ps->info.search);
		if (set == 0)
		{
			sts_pointer_list_insert(list, 0, info_ps);
		} else if (set == 1)
		{
			sts_pointer_list_push(list, info_ps);
		}
		info_ps ++;
	}
	s_sts_json_node *jone = sts_json_create_array();
	s_sts_json_node *jval = NULL;

	count = sts_json_get_int(handle->node, "count", 20);
	count = sts_between(count, 5, 200);
	for (int i=0;i<list->count&&i < count;i++) {
		info_ps = (s_stock_info_db *)sts_struct_list_get(list, i);
		jval = sts_json_create_array();
		sts_json_array_add_string(jval, info_ps->code, strlen(info_ps->code));
		sts_json_array_add_string(jval, info_ps->info.name, strlen(info_ps->info.name));
		sts_json_array_add_node(jone, jval);
	}
	sts_json_object_add_node(handle->node, STS_JSON_KEY_ARRAYS, jone);
	sts_pointer_list_destroy(list);
	
	sts_json_delete_node(sts_json_cmp_child_node(handle->node,"match"));
	sts_json_delete_node(sts_json_cmp_child_node(handle->node,"count"));

	size_t olen;
	char *str = sts_json_output_zip(handle->node, &olen);
	o = sts_sdsnewlen(str, olen); 
	sts_free(str);

error:	
	sts_json_close(handle);

   	return o;

}
