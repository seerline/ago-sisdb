
#include "sis_utils.h"

/////////////////////////////////////////////////
// 对 s_sis_dynamic_db 信息的提取和转换
/////////////////////////////////////////////////

s_sis_sds sis_sdbinfo_to_conf(s_sis_dynamic_db *db_, s_sis_sds in_)
{
	if (!in_)
	{
		in_ = sis_sdsempty();
	}
	in_ = sis_sdscatfmt(in_, "%s:", db_->name);
	in_ = sis_sdscat(in_, "{fields:{");
	char sign[5];
	int fnums = sis_map_list_getsize(db_->fields);
	for(int i = 0; i < fnums; i++)
	{
		s_sis_dynamic_field *unit= (s_sis_dynamic_field *)sis_map_list_geti(db_->fields, i);
		if(!unit) continue;
		if (i > 0)
		{
			in_ = sis_sdscat(in_, ",");
		}
		in_ = sis_sdscatfmt(in_, "%s:[", unit->fname);
		sign[0] = unit->style; sign[1] = 0;
		in_ = sis_sdscatfmt(in_, "%s", sign);
		in_ = sis_sdscatfmt(in_, ",%u", unit->len);
		int index = 0;
		if (unit->mindex)
		{
			sign[index] = 'I'; index++;
		} 
		if (unit->solely)
		{
			sign[index] = 'O'; index++;
		} 
		sign[index] = 0;
		if (sign[0]) 
		{
			in_ = sis_sdscatfmt(in_, ",%u", unit->count);
			in_ = sis_sdscatfmt(in_, ",%u", unit->dot);
			in_ = sis_sdscatfmt(in_, "%s", sign);
		}
		else
		{
			if (unit->dot > 0) 
			{
				in_ = sis_sdscatfmt(in_, ",%u", unit->count);
				in_ = sis_sdscatfmt(in_, ",%u", unit->dot);
			}	
			else
			{
				if (unit->count > 0) 
				{
					in_ = sis_sdscatfmt(in_, ",%u", unit->count);
				}
			}
		}
		in_ = sis_sdscat(in_, "]");
	}
	in_ = sis_sdscat(in_, "}}");
	return in_;
}

s_sis_json_node *sis_sdbinfo_to_json(s_sis_dynamic_db *db_)
{
	s_sis_json_node *jone = sis_json_create_object();
	s_sis_json_node *jfields = sis_json_create_object();

	// 先处理字段
	char sign[5];
	int  index = 0;
	int fnums = sis_map_list_getsize(db_->fields);
	for (int i = 0; i < fnums; i++)
	{
		s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_geti(db_->fields, i);
		if(!inunit) continue;
		s_sis_json_node *jfield = sis_json_create_array();
		// sis_json_array_add_uint(jfield, i);
		sign[0] = inunit->style; sign[1] = 0;
		sis_json_array_add_string(jfield, sign, 1);
		sis_json_array_add_uint(jfield, inunit->len);

		index = 0;
		if (inunit->mindex)
		{
			sign[index] = 'I'; index++;
		} 
		if (inunit->solely)
		{
			sign[index] = 'O'; index++;
		} 
		sign[index] = 0;
		if (sign[0]) 
		{
			sis_json_array_add_uint(jfield, inunit->count);
			sis_json_array_add_uint(jfield, inunit->dot);
			sis_json_array_add_string(jfield, sign, index);
		}
		else
		{
			if (inunit->dot > 0) 
			{
				sis_json_array_add_uint(jfield, inunit->count);
				sis_json_array_add_uint(jfield, inunit->dot);	
			}	
			else
			{
				if (inunit->count > 0) 
				{
					sis_json_array_add_uint(jfield, inunit->count);
				}
			}
		}
		sis_json_object_add_node(jfields, inunit->fname, jfield);
	}
	sis_json_object_add_node(jone, "fields", jfields);
	return jone;
}

