

#include "sts_fields.h"
#include "sts_str.h"
#include "sts_db.h"

// #include "zmalloc.h"
#include "sdsalloc.h"

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
inline bool is_intlen(int n)
{
	if (n == 1 || n == 2 || n == 4 || n == 8) return true;
	return false;
}

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

bool sts_field_is_times(int t_)
{
	switch (t_)
	{
	case STS_FIELD_INDEX:
	case STS_FIELD_TIME:
	case STS_FIELD_SECOND:
	case STS_FIELD_MIN1:
	case STS_FIELD_MIN5:	
	case STS_FIELD_DAY:
	case STS_FIELD_CODE:
		return true;
	default:
		return false;
	}
}