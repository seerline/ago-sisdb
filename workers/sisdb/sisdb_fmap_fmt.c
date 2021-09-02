
#include <sisdb_fmap.h>
#include <sisdb_io.h>
#include <sisdb.h>

////////////////////////////////////////////
// format 
///////////////////////////////////////////
// s_sis_json_node *sis_collect_get_fields_of_json(s_sis_dynamic_db *sdb_, s_sis_string_list *fields_)
// {
// 	s_sis_json_node *node = sis_json_create_object();
// 	// 先处理字段
// 	int  sno = 0;
// 	char sonkey[64];
// 	int count = sis_string_list_getsize(fields_);
// 	for (int i = 0; i < count; i++)
// 	{
// 		const char *key = sis_string_list_get(fields_, i);
// 		s_sisdb_field *fu = sis_dynamic_db_get_field(sdb_, NULL, (char *)key);
// 		if(!fu) continue;
// 		if (fu->count > 1)
// 		{
// 			for(int index = 0; index < fu->count; index++)
// 			{
// 				sis_sprintf(sonkey, 64, "%s.%d", key, index);
// 				sis_json_object_add_uint(node, sonkey, sno);
// 				sno++;
// 			}
// 		}
// 		else
// 		{
// 			sis_json_object_add_uint(node, key, sno);
// 			sno++;
// 		}		
// 	}
// 	// sis_json_object_add_node(jone, SIS_JSON_KEY_FIELDS, jfields);
// 	return node;
// }

// s_sis_sds sis_collect_get_fields_of_csv(s_sisdb_collect *collect_, s_sis_string_list *fields_)
// {
// 	s_sis_sds o = sis_sdsempty();

// 	// 先处理字段
// 	int  sno = 0;
// 	char sonkey[64];
// 	int count = sis_string_list_getsize(fields_);
// 	for (int i = 0; i < count; i++)
// 	{
// 		const char *key = sis_string_list_get(fields_, i);
// 		s_sisdb_field *fu = sis_dynamic_db_get_field(collect_->sdb, NULL, (char *)key);
// 		if(!fu) continue;
// 		if (fu->count > 1)
// 		{
// 			for(int index = 0; index < fu->count; index++)
// 			{
// 				sis_sprintf(sonkey, 64, "%s.%d", key, index);
// 				o = sis_csv_make_str(o, sonkey, sis_strlen(sonkey));
// 				sno++;
// 			}
// 		}
// 		else
// 		{
// 			o = sis_csv_make_str(o, key, sis_strlen(key));
// 			sno++;
// 		}		
// 	}
// 	o = sis_csv_make_end(o);
// 	return o;
// }

// s_sis_sds sisdb_collect_struct_to_sds(s_sis_dynamic_db *db_, const char *in_, size_t ilen_, s_sis_string_list *fields_)
// {
// 	// 一定不是全字段
// 	s_sis_sds o = NULL;

// 	int count = (int)(ilen_ / db_->size);
// 	const char *val = in_;
// 	for (int k = 0; k < count; k++)
// 	{
// 		for (int i = 0; i < sis_string_list_getsize(fields_); i++)
// 		{
// 			int index = 0;
// 			s_sisdb_field *fu = sis_dynamic_db_get_field(db_, &index, sis_string_list_get(fields_, i));
// 			if (!fu)
// 			{
// 				continue;
// 			}
// 			if (!o)
// 			{
// 				o = sis_sdsnewlen(val + fu->offset + index * fu->len, fu->len);
// 			}
// 			else
// 			{
// 				o = sis_sdscatlen(o, val + fu->offset + index * fu->len, fu->len);
// 			}
// 		}
// 		val += db_->size;
// 	}
// 	return o;
// }

// s_sis_sds sisdb_collect_struct_to_json_sds(s_sis_dynamic_db *db_, const char *in_, size_t ilen_,
// 										   const char *key_, s_sis_string_list *fields_,  bool isfields_, bool zip_)
// {
	
// 	int fnums = sis_string_list_getsize(fields_);

// 	s_sis_json_node *jone = sis_json_create_object();
	
