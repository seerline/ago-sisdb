

#include <sisdb_fmap.h>
#include <sisdb_io.h>
#include <sisdb.h>


///////////////////////////////////////////////////////////////////////////
//====================================s_sisdb_fmap_unit ================================================//
///////////////////////////////////////////////////////////////////////////

s_sisdb_fmap_unit *sisdb_fmap_unit_create(s_sis_object *kname_, s_sis_object *sname_, int ktype_, s_sis_dynamic_db *sdb_)
{
	if ((ktype_ == SIS_SDB_STYLE_SDB || ktype_ == SIS_SDB_STYLE_NON) && !sdb_)
	{
		return NULL;
	}
	s_sisdb_fmap_unit *o = SIS_MALLOC(s_sisdb_fmap_unit, o);

	if (kname_)
	{
		o->kname = sis_object_incr(kname_);
	}
	if (sname_)
	{
		o->sname = sis_object_incr(sname_);
	}
	o->ktype = ktype_;
	switch (ktype_)
	{
	case SIS_SDB_STYLE_ONE:
		{
			o->value = sis_sdsempty();
		}		
		break;
	case SIS_SDB_STYLE_MUL:
		{
			o->value = sis_node_create();
		}
		break;
	default:
		{
			sis_dynamic_db_incr(sdb_);
			o->sdb = sdb_;
			// if (!o->sdb->field_time && !o->sdb->field_mindex && o->ktype != SIS_SDB_STYLE_NON)
			if (!o->sdb->field_mindex  && o->ktype != SIS_SDB_STYLE_NON)
			{
				// 这里需要判断如果没有时间和索引字段强制 转换类型
				o->ktype = SIS_SDB_STYLE_NON;
			}
			o->scale = sis_disk_get_sdb_scale(o->sdb);
			o->value = sis_struct_list_create(sdb_->size);			
			o->fidxs = sis_struct_list_create(sizeof(s_sisdb_fmap_idx));
		}
		break;
	}
	return o;	
}
void sisdb_fmap_unit_destroy(void *unit_)
{
	s_sisdb_fmap_unit *o = (s_sisdb_fmap_unit *)unit_;
	if (o->value)
	{
		switch (o->ktype)
		{
		case SIS_SDB_STYLE_ONE:
			sis_sdsfree(o->value);
			break;
		case SIS_SDB_STYLE_MUL:
			{
				s_sis_node *node = (s_sis_node *)o->value;
				s_sis_node *next = sis_node_get(node, 0);
				while(next)
				{
					sis_sdsfree((s_sis_sds)next->value);
					next = sis_node_next(next);
				}
				sis_node_destroy(node);
			}
			break;
		default:
			sis_dynamic_db_destroy(o->sdb);
			sis_struct_list_destroy(o->fidxs);
			sis_struct_list_destroy(o->value);
			break;
		}
	}
	sis_object_decr(o->kname);
	sis_object_decr(o->sname);
	sis_free(o);
}
void sisdb_fmap_unit_clear(s_sisdb_fmap_unit *unit_)
{
	// 只清理数据相关信息 键值和属性不变
	switch (unit_->ktype)
	{
	case SIS_SDB_STYLE_ONE:
		sis_sdsclear(unit_->value);
		break;
	case SIS_SDB_STYLE_MUL:
		{
			s_sis_node *node = (s_sis_node *)unit_->value;
			s_sis_node *next = sis_node_get(node, 0);
			while (next)
			{
				sis_sdsfree((s_sis_sds)next->value);
				next = sis_node_next(next);
			}
			sis_node_clear(node);
		}
		break;
	default:
		sis_struct_list_clear(unit_->fidxs);
		sis_struct_list_clear(unit_->value);
		sis_struct_list_pack(unit_->fidxs);
		sis_struct_list_pack(unit_->value);
		break;
	}
	unit_->reads = 0;
	unit_->rmsec = 0;
	unit_->moved = 0;
	unit_->writed = 0;
	unit_->start = 0;
	unit_->stop = 0;
	unit_->step = 0;

}

