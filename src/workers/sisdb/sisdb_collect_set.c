

#include <sisdb_collect.h>
#include <sisdb_io.h>
#include <sisdb.h>

////////////////////////
//  update
////////////////////////

// 最后一个匹配的时间
int _sisdb_collect_search_from_last(s_sisdb_collect *collect_, uint64 finder_, int *mode_)
{
	s_sisdb_table *tb = collect_->sdb;
	for (int i = SIS_OBJ_LIST(collect_->obj)->count - 1; i >= 0 ; i--)
	{
		const char *val = sis_struct_list_get(SIS_OBJ_LIST(collect_->obj), i);
		uint64 mindex = sis_dynamic_db_get_mindex(tb->db, 0, (void *)val, tb->db->size);
		if (mindex < finder_)
		{
			*mode_ = SIS_SEARCH_RIGHT;
			return i + 1;
		}
		if (mindex == finder_)
		{
			*mode_ = SIS_SEARCH_OK;
			return i;
		}
	}
	*mode_ = SIS_SEARCH_NONE;
	return 0;
}
// 检查是否增加记录，只和最后一条记录做比较，返回4个，一是当日最新记录，一是新记录，一是老记录, 一个为当前记录
int _sisdb_collect_check_lasttime(s_sisdb_collect *collect_, uint64 finder_)
{
	if (SIS_OBJ_LIST(collect_->obj)->count < 1)
	{
		return SIS_CHECK_LASTTIME_NEW;
	}
	s_sisdb_table *tb = collect_->sdb;

	uint64 old_index = sis_dynamic_db_get_mindex(tb->db, 0, sis_struct_list_last(SIS_OBJ_LIST(collect_->obj)), tb->db->size);

	if (finder_ == old_index)
	{
		return SIS_CHECK_LASTTIME_OK;
	}
	else if (finder_ > old_index)
	{
		return SIS_CHECK_LASTTIME_NEW;
	}
	// if (finder_ < last_time)
	return SIS_CHECK_LASTTIME_OLD;
}

// 没索引 但需要检验唯一性
int _sisdb_collect_update_solely(s_sisdb_collect *collect_, const char *in_, int index_, uint64 mindex_)
{
	s_sisdb_table *tb = collect_->sdb;
	int start = SIS_OBJ_LIST(collect_->obj)->count - 1;
	if (index_ >= 0)
	{
		start = index_;
	}
	for (int i = start; i >= 0 ; i--)
	{
		const char *val = sis_struct_list_get(SIS_OBJ_LIST(collect_->obj), i);
		if (mindex_ > 0)
		{
			uint64 mindex = sis_dynamic_db_get_mindex(tb->db, 0, (void *)val, tb->db->size);
			if (mindex < mindex_)
			{
				if (index_ == -1)
				{
					sis_struct_list_push(SIS_OBJ_LIST(collect_->obj), (void *)in_);
				}
				else
				{
					sis_struct_list_insert(SIS_OBJ_LIST(collect_->obj), index_ + 1, (void *)in_);
				}
				return 1;
			}
		}
		bool ok = true;
		for (int k = 0; k < tb->db->field_solely->count; k++)
		{
			s_sisdb_field *fu = (s_sisdb_field *)sis_pointer_list_get(tb->db->field_solely, k);
			// sis_out_binary("in :", in_+fu->offset, fu->flags.len);
			// sis_out_binary("val:", val+fu->offset, fu->flags.len);
			if (memcmp(in_ + fu->offset, val + fu->offset, fu->len))
			{
				ok = false;
				break;
			}
		}
		// printf("ok=%d\n", ok);
		if (ok) //  所有字段都相同
		{
			sis_struct_list_update(SIS_OBJ_LIST(collect_->obj), i, (void *)in_);
			return 0;
		}
	}
	sis_struct_list_push(SIS_OBJ_LIST(collect_->obj), (void *)in_);
	return 1;
}
// 有索引 索引默认为整数
int sisdb_collect_update_mindex(s_sisdb_collect *collect_, const char *in_)
{
	s_sisdb_table *tb = collect_->sdb;

	int o = 0;
	// 计算时序
	uint64 new_index = sis_dynamic_db_get_mindex(tb->db, 0, (void *)in_, tb->db->size);
	int search_mode = _sisdb_collect_check_lasttime(collect_, new_index);
	if (search_mode == SIS_CHECK_LASTTIME_NEW)
	{
		sis_struct_list_push(SIS_OBJ_LIST(collect_->obj), (void *)in_);
		o  = 1;
	}
	if (search_mode == SIS_CHECK_LASTTIME_OK)
	{
		if (tb->db->field_solely->count > 0)
		{
			o = _sisdb_collect_update_solely(collect_, in_, -1, new_index);
		}
		else
		{
			sis_struct_list_push(SIS_OBJ_LIST(collect_->obj), (void *)in_);
			o  = 1;				
		}
	}
	if (search_mode == SIS_CHECK_LASTTIME_OLD)
	{
		int set = SIS_SEARCH_NONE;
		int index = _sisdb_collect_search_from_last(collect_, new_index, &set);
		if (set == SIS_SEARCH_OK)
		{
			o = _sisdb_collect_update_solely(collect_, in_, index, new_index);
		}
		else // if (set == SIS_SEARCH_RIGHT)
		{
			sis_struct_list_insert(SIS_OBJ_LIST(collect_->obj), index, (void *)in_);
		}
	}
	return o;
}

