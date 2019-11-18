
#include "sis_dynamic.h"
#include <sis_sds.h>
#include <sis_math.h>

uint64 _sis_field_get_uint(s_sis_dynamic_unit *unit_, const char *val_, int index_)
{
	uint64  o = 0;
	uint8  *v8;
	uint16 *v16;
	uint32 *v32;
	uint64 *v64;

	switch (unit_->len)
	{
	case 1:
		v8 = (uint8 *)(val_ + unit_->offset + index_*sizeof(uint8));
		o = *v8;
		break;
	case 2:
		v16 = (uint16 *)(val_ + unit_->offset + index_*sizeof(uint16));
		o = *v16;
		break;
	case 4:
		v32 = (uint32 *)(val_ + unit_->offset + index_*sizeof(uint32));
		o = *v32;
		break;
	case 8:
		v64 = (uint64 *)(val_ + unit_->offset + index_*sizeof(uint64));
		o = *v64;
		break;
	default:
		break;
	}
	return o;
}
int64 _sis_field_get_int(s_sis_dynamic_unit *unit_, const char *val_, int index_)
{
	int64  o = 0;
	int8  *v8;
	int16 *v16;
	int32 *v32;
	int64 *v64;

	switch (unit_->len)
	{
	case 1:
		v8 = (int8 *)(val_ + unit_->offset + index_*sizeof(uint8));
		o = *v8;
		break;
	case 2:
		v16 = (int16 *)(val_ + unit_->offset + index_*sizeof(uint16));
		o = *v16;
		break;
	case 4:
		v32 = (int32 *)(val_ + unit_->offset + index_*sizeof(uint32));
		o = *v32;
		break;
	case 8:
		v64 = (int64 *)(val_ + unit_->offset + index_*sizeof(uint64));
		o = *v64;
		break;
	default:
		break;
	}
	return o;
}
double _sis_field_get_float(s_sis_dynamic_unit *unit_, const char *val_, int index_)
{
	double   o = 0.0;
	float32 *f32;
	float64 *f64;
	switch (unit_->len)
	{
	case 4:
		f32 = (float32 *)(val_ + unit_->offset + index_*sizeof(float32));
		o = (double)*f32;
		break;
	case 8:
		f64 = (float64 *)(val_ + unit_->offset + index_*sizeof(float64));
		o = (double)*f64;
		break;
	default:
		break;
	}
	return o;
}
void _sis_field_set_uint(s_sis_dynamic_unit *unit_, char *val_, uint64 v64_, int index_)
{
	uint8   v8 = 0;
	uint16 v16 = 0;
	uint32 v32 = 0;
	uint64 v64 = 0;

	switch (unit_->len)
	{
	case 1:
		v8 = (uint8)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint8), &v8, unit_->len);
		break;
	case 2:
		v16 = (uint16)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint16), &v16, unit_->len);
		break;
	case 4:
		v32 = (uint32)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint32), &v32, unit_->len);
		break;
	case 8:
		v64 = (uint64)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint64), &v64, unit_->len);
		break;
	default:
		break;
	}	
}

void _sis_field_set_int(s_sis_dynamic_unit *unit_, char *val_, int64 v64_, int index_)
{
	int8   v8 = 0;
	int16 v16 = 0;
	int32 v32 = 0;
	int64 v64 = 0;

	switch (unit_->len)
	{
	case 1:
		v8 = (int8)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint8), &v8, unit_->len);
		break;
	case 2:
		v16 = (int16)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint16), &v16, unit_->len);
		break;
	case 4:
		v32 = (int32)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint32), &v32, unit_->len);
		break;
	case 8:
		v64 = (int64)v64_;
		memmove(val_ + unit_->offset + index_*sizeof(uint64), &v64, unit_->len);
		break;
	default:
		break;
	}	
}

void _sis_field_set_float(s_sis_dynamic_unit *unit_, char *val_, double f64_, int index_)
{
	float32 f32 = 0.0;
	float64 f64 = 0.0;
	switch (unit_->len)
	{
	case 4:
		f32 = (float32)f64_;
		memmove(val_ + unit_->offset + index_*sizeof(float32), &f32, unit_->len);
		break;
	case 8:
	default:
		f64 = (float64)f64_;
		memmove(val_ + unit_->offset + index_*sizeof(float64), &f64, unit_->len);
		break;
	}

}

////////////////////////////////
//
///////////////////////////////

int _sis_dynamic_get_style(const char *str_)
{
	if(!sis_strcasecmp(str_,"int")||!sis_strcasecmp(str_,"I"))
	{
		return SIS_DYNAMIC_TYPE_INT;
	}
	if(!sis_strcasecmp(str_,"uint")||!sis_strcasecmp(str_,"U"))
	{
		return SIS_DYNAMIC_TYPE_UINT;
	}
	if(!sis_strcasecmp(str_,"float")||!sis_strcasecmp(str_,"F"))
	{
		return SIS_DYNAMIC_TYPE_FLOAT;
	}
	if(!sis_strcasecmp(str_,"char")||!sis_strcasecmp(str_,"C"))
	{
		return SIS_DYNAMIC_TYPE_CHAR;
	}
	return SIS_DYNAMIC_TYPE_NONE;
}

