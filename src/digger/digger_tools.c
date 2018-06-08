
// #include "sts_fields.h"
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
//////////////////////////////////////////////////////////////////////
// int _sts_command_get_fields_size(int fcount_, s_sts_field_unit *fields_)
// {
// 	int o = 0;
// 	s_sts_field_unit *fp;
// 	for (int i = 0; i < fcount_; i++)
// 	{
// 		fp = fields_ + i;
// 		o += fp->flags.len;
// 	}
// 	return o;
// }
// // 按临时的字段定义生成格式化数据，支持多个股票组合的方式
// s_sts_sds _sts_command_struct_to_json_sds(s_sts_sds in_,int fcount_, s_sts_field_unit *fields_)
// {
// 	s_sts_json_node *jone = sts_json_create_object();
// 	s_sts_json_node *jtwo = sts_json_create_array();
// 	s_sts_json_node *jval = NULL;
// 	s_sts_json_node *jfields = sts_json_create_object();

// 	// 先处理字段
// 	s_sts_field_unit *fu;
// 	for (int i = 0; i < fcount_; i++)
// 	{
// 		fu = fields_ + i;
// 		sts_json_object_add_uint(jfields, fu->name, i);
// 	}
// 	sts_json_object_add_node(jone, STS_JSON_KEY_FIELDS, jfields);

// 	// printf("========%s rows=%d\n", tb->name, tb->control.limit_rows);

// 	int dot = 0; //小数点位数
// 	int skip_len = _sts_command_get_fields_size(fcount_, fields_);
// 	int count = (int)(sts_sdslen(in_) / skip_len);
// 	char *val = in_;
// 	for (int k = 0; k < count; k++)
// 	{
// 		// sts_out_binary("get", val, sts_sdslen(in_));
// 		jval = sts_json_create_array();
// 		for (int i = 0; i < fcount_; i++)
// 		{
// 			fu = fields_ + i;
// 			const char *ptr = (const char *)val;
// 			switch (fu->flags.type)
// 			{
// 			case STS_FIELD_STRING:
// 				sts_json_array_add_string(jval, ptr + fu->offset, fu->flags.len);
// 				break;
// 			case STS_FIELD_INT:
// 				sts_json_array_add_int(jval, sts_fields_get_int(fu, ptr));
// 				break;
// 			case STS_FIELD_UINT:
// 				sts_json_array_add_uint(jval, sts_fields_get_uint(fu, ptr));
// 				break;
// 			case STS_FIELD_DOUBLE:
// 				if (!fu->flags.io && fu->flags.zoom > 0)
// 				{
// 					dot = fu->flags.zoom;
// 				}
// 				sts_json_array_add_double(jval, sts_fields_get_double(fu, ptr), dot);
// 				break;
// 			default:
// 				sts_json_array_add_string(jval, " ", 1);
// 				break;
// 			}
// 		}
// 		sts_json_array_add_node(jtwo, jval);
// 		val += skip_len;
// 	}

// 	sts_json_object_add_node(jone, STS_JSON_KEY_ARRAYS, jtwo);

// 	// size_t ll;
// 	// printf("jone = %s\n", sts_json_output(jone, &ll));
// 	// 输出数据
// 	// printf("1112111 [%d]\n",tb->control.limit_rows);

// 	size_t olen;
// 	char *str = sts_json_output_zip(jone, &olen);
// 	s_sts_sds o = sts_sdsnewlen(str, olen);
// 	sts_free(str);
// 	sts_json_delete_node(jone);

// 	return o;
// }
// s_sts_sds _sts_command_struct_to_array_sds(s_sts_sds in_,int fcount_,s_sts_field_unit *fields_)
// {
// 	s_sts_json_node *jone = sts_json_create_array();
// 	s_sts_json_node *jval = NULL;

// 	s_sts_field_unit *fu;

