

#include <sisdb_fmap.h>
#include <sisdb_io.h>
#include <sisdb.h>

//////////////////////////////////////////////////
//  read
//////////////////////////////////////////////////

int sisdb_fmap_cxt_tsdb_read(s_sisdb_fmap_cxt *cxt_, s_sisdb_fmap_unit *unit_, s_sisdb_fmap_cmd *cmd_)
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
		if (cmd_->cb_fmap_read)
		{
			for (int i = ans.oindex, n = cmd_->count; n >= 0 && i < ans.oindex + ans.ocount; i++, n--)
			{
				char *mem = sis_struct_list_get(slist, i);
				cmd_->cb_fmap_read(cmd_->cb_source, mem, slist->len, n);
			}
		}
		else
		{
			// 这里直接返回数据指针 外部获取数据需要拷贝一份
			cmd_->imem = sis_struct_list_get(slist, ans.oindex);
			cmd_->isize = ans.ocount * slist->len;
		}
	}
	return ans.ocount;
}
