#include "sisdb_map.h"

// 所有需要快速检索的字符串都放这里，避免重复定义，需要设置不同的类型

static struct s_sis_map_define _sis_map_defines[] = {
	/////////////数据表类型定义/////////////
	// {"STS", SIS_MAP_DEFINE_TABLE_TYPE, SIS_TABLE_TYPE_STS, 0},
	// {"JSON", SIS_MAP_DEFINE_TABLE_TYPE, SIS_TABLE_TYPE_JSON, 0},
	/////////////时间接收和存储时的格式定义/////////////
	// {"NONE", SIS_MAP_DEFINE_TIME_FORMAT, SIS_TIME_FORMAT_NONE, 0},
	// {"INCR", SIS_MAP_DEFINE_TIME_FORMAT, SIS_TIME_FORMAT_INCR, 0},
	// {"MSEC", SIS_MAP_DEFINE_TIME_FORMAT, SIS_TIME_FORMAT_MSEC, 0},
	// {"SECOND", SIS_MAP_DEFINE_TIME_FORMAT, SIS_TIME_FORMAT_SECOND, 0},
	// {"DATE", SIS_MAP_DEFINE_TIME_FORMAT, SIS_TIME_FORMAT_DATE, 0},
	/////////////字段类型定义/////////////
	{"NONE", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_TYPE_NONE, 4},
	{"INT", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_TYPE_INT, 4},
	{"UINT", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_TYPE_UINT, 4},
	{"FLOAT", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_TYPE_FLOAT, 4},
	{"CHAR", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_TYPE_CHAR, 16},
	{"STRING", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_TYPE_STRING, 0},  // 不定长，json 数据表专用
	{"JSON", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_TYPE_JSON, 0},      // 不定长，json 数据表专用
	{"PRICE", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_TYPE_PRICE, 4},  
	{"VOLUME", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_TYPE_VOLUME, 4},
	{"AMOUNT", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_TYPE_AMOUNT, 4},
	{"MSEC", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_TYPE_MSEC, 8},
	{"SECOND", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_TYPE_SECOND, 4},
	{"DATE", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_TYPE_DATE, 4},
	// {"MSEC", SIS_MAP_DEFINE_TIME_FORMAT, SIS_TIME_FORMAT_MSEC, 0},
	// {"SECOND", SIS_MAP_DEFINE_TIME_FORMAT, SIS_TIME_FORMAT_SECOND, 0},
	// {"DATE", SIS_MAP_DEFINE_TIME_FORMAT, SIS_TIME_FORMAT_DATE, 0},
	/////////////时间尺度类型定义/////////////
	{"NONE", SIS_MAP_DEFINE_TIME_SCALE, SIS_TIME_SCALE_NONE, 0},
	{"INCR", SIS_MAP_DEFINE_TIME_SCALE, SIS_TIME_SCALE_INCR, 0},
	{"MSEC", SIS_MAP_DEFINE_TIME_SCALE, SIS_TIME_SCALE_MSEC, 0},
	{"SECOND", SIS_MAP_DEFINE_TIME_SCALE, SIS_TIME_SCALE_SECOND, 0},
	{"MIN1", SIS_MAP_DEFINE_TIME_SCALE, SIS_TIME_SCALE_MIN1, 0},
	{"MIN5", SIS_MAP_DEFINE_TIME_SCALE, SIS_TIME_SCALE_MIN5, 0},
	{"HOUR", SIS_MAP_DEFINE_TIME_SCALE, SIS_TIME_SCALE_HOUR, 0},
	{"DATE", SIS_MAP_DEFINE_TIME_SCALE, SIS_TIME_SCALE_DATE, 0},
	{"WEEK", SIS_MAP_DEFINE_TIME_SCALE, SIS_TIME_SCALE_WEEK, 0},
	{"MONTH", SIS_MAP_DEFINE_TIME_SCALE, SIS_TIME_SCALE_MONTH, 0},
	{"YEAR", SIS_MAP_DEFINE_TIME_SCALE, SIS_TIME_SCALE_YEAR, 0},
	/////////////插入和修改方式定义/////////////
	{"NONE", SIS_MAP_DEFINE_ADD_METHOD, SIS_ADD_METHOD_NONE, 0},
	{"ALWAYS", SIS_MAP_DEFINE_ADD_METHOD, SIS_ADD_METHOD_ALWAYS, 0},
	{"SORT", SIS_MAP_DEFINE_ADD_METHOD, SIS_ADD_METHOD_SORT, 0},
	{"TIME", SIS_MAP_DEFINE_ADD_METHOD, SIS_ADD_METHOD_TIME, 0},
	{"VOL", SIS_MAP_DEFINE_ADD_METHOD, SIS_ADD_METHOD_VOL, 0},
	{"CODE", SIS_MAP_DEFINE_ADD_METHOD, SIS_ADD_METHOD_CODE, 0},
	/////////////压缩类型定义/////////////
	// { "0", SIS_MAP_DEFINE_ZIP_MODE, SIS_FIELD_TYPE_NONE, 0 },
	// { "UP", SIS_MAP_DEFINE_ZIP_MODE, SIS_FIELD_TYPE_INDEX, 0 },
	// { "LOCAL", SIS_MAP_DEFINE_ZIP_MODE, SIS_FIELD_TYPE_NONE, 0 },
	// { "MULTI", SIS_MAP_DEFINE_ZIP_MODE, SIS_FIELD_TYPE_SECOND, 0 },
	/////////////当有广播数据时，数据的更新方法定义/////////////
	{"COPY", SIS_MAP_DEFINE_SUBS_METHOD, SIS_SUBS_METHOD_COPY, 0},
	{"MIN", SIS_MAP_DEFINE_SUBS_METHOD, SIS_SUBS_METHOD_MIN, 0},
	{"MAX", SIS_MAP_DEFINE_SUBS_METHOD, SIS_SUBS_METHOD_MAX, 0},
	{"ONCE", SIS_MAP_DEFINE_SUBS_METHOD, SIS_SUBS_METHOD_ONCE, 0},
	{"INCR", SIS_MAP_DEFINE_SUBS_METHOD, SIS_SUBS_METHOD_INCR, 0},
	/////////////编码定义/////////////
	// { "SRC", SIS_ENCODEING_SRC, 0 },
	// { "ROW", SIS_ENCODEING_ROW, 0 },
	// { "COL", SIS_ENCODEING_COL, 0 },
	// { "ALL", SIS_ENCODEING_ALL, 0 },
	// { "STR", SIS_ENCODEING_STR, 0 },
	// { "COD", SIS_ENCODEING_COD, 0 },
	/////////////写入和读取时数据类型定义/////////////
	{"ZIP", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_TYPE_ZIP, 0},
	{"STRUCT", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_TYPE_STRUCT, 0},
	{"STRING", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_TYPE_STRING, 0},
	{"JSON", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_TYPE_JSON, 0},
	{"CSV", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_TYPE_CSV, 0},
	{"ARRAY", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_TYPE_ARRAY, 0},
	// 下面是简称，使用的时候也可以直接按简称处理
	{"Z", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_TYPE_ZIP, 0},
	{"B", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_TYPE_STRUCT, 0},
	{"S", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_TYPE_STRING, 0},
	{"{", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_TYPE_JSON, 0},
	{"C", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_TYPE_CSV, 0},
	{"[", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_TYPE_ARRAY, 0}
};


