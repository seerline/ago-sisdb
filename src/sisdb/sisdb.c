

#include "sisdb.h"
#include "sisdb_map.h"
#include "sisdb_table.h"
#include "sisdb_collect.h"

s_sis_db *sisdb_create(char *name_) //数据库的名称，为空建立一个sys的数据库名
{
	s_sis_db *db = sis_malloc(sizeof(s_sis_db));
	memset(db, 0, sizeof(s_sis_db));
	db->name = sis_sdsnew(name_);

	db->dbs = sis_map_pointer_create();
	db->info = sis_pointer_list_create();
	db->info->free = sisdb_sysinfo_destroy;
	db->infos = sis_map_pointer_create();
	db->collects = sis_map_pointer_create();

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

	// 下面仅仅释放key
	sis_map_pointer_destroy(db_->infos);
	sis_struct_list_destroy(db_->info);
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
	sis_sdsfree(db_->conf);
	sis_map_pointer_destroy(db_->map);
	sis_struct_list_destroy(db_->save_plans);

	sis_free(db_);
}

s_sisdb_table *sisdb_get_db(s_sis_db *db_, const char *dbname_)
{
	s_sisdb_collect *val = NULL;
	if (db_->dbs)
	{
		s_sis_sds key = sis_sdsnew(dbname_);
		val = (s_sisdb_table *)sis_dict_fetch_value(db_->dbs, key);
		sis_sdsfree(key);
	}
	return val;
}
s_sisdb_table *sisdb_get_db_from_key(s_sis_db *db_, const char *key_)
{
	char db[SIS_MAXLEN_TABLE];
    sis_str_substr(db, SIS_MAXLEN_TABLE, key_, '.', 1);
	return sisdb_get_db(db_, db);
}

s_sisdb_sysinfo *sisdb_get_sysinfo(s_sis_db *db_, const char *key_)
{
	s_sisdb_sysinfo *val = NULL;
	if (db_->infos)
	{
		s_sis_sds key = sis_sdsnew(key_);
		val = (s_sisdb_sysinfo *)sis_dict_fetch_value(db_->infos, key);
		sis_sdsfree(key);
	}
	return val;
}
//获取股票的数据集合， 
s_sisdb_collect *sisdb_get_collect(s_sis_db *db_, const char *key_)
{
	s_sisdb_collect *val = NULL;
	if (db_->collects)
	{
		s_sis_sds key = sis_sdsnew(key_);
		val = (s_sisdb_collect *)sis_dict_fetch_value(db_->collects, key);
		sis_sdsfree(key);
	}
	return val;
}

// ----- //
s_sisdb_sysinfo *sisdb_sysinfo_create(s_sis_db *db_,const char *key_)
{
	s_sisdb_sysinfo *info = (s_sisdb_sysinfo *)sis_malloc(sizeof(s_sisdb_sysinfo));
	memset(info, 0 ,sizeof(s_sisdb_sysinfo));
	info->trade_time = sis_struct_list_create(sizeof(s_sis_time_pair), NULL, 0);

	info->dot = 2;
	info->prc_unit = 1;
	info->vol_unit = 1;
	info->work_time.first = 900;  
	info->work_time.second = 1530;

	s_sis_time_pair t; 
	t.first =  930, t.second = 1100;
	sis_struct_list_push(info->trade_time, &t);  
	t.first = 1300, t.second = 1500;
	sis_struct_list_push(info->trade_time, &t);  
	// 除了exch和info意以外的表才继续处理
	// 先根据key 找到info中的信息，如果找不到就用默认值
	// 再取key前两位，找到exch中的信息，如果找不到就用默认值，这里需要把字符串转换为二进制数据
	// 最后再把信息注册到db中，方便后续查询
	for (int i = 0; i < db_->info->count; i++)
	{
		s_sisdb_sysinfo *val = sis_struct_list_get(db_->info, i);
		if (!sisdb_sysinfo_compare(val, info))
		{
			sisdb_sysinfo_destroy(info);
			sis_map_buffer_set(db_->infos, key_, val);
			return val;
		}
	}
	sis_pointer_list_push(db_->info, info);
	sis_map_buffer_set(db_->infos, key_, info);
	return info;
}
	
