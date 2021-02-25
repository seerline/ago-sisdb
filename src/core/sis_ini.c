
#include <sis_ini.h>
#include <sis_memory.h>

int _sis_file_ini_parse(s_sis_file_ini *ini_)
{
	if (!ini_ || !ini_->fp)
	{
		return 0;
	}
	sis_file_seek(ini_->fp, 0, SEEK_SET);
	s_sis_memory *buffer = sis_memory_create();

	char gname[128];
	s_sis_map_list *items = NULL;

	while (1)
	{
		size_t bytes = sis_memory_readfile(buffer, ini_->fp, SIS_MEMORY_SIZE);
		// 结构化文件这样读没问题，这里这样最后一行可能就处理不到了
		if (bytes <= 0)
		{
			break;
		}

		size_t offset = sis_memory_get_line_sign(buffer);
		// printf("1 offset=%d %d %d %d\n", (int)offset, (int)buffer->offset, (int)buffer->size, (int)buffer->maxsize);
		// 偏移位置包括回车字符 0 表示没有回车符号，需要继续读
		// sis_sleep(1000);
		char kname[128];
		
		while (offset)
		{
			// printf("offset=%d %d %d %d\n", (int)offset, (int)buffer->offset, (int)buffer->size, (int)buffer->maxsize);
			const char *curptr = sis_memory(buffer);
			
			if (*curptr == '[') // mainid
			{
				if (items)
				{
					sis_map_list_set(ini_->groups, gname, items);
				}
				curptr++;
				const char *start = curptr;
				int cursor = 0;
				while (*curptr != ']' && cursor < offset)
				{
					curptr++; cursor++;
				}
				if (*curptr == ']' && cursor > 0)
				{
					sis_strncpy(gname, 128, start, cursor);
					sis_trim(gname);
					// printf("g = %s\n", gname);
					items = sis_map_list_create(sis_free_call);
				}
			}
			else // sonid
			{
				if (items)
				{
					const char *start = curptr;
					int cursor = 0;
					while (*curptr != '=' && cursor < offset)
					{
						curptr++; cursor++;
					}
					if (*curptr == '=' && cursor > 0)
					{
						sis_strncpy(kname, 128, start, cursor);
						sis_trim(kname);
						char *value = sis_malloc(offset);
						curptr++;
						sis_strncpy(value, offset, curptr, offset - cursor - 2);
						sis_trim(value);
						// printf("k = %s v = %s\n", kname, value);
						// sis_out_binary(kname, value, strlen(value));
						sis_map_list_set(items, kname, value);
					}
				}
				else
				{
					printf("other info.\n");
				}
			}
			sis_memory_move(buffer, offset);
			offset = sis_memory_get_line_sign(buffer);
			// sis_sleep(100);
		}
	}
	if (items)
	{
		sis_map_list_set(ini_->groups, gname, items);
	}
	sis_memory_destroy(buffer);

	return 0;
}
s_sis_file_ini *sis_file_ini_open(const char *name_, int mode_, int access_)
{
	mode_ = (!mode_) ? SIS_FILE_IO_READ : mode_;
	s_sis_file_handle fp = sis_file_open(name_, mode_, access_);
	if (!fp)
	{
		return NULL;
	}

	s_sis_file_ini *o = sis_malloc(sizeof(s_sis_file_ini));
	memset(o, 0, sizeof(s_sis_file_ini));

	o->groups = sis_map_list_create(sis_map_list_destroy);
	o->fp = fp;

	_sis_file_ini_parse(o);

	sis_file_close(o->fp);
	o->fp = NULL;
	return o;
}
void sis_file_ini_close(s_sis_file_ini *ini_)
{
	if (!ini_)
	{
		return;
	}
	// printf("------\n");
	sis_map_list_destroy(ini_->groups);
	sis_free(ini_);
}

int sis_file_ini_get_group_size(s_sis_file_ini *ini_)
{
	if (!ini_)
	{
		return 0;
	}
	return sis_map_list_getsize(ini_->groups);
}
int sis_file_ini_get_item_size(s_sis_file_ini *ini_, int gidx_)
{
	if (!ini_)
	{
		return 0;
	}
	s_sis_map_list *items = (s_sis_map_list *)sis_map_list_geti(ini_->groups, gidx_);
	if (!items)
	{
		return 0;
	}
	return sis_map_list_getsize(items);
}
const char *sis_file_ini_get_val(s_sis_file_ini *ini_, int gidx_, int idx_)
{
	if (!ini_)
	{
		return NULL;
	}
	s_sis_map_list *items = (s_sis_map_list *)sis_map_list_geti(ini_->groups, gidx_);
	if (!items)
	{
		return NULL;
	}
	const char *str = (const char *)sis_map_list_geti(items, idx_);
	return str;
}

#if 0

int main(int n, const char *argv[])
{
	safe_memory_start();
	s_sis_file_ini *ini = sis_file_ini_open("../../data/base/PLANK.MNU", 0, 0);
	int count = sis_file_ini_get_group_size(ini);
	for (int i = 0; i < count; i++)
	{
		int nums = sis_file_ini_get_item_size(ini, i);
		for (int k = 0; k < nums; k++)
		{
			printf("-- %d : %d %s\n",i, k, sis_file_ini_get_val(ini, i, k));
		}
	}
	sis_file_ini_close(ini);
	safe_memory_stop();
	return 0;
}
#endif