void sisdb_fmap_unit_reidx(s_sisdb_fmap_unit *unit_)
{
	if (!unit_->sdb || !unit_->sdb->field_mindex)
	{
		return ;
	}
	s_sis_struct_list *slist = (s_sis_struct_list *)unit_->value;
	unit_->start = sisdb_fmap_unit_get_mindex(unit_, 0);
	unit_->stop  = sisdb_fmap_unit_get_mindex(unit_, slist->count - 1);
	unit_->step = 0.0;
	if (slist->count > 0)
	{
		unit_->step = (unit_->stop - unit_->start) / slist->count;
	}	
}
msec_t sisdb_fmap_unit_get_mindex(s_sisdb_fmap_unit *unit_, int index_)
{
	return sis_dynamic_db_get_mindex(unit_->sdb, index_, 
		sis_struct_list_first((s_sis_struct_list *)unit_->value), 
		((s_sis_struct_list *)unit_->value)->count * ((s_sis_struct_list *)unit_->value)->len);
}
int sisdb_fmap_unit_get_start(s_sisdb_fmap_unit *unit_, msec_t mindex_)
{
	if (unit_->sdb->field_time == unit_->sdb->field_mindex)
	{
		return sis_time_unit_convert(unit_->sdb->field_time->style, SIS_DYNAMIC_TYPE_DATE, mindex_);
	}
	else
	{
		return mindex_;
	}
}

int sisdb_fmap_unit_count(s_sisdb_fmap_unit *unit_)
{
	int o = 0;
	if (unit_)
	{
		switch (unit_->ktype)
		{
		case SIS_SDB_STYLE_ONE:
			o = 1;
			break;
		case SIS_SDB_STYLE_MUL:
			o = sis_node_get_size((s_sis_node *)unit_->value);
			break;
		default:
			o = ((s_sis_struct_list *)(unit_->value))->count;
			break;
		}
	}
	return o;
}

