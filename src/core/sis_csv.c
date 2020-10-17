
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
	sis_pointer_list_destroy(csv_->list);
	sis_free(csv_);
}

int sis_file_csv_getsize(s_sis_file_csv *csv_)
{
	if (!csv_)
	{
		return 0;
	}
	return csv_->list->count;
}
int64 sis_file_csv_get_int(s_sis_file_csv *csv_, int idx_, int field, int64 defaultvalue_)
{
	s_sis_string_list *record = sis_pointer_list_get(csv_->list, idx_);
	if (!record || field < 0 || field > sis_string_list_getsize(record) - 1)
	{
		return 0;
	}
	return sis_atoll(sis_string_list_get(record, field));
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
// 	// if (!csv_ || !csv_->fp) {return  0;}
// 	return sis_file_read(csv_->fp, in_, ilen_);
// }
// size_t sis_file_csv_write(s_sis_file_csv *csv_, char *in_, size_t ilen_)
// {
// 	// if (!csv_ || !csv_->fp) {return  0;}
// 	return sis_file_write(csv_->fp, in_, ilen_);
// }

// s_sis_sds sis_file_csv_get(s_sis_file_csv *csv_, char *key_)
// {
// 	return NULL;
// }
// size_t sis_file_csv_set(s_sis_file_csv *csv_, char *key_, char *in_, size_t ilen_)
// {
// 	return 0;
// }
s_sis_sds sis_csv_make_str(s_sis_sds in_, const char *str_, size_t len_)
{
	size_t size = sis_sdslen(in_);
	if (size > 0 && in_[size - 1] != '\n')
	{
		in_ = sis_sdscatlen(in_, ",", 1);
		return sis_sdscatlen(in_, str_, len_);
	}
	else
	{
		return sis_sdscatlen(in_, str_, len_);
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
