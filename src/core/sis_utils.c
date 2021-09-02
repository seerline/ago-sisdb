
#include "sis_utils.h"

/////////////////////////////////////////////////
// 对 s_sis_dynamic_db 信息的提取和转换
/////////////////////////////////////////////////

s_sis_sds sis_dynamic_db_to_array_sds(s_sis_dynamic_db *db_, const char *key_, void *in_, size_t ilen_)
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
	// printf("to array : fnum = %d count = %d \n", fnums, count);
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
	// printf("to array : fnum = %d count = %d \n", fnums, count);
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

s_sis_sds sis_dynamic_db_to_csv_sds(s_sis_dynamic_db *db_, void *in_, size_t ilen_)
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

s_sis_sds sis_dynamic_dbinfo_to_conf(s_sis_dynamic_db *db_, s_sis_sds in_)
{
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

s_sis_json_node *sis_dynamic_dbinfo_to_json(s_sis_dynamic_db *db_)
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

s_sis_sds sis_dynamic_conf_to_array_sds(const char *confstr_, void *in_, size_t ilen_)
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
	s_sis_sds o = sis_dynamic_db_to_array_sds(indb, indb->name, in_, ilen_);
	sis_dynamic_db_destroy(indb);
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
	s_sis_sds o = sis_sdsnewlen(str, olen);
	sis_free(str);
    return o;
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
		sis_json_object_add_node(innode, db->name, sis_dynamic_dbinfo_to_json(db));
    }
    s_sis_sds o = sis_json_to_sds(innode, 1);
	sis_json_delete_node(innode);
	return o;
}
