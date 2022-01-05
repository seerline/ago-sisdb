

#include <sisdb_fmap.h>
#include <sisdb_io.h>
#include <sisdb.h>


// 仅仅增加一条 这里 idate 是日期
int sisdb_fmap_unit_add_idx(s_sisdb_fmap_unit *unit_, int startidx, int idate)
{
	int isign = unit_->scale == SIS_SDB_SCALE_YEAR ? idate / 10000 : idate;
	int start = (startidx < 0 || startidx >= unit_->fidxs->count) ? 0 : startidx;
	int isidx = 0;  // 1 表示已经有索引
	int count = 0;
	for (int i = start; i < unit_->fidxs->count; i++)
	{
		s_sisdb_fmap_idx *fidx = sis_struct_list_get(unit_->fidxs, i);
		if (isidx != 0)
		{
			fidx->start++;
		}
		else
		{
			if (isign == fidx->isign)
			{
				fidx->count++;
				fidx->moved = 0;
				fidx->writed = 1;
				startidx = i;
				isidx = 1;
			}
			else if (isign < fidx->isign)
			{
				s_sisdb_fmap_idx newidx;
				newidx.isign = isign;
				newidx.start = count;
				newidx.count = 1;
				newidx.moved = 0;
				newidx.writed = 1;
				startidx = sis_struct_list_insert(unit_->fidxs, i, &newidx);
				// 修改当前的起始位置
				count++;
				i++;
				isidx = 1;
			}
			else
			{
				count += fidx->count;
			}
		}
	}	
	if (isidx == 0)
	{
		s_sisdb_fmap_idx newidx;
		newidx.isign = isign;
		newidx.start = count;
		newidx.count = 1;
		newidx.moved = 0;
		newidx.writed = 1;
		startidx = sis_struct_list_push(unit_->fidxs, &newidx);
	}
	return startidx;
}
// 仅仅修改一条 这里 idate 是日期
int sisdb_fmap_unit_set_idx(s_sisdb_fmap_unit *unit_, int startidx, int idate)
{
	int isign = unit_->scale == SIS_SDB_SCALE_YEAR ? idate / 10000 : idate;
	int start = (startidx < 0 || startidx >= unit_->fidxs->count) ? 0 : startidx;
	if (unit_->fidxs->count == 0)
	{
		s_sisdb_fmap_idx newidx;
		newidx.isign = isign;
		newidx.start = 0;
		newidx.count = 1;
		newidx.moved = 0;
		newidx.writed = 1;
		sis_struct_list_push(unit_->fidxs, &newidx);
		return 0;
	}
	for (int i = start; i < unit_->fidxs->count; i++)
	{
		s_sisdb_fmap_idx *fidx = sis_struct_list_get(unit_->fidxs, i);
		if (isign == fidx->isign)
		{
			fidx->moved = 0;
			fidx->writed = 1;
			startidx = i;
			break;
		}
	}	
	return startidx;
}
// 删除 这里 index 为起始记录 count 为数量
int sisdb_fmap_unit_del_idx(s_sisdb_fmap_unit *unit_, int index, int count)
{
	int nums = 0;
	for (int i = 0; i < unit_->fidxs->count; i++)
	{
		s_sisdb_fmap_idx *fidx = sis_struct_list_get(unit_->fidxs, i);
		if (index == 0 && count == 0)
		{
			fidx->start -= nums;
			continue;
		}
		if (index >= fidx->count)
		{
			index -= fidx->count;
			continue;
		}
		if (fidx->count - index >= count)
		{
			nums += count;
			fidx->count -= count;  // 后面的 start 都要减去 nums
			count = 0;
			index = 0;
		}
		else
		{
			nums += fidx->count - index;
			fidx->count -= fidx->count - index; 
			count -= fidx->count - index;
			index = 0;
		}
		if (fidx->count == 0)
		{
			fidx->start = 0;
			fidx->moved = 1;
		}
		fidx->writed = 1;
	}	
	return 0;
}

