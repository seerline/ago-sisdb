
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
	
	db->cfg_infos = sis_pointer_list_create();
	db->cfg_infos->free = sis_free_call;
	db->cfg_exchs = sis_map_pointer_create();

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
	sis_struct_list_destroy(db_->cfg_infos);
	// 下面释放市场信息
	if (db_->cfg_exchs)
	{
		s_sis_dict_entry *de;
		s_sis_dict_iter *di = sis_dict_get_iter(db_->cfg_exchs);
		while ((de = sis_dict_next(di)) != NULL)
		{
			s_sisdb_cfg_exch *val = (s_sisdb_cfg_exch *)sis_dict_getval(de);
			sis_free(val);
		}
		sis_dict_iter_free(di);
	}
	sis_map_pointer_destroy(db_->cfg_exchs);
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
	sis_map_pointer_destroy(db_->calls);

	sis_free(db_);
}

s_sisdb_cfg_exch *sisdb_config_create_exch(s_sis_db *db_, const char *code_)
{
	char market[3];
	sis_strncpy(market, 3, code_, 2);
	// printf("### %s %s\n",market,code_);
	// 先去公共区查有没有该市场的信息
	s_sisdb_cfg_exch *exch = sis_map_buffer_get(db_->cfg_exchs, market); 

	if(!exch) 
	{
		// 如果没有就创建一个，然后用默认配置填充
		exch = (s_sisdb_cfg_exch *)sis_malloc(sizeof(s_sisdb_cfg_exch));
		memset(exch, 0, sizeof(s_sisdb_cfg_exch));
		sis_map_buffer_set(db_->cfg_exchs, market, exch);

		s_sisdb_cfg_exch *first = sis_map_buffer_get(db_->cfg_exchs, SIS_DEFAULT_EXCH); 
		if (first)
		{
			memmove(exch, first, sizeof(s_sisdb_cfg_exch));
		}
	}
	// 再去对应的exch表更新到最新的数据，数据指针不变；
	char key[SIS_MAXLEN_KEY];
	sis_sprintf(key, SIS_MAXLEN_KEY, "%s.%s", market, SIS_TABLE_EXCH);
	s_sisdb_collect *collect = sisdb_get_collect(db_, key);

	if (collect)
	{
		sisdb_collect_load_exch(collect, exch);
	}	
	return exch;
}
s_sisdb_cfg_info *sisdb_config_create_info(s_sis_db *db_, const char *code_)
{
	s_sisdb_cfg_info *info = (s_sisdb_cfg_info *)sis_malloc(sizeof(s_sisdb_cfg_info));
	memset(info, 0, sizeof(s_sisdb_cfg_info));

	// 取出默认值，当系统最初加载的时候就已经生成最少一条默认的值了
	s_sisdb_cfg_info *first = sis_struct_list_first(db_->cfg_infos);
	if (first)
	{
		memmove(info, first, sizeof(s_sisdb_cfg_info));
	}	

	// 去exch和info表中找找数据，有匹配的就更新info中对应字段
	char key[SIS_MAXLEN_KEY];
	sis_sprintf(key, SIS_MAXLEN_KEY, "%s.%s", code_, SIS_TABLE_INFO);
	s_sisdb_collect *collect = sisdb_get_collect(db_, key);
	if (collect)
	{
		sisdb_collect_load_info(collect, info);
	}
	// 第一条为默认配置,在列表中寻找有没有一样的，有就直接添加索引，没有需要在列表中增加一条
	// 最后再把信息注册到db中，方便后续查询
	for (int i = 1; i < db_->cfg_infos->count; i++)
	{
		s_sisdb_cfg_info *val = sis_struct_list_get(db_->cfg_infos, i);
		if (!memcmp(val, info, sizeof(s_sisdb_cfg_info)))
		{
			sis_free(info);
			return val;
		}
	}
	sis_struct_list_push(db_->cfg_infos, info);
	return info;
}