int sisdb_fmap_unit_goto(s_sisdb_fmap_unit *unit_, msec_t curr_)
{
	s_sis_struct_list *slist = (s_sis_struct_list *)unit_->value;
	if (slist->count < 1)
	{
		return -1;
	}
	if (curr_ <= unit_->start)
	{
		return 0;
	}
	if (curr_ >= unit_->stop)
	{
		return slist->count - 1;
	}
	// printf("goto %f\n", unit_->step);
	int index = 0;
	if (unit_->step > 1.000001)
	{
		index = (int)((curr_ - unit_->start) / unit_->step);
	}
	if (index > slist->count - 1)
	{
		return slist->count - 1;
	}
	return index;	
}
// 找到一个最接近的前置位置
// 如果返回时 = -1 表示比第一个记录小
// 如果返回时 >= count 表示比最后记录大
// 如果返回时 0 .. count - 1 表示插入位置 
int sisdb_fmap_cmp_find_head(s_sisdb_fmap_unit *unit_, msec_t  start_)
{
	int count = sisdb_fmap_unit_count(unit_);
	int index = sisdb_fmap_unit_goto(unit_, start_);
	if (index < 0)
	{
		return count;
	}
	int i = index;
	int dir = 0;
	while (i >= 0 && i < count)
	{
		msec_t ts = sisdb_fmap_unit_get_mindex(unit_, i);
		if (start_ > ts)
		{
			if (dir == -1)
			{
				return i + 1;
			}
			dir = 1;
			i += dir;
		}
		else if (start_ < ts)
		{
			if (dir == 1)
			{
				return i;
			}
			dir = -1;
			i += dir;
		}
		else
		{
			for (int k = i; k >= 0; k--)
			{
				msec_t kts = sisdb_fmap_unit_get_mindex(unit_, k);
				if (start_ != kts) 
				{
					return k + 1;
				}
			}
			return 0;
		}
	}
	return dir > 0 ? count : -1;
}
// 找到一个最接近的后置位置
// 如果返回时 = -1 表示比第一个记录小
// 如果返回时 >= count 表示比最后记录大
// 如果返回时 0 .. count - 1 表示插入位置 
int sisdb_fmap_cmp_find_tail(s_sisdb_fmap_unit *unit_, msec_t  start_)
{
	int count = sisdb_fmap_unit_count(unit_);
	int index = sisdb_fmap_unit_goto(unit_, start_);
	if (index < 0)
	{
		return count;
	}
	int i = index;
	int dir = 0;
	while (i >= 0 && i < count)
	{
		msec_t ts = sisdb_fmap_unit_get_mindex(unit_, i);
		if (start_ > ts)
		{
			if (dir == -1)
			{
				return i + 1;
			}
			dir = 1;
			i += dir;
		}
		else if (start_ < ts)
		{
			if (dir == 1)
			{
				return i;
			}
			dir = -1;
			i += dir;
		}
		else
		{
			for (int k = i; k < count; k++)
			{
				msec_t kts = sisdb_fmap_unit_get_mindex(unit_, k);
				if (start_ != kts) 
				{
					return k - 1;
				}
			}
			return count;
		}
	}
	return dir > 0 ? count : -1;
}
// 必须找到一个相等值，找到就返回数量 没找到就返回 -1
// 如果返回时 oindex = -1 表示比第一个记录小
// 如果返回时 oindex >= count 表示比最后记录大
// ostart == 0 表示没有匹配 
int sisdb_fmap_cmp_same(s_sisdb_fmap_unit *unit_, msec_t  start_, s_sisdb_fmap_cmp *ans_)
{
	ans_->ocount = 0; ans_->oindex = -1; ans_->ostart = 0;

	int count = sisdb_fmap_unit_count(unit_);
	int index = sisdb_fmap_unit_goto(unit_, start_);
	if (index < 0)
	{
		ans_->oindex = count;
		return -1;
	}
	int i = index;
	int dir = 0;
	ans_->ocount = 0;
	ans_->oindex = -1;
	while (i >= 0 && i < count)
	{
		msec_t ts = sisdb_fmap_unit_get_mindex(unit_, i);
		// printf(":::%lld %lld |%d %d = %d %d %d\n", start_, ts, i, dir, ans_->ocount, ans_->oindex, ans_->ostart);
		if (start_ > ts)
		{
			if (ans_->ocount > 0)
			{
				return ans_->ocount;
			}
			if (dir == -1)
			{
				ans_->oindex = i + 1;
				return -1;
			}
			dir = 1;
			i += dir;
		}
		else if (start_ < ts)
		{
			if (ans_->ocount > 0)
			{
				return ans_->ocount;
			}
			if (dir == 1)
			{
				ans_->oindex = i;
				return -1;
			}
			dir = -1;
			i += dir;
		}
		else
		{
			if (ans_->ocount == 0)
			{
				if (dir == 0)
				{
					for (int k = i + 1; k < count; k++)
					{
						msec_t kts = sisdb_fmap_unit_get_mindex(unit_, k);
						if (start_ != kts) 
						{
							break;
						}
						ans_->ocount++;
					}
					dir = -1;
				}
				ans_->ostart = sisdb_fmap_unit_get_start(unit_, ts);
				ans_->oindex = i;
			}
			else
			{
				if (dir == -1) 
				{
					ans_->oindex = i;
				}
			}
			ans_->ocount ++;
			i += dir;
		}
	}
	if (ans_->ocount > 0)
	{
		return ans_->ocount;
	}
	else
	{
		if (i < 0)
		{
			ans_->oindex = -1;
		}
		else
		{
			ans_->oindex = count;
		}
	}
	return -1;
}
// 根据当前的搜索结果强制向前寻找记录
// offset_ 仅仅为偏移记录数 不做变量值比较
int sisdb_fmap_offset_prev(s_sisdb_fmap_unit *unit_, int  offset_, int count_, s_sisdb_fmap_cmp *ans_)
{
	// 此时 ans 有值
	if (ans_->oindex < 0 || ans_->oindex < -1 * offset_)
	{
		// 前面没有数据了
		return -1;
	}
	// printf("sss1 : %d %d %d\n", ans_->ocount, ans_->oindex, ans_->ostart);
	for (int i = 0; i > offset_; i--)
	{
		if (ans_->oindex == 0)
		{
			break;
		}
		ans_->oindex--; 
		ans_->ocount++;
	}
	// printf("sss2 : %d %lld\n", ans_->oindex, sisdb_fmap_unit_get_mindex(unit_, 235));
	// printf("sss2 : %d %lld\n", ans_->oindex, sisdb_fmap_unit_get_mindex(unit_, 234));
	// printf("sss2 : %d %lld\n", ans_->oindex, sisdb_fmap_unit_get_mindex(unit_, 233));
	// printf("sss2 : %d %lld\n", ans_->oindex, sisdb_fmap_unit_get_mindex(unit_, 237));
	// printf("sss2 : %d %lld\n", ans_->oindex, sisdb_fmap_unit_get_mindex(unit_, 236));
	ans_->ocount = sis_min(ans_->ocount, count_);
	ans_->ostart = sisdb_fmap_unit_get_mindex(unit_, ans_->oindex);
	// printf("sss3 : %d %d %d\n", ans_->ocount, ans_->oindex, ans_->ostart);
	return ans_->ocount;
}
// 默认如果设置了日期 无论如何只能得到之前或当前的数据
#define CMP_FIND_OK     0   // 定位到数据
#define CMP_FIND_AGO    1   // 没找到相等数据 返回数据为前置第一条 supply = 1 有效
#define CMP_HEAD_NONE  -1   // 比第一条还小 但后面有值 supply = 0 有效
#define CMP_TAIL_NONE  -2   // 比最后一条还大 但前面有值 supply = 0 有效
#define CMP_MIDD_NONE  -3   // 没找到相等数据 但前后都有值 supply = 0 有效