s_sis_dynamic_db *sis_dynamic_db_create(s_sis_json_node *node_)
{	
	s_sis_json_node *fields = sis_json_cmp_child_node(node_, "fields");
	if (!fields)
	{
		return NULL;	
	}

	s_sis_dynamic_db *dyna = (s_sis_dynamic_db *)sis_malloc(sizeof(s_sis_dynamic_db));
	memset(dyna, 0, sizeof(s_sis_dynamic_db));

	dyna->fields = sis_struct_list_create(sizeof(s_sis_dynamic_unit)); 

	int offset = 0;
	s_sis_dynamic_unit unit;
	// s_sis_json_node *node = sis_json_first_node(node_);

	s_sis_json_node *node = sis_json_first_node(fields);
	while (node)
	{
		const char *name  = node->key;
		if (!name) 
		{
			node = sis_json_next_node(node);
			continue;
		}
		int style = _sis_dynamic_get_style(sis_json_get_str(node, "0"));
		// printf("style: %s %s : %s %s %s -- %c\n", 
		// 	node->key, name,
		// 	sis_json_get_str(node, "0"), 
		// 	sis_json_get_str(node, "1"), 
		// 	sis_json_get_str(node, "2"),
		// 	style);
		if (style == SIS_DYNAMIC_TYPE_NONE)
		{
			node = sis_json_next_node(node);
			continue;			
		}
		int len = sis_json_get_int(node, "1", 4);
		if (len > SIS_DYNAMIC_FIELD_LIMIT || len < 1)
		{
			node = sis_json_next_node(node);
			continue;				
		}
		int count = sis_json_get_int(node, "2", 1);
		if (count > SIS_DYNAMIC_FIELD_LIMIT || count < 1)
		{
			node = sis_json_next_node(node);
			continue;				
		}
		int dot = sis_json_get_int(node, "3", 0);
		// 到此认为数据合法	
		memset(&unit, 0, sizeof(s_sis_dynamic_unit));
		sis_strcpy(unit.name,SIS_DYNAMIC_CHR_LEN,name);
		unit.style = style;
		unit.len = len;
		unit.dot = dot;
		unit.count = count;
		unit.offset = offset;

		// printf("%s : %c %d %d %d\n",name, unit.style,unit.len,unit.offset,unit.count);
		offset += unit.len*unit.count;
		sis_struct_list_push(dyna->fields, &unit);		

		node = sis_json_next_node(node);
	}

	// printf("offset --- %d\n",offset);
	if (offset == 0)
	{
		sis_struct_list_destroy(dyna->fields);
		sis_free(dyna);
		return NULL;
	} 
	dyna->size = offset; 
	printf("dyna->size --- %d\n",dyna->size);
	return dyna;
}

void sis_dynamic_db_destroy(s_sis_dynamic_db *db_)
{
	sis_struct_list_destroy(db_->fields);
	sis_free(db_);
}

s_sis_sds sis_dynamic_db_to_conf(s_sis_dynamic_db *db_, s_sis_sds in_)
{
	in_ = sis_sdscat(in_, "{fields:{");
	char sign[2];
	for(int i = 0; i < db_->fields->count; i++)
	{
		if (i > 0)
		{
			in_ = sis_sdscat(in_, ",");
		}
		s_sis_dynamic_unit *unit= sis_struct_list_get(db_->fields, i);
		in_ = sis_sdscatfmt(in_, "%s:[", unit->name);
		sign[0] = unit->style; sign[1] = 0;
		in_ = sis_sdscatfmt(in_, "%s", sign);
		in_ = sis_sdscatfmt(in_, ",%u", unit->len);
		if(unit->count > 1||unit->dot > 0)
		{
			in_ = sis_sdscatfmt(in_, ",%u", unit->count);
		}
		if(unit->dot > 0)
		{
			in_ = sis_sdscatfmt(in_, ",%u", unit->dot);
		}
		in_ = sis_sdscat(in_, "]");
	}
	in_ = sis_sdscat(in_, "}}");
	return in_;
}
s_sis_dynamic_unit *_sis_dynamic_get_field(s_sis_dynamic_db *db_,const char *name_)
{
	s_sis_dynamic_unit *inunit = (s_sis_dynamic_unit *)sis_struct_list_first(db_->fields);
	while(inunit)
	{			
		if (!strcasecmp(inunit->name, name_))
		{
			return inunit;
		}
		inunit = sis_struct_list_next(db_->fields, inunit);
	}
	return NULL;
}