// 	if (isfields_)
// 	{
// 		s_sis_json_node *jfields = sis_json_create_object();
// 		for (int i = 0; i < fnums; i++)
// 		{
// 			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(db_->fields, sis_string_list_get(fields_, i));
// 			if (inunit->count > 1)
// 			{
// 				for (int k = 0; k < inunit->count; k++)
// 				{
// 					char fname[64];
// 					sis_sprintf(fname, 64, "%s%d", inunit->fname, k);
// 					sis_json_object_add_uint(jfields, fname, i);
// 				}
// 			}
// 			else
// 			{
// 				sis_json_object_add_uint(jfields, inunit->fname, i);
// 			}
			
// 		}
// 		sis_json_object_add_node(jone, "fields", jfields);
// 	}

// 	s_sis_json_node *jtwo = sis_json_create_array();
// 	const char *val = in_;
// 	int count = (int)(ilen_ / db_->size);
// 	for (int k = 0; k < count; k++)
// 	{
// 		s_sis_json_node *jval = sis_json_create_array();
// 		for (int i = 0; i < fnums; i++)
// 		{
// 			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(db_->fields, sis_string_list_get(fields_, i));
// 			sis_dynamic_field_to_array(jval, inunit, val);
// 		}
// 		sis_json_array_add_node(jtwo, jval);
// 		val += db_->size;
// 	}
// 	sis_json_object_add_node(jone, key_? key_ : "values", jtwo);

// 	s_sis_sds o = sis_json_to_sds(jone, zip_);

// 	sis_json_delete_node(jone);	
// 	return o;
// }

// s_sis_sds sisdb_collect_struct_to_array_sds(s_sis_dynamic_db *db_, const char *in_, size_t ilen_,
// 											s_sis_string_list *fields_, bool zip_)
// {
// 	s_sis_json_node *jone = sis_json_create_array();
// 	int fnums = sis_string_list_getsize(fields_);

// 	const char *val = in_;
// 	int count = (int)(ilen_ / db_->size);
// 	for (int k = 0; k < count; k++)
// 	{
// 		s_sis_json_node *jval = sis_json_create_array();
// 		for (int i = 0; i < fnums; i++)
// 		{
// 			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(db_->fields, sis_string_list_get(fields_, i));
// 			sis_dynamic_field_to_array(jval, inunit, val);
// 		}
// 		sis_json_array_add_node(jone, jval);
// 		val += db_->size;
// 	}
// 	s_sis_sds o = sis_json_to_sds(jone, zip_);
// 	sis_json_delete_node(jone);	
// 	return o;
// }

// s_sis_sds sisdb_collect_struct_to_csv_sds(s_sis_dynamic_db *db_, const char *in_, size_t ilen_,
// 										  s_sis_string_list *fields_, bool isfields_)
// {
// 	s_sis_sds o = sis_sdsempty();
// 	int fnums = sis_string_list_getsize(fields_);
// 	if (isfields_)
// 	{
// 		for (int i = 0; i < fnums; i++)
// 		{
// 			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(db_->fields, sis_string_list_get(fields_, i));
// 			if (inunit->count > 1)
// 			{
// 				for (int k = 0; k < inunit->count; k++)
// 				{
// 					char fname[64];
// 					sis_sprintf(fname, 64, "%s%d", inunit->fname, k);
// 					o = sis_csv_make_str(o, fname, sis_strlen(fname));
// 				}
// 			}
// 			else
// 			{
// 				o = sis_csv_make_str(o, inunit->fname, sis_strlen(inunit->fname));
// 			}
// 		}
// 		o = sis_csv_make_end(o);
// 	}

// 	const char *val = in_;
// 	int count = (int)(ilen_ / db_->size);

// 	for (int k = 0; k < count; k++)
// 	{
// 		for (int i = 0; i < fnums; i++)
// 		{
// 			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(db_->fields, sis_string_list_get(fields_, i));

// 			// printf("fnums=%d %d %p %zu\n", fnums, sis_map_list_getsize(db_->fields), val, ilen_);
// 			// sis_out_binary("val", val, db_->size * 2);
// 			// printf("inunit=%p  %s\n", inunit, sis_string_list_get(fields_, i));
// 			o = sis_dynamic_field_to_csv(o, inunit, val);			
// 		}
// 		o = sis_csv_make_end(o);
// 		val += db_->size;
// 	}
// 	return o;
// }


