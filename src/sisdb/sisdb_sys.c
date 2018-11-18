
#include <sisdb_sys.h>
#include <sisdb_call.h>
#include <sisdb_file.h>
#include <sisdb_fields.h>
#include <sisdb_collect.h>

////////////////
bool _sisdb_collect_load_exch(s_sisdb_collect *collect_, s_sisdb_sys_exch *exch_)
{
	s_sis_sds buffer = sisdb_collect_get_of_range_sds(collect_, 0, -1);
	if (!buffer)
	{
		return false;
	}
	size_t len = 0;
	const char *str;
	s_sis_json_handle *handle;

	exch_->status = sisdb_field_get_uint_from_key(collect_->db, "status", buffer);

	exch_->init_time = sisdb_field_get_uint_from_key(collect_->db, "init-time", buffer);
	exch_->init_date = sisdb_field_get_uint_from_key(collect_->db, "init-date", buffer);

	exch_->status = sisdb_field_get_uint_from_key(collect_->db, "status", buffer);

	str = sisdb_field_get_char_from_key(collect_->db, "market", buffer, &len);
	sis_strncpy(exch_->market, 3, str, len);

	str = sisdb_field_get_char_from_key(collect_->db, "work-time", buffer, &len);
	handle = sis_json_load(str, len);
	if (handle)
	{
		exch_->work_time.first = sis_json_get_int(handle->node, "0", 900);
		exch_->work_time.second = sis_json_get_int(handle->node, "1", 1530);
	}
	sis_json_close(handle);

	str = sisdb_field_get_char_from_key(collect_->db, "trade-time", buffer, &len);
	handle = sis_json_load(str, len);
	int index = 0;
	if (handle)
	{
		s_sis_json_node *next = sis_json_first_node(handle->node);
		while (next)
		{
			exch_->trade_time[index].first = sis_json_get_int(next, "0", 930);
			exch_->trade_time[index].second = sis_json_get_int(next, "1", 1500);
			index++;
			next = next->next;
		}
		exch_->trade_slot = index;
	}
	sis_json_close(handle);

	sis_sdsfree(buffer);
	return true;
}

bool _sisdb_collect_load_info(s_sisdb_collect *collect_, s_sisdb_sys_info *info_)
{
	s_sis_sds buffer = sisdb_collect_get_of_range_sds(collect_, 0, -1);
	if (!buffer)
	{
		return false;
	}
	info_->dot = sisdb_field_get_uint_from_key(collect_->db, "dot", buffer);
	info_->vol_unit = sisdb_field_get_uint_from_key(collect_->db, "vol-unit", buffer);
	info_->prc_unit = sisdb_field_get_uint_from_key(collect_->db, "prc-unit", buffer);
	sis_sdsfree(buffer);
	return true;
}