// 类型和长度一样，数量可能不一样
void _sis_dynamic_unit_copy(void *inunit_,void *in_, void *out_)
{
	s_sis_dynamic_unit *inunit = (s_sis_dynamic_unit *)inunit_;
	s_sis_dynamic_unit *outunit = (s_sis_dynamic_unit *)inunit->map_unit;
	int count = sis_min(inunit->count, outunit->count);
	memmove((char *)out_+outunit->offset, (char *)in_+inunit->offset, inunit->len*count);

	printf("convert : %c %c %d %d\n", inunit->style, outunit->style, inunit->offset, outunit->offset);
	// if (outunit->count > count)	
	// {
	// 	memmove((char *)out_+outunit->offset + outunit->len*count, 
	// 		0, outunit->len*(outunit->count - count));
	// }
}	
// 类型一样，长度不同，数量可能不同
void _sis_dynamic_unit_convert(void *inunit_,void *in_, void *out_)
{
	s_sis_dynamic_unit *inunit = (s_sis_dynamic_unit *)inunit_;
	s_sis_dynamic_unit *outunit = (s_sis_dynamic_unit *)inunit->map_unit;
	int count = sis_min(inunit->count, outunit->count);
	
	int64   i64 = 0;
	uint64  u64 = 0;
	float64 f64 =0.0;
	for(int i = 0; i < count; i++)
	{
		switch (inunit->style)
		{
			case SIS_DYNAMIC_TYPE_INT:	
				{
					i64 = _sis_field_get_int(inunit, in_, i);
					// printf("convert : %c %c %d\n", inunit->style, outunit->style, (int)i64);
					switch (outunit->style)
					{
					case SIS_DYNAMIC_TYPE_INT:
						_sis_field_set_int(outunit, (char *)out_, i64, i);
						break;
					case SIS_DYNAMIC_TYPE_UINT:
						_sis_field_set_uint(outunit, (char *)out_, (uint64)i64, i);
						break;
					case SIS_DYNAMIC_TYPE_FLOAT:
						_sis_field_set_float(outunit, (char *)out_, (double)i64, i);
						break;
					case SIS_DYNAMIC_TYPE_CHAR:
						// 不支持
						break;
					}
				}
				break;
			case SIS_DYNAMIC_TYPE_UINT:
				{
					u64 = _sis_field_get_uint(inunit, in_, i);
					switch (outunit->style)
					{
					case SIS_DYNAMIC_TYPE_INT:
						_sis_field_set_int(outunit, (char *)out_, (int64)u64, i);
						break;
					case SIS_DYNAMIC_TYPE_UINT:
						_sis_field_set_uint(outunit, (char *)out_, u64, i);
						break;
					case SIS_DYNAMIC_TYPE_FLOAT:
						_sis_field_set_float(outunit, (char *)out_, (double)u64, i);
						break;
					case SIS_DYNAMIC_TYPE_CHAR:
						// 不支持
						break;
					}
				}
				break;
			case SIS_DYNAMIC_TYPE_FLOAT:
				{
					f64 = _sis_field_get_float(inunit, in_, i);
					switch (outunit->style)
					{
					case SIS_DYNAMIC_TYPE_INT:
						_sis_field_set_int(outunit, (char *)out_, (int64)f64, i);
						break;
					case SIS_DYNAMIC_TYPE_UINT:
						_sis_field_set_uint(outunit, (char *)out_, (uint64)f64, i);
						break;
					case SIS_DYNAMIC_TYPE_FLOAT:
						_sis_field_set_float(outunit, (char *)out_, (double)f64, i);
						break;
					case SIS_DYNAMIC_TYPE_CHAR:
						// 不支持
						break;
					}
				}
				break;
			case SIS_DYNAMIC_TYPE_CHAR:
			//  字符串只能和字符串互转
				if (outunit->style == SIS_DYNAMIC_TYPE_CHAR)
				{
					size_t len = sis_min(inunit->len, outunit->len);
					memmove((char *)out_+outunit->offset + i*outunit->len, 
							(char *)in_+inunit->offset + i*inunit->len, 
							len);
					// if(outunit->len < inunit->len)
					// {
					// 	//设置结束符
					// 	char *out = (char *)out_+outunit->offset + (i+1)*outunit->len - 1;
					// 	*out = 0;
					// }
				}
				break;
			default:
				break;
		}
	}
}	

void _sis_dynamic_method_copy(void *indb_, void *in_, size_t ilen_, void *out_, size_t olen_)
{
	printf("_sis_dynamic_method_copy ------ \n");
	memmove((char *)out_, (char *)in_, olen_);
}
void _sis_dynamic_method_owner(void *indb_, void *in_, size_t ilen_, void *out_, size_t olen_)
{
	printf("_sis_dynamic_method_owner ------ \n");
	s_sis_dynamic_db *indb = (s_sis_dynamic_db *)indb_;
	s_sis_dynamic_db *outdb = indb->map_db;
	memset(out_, 0, olen_);
	const char *inptr = (const char *)in_;
	char *outptr = (char *)out_;
	int count = ilen_/indb->size;
	for(int i = 0; i < count; i++)
	{
		s_sis_dynamic_unit *inunit = (s_sis_dynamic_unit *)sis_struct_list_first(indb->fields);
		while(inunit)
		{			
			if(inunit->shift)
			{
				printf("shift ------ \n");
				inunit->shift(inunit, (void *)inptr, (void *)outptr);
			}
			inunit = sis_struct_list_next(indb->fields, inunit);
		}
		inptr += indb->size;
		outptr += outdb->size;
	}
}