// 	int dot = 0; //小数点位数
// 	int skip_len = _sts_command_get_fields_size(fcount_, fields_);
// 	int count = (int)(sts_sdslen(in_) / skip_len);
// 	char *val = in_;
// 	for (int k = 0; k < count; k++)
// 	{
// 		// sts_out_binary("get", val, sts_sdslen(in_));
// 		jval = sts_json_create_array();
// 		for (int i = 0; i < fcount_; i++)
// 		{
// 			fu = fields_ + i;
// 			const char *ptr = (const char *)val;
// 			switch (fu->flags.type)
// 			{
// 			case STS_FIELD_STRING:
// 				sts_json_array_add_string(jval, ptr + fu->offset, fu->flags.len);
// 				break;
// 			case STS_FIELD_INT:
// 				sts_json_array_add_int(jval, sts_fields_get_int(fu, ptr));
// 				break;
// 			case STS_FIELD_UINT:
// 				sts_json_array_add_uint(jval, sts_fields_get_uint(fu, ptr));
// 				break;
// 			case STS_FIELD_DOUBLE:
// 				if (!fu->flags.io && fu->flags.zoom > 0)
// 				{
// 					dot = fu->flags.zoom;
// 				}
// 				sts_json_array_add_double(jval, sts_fields_get_double(fu, ptr), dot);
// 				break;
// 			default:
// 				sts_json_array_add_string(jval, " ", 1);
// 				break;
// 			}
// 		}
// 		sts_json_array_add_node(jone, jval);
// 		val += skip_len;
// 	}
// 	// size_t ll;
// 	// printf("jone = %s\n", sts_json_output(jone, &ll));
// 	// 输出数据
// 	// printf("1112111 [%d]\n",tb->control.limit_rows);

// 	size_t olen;
// 	char *str = sts_json_output_zip(jone, &olen);
// 	s_sts_sds o = sts_sdsnewlen(str, olen);
// 	sts_free(str);
// 	sts_json_delete_node(jone);

// 	return o;
// }

// uint64 _sts_get_value_last_uint(s_sts_table *tb_, const char *code_, const char *field_)
// {
	// s_sts_collect_unit *collect = sts_map_buffer_get(tb_->collect_map, code_);
	// if (!collect)
	// {
	// 	return 0;
	// }
	// int recs = sts_collect_unit_recs(collect);
	// if (recs<1)
	// {
	// 	return 0;
	// }	
	// return sts_fields_get_uint_from_key(tb_, field_, sts_struct_list_get(collect->value, recs-1));
// 	return 0;
// }

// static struct s_sts_field_unit _sts_field_def_price[] = {
//     { 0,  0, "code",  {STS_FIELD_STRING, STS_CODE_MAXLEN, 0,0,0,0}, 0, "" },
//     { 0,  STS_CODE_MAXLEN, "close", {STS_FIELD_DOUBLE, 4, 0,2,0,0}, 0, "" },
// };

s_sts_sds sts_command_get_price_sds(s_digger_server *s_,const char *argv_)
{
//     s_sts_table *now_table = sts_db_get_table(db_, "now");
//     if (!now_table)
//     {
//         sts_out_error(3)("no find now db.\n");
//         return NULL;
//     }
//     s_sts_table *info_table = sts_db_get_table(db_, "info");
	
// 	s_sts_json_handle *handle = sts_json_load(argv_, strlen(argv_));
// 	if (!handle)
// 	{
//         sts_out_error(3)("parse %s error.\n", argv_);
// 		return NULL;
// 	}
//     s_sts_sds o = NULL;

//     s_sts_string_list *codes = sts_string_list_create_w();
// 	const char *str = sts_json_get_str(handle->node, "codes");
// 	sts_string_list_load(codes,str,strlen(str),",");

// 	int count = sts_string_list_getsize(codes);
//     if(count < 1) {
//         sts_out_error(3)("no find codes [%s].\n", argv_);
// 		goto error;
//     } 

//     uint32 close;
//     for (int i=0; i < count; i++){
//         const char *code = sts_string_list_get(codes, i);
//         o = sts_sdscatlen(o, code, STS_CODE_MAXLEN);
//         close = _sts_get_value_last_uint(now_table, code, "close");
//         if (close==0) {
//             close = _sts_get_value_last_uint(now_table, code, "buy1");
//         } else {
//             goto next;
//         }
//         if (close==0) {
//             close = _sts_get_value_last_uint(now_table, code, "sell1");
//         } else {
//             goto next;
//         }
//         if (close==0&&info_table) {
//             close = _sts_get_value_last_uint(info_table, code, "before");
//         }
// next:
//         o = sts_sdscatlen(o, &close, 4);
//     }
//     // 得到二进制的数据，再根据需求格式进行格式化
// 	int fields = sizeof(_sts_field_def_price) / sizeof(struct s_sts_field_unit);
//     s_sts_sds val = NULL;
//     int iformat = sts_command_get_format(db_, handle->node);
// 	switch (iformat)
// 	{
// 	// case STS_DATA_STRUCT:
// 	// 	break;
// 	case STS_DATA_JSON:
// 		val = _sts_command_struct_to_json_sds(o, fields, _sts_field_def_price);
// 		sts_sdsfree(o);
// 		o = val;
// 		break;
// 	case STS_DATA_ARRAY:
// 		val = _sts_command_struct_to_array_sds(o, fields, _sts_field_def_price);
// 		sts_sdsfree(o);
// 		o = val;
// 		break;
// 	}
// error:	
// 	sts_json_close(handle);
// 	sts_string_list_destroy(codes);
//    	return o;
	return sdsnew("my is right\n");
}

