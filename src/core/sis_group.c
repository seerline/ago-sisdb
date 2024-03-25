
#include "sis_math.h"
#include "sis_time.h"
#include "sis_group.h" 

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_groups ------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sis_groups *sis_groups_create(int nums, int style, void *vfree)
{
	s_sis_groups *o = SIS_MALLOC(s_sis_groups, o);
	o->nums  = nums;
	o->sorted = 0;
	o->zero_alone   = 1;
	o->zero_index   = -1;
	o->style   = style;
	o->vfree   = vfree;
	o->samples = sis_pointer_list_create();
	return o;
}
void _groups_free_sorted(s_sis_groups *groups)
{
	for (int i = 0; i < groups->count; i++)
	{
		sis_pointer_list_destroy(groups->vindexs[i]);
	}
	if (groups->sorts)
	{
		sis_free(groups->sorts);
		groups->sorts = NULL;
	}
	if (groups->vindexs)
	{
		sis_free(groups->vindexs);
		groups->vindexs = NULL;
	}
	groups->sorted = 0;
	groups->count = 0;
	groups->zero_index = -1;
}
void _groups_set_count(s_sis_groups *groups, int count)
{
	if (groups->sorted)
	{
		_groups_free_sorted(groups);
	}
	groups->count  = count;
	groups->sorts  = sis_malloc(sizeof(s_sis_groups_info) * count);
	memset(groups->sorts, 0, sizeof(s_sis_groups_info) * count);
	groups->vindexs = sis_malloc(sizeof(void *) * count);
	memset(groups->vindexs, 0, sizeof(void *) * count);
	for (int i = 0; i < count; i++)
	{
		groups->vindexs[i] = sis_pointer_list_create();
	}
}
void _groups_sample_zfree(s_sis_groups *groups)
{
	for (int i = 0; i < groups->samples->count; i++)
	{
		s_sis_groups_kv *gkv = sis_pointer_list_get(groups->samples, i);
		if (groups->vfree && gkv->value)
		{
			groups->vfree(gkv->value);
		}
		sis_free(gkv);
	}
}
void sis_groups_destroy(void *groups_)
{
	s_sis_groups *groups = (s_sis_groups *)groups_;
	_groups_sample_zfree(groups);
	sis_pointer_list_destroy(groups->samples);

	if (groups->sorted)
	{
		_groups_free_sorted(groups);
	}
	sis_free(groups);
}
void sis_groups_clear(s_sis_groups *groups)
{
	_groups_sample_zfree(groups);
	sis_pointer_list_clear(groups->samples);
	if (groups->sorted)
	{
		_groups_free_sorted(groups);
	}
}
////////////////////
double sis_groups_get_groupv(s_sis_groups *groups, int index)
{
	s_sis_groups_kv *gkv = sis_pointer_list_get(groups->samples, index);
	return gkv->groupv;
}
double sis_groups_get_placev(s_sis_groups *groups, int index)
{
	s_sis_groups_kv *gkv = sis_pointer_list_get(groups->samples, index);
	return gkv->placev;
}
int _groups_get_vindexs_index(s_sis_groups *groups, s_sis_groups_kv *gkv)
{
	int index = -1;
	for (int i = 0; i < groups->vindexs[gkv->gindex]->count; i++)
	{
		s_sis_groups_kv *midv = sis_pointer_list_get(groups->vindexs[gkv->gindex], i);
		if (midv->placev > gkv->placev)
		{
			index = i;
			break;
		}
		index = i + 1;
	}
	return index;
}
void _groups_insert_vindexs(s_sis_groups *groups, s_sis_groups_kv *gkv)
{
	int index = _groups_get_vindexs_index(groups, gkv);
	if (index < 0 || index > groups->vindexs[gkv->gindex]->count - 1)
	{
		sis_pointer_list_push(groups->vindexs[gkv->gindex], gkv);
	}
	else
	{
		sis_pointer_list_insert(groups->vindexs[gkv->gindex], index, gkv);
	}
}
void _groups_resort_vindexs(s_sis_groups *groups, s_sis_groups_kv *gkv)
{
	if (groups->zero_alone)
	{
		if (SIS_IS_ZERO(gkv->groupv))
		{
			gkv->gindex = groups->zero_index;
			_groups_insert_vindexs(groups, gkv);
			return ;
		}
	}
	// 升序
	for (int i = 0; i < groups->count; i++)
	{
		if (groups->zero_index == i)
		{
			continue;
		}
		if (i == 0 && gkv->groupv < groups->sorts[i].minv)
		{
			gkv->gindex = i;
			groups->sorts[i].minv = gkv->groupv - SIS_MINV;
			_groups_insert_vindexs(groups, gkv);
			break;
		}
		if (i == groups->count - 1 && gkv->groupv >= groups->sorts[i].maxv)
		{
			gkv->gindex = i;
			groups->sorts[i].maxv = gkv->groupv + SIS_MINV;
			_groups_insert_vindexs(groups, gkv);
			break;
		}
		if (gkv->groupv >= groups->sorts[i].minv && gkv->groupv < groups->sorts[i].maxv)
		{
			gkv->gindex = i;
			_groups_insert_vindexs(groups, gkv);
			break;
		}
	}
}
void sis_groups_change(s_sis_groups *groups, s_sis_groups_kv *gkv)
{
	if (groups->sorted)
	{
		if (gkv->gindex >= 0 && gkv->gindex < groups->count)
		{
			sis_pointer_list_find_and_delete(groups->vindexs[gkv->gindex], gkv);
			gkv->gindex = -1;
		}
		_groups_resort_vindexs(groups, gkv);
	}
	// 清理原始主序列顺序
	sis_pointer_list_find_and_delete(groups->samples, gkv);
	// 重新写入
	sis_groups_write(groups, gkv);
}