s_sisdb_sys_exch *sisdb_sys_create_exch(s_sis_db *db_, const char *code_)
{
	if (!db_->special) return NULL;

	char market[3];
	sis_strncpy(market, 3, code_, 2);
	// printf("### %s %s\n",market,code_);
	// 先去公共区查有没有该市场的信息
	s_sisdb_sys_exch *exch = sis_map_buffer_get(db_->sys_exchs, market);

	if (!exch)
	{
		// 如果没有就创建一个，然后用默认配置填充
		exch = (s_sisdb_sys_exch *)sis_malloc(sizeof(s_sisdb_sys_exch));
		memset(exch, 0, sizeof(s_sisdb_sys_exch));
		sis_map_buffer_set(db_->sys_exchs, market, exch);

		s_sisdb_sys_exch *first = sis_map_buffer_get(db_->sys_exchs, SIS_DEFAULT_EXCH);
		if (first)
		{
			memmove(exch, first, sizeof(s_sisdb_sys_exch));
		}
	}
	// 再去对应的exch表更新到最新的数据，数据指针不变；
	char key[SIS_MAXLEN_KEY];
	sis_sprintf(key, SIS_MAXLEN_KEY, "%s.%s", market, SIS_TABLE_EXCH);
	s_sisdb_collect *collect = sisdb_get_collect(db_, key);

	if (collect)
	{
		_sisdb_collect_load_exch(collect, exch);
	}
	return exch;
}
s_sisdb_sys_info *sisdb_sys_create_info(s_sis_db *db_, const char *code_)
{
	if (!db_->special) return NULL;

	s_sisdb_sys_info *info = (s_sisdb_sys_info *)sis_malloc(sizeof(s_sisdb_sys_info));
	memset(info, 0, sizeof(s_sisdb_sys_info));

	// 取出默认值，当系统最初加载的时候就已经生成最少一条默认的值了
	s_sisdb_sys_info *first = sis_struct_list_first(db_->sys_infos);
	if (first)
	{
		memmove(info, first, sizeof(s_sisdb_sys_info));
	}

	// 去exch和info表中找找数据，有匹配的就更新info中对应字段
	char key[SIS_MAXLEN_KEY];
	sis_sprintf(key, SIS_MAXLEN_KEY, "%s.%s", code_, SIS_TABLE_INFO);
	s_sisdb_collect *collect = sisdb_get_collect(db_, key);
	if (collect)
	{
		_sisdb_collect_load_info(collect, info);
	}
	// 第一条为默认配置,在列表中寻找有没有一样的，有就直接添加索引，没有需要在列表中增加一条
	// 最后再把信息注册到db中，方便后续查询
	for (int i = 1; i < db_->sys_infos->count; i++)
	{
		s_sisdb_sys_info *val = sis_struct_list_get(db_->sys_infos, i);
		if (!memcmp(val, info, sizeof(s_sisdb_sys_info)))
		{
			sis_free(info);
			return val;
		}
	}
	sis_struct_list_push(db_->sys_infos, info);
	return info;
}

void sisdb_sys_flush_work_time(void *collect)
{
	s_sisdb_collect *collect_ = (s_sisdb_collect *)collect;
	s_sisdb_sys_exch *exch = collect_->spec_exch;
	if (!exch)
	{
		return;
	}
	if (exch->status != SIS_MARKET_STATUS_INITING)
	{
		return;
	}
	if (!collect_->db->control.isinit)
	{
		// 来源数据并不需要初始化
		return;
	}
	s_sis_sds buffer = sisdb_collect_get_of_count_sds(collect_, -1, 1);
	// sisdb_collect_get_of_range_sds(collect_, 0, -1);
	if (!buffer)
	{
		return;
	}
	exch->init_time = sisdb_field_get_uint_from_key(collect_->db, "time", buffer);
	printf("work [%s]-- %d\n", exch->market, (int)exch->init_time);
	sis_sdsfree(buffer);
}
  
void sisdb_sys_load_default(s_sis_db *db_, s_sis_json_node *table_)
{
	if (!db_->special)
	{
		return;
	}
	s_sis_json_node *config = sis_json_cmp_child_node(table_, "default");
	if (!config)
	{
		return;
	}
	s_sisdb_sys_info *info = sis_struct_list_first(db_->sys_infos);
	if (!info)
	{
		info = (s_sisdb_sys_info *)sis_malloc(sizeof(s_sisdb_sys_info));
		sis_struct_list_push(db_->sys_infos, info);
	}
	// 默认的配置没有market信息
	memset(info, 0, sizeof(s_sisdb_sys_info));
	if (sis_json_cmp_child_node(config, "dot"))
	{
		info->dot = sis_json_get_int(config, "dot", 2);
	}
	if (sis_json_cmp_child_node(config, "prc-unit"))
	{
		info->prc_unit = sis_json_get_int(config, "prc-unit", 1);
	}
	if (sis_json_cmp_child_node(config, "vol-unit"))
	{
		info->vol_unit = sis_json_get_int(config, "vol-unit", 100);
	}

	s_sisdb_sys_exch *exch = sis_map_buffer_get(db_->sys_exchs, SIS_DEFAULT_EXCH);
	if (!exch)
	{
		exch = (s_sisdb_sys_exch *)sis_malloc(sizeof(s_sisdb_sys_exch));
		sis_map_buffer_set(db_->sys_exchs, SIS_DEFAULT_EXCH, exch);
	}
	s_sis_json_node *node = sis_json_cmp_child_node(config, "work-time");
	if (node)
	{
		exch->work_time.first = sis_json_get_int(node, "0", 900);
		exch->work_time.second = sis_json_get_int(node, "1", 1530);
	}
	node = sis_json_cmp_child_node(config, "trade-time");
	if (node)
	{
		int index = 0;
		s_sis_json_node *next = sis_json_first_node(node);
		while (next)
		{
			exch->trade_time[index].first = sis_json_get_int(next, "0", 930);
			exch->trade_time[index].second = sis_json_get_int(next, "1", 1130);
			index++;
			next = next->next;
		}
		exch->trade_slot = index;
	}
}

