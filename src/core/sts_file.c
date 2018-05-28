
#include <sts_file.h>

s_sts_sds sts_file_direct_read_sds(const char *fn_, size_t *len_)
{
	sts_file_handle fp = sts_file_open(fn_, STS_FILE_IO_READ, 0);
	if (!fp)
	{
		sts_out_error(3)("cann't open file [%s].\n", fn_);
		return NULL;
	}
	size_t size = sts_file_size(fp);
	sts_file_seek(fp, 0, SEEK_SET);
	if (size == 0)
	{
		sts_file_close(fp);
		return NULL;
	}
	s_sts_sds buffer = (s_sts_sds)sts_malloc(size + 1);
	memset(buffer, 0, size + 1);
	*len_ = sts_file_read(fp, buffer, 1, size);
	sts_file_close(fp);
	return buffer;
}