s_sis_sds sis_sdb_to_array_sds(s_sis_dynamic_db *db_, const char *key_, void *in_, size_t ilen_)
{
	s_sis_dynamic_db *indb = db_;

	int count = (int)(ilen_ / indb->size);
	int fnums = sis_map_list_getsize(indb->fields);

	if (count < 1 || fnums < 1)
	{
		return NULL;
	}
	s_sis_json_node *jone = sis_json_create_object();
	s_sis_json_node *jtwo = sis_json_create_array();
	printf("to array : fnum = %d count = %d \n", fnums, count);
	const char *val = (const char *)in_;
	for (int k = 0; k < count; k++)
	{
		s_sis_json_node *jval = sis_json_create_array();
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_geti(indb->fields, i);
			sis_dynamic_field_to_array(jval, inunit, val);
		}
		if (count == 1)
		{
			sis_json_delete_node(jtwo);	
			jtwo = jval;
		}
		else
		{
			sis_json_array_add_node(jtwo, jval);
		}
		val += indb->size;
	}
	printf("to array : fnum = %d count = %d \n", fnums, count);
	s_sis_sds o = NULL;	
	if (key_)
	{
		sis_json_object_add_node(jone, key_, jtwo);
		o = sis_json_to_sds(jone, true);
	}
	else
	{
		o = sis_json_to_sds(jtwo, true);	
	}
	sis_json_delete_node(jone);	
	return o;
}

s_sis_sds sis_sdb_to_csv_sds(s_sis_dynamic_db *db_, void *in_, size_t ilen_)
{
	s_sis_dynamic_db *indb = db_;

	int count = (int)(ilen_ / indb->size);

	s_sis_sds o = sis_sdsempty();
	int fnums = sis_map_list_getsize(indb->fields);
	const char *val = (const char *)in_;
	for (int k = 0; k < count; k++)
	{
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_geti(indb->fields, i);
			o = sis_dynamic_field_to_csv(o, inunit, val);
		}
		o = sis_csv_make_end(o);
		val += indb->size;
	}
	return o;
}

s_sis_sds sis_sdb_to_array_of_conf_sds(const char *confstr_, void *in_, size_t ilen_)
{
	s_sis_conf_handle *injson = sis_conf_load(confstr_, strlen(confstr_));
	if (!injson)
	{
		return NULL;
	}
	s_sis_dynamic_db *indb = sis_dynamic_db_create(injson->node);
	if (!indb)
	{
		LOG(1)("init db error.\n");
		sis_conf_close(injson);
		return NULL;
	}
	sis_conf_close(injson);
	s_sis_sds o = sis_sdb_to_array_sds(indb, indb->name, in_, ilen_);
	sis_dynamic_db_destroy(indb);
	return o;
}

s_sis_json_node *sis_sdb_get_fields_of_json(s_sis_dynamic_db *sdb_, s_sis_string_list *fields_)
{
	s_sis_json_node *node = sis_json_create_object();
	// 先处理字段
	int  sno = 0;
	char sonkey[64];
	int count = sis_string_list_getsize(fields_);
	for (int i = 0; i < count; i++)
	{
		const char *key = sis_string_list_get(fields_, i);
		s_sis_dynamic_field *fu = sis_dynamic_db_get_field(sdb_, NULL, (char *)key);
		if(!fu) continue;
		if (fu->count > 1)
		{
			for(int index = 0; index < fu->count; index++)
			{
				sis_sprintf(sonkey, 64, "%s.%d", key, index);
				sis_json_object_add_uint(node, sonkey, sno);
				sno++;
			}
		}
		else
		{
			sis_json_object_add_uint(node, key, sno);
			sno++;
		}		
	}
	// sis_json_object_add_node(jone, SIS_JSON_KEY_FIELDS, jfields);
	return node;	
}

