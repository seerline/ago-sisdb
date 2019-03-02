
#include "sis_dynamic.h"
#include <sis_sds.h>
#include <sis_json.h>

// uint64 sisdb_field_get_uint(s_sisdb_field *unit_, const char *val_)
// {
// 	if(!sisdb_field_is_integer(unit_)||!val_) return 0;

// 	uint64  o = 0;
// 	uint8  *v8;
// 	uint16 *v16;
// 	uint32 *v32;
// 	uint64 *v64;

// 	const char *ptr = val_;

// 	switch (unit_->flags.len)
// 	{
// 	case 1:
// 		v8 = (uint8 *)(ptr + unit_->offset);
// 		o = *v8;
// 		break;
// 	case 2:
// 		v16 = (uint16 *)(ptr + unit_->offset);
// 		o = *v16;
// 		break;
// 	case 4:
// 		v32 = (uint32 *)(ptr + unit_->offset);
// 		o = *v32;
// 		break;
// 	case 8:
// 		v64 = (uint64 *)(ptr + unit_->offset);
// 		o = *v64;
// 		break;
// 	default:
// 		break;
// 	}
// 	return o;
// }
// int64 sisdb_field_get_int(s_sisdb_field *unit_, const char *val_)
// {
// 	if(!sisdb_field_is_integer(unit_)||!val_) return 0;
// 	int64  o = 0;
// 	int8  *v8;
// 	int16 *v16;
// 	int32 *v32;
// 	int64 *v64;

// 	const char *ptr = val_;

// 	switch (unit_->flags.len)
// 	{
// 	case 1:
// 		v8 = (int8 *)(ptr + unit_->offset);
// 		o = *v8;
// 		break;
// 	case 2:
// 		v16 = (int16 *)(ptr + unit_->offset);
// 		o = *v16;
// 		break;
// 	case 4:
// 		v32 = (int32 *)(ptr + unit_->offset);
// 		o = *v32;
// 		break;
// 	case 8:
// 		v64 = (int64 *)(ptr + unit_->offset);
// 		o = *v64;
// 		break;
// 	default:
// 		break;
// 	}
// 	return o;
// }
// double sisdb_field_get_float(s_sisdb_field *unit_, const char *val_)
// {
// 	if(!sisdb_field_is_float(unit_)||!val_) return 0.0;

// 	double   o = 0.0;
// 	float32 *f32;
// 	float64 *f64;
// 	const char *ptr = val_;
// 	switch (unit_->flags.len)
// 	{
// 	case 4:
// 		f32 = (float32 *)(ptr + unit_->offset);
// 		o = (double)*f32;
// 		break;
// 	case 8:
// 		f64 = (float64 *)(ptr + unit_->offset);
// 		o = (double)*f64;
// 		break;
// 	default:
// 		break;
// 	}
// 	// printf("---[%s]=%f\n",fu_->name, out);
// 	return o;
// }
// void sisdb_field_set_uint(s_sisdb_field *unit_, char *val_, uint64 v64_)
// {
// 	uint64 zoom = 1;
// 	uint8   v8 = 0;
// 	uint16 v16 = 0;
// 	uint32 v32 = 0;
// 	uint64 v64 = 0;

// 	switch (unit_->flags.len)
// 	{
// 	case 1:
// 		v8 = (uint8)(v64_ / zoom);
// 		memmove(val_ + unit_->offset, &v8, unit_->flags.len);
// 		break;
// 	case 2:
// 		v16 = (uint16)(v64_ / zoom);
// 		memmove(val_ + unit_->offset, &v16, unit_->flags.len);
// 		break;
// 	case 4:
// 		v32 = (uint32)(v64_ / zoom);
// 		memmove(val_ + unit_->offset, &v32, unit_->flags.len);
// 		break;
// 	case 8:
// 	default:
// 		v64 = (uint64)(v64_ / zoom);
// 		memmove(val_ + unit_->offset, &v64, unit_->flags.len);
// 		break;
// 	}	
// }