// 找到对应数据的第一条记录
int _fmap_cmp_range_head(s_sisdb_fmap_unit *unit_, msec_t mindex_, s_sisdb_fmap_cmp *cmp_)
{
	// 进入该函数 一定有数据
	int i = sisdb_fmap_unit_goto(unit_, mindex_);
	int dir = 0;
	int count = sisdb_fmap_unit_count(unit_);
	cmp_->ocount =  0;
	cmp_->oindex = -1;
	while (i >= 0 && i < count)
	{
		msec_t ts = sisdb_fmap_unit_get_mindex(unit_, i);
		// printf("dir = %d ts=%lld %lld count= %d\n", dir, ts, mindex_, cmp_->ocount);
		if (ts < mindex_)
		{
			if (cmp_->ocount > 0)
			{
				return CMP_FIND_OK;
			}
			if (dir == -1)
			{
				cmp_->oindex = i;
				return CMP_FIND_AGO;  // 返回最接近的前值
			}
			dir = 1;
			i += dir;
		}
		else if (ts > mindex_)
		{ 
			if (cmp_->ocount > 0)
			{
				return CMP_FIND_OK;
			}
			if (dir == 1)
			{
				cmp_->oindex = i - 1;
				return CMP_FIND_AGO; // 返回最接近的前值
			}
			dir = -1;
			i += dir;
		}
		else
		{
			cmp_->ocount ++;
			cmp_->oindex = i;
			if (dir == 1)
			{
				// 此循环计算相等数量
				// for (int k = i + 1; k < count; k++)
				// {
				// 	msec_t kts = sisdb_fmap_unit_get_mindex(unit_, k);
				// 	if (mindex_ != kts) 
				// 	{
				// 		break;
				// 	}
				// 	cmp_->ocount++;
				// }
				return CMP_FIND_OK;
			}
			dir = -1;
			i += dir;
		}
	}
	return cmp_->ocount > 0 ? CMP_FIND_OK : i < 0 ? CMP_HEAD_NONE : CMP_TAIL_NONE;
}
// 找到对应数据的最后一条记录 
int _fmap_cmp_range_tail(s_sisdb_fmap_unit *unit_, msec_t mindex_, s_sisdb_fmap_cmp *cmp_)
{
	// 进入该函数 一定有数据
	int i = sisdb_fmap_unit_goto(unit_, mindex_);
	int dir = 0;
	int count = sisdb_fmap_unit_count(unit_);
	cmp_->ocount =  0;
	cmp_->oindex = -1;
	while (i >= 0 && i < count)
	{
		msec_t ts = sisdb_fmap_unit_get_mindex(unit_, i);
		if (ts < mindex_)
		{
			if (cmp_->ocount > 0)
			{
				return CMP_FIND_OK;
			}
			if (dir == -1)
			{
				cmp_->oindex = i;
				return CMP_FIND_AGO;  // 返回最接近的前值
			}
			dir = 1;
			i += dir;
		}
		else if (ts > mindex_)
		{ 
			if (cmp_->ocount > 0)
			{
				return CMP_FIND_OK;
			}
			if (dir == 1)
			{
				cmp_->oindex = i - 1;
				return CMP_FIND_AGO; // 返回最接近的前值
			}
			dir = -1;
			i += dir;
		}
		else
		{
			cmp_->ocount ++;
			cmp_->oindex = i;
			if (dir == -1)
			{
				// 此循环计算相等数量
				// for (int k = i - 1; k >= 0; k--)
				// {
				// 	msec_t kts = sisdb_fmap_unit_get_mindex(unit_, k);
				// 	if (mindex_ != kts) 
				// 	{
				// 		break;
				// 	}
				// 	cmp_->ocount++;
				// }
				return CMP_FIND_OK;
			}
			dir = 1;
			i += dir;
		}
	}
	return cmp_->ocount > 0 ? CMP_FIND_OK : i < 0 ? CMP_HEAD_NONE : CMP_TAIL_NONE;
}
int sisdb_fmap_cmp_where(s_sisdb_fmap_unit *unit_, msec_t  start_, int  offset_, s_sisdb_fmap_cmp *ans_)
{
	int nums = sisdb_fmap_cmp_same(unit_, start_, ans_);
	LOG(8)("where :: %d %d %d\n", start_, offset_, nums);
	if (offset_ == 0)
	{
		return nums;
	}
	else if (offset_ < 0)
	{
		return sisdb_fmap_offset_prev(unit_, offset_, 1, ans_);
	}
	else
	{
		LOG(8)("where :::: offset [%lld] > 0\n", offset_);
	}
	return -1;
}
// -1 表示没有匹配数据 
int sisdb_fmap_cmp_range(s_sisdb_fmap_unit *unit_, int64 start_, int64 stop_, int8 ifprev_, s_sisdb_fmap_cmp *ans_)
{
	ans_->ocount = 0; ans_->oindex = -1; ans_->ostart = 0;
	int count = sisdb_fmap_unit_count(unit_);
	if (count < 1)
	{
		return -1;
	}
	if (start_ <= -1)
	{
		start_ = sisdb_fmap_unit_get_mindex(unit_, sis_max(0, count + start_));
	}
	if (stop_ <= -1)
	{
		stop_ = sisdb_fmap_unit_get_mindex(unit_, count - 1);
	}
	else if (stop_ == 0)
	{
		stop_ = start_;
	}
	s_sisdb_fmap_cmp cmphead = {0, -1, 0};
	int headidx = _fmap_cmp_range_head(unit_, start_, &cmphead);
	// printf("%d | %d %d %d\n", headidx, cmphead.ostart, cmphead.oindex, cmphead.ocount);
	switch (headidx)
	{
	case CMP_HEAD_NONE:
		cmphead.oindex = 0;
		break;
	case CMP_TAIL_NONE:
		if (ifprev_ == 0)
		{
			return -1;
		}
		else
		{
			cmphead.oindex = count - 1;
		}
		break;
	case CMP_FIND_AGO:
		if (ifprev_ == 0)
		{
			cmphead.oindex = sis_min(cmphead.oindex + 1, count - 1); 
		}	
		break;
	default:
		break;
	}

	s_sisdb_fmap_cmp cmptail = {0, -1, 0};
	int tailidx = _fmap_cmp_range_tail(unit_, stop_, &cmptail);
	// printf("%d | %d %d %d\n", tailidx, cmptail.ostart, cmptail.oindex, cmptail.ocount);
	if (tailidx == CMP_HEAD_NONE)
	{
		return -1;
	}
	if (tailidx == CMP_TAIL_NONE)
	{
		cmptail.oindex = count - 1;
	}

	if (cmphead.oindex < 0 || cmptail.oindex < 0 || cmphead.oindex > cmptail.oindex)
	{
		return -1;
	}
	msec_t mindex = sisdb_fmap_unit_get_mindex(unit_, cmphead.oindex);
	ans_->ostart = sisdb_fmap_unit_get_start(unit_, mindex);
	ans_->oindex = cmphead.oindex;
	ans_->ocount = cmptail.oindex - cmphead.oindex + 1;
	return ans_->oindex;
}

