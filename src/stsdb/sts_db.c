

#include "sts_db.h"

#ifdef STATIC_STS_DB

static s_sts_db *_sys_sts_db = NULL;
static s_sts_map_pointer *_sys_sts_define = NULL;

static struct s_sts_map_define _sts_map_defines[] = {
	/////////////类型定义/////////////
	{ "NONE", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_NONE, 4 },
	{ "INDEX", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_INDEX, 4 },
	{ "SECOND", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_SECOND, 4 },
	{ "MIN1", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_MIN1, 4 },
	{ "MIN5", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_MIN5, 4 },
	{ "DAY", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_DAY, 4 },
	{ "TIME", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_TIME, 8 },
	{ "CODE", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_CODE, 8 },
	{ "STRING", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_STRING, 16 },
	{ "INT", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_INT, 4 }, 
	{ "FLOAT", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_FLOAT, 4 },
	{ "DOUBLE", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_DOUBLE, 8 },
	/////////////编码定义/////////////
	// { "SRC", STS_ENCODEING_SRC, 0 },
	// { "ROW", STS_ENCODEING_ROW, 0 },
	// { "COL", STS_ENCODEING_COL, 0 },
	// { "ALL", STS_ENCODEING_ALL, 0 },
	// { "STR", STS_ENCODEING_STR, 0 },
	// { "COD", STS_ENCODEING_COD, 0 },
	/////////////插入方式定义/////////////
	{ "PUSH", STS_MAP_DEFINE_INSERT_MODE, STS_INSERT_PUSH, 0 },
	{ "INCR-TIME", STS_MAP_DEFINE_INSERT_MODE, STS_INSERT_INCR_TIME, 0 },
	{ "INCR-VOL", STS_MAP_DEFINE_INSERT_MODE, STS_INSERT_INCR_VOL, 0 },
	/////////////数据类型定义/////////////
	{ "NONE", STS_MAP_DEFINE_SCALE, STS_FIELD_NONE, 0 },
	{ "INDEX", STS_MAP_DEFINE_SCALE, STS_FIELD_INDEX, 0 },
	{ "SECOND", STS_MAP_DEFINE_SCALE, STS_FIELD_SECOND, 0 },
	{ "DAY", STS_MAP_DEFINE_SCALE, STS_FIELD_DAY, 0 },
	{ "MIN1", STS_MAP_DEFINE_SCALE, STS_FIELD_MIN1, 0 },
	{ "MIN5", STS_MAP_DEFINE_SCALE, STS_FIELD_MIN5, 0 },
	/////////////压缩类型定义/////////////
	// { "0", STS_MAP_DEFINE_ZIP_MODE, STS_FIELD_NONE, 0 },
	// { "UP", STS_MAP_DEFINE_ZIP_MODE, STS_FIELD_INDEX, 0 },
	// { "LOCAL", STS_MAP_DEFINE_ZIP_MODE, STS_FIELD_NONE, 0 },
	// { "MULTI", STS_MAP_DEFINE_ZIP_MODE, STS_FIELD_SECOND, 0 },
	/////////////数据类型定义/////////////
	{ "ZIP", STS_MAP_DEFINE_DATA_TYPE, STS_DATA_ZIP, 0 },
	{ "STRUCT", STS_MAP_DEFINE_DATA_TYPE, STS_DATA_STRUCT, 0 },
	{ "STRING", STS_MAP_DEFINE_DATA_TYPE, STS_DATA_STRING, 0 },
	{ "JSON", STS_MAP_DEFINE_DATA_TYPE, STS_DATA_JSON, 0 },
	{ "ARRAY", STS_MAP_DEFINE_DATA_TYPE, STS_DATA_ARRAY, 0 }
};

void _init_map_define(s_sts_map_pointer *fields_)
{
	sts_map_pointer_clear(fields_);
	// int nums = sizeof(_sts_map_defines) / sizeof(struct s_sts_map_define);

	for (int i = 0; i < sizeof(_sts_map_defines); i++){
		struct s_sts_map_define *f = _sts_map_defines + i;
		sds key = sdsnew(f->key);
		key = sdscatfmt(key, ".%u", f->style);
		int rtn = dictAdd(fields_, key, f);
		assert(rtn == DICT_OK);
	}
}
void sts_db_create()  //数据库的名称，为空建立一个sys的数据库名
{
	if(!_sys_sts_define) {
		_sys_sts_define = sts_map_pointer_create();
		_init_map_define(_sys_sts_define);
	}
	if(!_sys_sts_db) {
		_sys_sts_db = sts_map_pointer_create();
	}
}
void sts_db_destroy()  //关闭一个数据库
{
	// 遍历字典中table，手动释放实际的table
	dictEntry *de;
	dictIterator *di = dictGetSafeIterator(_sys_sts_db);
	while ((de = dictNext(di)) != NULL) {
		s_sts_table *val = (s_sts_table *)dictGetVal(de);
		sts_table_destroy(val);
	}
	dictReleaseIterator(di);
	sts_map_pointer_destroy(_sys_sts_db);
	_sys_sts_db = NULL;

	sts_map_pointer_destroy(_sys_sts_define);
	_sys_sts_define = NULL;
}
//取数据和写数据
s_sts_table *sts_db_get_table(const char *name_)
{
	s_sts_table *val = (s_sts_table *)dictFetchValue(_sys_sts_db, name_);
	return val;
}

void sts_db_install_table(s_sts_table *tb_)
{
	dictAdd(_sys_sts_db, sdsnew(tb_->name), tb_);
}

s_sts_map_define *sts_db_find_map_define(const char *name_, uint8 style_)
{
	if (!name_) { return NULL; }
	char key[64];
	sts_sprintf(key, 64, "%s.%u", name_, style_);
	s_sts_map_define *val = (s_sts_map_define *)dictFetchValue(_sys_sts_define, key);
	return val;
}
#else
s_sts_db *sts_db_create()  //数据库的名称，为空建立一个sys的数据库名
{
	s_sts_db *db = sts_map_pointer_create();
	return db;
}
void sts_db_destroy(s_sts_db *db_)  //关闭一个数据库
{
	// 遍历字典中table，手动释放实际的table
	dictEntry *de;
	dictIterator *di = dictGetSafeIterator(db_);
	while ((de = dictNext(di)) != NULL) {
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