void sisdb_init_map_define(s_sis_map_pointer *fields_)
{
	sis_map_pointer_clear(fields_);
	int nums = sizeof(_sis_map_defines) / sizeof(struct s_sis_map_define);

	for (int i = 0; i < nums; i++)
	{
		s_sis_sds key = sis_sdsnew(_sis_map_defines[i].key);
		key = sis_sdscatfmt(key, ".%u", _sis_map_defines[i].style);
		int rtn = sis_dict_add(fields_, key, &_sis_map_defines[i]);
		assert(rtn == DICT_OK);
	}
}


s_sis_map_define *sisdb_find_map_define(s_sis_map_pointer *map_, const char *name_, uint8 style_)
{
	if (!name_)
	{
		return NULL;
	}
	s_sis_sds key = sis_sdsnew(name_);
	key = sis_sdscatfmt(key, ".%u", style_);
	s_sis_map_define *val = (s_sis_map_define *)sis_dict_fetch_value(map_, key);
	sis_sdsfree(key);
	// printf("%s,%p\n",name_,val);
	return val;
}

int sisdb_find_map_uid(s_sis_map_pointer *map_, const char *name_, uint8 style_)
{
	s_sis_map_define *val = sisdb_find_map_define(map_, name_, style_);
	if (!val)
	{
		return 0;
	}
	return val->uid;
}