// 修改后，检查config的相关性
void sisdb_sys_check_write(s_sis_db *db_, const char *key_, void *src_)
{
	if (!db_->special) return;

	s_sisdb_collect *collect_ = (s_sisdb_collect *)src_;
	// 只要不是修改具备config的数据表就不检查
	if (!sis_strcasecmp(SIS_TABLE_EXCH, collect_->db->name))
	{
		if (_sisdb_collect_load_exch(collect_, collect_->spec_exch))
		{
			// 直接修改完成，什么也不做
		}
	}
	if (!sis_strcasecmp(SIS_TABLE_INFO, collect_->db->name))
	{
		s_sisdb_sys_info info;
		if (_sisdb_collect_load_info(collect_, &info))
		{
			char key[SIS_MAXLEN_KEY];
			char code[SIS_MAXLEN_CODE];
			sis_str_substr(code, SIS_MAXLEN_CODE, key_, '.', 0);

			s_sis_dict_entry *de;
			s_sis_dict_iter *di = sis_dict_get_iter(db_->dbs);
			while ((de = sis_dict_next(di)) != NULL)
			{
				s_sisdb_table *table = (s_sisdb_table *)sis_dict_getval(de);
				if (!sis_strcasecmp(SIS_TABLE_EXCH, table->name))
					continue; // 对股票不需要设置市场的变量
				sis_sprintf(key, SIS_MAXLEN_KEY, "%s.%s", code, table->name);
				s_sisdb_collect *collect = sisdb_get_collect(db_, key);
				if (!collect)
					continue; // 没找到对应collect就返回
				if (!memcmp(collect->spec_info, &info, sizeof(s_sisdb_sys_info)))
					continue; // 结构体内容相同返回
				//----增加，不修改，重启后才会清理----//
				int finded = false;
				for (int i = 1; i < db_->sys_infos->count; i++)
				{
					s_sisdb_sys_info *ninfo = sis_struct_list_get(db_->sys_infos, i);
					if (!memcmp(ninfo, &info, sizeof(s_sisdb_sys_info)))
					{
						collect->spec_info = ninfo;
						finded = true;
						break;
					}
				}
				if (!finded)
				{
					s_sisdb_sys_info *ninfo = (s_sisdb_sys_info *)sis_malloc(sizeof(s_sisdb_sys_info));
					memmove(ninfo, &info, sizeof(s_sisdb_sys_info));
					sis_struct_list_push(db_->sys_infos, ninfo);
					collect->spec_info = ninfo;
				}
			}
			sis_dict_iter_free(di);
		}
	}
}