#if 0
// 这里测试 sisdb_fmap_cmp_range 和 sisdb_fmap_cmp_where
#pragma pack(push,1)
typedef struct s_date_data
{
	int32       date;     
	int32       newp;    
	char        name[4];  
} s_date_data;

typedef struct s_time_data
{
	int64       time;     
	int32       newp;    
	char        name[4];  
} s_time_data;
#pragma pack(pop)

static s_sisdb_fmap_idx _date_idxs[3] = {
	{2019,  0, 3, 0, 0}, 
	{2020,  3, 4, 0, 0}, 
	{2021,  7, 5, 0, 0}};

static s_date_data _date_datas[12] = {
	{20191010, 1, "111"}, 
	{20191110, 2, "111"}, 
	{20191210, 3, "111"}, 
	{20200110, 4, "111"}, 
	{20200210, 5, "111"}, 
	{20200310, 6, "111"}, 
	{20200310, 7, "111"}, 
	{20210315, 8, "111"},
	{20210320, 9, "111"},
	{20210320, 10, "111"},
	{20210410, 11, "111"},
	{20210410, 12, "111"}};

static s_sisdb_fmap_idx _time_idxs[3] = {
	{20210828,  0, 3, 0, 0}, 
	{20210829,  3, 4, 0, 0}, 
	{20210830,  7, 5, 0, 0}};

