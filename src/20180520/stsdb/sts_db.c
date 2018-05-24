

#include "sts_db.h"

static struct s_sts_map_define _sts_map_defines[] = {
	/////////////类型定义/////////////
	{"NONE", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_NONE, 4},
	// {"INDEX", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_INDEX, 4},
	// {"TIME", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_TIME, 8},
	// {"CODE", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_CODE, 8},
	{"STRING", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_STRING, 16},
	{"INT", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_INT, 4},
	{"UINT", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_UINT, 4},
	{"FLOAT", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_FLOAT, 4},
	{"DOUBLE", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_DOUBLE, 8},
	/////////////插入和修改方式定义/////////////
	{"NONE", STS_MAP_DEFINE_OPTION_MODE, STS_OPTION_NONE, 0},
	{"ALWAYS", STS_MAP_DEFINE_OPTION_MODE, STS_OPTION_ALWAYS, 0},
	{"TIME", STS_MAP_DEFINE_OPTION_MODE, STS_OPTION_TIME, 0},
	{"VOL", STS_MAP_DEFINE_OPTION_MODE, STS_OPTION_VOL, 0},
	/////////////数据类型定义/////////////
	{"NONE", STS_MAP_DEFINE_SCALE, STS_SCALE_NONE, 0},
	{"MSEC", STS_MAP_DEFINE_SCALE, STS_SCALE_MSEC, 0},
	{"SECOND", STS_MAP_DEFINE_SCALE, STS_SCALE_SECOND, 0},
	{"MIN1", STS_MAP_DEFINE_SCALE, STS_SCALE_MIN1, 0},
	{"MIN5", STS_MAP_DEFINE_SCALE, STS_SCALE_MIN5, 0},
	{"MIN30", STS_MAP_DEFINE_SCALE, STS_SCALE_MIN30, 0},
	{"DAY", STS_MAP_DEFINE_SCALE, STS_SCALE_DAY, 0},
	/////////////压缩类型定义/////////////
	// { "0", STS_MAP_DEFINE_ZIP_MODE, STS_FIELD_NONE, 0 },
	// { "UP", STS_MAP_DEFINE_ZIP_MODE, STS_FIELD_INDEX, 0 },
	// { "LOCAL", STS_MAP_DEFINE_ZIP_MODE, STS_FIELD_NONE, 0 },
	// { "MULTI", STS_MAP_DEFINE_ZIP_MODE, STS_FIELD_SECOND, 0 },
	/////////////编码定义/////////////
	// { "SRC", STS_ENCODEING_SRC, 0 },
	// { "ROW", STS_ENCODEING_ROW, 0 },
	// { "COL", STS_ENCODEING_COL, 0 },
	// { "ALL", STS_ENCODEING_ALL, 0 },
	// { "STR", STS_ENCODEING_STR, 0 },
	// { "COD", STS_ENCODEING_COD, 0 },
	/////////////数据类型定义/////////////
	{"ZIP", STS_MAP_DEFINE_DATA_TYPE, STS_DATA_ZIP, 0},
	{"STRUCT", STS_MAP_DEFINE_DATA_TYPE, STS_DATA_STRUCT, 0},
	{"STRING", STS_MAP_DEFINE_DATA_TYPE, STS_DATA_STRING, 0},
	{"JSON", STS_MAP_DEFINE_DATA_TYPE, STS_DATA_JSON, 0},
	{"ARRAY", STS_MAP_DEFINE_DATA_TYPE, STS_DATA_ARRAY, 0}};

void _init_map_define(s_sts_map_pointer *fields_)
{
	sts_map_pointer_clear(fields_);
	int nums = sizeof(_sts_map_defines) / sizeof(struct s_sts_map_define);

	for (int i = 0; i < nums; i++)
	{
		sds key = sdsnew(_sts_map_defines[i].key);
		key = sdscatfmt(key, ".%u", _sts_map_defines[i].style);
		int rtn = dictAdd(fields_, key, &_sts_map_defines[i]);
		assert(rtn == DICT_OK);
	}
}

#ifdef STATIC_STS_DB

