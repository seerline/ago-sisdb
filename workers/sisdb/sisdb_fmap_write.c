

#include <sisdb_fmap.h>
#include <sisdb_io.h>
#include <sisdb.h>

//////////////////////////////////////////////////
//  push
//////////////////////////////////////////////////

int sisdb_fmap_cxt_tsdb_push(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, s_sisdb_fmap_cmd *cmd_)
{
	s_sis_struct_list *slist = (s_sis_struct_list *)unit_->value;
	s_sisdb_fmap_cmp ans = { 0, -1, 0};
	// 根据 cmd 信息计算针对列表的开始索引和数量
	// push 一定可以成功
	sisdb_fmap_cmp_same(unit_, cmd_->start, &ans);
	
	// 先整理镜像索引
	int ostart = ans.ostart;
	if (ostart == 0)
	{
		ostart = sis_time_unit_convert(unit_->sdb->field_mindex->style, SIS_DYNAMIC_TYPE_DATE, cmd_->start);
	}
	sisdb_fmap_unit_push_idx(unit_, ostart);

	int count = cmd_->isize / unit_->sdb->size;
	if (ans.oindex < 0)
	{
		sis_struct_list_inserts(slist, 0, cmd_->imem, count);
	}
	else if (ans.oindex >= slist->count)
	{
		sis_struct_list_pushs(slist, cmd_->imem, count);
	}
	else 
	{
		sis_struct_list_inserts(slist, ans.oindex + ans.ocount, cmd_->imem, count);
	}
	// 重建索引
	sisdb_fmap_unit_reidx(unit_);
	return count;
}
//////////////////////////////////////////////////
//  update
//////////////////////////////////////////////////

int sisdb_fmap_cxt_tsdb_update(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, s_sisdb_fmap_cmd *cmd_)
{
	s_sis_struct_list *slist = (s_sis_struct_list *)unit_->value;
	s_sisdb_fmap_cmp ans = { 0, -1, 0};
	// 根据 cmd 信息计算针对列表的开始索引和数量
	if (sisdb_fmap_cmp_same(unit_, cmd_->start, &ans) < 0)
	{
		// 没找到就什么也不干
		return 0;
	}
	if (ans.oindex < 0)
	{
		return 0;
	}
	if (ans.ostart > 0)
	{
		// 先清理镜像索引
		sisdb_fmap_unit_update_idx(unit_, ans.ostart);
		// 再修改数据
		sis_struct_list_update(slist, ans.oindex, cmd_->imem);
		// int newnums = cmd_->isize / unit_->sdb->size;
		// for (int i = 0; i < newnums; i++)
		// {
		// 	if (ans.ocount > 0)
		// 	{
		// 		sis_struct_list_update(slist, ans.oindex + i, cmd_->imem + i * unit_->sdb->size);
		// 		ans.ocount--;
		// 	}
		// 	else
		// 	{
		// 		sis_struct_list_insert(slist, ans.oindex + i, cmd_->imem + i * unit_->sdb->size);
		// 	}
		// }	
	}
	return 1;
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
	if (cmd_->cmpmode == SISDB_FMAP_CMP_SAME) // 完全匹配 
	{
		if (sisdb_fmap_cmp_same(unit_, cmd_->start, &ans) < 0)
		{
			return 0;
		}
	}
	else // 尽量匹配
	{
		if (sisdb_fmap_cmp_range(unit_, cmd_->start, cmd_->stop, &ans) < 0)
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

int sisdb_fmap_cxt_tsdb_move(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_)
{
	// 清理镜像索引但保留信息
	sisdb_fmap_unit_move_idx(unit_);
	// 清理可能存在的数据列表
	sis_struct_list_clear(unit_->value);
	return 0;
}