// s_sis_sds sisdb_collect_json_to_struct_sds(s_sisdb_collect *collect_, s_sis_sds in_)
// {
// 	s_sis_json_handle *handle = sis_json_load(in_, sis_sdslen(in_));
// 	if (!handle)
// 	{
// 		return NULL;
// 	}
// 	// 取最后一条记录的数据作为底
// 	s_sis_sds o = NULL;
// 	if (SIS_OBJ_LIST(collect_->obj))
// 	{
// 		const char *src = sis_struct_list_last(SIS_OBJ_LIST(collect_->obj));
// 		o = sis_sdsnewlen(src, collect_->sdb->size);
// 	}
// 	else
// 	{
// 		o = sis_sdsnewlen(NULL, collect_->sdb->size);
// 	}
	
// 	s_sisdb_table *table_ = collect_->sdb;

// 	int fnums = sis_map_list_getsize(table_->fields);
// 	char key[64];
// 	for (int k = 0; k < fnums; k++)
// 	{
// 		s_sisdb_field *fu = (s_sisdb_field *)sis_map_list_geti(table_->fields, k);
// 		for (int i = 0; i < fu->count; i++)
// 		{
// 			if (fu->count > 1)
// 			{
// 				sis_sprintf(key, 64, "%s%d", fu->fname, i);
// 				// printf("key = %s %d\n", key, i);
// 				sis_dynamic_field_json_to_struct(o, fu, i, key, handle->node);
// 			}
// 			else
// 			{
// 				sis_dynamic_field_json_to_struct(o, fu, i, fu->fname, handle->node);
// 			}			
// 		}
// 	}
// 	sis_json_close(handle);
// 	// printf("struct len = %d %d\n", collect_->sdb->size, sis_sdslen(o));
// 	return o;
// }

// s_sis_sds sisdb_collect_array_to_struct_sds(s_sisdb_collect *collect_, s_sis_sds in_)
// {
// 	// 字段个数一定要一样
// 	s_sis_json_handle *handle = sis_json_load(in_, sis_sdslen(in_));
// 	if (!handle)
// 	{
// 		return NULL;
// 	}
// 	// 获取字段个数
// 	s_sis_json_node *jval = NULL;

// 	int count = 0;
// 	if (handle->node->child && handle->node->child->type == SIS_JSON_ARRAY)
// 	{ // 表示二维数组
// 		jval = handle->node->child;
// 		count = sis_json_get_size(handle->node);
// 	}
// 	else
// 	{
// 		count = 1;
// 		jval = handle->node;
// 	}
// 	// printf("array to = %d %d\n", count, collect_->sdb->size);
// 	if (count < 1)
// 	{
// 		sis_json_close(handle);
// 		return NULL;
// 	}

// 	int fnums = sis_map_list_getsize(collect_->sdb->fields);
// 	s_sis_sds o = sis_sdsnewlen(NULL, count * collect_->sdb->size);
// 	int index = 0;
// 	while (jval)
// 	{
// 		int size = sis_json_get_size(jval);
// 		if (size != fnums)
// 		{
// 			LOG(3)("input fields[%d] count error [%d].\n", size, fnums);
// 			jval = jval->next;
// 			continue;
// 		}
// 		char key[32];
// 		int fidx = 0;
// 		for (int k = 0; k < fnums; k++)
// 		{
// 			s_sisdb_field *fu = (s_sisdb_field *)sis_map_list_geti(collect_->sdb->fields, k);
// 			for (int i = 0; i < fu->count; i++)
// 			{
// 				sis_llutoa(fidx, key, 32, 10);
// 				sis_dynamic_field_json_to_struct(o + index * collect_->sdb->size, fu, i, key, jval);
// 				// printf("%s %d\n",key, fidx);
// 				fidx++;
// 			}
// 		}
// 		index++;
// 		jval = jval->next;
// 	}
// 	sis_json_close(handle);
// 	return o;
// }