// 修改后，检查config的相关性
void sisdb_write_config(s_sis_db *db_, const char *key_, void *src_)
{
	s_sisdb_collect *collect_ = (s_sisdb_collect *)src_;
	// 只要不是修改具备config的数据表就不检查
	if(!sis_strcasecmp(SIS_TABLE_EXCH, collect_->db->name)) 
	{
		if(sisdb_collect_load_exch(collect_, collect_->cfg_exch))
		{
			// 直接修改完成，什么也不做
		}
	}
	if(!sis_strcasecmp(SIS_TABLE_INFO, collect_->db->name)) 
	{
		s_sisdb_cfg_info info;
		if(sisdb_collect_load_info(collect_, &info))
		{
			char key[SIS_MAXLEN_KEY];
			char code[SIS_MAXLEN_CODE];
			sis_str_substr(code, SIS_MAXLEN_CODE, key_, '.', 0);

			s_sis_dict_entry *de;
			s_sis_dict_iter *di = sis_dict_get_iter(db_->dbs);
			while ((de = sis_dict_next(di)) != NULL)
			{
				s_sisdb_table *table = (s_sisdb_table *)sis_dict_getval(de);
				if(!sis_strcasecmp(SIS_TABLE_EXCH, table->name)) continue; // 对股票不需要设置市场的变量
				sis_sprintf(key, SIS_MAXLEN_KEY, "%s.%s", code, table->name);
				s_sisdb_collect *collect = sisdb_get_collect(db_,key);
				if (!collect) continue;  // 没找到对应collect就返回
				if (!memcmp(collect->cfg_info, &info, sizeof(s_sisdb_cfg_info))) continue; // 结构体内容相同返回
				//----增加，不修改，重启后才会清理----//
				int finded = false;
				for (int i = 1; i < db_->cfg_infos->count; i++)
				{	
					s_sisdb_cfg_info *ninfo = sis_struct_list_get(db_->cfg_infos, i);
					if (!memcmp(ninfo, &info, sizeof(s_sisdb_cfg_info)))
					{
						collect->cfg_info = ninfo;
						finded = true;
						break;
					}
				}
				if (!finded) 
				{
					s_sisdb_cfg_info *ninfo = (s_sisdb_cfg_info *)sis_malloc(sizeof(s_sisdb_cfg_info));
					memmove(ninfo, &info, sizeof(s_sisdb_cfg_info));
					sis_struct_list_push(db_->cfg_infos, ninfo);
					collect->cfg_info = ninfo;
				}
			}
			sis_dict_iter_free(di);
		}
	}
}
//time_t 返回０－２３９
// 不允许返回-1
uint16 sisdb_ttime_to_trade_index_(uint64 ttime_, s_sis_struct_list *tradetime_)
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

uint64 sisdb_trade_index_to_ttime_(int date_, int idx_, s_sis_struct_list *tradetime_)
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

uint16 sisdb_ttime_to_trade_index(uint64 ttime_, s_sisdb_cfg_exch *cfg_)
{
	int tt = sis_time_get_iminute(ttime_);

	int nowmin = 0;
	int out = 0;

	for (int i = 0; i < cfg_->trade_slot; i++)
	{
		if (tt > cfg_->trade_time[i].first && 
			tt < cfg_->trade_time[i].second) //9:31:00--11:29:59  13:01:00--14:59:59
		{
			out = nowmin + sis_time_get_iminute_offset_i(cfg_->trade_time[i].first, tt);
			break;
		}
		if (tt <= cfg_->trade_time[i].first && i == 0)
		{ // 8:00:00---9:30:59秒前都=0
			return 0;
		}
		if (tt <= cfg_->trade_time[i].first && 
		   (tt > sis_time_get_iminute_minnum(cfg_->trade_time[i].first, -5)))
		{ //12:55:59--13:00:59秒
			return nowmin;
		}

		nowmin += sis_time_get_iminute_offset_i(
						cfg_->trade_time[i].first, 
						cfg_->trade_time[i].second);

		if (tt >= cfg_->trade_time[i].second && i == cfg_->trade_slot - 1)
		{ //15:00:00秒后
			return nowmin - 1;
		}
		// if (tt >= pair->second && (tt < sis_time_get_iminute_minnum(pair->second, 5)))
		// { //11:30:00--11:34:59秒
		// 	return nowmin - 1;
		// }
		if (tt >= cfg_->trade_time[i].second)
		{ //11:30:00--11:34:59秒
			if (tt < sis_time_get_iminute_minnum(cfg_->trade_time[i].second, 5))
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

uint64 sisdb_trade_index_to_ttime(int date_, int idx_, s_sisdb_cfg_exch *cfg_)
{
	int tt = idx_;
	int nowmin = 0;

	uint64 out = sis_time_make_time(date_, cfg_->trade_time[cfg_->trade_slot-1].second * 100);
	for (int i = 0; i < cfg_->trade_slot; i++)
	{
		nowmin = sis_time_get_iminute_offset_i(
			cfg_->trade_time[i].first,
		    cfg_->trade_time[i].second);
		if (tt < nowmin)
		{
			int min = sis_time_get_iminute_minnum(cfg_->trade_time[i].first, tt + 1);
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