// 没索引 但需要检验唯一性
int sisdb_collect_update_nomindex(s_sisdb_collect *collect_, const char *in_)
{
	return _sisdb_collect_update_solely(collect_, in_, -1, 0);
}

int sisdb_collect_update(s_sisdb_collect *collect_, s_sis_sds in_)
{
	int ilen = sis_sdslen(in_);

	// printf("-----len=%d:%d\n", ilen, collect_->sdb->db->size);
	if (ilen < collect_->sdb->db->size)
	{
		return 0;
	}

	int count = (int)(ilen / collect_->sdb->db->size);
	//这里应该判断数据完整性
	if (count * collect_->sdb->db->size != ilen)
	{
		LOG(3)("source format error [%d*%d!=%u]\n", count, collect_->sdb->db->size, ilen);
		return 0;
	}
	s_sisdb_table *tb = collect_->sdb;

	// 先判断是否可以直接插入
	if ((tb->db->field_solely->count == 0 && !tb->db->field_mindex) ||
		(SIS_OBJ_LIST(collect_->obj)->count < 1))
		// !tb->db->field_time field_mindex 为空则 field_time 一定为空
	{
		sis_struct_list_pushs(SIS_OBJ_LIST(collect_->obj), in_, count);
	}
	else
	{		
		const char *ptr = in_;
		for (int i = 0; i < count; i++)
		{
			if (tb->db->field_mindex)
			{
				sisdb_collect_update_mindex(collect_, ptr);	
			}
			else
			{
				sisdb_collect_update_nomindex(collect_, ptr);	
			}			
			ptr += collect_->sdb->db->size;
		}
	}
	if (collect_->sdb->db->field_mindex)
	{
		// 重建索引
		sisdb_stepindex_rebuild(collect_->stepinfo,
								sisdb_collect_get_mindex(collect_, 0),
								sisdb_collect_get_mindex(collect_, SIS_OBJ_LIST(collect_->obj)->count - 1),
								SIS_OBJ_LIST(collect_->obj)->count);
	}
	return count;
}
////////////////

// 从磁盘中整块写入，不逐条进行校验
int sisdb_collect_wpush(s_sisdb_collect *collect_, char *in_, size_t ilen_)
{
	if (ilen_ < collect_->sdb->db->size)
	{
		return 0;
	}
	int count = (int)(ilen_ / collect_->sdb->db->size);
	//这里应该判断数据完整性
	printf("wpush %s %s %d %p %d\n", collect_->name, collect_->sdb->db->name, collect_->sdb->db->size, collect_->sdb->db, count);
	if (count * collect_->sdb->db->size != ilen_)
	{
		LOG(3)("source format error [%d*%d!=%d]\n", count, collect_->sdb->db->size, ilen_);
		return 0;
	}
	sis_struct_list_pushs(SIS_OBJ_LIST(collect_->obj), in_, count);

	if (collect_->sdb->db->field_mindex)
	{
		sisdb_stepindex_rebuild(collect_->stepinfo,
								sisdb_collect_get_mindex(collect_, 0),
								sisdb_collect_get_mindex(collect_, SIS_OBJ_LIST(collect_->obj)->count - 1),
								SIS_OBJ_LIST(collect_->obj)->count);
	}
	return count;
}
