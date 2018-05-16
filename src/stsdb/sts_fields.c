

#include "sts_fields.h"
#include "zmalloc.h"
#include "sts_str.h"
#include "sts_db.h"

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
void set_field_flags(s_sts_fields_flags *fp_, int index_, const char *type_, int len_, int group_)
{
	s_sts_map_define *type = sts_db_find_map_define(type_);
	if (!type) {
		if (index_ == 0){
			fp_->type = STS_FIELD_SECOND;
			fp_->len = 4;
		}
		else {
			fp_->type = STS_FIELD_PRC;
			fp_->len = 4;
		}
		return;
	}
	fp_->type = type->uid;
	// fp_->group = group_;  // == 0
	switch (fp_->type)
	{
	case STS_FIELD_INDEX:
	case STS_FIELD_TIME:
	case STS_FIELD_SECOND:
	case STS_FIELD_MINUTE:
	case STS_FIELD_DAY:
	case STS_FIELD_MONTH:
	case STS_FIELD_YEAR:
	case STS_FIELD_CODE:
	case STS_FIELD_INT1:
	case STS_FIELD_INT2:
	case STS_FIELD_INT4:
	case STS_FIELD_INT8:
	case STS_FIELD_INTZ:
	case STS_FIELD_FLOAT:
	case STS_FIELD_DOUBLE:
		fp_->len = type->size;
		break;
	case STS_FIELD_PRC:
	case STS_FIELD_VOL:
		fp_->len = is_intlen(len_) ? len_ : type->size;
		break;
	case STS_FIELD_CHAR:
		fp_->len = (len_>0 && len_< 64) ? len_ : type->size;
		// 这里暂时只处理短字符串，长字符串以后有需要再说
		break;
	default:
		fp_->type = STS_FIELD_PRC;
		fp_->len = 4;
		break;
	}
}
s_sts_field_unit *sts_field_unit_create(int index_, const char *name_, const char *type_, const char *zip_, int len_, int group_)
{
	s_sts_field_unit *unit = zmalloc(sizeof(s_sts_field_unit));
	unit->index = index_;
	sts_strcpy(unit->name, STS_FIELD_MAXLEN, name_);
	s_sts_map_define *zip = sts_db_find_map_define(zip_);
	if (!zip) { 
		// unit->flags.encoding = STS_ZIP_NO; 
	}
	else {
		// unit->flags.encoding = zip->uid;
	}
	set_field_flags(&unit->flags, index_, type_, len_, group_);
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
	case STS_FIELD_MINUTE:
	case STS_FIELD_DAY:
	case STS_FIELD_MONTH:
	case STS_FIELD_YEAR:
	case STS_FIELD_CODE:
		return true;
	default:
		return false;
	}
}