// void sisdb_field_set_int(s_sisdb_field *unit_, char *val_, int64 v64_)
// {
// 	int64 zoom = 1;
// 	int8   v8 = 0;
// 	int16 v16 = 0;
// 	int32 v32 = 0;
// 	int64 v64 = 0;

// 	switch (unit_->flags.len)
// 	{
// 	case 1:
// 		v8 = (int8)(v64_ / zoom);
// 		memmove(val_ + unit_->offset, &v8, unit_->flags.len);
// 		break;
// 	case 2:
// 		v16 = (int16)(v64_ / zoom);
// 		memmove(val_ + unit_->offset, &v16, unit_->flags.len);
// 		break;
// 	case 4:
// 		v32 = (int32)(v64_ / zoom);
// 		memmove(val_ + unit_->offset, &v32, unit_->flags.len);
// 		break;
// 	case 8:
// 	default:
// 		v64 = (int64)(v64_ / zoom);
// 		memmove(val_ + unit_->offset, &v64, unit_->flags.len);
// 		break;
// 	}	
// }
// void sisdb_field_set_price(s_sisdb_field *unit_, char *val_, double f64_, int dot_)
// {
// 	int32 v32 = 0;
// 	int64 v64 = 0;
// 	int64 zoom = sis_zoom10(dot_);
// 	switch (unit_->flags.len)
// 	{
// 	case 4:
// 		v32 = (int32)(f64_ * zoom);
// 		memmove(val_ + unit_->offset, &v32, unit_->flags.len);
// 		break;
// 	case 8:
// 	default:
// 		v64 = (int64)(f64_ * zoom);
// 		memmove(val_ + unit_->offset, &v64, unit_->flags.len);
// 		break;
// 	}

// }
// void sisdb_field_set_float(s_sisdb_field *unit_, char *val_, double f64_)
// {
// 	float32 f32 = 0.0;
// 	float64 f64 = 0.0;
// 	switch (unit_->flags.len)
// 	{
// 	case 4:
// 		f32 = (float32)f64_;
// 		memmove(val_ + unit_->offset, &f32, unit_->flags.len);
// 		break;
// 	case 8:
// 	default:
// 		f64 = (float64)f64_;
// 		memmove(val_ + unit_->offset, &f64, unit_->flags.len);
// 		break;
// 	}

// }
////////////////////////////////////////
// 定义各种转换方法，初始化时链接到各个字段下
////////////////////////////////////////

void _sis_dynamic_int_to_uint(void *inunit_,void *in_, void *out_)
{
	// s_sis_dynamic_unit *inunit = (s_sis_dynamic_unit *)inunit_;
	// const char *inptr = (const char *)in_;
	// const char *outptr = (const char *)out_;
	// s_sis_dynamic_unit *outunit = (s_sis_dynamic_unit *)inunit->map_unit;
	
	
	
	//memmove(outptr+outunit->offset, inptr+inunit->offset, inunit->len*inunit->count);	

}
void _sis_dynamic_uint_to_int(void *inunit_,void *in_, void *out_)
{
	// s_sis_dynamic_unit *inunit = (s_sis_dynamic_unit *)inunit_;
	// const char *inptr = (const char *)in_;
	// const char *outptr = (const char *)out_;
	// s_sis_dynamic_unit *outunit = (s_sis_dynamic_unit *)inunit->map_unit;
	
	
	
	//memmove(outptr+outunit->offset, inptr+inunit->offset, inunit->len*inunit->count);	

}
// 浮点数长度变更
void _sis_dynamic_float_relen(void *inunit_,void *in_, void *out_)
{
	// s_sis_dynamic_unit *inunit = (s_sis_dynamic_unit *)inunit_;
	// const char *inptr = (const char *)in_;
	// const char *outptr = (const char *)out_;
	// s_sis_dynamic_unit *outunit = (s_sis_dynamic_unit *)inunit->map_unit;
	
	// double value = _sis_dynamic_get_float(inunit, inptr);
	// _sis_dynamic_set_float(outunit, outptr, value);
}
////////////////////////////////
//
///////////////////////////////