void _sisdb_market_set_status(s_sisdb_collect *collect_, int status)
{
	// return ;
	// s_sisdb_sys_exch *exch = sis_map_buffer_get(collect_->db->father->sys_exchs, market);
	s_sisdb_sys_exch *exch = collect_->spec_exch;
	if (!exch)
	{
		return;
	}
	exch->status = (uint8)status;
	// count = 0 时传入数据可能会造成core
	s_sis_sds buffer = sisdb_collect_get_of_count_sds(collect_, -1, 1);
	if (!buffer)
	{
		return;
	}
	// printf("--- to me ..1.. \n");
	switch (status)
	{
	case SIS_MARKET_STATUS_INITING:
		exch->init_time = 0;
		// 即便是美国，获取的日期也是上一交易日的，
		// exch->init_date = sisdb_field_get_uint_from_key(collect_->db, "init-date", buffer);
		// 接收到now数据后就对new_time赋值，当发现new_time 日期大于交易日期就可以判定需要初始化
		sisdb_field_set_uint_from_key(collect_->db, "init-time", buffer, exch->init_time);
		break;
	case SIS_MARKET_STATUS_INITED:
		sisdb_field_set_uint_from_key(collect_->db, "init-time", buffer, exch->init_time);
		sisdb_field_set_uint_from_key(collect_->db, "init-date", buffer, sis_time_get_idate(exch->init_time));

		break;
	case SIS_MARKET_STATUS_CLOSE:
		// sisdb_field_set_uint_from_key(collect_->db, "close-date", buffer, sis_time_get_idate(0));
		sisdb_field_set_uint_from_key(collect_->db, "close-date", buffer, sis_time_get_idate(exch->init_time));
		break;
	default:
		break;
	}
	// printf("--- to me ..3.. \n");
	sisdb_field_set_uint_from_key(collect_->db, "status", buffer, status);

	char key[SIS_MAXLEN_KEY];
	sis_sprintf(key, SIS_MAXLEN_KEY, "%s.%s", exch->market, SIS_TABLE_EXCH);
	if (sisdb_write_begin(SIS_DATA_TYPE_STRUCT, key, buffer, sis_sdslen(buffer)) != SIS_SERVER_REPLY_ERR)
	{
		// 如果保存aof失败就返回错误
		sisdb_collect_update(collect_, buffer);
		sisdb_write_end();
	}

	sis_sdsfree(buffer);
}

bool _sisdb_market_start_init(s_sisdb_collect *collect_, const char *market_)
{
	// s_sisdb_sys_exch *exch = sis_map_buffer_get(db_->sys_exchs, market);
	s_sisdb_sys_exch *exch = collect_->spec_exch;
	if (!exch)
	{
		return false;
	}
	if (exch->status != SIS_MARKET_STATUS_INITING)
	{
		return false;
	}
	// printf("----%s | %s  %d %d %d \n", exch->market, market_, (int)exch->init_time, sis_time_get_idate(exch->init_time), exch->init-date);
	if (exch->init_time == 0)
	{
		return false;
	}
	int date = sis_time_get_idate(exch->init_time);
	if (date > exch->init_date)
	{
		return true;
	}
	return false;
}