s_sis_sds sis_sdb_get_fields_of_csv(s_sis_dynamic_db *db_, s_sis_string_list *fields_)
{
	s_sis_sds o = sis_sdsempty();

	// 先处理字段
	int  sno = 0;
	char sonkey[64];
	int count = sis_string_list_getsize(fields_);
	for (int i = 0; i < count; i++)
	{
		const char *key = sis_string_list_get(fields_, i);
		s_sis_dynamic_field *fu = sis_dynamic_db_get_field(db_, NULL, (char *)key);
		if(!fu) continue;
		if (fu->count > 1)
		{
			for(int index = 0; index < fu->count; index++)
			{
				sis_sprintf(sonkey, 64, "%s.%d", key, index);
				o = sis_csv_make_str(o, sonkey, sis_strlen(sonkey));
				sno++;
			}
		}
		else
		{
			o = sis_csv_make_str(o, key, sis_strlen(key));
			sno++;
		}		
	}
	o = sis_csv_make_end(o);
	return o;	
}

s_sis_sds sisdb_sdb_struct_to_sds(s_sis_dynamic_db *db_, const char *in_, size_t ilen_, s_sis_string_list *fields_)
{
	// 一定不是全字段
	s_sis_sds o = NULL;

	int count = (int)(ilen_ / db_->size);
	const char *val = in_;
	for (int k = 0; k < count; k++)
	{
		for (int i = 0; i < sis_string_list_getsize(fields_); i++)
		{
			int index = 0;
			s_sis_dynamic_field *fu = sis_dynamic_db_get_field(db_, &index, sis_string_list_get(fields_, i));
			if (!fu)
			{
				continue;
			}
			if (!o)
			{
				o = sis_sdsnewlen(val + fu->offset + index * fu->len, fu->len);
			}
			else
			{
				o = sis_sdscatlen(o, val + fu->offset + index * fu->len, fu->len);
			}
		}
		val += db_->size;
	}
	return o;
}
// 数据按字段转换为csv
s_sis_sds sis_sdb_fields_to_csv_sds(s_sis_dynamic_db *db_, void *in_, size_t ilen_, s_sis_string_list *fields_, bool isfields_)
{
	s_sis_sds o = sis_sdsempty();
	int fnums = sis_string_list_getsize(fields_);
	// sis_map_list_getsize(db_->fields);
	// if (fields_)
	// {
	// 	fnums = sis_string_list_getsize(fields_);
	// }
	if (isfields_)
	{
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(db_->fields, sis_string_list_get(fields_, i));
			if (inunit->count > 1)
			{
				for (int k = 0; k < inunit->count; k++)
				{
					char fname[64];
					sis_sprintf(fname, 64, "%s%d", inunit->fname, k);
					o = sis_csv_make_str(o, fname, sis_strlen(fname));
				}
			}
			else
			{
				o = sis_csv_make_str(o, inunit->fname, sis_strlen(inunit->fname));
			}
		}
		o = sis_csv_make_end(o);
	}

	const char *val = in_;
	int count = (int)(ilen_ / db_->size);

	for (int k = 0; k < count; k++)
	{
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(db_->fields, sis_string_list_get(fields_, i));

			// printf("fnums=%d %d %p %zu\n", fnums, sis_map_list_getsize(db_->fields), val, ilen_);
			// sis_out_binary("val", val, db_->size * 2);
			// printf("inunit=%p  %s\n", inunit, sis_string_list_get(fields_, i));
			o = sis_dynamic_field_to_csv(o, inunit, val);			
		}
		o = sis_csv_make_end(o);
		val += db_->size;
	}
	return o;	
}

// 数据按字段转换为 array
s_sis_sds sis_sdb_fields_to_array_sds(s_sis_dynamic_db *db_, void *in_, size_t ilen_, s_sis_string_list *fields_, bool iszip_)
{
	s_sis_json_node *jone = sis_json_create_array();
	int fnums = sis_string_list_getsize(fields_);

	const char *val = in_;
	int count = (int)(ilen_ / db_->size);
	for (int k = 0; k < count; k++)
	{
		s_sis_json_node *jval = sis_json_create_array();
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(db_->fields, sis_string_list_get(fields_, i));
			sis_dynamic_field_to_array(jval, inunit, val);
		}
		sis_json_array_add_node(jone, jval);
		val += db_->size;
	}
	s_sis_sds o = sis_json_to_sds(jone, iszip_);
	sis_json_delete_node(jone);		
	return o;
}

