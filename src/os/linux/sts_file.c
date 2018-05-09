
#include <sts_file.h>

sts_file_handle sts_file_open(const char *fn_, int mode_, int access_)
{
	sts_file_handle fp = NULL;
	if (mode_ & STS_FILE_IO_CREATE)
	{
		fp = fopen(fn_, "wb");
	}
	else
	{
		fp = fopen(fn_, "rb");
	}
	return fp;
}

size_t sts_file_size(sts_file_handle fp_)
{
	fseek(fp_, 0, SEEK_END);
	return ftell(fp_);
}
size_t sts_file_read(sts_file_handle fp_, const char *in_, size_t size_, size_t len_)
{
	return fread((char *)in_, size_, len_, fp_) * size_;
}
size_t sts_file_write(sts_file_handle fp_, const char *in_, size_t size_, size_t len_)
{
	return fwrite((char *)in_, size_, len_, fp_) * size_;
}

// STS_MALLOC
char *sts_file_open_and_read(const char *fn_, size_t *len_)
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
	char *buffer = (char *)sts_malloc(size + 1);
	memset(buffer, 0, size + 1);
	*len_ = sts_file_read(fp, buffer, 1, size);
	sts_file_close(fp);
	return buffer;
}

void sts_file_getpath(const char *fn, char *out_, int olen_)
{

	int i, len = (int)strlen(fn);
	for (i = len - 1; i > 0; i--)
	{
		if (fn[i] == '\\' || fn[i] == '/')
		{
			break;
		}
	}
	sts_strncpy(out_, olen_, fn, i + 1);
}