void sisdb_market_work_init(s_sis_db *db_)
{
	if (!db_->special) return ;
	// _printf_info();

	s_sisdb_table *tb = sisdb_get_table(db_, SIS_TABLE_EXCH);
	int count = sis_string_list_getsize(tb->collect_list);

	char key[SIS_MAXLEN_KEY];
	s_sisdb_sys_exch exch;

	int min = sis_time_get_iminute(0);

	for (int k = 0; k < count; k++)
	{
		const char *market = sis_string_list_get(tb->collect_list, k);
		sis_sprintf(key, SIS_MAXLEN_KEY, "%s.%s", market, SIS_TABLE_EXCH);
		s_sisdb_collect *collect = sisdb_get_collect(db_, key);
		if (!collect)
		{
			continue;
		}
		// printf("%s===%s  %s\n", collect->spec_exch->market, market, key);
		if (!_sisdb_collect_load_exch(collect, &exch))
			continue;

		// printf("work-time=%d %d\n", exch.work_time.first, exch.work_time.second);
		int status = exch.status;
		if (exch.work_time.first == exch.work_time.second)
		{
			if (exch.work_time.first == min)
			{
				if (status != SIS_MARKET_STATUS_INITING)
				{
					_sisdb_market_set_status(collect, SIS_MARKET_STATUS_CLOSE);
					_sisdb_market_set_status(collect, SIS_MARKET_STATUS_INITING);
				}
				else
				{
					// 需要等待第一个有行情的股票来了数据才初始化完成
					if (_sisdb_market_start_init(collect, market))
					{
						sisdb_call_market_init(db_, market);
						// 初始化后应该存一次盘，或者清理掉所有该表的写入
						printf("init 2 %s\n", market);
						_sisdb_market_set_status(collect, SIS_MARKET_STATUS_INITED);
					}
				}
			}
		}
		else
		{
			if ((exch.work_time.first < exch.work_time.second && min > exch.work_time.first && min < exch.work_time.second) ||
				(exch.work_time.first > exch.work_time.second && (min > exch.work_time.first || min < exch.work_time.second)))
			{
				printf("start work: status %d [%s]\n", status, market);
				if (status == SIS_MARKET_STATUS_NOINIT || status == SIS_MARKET_STATUS_CLOSE)
				{
					_sisdb_market_set_status(collect, SIS_MARKET_STATUS_INITING);
				}
				else if (status == SIS_MARKET_STATUS_INITING)
				{
					// 需要等待第一个有行情的股票来了数据才初始化完成
					if (_sisdb_market_start_init(collect, market))
					{
						sisdb_call_market_init(db_, market);
						// 初始化后应该存一次盘，或者清理掉所有该表的写入
						printf("init 1 %s\n", market);
						_sisdb_market_set_status(collect, SIS_MARKET_STATUS_INITED);
					}
				}
			}
			else
			{
				if (status == SIS_MARKET_STATUS_INITED)
				{
					_sisdb_market_set_status(collect, SIS_MARKET_STATUS_CLOSE);
				}
			}
		}
	}
}

// //time_t 返回０－２３９
// // 不允许返回-1
// uint16 sisdb_ttime_to_trade_index_(uint64 ttime_, s_sis_struct_list *tradetime_)
// {
// 	int tt = sis_time_get_iminute(ttime_);

// 	int nowmin = 0;
// 	int out = 0;

// 	s_sis_time_pair *pair;
// 	for (int i = 0; i < tradetime_->count; i++)
// 	{
// 		pair = (s_sis_time_pair *)sis_struct_list_get(tradetime_, i);
// 		if (tt > pair->first && tt < pair->second) //9:31:00--11:29:59  13:01:00--14:59:59
// 		{
// 			out = nowmin + sis_time_get_iminute_offset_i(pair->first, tt);
// 			break;
// 		}
// 		if (tt <= pair->first && i == 0)
// 		{ // 8:00:00---9:30:59秒前都=0
// 			return 0;
// 		}
// 		if (tt <= pair->first && (tt > sis_time_get_iminute_minnum(pair->first, -5)))
// 		{ //12:55:59--13:00:59秒
// 			return nowmin;
// 		}

// 		nowmin += sis_time_get_iminute_offset_i(pair->first, pair->second);

// 		if (tt >= pair->second && i == tradetime_->count - 1)
// 		{ //15:00:00秒后
// 			return nowmin - 1;
// 		}
// 		// if (tt >= pair->second && (tt < sis_time_get_iminute_minnum(pair->second, 5)))
// 		// { //11:30:00--11:34:59秒
// 		// 	return nowmin - 1;
// 		// }
// 		if (tt >= pair->second)
// 		{ //11:30:00--11:34:59秒
// 			if (tt < sis_time_get_iminute_minnum(pair->second, 5))
// 			{
// 				return nowmin - 1;
// 			}
// 			else
// 			{
// 				out = nowmin;
// 			}
// 		}
// 	}
// 	return out;
// }