// 删除键值 需要做标记 仅仅针对时序数据
int sisdb_fmap_unit_move_idx(s_sisdb_fmap_unit *unit_)
{
	for (int i = 0; i < unit_->fidxs->count; i++)
	{
		s_sisdb_fmap_idx *fidx = sis_struct_list_get(unit_->fidxs, i);
		fidx->start = 0;
		fidx->count = 0;
		fidx->moved = 1;
		fidx->writed = 1;
	}
	return 0;
}

//////////////////////////////////////////////////
//  del
//////////////////////////////////////////////////

// 返回删除的数量
int sisdb_fmap_cxt_tsdb_del(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, s_sisdb_fmap_cmd *cmd_)
{
	s_sis_struct_list *slist = (s_sis_struct_list *)unit_->value;
	s_sisdb_fmap_cmp ans = { 0, -1, 0}; 
	// 根据 cmd 信息计算针对列表的开始索引和数量
	if (cmd_->cmpmode == SISDB_FMAP_CMP_WHERE) // 完全匹配 
	{
		if (sisdb_fmap_cmp_where(unit_, cmd_->start, 0, &ans) < 0)
		{
			return 0;
		}
	}
	else // 尽量匹配
	{
		if (sisdb_fmap_cmp_range(unit_, cmd_->start, cmd_->stop, 0, &ans) < 0)
		{
			return 0;
		}
	}
	if (ans.ocount > 0 && ans.oindex >= 0)
	{
		// 先清理镜像索引
		sisdb_fmap_unit_del_idx(unit_, ans.oindex, ans.ocount);
		// 再删除数据
		sis_struct_list_delete(slist, ans.oindex, ans.ocount);
		// 重建索引
		sisdb_fmap_unit_reidx(unit_);
	}
	// 数据都没有也不能认为键值应该被move 除非明确的move指令
	return ans.ocount;
}

//////////////////////////////////////////////////
//  move
//////////////////////////////////////////////////

int sisdb_fmap_cxt_tsdb_remove(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_)
{
	// 清理镜像索引但保留信息
	sisdb_fmap_unit_move_idx(unit_);
	// 清理可能存在的数据列表
	sis_struct_list_clear(unit_->value);
	return 0;
}

//////////////////////////////////////////////////
//  push 从磁盘中整块写入，不逐条进行校验
//////////////////////////////////////////////////
// 按顺序插入到原有队列中 
// 1 表示尾部记录改变 0 表示修改中间 -1 表示头部记录改变
static int _fmap_cxt_vlist_add(s_sis_struct_list *slist, void *imem, int index)
{
	if (index < 0)
	{
		sis_struct_list_insert(slist, 0, imem);
		return -1;
	}
	else if (index >= slist->count)
	{
		sis_struct_list_push(slist, imem);
		return 1;
	}
	// else 
	{
		sis_struct_list_insert(slist, index, imem);
	}
	return 0;
}

//////////////////////////////////////////////////
//  update
//////////////////////////////////////////////////