static s_sts_db *_sys_sts_db = NULL;
static s_sts_map_pointer *_sys_sts_define = NULL;

void sts_db_create() //数据库的名称，为空建立一个sys的数据库名
{
	if (!_sys_sts_define)
	{
		_sys_sts_define = sts_map_pointer_create();
		_init_map_define(_sys_sts_define);
	}
	if (!_sys_sts_db)
	{
		_sys_sts_db = sts_map_pointer_create();
	}
}
void sts_db_destroy() //关闭一个数据库
{
	// 遍历字典中table，手动释放实际的table
	if (_sys_sts_db)
	{
		dictEntry *de;
		dictIterator *di = dictGetSafeIterator(_sys_sts_db);
		while ((de = dictNext(di)) != NULL)
		{
			s_sts_table *val = (s_sts_table *)dictGetVal(de);
			sts_table_destroy(val);
		}
		dictReleaseIterator(di);
		sts_map_pointer_destroy(_sys_sts_db);
		_sys_sts_db = NULL;
	}

	if (_sys_sts_define)
	{
		sts_map_pointer_destroy(_sys_sts_define);
		_sys_sts_define = NULL;
	}
}

sds sts_db_get_tables()
{
	sds list = sdsempty();
	if (_sys_sts_db)
	{
		dictEntry *de;
		dictIterator *di = dictGetSafeIterator(_sys_sts_db);
		while ((de = dictNext(di)) != NULL)
		{
			s_sts_table *val = (s_sts_table *)dictGetVal(de);
			list = sdscatprintf(list, "  %-10s : fields=%2d, collects=%lu\n",
					val->name, 
					sts_string_list_getsize(val->field_name),
					sts_map_buffer_getsize(val->collect_map));
		}
	}

	return list;
}
//取数据和写数据
s_sts_table *sts_db_get_table(const char *name_)
{
	s_sts_table *val = NULL;
	if (_sys_sts_db)
	{	
		sds key = sdsnew(name_);
		val = (s_sts_table *)dictFetchValue(_sys_sts_db, key);
		sdsfree(key);
	}
	return val;
}

void sts_db_install_table(s_sts_table *tb_)
{
	if (_sys_sts_db)
	{
		dictAdd(_sys_sts_db, sdsnew(tb_->name), tb_);
	}
}


#else
s_sts_db *sts_db_create() //数据库的名称，为空建立一个sys的数据库名
{
	if (!_sys_sts_define)
	{
		_sys_sts_define = sts_map_pointer_create();
		_init_map_define(_sys_sts_define);
	}
	s_sts_db *db = sts_map_pointer_create();
	return db;
}

void sts_db_destroy(s_sts_db *db_) //关闭一个数据库
{
	// 遍历字典中table，手动释放实际的table
	dictEntry *de;
	dictIterator *di = dictGetSafeIterator(db_);
	while ((de = dictNext(di)) != NULL)
	{
		s_sts_table *val = (s_sts_table *)dictGetVal(de);
		sts_table_destroy(val);
	}
	dictReleaseIterator(di);
	sts_map_pointer_destroy(db_);
}
//取数据和写数据
s_sts_table *sts_db_get_table(s_sts_db *db_, const char *name_)
{
	s_sts_table *val = (s_sts_table *)dictFetchValue(db_, name_);
	return val;
}

void sts_db_install_table(s_sts_db *db_, s_sts_table *table_)
{
	dictAdd(db_, sdsnew(table_->name), table_);
}
#endif

s_sts_map_define *sts_db_find_map_define(const char *name_, uint8 style_)
{
	if (!name_)
	{
		return NULL;
	}
	sds key = sdsnew(name_);
	key = sdscatfmt(key, ".%u", style_);
	s_sts_map_define *val = (s_sts_map_define *)dictFetchValue(_sys_sts_define, key);
	sdsfree(key);
	// printf("%s,%p\n",name_,val);
	return val;
}

int sts_db_find_map_uid(const char *name_, uint8 style_)
{
	s_sts_map_define *map =sts_db_find_map_define(name_, style_);
	if (!map)
	{
		return 0;
	}
	return map->uid;
}