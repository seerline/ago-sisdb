
#include "sisdb.h"
#include "sisdb_map.h"
#include "sisdb_fields.h"
#include "sisdb_table.h"
#include "sisdb_collect.h"
#include "sisdb_call.h"

s_sis_db *sisdb_create(char *name_) //数据库的名称，为空建立一个sys的数据库名
{
	s_sis_db *db = sis_malloc(sizeof(s_sis_db));
	memset(db, 0, sizeof(s_sis_db));
	db->name = sis_sdsnew(name_);

	db->dbs = sis_map_pointer_create();
	
	db->sys_infos = sis_pointer_list_create();
	db->sys_infos->free = sis_free_call;
	db->sys_exchs = sis_map_pointer_create();

	db->collects = sis_map_pointer_create();

	db->map = sis_map_pointer_create();
	sisdb_init_map_define(db->map);

	db->calls = sis_map_pointer_create();
	sisdb_init_call_define(db->calls);

	db->save_task = sis_plan_task_create();
	db->init_task = sis_plan_task_create();

	return db;
}

void sisdb_destroy(s_sis_db *db_) //关闭一个数据库
{
	if (!db_)
	{
		return;
	}
	sis_plan_task_destroy(db_->save_task);
	sis_plan_task_destroy(db_->init_task);

	// 遍历字典中table，手动释放实际的table
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(db_->dbs);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sisdb_table *val = (s_sisdb_table *)sis_dict_getval(de);
		sisdb_table_destroy(val);
	}
	sis_dict_iter_free(di);
	// 如果没有sis_map_pointer_destroy就必须加下面这句
	// sis_map_buffer_clear(db_->db);
	// 下面仅仅释放key
	sis_map_pointer_destroy(db_->dbs);

	// 下面释放股票信息
	sis_struct_list_destroy(db_->sys_infos);
	// 下面释放市场信息
	if (db_->sys_exchs)
	{
		s_sis_dict_entry *de;
		s_sis_dict_iter *di = sis_dict_get_iter(db_->sys_exchs);
		while ((de = sis_dict_next(di)) != NULL)
		{
			s_sisdb_sys_exch *val = (s_sisdb_sys_exch *)sis_dict_getval(de);
			sis_free(val);
		}
		sis_dict_iter_free(di);
	}
	sis_map_pointer_destroy(db_->sys_exchs);
	// 遍历字典中table，手动释放实际的table
	if (db_->collects)
	{
		s_sis_dict_entry *de;
		s_sis_dict_iter *di = sis_dict_get_iter(db_->collects);
		while ((de = sis_dict_next(di)) != NULL)
		{
			s_sisdb_collect *val = (s_sisdb_collect *)sis_dict_getval(de);
			sisdb_collect_destroy(val);
		}
		sis_dict_iter_free(di);
	}
	// 下面仅仅释放key
	sis_map_pointer_destroy(db_->collects);

	sis_sdsfree(db_->name);
	sis_json_delete_node(db_->conf);
	sis_map_pointer_destroy(db_->map);
	sis_map_pointer_destroy(db_->calls);

	sis_free(db_);
}


int sis_from_node_get_format(s_sis_db *db_, s_sis_json_node *node_)
{
	int o = SIS_DATA_TYPE_JSON;
	s_sis_json_node *format = sis_json_cmp_child_node(node_, "format");
	if (format)
	{
		s_sis_map_define *smd = sisdb_find_map_define(db_->map, format->value, SIS_MAP_DEFINE_DATA_TYPE);
		if (smd)
		{
			o = smd->uid;
		}
	}
	return o;
}
