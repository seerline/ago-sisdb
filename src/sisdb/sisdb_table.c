
#include "sisdb_table.h"
#include "sisdb_map.h"
#include "sisdb_fields.h"
#include "sisdb_io.h"

static struct s_sisdb_method _sis_method_table[] = {
	{"incr",     "append", sisdb_append_method_incr},
	{"nonzero",  "append", sisdb_append_method_nonezero},
	{"only",     "append", sisdb_append_method_only},

	{"once",    "subscribe", sisdb_subscribe_method_once},
	{"max",  	"subscribe", sisdb_subscribe_method_max},
	{"min", 	"subscribe", sisdb_subscribe_method_min},
	{"incr", 	"subscribe", sisdb_subscribe_method_incr},
};

void sisdb_init_method_define(s_sis_map_pointer *map_)
{
	sis_map_pointer_clear(map_);
	int nums = sizeof(_sis_method_table) / sizeof(struct s_sisdb_method);

	for (int i = 0; i < nums; i++)
	{
		struct s_sisdb_method *c = _sis_method_table + i;
		// int o = sis_dict_add(map, sis_sdsnew(c->name), c);
		s_sis_sds key = sis_sdsnew(c->style);
        key = sis_sdscatfmt(key, ".%s", c->name);
		int o = sis_dict_add(map, key, c);
		assert(o == DICT_OK);
	}    
}

s_sisdb_method *sisdb_method_find_define(s_sis_map_pointer *map_, const char *name_, const char *style_)
{
    if (!map_) 
    {
        s_sisdb_server *server = digger_get_server();
        map_ = server->db->methods;
    }
	s_sisdb_method *val = NULL;
	if (map_)
	{
		s_sis_sds key = sis_sdsnew(style_);
        key = sis_sdscatfmt(key, ".%s", name_);
		val = (s_sisdb_method *)sis_dict_fetch_value(map_, key);
		sis_sdsfree(key);
	} 
	return val;
}

s_sisdb_method_alone* _sisdb_method_alone_load(const char *style_,
    s_sis_json_node *node_, s_sisdb_method_alone *prev_)
{
	if (!node_) return NULL;

	s_sisdb_method_alone *new = (s_sisdb_method_alone *)sis_malloc(sizeof(s_sisdb_method_alone));
	memset(new, 0 ,sizeof(s_sisdb_method_alone));

    new->method = sisdb_method_find_define(NULL, (const char *)&node_->key[1], style_);
    new->argv = sis_json_clone(sis_json_cmp_child_node(node_,"argv"), 1); // 全节点赋值

	if (prev_) 
	{
		prev_->next = new;
		new->prev = prev_;
	}
    return new;
}

s_sisdb_method_alone *sisdb_method_alone_create(const char *style_, s_sis_json_node *node_)
{
    s_sisdb_method_alone *first = NULL;

    s_sis_json_node *next = sis_json_first_node(node_);
    while (next)
    {
        if (next->key[0] == '$') 
        {
            first = _sisdb_method_alone_load(style_, next, first);
        }
        next = next->next;
    }
    return sisdb_method_alone_first(first);
}

