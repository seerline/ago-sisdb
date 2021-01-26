
#include "sis_db.h"

int sis_db_get_format(s_sis_sds argv_)
{
    int iformat = SISDB_FORMAT_CHARS;
    s_sis_json_handle *handle = argv_ ? sis_json_load(argv_, sis_sdslen(argv_)) : NULL;
    if (handle)
    {
        iformat = sis_db_get_format_from_node(handle->node, SISDB_FORMAT_JSON);
        sis_json_close(handle);
    }
    return iformat;
}

int sis_db_get_format_from_node(s_sis_json_node *node_, int default_)
{
	int o = default_;
	s_sis_json_node *node = sis_json_cmp_child_node(node_, "format");
	if (node && sis_strlen(node->value) > 0)
	{
        char ch = node->value[0];
        switch (ch)
        {
        case 'z':  // bitzip "zip"
        case 'Z':
            o = SISDB_FORMAT_BITZIP;
            break;
        case 's':  // struct 
        case 'S':
            o = SISDB_FORMAT_STRUCT;
            break;
        case 'b':  // bytes
        case 'B':
            // o = SISDB_FORMAT_BYTES;
            o = SISDB_FORMAT_BUFFER;
            break;
        case 'j':
        case 'J':
            o = SISDB_FORMAT_JSON;
            break;
        case 'a':
        case 'A':
            o = SISDB_FORMAT_ARRAY;
            break;
        case 'c':
        case 'C':
            o = SISDB_FORMAT_CSV;
            break;        
        default:
            o = SISDB_FORMAT_STRING;
            break;
        }    
	}
	return o;
}


s_sis_sds sis_db_struct_to_json_sds(s_sis_dynamic_db *db_, const char *in_, size_t ilen_, const char *key_,int isfields_)
{	
	int fnums = sis_map_list_getsize(db_->fields);
	s_sis_json_node *jone = sis_json_create_object();
	if (isfields_)
	{
		s_sis_json_node *jfields = sis_json_create_object();
		int index = 0;
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_geti(db_->fields, i);
			if (inunit->count > 1)
			{
				for (int k = 0; k < inunit->count; k++)
				{
					char fname[64];
					sis_sprintf(fname, 64, "%s%d", inunit->name, k);
					sis_json_object_add_uint(jfields, fname, index++);
				}
			}
			else
			{
				sis_json_object_add_uint(jfields, inunit->name, index++);
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
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_geti(db_->fields, i);
			sis_dynamic_field_to_array(jval, inunit, val);
		}
		sis_json_array_add_node(jtwo, jval);
		val += db_->size;
	}
	sis_json_object_add_node(jone, key_? key_ : "values", jtwo);

	s_sis_sds o = sis_json_to_sds(jone, true);

	sis_json_delete_node(jone);	
	return o;
}

s_sis_sds sis_db_struct_to_array_sds(s_sis_dynamic_db *db_, const char *in_, size_t ilen_)
{
	int fnums = sis_map_list_getsize(db_->fields);
	s_sis_json_node *jone = sis_json_create_array();

	const char *val = in_;
	int count = (int)(ilen_ / db_->size);
	for (int k = 0; k < count; k++)
	{
		s_sis_json_node *jval = sis_json_create_array();
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_geti(db_->fields, i);
			sis_dynamic_field_to_array(jval, inunit, val);
		}
		sis_json_array_add_node(jone, jval);
		val += db_->size;
	}
	s_sis_sds o = sis_json_to_sds(jone, true);
	sis_json_delete_node(jone);	
	return o;
}

s_sis_sds sis_db_struct_to_csv_sds(s_sis_dynamic_db *db_, const char *in_, size_t ilen_, int isfields_)
{
	int fnums = sis_map_list_getsize(db_->fields);
	s_sis_sds o = sis_sdsempty();
	if (isfields_)
	{
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_geti(db_->fields, i);
			if (inunit->count > 1)
			{
				for (int k = 0; k < inunit->count; k++)
				{
					char fname[64];
					sis_sprintf(fname, 64, "%s%d", inunit->name, k);
					o = sis_csv_make_str(o, fname, sis_strlen(fname));
				}
			}
			else
			{
				o = sis_csv_make_str(o, inunit->name, sis_sdslen(inunit->name));
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
			s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_geti(db_->fields, i);
			o = sis_dynamic_field_to_csv(o, inunit, val);			
		}
		o = sis_csv_make_end(o);
		val += db_->size;
	}
	return o;
}

s_sis_sds sis_db_format_sds(s_sis_dynamic_db *db_, const char *key_, int iformat_, const char *in_, size_t ilen_, int isfields_)
{
	s_sis_sds other = NULL;
	switch (iformat_)
	{
	case SISDB_FORMAT_JSON:
		other = sis_db_struct_to_json_sds(db_, in_, ilen_, key_, isfields_);
		break;
	case SISDB_FORMAT_ARRAY:
		other = sis_db_struct_to_array_sds(db_, in_, ilen_);
		break;
	case SISDB_FORMAT_CSV:
		other = sis_db_struct_to_csv_sds(db_, in_, ilen_, isfields_);
		break;
	}
	return other;
}