bool sisdb_sysinfo_compare(s_sisdb_sysinfo *info1_,s_sisdb_sysinfo *info2_)
{
	bool o = 1;
	
	if (info1_->dot!=info2_->dot) return o;
	if (info1_->prc_unit!=info2_->prc_unit) return o;
	if (info1_->vol_unit!=info2_->vol_unit) return o;
	if (info1_->work_time.first!=info2_->work_time.first||
		info1_->work_time.second!=info2_->work_time.second) return o;
	
	if (info1_->trade_time->count!=info2_->trade_time->count||
		info1_->trade_time->len!=info2_->trade_time->len)  return o;

	for (int i = 0; i < info1_->trade_time->count; i++)
	{
		s_sis_time_pair *tt1 = sis_struct_list_get(info1_, i);
		s_sis_time_pair *tt2 = sis_struct_list_get(info2_, i);
		if (tt1->first!=tt2->first||tt1->second!=tt2->second)
		{
			return o;
		}
	}
	return 0;
}
void sisdb_sysinfo_destroy(void *info_)
{
	s_sisdb_sysinfo *info = (s_sisdb_sysinfo *)info_;

	sis_struct_list_destroy(info->trade_time);

	sis_free(info);
}

//time_t 返回０－２３９
// 不允许返回-1
uint16 sisdb_ttime_to_trade_index(uint64 ttime_, s_sis_struct_list *tradetime_)
{
	int tt = sis_time_get_iminute(ttime_);

	int nowmin = 0;
	int out = 0;

	s_sis_time_pair *pair;
	for (int i = 0; i < tradetime_->count; i++)
	{
		pair = (s_sis_time_pair *)sis_struct_list_get(tradetime_, i);
		if (tt > pair->first && tt < pair->second) //9:31:00--11:29:59  13:01:00--14:59:59
		{
			out = nowmin + sis_time_get_iminute_offset_i(pair->first, tt);
			break;
		}
		if (tt <= pair->first && i == 0)
		{ // 8:00:00---9:30:59秒前都=0
			return 0;
		}
		if (tt <= pair->first && (tt > sis_time_get_iminute_minnum(pair->first, -5)))
		{ //12:55:59--13:00:59秒
			return nowmin;
		}

		nowmin += sis_time_get_iminute_offset_i(pair->first, pair->second);

		if (tt >= pair->second && i == tradetime_->count - 1)
		{ //15:00:00秒后
			return nowmin - 1;
		}
		// if (tt >= pair->second && (tt < sis_time_get_iminute_minnum(pair->second, 5)))
		// { //11:30:00--11:34:59秒
		// 	return nowmin - 1;
		// }
		if (tt >= pair->second)
		{ //11:30:00--11:34:59秒
			if (tt < sis_time_get_iminute_minnum(pair->second, 5))
			{
				return nowmin - 1;
			}
			else
			{
				out = nowmin;
			}
		}
	}
	return out;
}

uint64 sisdb_trade_index_to_ttime(int date_, int idx_, s_sis_struct_list *tradetime_)
{
	int tt = idx_;
	int nowmin = 0;
	s_sis_time_pair *pair = (s_sis_time_pair *)sis_struct_list_last(tradetime_);
	uint64 out = sis_time_make_time(date_, pair->second * 100);
	for (int i = 0; i < tradetime_->count; i++)
	{
		pair = (s_sis_time_pair *)sis_struct_list_get(tradetime_, i);
		nowmin = sis_time_get_iminute_offset_i(pair->first, pair->second);
		if (tt < nowmin)
		{
			int min = sis_time_get_iminute_minnum(pair->first, tt + 1);
			out = sis_time_make_time(date_, min * 100);
			break;
		}
		tt -= nowmin;
	}
	return out;
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

