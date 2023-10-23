
#include "sis_dynamic.h"
#include <sis_sds.h>
#include <sis_math.h>
#include <sis_time.h>



////////////////////////////////
//
///////////////////////////////

int _sis_dynamic_get_style(const char *str_)
{
	if(!sis_strcasecmp(str_,"int")||!sis_strcasecmp(str_,"I"))
	{
		return SIS_DYNAMIC_TYPE_INT;
	}
	if(!sis_strcasecmp(str_,"wsec")||!sis_strcasecmp(str_,"W"))
	{
		return SIS_DYNAMIC_TYPE_WSEC;
	}
	if(!sis_strcasecmp(str_,"msec")||!sis_strcasecmp(str_,"T"))
	{
		return SIS_DYNAMIC_TYPE_MSEC;
	}
	if(!sis_strcasecmp(str_,"second")||!sis_strcasecmp(str_,"S"))
	{
		return SIS_DYNAMIC_TYPE_TSEC;
	}
	if(!sis_strcasecmp(str_,"minute")||!sis_strcasecmp(str_,"M"))
	{
		return SIS_DYNAMIC_TYPE_MINU;
	}
	if(!sis_strcasecmp(str_,"date")||!sis_strcasecmp(str_,"D"))
	{
		return SIS_DYNAMIC_TYPE_DATE;
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
	if(!sis_strcasecmp(str_,"price")||!sis_strcasecmp(str_,"P"))
	{
		return SIS_DYNAMIC_TYPE_PRICE;
	}
	return SIS_DYNAMIC_TYPE_NONE;
}
////////////////////////////////////////////////////////////////
// s_sis_dynamic_field
////////////////////////////////////////////////////////////////
s_sis_dynamic_field *sis_dynamic_field_create(const char *name_)
{
	s_sis_dynamic_field *o = SIS_MALLOC(s_sis_dynamic_field, o);
	sis_strcpy(o->fname, 32, name_);
	return o;
}
void sis_dynamic_field_destroy(void *field_)
{
	s_sis_dynamic_field *field = (s_sis_dynamic_field *)field_;
	sis_free(field);
}
s_sis_dynamic_field *sis_dynamic_db_add_field(s_sis_dynamic_db *db_, const char *fname_, int style, int ilen, int count, int dot)
{
	s_sis_dynamic_field *field = sis_map_list_get(db_->fields, fname_);
	if (!field)
	{
		field = sis_dynamic_field_create(fname_);
		field->style  = style;      
		field->len    = ilen;       
		field->count  = count;	
		field->dot    = dot;     // 输出为字符串时保留的小数点
		field->mindex = 0;       // 该字段是否为主索引
		field->solely = 0;       // 该字段是否为唯一值
		field->offset = db_->size;
		db_->size += field->count * field->len;
		sis_map_list_set(db_->fields, fname_, field);
		return field;
	}
	return NULL;
}

int sis_dynamic_field_scale(int style_)
{
	int o = 1;
	switch (style_)
	{
	case SIS_DYNAMIC_TYPE_WSEC: o = 864000000;
		break;
	case SIS_DYNAMIC_TYPE_MSEC: o = 86400000;
		break;
	case SIS_DYNAMIC_TYPE_TSEC:  o = 86400;
		break;
	case SIS_DYNAMIC_TYPE_MINU: o = 1440;
		break;	
	default: // SIS_DYNAMIC_TYPE_DATE
		o = 1;
		break;
	}
	return o;
}

////////////////////////////////////////////////////////////////
// s_sis_dynamic_db
////////////////////////////////////////////////////////////////
s_sis_dynamic_db *sis_dynamic_db_create(s_sis_json_node *node_)
{	
	s_sis_json_node *fields = sis_json_cmp_child_node(node_, "fields");
	if (!fields)
	{
		LOG(8)("no fields.\n");
		return NULL;	
	}
	
	s_sis_dynamic_db *dyna = SIS_MALLOC(s_sis_dynamic_db, dyna);
	dyna->name = sis_sdsnew(node_->key);

	dyna->fields = sis_map_list_create(sis_dynamic_field_destroy); 
	dyna->field_solely = sis_pointer_list_create();

	int offset = 0;

	s_sis_json_node *node = sis_json_first_node(fields);
	while (node)
	{
		char *name  = node->key;
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
		if (style == SIS_DYNAMIC_TYPE_PRICE && dot == 0)
		{
			dot = 3;
		}
		const char *flag = sis_json_get_str(node, "4");

		// 到此认为数据合法	
		s_sis_dynamic_field *unit = sis_dynamic_field_create(name);
		unit->style = style;
		unit->len = len;
		unit->dot = dot;
		unit->count = count;
		unit->mindex = (flag && (strchr(flag, 'I') || strchr(flag, 'i'))) ? 1 : 0;
		unit->solely = (flag && (strchr(flag, 'O') || strchr(flag, 'o'))) ? 1 : 0;
		unit->offset = offset;

		// printf("%s : %c %d %d %d\n",name, unit->style,unit->len,unit->offset,unit->count);
		offset += unit->len * unit->count;
		sis_map_list_set(dyna->fields, name, unit);		

		if (!dyna->field_time &&
		 	(unit->style == SIS_DYNAMIC_TYPE_TSEC || 
			 unit->style == SIS_DYNAMIC_TYPE_MSEC || 
			 unit->style == SIS_DYNAMIC_TYPE_MINU ||
			 unit->style == SIS_DYNAMIC_TYPE_DATE))
		{
			dyna->field_time = unit;
		}
		if (unit->mindex)
		{
			dyna->field_mindex = unit;
		}
		if (unit->solely)
		{
			sis_pointer_list_push(dyna->field_solely, unit);			
		}
		node = sis_json_next_node(node);
	}
	if (offset == 0)
	{
		// LOG(8)("offset == 0.\n");
		sis_map_list_destroy(dyna->fields);
		sis_free(dyna);
		return NULL;
	} 
	dyna->size = offset; 
	// if have time field, than must set main index is it.
	if (dyna->field_time)
	{
		dyna->field_mindex = dyna->field_time;
	}
	LOG(5)("dyna->size ---%s %d %p\n", dyna->name, dyna->size, dyna);
	dyna->refs = 1;
	return dyna;
}
s_sis_dynamic_db *sis_dynamic_db_create_none(const char *name_, size_t size_)
{	
	s_sis_dynamic_db *dyna = SIS_MALLOC(s_sis_dynamic_db, dyna);
	dyna->name = sis_sdsnew(name_);
	dyna->size = size_; 
	dyna->refs = 1;
	return dyna;
}

void sis_dynamic_db_decr(s_sis_dynamic_db *db_)
{
	if (db_->refs == 1)
	{
		sis_sdsfree(db_->name);
		if (db_->fields)
		{
			sis_map_list_destroy(db_->fields);
		}
		if (db_->field_solely)
		{
			sis_pointer_list_destroy(db_->field_solely);
		}
		sis_free(db_);
	}
	else
	{
		db_->refs--;
	}
}
void sis_dynamic_db_setname(s_sis_dynamic_db *db_, const char *name_)
{
	sis_sdsfree(db_->name);
	db_->name = sis_sdsnew(name_);
}

void sis_dynamic_db_incr(s_sis_dynamic_db *db_)
{
	// printf("++++ %s %d %p\n", db_->name, db_->refs, db_);
	db_->refs++;
}
void sis_dynamic_db_destroy(void *db)
{
	// s_sis_dynamic_db *db_ = (s_sis_dynamic_db *)db;
	// printf("--- %s %d %p\n", db_->name, db_->refs, db_);
	sis_dynamic_db_decr((s_sis_dynamic_db *)db);
}

s_sis_dynamic_field *sis_dynamic_db_get_field(s_sis_dynamic_db *db_, int *index_, const char *field_)
{
	s_sis_dynamic_field *field = sis_map_list_get(db_->fields, field_);
	if (!index_ || field)
	{
		return field;
	}
    char argv[2][128]; 
    int cmds = sis_str_divide(field_, '.', argv[0], argv[1]);
	field = sis_map_list_get(db_->fields, argv[0]);
	if (cmds != 2 || !field)
	{
		return NULL;
	}
	*index_ = (int)sis_atoll(argv[1]);
	return field;
}

bool sis_dynamic_dbinfo_same(s_sis_dynamic_db *db1_, s_sis_dynamic_db *db2_)
{
	int fnums = sis_map_list_getsize(db1_->fields);
	// printf("%s %s : %d %d\n", db1_->name, db2_->name, fnums, sis_map_list_getsize(db2_->fields));
	if(fnums != sis_map_list_getsize(db2_->fields))	
	{
		return false;
	}
	for (int i = 0; i < fnums; i++)
	{
		s_sis_dynamic_field *unit1 = (s_sis_dynamic_field *)sis_map_list_geti(db1_->fields, i);
		s_sis_dynamic_field *unit2 = (s_sis_dynamic_field *)sis_map_list_geti(db2_->fields, i);
		if(!unit1||!unit2) 
		{
			return false;
		}
		// sis_out_binary("1", unit1, sizeof(s_sis_dynamic_field));
		// sis_out_binary("2", unit2, sizeof(s_sis_dynamic_field));
		if (memcmp(unit1, unit2, sizeof(s_sis_dynamic_field)))
		{
			return false;
		}
	}
	return true;
}

msec_t sis_dynamic_db_get_time(s_sis_dynamic_db *db_, int index_, void *in_, size_t ilen_)
{
	if (!db_->field_time || !in_ || ilen_ % db_->size || ( index_ + 1 ) * db_->size > ilen_)
	{
		return 0;
	}
	return _sis_field_get_uint(db_->field_time, (const char*)in_ + index_ * db_->size, 0);
}
uint64 sis_dynamic_db_get_mindex(s_sis_dynamic_db *db_, int index_, void *in_, size_t ilen_)
{
	if (!db_->field_mindex || !in_ || ilen_ % db_->size || ( index_ + 1 ) * db_->size > ilen_)
	{
		return 0;
	}
	return _sis_field_get_uint(db_->field_mindex, (const char*)in_ + index_ * db_->size, 0);
}

///////////////////////////////
// 转换对象定义
///////////////////////////////

// 类型和长度一样，数量可能不一样
void _sis_dynamic_unit_copy(void *inunit_, void *in_, void *out_)
{
	s_sis_dynamic_field *inunit = ((s_sis_dynamic_field_convert *)inunit_)->in_field;
	s_sis_dynamic_field *outunit = ((s_sis_dynamic_field_convert *)inunit_)->out_field;
	int count = sis_min(inunit->count, outunit->count);
	memmove((char *)out_ + outunit->offset, (char *)in_ + inunit->offset, inunit->len * count);

	// printf("convert : %c %c %d %d\n", inunit->style, outunit->style, inunit->offset, outunit->offset);
	// if (outunit->count > count)	
	// {
	// 	memmove((char *)out_+outunit->offset + outunit->len*count, 
	// 		0, outunit->len*(outunit->count - count));
	// }
}	
uint64 sis_time_unit_convert(int instyle, int outstyle, uint64 in64)
{
	uint64 u64 = in64;
	switch (instyle)
	{
	case SIS_DYNAMIC_TYPE_TSEC:
		switch (outstyle)
		{
			case SIS_DYNAMIC_TYPE_MSEC: u64 = in64 * 1000 + 999; break;
			case SIS_DYNAMIC_TYPE_MINU: u64 = in64 / 60;   break;
			case SIS_DYNAMIC_TYPE_DATE: u64 = sis_time_get_idate(in64);   break;
			case SIS_DYNAMIC_TYPE_YEAR: u64 = sis_time_get_idate(in64) / 10000;   break;
		}
		break;
	case SIS_DYNAMIC_TYPE_MSEC:
		switch (outstyle)
		{
			case SIS_DYNAMIC_TYPE_TSEC: u64 = in64 / 1000; break;
			case SIS_DYNAMIC_TYPE_MINU: u64 = in64 / 1000 / 60;   break;
			case SIS_DYNAMIC_TYPE_DATE: u64 = sis_time_get_idate(in64 / 1000);   break;
			case SIS_DYNAMIC_TYPE_YEAR: u64 = sis_time_get_idate(in64 / 1000) / 10000;   break;
		}
		break;
	case SIS_DYNAMIC_TYPE_MINU:
		switch (outstyle)
		{
			case SIS_DYNAMIC_TYPE_MSEC: u64 = in64 * 60 * 1000 + 59999; break;
			case SIS_DYNAMIC_TYPE_TSEC: u64 = in64 * 60; break;
			case SIS_DYNAMIC_TYPE_DATE: u64 = sis_time_get_idate(in64 * 60);   break;
			case SIS_DYNAMIC_TYPE_YEAR: u64 = sis_time_get_idate(in64 * 60) / 10000;   break;
		}
		break;
	case SIS_DYNAMIC_TYPE_DATE:
		switch (outstyle)
		{
			case SIS_DYNAMIC_TYPE_MSEC: u64 = sis_time_make_time(in64, 235959) * 1000 + 999; break;
			case SIS_DYNAMIC_TYPE_TSEC:  u64 = sis_time_make_time(in64, 235959); break;
			case SIS_DYNAMIC_TYPE_MINU: u64 = sis_time_make_time(in64, 235959) / 60;   break;
			case SIS_DYNAMIC_TYPE_YEAR: u64 = in64 / 10000;   break;
		}
		break;
	}
	return u64;
}
// 类型一样，长度不同，数量可能不同
void _sis_dynamic_unit_convert(void *inunit_,void *in_, void *out_)
{
	s_sis_dynamic_field *inunit = ((s_sis_dynamic_field_convert *)inunit_)->in_field;
	s_sis_dynamic_field *outunit = ((s_sis_dynamic_field_convert *)inunit_)->out_field;
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
					case SIS_DYNAMIC_TYPE_PRICE:
						_sis_field_set_int(outunit, (char *)out_, i64, i);
						break;
					case SIS_DYNAMIC_TYPE_TSEC:
					case SIS_DYNAMIC_TYPE_MSEC:
					case SIS_DYNAMIC_TYPE_MINU:
					case SIS_DYNAMIC_TYPE_DATE:
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
			case SIS_DYNAMIC_TYPE_TSEC:
			case SIS_DYNAMIC_TYPE_MSEC:
			case SIS_DYNAMIC_TYPE_MINU:
			case SIS_DYNAMIC_TYPE_DATE:
			case SIS_DYNAMIC_TYPE_UINT:
				{
					u64 = _sis_field_get_uint(inunit, in_, i);
					switch (outunit->style)
					{
					case SIS_DYNAMIC_TYPE_INT:
						_sis_field_set_int(outunit, (char *)out_, (int64)u64, i);
						break;
					case SIS_DYNAMIC_TYPE_PRICE:
						_sis_field_set_int(outunit, (char *)out_, (int64)u64, i);
						break;
					case SIS_DYNAMIC_TYPE_TSEC:
					case SIS_DYNAMIC_TYPE_MSEC:
					case SIS_DYNAMIC_TYPE_MINU:
					case SIS_DYNAMIC_TYPE_DATE:
						u64 = sis_time_unit_convert(inunit->style, outunit->style, u64);
						_sis_field_set_uint(outunit, (char *)out_, u64, i);
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
					case SIS_DYNAMIC_TYPE_TSEC:
					case SIS_DYNAMIC_TYPE_MSEC:
					case SIS_DYNAMIC_TYPE_MINU:
					case SIS_DYNAMIC_TYPE_DATE:
					case SIS_DYNAMIC_TYPE_UINT:
						_sis_field_set_uint(outunit, (char *)out_, (uint64)f64, i);
						break;
					case SIS_DYNAMIC_TYPE_FLOAT:
						_sis_field_set_float(outunit, (char *)out_, (double)f64, i);
						break;
					case SIS_DYNAMIC_TYPE_PRICE:
						// 这里特殊处理一下
						_sis_field_set_price(outunit, (char *)out_, (double)f64, inunit->dot, 1, i);
						break;
					case SIS_DYNAMIC_TYPE_CHAR:
						// 不支持
						break;
					}
				}
				break;
			case SIS_DYNAMIC_TYPE_PRICE:
				{
					f64 = _sis_field_get_price(inunit, in_, i);
					switch (outunit->style)
					{
					case SIS_DYNAMIC_TYPE_INT:
						_sis_field_set_int(outunit, (char *)out_, (int64)f64, i);
						break;
					case SIS_DYNAMIC_TYPE_TSEC:
					case SIS_DYNAMIC_TYPE_MSEC:
					case SIS_DYNAMIC_TYPE_MINU:
					case SIS_DYNAMIC_TYPE_DATE:
					case SIS_DYNAMIC_TYPE_UINT:
						_sis_field_set_uint(outunit, (char *)out_, (uint64)f64, i);
						break;
					case SIS_DYNAMIC_TYPE_FLOAT:
						_sis_field_set_float(outunit, (char *)out_, (double)f64, i);
						break;
					case SIS_DYNAMIC_TYPE_PRICE:
						// 这里特殊处理一下
						{
							int dot = _sis_field_get_price_dot(inunit, in_, i);
							int valid = _sis_field_get_price_valid(inunit, in_, i);
							_sis_field_set_price(outunit, (char *)out_, (double)f64, dot, valid, i);
						}
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

void _sis_dynamic_method_copy(void *convert_, void *in_, size_t ilen_, void *out_, size_t olen_)
{
	// printf("_sis_dynamic_method_copy ------ \n");
	memmove((char *)out_, (char *)in_, olen_);
}
void _sis_dynamic_method_owner(void *convert_, void *in_, size_t ilen_, void *out_, size_t olen_)
{
	// printf("_sis_dynamic_method_owner ------ \n");
	s_sis_dynamic_convert *convert = (s_sis_dynamic_convert *)convert_;
	s_sis_dynamic_db *indb = convert->in;
	s_sis_dynamic_db *outdb = convert->out;
	memset(out_, 0, olen_);
	const char *inptr = (const char *)in_;
	char *outptr = (char *)out_;
	int count = ilen_/indb->size;

	int fnums = sis_map_list_getsize(convert->map_fields);
	for(int i = 0; i < count; i++)
	{
		for(int k = 0; k < fnums; k++)
		{
			s_sis_dynamic_field_convert *curunit = (s_sis_dynamic_field_convert *)sis_map_list_geti(convert->map_fields, k);
			if(curunit && curunit->cb_shift)
			{
				// printf("shift ------ \n");
				curunit->cb_shift(curunit, (void *)inptr, (void *)outptr);
			}
		}
		inptr += indb->size;
		outptr += outdb->size;
	}
}


////////////////////////////////////////////////////
// 单个db转换为其他结构 传入需要转换的db
////////////////////////////////////////////////////

void _sis_dynamic_match(s_sis_dynamic_convert *convert_)
{
	convert_->cb_convert = _sis_dynamic_method_copy;
	bool change = false;

	s_sis_dynamic_db *in = convert_->in;
	s_sis_dynamic_db *out = convert_->out;
	int rfnums = sis_map_list_getsize(out->fields);
	if( out->size != in->size || rfnums != sis_map_list_getsize(in->fields) )
	{
		change = true;
	}
	// 只要下面没有字段不一致，就需要赋值为 SIS_DYNAMIC_METHOD_OWNER
	for (int i = 0; i < rfnums; i++)
	{
		s_sis_dynamic_field *outunit = (s_sis_dynamic_field *)sis_map_list_geti(out->fields, i);
		s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_get(in->fields, outunit->fname);

		s_sis_dynamic_field_convert *curunit = SIS_MALLOC(s_sis_dynamic_field_convert, curunit);
		curunit->out_field = outunit;
		curunit->in_field = inunit;
		curunit->cb_shift = NULL;
		sis_map_list_set(convert_->map_fields, outunit->fname, curunit);

		if(inunit)
		{
			if (inunit->style != outunit->style)
			{
				change = true;
				curunit->cb_shift = _sis_dynamic_unit_convert;
				// printf("shift: convert \n");
			}
			else if (inunit->len != outunit->len)
			{
				// 类型一样 长度不同，数量可能不同
				change = true;
				curunit->cb_shift = _sis_dynamic_unit_convert;
				// printf("shift: convert \n");
			}
			else if (inunit->count != outunit->count)
			{
				change = true;
				curunit->cb_shift = _sis_dynamic_unit_copy;
			}
			else
			{
				curunit->cb_shift = _sis_dynamic_unit_copy;
				// printf("shift: copy \n");
			}
		}
		// printf("inunit: %s -> %s \n", inunit->name, outunit->name);
	}
	if (change)
	{
		convert_->cb_convert = _sis_dynamic_method_owner;
	}
}

s_sis_dynamic_convert *sis_dynamic_convert_create(s_sis_dynamic_db *in_, s_sis_dynamic_db *out_)
{
	s_sis_dynamic_convert *o = SIS_MALLOC(s_sis_dynamic_convert, o);
	o->map_fields = sis_map_list_create(sis_free_call);
	o->in = in_;
	o->out = out_;
	_sis_dynamic_match(o);
	return o;
}

void sis_dynamic_convert_destroy(void *convert_)
{
	s_sis_dynamic_convert *convert = (s_sis_dynamic_convert *)convert_;
	sis_map_list_destroy(convert->map_fields);
	sis_free(convert);
}

size_t sis_dynamic_convert_length(s_sis_dynamic_convert *cls_, const char *in_, size_t ilen_)
{
	cls_->error = SIS_DYNAMIC_OK;
	if (!in_||!ilen_)
	{
		cls_->error = SIS_DYNAMIC_ERR;
		goto error;
	}
	int count = ilen_/cls_->in->size;
	if (count * cls_->in->size != ilen_)
	{
		// 来源数据尺寸不匹配
		cls_->error = SIS_DYNAMIC_SIZE;
		goto error;
	}	
	return count * cls_->out->size;
error:
	return 0;
}
// 返回0表示数据处理正常，解析数据要记得释放内存
int sis_dynamic_convert(s_sis_dynamic_convert *cls_, 
		const char *in_, size_t ilen_, char *out_, size_t olen_)
{
	cls_->error = SIS_DYNAMIC_OK;
	if (!in_ || !ilen_ || !out_ || !olen_)
	{
		cls_->error = SIS_DYNAMIC_ERR;
		goto error;
	}
	s_sis_dynamic_db *indb = cls_->in;
	s_sis_dynamic_db *outdb = cls_->out;

	int count = ilen_ / indb->size;
	if (count * indb->size != ilen_)
	{
		// 来源数据尺寸不匹配
		cls_->error = SIS_DYNAMIC_SIZE;
		goto error;
	}
	if (count * outdb->size != olen_)
	{
		cls_->error = SIS_DYNAMIC_LEN;
		goto error;
	}
	if (!cls_->cb_convert)
	{
		// 本地端版本滞后，无法解析远端的数据格式
		cls_->error = SIS_DYNAMIC_NOOPPO;
		goto error;
	}
	cls_->cb_convert(cls_, (void *)in_, ilen_, out_, olen_);
	// printf("command : %d * %d = %d  --> %d\n", count , indb->size, (int)ilen_, outdb->size);

error:
	return cls_->error;
}

// s_sis_dynamic_class *_sis_dynamic_class_create(s_sis_json_node *innode_,s_sis_json_node *outnode_)
// {
// 	s_sis_dynamic_class *class = (s_sis_dynamic_class *)sis_malloc(sizeof(s_sis_dynamic_class));
// 	memset(class, 0, sizeof(s_sis_dynamic_class));

// 	class->remote = sis_map_pointer_create(); 
// 	s_sis_json_node *innode = sis_json_first_node(innode_);
// 	while (innode)
// 	{
// 		s_sis_dynamic_db *dynamic = sis_dynamic_db_create(innode);
// 		if (!dynamic)
// 		{
// 			continue;
// 		}
// 		sis_map_pointer_set(class->remote, innode->key, dynamic);
// 		innode = sis_json_next_node(innode);
// 	}
	
// 	class->local = sis_map_pointer_create();
// 	s_sis_json_node *outnode = sis_json_first_node(outnode_);
// 	while (outnode)
// 	{
// 		s_sis_dynamic_db *dynamic = sis_dynamic_db_create(outnode);
// 		if (!dynamic)
// 		{
// 			continue;
// 		}
// 		sis_map_pointer_set(class->local, outnode->key, dynamic);
// 		outnode = sis_json_next_node(outnode);
// 	}

// 	////  下面开始处理关联性的信息  //////
// 	{
// 		s_sis_dict_entry *de;
// 		s_sis_dict_iter *di = sis_dict_get_iter(class->remote);
// 		while ((de = sis_dict_next(di)) != NULL)
// 		{
// 			s_sis_dynamic_db *remote = (s_sis_dynamic_db *)sis_dict_getval(de);
// 			s_sis_dynamic_db *local = sis_map_pointer_get(class->local, sis_dict_getkey(de));
// 			_sis_dynamic_match(remote, local);
// 		}
// 		sis_dict_iter_free(di);
// 	}
// 	{
// 		s_sis_dict_entry *de;
// 		s_sis_dict_iter *di = sis_dict_get_iter(class->local);
// 		while ((de = sis_dict_next(di)) != NULL)
// 		{
// 			s_sis_dynamic_db *local = (s_sis_dynamic_db *)sis_dict_getval(de);
// 			s_sis_dynamic_db *remote = sis_map_pointer_get(class->remote, sis_dict_getkey(de));
// 			_sis_dynamic_match(local, remote);
// 		}
// 		sis_dict_iter_free(di);
// 	}
// 	//////////////////////////////////

//     return class;	
// }

// s_sis_dynamic_class *sis_dynamic_class_create_of_conf(
//     const char *remote_,size_t rlen_,
//     const char *local_,size_t llen_)
// {
// 	s_sis_conf_handle *injson = sis_conf_load(remote_, rlen_);
// 	s_sis_conf_handle *outjson = sis_conf_load(local_, llen_);

// 	if (!injson||!outjson)
// 	{
// 		sis_conf_close(injson);
// 		sis_conf_close(outjson);
// 		return NULL;
// 	}
// 	s_sis_dynamic_class *class = _sis_dynamic_class_create(injson->node, outjson->node);

// 	sis_conf_close(injson);
// 	sis_conf_close(outjson);

// 	return class;
// }
// s_sis_dynamic_class *sis_dynamic_class_create_of_conf_file(
//     const char *rfile_,const char *lfile_)
// {
// 	s_sis_conf_handle *injson = sis_conf_open(rfile_);
// 	s_sis_conf_handle *outjson = sis_conf_open(lfile_);

// 	if (!injson||!outjson)
// 	{
// 		sis_conf_close(injson);
// 		sis_conf_close(outjson);
// 		return NULL;
// 	}
// 	s_sis_dynamic_class *class = _sis_dynamic_class_create(injson->node, outjson->node);

// 	sis_conf_close(injson);
// 	sis_conf_close(outjson);

// 	return class;	
// }

// // 收到server的结构体说明后，初始化一次，后面就不用再匹配了
// s_sis_dynamic_class *sis_dynamic_class_create_of_json(
//     const char *remote_,size_t rlen_,
//     const char *local_,size_t llen_)
// {
// 	s_sis_json_handle *injson = sis_json_load(remote_, rlen_);
// 	s_sis_json_handle *outjson = sis_json_load(local_, llen_);

// 	if (!injson||!outjson)
// 	{
// 		sis_json_close(injson);
// 		sis_json_close(outjson);
// 		return NULL;
// 	}

// 	s_sis_dynamic_class *class = _sis_dynamic_class_create(injson->node, outjson->node);

// 	sis_json_close(injson);
// 	sis_json_close(outjson);

// 	return class;
// }
// s_sis_dynamic_class *sis_dynamic_class_create_of_json_file(
//     const char *rfile_,const char *lfile_)
// {
// 	s_sis_json_handle *injson = sis_json_open(rfile_);
// 	s_sis_json_handle *outjson = sis_json_open(lfile_);

// 	if (!injson||!outjson)
// 	{
// 		sis_json_close(injson);
// 		sis_json_close(outjson);
// 		return NULL;
// 	}
// 	s_sis_dynamic_class *class = _sis_dynamic_class_create(injson->node, outjson->node);

// 	sis_json_close(injson);
// 	sis_json_close(outjson);

// 	return class;	
// }
// void sis_dynamic_class_destroy(s_sis_dynamic_class *cls_)
// {
// 	if (!cls_)
// 	{
// 		return ;
// 	}
// 	{
// 		s_sis_dict_entry *de;
// 		s_sis_dict_iter *di = sis_dict_get_iter(cls_->remote);
// 		while ((de = sis_dict_next(di)) != NULL)
// 		{
// 			s_sis_dynamic_db *val = (s_sis_dynamic_db *)sis_dict_getval(de);
// 			sis_dynamic_db_destroy(val);
// 		}
// 		sis_dict_iter_free(di);
// 	}
// 	sis_map_pointer_destroy(cls_->remote);
// 	{
// 		s_sis_dict_entry *de;
// 		s_sis_dict_iter *di = sis_dict_get_iter(cls_->local);
// 		while ((de = sis_dict_next(di)) != NULL)
// 		{
// 			s_sis_dynamic_db *val = (s_sis_dynamic_db *)sis_dict_getval(de);
// 			sis_dynamic_db_destroy(val);
// 		}
// 		sis_dict_iter_free(di);
// 	}
// 	sis_map_pointer_destroy(cls_->local);
// 	sis_free(cls_);
	
// }


// // 返回0表示数据处理正常，解析数据要记得释放内存
// int sis_dynamic_analysis(
// 		s_sis_dynamic_class *cls_, 
//         const char *key_, int dir_,
//         const char *in_, size_t ilen_,
//         char *out_, size_t olen_)
// {
// 	cls_->error = SIS_DYNAMIC_OK;
// 	if (!in_||!ilen_||!out_||!olen_)
// 	{
// 		cls_->error = SIS_DYNAMIC_ERR;
// 		goto error;
// 	}
// 	s_sis_dynamic_db *indb = NULL;
// 	if (dir_ == SIS_DYNAMIC_DIR_LTOR)
// 	{
// 		indb = sis_map_pointer_get(cls_->local, key_);
// 	}
// 	else
// 	{
// 		indb = sis_map_pointer_get(cls_->remote, key_);
// 	}
// 	if (!indb)
// 	{
// 		// 传入的数据在远端定义中没有发现
// 		cls_->error = SIS_DYNAMIC_NOKEY;
// 		goto error;
// 	}
// 	if (!indb->map_db)
// 	{
// 		// 传入的数据在没有对应的本地结构定义
// 		cls_->error = SIS_DYNAMIC_NOOPPO;
// 		goto error;
// 	}
// 	s_sis_dynamic_db *outdb = (s_sis_dynamic_db *)indb->map_db;

// 	int count = ilen_/indb->size;
// 	if (count*indb->size != ilen_)
// 	{
// 		// 来源数据尺寸不匹配
// 		cls_->error = SIS_DYNAMIC_SIZE;
// 		goto error;
// 	}	
// 	if (count*outdb->size != olen_)
// 	{
// 		cls_->error = SIS_DYNAMIC_LEN;
// 		goto error;		
// 	}
// 	if (!indb->method)
// 	{
// 		// 本地端版本滞后，无法解析远端的数据格式
// 		cls_->error = SIS_DYNAMIC_NOOPPO;
// 		goto error;
// 	}
// 	indb->method(indb, (void *)in_, ilen_, out_, olen_);
// 	// printf("command : %d * %d = %d  --> %d\n", count , indb->size, (int)ilen_, outdb->size);

// error:
// 	return cls_->error;
// }

// // 远程结构转本地结构
// size_t sis_dynamic_analysis_length(
// 		s_sis_dynamic_class *cls_, 
//         const char *key_, int dir_,
//         const char *in_, size_t ilen_)
// {
// 	cls_->error = SIS_DYNAMIC_OK;
// 	if (!in_||!ilen_)
// 	{
// 		cls_->error = SIS_DYNAMIC_ERR;
// 		goto error;
// 	}
// 	s_sis_dynamic_db *indb = NULL;
// 	if (dir_ == SIS_DYNAMIC_DIR_LTOR)
// 	{
// 		indb = sis_map_pointer_get(cls_->local, key_);
// 	}
// 	else
// 	{
// 		indb = sis_map_pointer_get(cls_->remote, key_);
// 	}
// 	if (!indb)
// 	{
// 		// 传入的数据在远端定义中没有发现
// 		cls_->error = SIS_DYNAMIC_NOKEY;
// 		goto error;
// 	}
// 	if (!indb->map_db)
// 	{
// 		// 传入的数据在没有对应的本地结构定义
// 		cls_->error = SIS_DYNAMIC_NOOPPO;
// 		goto error;
// 	}
	
// 	int count = ilen_/indb->size;
// 	// printf("%d * %d = %d  --> %d\n", count , indb->size, (int)ilen_, indb->map_db->size);

// 	if (count*indb->size != ilen_)
// 	{
// 		// 来源数据尺寸不匹配
// 		cls_->error = SIS_DYNAMIC_SIZE;
// 		goto error;
// 	}	
// 	return count*indb->map_db->size;
// error:
// 	return 0;
// }
// // 本地数据转远程结构
// size_t sis_dynamic_analysis_ltor_length(
// 		s_sis_dynamic_class *cls_, 
//         const char *key_,
//         const char *in_, size_t ilen_)
// {
// 	return sis_dynamic_analysis_length(cls_, key_, SIS_DYNAMIC_DIR_LTOR, in_, ilen_);
// }

// int sis_dynamic_analysis_ltor(
// 		s_sis_dynamic_class *cls_, 
//         const char *key_,
//         const char *in_, size_t ilen_,
//         char *out_, size_t olen_)
// {
// 	return sis_dynamic_analysis(cls_, key_, SIS_DYNAMIC_DIR_LTOR, in_, ilen_, out_, olen_);
// }

// // 远程结构转本地结构
// size_t sis_dynamic_analysis_rtol_length(
// 		s_sis_dynamic_class *cls_, 
//         const char *key_,
//         const char *in_, size_t ilen_)
// {
// 	return sis_dynamic_analysis_length(cls_, key_, SIS_DYNAMIC_DIR_RTOL, in_, ilen_);
// }

// int sis_dynamic_analysis_rtol(
// 		s_sis_dynamic_class *cls_, 
//         const char *key_,
//         const char *in_, size_t ilen_,
//         char *out_, size_t olen_)
// {
// 	return sis_dynamic_analysis(cls_, key_, SIS_DYNAMIC_DIR_RTOL, in_, ilen_, out_, olen_);
// }
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


// // 调用该函数，需要释放内存
// s_sis_sds sis_dynamic_struct_to_json(
// 		s_sis_dynamic_class *cls_, 
//         const char *key_, int dir_,
//         const char *in_, size_t ilen_)
// {
// 	s_sis_sds o = NULL;

// 	cls_->error = SIS_DYNAMIC_OK;
// 	if (!in_||!ilen_)
// 	{
// 		cls_->error = SIS_DYNAMIC_ERR;
// 		goto error;
// 	}
// 	s_sis_dynamic_db *indb = NULL;
// 	if (dir_ == SIS_DYNAMIC_DIR_LTOR)
// 	{
// 		indb = sis_map_pointer_get(cls_->local, key_);
// 	}
// 	else
// 	{
// 		indb = sis_map_pointer_get(cls_->remote, key_);
// 	}
// 	if (!indb)
// 	{
// 		// 传入的数据在远端定义中没有发现
// 		cls_->error = SIS_DYNAMIC_NOKEY;
// 		goto error;
// 	}
	
// 	s_sis_json_node *jone = sis_json_create_object();
// 	s_sis_json_node *jtwo = sis_json_create_array();
// 	s_sis_json_node *jfields = sis_json_create_object();

// 	// 先处理字段
// 	char sign[2];
// 	int fnums = sis_map_list_getsize(indb->fields);
// 	for (int i = 0; i < fnums; i++)
// 	{
// 		s_sis_dynamic_field *inunit = (s_sis_dynamic_field *)sis_map_list_geti(indb->fields, i);
// 		if(!inunit) continue;
// 		s_sis_json_node *jfield = sis_json_create_array();
// 		// sis_json_array_add_uint(jfield, i);
// 		sign[0] = inunit->style; sign[1] = 0;
// 		sis_json_array_add_string(jfield, sign, 1);
// 		sis_json_array_add_uint(jfield, inunit->len);
// 		sis_json_array_add_uint(jfield, inunit->count);
// 		sis_json_array_add_uint(jfield, inunit->dot);
// 		sis_json_object_add_node(jfields, inunit->name, jfield);
// 	}
// 	sis_json_object_add_node(jone, "fields", jfields);

// 	int count = (int)(ilen_ / indb->size);

// 	// printf("%d * %d = %d\n", count , indb->size, (int)ilen_);

// 	const char *val = in_;
// 	for (int k = 0; k < count; k++)
// 	{
// 		s_sis_json_node *jval = sis_dynamic_struct_to_array(indb, val);
// 		sis_json_array_add_node(jtwo, jval);
// 		val += indb->size;
// 	}
// 	sis_json_object_add_node(jone, "values", jtwo);
// 	// size_t ll;
// 	// printf("jone = %s\n", sis_json_output(jone, &ll));
// 	// 输出数据
// 	// printf("1112111 [%d]\n",tb->control.limits);
// 	char *str;
// 	size_t olen;
// 	str = sis_json_output(jone, &olen);
// 	o = sis_sdsnewlen(str, olen);
// 	sis_free(str);
// 	sis_json_delete_node(jone);
// error:
// 	return o;

// }

#if 0
#pragma pack(push,1)
typedef struct _local_info {
	int open;
	double close;
	int ask[5];
	char name[4];
} _local_info;

typedef struct _remote_info {
	uint64 open;
	float close;
	int ask[10];
	char name[8];
	int agop;
} _remote_info;

#pragma pack(pop)
int main()
{
	char argv[2][32];
    sis_str_divide("123.234.456.789", '.', argv[0], argv[1]);
	printf("%s %s\n", argv[0], argv[1]);

	const char *test_indb = "{stock:{fields:{open:[I,4],close:[F,8,1,3],ask:[I,4,5],name:[C,4]}}}";
	const char *test_outdb = "{stock:{fields:{open:[U,8],close:[F,4,1,2],ask:[I,4,10],name:[C,8],agop:[I,4]}}}";

	s_sis_conf_handle *injson = sis_conf_load(test_indb, strlen(test_indb));
	s_sis_dynamic_db *indb = sis_dynamic_db_create(sis_json_first_node(injson->node));
	sis_conf_close(injson);

	s_sis_conf_handle *outjson = sis_conf_load(test_outdb, strlen(test_outdb));
	s_sis_dynamic_db *outdb = sis_dynamic_db_create(sis_json_first_node(outjson->node));
	sis_conf_close(outjson);

	int count = 2;
	_local_info in_info[count];
	in_info[0].open = 1111;
	in_info[1].open = 2222;
	in_info[0].close = 0xFFFFFFFFFFFFFFFF;
	in_info[1].close = 4444;
	for (size_t i = 0; i < 5; i++)
	{
		in_info[0].ask[i] = 100 + i;
		in_info[1].ask[i] = 200 + i;
	}
	sprintf(in_info[0].name, "123");
	sprintf(in_info[1].name, "2345");

	s_sis_sds in = sis_sdb_to_array_sds(indb, "stock", in_info, count*sizeof(_local_info));
	printf("indb = \n%s\n",in);
	sis_sdsfree(in);	

	s_sis_dynamic_convert *convert = sis_dynamic_convert_create(indb, outdb);
	size_t size = sis_dynamic_convert_length(convert, (const char *)in_info, count*sizeof(_local_info)); 
	char *out_info = sis_malloc(size + 1);
	sis_dynamic_convert(convert, (const char *)in_info, count*sizeof(_local_info), out_info, size);

	s_sis_sds out = sis_sdb_to_array_sds(outdb, "stock", out_info, size);
	printf("outdb = \n%s\n",out);
	sis_sdsfree(out);	

	sis_free(out_info);
	sis_dynamic_convert_destroy(convert);

	sis_dynamic_db_destroy(indb);
	sis_dynamic_db_destroy(outdb);
	return 0;
}

#endif


