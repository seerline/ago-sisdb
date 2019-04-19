
#include "sisdb_table.h"
#include "sisdb_map.h"
#include "sisdb_fields.h"
#include "sisdb_io.h"
#include "sisdb_method.h"
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
	printf("loaded %s.\n", tb->name);
	tb->father = db_;

	tb->write_style = SIS_WRITE_ALWAYS;
	tb->write_sort = NULL;
	tb->write_solely = sis_pointer_list_create();
	tb->write_method = NULL;

	sis_dict_add(db_->dbs, sis_sdsnew(name_), tb);

	// 加载实际配置
	const char *strval = sis_json_get_str(com_, "scale");
	if (!strval)
	{
		tb->control.scale = SIS_TIME_SCALE_NONE;
	} else {
		s_sisdb_server *server = sisdb_get_server();
		s_sis_map_define *mm = sisdb_find_map_define(server->maps, strval, SIS_MAP_DEFINE_TIME_SCALE);
		if (mm)
		{
			tb->control.scale = mm->uid;
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
	sisdb_table_set_fields(db_, tb, sis_json_cmp_child_node(com_, "fields"));
	// int count = sis_string_list_getsize(tb->field_name);
	// for (int i = 0; i < count; i++)
	// {
	// 	printf("---111  %s\n",sis_string_list_get(tb->field_name, i));
	// }
	s_sisdb_server *server = sisdb_get_server();

	s_sis_json_node *writes = sis_json_cmp_child_node(com_, "write-method");
	if (writes)
	{
		tb->write_style |= SIS_WRITE_CHECKED;
		if (sis_json_cmp_child_node(writes, "sort"))
		{
			tb->write_sort = sis_sdsnew(sis_json_get_str(writes, "sort"));
			if(!sis_strcasecmp(tb->write_sort, "time")) 
			{
				tb->write_style |= SIS_WRITE_SORT_TIME;
			} else 
			{
				tb->write_style |= SIS_WRITE_SORT_OTHER;
			}
		} 
		else
		{
			tb->write_style |= SIS_WRITE_SORT_NONE;
		}
		if (sis_json_cmp_child_node(writes, "solely"))
		{
			strval = sis_json_get_str(writes, "solely");
			int count = sis_str_substr_nums(strval, ',');	
			for(int i = 0; i < count; i++)
			{
				char field[32];
				sis_str_substr(field, 32, strval, ',', i);
				s_sisdb_field *fu = sisdb_field_get_from_key(tb, field);
				sis_pointer_list_push(tb->write_solely, fu);
			}
		}
		if (tb->write_solely->count < 1)
		{
			tb->write_style |= SIS_WRITE_SOLE_NONE;
		} else 
		{
			bool finded = false;
			for(int i = 0; i < tb->write_solely->count; i++)
			{
				s_sisdb_field *fu = (s_sisdb_field *)sis_pointer_list_get(tb->write_solely, i);
				if(fu&&!sis_strcasecmp(fu->name, "time")) 
				{
					finded = true;
					break;
				}
			}
			if (!finded)
			{
				tb->write_style |= SIS_WRITE_SOLE_OTHER;
			} else 
			{
				if (tb->write_solely->count == 1)
				{
					tb->write_style |= SIS_WRITE_SOLE_TIME;
				} else 
				{
					tb->write_style |= SIS_WRITE_SOLE_MULS;
				}
			}
		}		
		s_sis_json_node *methods = sis_json_cmp_child_node(writes, "checked");
		if (methods)
		{
			tb->write_method = sis_method_class_create(server->methods, SISDB_METHOD_STYLE_WRITE, methods);
			tb->write_method->style = SIS_METHOD_CLASS_JUDGE;
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
				fu->subscribe_method = sis_method_class_create(server->methods, SISDB_METHOD_STYLE_SUBSCRIBE, child);
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

	sisdb_sys_load_default(db_, com_);
	// 加载默认值
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

	if (tb_->write_sort) sis_sdsfree(tb_->write_sort);
	sis_pointer_list_destroy(tb_->write_solely);
	if(tb_->write_method)
	{
		sis_method_class_destroy(tb_->write_method);
	}
	sis_string_list_destroy(tb_->field_name);
	
	sis_sdsfree(tb_->name);
	sis_free(tb_);
}

s_sisdb_table *sisdb_get_table(s_sis_db *db_, const char *tbname_)
{
	s_sisdb_table *val = (s_sisdb_table *)sis_map_pointer_get(db_->dbs, tbname_);
	// s_sisdb_table *val = NULL;
	// if (db_->dbs)
	// {
	// 	s_sis_sds key = sis_sdsnew(tbname_);
	// 	val = (s_sisdb_table *)sis_dict_fetch_value(db_->dbs, key);
	// 	sis_sdsfree(key);
	// }
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

	s_sisdb_field_flags flags;
	s_sis_map_define *map = NULL;
	int index = 0;
	int offset = 0;

	s_sisdb_server *server = sisdb_get_server();

	s_sis_json_node *node = sis_json_first_node(fields_);
	while (node)
	{
		// size_t ss;
		// printf("node=%s\n", sis_json_output(node, &ss));
		const char *name = node->key;
		if (!name) 
		{
			node = sis_json_next_node(node);
			continue;
		}
		flags.type = SIS_FIELD_TYPE_INT;
		const char *val = sis_json_get_str(node, "0");
		map = sisdb_find_map_define(server->maps, val, SIS_MAP_DEFINE_FIELD_TYPE);
		if (map)
		{
			flags.type = map->uid;
		}
		int len = sis_json_get_int(node, "1", 4);
		// 合法性检查以后用统一出口检查，包括整型检查等
		flags.len = len;
		if(flags.type==SIS_FIELD_TYPE_STRING||flags.type==SIS_FIELD_TYPE_JSON)
		{
			flags.len = sizeof(void *);
		}
		int count = sis_json_get_int(node, "2", 1);
		flags.dot = sis_json_get_int(node, "3", 0);
		if (count > 1)
		{
			char field_name[32];
			
			for(int i = 0; i < count; i++)
			{
				sis_sprintf(field_name, 32, "%s%d", name, i+1);
				s_sisdb_field *unit = sisdb_field_create(index++, field_name, &flags);
				unit->offset = offset;
				offset += unit->flags.len;
				// printf("[%d:%d] name=%s len=%d\n", index, offset, name, flags.len);
				sis_map_pointer_set(tb_->field_map, field_name, unit);
				sis_string_list_push(tb_->field_name, field_name, strlen(field_name));			
			}
		} else 
		{
			s_sisdb_field *unit = sisdb_field_create(index++, name, &flags);
			unit->offset = offset;
			offset += unit->flags.len;
			// printf("[%d:%d] name=%s len=%d\n", index, offset, name, flags.len);
			sis_map_pointer_set(tb_->field_map, name, unit);
			sis_string_list_push(tb_->field_name, name, strlen(name));			
		}
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
						sis_json_array_set_int(one, 3, 1);
						char tail[16];
        				sis_str_substr(tail, 16, next->value, '.', 1);
						sis_json_array_set_int(one, 4, strlen(tail));
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

	s_sis_json_node *write_method = sis_json_create_object(); 
	sis_json_object_add_string(write_method, "sort", "time", strlen("time"));  
	sis_json_object_add_string(write_method, "solely", "time", strlen("time"));  
	sis_json_object_add_node(node, "write-method", write_method);

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