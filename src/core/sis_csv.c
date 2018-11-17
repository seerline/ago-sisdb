
#include <sis_csv.h>
#include <sis_memory.h>

int _sis_file_csv_parse(s_sis_file_csv *csv_)
{
	if (!csv_ || !csv_->fp)
		return 0;

	sis_file_seek(csv_->fp, 0, SEEK_SET);

	s_sis_memory *buffer = sis_memory_create();
	while (1)
	{
        size_t bytes = sis_memory_readfile(buffer, csv_->fp, SIS_DB_MEMORY_SIZE);
        // 结构化文件这样读没问题，这里这样最后一行可能就处理不到了
        if (bytes <= 0)
            break;

		size_t offset = sis_memory_get_line_sign(buffer);
		// printf("1 offset=%d %d %d %d\n", (int)offset, (int)buffer->offset, (int)buffer->size, (int)buffer->maxsize);
		// 偏移位置包括回车字符 0 表示没有回车符号，需要继续读
		// sis_sleep(1000);
		while (offset)
		{
			// printf("offset=%d %d %d %d\n", (int)offset, (int)buffer->offset, (int)buffer->size, (int)buffer->maxsize);
			s_sis_string_list *str = sis_string_list_create_r();
			sis_string_list_load(str, sis_memory(buffer), offset, csv_->sign);
			if (sis_string_list_getsize(str) > 0)
			{
				sis_struct_list_push(csv_->list, str);
			}
			else
			{
				sis_string_list_destroy(str);
			}
			sis_memory_move(buffer, offset);
			offset = sis_memory_get_line_sign(buffer);
			// sis_sleep(100);
		}
	}
	sis_memory_destroy(buffer);

	return csv_->list->count;
}
s_sis_file_csv *sis_file_csv_open(const char *name_, char c_, int mode_, int access_)
{
	mode_ = (!mode_) ? SIS_FILE_IO_READ : mode_;
	sis_file_handle fp = sis_file_open(name_, mode_, access_);
	if (!fp)
	{
		return NULL;
	}

	s_sis_file_csv *o = sis_malloc(sizeof(s_sis_file_csv));
	memset(o, 0, sizeof(s_sis_file_csv));
	if (!c_)
		c_ = ',';
	o->sign[0] = c_, o->sign[1] = 0;
	o->list = sis_pointer_list_create();
	o->list->free = sis_string_list_destroy;
	o->fp = fp;

	_sis_file_csv_parse(o);

	sis_file_close(o->fp);
	return o;
}
void sis_file_csv_close(s_sis_file_csv *csv_)
{
	if (!csv_)
		return;
	sis_pointer_list_destroy(csv_->list);
	sis_free(csv_);
}

int sis_file_csv_getsize(s_sis_file_csv *csv_)
{
	if (!csv_)
		return 0;
	return csv_->list->count;
}
int64 sis_file_csv_get_int(s_sis_file_csv *csv_, int idx_, int field, int64 defaultvalue_)
{
	s_sis_string_list *record = sis_pointer_list_get(csv_->list, idx_);
	if (!record || field < 0 || field > sis_string_list_getsize(record) - 1)
	{
		return 0;
	}
	return atoll(sis_string_list_get(record, field));
}
double sis_file_csv_get_double(s_sis_file_csv *csv_, int idx_, int field, double defaultvalue_)
{
	s_sis_string_list *record = sis_pointer_list_get(csv_->list, idx_);
	if (!record || field < 0 || field > sis_string_list_getsize(record) - 1)
	{
		return 0;
	}
	return atof(sis_string_list_get(record, field));
}
void sis_file_csv_get_str(s_sis_file_csv *csv_, int idx_, int field, char *out_, size_t olen_)
{
	s_sis_string_list *record = sis_pointer_list_get(csv_->list, idx_);
	if (!record || field < 0 || field > sis_string_list_getsize(record) - 1)
	{
		out_[0] = 0;
		return;
	}
	sis_strcpy(out_, olen_, sis_string_list_get(record, field));
}
const char *sis_file_csv_get_ptr(s_sis_file_csv *csv_, int idx_, int field)
{
	s_sis_string_list *record = sis_pointer_list_get(csv_->list, idx_);
	if (!record || field < 0 || field > sis_string_list_getsize(record) - 1)
	{
		return NULL;
	}
	return sis_string_list_get(record, field);
}
// size_t sis_file_csv_read(s_sis_file_csv *csv_, char *in_, size_t ilen_)
// {
// 	// if (!csv_ || !csv_->fp) return  0;
// 	return sis_file_read(csv_->fp, in_, 1, ilen_);
// }
// size_t sis_file_csv_write(s_sis_file_csv *csv_, char *in_, size_t ilen_)
// {
// 	// if (!csv_ || !csv_->fp) return  0;
// 	return sis_file_write(csv_->fp, in_, 1, ilen_);
// }

// s_sis_sds sis_file_csv_get(s_sis_file_csv *csv_, char *key_)
// {
// 	return NULL;
// }
// size_t sis_file_csv_set(s_sis_file_csv *csv_, char *key_, char *in_, size_t ilen_)
// {
// 	return 0;
// }

#if 0

int main(int n, const char *argv[])
{
	safe_memory_start();
	s_sis_file_csv *csv = sis_file_csv_open("../test/test.csv", '|', 0, 0);
	char *str =sis_malloc(100);
	if (csv)
	{
		sis_file_csv_get_str(csv,0,6,str,100);
		printf("str=%s  %s\n", str, sis_file_csv_get_ptr(csv, 0, 1));
		sis_file_csv_close(csv);
	}
	printf("end.\n");
	sis_free(str);
	safe_memory_stop();
	return 0;
}

#endif