void _sis_dynamic_match(s_sis_dynamic_db *remote_, s_sis_dynamic_db *local_)
{
	remote_->method = NULL;
	remote_->map_db = local_;
	// 类型不同，就先转字符串，再转回去
	if (!local_)
	{
		// 表示远端有的数据集合，本地没有,直接返回，
		return ;
	}
	remote_->method = _sis_dynamic_method_copy;
	bool change = false;
	if(remote_->size!=local_->size||remote_->fields->count!=local_->fields->count)
	{
		change = true;
	}
	// 只要下面没有字段不一致，就需要赋值为 SIS_DYNAMIC_METHOD_OWNER
	s_sis_dynamic_unit *inunit = (s_sis_dynamic_unit *)sis_struct_list_first(remote_->fields);
	while(inunit)
	{		
		inunit->shift = NULL;	
		s_sis_dynamic_unit *outunit = _sis_dynamic_get_field(local_, inunit->name);
		if(!outunit)
		{
			change = true;
			inunit->map_unit = NULL;
		}
		else
		{
			inunit->map_unit = outunit;
			if (inunit->style != outunit->style)
			{
				change = true;
				inunit->shift = _sis_dynamic_unit_convert;
				printf("shift: convert \n");
			}
			else if (inunit->len != outunit->len)
			{
				// 类型一样 长度不同，数量可能不同
				change = true;
				inunit->shift = _sis_dynamic_unit_convert;
				printf("shift: convert \n");
			}
			else if (inunit->count != outunit->count)
			{
				change = true;
				inunit->shift = _sis_dynamic_unit_copy;
			}
			else
			{
				inunit->shift = _sis_dynamic_unit_copy;
				printf("shift: copy \n");
			}
		}
		printf("inunit: %s -> %s \n", inunit->name, inunit->map_unit->name);
		inunit = sis_struct_list_next(remote_->fields, inunit);
	}
	// printf("local: %d -> %d \n", local_->size, local_->map_db?local_->map_db->size:0);
	// printf("remote: %d -> %d \n", remote_->size, remote_->map_db?remote_->map_db->size:0);
	if (change)
	{
		remote_->method = _sis_dynamic_method_owner;

	}
}
s_sis_dynamic_class *_sis_dynamic_class_create(s_sis_json_node *innode_,s_sis_json_node *outnode_)
{
	s_sis_dynamic_class *class = (s_sis_dynamic_class *)sis_malloc(sizeof(s_sis_dynamic_class));
	memset(class, 0, sizeof(s_sis_dynamic_class));

	class->remote = sis_map_pointer_create(); 
	s_sis_json_node *innode = sis_json_first_node(innode_);
	while (innode)
	{
		s_sis_dynamic_db *dynamic = sis_dynamic_db_create(innode);
		if (!dynamic)
		{
			continue;
		}
		sis_map_pointer_set(class->remote, innode->key, dynamic);
		innode = sis_json_next_node(innode);
	}
	
	class->local = sis_map_pointer_create();
	s_sis_json_node *outnode = sis_json_first_node(outnode_);
	while (outnode)
	{
		s_sis_dynamic_db *dynamic = sis_dynamic_db_create(outnode);
		if (!dynamic)
		{
			continue;
		}
		sis_map_pointer_set(class->local, outnode->key, dynamic);
		outnode = sis_json_next_node(outnode);
	}

	////  下面开始处理关联性的信息  //////
	{
		s_sis_dict_entry *de;
		s_sis_dict_iter *di = sis_dict_get_iter(class->remote);
		while ((de = sis_dict_next(di)) != NULL)
		{
			s_sis_dynamic_db *remote = (s_sis_dynamic_db *)sis_dict_getval(de);
			s_sis_dynamic_db *local = sis_map_pointer_get(class->local, sis_dict_getkey(de));
			_sis_dynamic_match(remote, local);
		}
		sis_dict_iter_free(di);
	}
	{
		s_sis_dict_entry *de;
		s_sis_dict_iter *di = sis_dict_get_iter(class->local);
		while ((de = sis_dict_next(di)) != NULL)
		{
			s_sis_dynamic_db *local = (s_sis_dynamic_db *)sis_dict_getval(de);
			s_sis_dynamic_db *remote = sis_map_pointer_get(class->remote, sis_dict_getkey(de));
			_sis_dynamic_match(local, remote);
		}
		sis_dict_iter_free(di);
	}
	//////////////////////////////////

    return class;	
}

s_sis_dynamic_class *sis_dynamic_class_create_of_conf(
    const char *remote_,size_t rlen_,
    const char *local_,size_t llen_)
{
	s_sis_conf_handle *injson = sis_conf_load(remote_, rlen_);
	s_sis_conf_handle *outjson = sis_conf_load(local_, llen_);

	if (!injson||!outjson)
	{
		sis_conf_close(injson);
		sis_conf_close(outjson);
		return NULL;
	}
	s_sis_dynamic_class *class = _sis_dynamic_class_create(injson->node, outjson->node);

	sis_conf_close(injson);
	sis_conf_close(outjson);

	return class;
}
s_sis_dynamic_class *sis_dynamic_class_create_of_conf_file(
    const char *rfile_,const char *lfile_)
{
	s_sis_conf_handle *injson = sis_conf_open(rfile_);
	s_sis_conf_handle *outjson = sis_conf_open(lfile_);

	if (!injson||!outjson)
	{
		sis_conf_close(injson);
		sis_conf_close(outjson);
		return NULL;
	}
	s_sis_dynamic_class *class = _sis_dynamic_class_create(injson->node, outjson->node);

	sis_conf_close(injson);
	sis_conf_close(outjson);

	return class;	
}