s_sis_sds sis_sdb_fields_to_json_sds(s_sis_dynamic_db *db_, void *in_, size_t ilen_, const char *key_, s_sis_string_list *fields_, bool isfields_, bool iszip_)
{
	int fnums = sis_string_list_getsize(fields_);

	s_sis_json_node *jone = sis_json_create_object();
	
	if (isfields_)
	{
		s_sis_json_node *jfields = sis_json_create_object();
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(db_->fields, sis_string_list_get(fields_, i));
			if (inunit->count > 1)
			{
				for (int k = 0; k < inunit->count; k++)
				{
					char fname[64];
					sis_sprintf(fname, 64, "%s%d", inunit->fname, k);
					sis_json_object_add_uint(jfields, fname, i);
				}
			}
			else
			{
				sis_json_object_add_uint(jfields, inunit->fname, i);
			}
			
		}
		sis_json_object_add_node(jone, "fields", jfields);
	}

	s_sis_json_node *jtwo = sis_json_create_array();
	const char *val = in_;
	int count = (int)(ilen_ / db_->size);
	for (int k = 0; k < count; k++)
	{
		s_sis_json_node *jval = sis_json_create_array();
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(db_->fields, sis_string_list_get(fields_, i));
			sis_dynamic_field_to_array(jval, inunit, val);
		}
		sis_json_array_add_node(jtwo, jval);
		val += db_->size;
	}
	sis_json_object_add_node(jone, key_? key_ : "datas", jtwo);

	s_sis_sds o = sis_json_to_sds(jone, iszip_);

	sis_json_delete_node(jone);	
	return o;	
}


s_sis_sds sis_array_to_struct_sds(s_sis_dynamic_db *db_, s_sis_sds in_)
{
	s_sis_json_handle *handle = sis_json_load(in_, sis_sdslen(in_));
	if (!handle)
	{
		return NULL;
	}
	// 获取字段个数
	s_sis_json_node *jval = NULL;

	int count = 0;
	if (handle->node->child && handle->node->child->type == SIS_JSON_ARRAY)
	{ // 表示二维数组
		jval = handle->node->child;
		count = sis_json_get_size(handle->node);
	}
	else
	{
		count = 1;
		jval = handle->node;
	}
	// printf("array to = %d %d\n", count, collect_->sdb->size);
	if (count < 1)
	{
		sis_json_close(handle);
		return NULL;
	}

	int fnums = sis_map_list_getsize(db_->fields);
	s_sis_sds o = sis_sdsnewlen(NULL, count * db_->size);
	int index = 0;
	while (jval)
	{
		int size = sis_json_get_size(jval);
		if (size != fnums)
		{
			LOG(3)("input fields[%d] count error [%d].\n", size, fnums);
			jval = jval->next;
			continue;
		}
		char key[32];
		int fidx = 0;
		for (int k = 0; k < fnums; k++)
		{
			s_sis_dynamic_field *fu = (s_sis_dynamic_field *)sis_map_list_geti(db_->fields, k);
			for (int i = 0; i < fu->count; i++)
			{
				sis_llutoa(fidx, key, 32, 10);
				sis_dynamic_field_json_to_struct(o + index * db_->size, fu, i, key, jval);
				// printf("%s %d\n",key, fidx);
				fidx++;
			}
		}
		index++;
		jval = jval->next;
	}
	sis_json_close(handle);
	return o;	
}

