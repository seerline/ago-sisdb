

#include "sts_fields.h"
#include "sts_math.h"

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
	s_sts_field_unit *unit = zmalloc(sizeof(s_sts_field_unit));
	unit->index = index_;
	sts_strcpy(unit->name, STS_FIELD_MAXLEN, name_);
	memmove(&unit->flags, flags_, sizeof(s_sts_fields_flags));
	return unit;
}
void sts_field_unit_destroy(s_sts_field_unit *unit_)
{
	zfree(unit_);
}

bool sts_field_is_times(s_sts_field_unit *unit_)
{
	if (unit_->index == 0 && !sts_strcasecmp(unit_->name,"time")){
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
double sts_fields_get_double(s_sts_field_unit *fu_, const char *val_)
{
	double out = 0.0;
	float *f32;
	double *f64;
	const char *ptr = val_;

	switch (fu_->flags.len)
	{
	case 4:
		f32 = (float *)(ptr + fu_->offset);
		out = *f32;
		break;
	case 8:
		f64 = (double *)(ptr + fu_->offset);
		out = *f64;
		break;
	default:
		break;
	}
	if (!fu_->flags.io && fu_->flags.zoom > 0)
	{
		out /= sts_zoom10(fu_->flags.zoom);
	}
	return out;
}