int sis_double_list_count_split_nosort(s_sis_double_list *list_, s_sis_struct_list *splits_, int nums_)
{
	sis_struct_list_clear(splits_);
	int count = list_->value->count;
	if (count < 1 || nums_ < 1)
	{
		return 0;
	}
	s_sis_double_split split;
	if (nums_ > count)
	{
		double minv = sis_double_list_get(list_, 0) - SIS_MINV;
		double maxv = sis_double_list_get(list_, list_->value->count - 1) + SIS_MINV;
		double step = (maxv - minv) / (double)nums_;
		double agov = minv;
		for (int i = 0; i < nums_; i++)
		{
			if (i == 0)
			{
				split.minv  = minv;
				split.maxv += step;
				agov = split.maxv;
			}
			else if (i == nums_ - 1)
			{
				split.minv = agov;
				split.maxv = maxv;
			}
			else
			{
				split.minv = agov;
				split.maxv += step;
				agov = split.maxv;
			}
			sis_struct_list_push(splits_, &split);
		}
	}
	else
	{
		double step = (double)count / (double)nums_;
		double agov = sis_double_list_get(list_, 0) - SIS_MINV;
		for (int i = 0; i < nums_; i++)
		{
			if (i == 0)
			{
				split.minv = sis_double_list_get(list_, 0) - SIS_MINV;
				split.maxv = sis_double_list_get(list_, (int)step);
				agov = split.maxv;
			}
			else if (i == nums_ - 1)
			{
				split.minv = agov;
				split.maxv = sis_double_list_get(list_, list_->value->count - 1) + SIS_MINV;
			}
			else
			{
				split.minv = agov;
				split.maxv = sis_double_list_get(list_, (int)((i + 1) * step));
				agov = split.maxv;
			}
			sis_struct_list_push(splits_, &split);
		}
	}
	return splits_->count;
}