s_sis_sds sis_json_to_struct_sds(s_sis_dynamic_db *db_, s_sis_sds in_, s_sis_sds ago_)
{
	s_sis_json_handle *handle = sis_json_load(in_, sis_sdslen(in_));
	if (!handle)
	{
		return NULL;
	}
	// 取最后一条记录的数据作为底
	s_sis_sds o = NULL;
	if (ago_)
	{
		o = sis_sdsdup(ago_);
	}
	else
	{
		o = sis_sdsnewlen(NULL, db_->size);
	}
	int fnums = sis_map_list_getsize(db_->fields);
	char key[64];
	for (int k = 0; k < fnums; k++)
	{
		s_sis_dynamic_field *fu = (s_sis_dynamic_field *)sis_map_list_geti(db_->fields, k);
		for (int i = 0; i < fu->count; i++)
		{
			if (fu->count > 1)
			{
				sis_sprintf(key, 64, "%s%d", fu->fname, i);
				// printf("key = %s %d\n", key, i);
				sis_dynamic_field_json_to_struct(o, fu, i, key, handle->node);
			}
			else
			{
				sis_dynamic_field_json_to_struct(o, fu, i, fu->fname, handle->node);
			}			
		}
	}
	sis_json_close(handle);
	// printf("struct len = %d %d\n", collect_->sdb->size, sis_sdslen(o));
	return o;
}
///////////////////////////////////////
//  其他公用函数
///////////////////////////////////////
s_sis_sds sis_json_to_sds(s_sis_json_node *node_, bool iszip_)
{
	char *str = NULL;
	size_t olen;
	if (iszip_)
	{
		str = sis_json_output_zip(node_, &olen);
	}
	else
	{
		str = sis_json_output(node_, &olen);
	}
	s_sis_sds o = NULL;
	if (str)
	{
		o = sis_sdsnewlen(str, olen);
		sis_free(str);
	}
    return o;
}

int sis_json_merge_rpath(s_sis_json_node *node_, const char *rkey, const char *rpath_)
{
	const char *str = sis_json_get_str(node_, rkey);
	if (str)
	{
		s_sis_sds rpath = sis_sdsnew(rpath_);
		rpath = sis_sdscatfmt(rpath, "/%s", str);
		sis_json_object_set_string(node_, rkey, rpath, sis_sdslen(rpath));
		return 1;
	}
	// else
	{
		sis_json_object_add_string(node_, rkey, rpath_, sis_strlen(rpath_));
	}
	return 0;
}

// match_keys : * --> whole_keys
// match_keys : k,m1 | whole_keys : k1,k2,m1,m2 --> k1,k2,m1
s_sis_sds sis_match_key(s_sis_sds match_keys, s_sis_sds whole_keys)
{
	s_sis_sds o = NULL;
	if (!sis_strcasecmp(match_keys, "*"))
	{
		o = sis_sdsdup(whole_keys);
	}
	else
	{
        s_sis_string_list *slist = sis_string_list_create();
        sis_string_list_load(slist, whole_keys, sis_sdslen(whole_keys), ",");
        for (int i = 0; i < sis_string_list_getsize(slist); i++)
        {
            const char *key = sis_string_list_get(slist, i);
            int index = sis_str_subcmp(key, match_keys, ',');
		    if (index >= 0)
		    {
                if (o)
                {
                    o = sis_sdscatfmt(o, ",%s", key);
                }
                else
                {
                    o = sis_sdsnew(key);
                }
    		}
        }
        sis_string_list_destroy(slist);
	}
	return o;
}
s_sis_sds sis_match_sdb(s_sis_sds match_sdbs, s_sis_sds whole_sdbs)
{
	s_sis_sds o = NULL;
	s_sis_json_handle *injson = sis_json_load(whole_sdbs, sis_sdslen(whole_sdbs));
	if (!injson)
	{
		o = sis_sdsdup(match_sdbs);
	}
	else
	{
		s_sis_json_node *innode = sis_json_first_node(injson->node);
		while (innode)
		{
			if (!sis_strcasecmp(match_sdbs, "*") || sis_str_subcmp_strict(innode->key, match_sdbs, ',') >= 0)
			{
				if (!o)
				{
					o = sis_sdsnew(innode->key);
				}
				else
				{
					o = sis_sdscatfmt(o, ",%s", innode->key);
				}
			}
			innode = sis_json_next_node(innode);
		}
		sis_json_close(injson);
	}
	return o;
}
s_sis_sds sis_match_sdb_of_sds(s_sis_sds match_sdbs, s_sis_sds whole_sdbs)
{
	s_sis_sds o = NULL;
	if (!sis_strcasecmp(match_sdbs, "*"))
	{
		o = sis_sdsdup(whole_sdbs);
	}
	else
	{
        s_sis_json_handle *injson = sis_json_load(whole_sdbs, sis_sdslen(whole_sdbs));
		if (!injson)
		{
			o = sis_sdsdup(whole_sdbs);
		}
		else
		{
			s_sis_json_node *innode = sis_json_first_node(injson->node);
			s_sis_json_node *next = innode;
			while (innode)
			{
				int index = sis_str_subcmp_strict(innode->key, match_sdbs, ',');
				if (index < 0)
				{
					next = sis_json_next_node(innode);
					// 这里删除
					sis_json_delete_node(innode);
					innode = next;
				}
				else
				{
					innode = sis_json_next_node(innode);
				}
			}
			o = sis_json_to_sds(injson->node, 1);
			sis_json_close(injson);
		}
	}
	return o;
}
s_sis_sds sis_match_sdb_of_map(s_sis_sds match_sdbs, s_sis_map_list *whole_sdbs)
{
    s_sis_json_node *innode = sis_json_create_object();
    int count = sis_map_list_getsize(whole_sdbs);
    for (int i = 0; i < count; i++)
    {
        s_sis_dynamic_db *db = sis_map_list_geti(whole_sdbs, i);
		int index = sis_str_subcmp_strict(db->name, match_sdbs, ',');
		if (index < 0)
		{
			continue;
		}
		sis_json_object_add_node(innode, db->name, sis_sdbinfo_to_json(db));
    }
    s_sis_sds o = sis_json_to_sds(innode, 1);
	sis_json_delete_node(innode);
	return o;
}

