

#include "sts_fields.h"
#include "sts_math.h"
#include "sts_table.h"

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

s_sts_field_unit *sts_field_unit_create(int index_, const char *name_, s_sts_fields_flags *flags_)
{
	s_sts_field_unit *unit = sts_malloc(sizeof(s_sts_field_unit));
	memset(unit, 0, sizeof(s_sts_field_unit));
	unit->index = index_;
	sts_strcpy(unit->name, STS_FIELD_MAXLEN, name_);
	memmove(&unit->flags, flags_, sizeof(s_sts_fields_flags));
	return unit;
}
void sts_field_unit_destroy(s_sts_field_unit *unit_)
{
	sts_free(unit_);
}
bool sts_field_is_price(s_sts_field_unit *unit_)
{
	if (!sts_strcase_match(unit_->name, "open") ||
		!sts_strcase_match(unit_->name, "high") ||
		!sts_strcase_match(unit_->name, "low") ||
		!sts_strcase_match(unit_->name, "close"))
	{
		return true;
	}
	return false;
}
bool sts_field_is_time(s_sts_field_unit *unit_)
{
	if (!sts_strcasecmp(unit_->name, "time"))
	{
		return true;
	}
	return false;
}
bool sts_field_is_volume(s_sts_field_unit *unit_)
{
	if (!sts_strcasecmp(unit_->name, "vol") || !sts_strcasecmp(unit_->name, "money"))
	{
		return true;
	}
	return false;
}
uint64 sts_fields_get_uint(s_sts_field_unit *fu_, const char *val_)
{
	uint64 out = 0;
	uint8 *u8;
	uint16 *u16;
	uint32 *u32;
	uint64 *u64;
	const char *ptr = val_;

	switch (fu_->flags.len)
	{
	case 1:
		u8 = (uint8 *)(ptr + fu_->offset);
		out = *u8;
		break;
	case 2:
		u16 = (uint16 *)(ptr + fu_->offset);
		out = *u16;
		break;
	case 4:
		u32 = (uint32 *)(ptr + fu_->offset);
		out = *u32;
		break;
	case 8:
		u64 = (uint64 *)(ptr + fu_->offset);
		out = *u64;
		break;
	default:
		break;
	}
	if (fu_->flags.io && fu_->flags.zoom > 0)
	{
		out *= sts_zoom10(fu_->flags.zoom);
	}
	return out;
}
int64 sts_fields_get_int(s_sts_field_unit *fu_, const char *val_)
{
	int64 out = 0;
	int8 *u8;
	int16 *u16;
	int32 *u32;
	int64 *u64;
	const char *ptr = val_;

	switch (fu_->flags.len)
	{
	case 1:
		u8 = (int8 *)(ptr + fu_->offset);
		out = *u8;
		break;
	case 2:
		u16 = (int16 *)(ptr + fu_->offset);
		out = *u16;
		break;
	case 4:
		u32 = (int32 *)(ptr + fu_->offset);
		out = *u32;
		break;
	case 8:
		u64 = (int64 *)(ptr + fu_->offset);
		out = *u64;
		break;
	default:
		break;
	}
	if (fu_->flags.io && fu_->flags.zoom > 0)
	{
		out *= sts_zoom10(fu_->flags.zoom);
	}
	return out;
}
// double sts_fields_get_double(s_sts_field_unit *fu_, const char *val_)
// {
// 	double out = 0.0;
// 	float *f32;
// 	double *f64;
// 	const char *ptr = val_;

// 	switch (fu_->flags.len)
// 	{
// 	case 4:
// 		f32 = (float *)(ptr + fu_->offset);
// 		out = *f32;
// 		break;
// 	case 8:
// 		f64 = (double *)(ptr + fu_->offset);
// 		out = *f64;
// 		break;
// 	default:
// 		break;
// 	}
// 	printf("---[%s]=%.f\n",fu_->name, out);
// 	if (!fu_->flags.io && fu_->flags.zoom > 0)
// 	{
// 		out /= sts_zoom10(fu_->flags.zoom);
// 	}
// 	return out;
// }
double sts_fields_get_double(s_sts_field_unit *fu_, const char *val_)
{
	double out = 0.0;
	int32 *i32;
	int64 *i64;
	const char *ptr = val_;

	if (!fu_->flags.io && fu_->flags.zoom > 0)
	{
		out = sts_zoom10(fu_->flags.zoom);
	}
	switch (fu_->flags.len)
	{
	case 4:
		i32 = (int32 *)(ptr + fu_->offset);
		out = (double)*i32/out;
		break;
	case 8:
		i64 = (int64 *)(ptr + fu_->offset);
		out = (double)*i64/out;
		break;
	default:
		break;
	}
	// printf("---[%s]=%f\n",fu_->name, out);
	return out;
}
void sts_fields_set_uint(s_sts_field_unit *fu_, char *val_, uint64 u64_)
{
	uint64 zoom = 1;
	uint8 u8 = 0;
	uint16 u16 = 0;
	uint32 u32 = 0;
	uint64 u64 = 0;

	if (fu_->flags.io && fu_->flags.zoom > 0)
	{
		zoom = sts_zoom10(fu_->flags.zoom);
	}
	if (fu_->flags.len == 1)
	{
		u8 = (uint8)(u64_ / zoom);
		memmove(val_ + fu_->offset, &u8, fu_->flags.len);
	}
	else if (fu_->flags.len == 2)
	{
		u16 = (uint16)(u64_ / zoom);
		memmove(val_ + fu_->offset, &u16, fu_->flags.len);
	}
	else if (fu_->flags.len == 4)
	{
		u32 = (uint32)(u64_ / zoom);
		memmove(val_ + fu_->offset, &u32, fu_->flags.len);
	}
	else
	{
		u64 = (uint64)(u64_ / zoom);
		memmove(val_ + fu_->offset, &u64, fu_->flags.len);
	}
}