// 收到server的结构体说明后，初始化一次，后面就不用再匹配了
s_sis_dynamic_class *sis_dynamic_class_create_of_json(
    const char *remote_,size_t rlen_,
    const char *local_,size_t llen_)
{
	s_sis_json_handle *injson = sis_json_load(remote_, rlen_);
	s_sis_json_handle *outjson = sis_json_load(local_, llen_);

	if (!injson||!outjson)
	{
		sis_json_close(injson);
		sis_json_close(outjson);
		return NULL;
	}

	s_sis_dynamic_class *class = _sis_dynamic_class_create(injson->node, outjson->node);

	sis_json_close(injson);
	sis_json_close(outjson);

	return class;
}
s_sis_dynamic_class *sis_dynamic_class_create_of_json_file(
    const char *rfile_,const char *lfile_)
{
	s_sis_json_handle *injson = sis_json_open(rfile_);
	s_sis_json_handle *outjson = sis_json_open(lfile_);

	if (!injson||!outjson)
	{
		sis_json_close(injson);
		sis_json_close(outjson);
		return NULL;
	}
	s_sis_dynamic_class *class = _sis_dynamic_class_create(injson->node, outjson->node);

	sis_json_close(injson);
	sis_json_close(outjson);

	return class;	
}
void sis_dynamic_class_destroy(s_sis_dynamic_class *cls_)
{
	if (!cls_)
	{
		return ;
	}
	{
		s_sis_dict_entry *de;
		s_sis_dict_iter *di = sis_dict_get_iter(cls_->remote);
		while ((de = sis_dict_next(di)) != NULL)
		{
			s_sis_dynamic_db *val = (s_sis_dynamic_db *)sis_dict_getval(de);
			sis_dynamic_db_destroy(val);
		}
		sis_dict_iter_free(di);
	}
	sis_map_pointer_destroy(cls_->remote);
	{
		s_sis_dict_entry *de;
		s_sis_dict_iter *di = sis_dict_get_iter(cls_->local);
		while ((de = sis_dict_next(di)) != NULL)
		{
			s_sis_dynamic_db *val = (s_sis_dynamic_db *)sis_dict_getval(de);
			sis_dynamic_db_destroy(val);
		}
		sis_dict_iter_free(di);
	}
	sis_map_pointer_destroy(cls_->local);
	sis_free(cls_);
	
}


// 返回0表示数据处理正常，解析数据要记得释放内存
int sis_dynamic_analysis(
		s_sis_dynamic_class *cls_, 
        const char *key_, int dir_,
        const char *in_, size_t ilen_,
        char *out_, size_t olen_)
{
	cls_->error = SIS_DYNAMIC_OK;
	if (!in_||!ilen_||!out_||!olen_)
	{
		cls_->error = SIS_DYNAMIC_ERR;
		goto error;
	}
	s_sis_dynamic_db *indb = NULL;
	if (dir_ == SIS_DYNAMIC_DIR_LTOR)
	{
		indb = sis_map_pointer_get(cls_->local, key_);
	}
	else
	{
		indb = sis_map_pointer_get(cls_->remote, key_);
	}
	if (!indb)
	{
		// 传入的数据在远端定义中没有发现
		cls_->error = SIS_DYNAMIC_NOKEY;
		goto error;
	}
	if (!indb->map_db)
	{
		// 传入的数据在没有对应的本地结构定义
		cls_->error = SIS_DYNAMIC_NOOPPO;
		goto error;
	}
	s_sis_dynamic_db *outdb = (s_sis_dynamic_db *)indb->map_db;

	int count = ilen_/indb->size;
	if (count*indb->size != ilen_)
	{
		// 来源数据尺寸不匹配
		cls_->error = SIS_DYNAMIC_SIZE;
		goto error;
	}	
	if (count*outdb->size != olen_)
	{
		cls_->error = SIS_DYNAMIC_LEN;
		goto error;		
	}
	if (!indb->method)
	{
		// 本地端版本滞后，无法解析远端的数据格式
		cls_->error = SIS_DYNAMIC_NOOPPO;
		goto error;
	}
	indb->method(indb, (void *)in_, ilen_, out_, olen_);
	printf("command : %d * %d = %d  --> %d\n", count , indb->size, (int)ilen_, outdb->size);

error:
	return cls_->error;
}

