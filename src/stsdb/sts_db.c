

#include "sts_db.h"

#ifdef STATIC_STS_DB

static sts_db *__sys_sts_db = NULL;
static s_map_pointer *__sys_sts_define = NULL;

static struct sts_map_define stsFieldTypes[] = {
	/////////////类型定义/////////////
	{ "TIDX", STS_FIELD_INDEX, 4 },
	{ "TTIME", STS_FIELD_TIME, 8 },
	{ "TSEC", STS_FIELD_SECOND, 4 },
	{ "TMIN", STS_FIELD_MINUTE, 4 },
	{ "TDAY", STS_FIELD_DAY, 4 },
	{ "TMON", STS_FIELD_MONTH, 4 },
	{ "TYEAR", STS_FIELD_YEAR, 2 },
	{ "TCODE", STS_FIELD_CODE, 8 },
	{ "INT1", STS_FIELD_INT1, 1 }, 
	{ "INT2", STS_FIELD_INT1, 2 },
	{ "INT4", STS_FIELD_INT1, 4 },
	{ "INTZ", STS_FIELD_INTZ, 4 },
	{ "INT8", STS_FIELD_INT1, 8 },
	{ "FLOAT", STS_FIELD_FLOAT, 4 },
	{ "DOUBLE", STS_FIELD_DOUBLE, 8 },
	{ "CHAR", STS_FIELD_CHAR, 16 },
	{ "PRC", STS_FIELD_PRC, 4 },
	{ "VOL", STS_FIELD_VOL, 4 },
	/////////////编码定义/////////////
	{ "SRC", STS_ENCODEING_SRC, 0 },
	{ "ROW", STS_ENCODEING_ROW, 0 },
	{ "COL", STS_ENCODEING_COL, 0 },
	{ "ALL", STS_ENCODEING_ALL, 0 },
	{ "STR", STS_ENCODEING_STR, 0 },
	{ "COD", STS_ENCODEING_COD, 0 },
	/////////////编码定义/////////////
	{ "ZIP", STS_DATA_ZIP, 0 },
	{ "BIN", STS_DATA_BIN, 0 },
	{ "STRING", STS_DATA_STRING, 0 },
	{ "JSON", STS_DATA_JSON, 0 },
	{ "ARRAY", STS_DATA_ARRAY, 0 }
};

void __init_map_define(s_map_pointer *fields_)
{
	clear_map_pointer(fields_);
	int nums = sizeof(stsFieldTypes) / sizeof(struct sts_map_define);

	for (int i = 0; i < sizeof(stsFieldTypes); i++){
		struct sts_map_define *f = stsFieldTypes + i;
		int rtn = dictAdd(fields_, sdsnew(f->key), f);
		assert(rtn == DICT_OK);
	}
}
void create_sts_db()  //数据库的名称，为空建立一个sys的数据库名
{
	__sys_sts_define = create_map_pointer();
	__init_map_define(__sys_sts_define);

	__sys_sts_db = create_map_pointer();
}
void destroy_sts_db()  //关闭一个数据库
{
	// 遍历字典中table，手动释放实际的table
	dictEntry *de;
	dictIterator *di = dictGetSafeIterator(__sys_sts_db);
	while ((de = dictNext(di)) != NULL) {
		sts_table *val = (sts_table *)dictGetVal(de);
		destroy_sts_table(val);
	}
	dictReleaseIterator(di);
	destroy_map_pointer(__sys_sts_db);
	__sys_sts_db = NULL;

	destroy_map_pointer(__sys_sts_define);
	__sys_sts_define = NULL;
}
//取数据和写数据
sts_table *sts_db_get_table(const char *name_)
{
	sts_table *val = (sts_table *)dictFetchValue(__sys_sts_db, name_);
	return val;
}

void sts_db_install_table(sts_table *tb_)
{
	dictAdd(__sys_sts_db, sdsnew(tb_->name), tb_);
}

sts_map_define *sts_db_find_map_define(const char *name_)
{
	if (!name_) { return NULL; }
	sts_map_define *val = (sts_map_define *)dictFetchValue(__sys_sts_define, name_);
	return val;
}
#else
sts_db *create_sts_db()  //数据库的名称，为空建立一个sys的数据库名
{
	sts_db *db = create_map_pointer();
	return db;
}
void destroy_sts_db(sts_db *db_)  //关闭一个数据库
{
	// 遍历字典中table，手动释放实际的table
	dictEntry *de;
	dictIterator *di = dictGetSafeIterator(db_);
	while ((de = dictNext(di)) != NULL) {
		sts_table *val = (sts_table *)dictGetVal(de);
		destroy_sts_table(val);
	}
	dictReleaseIterator(di);
	destroy_map_pointer(db_);
}
//取数据和写数据
sts_table *sts_db_get_table(sts_db *db_, const char *name_)
{
	sts_table *val = (sts_table *)dictFetchValue(db_, name_);
	return val;
}

void sts_db_install_table(sts_db *db_, sts_table *table_)
{
	dictAdd(db_, sdsnew(table_->name), table_);
}
#endif