// uint64 sisdb_trade_index_to_ttime_(int date_, int idx_, s_sis_struct_list *tradetime_)
// {
// 	int tt = idx_;
// 	int nowmin = 0;
// 	s_sis_time_pair *pair = (s_sis_time_pair *)sis_struct_list_last(tradetime_);
// 	uint64 out = sis_time_make_time(date_, pair->second * 100);
// 	for (int i = 0; i < tradetime_->count; i++)
// 	{
// 		pair = (s_sis_time_pair *)sis_struct_list_get(tradetime_, i);
// 		nowmin = sis_time_get_iminute_offset_i(pair->first, pair->second);
// 		if (tt < nowmin)
// 		{
// 			int min = sis_time_get_iminute_minnum(pair->first, tt + 1);
// 			out = sis_time_make_time(date_, min * 100);
// 			break;
// 		}
// 		tt -= nowmin;
// 	}
// 	return out;
// }

uint16 sisdb_ttime_to_trade_index(uint64 ttime_, s_sisdb_sys_exch *cfg_)
{
	if (!cfg_)
		return 0;
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

uint64 sisdb_trade_index_to_ttime(int date_, int idx_, s_sisdb_sys_exch *cfg_)
{
	if (!cfg_)
		return 0;
	int tt = idx_;
	int nowmin = 0;

	uint64 out = sis_time_make_time(date_, cfg_->trade_time[cfg_->trade_slot - 1].second * 100);
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

bool sis_stock_cn_get_fullcode(const char *code_, char *fc_, size_t len)
{
	if (!sis_strcase_match("60", code_))
	{
		sis_sprintf(fc_, len, "SH%s", code_);
	}
	else if (!sis_strcase_match("00", code_) || !sis_strcase_match("300", code_))
	{
		sis_sprintf(fc_, len, "SZ%s", code_);
	}
	else
	{
		return false;
	}
	return true;
}

inline uint32 _sis_exright_get_vol(uint32 vol, s_sisdb_right *para_, int mode)
{
	if (mode == SIS_EXRIGHT_FORWORD)
	{
		return (uint32)(vol * para_->vol_factor / 10000);
	}
	else
	{
		return (uint32)(vol * 10000 / para_->vol_factor);
	}
}

inline uint32 _sis_exright_get_price(uint32 price, int unit_, s_sisdb_right *para_, int mode)
{
	int zoom = (unit_ == 0) ? 100 : (10000 / unit_);
	if (mode == SIS_EXRIGHT_FORWORD)
	{
		price = (uint32)((double)(price * zoom - para_->prc_factor) * 10000 / para_->vol_factor);
	}
	else
	{
		price = (uint32)((double)(price * zoom * para_->vol_factor) / 10000 + para_->prc_factor);
	}
	return (uint32)((double)price / zoom + 0.5);
}

bool _sis_isright(int start_, int stop_, int rightdate)
{
	int stop = stop_;
	int start = start_;
	if (start_ > stop_)
	{
		stop = start_;
		start = stop_;
	}
	if (rightdate > start && rightdate <= stop)
	{
		return true;
	}
	return false;
};
uint32 sis_stock_exright_vol(uint32 now_, uint32 stop_, uint32 vol_, s_sis_struct_list *rights_)
{
	int mode = SIS_EXRIGHT_FORWORD;
	if (stop_ < now_)
	{
		mode = SIS_EXRIGHT_BEHAND;
	}
	uint32 vol = vol_;
	s_sisdb_right *right_ps;

	for (int k = 0; k < rights_->count; k++)
	{
		right_ps = sis_struct_list_get(rights_, k);
		if (!_sis_isright(now_, stop_, right_ps->time))
			continue;
		vol = _sis_exright_get_vol(vol_, right_ps, mode);
	}
	return vol;
}
uint32 sis_stock_exright_price(uint32 now_, uint32 stop_, uint32 price_, int unit_, s_sis_struct_list *rights_)
{
	int mode = SIS_EXRIGHT_FORWORD;
	if (stop_ < now_)
	{
		mode = SIS_EXRIGHT_BEHAND;
	}
	uint32 price = price_;
	s_sisdb_right *right_ps;

	for (int k = 0; k < rights_->count; k++)
	{
		right_ps = sis_struct_list_get(rights_, k);
		if (!_sis_isright(now_, stop_, right_ps->time))
			continue;
		price = _sis_exright_get_price(price_, unit_, right_ps, mode); //open
	}
	return price;
}