void sis_groups_resort(s_sis_groups *groups)
{
	s_sis_double_list *uplist = sis_double_list_create();
	s_sis_double_list *dnlist = sis_double_list_create();
	for (int i = 0; i < groups->samples->count; i++)
	{
		s_sis_groups_kv *gkv = sis_pointer_list_get(groups->samples, i);
		if (gkv->groupv > 0.00000001)
		{
			sis_double_list_push(uplist, gkv->groupv);
		}
		if (gkv->groupv < -0.00000001)
		{
			sis_double_list_push(dnlist, gkv->groupv);
		}
	}
	int upnums = uplist->value->count < 1 ? 0 : groups->nums;
	int dnnums = dnlist->value->count < 1 ? 0 : groups->nums;
	// printf("== %d %d | %d %d\n", uplist->value->count, dnlist->value->count, upnums, dnnums);
	if ((dnnums + upnums) > groups->nums)
	{
		if (groups->nums >= 2)
		{
			upnums = uplist->value->count > dnlist->value->count ? groups->nums / 2 + groups->nums % 2 : groups->nums / 2;
			dnnums = dnlist->value->count > uplist->value->count ? groups->nums / 2 + groups->nums % 2 : groups->nums / 2;
		}
	}
	int count = upnums + dnnums;
	if (groups->zero_alone)
	{
		count += 1;
	}
	// printf("== %d %d | %d %d\n", uplist->value->count, dnlist->value->count, upnums, dnnums);

	_groups_set_count(groups, count);

	s_sis_struct_list *upsplits = sis_struct_list_create(sizeof(s_sis_double_split));
	s_sis_struct_list *dnsplits = sis_struct_list_create(sizeof(s_sis_double_split));
	switch (groups->style)
	{
	case 1:
		break;
	case 2:
		// sis_double_list_incr_split_nosort(slist, splits, groups->count - 1);
		break;
	case 3:
		// sis_double_list_incr_zero_pair_nosort(slist, splits, groups->count - 1);
		break;
	default:  // 0
		sis_double_list_count_split_nosort(uplist, upsplits, upnums);
		sis_double_list_count_split_nosort(dnlist, dnsplits, dnnums);
		break;
	}
	int k = 0;
	for (int i = 0; i < dnsplits->count; i++)
	{
		s_sis_double_split *split = sis_struct_list_get(dnsplits, i);
		groups->sorts[k].minv = split->minv;
		groups->sorts[k].maxv = split->maxv;
		if (k == dnsplits->count - 1)
		{
			groups->sorts[k].maxv = -1 * SIS_MINV;
		}
		k++;
	}
	if (groups->zero_alone)
	{
		groups->zero_index = k;
		k++;
	}
	for (int i = 0; i < upsplits->count; i++)
	{
		s_sis_double_split *split = sis_struct_list_get(upsplits, i);
		groups->sorts[k].minv = split->minv;
		groups->sorts[k].maxv = split->maxv;
		if (k == 0)
		{
			groups->sorts[k].minv = SIS_MINV;
		}
		k++;
	}
	// for (int i = 0; i < count; i++)
	// {
	// 	printf("== %d %.2f %.2f\n", i, groups->sorts[i].minv, groups->sorts[i].maxv);
	// }
	
	sis_struct_list_destroy(dnsplits);
	sis_struct_list_destroy(upsplits);
	sis_double_list_destroy(dnlist);
	sis_double_list_destroy(uplist);
	// 区间变量重新设置完毕
	for (int i = 0; i < groups->count; i++)
	{
		sis_pointer_list_clear(groups->vindexs[i]);
	}
	for (int k = 0; k < groups->samples->count; k++)
	{
		s_sis_groups_kv *gkv = sis_pointer_list_get(groups->samples, k);
		_groups_resort_vindexs(groups, gkv);
	}
	groups->sorted = 1;
}