int _sis_dynamic_get_style(const char *str_)
{
	if(sis_strcasecmp(str_,"int"))
	{
		return SIS_DYNAMIC_TYPE_INT;
	}
	if(sis_strcasecmp(str_,"uint"))
	{
		return SIS_DYNAMIC_TYPE_UINT;
	}
	if(sis_strcasecmp(str_,"float"))
	{
		return SIS_DYNAMIC_TYPE_FLOAT;
	}
	if(sis_strcasecmp(str_,"char"))
	{
		return SIS_DYNAMIC_TYPE_CHAR;
	}
	return SIS_DYNAMIC_TYPE_NONE;
}

s_sis_dynamic_db *_sis_dynamic_create(s_sis_json_node *node_)
{
	s_sis_dynamic_db *dyna = (s_sis_dynamic_db *)sis_malloc(sizeof(s_sis_dynamic_db));
	memset(dyna, 0, sizeof(s_sis_dynamic_db));

	dyna->fields = sis_struct_list_create(sizeof(s_sis_dynamic_unit),NULL,0); 

	int offset = 0;
	s_sis_json_node *node = sis_json_first_node(node_);
	while (node)
	{
		const char *name = sis_json_get_str(node, "0");
		if (!name) 
		{
			node = sis_json_next_node(node);
			continue;
		}
		int style = _sis_dynamic_get_style(sis_json_get_str(node, "1"));
		if (style == SIS_DYNAMIC_TYPE_NONE)
		{
			node = sis_json_next_node(node);
			continue;			
		}
		int len = sis_json_get_int(node, "2", 4);
		if (len > 255)
		{
			node = sis_json_next_node(node);
			continue;				
		}
		int count = sis_json_get_int(node, "3", 1);
		if (count > 255 || count < 1)
		{
			node = sis_json_next_node(node);
			continue;				
		}
		// 到此认为数据合法
		s_sis_dynamic_unit unit;
		memset(&unit, 0, sizeof(s_sis_dynamic_unit));
		sis_strcpy(unit.name,SIS_DYNAMIC_FIELD_LEN,name);
		unit.style = style;
		unit.len = len;
		unit.count = count;
		unit.offset = offset;
		offset += unit.len*unit.count;
		sis_struct_list_push(dyna->fields, &unit);		

		node = sis_json_next_node(node);
	}
	if (offset == 0)
	{
		sis_struct_list_destroy(dyna->fields);
		sis_free(dyna);
		return NULL;
	} 
	dyna->size = offset; 
	return dyna;
}

void _sis_dynamic_destroy(s_sis_dynamic_db *dyna_)
{
	sis_struct_list_destroy(dyna_->fields);
	sis_free(dyna_);
}

void _sis_dynamic_match(s_sis_dynamic_db *remote_, s_sis_dynamic_db *local_)
{
	if (!local_)
	{
		// 表示远端有的数据集合，本地没有,直接返回，
		return ;
	}
}
// 收到server的结构体说明后，初始化一次，后面就不用再匹配了
s_sis_dynamic_class *sis_dynamic_class_create(const char *in_, const char *out_)
{
	s_sis_dynamic_class *class = NULL;
	s_sis_json_handle *injson = sis_json_load(in_, strlen(in_));
	s_sis_json_handle *outjson = sis_json_load(out_, strlen(out_));
	if (!injson||!outjson)
	{
		sis_json_close(injson);
		sis_json_close(outjson);
		return NULL;
	}

	class = (s_sis_dynamic_class *)sis_malloc(sizeof(s_sis_dynamic_class));
	memset(class, 0, sizeof(s_sis_dynamic_class));

	class->remote = sis_map_pointer_create(); 
	s_sis_json_node *innode = sis_json_first_node(injson->node);
	while (innode)
	{
		s_sis_dynamic_db *dynamic = _sis_dynamic_create(innode);
		sis_map_pointer_set(class->remote, innode->key, dynamic);
		innode = sis_json_next_node(innode);
	}
	sis_json_close(injson);
	
	class->local = sis_map_pointer_create();
	s_sis_json_node *outnode = sis_json_first_node(outjson->node);
	while (outnode)
	{
		s_sis_dynamic_db *dynamic = _sis_dynamic_create(outnode);
		sis_map_pointer_set(class->local, outnode->key, dynamic);
		outnode = sis_json_next_node(outnode);
	}
	sis_json_close(outjson);

	////  下面开始处理关联性的信息  //////
	// 仅仅只处理服务端下发的结构集合，对于服务端已经不支持的结构，客户端应该强制升级处理
	// 对于服务端有客户端没有的结构体，客户端应该收到后直接抛弃
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
	//////////////////////////////////

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
			_sis_dynamic_destroy(val);
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
			_sis_dynamic_destroy(val);
		}
		sis_dict_iter_free(di);
	}
	sis_map_pointer_destroy(cls_->local);
	sis_free(cls_);
	
}

