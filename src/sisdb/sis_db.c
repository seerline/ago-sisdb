

#include "sis_db.h"
#include "sis_table.h"

static struct s_sis_map_define _sis_map_defines[] = {
	/////////////类型定义/////////////
	{"NONE", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_NONE, 4},
	// {"INDEX", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_INDEX, 4},
	// {"TIME", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_TIME, 8},
	// {"CODE", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_CODE, 8},
	{"STRING", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_STRING, 16},
	{"INT", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_INT, 4},
	{"UINT", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_UINT, 4},
	// {"FLOAT", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_FLOAT, 4},
	{"DOUBLE", SIS_MAP_DEFINE_FIELD_TYPE, SIS_FIELD_DOUBLE, 8},
	/////////////插入和修改方式定义/////////////
	{"NONE", SIS_MAP_DEFINE_OPTION_MODE, SIS_OPTION_NONE, 0},
	{"ALWAYS", SIS_MAP_DEFINE_OPTION_MODE, SIS_OPTION_ALWAYS, 0},
	{"SORT", SIS_MAP_DEFINE_OPTION_MODE, SIS_OPTION_SORT, 0},
	{"TIME", SIS_MAP_DEFINE_OPTION_MODE, SIS_OPTION_TIME, 0},
	{"VOL", SIS_MAP_DEFINE_OPTION_MODE, SIS_OPTION_VOL, 0},
	{"CODE", SIS_MAP_DEFINE_OPTION_MODE, SIS_OPTION_CODE, 0},
	/////////////数据类型定义/////////////
	{"NONE", SIS_MAP_DEFINE_SCALE, SIS_SCALE_NONE, 0},
	{"MSEC", SIS_MAP_DEFINE_SCALE, SIS_SCALE_MSEC, 0},
	{"SECOND", SIS_MAP_DEFINE_SCALE, SIS_SCALE_SECOND, 0},
	{"INDEX", SIS_MAP_DEFINE_SCALE, SIS_SCALE_INDEX, 0},
	{"MIN1", SIS_MAP_DEFINE_SCALE, SIS_SCALE_MIN1, 0},
	{"MIN5", SIS_MAP_DEFINE_SCALE, SIS_SCALE_MIN5, 0},
	{"MIN30", SIS_MAP_DEFINE_SCALE, SIS_SCALE_MIN30, 0},
	{"DAY", SIS_MAP_DEFINE_SCALE, SIS_SCALE_DAY, 0},
	{"MONTH", SIS_MAP_DEFINE_SCALE, SIS_SCALE_MONTH, 0},
	/////////////存入数据的方法定义/////////////
	{"COVER", SIS_MAP_DEFINE_FIELD_METHOD, SIS_FIELD_METHOD_COVER, 0},
	{"MIN", SIS_MAP_DEFINE_FIELD_METHOD, SIS_FIELD_METHOD_MIN, 0},
	{"MAX", SIS_MAP_DEFINE_FIELD_METHOD, SIS_FIELD_METHOD_MAX, 0},
	{"INIT", SIS_MAP_DEFINE_FIELD_METHOD, SIS_FIELD_METHOD_INIT, 0},
	{"INCR", SIS_MAP_DEFINE_FIELD_METHOD, SIS_FIELD_METHOD_INCR, 0},
	/////////////压缩类型定义/////////////
	// { "0", SIS_MAP_DEFINE_ZIP_MODE, SIS_FIELD_NONE, 0 },
	// { "UP", SIS_MAP_DEFINE_ZIP_MODE, SIS_FIELD_INDEX, 0 },
	// { "LOCAL", SIS_MAP_DEFINE_ZIP_MODE, SIS_FIELD_NONE, 0 },
	// { "MULTI", SIS_MAP_DEFINE_ZIP_MODE, SIS_FIELD_SECOND, 0 },
	/////////////编码定义/////////////
	// { "SRC", SIS_ENCODEING_SRC, 0 },
	// { "ROW", SIS_ENCODEING_ROW, 0 },
	// { "COL", SIS_ENCODEING_COL, 0 },
	// { "ALL", SIS_ENCODEING_ALL, 0 },
	// { "STR", SIS_ENCODEING_STR, 0 },
	// { "COD", SIS_ENCODEING_COD, 0 },
	/////////////数据类型定义/////////////
	{"ZIP", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_ZIP, 0},
	{"STRUCT", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_STRUCT, 0},
	{"STRING", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_STRING, 0},
	{"JSON", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_JSON, 0},
	{"CSV", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_CSV, 0},
	{"ARRAY", SIS_MAP_DEFINE_DATA_TYPE, SIS_DATA_ARRAY, 0}};

