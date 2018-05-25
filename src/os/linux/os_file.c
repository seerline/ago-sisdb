
#include <os_file.h>

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

bool sts_file_exists(const char *fn_)
{
	if (access(fn_, STS_FILE_ACCESS_EXISTS) == 0) return true;
	else return false;
}