

#include <sisdb_collect.h>
#include <sisdb_io.h>
#include <sisdb.h>


//////////////////////////////////////////////////
//  delete
//////////////////////////////////////////////////
int _sisdb_collect_delete(s_sisdb_collect *collect_, int start_, int count_)
{
	sis_struct_list_delete(SIS_OBJ_LIST(collect_->obj), start_, count_);
	if (collect_->sdb->db->field_mindex)
	{
		sisdb_stepindex_rebuild(collect_->stepinfo,
								sisdb_collect_get_mindex(collect_, 0),
								sisdb_collect_get_mindex(collect_, SIS_OBJ_LIST(collect_->obj)->count - 1),
								SIS_OBJ_LIST(collect_->obj)->count);
	}
	return 0;
}
int sisdb_collect_delete_of_range(s_sisdb_collect *collect_, int start_, int stop_)
{
	bool o = sisdb_collect_trans_of_range(collect_, &start_, &stop_);
	if (!o)
	{
		return 0;
	}
	int count = (stop_ - start_) + 1;
	return _sisdb_collect_delete(collect_, start_, count);
}

int sisdb_collect_delete_of_count(s_sisdb_collect *collect_, int start_, int count_)
{
	bool o = sisdb_collect_trans_of_count(collect_, &start_, &count_);
	if (!o)
	{
		return 0;
	}
	return _sisdb_collect_delete(collect_, start_, count_);
}

int sisdb_collect_delete(s_sisdb_collect  *collect_, s_sis_json_node *jsql)
{
	// printf("%s: %d\n", __func__, SIS_OBJ_LIST(collect_->obj)->count);
	s_sis_json_node *search = sis_json_cmp_child_node(jsql, "search");
	if (!search)
	{
		return SIS_OBJ_LIST(collect_->obj)->count;
	}
	uint64 min, max;
	int start, stop;
	int count = 0;
	int out = 0;
	int minX, maxX;

	bool by_time = sis_json_cmp_child_node(search, "min") != NULL;
	bool by_region = sis_json_cmp_child_node(search, "start") != NULL;
	if (by_time)
	{
		min = sis_json_get_int(search, "min", 0);
		if (sis_json_cmp_child_node(search, "count"))
		{
			count = sis_json_get_int(search, "count", 1);
			start = sisdb_collect_search(collect_, min);
			if (start >= 0)
			{
				out = sisdb_collect_delete_of_count(collect_, start, count);
			}
		}
		else
		{
			if (sis_json_cmp_child_node(search, "max"))
			{
				max = sis_json_get_int(search, "max", 0);
				start = sisdb_collect_search_right(collect_, min, &minX);
				stop = sisdb_collect_search_left(collect_, max, &maxX);
				if (minX != SIS_SEARCH_NONE && maxX != SIS_SEARCH_NONE)
				{
					out = sisdb_collect_delete_of_range(collect_, start, stop);
				}
			}
			else
			{
				start = sisdb_collect_search(collect_, min);
				if (start >= 0)
				{
					out = sisdb_collect_delete_of_count(collect_, start, 1);
				}
			}
		}
		return out;
	}
	if (by_region)
	{
		start = sis_json_get_int(search, "start", -1); // -1 为最新一条记录
		if (sis_json_cmp_child_node(search, "count"))
		{
			count = sis_json_get_int(search, "count", 1);
			out = sisdb_collect_delete_of_count(collect_, start, count); // 定位后按数量删除
		}
		else
		{
			if (sis_json_cmp_child_node(search, "stop"))
			{
				stop = sis_json_get_int(search, "stop", -1);			   // -1 为最新一条记录
				out = sisdb_collect_delete_of_range(collect_, start, stop); // 定位后删除
			}
			else
			{
				out = sisdb_collect_delete_of_count(collect_, start, 1); // 定位后按数量删除
			}
		}
	}
	return out;
}

