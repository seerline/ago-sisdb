#include "sis_math.h"
#include "sisdb_map.h"
#include "sisdb_fields.h"
#include "sisdb_table.h"

/*
inline int zip_zoom(int n)  // 1000 ==> 0x00011  -1000 ==> 0x10011
{
	//int plus = 0x00;  // 正号
	//int minus = 0x10; // 负号
	int val = 0;
	int sign = n >= 0 ? 0x00 : 0x10;
	n = abs(n);
	while (1)
	{
		n = (int)(n / 10);
		if (n >= 1) { val++; } 
		else { break; }
	}
	if (val > 0x0F) val = 0; // 防止越界
	return val | sign;
};
inline unsigned long long unzip_zoom(int n)  // 1000 ==> 0x00011  -1000 ==> 0x10011
{
	//int plus = 0x00;  // 正号
	//int minus = 0x10; // 负号
	unsigned long long val = 1;
	int sign = n & 0x10 ? -1 : 1;
	n = n & 0x0F;
	for (int i = 0; i < n; i++) { 
		val = val * 10;
	}
	return sign*val;
};*/
// inline bool is_intlen(int n)
// {
// 	if (n == 1 || n == 2 || n == 4 || n == 8) return true;
// 	return false;
// }

s_sisdb_field *sisdb_field_create(int index_, const char *name_, s_sisdb_field_flags *flags_)
{
	s_sisdb_field *unit = sis_malloc(sizeof(s_sisdb_field));
	memset(unit, 0, sizeof(s_sisdb_field));
	unit->index = index_;
	sis_strcpy(unit->name, SIS_FIELD_MAXLEN, name_);
	memmove(&unit->flags, flags_, sizeof(s_sisdb_field_flags));
	return unit;
}
void sisdb_field_destroy(s_sisdb_field *unit_)
{
	sis_free(unit_);
}

bool sisdb_field_is_time(s_sisdb_field *unit_)
{
	return  unit_->flags.type == SIS_FIELD_TYPE_UINT||
			unit_->flags.type == SIS_FIELD_TYPE_MSEC||
			unit_->flags.type == SIS_FIELD_TYPE_SECOND||
			unit_->flags.type == SIS_FIELD_TYPE_DATE;
}
bool sisdb_field_is_price(s_sisdb_field *unit_)
{
	return unit_->flags.type == SIS_FIELD_TYPE_PRICE;
}
bool sisdb_field_is_volume(s_sisdb_field *unit_)
{
	return  unit_->flags.type == SIS_FIELD_TYPE_VOLUME||
			unit_->flags.type == SIS_FIELD_TYPE_AMOUNT;
}
bool sisdb_field_is_float(s_sisdb_field *unit_)
{
	return unit_->flags.type == SIS_FIELD_TYPE_FLOAT||
		   unit_->flags.type == SIS_FIELD_TYPE_PRICE;
}
bool sisdb_field_is_integer(s_sisdb_field *unit_)
{
	return  unit_->flags.type == SIS_FIELD_TYPE_INT||
			unit_->flags.type == SIS_FIELD_TYPE_UINT||
			unit_->flags.type == SIS_FIELD_TYPE_VOLUME||
			unit_->flags.type == SIS_FIELD_TYPE_AMOUNT||
			unit_->flags.type == SIS_FIELD_TYPE_MSEC||
			unit_->flags.type == SIS_FIELD_TYPE_SECOND||
			unit_->flags.type == SIS_FIELD_TYPE_DATE;
}
uint64 sisdb_field_get_uint(s_sisdb_field *unit_, const char *val_)
{
	if(!sisdb_field_is_integer(unit_)) return 0;

	uint64  o = 0;
	uint8  *v8;
	uint16 *v16;
	uint32 *v32;
	uint64 *v64;

	const char *ptr = val_;

	switch (unit_->flags.len)
	{
	case 1:
		v8 = (uint8 *)(ptr + unit_->offset);
		o = *v8;
		break;
	case 2:
		v16 = (uint16 *)(ptr + unit_->offset);
		o = *v16;
		break;
	case 4:
		v32 = (uint32 *)(ptr + unit_->offset);
		o = *v32;
		break;
	case 8:
		v64 = (uint64 *)(ptr + unit_->offset);
		o = *v64;
		break;
	default:
		break;
	}
	return o;
}
int64 sisdb_field_get_int(s_sisdb_field *unit_, const char *val_)
{
	if(!sisdb_field_is_integer(unit_)) return 0;
	int64  o = 0;
	int8  *v8;
	int16 *v16;
	int32 *v32;
	int64 *v64;

	const char *ptr = val_;

	switch (unit_->flags.len)
	{
	case 1:
		v8 = (int8 *)(ptr + unit_->offset);
		o = *v8;
		break;
	case 2:
		v16 = (int16 *)(ptr + unit_->offset);
		o = *v16;
		break;
	case 4:
		v32 = (int32 *)(ptr + unit_->offset);
		o = *v32;
		break;
	case 8:
		v64 = (int64 *)(ptr + unit_->offset);
		o = *v64;
		break;
	default:
		break;
	}
	return o;
}
double sisdb_field_get_float(s_sisdb_field *unit_, const char *val_, int dot_)
{
	if(!sisdb_field_is_float(unit_)) return 0.0;

	double   o = 0.0;
	int32 *v32;
	int64 *v64;

	const char *ptr = val_;
	int zoom = sis_zoom10(unit_->flags.dot);
	if(sisdb_field_is_price(unit_)) 
	{
		zoom = sis_zoom10(dot_);
	}

	switch (unit_->flags.len)
	{
	case 4:
		v32 = (int32 *)(ptr + unit_->offset);
		o = (double)*v32/zoom;
		break;
	case 8:
		v64 = (int64 *)(ptr + unit_->offset);
		o = (double)*v64/zoom;
		break;
	default:
		break;
	}
	// printf("---[%s]=%f\n",fu_->name, out);
	return o;
}