int _groups_find_groupv(s_sis_groups *groups, double groupv_)
{
	int index = -1;
	for (int i = 0; i < groups->samples->count; i++)
	{
		s_sis_groups_kv *midv = sis_pointer_list_get(groups->samples, i);
		// 升序
		if (midv->groupv > groupv_)
		{
			return i;
		}
		index = i + 1;
	}
	return index;
}
int sis_groups_set(s_sis_groups *groups, double groupv_, double placev_, void *value_)
{
	s_sis_groups_kv *gkv = SIS_MALLOC(s_sis_groups_kv, gkv);
	gkv->gindex = -1;
	gkv->groupv = groupv_;
	gkv->placev = placev_;
	gkv->value  = value_;
	int index = sis_groups_write(groups, gkv);
	if (groups->sorted)
	{
		_groups_resort_vindexs(groups, gkv);
	}
	return index;
}
int  sis_groups_write(s_sis_groups *groups, s_sis_groups_kv *gkv)
{
	int index = _groups_find_groupv(groups, gkv->groupv);
	if (index < 0 || index > groups->samples->count - 1)
	{
		index = groups->samples->count;
		sis_pointer_list_push(groups->samples, gkv);
	}
	else
	{
		sis_pointer_list_insert(groups->samples, index, gkv);
	}
	// printf("add %p %p %d\n", gkv->value, groups, groups->samples->count);
	// sis_groups_change(groups, gkv);
	
	return index;
}
int sis_groups_find(s_sis_groups *groups, void *value_)
{
	int index = -1;
	for (int i = 0; i < groups->samples->count; i++)
	{
		s_sis_groups_kv *gkv = sis_pointer_list_get(groups->samples, i);
		// printf("find---- %d %p %p %p\n", i, value_, gkv->value, groups);
		if (gkv->value == value_)
		{
			return i;
		}
	}
	return index;
}
void sis_groups_delete_v(s_sis_groups *groups, void *value_)
{
	int index = sis_groups_find(groups, value_);
	if (index >= 0)
	{
		s_sis_groups_kv *gkv = sis_pointer_list_get(groups->samples, index);
		if (groups->vfree && gkv->value)
		{
			groups->vfree(gkv->value);
		}
		if (groups->sorted && gkv->gindex >= 0 && gkv->gindex < groups->count)
		{
			for (int k = 0; k < groups->vindexs[gkv->gindex]->count; k++)
			{
				void *skv = sis_pointer_list_get(groups->vindexs[gkv->gindex], k);
				if (gkv == skv)
				{
					sis_pointer_list_delete(groups->vindexs[gkv->gindex], k, 1);
					break ;
				}
			}	
		}
		// 最后再删除
		sis_pointer_list_delete(groups->samples, index, 1);
		sis_free(gkv);
	}
}
void sis_groups_remove_v(s_sis_groups *groups, void *value_)
{
	int index = sis_groups_find(groups, value_);
	// printf("remove---- %d %p\n", index, value_);
	if (index >= 0)
	{
		s_sis_groups_kv *gkv = sis_pointer_list_get(groups->samples, index);
		if (groups->sorted && gkv->gindex >= 0 && gkv->gindex < groups->count)
		{
			for (int k = 0; k < groups->vindexs[gkv->gindex]->count; k++)
			{
				void *skv = sis_pointer_list_get(groups->vindexs[gkv->gindex], k);
				if (gkv == skv)
				{
					sis_pointer_list_delete(groups->vindexs[gkv->gindex], k, 1);
					break ;
				}
			}	
		}
		sis_pointer_list_delete(groups->samples, index, 1);
		sis_free(gkv);
	}
}

s_sis_groups_kv *sis_groups_get_kv(s_sis_groups *groups, int index_)
{
	return sis_pointer_list_get(groups->samples, index_);
}
void *sis_groups_get(s_sis_groups *groups, int index_)
{
	s_sis_groups_kv *gkv = sis_pointer_list_get(groups->samples, index_);
	return gkv ? gkv->value : NULL;
}

int sis_groups_getsize(s_sis_groups *groups)
{
	return groups->samples->count;
}