// 返回0表示数据处理正常，解析数据要记得释放内存
int sis_dynamic_analysis(
		s_sis_dynamic_class *cls_, 
        const char *key_,
        const char *in_, size_t ilen_,
        char *out_, size_t *olen_)
{
	if (!olen_||!in_||!ilen_)
	{
		return SIS_DYNAMIC_ERR;
	}
	out_ = NULL; *olen_ = 0;
	s_sis_dynamic_db *indb = sis_map_pointer_get(cls_->remote, key_);
	if (!indb)
	{
		// 传入的数据不是远端定义的数据集合
		return SIS_DYNAMIC_NOKEY;
	}
	int count = ilen_/indb->size;
	if (count*indb->size != ilen_)
	{
		// 来源数据尺寸不匹配
		return SIS_DYNAMIC_SIZE;
	}

	int o = SIS_DYNAMIC_OK;
	switch (indb->method)
	{
		case SIS_DYNAMIC_METHOD_NONE:
			// 本地端版本滞后，无法解析远端的数据格式
			// 可通过 sis_dynamic_analysis_to_json 获取json格式的数据字串
			o = SIS_DYNAMIC_NEW;
			break;	
		case SIS_DYNAMIC_METHOD_COPY:
			*olen_ = ilen_;
			out_ = sis_malloc(ilen_ + 1);
			memmove(out_, in_, *olen_);
			// 直接拷贝
			break;	
		case SIS_DYNAMIC_METHOD_OWNER:
			{
				s_sis_dynamic_db *outdb = (s_sis_dynamic_db *)indb->map_db;
				*olen_ = count * outdb->size;
				out_ = sis_calloc(*olen_ + 1);
				// memset(out_, 0, *olen_);
				const char *inptr = in_;
				char *outptr = out_;
				for(int i = 0; i < count; i++)
				{
					s_sis_dynamic_unit *inunit = (s_sis_dynamic_unit *)sis_struct_list_first(indb->fields);
					while(inunit)
					{			
						if(inunit->shift)
						{
							inunit->shift(inunit, (void *)inptr, (void *)outptr);
						}
						else
						{
							s_sis_dynamic_unit *outunit = (s_sis_dynamic_unit *)inunit->map_unit;
							memmove(outptr+outunit->offset, inptr+inunit->offset, inunit->len*inunit->count);	
						}
						inunit = sis_struct_list_next(indb->fields, inunit);
					}
					inptr += indb->size;
					outptr += outdb->size;
				}
			}
			break;	
		default:
			o = SIS_DYNAMIC_ERR;
			break;
	}
    return o;
}

// int sis_dynamic_analysis_to_json(
// 		s_sis_dynamic_class *cls_, 
//         const char *key_,
//         const char *in_, size_t ilen_,
//         const char *out_, size_t *olen_)

#if 1
int main()
{
	return 0;
}
#endif