// 远程结构转本地结构
size_t sis_dynamic_analysis_length(
		s_sis_dynamic_class *cls_, 
        const char *key_, int dir_,
        const char *in_, size_t ilen_)
{
	cls_->error = SIS_DYNAMIC_OK;
	if (!in_||!ilen_)
	{
		cls_->error = SIS_DYNAMIC_ERR;
		goto error;
	}
	s_sis_dynamic_db *indb = NULL;
	if (dir_ == SIS_DYNAMIC_DIR_LTOR)
	{
		indb = sis_map_pointer_get(cls_->local, key_);
	}
	else
	{
		indb = sis_map_pointer_get(cls_->remote, key_);
	}
	if (!indb)
	{
		// 传入的数据在远端定义中没有发现
		cls_->error = SIS_DYNAMIC_NOKEY;
		goto error;
	}
	if (!indb->map_db)
	{
		// 传入的数据在没有对应的本地结构定义
		cls_->error = SIS_DYNAMIC_NOOPPO;
		goto error;
	}
	
	int count = ilen_/indb->size;
	// printf("%d * %d = %d  --> %d\n", count , indb->size, (int)ilen_, indb->map_db->size);

	if (count*indb->size != ilen_)
	{
		// 来源数据尺寸不匹配
		cls_->error = SIS_DYNAMIC_SIZE;
		goto error;
	}	
	return count*indb->map_db->size;
error:
	return 0;
}
// 本地数据转远程结构
size_t sis_dynamic_analysis_ltor_length(
		s_sis_dynamic_class *cls_, 
        const char *key_,
        const char *in_, size_t ilen_)
{
	return sis_dynamic_analysis_length(cls_, key_, SIS_DYNAMIC_DIR_LTOR, in_, ilen_);
}

int sis_dynamic_analysis_ltor(
		s_sis_dynamic_class *cls_, 
        const char *key_,
        const char *in_, size_t ilen_,
        char *out_, size_t olen_)
{
	return sis_dynamic_analysis(cls_, key_, SIS_DYNAMIC_DIR_LTOR, in_, ilen_, out_, olen_);
}

// 远程结构转本地结构
size_t sis_dynamic_analysis_rtol_length(
		s_sis_dynamic_class *cls_, 
        const char *key_,
        const char *in_, size_t ilen_)
{
	return sis_dynamic_analysis_length(cls_, key_, SIS_DYNAMIC_DIR_RTOL, in_, ilen_);
}

int sis_dynamic_analysis_rtol(
		s_sis_dynamic_class *cls_, 
        const char *key_,
        const char *in_, size_t ilen_,
        char *out_, size_t olen_)
{
	return sis_dynamic_analysis(cls_, key_, SIS_DYNAMIC_DIR_RTOL, in_, ilen_, out_, olen_);
}
// void f2d( float f , double *x )
// {
//     uint32 a , b;
//     uint32 uf = *(uint32*)&f;
//     uint32*ux = (uint32*)x;
     
//     ux[0] = ux[1] = 0;
//     ux[1] |= uf&0x80000000;
     
//     a = (uf&0x7f800000)>>23;
//     b = uf&0x7fffff;
//     a += 1024 - 128;
//     ux[1] |= a<<20;
//     ux[1] |= b>>3 ;
//     ux[0] |= b<<29;    
// }