int _fmap_unit_solely_update_noidx(s_sisdb_fmap_unit *unit_, const char *imem_)
{
	s_sis_struct_list *slist = (s_sis_struct_list *)unit_->value;
	// 默认从尾部开始匹配
	int start = slist->count - 1;
	// 定位匹配记录位置
	for (int i = start; i >= 0 ; i--)
	{
		const char *v = sis_struct_list_get(slist, i);
		bool ok = true;
		for (int k = 0; k < unit_->sdb->field_solely->count; k++)
		{
			s_sis_dynamic_field *fu = (s_sis_dynamic_field *)sis_pointer_list_get(unit_->sdb->field_solely, k);
			if (memcmp(imem_ + fu->offset, v + fu->offset, fu->len))
			{
				ok = false;
				break;
			}
		}
		if (ok) //  所有字段都相同
		{
			sis_struct_list_update(slist, i, (void *)imem_);
			return 0;
		}
	}
	// 全部没有匹配 放在最后
	_fmap_cxt_vlist_add(slist, (void *)imem_, slist->count);
	return 1;
}
// 需要检验唯一性
int _fmap_unit_solely_update(s_sisdb_fmap_unit *unit_, const char *imem_, int index_, uint64 mindex_)
{
// 如果index_ = -1 表示比第一个记录小
// 如果index_ >= count 表示比最后记录大
// 如果index_ 0 .. count - 1 表示匹配mindex的最后一个位置 
	s_sis_struct_list *slist = (s_sis_struct_list *)unit_->value;
	if (index_ < 0)
	{
		_fmap_cxt_vlist_add(slist, (void *)imem_, -1);
		return 1;
	}
	// else if (index_ >= slist->count)
	// {
	// 	_fmap_cxt_vlist_add(slist, (void *)imem_, slist->count);
	// 	return 1;
	// }
	int start = slist->count - 1;
	start = sis_min(index_, start);
	// 定位索引
	for (int i = start; i >= 0 ; i--)
	{
		const char *v = sis_struct_list_get(slist, i);
		uint64 mindex = sis_dynamic_db_get_mindex(unit_->sdb, 0, (void *)v, unit_->sdb->size);
		if (mindex < mindex_)
		{
			// printf("_fmap_cxt_vlist_add 1, %d %d\n", index_, slist->count);
			_fmap_cxt_vlist_add(slist, (void *)imem_, index_);
			return 1;
		}
		bool ok = true;
		for (int k = 0; k < unit_->sdb->field_solely->count; k++)
		{
			s_sis_dynamic_field *fu = (s_sis_dynamic_field *)sis_pointer_list_get(unit_->sdb->field_solely, k);
			if (memcmp(imem_ + fu->offset, v + fu->offset, fu->len))
			{
				ok = false;
				break;
			}
		}
		if (ok) //  所有字段都相同
		{
			sis_struct_list_update(slist, i, (void *)imem_);
			return 0;
		}
	}
	// 全部没有匹配 放在最后
	// printf("_fmap_cxt_vlist_add 2, %d %d\n", index_, slist->count);
	_fmap_cxt_vlist_add(slist, (void *)imem_, index_ + 1);
	return 1;
}
// 到这里一定有需要匹配的字段
int sisdb_fmap_cxt_solely_update(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, s_sisdb_fmap_cmd *cmd_)
{
	int count = 0;
	int nums = cmd_->isize / unit_->sdb->size;
	const char *ptr = cmd_->imem;
	int start_fidx = -1;
	printf("unit == %d %zu %d\n", nums, cmd_->isize, unit_->sdb->size);
	for (int i = 0; i < nums; i++)
	{
		if (!unit_->sdb->field_mindex)
		{
			// 没有索引字段 找到匹配字段 然后插入
			count += _fmap_unit_solely_update_noidx(unit_, ptr);
		}
		else
		{
			uint64 mindex = sis_dynamic_db_get_mindex(unit_->sdb, 0, (void *)ptr, unit_->sdb->size);
			int oindex = sisdb_fmap_cmp_find_tail(unit_, mindex);
			int o = _fmap_unit_solely_update(unit_, ptr, oindex, mindex);
			// o = 0 表示修改 o = 1 表示增加
			int ostart = sisdb_fmap_unit_get_start(unit_, mindex); // 如果为时间索引 就转成日期
			if (o == 0)
			{
				start_fidx = sisdb_fmap_unit_set_idx(unit_, start_fidx, ostart);
			}
			else
			{
				start_fidx = sisdb_fmap_unit_add_idx(unit_, start_fidx, ostart);
			}
			printf("unit fidxs == %d %d oindex %d|%lld %d %d\n", unit_->fidxs->count, start_fidx, oindex, mindex, o, ostart);
			count += o;
		}
		ptr += unit_->sdb->size;
	}
	return count;
}
// 只有索引 没有时间字段
int sisdb_fmap_cxt_mindex_update(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, s_sisdb_fmap_cmd *cmd_)
{
	int start_fidx = -1;
	const char *ptr = cmd_->imem;
	s_sis_struct_list *slist = (s_sis_struct_list *)unit_->value;
	int count = cmd_->isize / unit_->sdb->size;
	uint64 tail_idx = sis_dynamic_db_get_mindex(unit_->sdb, 0, sis_struct_list_last(slist), unit_->sdb->size);
	for (int i = 0; i < count; i++)
	{
		uint64 curr_idx = sis_dynamic_db_get_mindex(unit_->sdb, 0, (void *)ptr, unit_->sdb->size);
		if (curr_idx >= tail_idx)
		{
			sis_struct_list_push(slist, (void *)ptr);
			tail_idx = curr_idx;
		}
		else // curr_idx < tail_idx
		{
			int oindex = sisdb_fmap_cmp_find_tail(unit_, curr_idx);
			if (oindex >= slist->count)
			{
				sis_struct_list_push(slist, (void *)ptr);
				tail_idx = curr_idx;
			}
			else if (oindex < 0)
			{
				sis_struct_list_insert(slist, 0, (void *)ptr);
			}
			else
			{
				uint64 find_idx = sis_dynamic_db_get_mindex(unit_->sdb, oindex, sis_struct_list_first(slist), unit_->sdb->size);
				if (find_idx == curr_idx)
				{
					if (oindex + 1 >= slist->count)
					{
						sis_struct_list_push(slist, (void *)ptr);
						tail_idx = curr_idx;
					}
					else
					{
						sis_struct_list_insert(slist, oindex + 1, (void *)ptr);
					}
				}
				else
				{
					sis_struct_list_insert(slist, oindex, (void *)ptr);
				}
			}
		}
		int ostart = sisdb_fmap_unit_get_start(unit_, curr_idx);
		start_fidx = sisdb_fmap_unit_add_idx(unit_, start_fidx, ostart);
		ptr += unit_->sdb->size;
	}
	return count;
}