s_sis_groups_kv *sis_group_get_kv(s_sis_groups *groups, int gindex_, int index_)
{
	s_sis_groups_kv *v = NULL;
	if (groups->sorted)
	{
		if (gindex_ >= 0 && gindex_ < groups->count)
		{
			if (groups->vindexs[gindex_]->count > 0)
			{
				v = sis_pointer_list_get(groups->vindexs[gindex_], index_);
			}
		}
	}
	else
	{
		v = sis_pointer_list_get(groups->samples, index_);
	}
	return v;
}
int sis_group_getsize(s_sis_groups *groups, int gindex_)
{
	int count = 0;
	if (groups->sorted)
	{
		if (gindex_ >= 0 && gindex_ < groups->count)
		{
			count = groups->vindexs[gindex_]->count;
		}
	}
	else
	{
		count = groups->samples->count;
	}
	return count;
}
int _groups_get_next_gindex(s_sis_groups *groups, s_sis_groups_iter *iter)
{
	int index = -1;
	if (iter->sortly == 1)
	{   // 升序
		int gindex = iter->gindex < 0 ? 0 : iter->gindex + 1;
		while(gindex < groups->count)
		{
			if (iter->skip0 && gindex == groups->zero_index)
			{
				gindex++;
				continue;
			}
			if (groups->vindexs[gindex]->count > 0)
			{
				index = gindex;
				break;
			}
			gindex++;
		}
	}
	else
	{
		// 降序
		int gindex = iter->gindex < 0 ?  groups->count - 1 : iter->gindex - 1;
		while(gindex >= 0)
		{
			if (iter->skip0 && gindex == groups->zero_index)
			{
				gindex--;
				continue;
			}
			if (groups->vindexs[gindex]->count > 0)
			{
				index = gindex;
				break;
			}
			gindex--;
		};
	}
	return index;
}
// 数组是 0 然后从小到大 其他都是从大到小
s_sis_groups_iter *sis_groups_iter_start(s_sis_groups *groups, int sortly, int skip0)
{
	if (groups->samples->count < 1)
	{
		return NULL;
	}
	groups->iter = SIS_MALLOC(s_sis_groups_iter, groups->iter);
	// s_sis_groups_iter iter = {0};
	groups->iter->skip0 = skip0;
	groups->iter->sortly = sortly;
	groups->iter->inited = 0;
	// iter.gindex = -1;
	// if (groups->sorted)
	// {
	// 	int gindex = _groups_get_next_gindex(groups, &iter);
	// 	if (gindex < 0)
	// 	{
	// 		return NULL;
	// 	}
	// 	else
	// 	{
	// 		iter.gindex = gindex;
	// 		iter.pindex = iter.sortly == 1 ? 0 : groups->vindexs[iter.gindex]->count - 1;
	// 	}
	// }
	// else
	// {
	// 	iter.gindex = -1;
	// 	iter.pindex = iter.sortly == 1 ? 0 : groups->samples->count - 1;
	// }
	// groups->iter = SIS_MALLOC(s_sis_groups_iter, groups->iter);
	// memmove(groups->iter, &iter, sizeof(s_sis_groups_iter));
	return groups->iter;
}


// 获取下一条数据 NULL 表示队列结束
s_sis_groups_iter *sis_groups_iter_next(s_sis_groups *groups)
{
	if (!groups->iter || groups->samples->count < 1)
	{
		return NULL;
	}
	s_sis_groups_iter *iter = groups->iter;
	if (iter->inited == 0)
	{
		iter->inited = 1;
		iter->gindex = -1;
		if (groups->sorted)
		{
			int gindex = _groups_get_next_gindex(groups, iter);
			if (gindex < 0)
			{
				return NULL;
			}
			else
			{
				iter->gindex = gindex;
				iter->pindex = iter->sortly == 1 ? 0 : groups->vindexs[iter->gindex]->count - 1;
			}
		}
		else
		{
			iter->gindex = -1;
			iter->pindex = iter->sortly == 1 ? 0 : groups->samples->count - 1;
		}
		return groups->iter;
	}
	if (groups->sorted)
	{
		if (iter->sortly == 1)  
		{
			// 升序
			if (iter->pindex < groups->vindexs[iter->gindex]->count - 1)
			{
				iter->pindex ++;
			}
			else
			{
				iter->gindex = _groups_get_next_gindex(groups, iter);
				if (iter->gindex < 0)
				{
					iter->gindex = -1;
					iter->pindex = -1;
					return NULL;
				}
				else
				{
					iter->pindex = 0;
				}
			}
		}
		else
		{
			// 降序
			if (iter->pindex > 0)
			{
				iter->pindex--;
			}
			else
			{
				iter->gindex = _groups_get_next_gindex(groups, iter);
				if (iter->gindex < 0)
				{
					iter->gindex = -1;
					iter->pindex = -1;
					return NULL;
				}
				else
				{
					iter->pindex = groups->vindexs[iter->gindex]->count - 1;
				}
			}
		}
	}
	else
	{
		if (iter->sortly == 1)
		{
			// 升序
			if (iter->pindex < groups->samples->count - 1)
			{
				iter->pindex ++;
			}
			else
			{
				iter->gindex = -1;
				iter->pindex = -1;
				return NULL;
			}
		}
		else
		{
			// 降序
			if (iter->pindex > 0)
			{
				iter->pindex -- ;
			}
			else
			{
				iter->gindex = -1;
				iter->pindex = -1;
				return NULL;
			}
		} 
	}
	return groups->iter;
}
void *sis_groups_iter_get_v(s_sis_groups *groups)
{
	if (!groups->iter || groups->samples->count < 1)
	{
		return NULL;
	}
	s_sis_groups_kv *gkv = sis_group_get_kv(groups, groups->iter->gindex, groups->iter->pindex);
	return gkv ? gkv->value : NULL;
}