static s_sis_json_node *_sis_dynamic_struct_to_array(s_sis_dynamic_db *indb_, const char *val_)
{
	s_sis_json_node *o = sis_json_create_array();
	for (int i = 0; i < indb_->fields->count; i++)
	{
		s_sis_dynamic_unit *inunit = (s_sis_dynamic_unit *)sis_struct_list_get(indb_->fields, i);
		const char *ptr = (const char *)val_;
		for(int index = 0; index < inunit->count; index++)
		{
			switch (inunit->style)
			{
			case SIS_DYNAMIC_TYPE_INT:
				sis_json_array_add_int(o, _sis_field_get_int(inunit, ptr, index));
				break;
			case SIS_DYNAMIC_TYPE_UINT:
				sis_json_array_add_uint(o, _sis_field_get_uint(inunit, ptr, index));
				break;
			case SIS_DYNAMIC_TYPE_FLOAT:
				// if (inunit->len == 4)
				// {

				// 	float f32 = (float)_sis_field_get_float(inunit, ptr, index);
				// 	printf("f32=%.2f  %.2f\n",f32, (double)f32);
				// 	sis_json_array_add_double(o, f32, 2);
				// }
				// else
				{
					LOG(0)("dot:%d\n", inunit->dot);
					sis_json_array_add_double(o, _sis_field_get_float(inunit, ptr, index), inunit->dot);
				}
				break;
			case SIS_DYNAMIC_TYPE_CHAR:
				sis_json_array_add_string(o, ptr + inunit->offset + index*inunit->len, inunit->len);
				break;
			default:
				sis_json_array_add_string(o, " ", 1);
				break;
			}
		}
	}
	return o;
}
// 调用该函数，需要释放内存
s_sis_sds sis_dynamic_struct_to_json(
		s_sis_dynamic_class *cls_, 
        const char *key_, int dir_,
        const char *in_, size_t ilen_)
{
	s_sis_sds o = NULL;

	cls_->error = SIS_DYNAMIC_OK;
	if (!in_||!ilen_)
	{
		cls_->error = SIS_DYNAMIC_ERR;
		goto error;
	}
	s_sis_dynamic_db *indb = NULL;
	if (dir_ == SIS_DYNAMIC_DIR_LTOR)
	{
		indb = sis_map_pointer_get(cls_->local, key_);
	}
	else
	{
		indb = sis_map_pointer_get(cls_->remote, key_);
	}
	if (!indb)
	{
		// 传入的数据在远端定义中没有发现
		cls_->error = SIS_DYNAMIC_NOKEY;
		goto error;
	}
	
	s_sis_json_node *jone = sis_json_create_object();
	s_sis_json_node *jtwo = sis_json_create_array();
	s_sis_json_node *jfields = sis_json_create_object();

	// 先处理字段
	char sign[2];
	for (int i = 0; i < indb->fields->count; i++)
	{
		s_sis_dynamic_unit *inunit = (s_sis_dynamic_unit *)sis_struct_list_get(indb->fields, i);
		s_sis_json_node *jfield = sis_json_create_array();
		// sis_json_array_add_uint(jfield, i);
		sign[0] = inunit->style; sign[1] = 0;
		sis_json_array_add_string(jfield, sign, 1);
		sis_json_array_add_uint(jfield, inunit->len);
		sis_json_array_add_uint(jfield, inunit->count);
		sis_json_array_add_uint(jfield, inunit->dot);
		sis_json_object_add_node(jfields, inunit->name, jfield);
	}
	sis_json_object_add_node(jone, "fields", jfields);

	int count = (int)(ilen_ / indb->size);

	// printf("%d * %d = %d\n", count , indb->size, (int)ilen_);

	const char *val = in_;
	for (int k = 0; k < count; k++)
	{
		s_sis_json_node *jval = _sis_dynamic_struct_to_array(indb, val);
		sis_json_array_add_node(jtwo, jval);
		val += indb->size;
	}
	sis_json_object_add_node(jone, "values", jtwo);
	// size_t ll;
	// printf("jone = %s\n", sis_json_output(jone, &ll));
	// 输出数据
	// printf("1112111 [%d]\n",tb->control.limits);
	char *str;
	size_t olen;
	str = sis_json_output(jone, &olen);
	o = sis_sdsnewlen(str, olen);
	sis_free(str);
	sis_json_delete_node(jone);
error:
	return o;

}
s_sis_sds sis_dynamic_db_to_array_sds(s_sis_dynamic_db *db_, void *in_, size_t ilen_)
{
	s_sis_dynamic_db *indb = db_;

	s_sis_json_node *jone = sis_json_create_object();
	s_sis_json_node *jtwo = sis_json_create_array();

	int count = (int)(ilen_ / indb->size);

	// printf("%d * %d = %d\n", count , indb->size, (int)ilen_);

	const char *val = (const char *)in_;
	for (int k = 0; k < count; k++)
	{
		s_sis_json_node *jval = _sis_dynamic_struct_to_array(indb, val);
		sis_json_array_add_node(jtwo, jval);
		val += indb->size;
	}
	sis_json_object_add_node(jone, "values", jtwo);
	s_sis_sds o = NULL;
	char *str;
	size_t olen;
	str = sis_json_output(jone, &olen);
	o = sis_sdsnewlen(str, olen);
	sis_free(str);
	sis_json_delete_node(jone);	
	return o;
}
s_sis_sds sis_dynamic_conf_to_array_sds(const char *dbstr_, void *in_, size_t ilen_)
{
	s_sis_conf_handle *injson = sis_conf_load(dbstr_, strlen(dbstr_));
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

	return sis_dynamic_db_to_array_sds(indb, in_, ilen_);

}

#if 0
#pragma pack(push, 1)
typedef struct _local_info {
	int open;
	double close;
	int ask[5];
	char name[4];
} _local_info;

typedef struct _remote_info {
	uint32 open;
	float close;
	int ask[10];
	char name[8];
} _remote_info;