/////////////////////////////////////////
// 传入格式为 code = "sh600600" , 
// start:20180101,stop:20180303, vol:1000,close:12.34}
// 由于用户传入的数据可能有多个价格，所以返回格式只有json一种格式，用户要什么就传回给什么
// 除vol外，其他支持open，high，low，close四个字段
// 默认vol为股，其他都是带小数点的，需要根据info的价格单位处理后再传入
/////////////////////////////////////////

// static struct s_sts_field_unit _sts_field_def_right[] = {
//     { 0,  0, "code",  {STS_FIELD_STRING, STS_CODE_MAXLEN, 0,0,0,0}, 0, "" },
//     { 0,  STS_CODE_MAXLEN, "close", {STS_FIELD_DOUBLE, 4, 0,2,0,0}, 0, "" },
// 	{ 0,  STS_CODE_MAXLEN, "vol",   {STS_FIELD_UINT,   4, 0,0,0,0}, 0, "" },
// };

s_sts_sds sts_command_get_right_sds(s_digger_server *s_,const char *argv_)
{

// 	s_sts_table *right_table = sts_db_get_table(db_, "right");
//     if (!right_table)
//     {
//         sts_out_error(3)("no find right db.\n");
// 		return sdsnewlen(argv_,strlen(argv_));
//     }
// 	s_sts_table *info_table = sts_db_get_table(db_, "info");

// 	s_sts_json_handle *handle = sts_json_load(argv_, strlen(argv_));
// 	if (!handle)
// 	{
//         sts_out_error(3)("parse %s error.\n", argv_);
// 		return sdsnewlen(argv_,strlen(argv_));
// 	}

// 	const char *code = sts_json_get_str(handle->node, "code");
// 	int stop = sts_json_get_int(handle->node, "stop", sts_time_get_idate());
// 	int start = sts_json_get_int(handle->node, "start", sts_time_get_idate())
// 	// 得到需要使用的除权信息
// 	s_sts_sds right = sts_table_get_search_sds(right_table, code, start, stop);
// 	if (!right)
// 	{
// 		// 原样返回
//         o = sdsnewlen(argv_,strlen(argv_));
// 		goto error;
// 	}
// 	uint32 ui = sts_json_get_uint(handle->node, "vol", 0);
// 	if(ui>0) {
// 		ui = sts_stock_exright_vol(right, ui);
// 		sts_json_object_set_uint(handle->node, "vol", ui);
// 	}
// 	// 得到股票的价格放大倍数
// 	int dot = 2;
// 	int zoom = sts_zoom10(dot);
// 	double open = sts_json_get_double(handle->node, "open", 0.0001);
// 	if(open>0.0001) {
// 		ui = (uint32)(open*zoom);
// 		ui = sts_stock_exright_price(right, ui);
// 		sts_json_object_set_double(handle->node, "open", ui/zoom, dot);
// 	}
// 	double high= sts_json_get_double(handle->node, "high", 0.0001);
// 	if(high > 0.0001) {
// 		ui = (uint32)(high*zoom);
// 		ui = sts_stock_exright_price(right, ui);
// 		sts_json_object_set_double(handle->node, "high", ui/zoom, dot);
// 	}
// 	double low= sts_json_get_double(handle->node, "low", 0.0001);
// 	if(low > 0.0001) {
// 		ui = (uint32)(low*zoom);
// 		ui = sts_stock_exright_price(right, ui);
// 		sts_json_object_set_double(handle->node, "low", ui/zoom, dot);
// 	}
// 	double close= sts_json_get_double(handle->node, "close", 0.0001);
// 	if(close > 0.0001) {
// 		ui = (uint32)(close*zoom);
// 		ui = sts_stock_exright_price(right, ui);
// 		sts_json_object_set_double(handle->node, "close", ui/zoom, dot);
// 	}		
//     s_sts_sds o = NULL;
// 	size_t olen;
// 	str = sts_json_output_zip(handle->node, &olen);
// 	s_sts_sds o = sts_sdsnewlen(str, olen);
// 	sts_free(str);
// error:	
// 	sts_json_close(handle);
//  return o;
return sdsnew("my is right\n");
}

s_sts_sds sts_command_find_code_sds(s_digger_server *s_,const char *argv_)
{
	s_sts_sds o = get_stsdb_info(s_, "sh600600.now", NULL);
    return o;
}