s_sis_groups_kv *sis_groups_iter_get_kv(s_sis_groups *groups)
{
	if (!groups->iter || groups->samples->count < 1)
	{
		return NULL;
	}
	s_sis_groups_kv *gkv = sis_group_get_kv(groups, groups->iter->gindex, groups->iter->pindex);
	return gkv;
}

void sis_groups_iter_stop(s_sis_groups *groups)
{
	if (groups->iter)
	{
		sis_free(groups->iter);
	}
	groups->iter =  NULL;
}


///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_fgroup --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sis_fgroup *sis_fgroup_create(void *vfree_)
{
	s_sis_fgroup *o = SIS_MALLOC(s_sis_fgroup, o);
	o->maxcount = 0;
	o->count = 0;
	o->key = NULL;
	o->val = NULL;
	o->vfree = vfree_;
	return o;
}
void sis_fgroup_destroy(void *list_)
{
	s_sis_fgroup *list = (s_sis_fgroup *)list_;
	sis_fgroup_clear(list);
	if (list->key)
	{
		sis_free(list->key);
	}
	if (list->val)
	{
		sis_free(list->val);
	}
	sis_free(list);
}
void sis_fgroup_clear(s_sis_fgroup *list_)
{
	char **ptr = (char **)list_->val;
	for (int i = 0; i < list_->count; i++)
	{
		if (list_->vfree && ptr[i])
		{
			list_->vfree(ptr[i]);
			ptr[i] = NULL;
		}
	}
	list_->count = 0;
}
void _fgroup_grow(s_sis_fgroup *list_, int incr_)
{
	int newlen = list_->count + incr_;
	if (newlen <= list_->maxcount)
	{
		return;
	}
	int maxlen = newlen;
	if (newlen == 1)
	{
		maxlen = 1;
	}
	else if (newlen < 16)
	{
		maxlen = 16;
	}
	else if (newlen >= 16 && newlen < 256)
	{
		maxlen = 256;
	}
	else if (newlen >= 256 && newlen < 4096)
	{
		maxlen = 4096;
	}
	else
	{
		maxlen = newlen * 2;
	}
	double *newkey = sis_malloc(maxlen * sizeof(double));
	void  *newval = sis_malloc(maxlen * sizeof(char *));
	if (list_->val)
	{
		if (list_->count > 0)
		{
			memmove(newval, list_->val, list_->count * sizeof(char *));
		}
		sis_free(list_->val);
	}
	if (list_->key)
	{
		if (list_->count > 0)
		{
			memmove(newkey, list_->key, list_->count * sizeof(double));
		}
		sis_free(list_->key);
	}
	list_->key = newkey;
	list_->val = newval;
	list_->maxcount = maxlen;
}
int _fgroup_find(s_sis_fgroup *list_, int left, int right, double curv)
{
	while(left <= right) 
	{
        int mid = left + (right - left) / 2;

        // 检查中间元素
        if (list_->key[mid] == curv) 
		{
            return mid;
        }

        // 如果目标值比中间元素小，只需在右侧查找 
        if (list_->key[mid] > curv) 
		{
            left = mid + 1;
        }
        // 如果目标值比中间元素大，只需在左侧查找
        else 
		{
            right = mid - 1;
        }
    }
	return left;
}
int sis_fgroup_set(s_sis_fgroup *list_, double key_, void *in_)
{
	int index = _fgroup_find(list_, 0, list_->count - 1, key_);
	_fgroup_grow(list_, 1);
	if (index < 0 || index > list_->count - 1)
	{
		list_->key[list_->count] = key_;
		char **ptr = (char **)list_->val;
		ptr[list_->count] = (char *)in_;
		
		list_->count++;
		index = list_->count;
	}
	else
	{
		memmove(&list_->key[index + 1], &list_->key[index], (list_->count - index) * sizeof(double));
		memmove((char *)list_->val + ((index + 1) * sizeof(char *)), (char *)list_->val + (index * sizeof(char *)),
			(list_->count - index) * sizeof(char *));

		list_->key[index] = key_;
		char **ptr = (char **)list_->val;
		ptr[index] = (char *)in_;

		list_->count++;
	}
	
	return index;
}
int sis_fgroup_find(s_sis_fgroup *list_, void *value_)
{
	char **ptr = (char **)list_->val;
	for (int i = 0; i < list_->count; i++)
	{
		if (ptr[i]== value_)
		{
			return i;
		}
	}
	return -1;
}
void *sis_fgroup_get(s_sis_fgroup *list_, int index_)
{
	char *rtn = NULL;
	if (index_ >= 0 && index_ < list_->count)
	{
		char **ptr = (char **)list_->val;
		rtn = ptr[index_];
	}
	return rtn;
}
double sis_fgroup_getkey(s_sis_fgroup *list_, int index_)
{
	double rtn = 0.0;
	if (index_ >= 0 && index_ < list_->count)
	{
		rtn = list_->key[index_];
	}
	return rtn;
}
void sis_fgroup_del(s_sis_fgroup *list_, int index_)
{
	if (index_ < 0 || list_->count < 1 || index_ > list_->count - 1)
	{
		return ;
	}
	char **ptr = (char **)list_->val;
	if (list_->vfree && ptr[index_])
	{
		list_->vfree(ptr[index_]);
		ptr[index_] = NULL;
	}
	if (index_ < list_->count - 1)
	{
		memmove(&list_->key[index_], &list_->key[index_ + 1], (list_->count - index_ - 1) * sizeof(double));
		memmove((char *)list_->val + (index_ * sizeof(char *)), (char *)list_->val + ((index_ + 1 )* sizeof(char *)),
			(list_->count - index_ - 1) * sizeof(char *));
	}
	list_->count--;
}