#pragma pack(pop)
int main()
{
	// const char *local = "{\"info\":{\"open\":[0,\"I\",4]}}";
	// const char *local = "{\"info\":{\"open\":[\"I\",4],\"close\":[\"F\",8,1,2],\"ask\":[\"I\",4,5],\"name\":[\"C\",4]}}";
	// const char *remote = "{\"info\":{\"open\":[\"U\",4],\"close\":[\"F\",4,1,3],\"ask\":[\"I\",4,10],\"name\":[\"C\",8]}}";
	const char *local = "{stock:{fields:{open:[I,4]}},info:{fields:{open:[I,4],close:[F,8,1,2],ask:[I,4,5],name:[C,4]}}}";
	const char *remote = "{info:{fields:{open:[U,4],close:[F,4,1,3],ask:[I,4,10],name:[C,8]}}}";
	const char *alone = "{fields:{open:[U,4],close:[F,4,1,3],ask:[I,4,10],name:[C,8]}}";
	// s_sis_dynamic_class *class = sis_dynamic_class_create_of_conf_file("remote.conf","local.conf");

	{
		int count = 2;
		_remote_info in_info[count];
		memset(&in_info, 0,sizeof(_remote_info)*count);
		in_info[0].open = 1800;
		in_info[1].open = 2000;
		in_info[0].close = 1500;
		in_info[1].close = 2100;

		s_sis_sds out = sis_dynamic_conf_to_array_sds(alone, &in_info, sizeof(_remote_info)*count);
		printf("alone = \n%s\n",out);
		sis_sdsfree(out);
	}	
	return 0;

	s_sis_dynamic_class *class = sis_dynamic_class_create_of_conf(remote,strlen(remote),local,strlen(local));
	// s_sis_dynamic_class *class = sis_dynamic_class_create_of_json(remote,strlen(remote),local,strlen(local));
	// 测试 remote 到 local 转换
	{
		s_sis_sds out = sis_sdsempty();
		s_sis_dynamic_db *db = (s_sis_dynamic_db *)sis_map_pointer_get(class->local, "info");
		out = sis_dynamic_db_to_conf(db, out);
		printf("out =  %s\n", out);
		sis_sdsfree(out);
	}
	{
		int count = 2;
		_remote_info in_info[count];
		in_info[0].open = 0xFFFFFFFF;
		in_info[1].open = 2000;
		in_info[0].close = 1500;
		in_info[1].close = 0xFFFFFFFF;

		for(size_t i = 0; i < 10; i++)
		{
			in_info[0].ask[i] = 100 + i;
			in_info[1].ask[i] = 200 + i; 
		}
		sprintf(in_info[0].name,"100+1");
		sprintf(in_info[1].name,"100+123");

		size_t ilen = count * sizeof(_remote_info);
		size_t olen = sis_dynamic_analysis_rtol_length(class, "info", (const char *)&in_info[0], ilen);

		if (olen < 1)
		{
			printf("error: %d\n", class->error);
		}
		else
		{
			char *out = (char *)sis_malloc(olen);
			sis_dynamic_analysis_rtol(class, "info", (const char *)&in_info[0], ilen, out, olen);
			printf("error: %d  olen=%d\n", class->error, (int)olen);

			s_sis_sds in_str = sis_dynamic_struct_to_json(class, "info", SIS_DYNAMIC_DIR_RTOL, (const char *)&in_info[0], ilen);
			printf("remote: \n%s\n", in_str);
			sis_sdsfree(in_str);
			s_sis_sds out_str = sis_dynamic_struct_to_json(class, "info", SIS_DYNAMIC_DIR_LTOR, (const char *)out, olen);
			printf("local: \n%s\n", out_str);
			sis_sdsfree(out_str);
			sis_free(out);
		}
	}
	// 测试local 到remote 转换
	{
		int count = 2;
		_local_info in_info[count];
		in_info[0].open = 1111;
		in_info[1].open = 2222;
		in_info[0].close = 0xFFFFFFFFFFFFFFFF;
		in_info[1].close = 4444;
		for(size_t i = 0; i < 5; i++)
		{
			in_info[0].ask[i] = 100 + i;
			in_info[1].ask[i] = 200 + i; 
		}
		sprintf(in_info[0].name,"123");
		sprintf(in_info[1].name,"2345");

		size_t ilen = count * sizeof(_local_info);
		size_t olen = sis_dynamic_analysis_ltor_length(class, "info", (const char *)&in_info[0], ilen);

		if (olen < 1)
		{
			printf("error: %d ilen= %d\n", class->error, (int)ilen);
		}
		else
		{
			char *out = (char *)sis_malloc(olen);
			sis_dynamic_analysis_ltor(class, "info", (const char *)&in_info[0], ilen, out, olen);
			printf("error: %d  olen=%d\n", class->error, (int)olen);

			s_sis_sds in_str = sis_dynamic_struct_to_json(class, "info", SIS_DYNAMIC_DIR_LTOR, (const char *)&in_info[0], ilen);
			printf("remote: \n%s\n", in_str);
			sis_sdsfree(in_str);
			s_sis_sds out_str = sis_dynamic_struct_to_json(class, "info", SIS_DYNAMIC_DIR_RTOL, (const char *)out, olen);
			printf("local: \n%s\n", out_str);
			sis_sdsfree(out_str);
			sis_free(out);
		}
	}
	sis_dynamic_class_destroy(class);
	return 0;
}

#endif

#if 0

// ########
typedef void _method_define();

typedef struct s_method {
	int id;
	void(*command)();
}s_method;

void json_demo1()
{
	printf("-1-\n");
}
void json_demo2()
{
	printf("-2-\n");
}
// #define REGISTER_API(name) 
//     moduleRegisterApi("RedisModule_" #name, (void *)(long)json_ ## name)

#define REGISTER_API(a,name) { a->command = json_##name; }
// #define REGISTER_API_CHR(a,name) { a->command = "json_" #name ; }

#define REGISTER_API_STR(a,name) { a->command = json_##name; }

int main()
{
	s_method *method = sis_malloc(sizeof(s_method));
	memset(method, 0, sizeof(s_method));
	method->id = 100;

	void *ptr = method;
	// method->command = json_ ## name;
	REGISTER_API(method,demo1);
	method->command();

	REGISTER_API(method,demo2);
	method->command();

	REGISTER_API_STR(method,"demo2");
	method->command();
	// REGISTER_API_CHR(method,"demo1");
	// method->command();

	// REGISTER_API_CHR(method,"demo2");
	// method->command();

	printf(" 1 method = %p, ptr= %p\n", method, ptr);
	sis_free(method);
	printf(" 2 method = %p, ptr= %p\n", method, ptr);
	// &method = (s_method *)NULL;
	printf(" 3 method = %p, ptr= %p\n", method, ptr);
	return 0;
}
#endif
