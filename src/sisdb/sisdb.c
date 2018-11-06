

#include "sisdb.h"
#include "sisdb_map.h"
#include "sisdb_table.h"

s_sis_db *sisdb_create(char *name_) //数据库的名称，为空建立一个sys的数据库名
{
	s_sis_db *db = sis_malloc(sizeof(s_sis_db));
	memset(db, 0, sizeof(s_sis_db));
	db->name = sis_sdsnew(name_);
	db->dbs = sis_map_pointer_create();

	db->map = sis_map_pointer_create();
	sisdb_init_map_define(db->map);

	db->save_plans = sis_struct_list_create(sizeof(uint16), NULL, 0);

	return db;
}

void sisdb_destroy(s_sis_db *db_) //关闭一个数据库
{
	if (!db_)
	{
		return;
	}
	// 遍历字典中table，手动释放实际的table
	if (db_->dbs)
	{
		s_sis_dict_entry *de;
		s_sis_dict_iter *di = sis_dict_get_iter(db_->dbs);
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
	sis_map_pointer_destroy(db_->dbs);

	sis_sdsfree(db_->name);
	sis_sdsfree(db_->conf);
	sis_map_pointer_destroy(db_->map);
	sis_struct_list_destroy(db_->save_plans);

	sis_free(db_);
}
	// db->trade_time = sis_struct_list_create(sizeof(s_sis_time_pair), NULL, 0);
	// sis_struct_list_destroy(db_->trade_time);
