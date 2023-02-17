
#include <sis_csv.h>
#include <sis_memory.h>

int _sis_file_csv_parse(s_sis_file_csv *csv_)
{
	if (!csv_ || !csv_->fp)
	{
		return 0;
	}
	sis_file_seek(csv_->fp, 0, SEEK_SET);

	s_sis_memory *buffer = sis_memory_create();
	while (1)
	{
		size_t bytes = sis_memory_readfile(buffer, csv_->fp, SIS_MEMORY_SIZE);
		// 结构化文件这样读没问题，这里这样最后一行可能就处理不到了
		if (bytes <= 0)
		{
			break;
		}

		size_t offset = sis_memory_get_line_sign(buffer);
		// printf("1 offset=%d %d %d %d\n", (int)offset, (int)buffer->offset, (int)buffer->size, (int)buffer->maxsize);
		// 偏移位置包括回车字符 0 表示没有回车符号，需要继续读
		// sis_sleep(1000);
		while (offset)
		{
			// printf("offset=%d %d %d %d\n", (int)offset, (int)buffer->offset, (int)buffer->size, (int)buffer->maxsize);
			s_sis_string_list *str = sis_string_list_create();
			sis_string_list_load(str, sis_memory(buffer), offset, csv_->sign);
			if (sis_string_list_getsize(str) > 0)
			{
				if (sis_map_int_getsize(csv_->head) == 0)
				{
					for (int i = 0; i < sis_string_list_getsize(str); i++)
					{
						const char *kstr = sis_string_list_get(str, i);
						sis_map_int_set(csv_->head, kstr, i);
					}
				}
				sis_pointer_list_push(csv_->list, str);
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
	s_sis_file_handle fp = sis_file_open(name_, mode_, access_);
	if (!fp)
	{
		return NULL;
	}

	s_sis_file_csv *o = sis_malloc(sizeof(s_sis_file_csv));
	memset(o, 0, sizeof(s_sis_file_csv));
	if (!c_)
	{
		c_ = ',';
	}
	o->sign[0] = c_, o->sign[1] = 0;
	o->list = sis_pointer_list_create();
	o->list->vfree = sis_string_list_destroy;
	o->head = sis_map_int_create();
	o->fp = fp;

	_sis_file_csv_parse(o);

	sis_file_close(o->fp);
	return o;
}
void sis_file_csv_close(s_sis_file_csv *csv_)
{
	if (!csv_)
	{
		return;
	}
	sis_map_int_destroy(csv_->head);
	sis_pointer_list_destroy(csv_->list);
	sis_free(csv_);
}

int sis_file_csv_getsize(s_sis_file_csv *csv_)
{
	if (!csv_)
	{
		return 0;
	}
	return csv_->list->count - 1;
}
int64 sis_file_csv_fget_int(s_sis_file_csv *csv_, int idx_, int field, int64 defaultvalue_)
{
	s_sis_string_list *record = sis_pointer_list_get(csv_->list, idx_ + 1);
	if (!record || field < 0 || field > sis_string_list_getsize(record) - 1)
	{
		return defaultvalue_;
	}
	return sis_atoll(sis_string_list_get(record, field));
}
double sis_file_csv_fget_double(s_sis_file_csv *csv_, int idx_, int field, double defaultvalue_)
{
	s_sis_string_list *record = sis_pointer_list_get(csv_->list, idx_ + 1);
	if (!record || field < 0 || field > sis_string_list_getsize(record) - 1)
	{
		return defaultvalue_;
	}
	return atof(sis_string_list_get(record, field));
}
const char *sis_file_csv_fget_str(s_sis_file_csv *csv_, int idx_, int field)
{
	s_sis_string_list *record = sis_pointer_list_get(csv_->list, idx_ + 1);
	if (!record || field < 0 || field > sis_string_list_getsize(record) - 1)
	{
		return NULL;
	}
	return sis_string_list_get(record, field);
}
int64 sis_file_csv_get_int(s_sis_file_csv *csv_, int idx_, const char *field, int64 defaultvalue_)
{
	s_sis_string_list *record = sis_pointer_list_get(csv_->list, idx_ + 1);
	int fidx = sis_map_int_get(csv_->head, field);
	if (!record || fidx < 0 || fidx > sis_string_list_getsize(record) - 1)
	{
		return defaultvalue_;
	}
	return sis_atoll(sis_string_list_get(record, fidx));
}	
	
double sis_file_csv_get_double(s_sis_file_csv *csv_, int idx_, const char *field, double defaultvalue_)
{
	s_sis_string_list *record = sis_pointer_list_get(csv_->list, idx_ + 1);
	int fidx = sis_map_int_get(csv_->head, field);
	if (!record || fidx < 0 || fidx > sis_string_list_getsize(record) - 1)
	{
		return defaultvalue_;
	}
	return atof(sis_string_list_get(record, fidx));
}
const char *sis_file_csv_get_str(s_sis_file_csv *csv_, int idx_, const char *field)
{
	s_sis_string_list *record = sis_pointer_list_get(csv_->list, idx_ + 1);
	int fidx = sis_map_int_get(csv_->head, field);
	if (!record || fidx < 0 || fidx > sis_string_list_getsize(record) - 1)
	{
		return NULL;
	}
	return sis_string_list_get(record, fidx);
}
const char *sis_file_csv_get_head(s_sis_file_csv *csv_, int hidx_)
{
	s_sis_string_list *head = sis_pointer_list_get(csv_->list, 0);
	if (!head || hidx_ < 0 || hidx_ > sis_string_list_getsize(head) - 1)
	{
		return NULL;
	}
	return sis_string_list_get(head, hidx_);
}

s_sis_file_handle sis_csv_write_open(const char *name_, int isnew_)
{
	if (isnew_)
	{
		sis_file_delete(name_);
	}
	s_sis_file_handle fp = sis_file_open(name_, SIS_FILE_IO_READ | SIS_FILE_IO_WRITE | SIS_FILE_IO_CREATE, 0);
	if (!fp)
	{
		return NULL;
	}
	return fp;
}
size_t sis_csv_write(s_sis_file_handle fp_, s_sis_sds isds_)
{
	return sis_file_write(fp_, isds_, sis_sdslen(isds_));
}
void sis_csv_write_close(s_sis_file_handle fp_)
{
	sis_file_close(fp_);
}

s_sis_sds sis_csv_make_str(s_sis_sds in_, const char *str_, size_t len_)
{
	size_t size = sis_sdslen(in_);
	int len = len_;
	if (!str_[len_ - 1])
	{
		len = sis_strlen(str_);
	}
	if (size > 0 && in_[size - 1] != '\n')
	{
		in_ = sis_sdscatlen(in_, ",", 1);
		return sis_sdscatlen(in_, str_, len);
	}
	else
	{
		return sis_sdscatlen(in_, str_, len);
	}
}
s_sis_sds sis_csv_make_int(s_sis_sds in_, int64 val_)
{
	size_t size = sis_sdslen(in_);
	if (size > 0 && in_[size - 1] != '\n')
	{
		return sis_sdscatfmt(in_, ",%I", val_);
	}
	else
	{
		return sis_sdscatfmt(in_, "%I", val_);
	}
}
s_sis_sds sis_csv_make_uint(s_sis_sds in_, uint64 val_)
{
	size_t size = sis_sdslen(in_);
	if (size > 0 && in_[size - 1] != '\n')
	{
		return sis_sdscatfmt(in_, ",%U", val_);
	}
	else
	{
		return sis_sdscatfmt(in_, "%U", val_);
	}
}
s_sis_sds sis_csv_make_double(s_sis_sds in_, double val_, int dot_)
{
	char str[16];
	size_t size = sis_sdslen(in_);
	if (size > 0 && in_[size - 1] != '\n')
	{
		sis_sprintf(str, 16, ",%.*f", dot_, val_);
	}
	else
	{
		sis_sprintf(str, 16, "%.*f", dot_, val_);
	}
	return sis_sdscatlen(in_, str, strlen(str));
}
s_sis_sds sis_csv_make_end(s_sis_sds in_)
{
	return sis_sdscatlen(in_, "\r\n", 2);
}

// s_sis_file_csv_unit 返回参数结构体
int sis_file_csv_read_sub(const char *name_, char c_, void *cb_source, sis_method_define *cb_)
{
	s_sis_file_handle fp = sis_file_open(name_, SIS_FILE_IO_READ, 0);
	if (!fp)
	{
		return -1;
	}
	s_sis_file_csv *o = SIS_MALLOC(s_sis_file_csv, o);
	o->sign[0] = c_ ? c_ : ',';
	o->sign[1] = 0;
	o->fp = fp;
	// 下面来解析数据
	s_sis_file_csv_unit unit;
	unit.index = 0;
	unit.cols = 0;
	unit.argv = NULL;
	unit.argsize = NULL;

	// size_t rsize = 0;
	sis_file_seek(o->fp, 0, SEEK_SET);
	s_sis_memory *memory = sis_memory_create();
	while (1)
	{
		// printf("0 rsize = %zu %zu | %zu %zu\n", rsize, sis_memory_get_size(memory), memory->offset, memory->size);
		size_t bytes = sis_memory_readfile(memory, o->fp, 64 * 1024 * 1024);
		if (bytes <= 0)
		{
			break;
		}
		size_t offset = sis_memory_get_line_sign(memory);
		// rsize += bytes;
		// if (offset == 0)
		// {
		// 	sis_out_binary("--", sis_memory(memory), 128);
		// }
		// printf("1 rsize = %zu %zu | %zu %zu: offset = %zu\n", rsize, sis_memory_get_size(memory), memory->offset, memory->size, offset);
		// 偏移位置包括回车字符 0 表示没有回车符号，需要继续读
		// sis_sleep(1000);
		while (offset)
		{
			// printf("0 rsize = %zu : offset = %zu\n", rsize, offset);
			int cols = sis_str_substr_nums(sis_memory(memory), offset, o->sign[0]);
			if (unit.cols < cols)
			{
				if (unit.argv)
				{
					sis_free(unit.argv);
				}
				if (unit.argsize)
				{
					sis_free(unit.argsize);
				}
				unit.cols = cols;
				unit.argv = sis_malloc(sizeof(const char *) * unit.cols);
				unit.argsize = sis_malloc(sizeof(int) * unit.cols);
			}
			unit.cols = cols;
			memset(unit.argv, 0, sizeof(const char *) * unit.cols);
			memset(unit.argsize, 0, sizeof(int) * unit.cols);
			const char *ptr = sis_memory(memory);
			int colidx = 0;
			unit.argv[colidx] = ptr;
			for (int i = 0; i < offset && colidx < unit.cols; i++, ptr++)
			{
				if (*ptr == o->sign[0])
				{
					unit.argsize[colidx] = ptr - unit.argv[colidx];
					colidx++;
					if (colidx < unit.cols)
					{
						unit.argv[colidx] = ptr + 1;
					}
				}
			}
			if (colidx == unit.cols - 1)
			{
				unit.argsize[colidx] = ptr - unit.argv[colidx];
			}	
			if (cb_)
			{
				cb_(cb_source, &unit);
			}		
			unit.index++;
			sis_memory_move(memory, offset);
			offset = sis_memory_get_line_sign(memory);
		}
	}
	if (unit.argv)
	{
		sis_free(unit.argv);
	}
	if (unit.argsize)
	{
		sis_free(unit.argsize);
	}
	sis_memory_destroy(memory);	
	// 解析数据结束
	sis_file_close(o->fp);
	sis_free(o);	
	return 0;
}
#if 0
int cb_read(void *source, void *argv)
{
	s_sis_file_csv_unit *unit = (s_sis_file_csv_unit *)argv;
	printf("===== %5d : %3d \n", unit->index, unit->cols);
	char str[100];
	for (int i = 0; i < unit->cols; i++)
	{
		sis_out_binary("--", unit->argv[i], unit->argsize[i]);
		sis_strncpy(str, 100, unit->argv[i], unit->argsize[i]);
		sis_trim(str);
		sis_out_binary("--", str, strlen(str));
	}
	return 0;
}
int main(int n, const char *argv[])
{
	safe_memory_start();
	sis_file_csv_read_sub("./test.csv", '|', NULL, cb_read);
	safe_memory_stop();
	return 0;
}
#endif

#if 0

int main(int n, const char *argv[])
{
	safe_memory_start();
	s_sis_file_csv *csv = sis_file_csv_open("money.csv", ',', 0, 0);
	printf("start.\n");
	if (csv)
	{
		printf("str=%s \n", sis_file_csv_fget_str(csv, 0, 1));

		printf("str=%s \n", sis_file_csv_get_str(csv, 0, "date"));

		printf("str=%s \n", sis_file_csv_fget_str(csv, 0, 1));

		printf("str=%s \n", sis_file_csv_get_head(csv, 1));
		sis_file_csv_close(csv);
	}
	printf("end.\n");
	safe_memory_stop();
	return 0;
}
#endif

#if 0

#include <sis_math.h> 

int main(int n, const char *argv[])
{
	s_sis_sds str = sis_sdsempty();
	sis_init_random();

	// printf("%.*f \n", 2, sis_double_random(0.001, 0.999));
	// return 0;
	double box[6];
	box[0] = 0.001;
	box[1] = 0.2;
	box[2] = 0.4;
	box[3] = 0.6;
	box[4] = 0.8;
	box[5] = 0.999;
	
	int out;
	for (int i = 0; i < 10000; i++)
	{
		str = sis_csv_make_int(str, 0);
		str = sis_csv_make_int(str, 0);
		str = sis_csv_make_int(str, 0);
		str = sis_csv_make_int(str, 0);
		str = sis_csv_make_int(str, 0);
		out = (int)(sis_get_random(0, 4.0) + 0.5);
		str = sis_csv_make_int(str, out);
		for (int j = 0; j < 100; j++)
		{
			if (j % 10 == 0)
			{
				str = sis_csv_make_double(str, sis_get_random(0.001 , 0.999), 3);
			}
			else
			{
				str = sis_csv_make_double(str, sis_get_random(box[out] , box[out + 1]), 3);
			}
		}
		str = sis_csv_make_end(str);
		// printf("%d %d %d\n",str[sis_sdslen(str) - 1], '\n', '\r');
	}	
	sis_writefile("sh600745.study.csv", str, sis_sdslen(str));
	str = sis_sdsempty();
	for (int i = 0; i < 100; i++)
	{
		str = sis_csv_make_int(str, 0);
		str = sis_csv_make_int(str, 0);
		str = sis_csv_make_int(str, 0);
		str = sis_csv_make_int(str, 0);
		str = sis_csv_make_int(str, 0);
		out = (int)(sis_get_random(0, 4.0) + 0.5);
		str = sis_csv_make_int(str, out);
		for (int j = 0; j < 100; j++)
		{
			if (j % 10 == 0)
			{
				str = sis_csv_make_double(str, sis_get_random(0.001 , 0.999), 3);
			}
			else
			{
				str = sis_csv_make_double(str, sis_get_random(box[out] , box[out + 1]), 3);
			}		}
		str = sis_csv_make_end(str);
		// printf("%f\n",sis_get_random(0.001, 0.999));
	}	
	sis_writefile("sh600745.test.csv", str, sis_sdslen(str));

	sis_sdsfree(str);
	return 0;
}

#endif