void _init_map_define(s_sis_map_pointer *fields_)
{
	sis_map_pointer_clear(fields_);
	int nums = sizeof(_sis_map_defines) / sizeof(struct s_sis_map_define);

	for (int i = 0; i < nums; i++)
	{
		s_sis_sds key = sis_sdsnew(_sis_map_defines[i].key);
		key = sdscatfmt(key, ".%u", _sis_map_defines[i].style);
		int rtn = sis_dict_add(fields_, key, &_sis_map_defines[i]);
		assert(rtn == DICT_OK);
	}
}

s_sis_db *sis_db_create(char *name_) //数据库的名称，为空建立一个sys的数据库名
{
	s_sis_db *db = sis_malloc(sizeof(s_sis_db));
	memset(db, 0, sizeof(s_sis_db));
	db->name = sis_sdsnew(name_);
	db->db = sis_map_pointer_create();

	db->trade_time = sis_struct_list_create(sizeof(s_sis_time_pair), NULL, 0);
	db->save_plans = sis_struct_list_create(sizeof(uint16), NULL, 0);

	db->map = sis_map_pointer_create();

	_init_map_define(db->map);
	return db;
}

void sis_db_destroy(s_sis_db *db_) //关闭一个数据库
{
	if (!db_)
	{
		return;
	}
	// 遍历字典中table，手动释放实际的table
	if (db_->db)
	{
		s_sis_dict_entry *de;
		s_sis_dict_iter *di = sis_dict_get_iter(db_->db);
		while ((de = sis_dict_next(di)) != NULL)
		{
			s_sis_table *val = (s_sis_table *)sis_dict_getval(de);
			sis_table_destroy(val);
		}
		sis_dict_iter_free(di);
		// 如果没有sis_map_pointer_destroy就必须加下面这句
		// sis_map_buffer_clear(db_->db);
	}
	// 下面仅仅释放key
	sis_map_pointer_destroy(db_->db);

	sis_sdsfree(db_->name);
	sis_sdsfree(db_->conf);
	sis_map_pointer_destroy(db_->map);
	sis_struct_list_destroy(db_->trade_time);
	sis_struct_list_destroy(db_->save_plans);

	sis_free(db_);
}

s_sis_sds sis_db_get_table_info_sds(s_sis_db *db_)
{
	s_sis_sds list = sis_sdsempty();
	if (db_->db)
	{
		s_sis_dict_entry *de;
		s_sis_dict_iter *di = sis_dict_get_iter(db_->db);
		while ((de = sis_dict_next(di)) != NULL)
		{
			s_sis_table *val = (s_sis_table *)sis_dict_getval(de);
			list = sdscatprintf(list, "  %-10s : fields=%2d, collects=%lu\n",
								val->name,
								sis_string_list_getsize(val->field_name),
								sis_map_buffer_getsize(val->collect_map));
		}
	}

	return list;
}

s_sis_map_define *sis_db_find_map_define(s_sis_db *db_, const char *name_, uint8 style_)
{
	if (!name_)
	{
		return NULL;
	}
	s_sis_sds key = sis_sdsnew(name_);
	key = sdscatfmt(key, ".%u", style_);
	s_sis_map_define *val = (s_sis_map_define *)sis_dict_fetch_value(db_->map, key);
	sis_sdsfree(key);
	// printf("%s,%p\n",name_,val);
	return val;
}

int sis_db_find_map_uid(s_sis_db *db_, const char *name_, uint8 style_)
{
	s_sis_map_define *map = sis_db_find_map_define(db_, name_, style_);
	if (!map)
	{
		return 0;
	}
	return map->uid;
}