////////////////////////////////
//   --- set ----
/////////////////////////////////
void sisdb_field_set_uint(s_sisdb_field *unit_, char *val_, uint64 v64_)
{
	uint64 zoom = 1;
	uint8   v8 = 0;
	uint16 v16 = 0;
	uint32 v32 = 0;
	uint64 v64 = 0;

	switch (unit_->flags.len)
	{
	case 1:
		v8 = (uint8)(v64_ / zoom);
		memmove(val_ + unit_->offset, &v8, unit_->flags.len);
		break;
	case 2:
		v16 = (uint16)(v64_ / zoom);
		memmove(val_ + unit_->offset, &v16, unit_->flags.len);
		break;
	case 4:
		v32 = (uint32)(v64_ / zoom);
		memmove(val_ + unit_->offset, &v32, unit_->flags.len);
		break;
	case 8:
	default:
		v64 = (uint64)(v64_ / zoom);
		memmove(val_ + unit_->offset, &v64, unit_->flags.len);
		break;
	}	
}

void sisdb_field_set_int(s_sisdb_field *unit_, char *val_, int64 v64_)
{
	int64 zoom = 1;
	int8   v8 = 0;
	int16 v16 = 0;
	int32 v32 = 0;
	int64 v64 = 0;

	switch (unit_->flags.len)
	{
	case 1:
		v8 = (int8)(v64_ / zoom);
		memmove(val_ + unit_->offset, &v8, unit_->flags.len);
		break;
	case 2:
		v16 = (int16)(v64_ / zoom);
		memmove(val_ + unit_->offset, &v16, unit_->flags.len);
		break;
	case 4:
		v32 = (int32)(v64_ / zoom);
		memmove(val_ + unit_->offset, &v32, unit_->flags.len);
		break;
	case 8:
	default:
		v64 = (int64)(v64_ / zoom);
		memmove(val_ + unit_->offset, &v64, unit_->flags.len);
		break;
	}	
}
void sisdb_field_set_float(s_sisdb_field *unit_, char *val_, double f64_, int dot_)
{

	int32 v32 = 0;
	int64 v64 = 0;
	int64 zoom = 1;

	// const char *ptr = val_;
	zoom = sis_zoom10(unit_->flags.dot);
	if(sisdb_field_is_price(unit_)) 
	{
		zoom = sis_zoom10(dot_);
	}

	switch (unit_->flags.len)
	{
	case 4:
		v32 = (int32)(f64_ * zoom);
		memmove(val_ + unit_->offset, &v32, unit_->flags.len);
		break;
	case 8:
	default:
		v64 = (int64)(f64_ * zoom);
		memmove(val_ + unit_->offset, &v64, unit_->flags.len);
		break;
	}

}