void sis_fgroup_reset(s_sis_fgroup *list_, double key_, int index_)
{
	if (index_ < 0 || list_->count < 1 || index_ > list_->count - 1)
	{
		return ;
	}
	char **ptr = (char **)list_->val;
	char *mval = ptr[index_];
	if (index_ < list_->count - 1)
	{
		memmove(&list_->key[index_], &list_->key[index_ + 1], (list_->count - index_ - 1) * sizeof(double));
		memmove((char *)list_->val + (index_ * sizeof(char *)), (char *)list_->val + ((index_ + 1 )* sizeof(char *)),
			(list_->count - index_ - 1) * sizeof(char *));
	}
	list_->count--;
	sis_fgroup_set(list_, key_, mval);
}

int sis_fgroup_getsize(s_sis_fgroup *list_)
{
	return list_->count;
}
#if 0
void printf_group(s_sis_groups *g)
{
	printf("==0== count:%d style:%d sorted:%d | samples: %d  ===== \n", g->count, g->style, g->sorted, g->samples->count);
	for (int i = 0; i < g->count; i++)
	{
		printf("sorts: %10.4f %10.4f count = %d\n", g->sorts[i].minv, g->sorts[i].maxv, g->vindexs[i]->count); 
		for (int k = 0; k < g->vindexs[i]->count; k++)
		{
			s_sis_groups_kv *v = sis_pointer_list_get(g->vindexs[i], k);
			printf("[%d]: %d %10.4f %10.4f %s\n", k, v->gindex, v->groupv, v->placev, (char *)v->value); 
		}	
	}
}
void printf_group_1(s_sis_groups *g)
{
	printf("==1== count:%d style:%d sorted:%d | samples: %d  ===== \n", g->count, g->style, g->sorted, g->samples->count);
	for (int i = 0; i < g->samples->count; i++)
	{
		s_sis_groups_kv *v = sis_pointer_list_get(g->samples, i);
		printf("[%d]: %d %10.4f %10.4f %s\n", i, v->gindex, v->groupv, v->placev, (char *)v->value); 
	}
}
void printf_group_iter(s_sis_groups *g, int sortly, int skip0)
{
	printf("=======================%d %d================\n", sortly, skip0);
	s_sis_groups_iter *iter = sis_groups_iter_start(g, sortly, skip0);
	int k = 0;
	while((iter = sis_groups_iter_next(g)) != NULL)
	{
		s_sis_groups_kv *v = sis_groups_iter_get_kv(g);
		printf("[%d]: %d %10.4f %10.4f %s\n", k++, v->gindex, v->groupv, v->placev, (char *)v->value);
		// 获取下一条数据 NULL 表示队列结束
	}
	sis_groups_iter_stop(g);
}
int main()
{	
	safe_memory_start();
	s_sis_groups *g = sis_groups_create(1, 0, sis_free_call);

	double groupv[] = {10.22, 5.34, -2.23, -5.2, -0.0, 0.0,  1.22,  4.33, -3.2,  6.88}; 
	double placev[] = {1.22,  8.34,  2.31, -1.2, 2.01, 1.05, 3.22, -0.33,  1.2, -1.88}; 
	char *curr = NULL;
	for (int i = 0; i < 10; i++)
	{
		char *info = sis_malloc(10);
		sis_sprintf(info, 10, "I=%d", i);
		if (i == 5)
		{
			curr = info;
		}
		sis_groups_set(g, groupv[i], placev[i], info);
	}
	printf_group(g);
	printf_group_1(g);
	sis_groups_resort(g);
	printf_group(g);
	// printf_group_1(g);
	printf_group_iter(g, 0, 0);
	exit(0);

	int index = sis_groups_find(g, curr);
	s_sis_groups_kv *v = sis_groups_get_kv(g, index);
	v->groupv =  3.87;
	v->placev = -3.87;
	sis_groups_change(g, v);
	printf_group_1(g);


	sis_groups_resort(g);
	printf_group(g);
	printf_group_iter(g, 0, 0);

	v->groupv = 3.87;
	v->placev = -3.87;
	sis_groups_change(g, v);
	printf_group(g);
	printf_group_1(g);

	v->groupv = 0.0;
	v->placev = -1.87;
	sis_groups_change(g, v);
	printf_group(g);
	printf_group_1(g);

	sis_groups_delete_v(g, curr);
	printf_group(g);
	printf_group_1(g);

	{
		char *info = sis_malloc(10);
		sis_sprintf(info, 10, "I=111");
		sis_groups_set(g, -12.22, 1.38, info);
	}
	{
		char *info = sis_malloc(10);
		sis_sprintf(info, 10, "I=122");
		sis_groups_set(g, -1.11, 3.38, info);
	}
	printf_group(g);
	printf_group_1(g);

	printf("========\n");
	sis_groups_resort(g);
	printf_group(g);
	printf_group_1(g);
	printf_group_iter(g, 0, 0);
	printf_group_iter(g, 1, 0);
	printf_group_iter(g, 0, 1);
	printf_group_iter(g, 1, 1);
	
	sis_groups_clear(g);
	// printf_group(g);
	// safe_memory_stop();
	sis_groups_destroy(g);
	safe_memory_stop();
	return 0;
}


#endif