int _disk_save_fmap_sdb(s_sisdb_fmap_cxt *cxt, s_sis_disk_writer *wfile, s_sisdb_fmap_unit *funit)
{
    int count = 0;
    // printf("save ==== %s %s %d\n", SIS_OBJ_GET_CHAR(funit->kname), SIS_OBJ_GET_CHAR(funit->sname), funit->fidxs->count);
    for (int i = 0; i < funit->fidxs->count; i++)
    {
        s_sisdb_fmap_idx *fidx = sis_struct_list_get(funit->fidxs, i);
        printf("save ==== %s.%s %d %d %d\n", SIS_OBJ_GET_CHAR(funit->kname), SIS_OBJ_GET_CHAR(funit->sname), fidx->moved, fidx->writed, funit->fidxs->count);
        if (!fidx->writed)
        {
            continue;
        }
        count++;
        if (fidx->moved)
        {
            sis_disk_writer_sdb_remove(wfile, SIS_OBJ_GET_CHAR(funit->kname), SIS_OBJ_GET_CHAR(funit->sname), fidx->isign);
        }
        else
        {
            s_sis_struct_list *slist = (s_sis_struct_list *)funit->value;
            if (slist->count > 0)
            {
                sis_disk_writer_sdb(wfile, SIS_OBJ_GET_CHAR(funit->kname), SIS_OBJ_GET_CHAR(funit->sname), 
                    sis_struct_list_first(slist), slist->count * slist->len);
            }
            fidx->writed = 0;
        }
    }
    return count;  
}

int _disk_save_fmap(s_sisdb_fmap_cxt *cxt, s_sis_disk_writer *wfile, s_sisdb_fmap_unit *funit)
{
    int count = 1;
    switch (funit->ktype)
    {
    case SIS_SDB_STYLE_ONE:
        {
            if (funit->moved)
            {
                sis_disk_writer_one_remove(wfile, SIS_OBJ_GET_CHAR(funit->kname));
            }
            else
            {
                s_sis_sds str = (s_sis_sds)funit->value;
                sis_disk_writer_one(wfile, SIS_OBJ_GET_CHAR(funit->kname), str, sis_sdslen(str));
            }
        }
        break;
    case SIS_SDB_STYLE_MUL:
        {
            if (funit->moved)
            {
                // s_sis_node *node = (s_sis_node *)unit->value;
                // sis_disk_writer_mul_remove(wfile, SIS_OBJ_GET_CHAR(funit->kname));
            }
            else
            {
                // s_sis_node *node = (s_sis_node *)funit->value;
                // sis_disk_writer_mul(wfile, SIS_OBJ_GET_CHAR(funit->kname), str, sis_sdslen(str));
            }
        }
        break;
    case SIS_SDB_STYLE_NON:	
        {
            // printf("save ==1== %d %d %d %d| %p\n", funit->ktype, funit->moved, funit->reads, funit->writed, funit->fidxs);
            if (funit->moved)
            {
                sis_disk_writer_sdb_remove(wfile, SIS_OBJ_GET_CHAR(funit->kname), SIS_OBJ_GET_CHAR(funit->sname), 0);
            }
            else
            {
                if (funit->writed)
                {
                    s_sis_struct_list *slist = (s_sis_struct_list *)funit->value;
                    if (slist->count > 0)
                    {
                        sis_disk_writer_sdb(wfile, SIS_OBJ_GET_CHAR(funit->kname), SIS_OBJ_GET_CHAR(funit->sname), 
                            sis_struct_list_first(slist), slist->count * slist->len);
                    }
                    printf("save ==ok== %d \n", slist->count);
                    funit->writed = 0;
                }
            }
        }
        break;
    default:
        {
            count = _disk_save_fmap_sdb(cxt, wfile, funit);
        }
        break;
    }
    return count;
}

