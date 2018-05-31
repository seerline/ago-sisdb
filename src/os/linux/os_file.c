
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
void sts_file_getpath(const char *fn_, char *out_, int olen_)
{
	int i, len = (int)strlen(fn_);
	for (i = len - 1; i > 0; i--)
	{
		if (fn_[i] == '\\' || fn_[i] == '/')
		{
			break;
		}
	}
	sts_strncpy(out_, olen_, fn_, i + 1);
}
bool sts_file_exists(const char *fn_)
{
	if (access(fn_, STS_FILE_ACCESS_EXISTS) == 0) return true;
	else return false;
}
bool sts_path_exists(const char *path_)
{
	char path[STS_PATH_LEN];
	sts_file_getpath(path_, path, STS_PATH_LEN);
	if (access(path, STS_FILE_ACCESS_EXISTS) == 0)  return true;
	else return false;
}
bool sts_path_mkdir(const char *path_)
{
	int len = (int)strlen(path_);
	if (len == 0) {
		return false;
	}
	char dir[STS_PATH_LEN];
	for (int i = 0; i<len; i++)
	{
		if (path_[i] == '\\' || path_[i] == '/')
		{
			sts_strncpy(dir, STS_PATH_LEN, path_, i + 1);
			if (sts_path_exists(dir)) {
				continue;
			}
			mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO);
		}
	}
	if (sts_path_exists(path_)) return true;
	else  return  false;
}

void sts_file_rename(char *oldn_, char *newn_)
{
	rename(oldn_, newn_);
}

void sts_file_delete(const char *fn_)
{
	unlink(fn_);
}

char sts_path_separator()
{
	return '/';
}

void sts_path_complete(char *path_,int maxlen_)
{
	size_t len = strlen(path_);
	for(int i=0;i<len;i++)
	{
		if(path_[i]=='\\') {
			path_[i] = '/';
		}
		if (i==len-1){
			if (path_[i]!='/') {
				if (maxlen_>len) 
				{
					path_[i+1] = '/';
				} else {
					path_[i] = '/';
				}
			}
		}
	}
}