void sisdb_method_alone_destroy(void *other_, void *node_)
{
    SIS_NOTUSED(other_);
	s_sisdb_method_alone *node = sisdb_method_alone_first((s_sisdb_method_alone *)node_);
	while (node)
	{
		s_sisdb_method_alone *next = node->next;
    	sis_json_delete_node(node->argv);
	    sis_free(node);
        node = next;
	} 
}
s_sisdb_method_alone *sisdb_method_alone_first(s_sisdb_method_alone *node_)
{
	while (node_->prev)
	{
		node_ = node_->prev;
	}
    return node_;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// 
////////////////////////////////////////////////////////////////////////////////////////////////
//com_为一个json格式字段定义
s_sisdb_table *sisdb_table_create(s_sis_db *db_, const char *name_, s_sis_json_node *com_)
{
	s_sisdb_table *tb = sisdb_get_table(db_, name_);
	if (tb)
	{ 
		sis_out_log(1)("%s already exist.", name_);
		return NULL;
		// sisdb_table_destroy(tb);
	}
	// 先加载默认配置
	tb = sis_malloc(sizeof(s_sisdb_table));
	memset(tb, 0, sizeof(s_sisdb_table));

	tb->control.type = SIS_TABLE_TYPE_STS; // 默认保存的目前都是struct，
	tb->control.scale = SIS_TIME_SCALE_SECOND;
	tb->control.limits = sis_json_get_int(com_, "limit", 0);
	tb->control.issys = 0;
	tb->control.isinit = sis_json_get_int(com_, "isinit", 0);
	tb->control.issubs = 0;
	tb->control.iszip = 0;
	//
	tb->collect_list = sis_string_list_create_w();

	tb->version = (uint32)sis_time_get_now();
	tb->name = sis_sdsnew(name_);
	tb->father = db_;
	tb->append_method = NULL;

	sis_dict_add(db_->dbs, sis_sdsnew(name_), tb);

	// 加载实际配置
	const char *strval = sis_json_get_str(com_, "scale");
	s_sis_map_define *mm = sisdb_find_map_define(db_->map, strval, SIS_MAP_DEFINE_TIME_SCALE);
	if (mm)
	{
		tb->control.scale = mm->uid;
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
	sisdb_table_set_fields(db_, tb, sis_json_cmp_child_node(com_, "fields"));
	// int count = sis_string_list_getsize(tb->field_name);
	// for (int i = 0; i < count; i++)
	// {
	// 	printf("---111  %s\n",sis_string_list_get(tb->field_name, i));
	// }

	s_sis_json_node *appends = sis_json_cmp_child_node(com_, "append-method");
	if (appends)
	{
		tb->append_method = sisdb_method_alone_create("append", appends);
	} 
	else 
	{
		if (tb->control.limits == 0)
		{
			tb->control.limits = 1;
		}
	}
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
				fu->subscribe_method = sisdb_method_alone_create("subscribe", child);
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

	sis_string_list_destroy(tb_->collect_list);
	sis_string_list_destroy(tb_->publishs);

	sis_string_list_destroy(tb_->field_name);
	
	sis_sdsfree(tb_->name);
	sis_free(tb_);
}

s_sisdb_table *sisdb_get_table(s_sis_db *db_, const char *dbname_)
{
	s_sisdb_table *val = NULL;
	if (db_->dbs)
	{
		s_sis_sds key = sis_sdsnew(dbname_);
		val = (s_sisdb_table *)sis_dict_fetch_value(db_->dbs, key);
		sis_sdsfree(key);
	}
	return val;
}
s_sisdb_table *sisdb_get_table_from_key(s_sis_db *db_, const char *key_)
{
	char db[SIS_MAXLEN_TABLE];
    sis_str_substr(db, SIS_MAXLEN_TABLE, key_, '.', 1);
	return sisdb_get_table(db_, db);
}

/////////////////////////////////////
//对数据库的各种属性设置
////////////////////////////////////

int sisdb_table_set_fields(s_sis_db *db_,s_sisdb_table *tb_, s_sis_json_node *fields_)
{
	int o = 0;
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
		map = sisdb_find_map_define(db_->map, val, SIS_MAP_DEFINE_FIELD_TYPE);
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

		if(!db_->special) 
		{
			// 非专用数据表不支持某些字段类型
			if (flags.type == SIS_FIELD_TYPE_PRICE)
			{
				flags.type = SIS_FIELD_TYPE_FLOAT;
				flags.dot = flags.dot == 0 ? 2 : flags.dot;
			}
			if (flags.type == SIS_FIELD_TYPE_VOLUME||flags.type == SIS_FIELD_TYPE_AMOUNT)
			{
				flags.type = SIS_FIELD_TYPE_UINT;
			}
		}

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
// #define STR_APPEND_METHOD "{ \"$only\":{ \"fields\": \"time\"} }"
#define STR_APPEND_METHOD "time"

// 这里的source指的就是实际的数据,根据实际数据生成默认的配置字符串
s_sis_json_node *sisdb_table_new_config(const char *source_ ,size_t len_)
{
	s_sis_json_handle *handle = sis_json_load(source_, len_);
	if (!handle)
	{
		return NULL;
	}
	s_sis_json_node *source = sis_json_clone(handle->node, 1);
	sis_json_close(handle);

	s_sis_json_node *node = sis_json_create_object();

	s_sis_json_node *fields = sis_json_create_array(); 
	s_sis_json_node *next = sis_json_first_node(source);
	while(next)
	{
		s_sis_json_node *one = sis_json_create_array(); 
		sis_json_array_set_string(one, 0, next->key, strlen(next->key));
		if (!sis_strcasecmp(next->key, "time"))
		{	
			long val = atol(next->value);
			if((val > 19000101) && (val < 20500101))
			{
				sis_json_object_set_string(node, "scale", "date", 4);
				sis_json_array_set_string(one, 1, "date", strlen("date"));
				sis_json_array_set_int(one, 2, 4);
			} else
			{
				sis_json_object_set_string(node, "scale", "second", 6);
				sis_json_array_set_string(one, 1, "second", strlen("second"));
				sis_json_array_set_int(one, 2, 4);
			}
		} 
		else 
		{
			switch (next->type)
			{
				case SIS_JSON_STRING:
					sis_json_array_set_string(one, 1, "char", strlen("char"));
					sis_json_array_set_int(one, 2, strlen(next->value) * 2);
					break;
				case SIS_JSON_DOUBLE:
					{
						sis_json_array_set_string(one, 1, "float", strlen("float"));
						sis_json_array_set_int(one, 2, 4);
						char tail[16];
        				sis_str_substr(tail, 16, next->value, '.', 1);
						sis_json_array_set_int(one, 3, strlen(tail));
					}
					break;
				case SIS_JSON_INT:
				default:
					{
						if (next->value[0]=='-')
						{
							sis_json_array_set_string(one, 1, "int", strlen("int"));
						} else
						{
							sis_json_array_set_string(one, 1, "uint", strlen("uint"));
						} 
						sis_json_array_set_int(one, 2, 4);
					}
					break;
			}
		}
		sis_json_array_add_node(fields, one);
		next = next->next;
	}
	sis_json_object_add_node(node, "fields", fields);
	// --- 设置其他字段
	// sis_json_object_add_string(node, "scale", "date", 4);  // 默认为日线数据
	sis_json_object_add_uint(node, "limit", 0);
	// sis_json_object_add_uint(node, "isinit", 0);
	// sis_json_object_add_uint(node, "issys", 0);
	sis_json_object_add_string(node, "append-method", STR_APPEND_METHOD, strlen(STR_APPEND_METHOD));  
	// handle = sis_json_load(STR_APPEND_METHOD, strlen(STR_APPEND_METHOD));
	// if (handle)
	// {
	// 	s_sis_json_node *append = sis_json_clone(handle->node, 1);
	// 	sis_json_object_add_node(node, "append-method", append);
	// 	sis_json_close(handle);
	// }

	// size_t len;
	// char *str = sis_json_output(node, &len);
	// if (str)
	// {
	// 	printf("%s\n", str);
	// 	sis_free(str);
	// }
	return node;

}

// s_sis_json_node *sisdb_table_make(s_sis_db *db_, s_sisdb_table *tb_)
// {
// 	s_sis_json_node *node = sis_json_create_node();

// 	const char *str = sisdb_find_map_name(db_->map, tb_->control.scale, SIS_MAP_DEFINE_TIME_SCALE);
	
// 	sis_json_object_set_string(node, "scale", str, strlen(str));
// 	sis_json_object_set_int(node, "limit", tb_->control.limits);
// 	sis_json_object_set_int(node, "isinit", tb_->control.isinit);
// 	sis_json_object_set_int(node, "issys", tb_->control.issys);

// 	int count = sis_string_list_getsize(tb_->publishs);
// 	if (count > 0)
// 	{
// 		s_sis_sds val = sis_sdsnew(NULL);
// 		for(int i = 0; i < count; i++)
// 		{
// 			val = sis_sdscat(val, sis_string_list_get(tb_->publishs, i));
// 		}
// 		sis_json_object_set_string(node, "publishs", val, sis_sdslen(val));
// 		sis_sdsfree(val);
// 	}
// 	//处理字段定义
// 	if (count > 0)
// 	{
// 		s_sis_json_node *fields = sis_json_create_array(); 
// 		count = sis_string_list_getsize(tb_->field_name);
// 		for(int i = 0; i < count; i++)
// 		{
// 			const char *key = sis_string_list_get(tb_->field_name, i);
// 			s_sisdb_field * val = (s_sisdb_field *)sis_map_pointer_get(tb->field_map, key);
// 			s_sis_json_node *one = sis_json_create_array(); 

// 			sis_json_array_set_string(one, i, val->name, strlen(val->name));
// 			str = sisdb_find_map_name(db_->map, val->flags.type, SIS_MAP_DEFINE_FIELD_TYPE);
// 			sis_json_array_set_string(one, i, str, strlen(str));
// 			sis_json_array_set_int(one, i, val->flags.int);
// 			if (val->flags.type == SIS_FIELD_TYPE_FLOAT)
// 			{
// 				sis_json_array_set_int(one, i, val->flags.dot);
// 			}
// 			sis_json_array_add_node(fields, one);
// 		}
// 		sis_json_object_add_node(node, "fields", fields);
// 	}
// 	if (tb_->append_method != SIS_ADD_METHOD_NONE)
// 	{

// 	}
// 	if (tb_->control.issubs)
// 	{

// 	}
// 	if (tb_->control.iszip)
// 	{
		
// 	}	
// }

// 根据各个属性不同，单独处理到json中，然后统一调用, 没有属性什么也不做
// table.scale  1
// table.fields []
int sisdb_table_update(s_sis_db *db_,s_sisdb_table *tb_, const char *key_ ,const char *val_ ,size_t len_) 
{
	// if (strlen(key_)<1)
	// {

	// 	// 如果
	// 	// return sisdb_table_set_conf();
	// } 
	// tb_一定有值
	return 0;
} 

// 先从tb中生成一个 只更新有的关键字，没有的关键字用cfg_的值，
// 仅仅是改conf，并不对数据进行操作
int sisdb_table_set_conf(s_sis_db *db_, const char *table_, s_sis_json_node *cfg_)
{
	s_sis_json_node *tables = sis_json_cmp_child_node(db_->conf, "tables");
	s_sis_json_node *node = sis_json_cmp_child_node(tables, table_);
	if (!node)
	{
		s_sis_json_node *cfg = sis_json_clone(cfg_, 1);
		sis_json_object_add_node(tables, table_, cfg);
		return 0;
	}
	return 1;
}