int sisdb_fmap_cxt_save(s_sisdb_fmap_cxt *cxt_, const char *work_path_, const char *work_name_)
{
    int count = sisdb_fmap_cxt_get_key_count(cxt_);
    if (count > 0)
    {
        s_sis_disk_writer *wfile = sis_disk_writer_create(work_path_, work_name_, SIS_DISK_TYPE_SDB);
        // 不能删除老文件的信息
        sis_disk_writer_open(wfile, 0);
        {
            s_sis_sds keys = sisdb_fmap_cxt_get_keys(cxt_);
            sis_disk_writer_set_kdict(wfile, keys, sis_sdslen(keys));
            s_sis_sds sdbs = sisdb_fmap_cxt_get_sdbs(cxt_, 0, 1);
			if (sdbs)
			{
	            sis_disk_writer_set_sdict(wfile, sdbs, sis_sdslen(sdbs));
            	sis_sdsfree(sdbs);
			}
            sis_sdsfree(keys);
        }
        {
            printf("save ==s== %p %d\n", cxt_, count);
            s_sis_dict_entry *de;
            s_sis_dict_iter *di = sis_dict_get_iter(cxt_->work_keys);
            while ((de = sis_dict_next(di)) != NULL)
            {
                s_sisdb_fmap_unit *funit = (s_sisdb_fmap_unit *)sis_dict_getval(de);
                _disk_save_fmap(cxt_, wfile, funit);
            }
            sis_dict_iter_free(di);
        }
        sis_disk_writer_close(wfile);
        sis_disk_writer_destroy(wfile);
    }
    else if (cxt_->sdbs_writed)
    {
        // 仅仅存结构
        s_sis_disk_writer *wfile = sis_disk_writer_create(work_path_, work_name_, SIS_DISK_TYPE_SDB);
        // 不能删除老文件的信息
        sis_disk_writer_open(wfile, 0);
        {
            s_sis_sds sdbs = sisdb_fmap_cxt_get_sdbs(cxt_, 0, 1);
			if (sdbs)
			{
				sis_disk_writer_set_sdict(wfile, sdbs, sis_sdslen(sdbs));
				sis_sdsfree(sdbs);
			}
        }
        sis_disk_writer_close(wfile);
        sis_disk_writer_destroy(wfile);
    }	
	return count;
}

int sisdb_fmap_cxt_free_data(s_sisdb_fmap_cxt *cxt_, int level_)
{
	s_sis_dict_entry *de;
	s_sis_dict_iter *di = sis_dict_get_iter(cxt_->work_keys);
	while ((de = sis_dict_next(di)) != NULL)
	{
		s_sisdb_fmap_unit *funit = (s_sisdb_fmap_unit *)sis_dict_getval(de);
		switch(level_)
		{
			case 0:
				sisdb_fmap_unit_clear(funit);
				break;
			case 1:
				{
					if (funit->scale == SIS_SDB_SCALE_DATE)
					{
						sisdb_fmap_unit_clear(funit);
					}
					// 如果是年数据 只保存当前年的数据在内存，以前的数据从内存中清除
					
				}
				break;
		}
	}
	sis_dict_iter_free(di);
	return 0;
}