///////////////////////////////////////
//获取数据库的各种值
///////////////////////////////////////
s_sisdb_field *sisdb_field_get_from_key(s_sisdb_table *tb_, const char *key_)
{
	// if (!key_||!tb_->field_map)
	// {
	// 	return NULL;
	// }
	s_sisdb_field *fu = (s_sisdb_field *)sis_map_buffer_get(tb_->field_map, key_);
	return fu;
}
int sisdb_field_get_offset(s_sisdb_table *tb_, const char *key_)
{
	s_sisdb_field *fu = sisdb_field_get_from_key(tb_, key_);
	if (!fu)
	{
		return 0;
	}
	return fu->offset + fu->flags.len;
}
const char * sisdb_field_get_char_from_key(s_sisdb_table *tb_, const char *key_, const char *val_, size_t *len_)
{
	s_sisdb_field *fu = sisdb_field_get_from_key(tb_, key_);
	if (!fu)
	{
		return NULL;
	}	
	*len_ = fu->flags.len;
	return val_ + fu->offset;
}

uint64 sisdb_field_get_uint_from_key(s_sisdb_table *tb_, const char *key_, const char *val_)
{
	s_sisdb_field *fu = sisdb_field_get_from_key(tb_, key_);
	if (!fu)
	{
		return 0;
	}
	return sisdb_field_get_uint(fu, val_);
}
// int64 sisdb_field_get_int_from_key(s_sisdb_table *tb_, const char *key_, const char *val_)
// {
// 	s_sisdb_field *fu = sisdb_field_get_from_key(tb_, key_);
// 	if (!fu)
// 		return 0;
// 	return sisdb_field_get_int(fu, val_);
// }
// double sisdb_field_get_float_from_key(s_sisdb_table *tb_, const char *key_, const char *val_)
// {
// 	s_sisdb_field *fu = sisdb_field_get_from_key(tb_, key_);
// 	if (!fu)
// 	{
// 		return 0.0;
// 	}
// 	return sisdb_field_get_float(fu, val_);
// }

// void sisdb_field_set_uint_from_key(s_sisdb_table *tb_, const char *key_, char *val_, uint64 u64_)
// {
// 	s_sisdb_field *fu = sisdb_field_get_from_key(tb_, key_);
// 	if (fu)
// 	{
// 		sisdb_field_set_uint(fu, val_, u64_);
// 	}
// }
// void sisdb_field_set_int_from_key(s_sisdb_table *tb_, const char *key_, char *val_, int64 i64_)
// {
// 	s_sisdb_field *fu = sisdb_field_get_from_key(tb_, key_);
// 	if (fu)
// 	{
// 		sisdb_field_set_int(fu, val_, i64_);
// 	}
// }
// void sisdb_field_set_double_from_key(s_sisdb_table *tb_, const char *key_, char *val_, double f64_)
// {
// 	s_sisdb_field *fu = sisdb_field_get_from_key(tb_, key_);
// 	if (fu)
// 	{
// 		sisdb_field_set_float(fu, val_, f64_);
// 	}
// }
void sisdb_field_copy(char *des_, const char *src_, size_t len_)
{
	if (!des_)
		return;

	if (!src_)
	{
		memset(des_, 0, len_);
	}
	else
	{
		memmove(des_, src_, len_);
	}
}

void sisdb_field_json_to_struct(s_sis_sds in_, s_sisdb_field *fu_, 
			char *key_, s_sis_json_node *node_,s_sisdb_config  *cfg_)
{
	int64  i64 = 0;
	double f64 = 0.0;
	const char *str;
	switch (fu_->flags.type)
	{
	case SIS_FIELD_TYPE_CHAR:
		str = sis_json_get_str(node_, key_);
		if (str)
		{
			memmove(in_ + fu_->offset, str, fu_->flags.len);
		}
		break;
	case SIS_FIELD_TYPE_UINT:
	case SIS_FIELD_TYPE_VOLUME:
	case SIS_FIELD_TYPE_AMOUNT:
	case SIS_FIELD_TYPE_MSEC:
	case SIS_FIELD_TYPE_SECOND:
	case SIS_FIELD_TYPE_DATE:
		if (sis_json_find_node(node_, key_))
		{
			i64 = sis_json_get_int(node_, key_, 0);
			sisdb_field_set_uint(fu_, in_, (uint64)i64);
		}
		break;
	case SIS_FIELD_TYPE_INT:
		if (sis_json_find_node(node_, key_))
		{
			i64 = sis_json_get_int(node_, key_, 0);
			sisdb_field_set_int(fu_, in_, i64);
		}
		break;
	case SIS_FIELD_TYPE_FLOAT:
	case SIS_FIELD_TYPE_PRICE:
		if (sis_json_find_node(node_, key_))
		{
			f64 = sis_json_get_double(node_, key_, 0.0);
			sisdb_field_set_float(fu_, in_, f64, cfg_->info.dot);
		}
		break;
	default:
		break;
	}
}