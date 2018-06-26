

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
	// {"FLOAT", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_FLOAT, 4},
	{"DOUBLE", STS_MAP_DEFINE_FIELD_TYPE, STS_FIELD_DOUBLE, 8},
	/////////////插入和修改方式定义/////////////
	{"NONE", STS_MAP_DEFINE_OPTION_MODE, STS_OPTION_NONE, 0},
	{"ALWAYS", STS_MAP_DEFINE_OPTION_MODE, STS_OPTION_ALWAYS, 0},
	{"SORT", STS_MAP_DEFINE_OPTION_MODE, STS_OPTION_SORT, 0},
	{"TIME", STS_MAP_DEFINE_OPTION_MODE, STS_OPTION_TIME, 0},
	{"VOL", STS_MAP_DEFINE_OPTION_MODE, STS_OPTION_VOL, 0},
	{"CODE", STS_MAP_DEFINE_OPTION_MODE, STS_OPTION_CODE, 0},
	/////////////数据类型定义/////////////
	{"NONE", STS_MAP_DEFINE_SCALE, STS_SCALE_NONE, 0},
	{"MSEC", STS_MAP_DEFINE_SCALE, STS_SCALE_MSEC, 0},
	{"SECOND", STS_MAP_DEFINE_SCALE, STS_SCALE_SECOND, 0},
	{"INDEX", STS_MAP_DEFINE_SCALE, STS_SCALE_INDEX, 0},
	{"MIN1", STS_MAP_DEFINE_SCALE, STS_SCALE_MIN1, 0},
	{"MIN5", STS_MAP_DEFINE_SCALE, STS_SCALE_MIN5, 0},
	{"MIN30", STS_MAP_DEFINE_SCALE, STS_SCALE_MIN30, 0},
	{"DAY", STS_MAP_DEFINE_SCALE, STS_SCALE_DAY, 0},
	{"MONTH", STS_MAP_DEFINE_SCALE, STS_SCALE_MONTH, 0},
	/////////////存入数据的方法定义/////////////
	{"COVER", STS_MAP_DEFINE_FIELD_METHOD, STS_FIELD_METHOD_COVER, 0},
	{"MIN", STS_MAP_DEFINE_FIELD_METHOD, STS_FIELD_METHOD_MIN, 0},
	{"MAX", STS_MAP_DEFINE_FIELD_METHOD, STS_FIELD_METHOD_MAX, 0},
	{"INIT", STS_MAP_DEFINE_FIELD_METHOD, STS_FIELD_METHOD_INIT, 0},
	{"INCR", STS_MAP_DEFINE_FIELD_METHOD, STS_FIELD_METHOD_INCR, 0},
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
	{"CSV", STS_MAP_DEFINE_DATA_TYPE, STS_DATA_CSV, 0},
	{"ARRAY", STS_MAP_DEFINE_DATA_TYPE, STS_DATA_ARRAY, 0}};

void _init_map_define(s_sts_map_pointer *fields_)
{
	sts_map_pointer_clear(fields_);
	int nums = sizeof(_sts_map_defines) / sizeof(struct s_sts_map_define);

	for (int i = 0; i < nums; i++)
	{
		s_sts_sds key = sts_sdsnew(_sts_map_defines[i].key);
		key = sdscatfmt(key, ".%u", _sts_map_defines[i].style);
		int rtn = sts_dict_add(fields_, key, &_sts_map_defines[i]);
		assert(rtn == DICT_OK);
	}
}

s_sts_db *sts_db_create(char *name_) //数据库的名称，为空建立一个sys的数据库名
{
	s_sts_db *db = sts_malloc(sizeof(s_sts_db));
	memset(db, 0, sizeof(s_sts_db));
	db->name = sts_sdsnew(name_);
	db->db = sts_map_pointer_create();

	db->trade_time = sts_struct_list_create(sizeof(s_sts_time_pair), NULL, 0);
	db->save_plans = sts_struct_list_create(sizeof(uint16), NULL, 0);

	db->map = sts_map_pointer_create();

	_init_map_define(db->map);
	return db;
}

void sts_db_destroy(s_sts_db *db_) //关闭一个数据库
{
	if (!db_)
	{
		return;
	}
	// 遍历字典中table，手动释放实际的table
	if (db_->db)
	{
		s_sts_dict_entry *de;
		s_sts_dict_iter *di = sts_dict_get_iter(db_->db);
		while ((de = sts_dict_next(di)) != NULL)
		{
			s_sts_table *val = (s_sts_table *)sts_dict_getval(de);
			sts_table_destroy(val);
		}
		sts_dict_iter_free(di);
		// 如果没有sts_map_pointer_destroy就必须加下面这句
		// sts_map_buffer_clear(db_->db);
	}
	// 下面仅仅释放key
	sts_map_pointer_destroy(db_->db);

	sts_sdsfree(db_->name);
	sts_sdsfree(db_->conf);
	sts_map_pointer_destroy(db_->map);
	sts_struct_list_destroy(db_->trade_time);
	sts_struct_list_destroy(db_->save_plans);

	sts_free(db_);
}
//取数据和写数据
s_sts_table *sts_db_get_table(s_sts_db *db_, const char *name_)
{
	s_sts_table *val = NULL;
	if (db_->db)
	{
		s_sts_sds key = sts_sdsnew(name_);
		val = (s_sts_table *)sts_dict_fetch_value(db_->db, key);
		sts_sdsfree(key);
	}
	return val;
}

s_sts_sds sts_db_get_table_info_sds(s_sts_db *db_)
{
	s_sts_sds list = sts_sdsempty();
	if (db_->db)
	{
		s_sts_dict_entry *de;
		s_sts_dict_iter *di = sts_dict_get_iter(db_->db);
		while ((de = sts_dict_next(di)) != NULL)
		{
			s_sts_table *val = (s_sts_table *)sts_dict_getval(de);
			list = sdscatprintf(list, "  %-10s : fields=%2d, collects=%lu\n",
								val->name,
								sts_string_list_getsize(val->field_name),
								sts_map_buffer_getsize(val->collect_map));
		}
	}

	return list;
}

s_sts_map_define *sts_db_find_map_define(s_sts_db *db_, const char *name_, uint8 style_)
{
	if (!name_)
	{
		return NULL;
	}
	s_sts_sds key = sts_sdsnew(name_);
	key = sdscatfmt(key, ".%u", style_);
	s_sts_map_define *val = (s_sts_map_define *)sts_dict_fetch_value(db_->map, key);
	sts_sdsfree(key);
	// printf("%s,%p\n",name_,val);
	return val;
}

int sts_db_find_map_uid(s_sts_db *db_, const char *name_, uint8 style_)
{
	s_sts_map_define *map = sts_db_find_map_define(db_, name_, style_);
	if (!map)
	{
		return 0;
	}
	return map->uid;
}