static s_time_data _time_datas[12] = {
	{1630087372600, 1, "111"}, // 20210828-020252
	{1630087572600, 2, "111"}, 
	{1630087672600, 3, "111"}, 
	{1630187272600, 4, "111"}, 
	{1630187372600, 5, "111"}, // 20210829-054932
	{1630187372600, 6, "111"}, 
	{1630187572600, 7, "111"}, 
	{1630287372600, 8, "111"}, // 20210830-093612
	{1630287372600, 9, "111"},
	{1630287372600, 10, "111"},
	{1630288372600, 11, "111"},
	{1630288372600, 12, "111"}};

int main1()
{
	const char *_timedb = "{timedb:{fields:{time:[T,8],newp:[I,4],name:[C,4]}}}";
	s_sis_conf_handle *timejson = sis_conf_load(_timedb, strlen(_timedb));
	s_sis_dynamic_db *timedb = sis_dynamic_db_create(sis_json_first_node(timejson->node));
	sis_conf_close(timejson);
	// 测试 timedb
	s_sisdb_fmap_unit *funit = sisdb_fmap_unit_create(NULL, NULL, SIS_SDB_STYLE_SDB, timedb);
	for (int i = 0; i < 3; i++)
	{
		sis_struct_list_push(funit->fidxs, &_time_idxs[i]);
	}
	s_sis_struct_list *slist = funit->value;
	for (int i = 0; i < 12; i++)
	{
		sis_struct_list_push(slist, &_time_datas[i]);
	}


	sisdb_fmap_unit_reidx(funit);

	s_sisdb_fmap_cmp ans = {0, -1, 0};
	{
		sisdb_fmap_cmp_where(funit, 1630087572600, -1, &ans);
		printf("same one  = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
	}
	{
		//  0       0    表示取最新的那一日数据 当日 或 当年最后一天记录 
		printf("====== test 0 0 ======\n");
		sisdb_fmap_cmp_range(funit, 0, 0, &ans);
		printf("range last one = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		// day1     0    表示取 day1 到 当日的所有数据 
		printf("====== test day1 0 ======\n");
		sisdb_fmap_cmp_range(funit, 1630187372600, 0, &ans);
		printf("range date mul = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		// day1    day2  表示取 day1 到 day2 的所有数据
		printf("====== test day1 day2 ======\n");
		sisdb_fmap_cmp_range(funit, 1630187272600, 1630287372600, &ans);
		printf("range day1-> 2 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		// day1    day1  表示取 == day1 的数据 可能有多条 也可能一条没有 没有的情况下返回 NULL 
		printf("====== test day1 day1 ======\n"); 
		sisdb_fmap_cmp_range(funit, 1630187272600, 1630187272600, &ans);
		printf("range day1-> 1 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);

		// day1    -1    表示取 == day1 的数据 如果没有匹配 用前一个有效日期数据返回 前面没有数据再返回 NULL 
		printf("====== test day1 -1 ======\n"); 
		sisdb_fmap_cmp_range(funit, 1630287372600, -1, &ans);
		printf("range day1->-1 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_range(funit, 1630187472600, -1, &ans);
		printf("range day1->-1 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
	}

	sisdb_fmap_unit_destroy(funit);
	return 0;
}

int main()
{
	const char *_datedb = "{datedb:{fields:{date:[D,4],newp:[I,4],name:[C,4]}}}";
	s_sis_conf_handle *datejson = sis_conf_load(_datedb, strlen(_datedb));
	s_sis_dynamic_db *datedb = sis_dynamic_db_create(sis_json_first_node(datejson->node));
	sis_conf_close(datejson);

	// 测试 datedb
	s_sisdb_fmap_unit *funit = sisdb_fmap_unit_create(NULL, NULL, SIS_SDB_STYLE_SDB, datedb);
	for (int i = 0; i < 3; i++)
	{
		sis_struct_list_push(funit->fidxs, &_date_idxs[i]);
	}
	s_sis_struct_list *slist = funit->value;
	for (int i = 0; i < 12; i++)
	{
		sis_struct_list_push(slist, &_date_datas[i]);
	}

	sisdb_fmap_unit_reidx(funit);

	s_sisdb_fmap_cmp ans = {0, -1, 0};
	{
		sisdb_fmap_cmp_where(funit, 20210315, 0, &ans);
		printf("same one  = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_where(funit, 20200310, 0, &ans);
		printf("same mul  = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_where(funit, 20200311, 0, &ans);
		printf("same none = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_where(funit, 20191009, 0, &ans);
		printf("same none = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_where(funit, 20210411, 0, &ans);
		printf("same none = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
	}
	{
		//  0       0    表示取最新的那一日数据 当日 或 当年最后一天记录 
		printf("====== test 0 0 ======\n");
		sisdb_fmap_cmp_range(funit, 0, 0, &ans);
		printf("range last one = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		// day1     0    表示取 day1 到 当日的所有数据 
		printf("====== test day1 0 ======\n");
		sisdb_fmap_cmp_range(funit, 20191009, 0, &ans);
		printf("range date mul = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_range(funit, 20210411, 0, &ans);
		printf("range date mul = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_range(funit, 20200310, 0, &ans);
		printf("range date mul = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		// day1    day2  表示取 day1 到 day2 的所有数据
		printf("====== test day1 day2 ======\n");
		sisdb_fmap_cmp_range(funit, 20200310, 20210320, &ans);
		printf("range day1 ->2 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_range(funit, 20200311, 20210411, &ans);
		printf("range day1 ->2 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_range(funit, 20181010, 20200310, &ans);
		printf("range day1 ->1 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_range(funit, 20200310, 20210411, &ans);
		printf("range day1 ->1 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		// day1    day1  表示取 == day1 的数据 可能有多条 也可能一条没有 没有的情况下返回 NULL 
		printf("====== test day1 day1 ======\n"); 
		sisdb_fmap_cmp_range(funit, 20200310, 20200310, &ans);
		printf("range day1 ->1 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_range(funit, 20200311, 20200311, &ans);
		printf("range day1 ->1 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_range(funit, 20191010, 20191010, &ans);
		printf("range day1 ->1 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_range(funit, 20181010, 20181010, &ans);
		printf("range day1 ->1 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_range(funit, 20210411, 20210411, &ans);
		printf("range day1 ->1 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);

		// day1    -1    表示取 == day1 的数据 如果没有匹配 用前一个有效日期数据返回 前面没有数据再返回 NULL 
		printf("====== test day1 -1 ======\n"); 
		sisdb_fmap_cmp_range(funit, 20200310, -1, &ans);
		printf("range day1->-1 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_range(funit, 20200311, -1, &ans);
		printf("range day1->-1 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_range(funit, 20210316, -1, &ans);
		printf("range day1->-1 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_range(funit, 20181010, -1, &ans);
		printf("range day1->-1 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
		sisdb_fmap_cmp_range(funit, 20210411, -1, &ans);
		printf("range day1->-1 = %d %d %d\n", ans.ostart, ans.oindex, ans.ocount);
	}

	sisdb_fmap_unit_destroy(funit);
	return 0;
}



#endif