void sts_fields_set_int(s_sts_field_unit *fu_, char *val_, int64 i64_)
{
	int64 zoom = 1;
	int8 i8 = 0;
	int16 i16 = 0;
	int32 i32 = 0;
	int64 i64 = 0;

	if (fu_->flags.io && fu_->flags.zoom > 0)
	{
		zoom = sts_zoom10(fu_->flags.zoom);
	}
	if (fu_->flags.len == 1)
	{
		i8 = (int8)(i64_ / zoom);
		memmove(val_ + fu_->offset, &i8, fu_->flags.len);
	}
	else if (fu_->flags.len == 2)
	{
		i16 = (int16)(i64_ / zoom);
		memmove(val_ + fu_->offset, &i16, fu_->flags.len);
	}
	else if (fu_->flags.len == 4)
	{
		i32 = (int32)(i64_ / zoom);
		memmove(val_ + fu_->offset, &i32, fu_->flags.len);
	}
	else
	{
		i64 = (int64)(i64_ / zoom);
		memmove(val_ + fu_->offset, &i64, fu_->flags.len);
	}
}

void sts_fields_set_double(s_sts_field_unit *fu_, char *val_, double f64_)
{

	int32 i32 = 0;
	int64 i64 = 0;
	int64 zoom = 1;
	if (!fu_->flags.io && fu_->flags.zoom > 0)
	{
		zoom = sts_zoom10(fu_->flags.zoom);
	}
	if (fu_->flags.len == 4)
	{
		i32 = (int32)(f64_ * zoom);
		memmove(val_ + fu_->offset, &i32, fu_->flags.len);
	}
	else
	{
		i64 = (int64)(f64_ * zoom);
		memmove(val_ + fu_->offset, &i64, fu_->flags.len);
	}
}

//获取数据库的各种值
s_sts_field_unit *sts_field_get_from_key(s_sts_table *tb_, const char *key_)
{
	if (!key_||!tb_->field_map)
	{
		return NULL;
	}
	s_sts_field_unit *fu = (s_sts_field_unit *)sts_map_buffer_get(tb_->field_map, key_);
	return fu;
}

uint64 sts_fields_get_uint_from_key(s_sts_table *tb_, const char *key_, const char *val_)
{
	s_sts_field_unit *fu = sts_field_get_from_key(tb_, key_);
	if (!fu)
	{
		return 0;
	}
	return sts_fields_get_uint(fu, val_);
}
int64 sts_fields_get_int_from_key(s_sts_table *tb_, const char *key_, const char *val_)
{
	s_sts_field_unit *fu = sts_field_get_from_key(tb_, key_);
	if (!fu)
		return 0;
	return sts_fields_get_int(fu, val_);
}
double sts_fields_get_double_from_key(s_sts_table *tb_, const char *key_, const char *val_)
{
	s_sts_field_unit *fu = sts_field_get_from_key(tb_, key_);
	if (!fu)
		return 0.0;
	return sts_fields_get_double(fu, val_);
}

void sts_fields_set_uint_from_key(s_sts_table *tb_, const char *key_, char *val_, uint64 u64_)
{
	s_sts_field_unit *fu = sts_field_get_from_key(tb_, key_);
	if (fu)
	{
		sts_fields_set_uint(fu, val_, u64_);
	}
}
void sts_fields_set_int_from_key(s_sts_table *tb_, const char *key_, char *val_, int64 i64_)
{
	s_sts_field_unit *fu = sts_field_get_from_key(tb_, key_);
	if (fu)
	{
		sts_fields_set_int(fu, val_, i64_);
	}
}
void sts_fields_set_double_from_key(s_sts_table *tb_, const char *key_, char *val_, double f64_)
{
	s_sts_field_unit *fu = sts_field_get_from_key(tb_, key_);
	if (fu)
	{
		sts_fields_set_double(fu, val_, f64_);
	}
}
void sts_fields_copy(char *des_, const char *src_, size_t len_)
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