int sis_get_map_keys(s_sis_sds keys_, s_sis_map_list *map_keys_)
{
	s_sis_string_list *klist = sis_string_list_create();
	sis_string_list_load(klist, keys_, sis_sdslen(keys_), ",");
	// 重新设置keys
	int count = sis_string_list_getsize(klist);
	for (int i = 0; i < count; i++)
	{
		s_sis_sds key = sis_sdsnew(sis_string_list_get(klist, i));
		sis_map_list_set(map_keys_, key, key);	
	}
	sis_string_list_destroy(klist);
	return sis_map_list_getsize(map_keys_);
}

int sis_get_map_sdbs(s_sis_sds sdbs_, s_sis_map_list *map_sdbs_)
{
	s_sis_json_handle *injson = sis_json_load(sdbs_, sis_sdslen(sdbs_));
	if (!injson)
	{
		return 0;
	}
	s_sis_json_node *innode = sis_json_first_node(injson->node);
	while (innode)
	{
		s_sis_dynamic_db *db = sis_dynamic_db_create(innode);
		if (db)
		{
			sis_map_list_set(map_sdbs_, db->name, db);
		}
		innode = sis_json_next_node(innode);
	}
	sis_json_close(injson);
	return sis_map_list_getsize(map_sdbs_);
}

s_sis_sds sis_map_as_keys(s_sis_map_list *map_keys_)
{
	int count = sis_map_list_getsize(map_keys_);
	s_sis_sds o = NULL;
	for (int i = 0; i < count; i++)
	{
		s_sis_sds key = sis_map_list_geti(map_keys_, i);
		if (o)
		{
			o = sis_sdscatfmt(o, ",%s", key);
		}
		else
		{
			o = sis_sdsnew(key);
		}
	}
	return o;
}

s_sis_sds sis_map_as_sdbs(s_sis_map_list *map_sdbs_)
{
    s_sis_json_node *innode = sis_json_create_object();
    int count = sis_map_list_getsize(map_sdbs_);
    for (int i = 0; i < count; i++)
    {
        s_sis_dynamic_db *db = sis_map_list_geti(map_sdbs_, i);
		sis_json_object_add_node(innode, db->name, sis_sdbinfo_to_json(db));
    }
    s_sis_sds o = sis_json_to_sds(innode, 1);
	sis_json_delete_node(innode);
	return o;
}