
#include <os_file.h>

int sis_open(const char *fn_, int mode_)
{
	return open(fn_, mode_);
}


sis_file_handle sis_file_open(const char *fn_, int mode_, int access_)
{
	sis_file_handle fp = NULL;
	char mode[5];
	int  index = 0;
	if(mode_ & SIS_FILE_IO_TRUCT) {
		mode[index] = 'w';  index++;
	} else {
		if (mode_ & SIS_FILE_IO_CREATE) {
			mode[index] = 'a';  index++;
		}
		if (mode_ & SIS_FILE_IO_READ) {
			mode[index] = 'r';  index++;
		}
	}
	mode[index] = 'b'; index++;
	if (mode_ & SIS_FILE_IO_READ && mode_ & SIS_FILE_IO_WRITE) 
	{
		mode[index] = '+';
		index++;
	}
	mode[index]=0;

	fp = fopen(fn_, mode);
	
	return fp;
}

size_t sis_file_size(sis_file_handle fp_)
{
	fseek(fp_, 0, SEEK_END);
	return ftell(fp_);
}
size_t sis_file_read(sis_file_handle fp_, const char *in_, size_t size_, size_t len_)
{
	return fread((char *)in_, size_, len_, fp_) * size_;
}
size_t sis_file_write(sis_file_handle fp_, const char *in_, size_t size_, size_t len_)
{
	return fwrite((char *)in_, size_, len_, fp_) * size_;
}
void sis_file_getpath(const char *fn_, char *out_, int olen_)
{
	out_[0] = 0;
	int i, len = (int)strlen(fn_);
	for (i = len - 1; i > 0; i--)
	{
		if (fn_[i] == '\\' || fn_[i] == '/')
		{
			sis_strncpy(out_, olen_, fn_, i + 1);
			return;
		}
	}
}
void sis_file_getname(const char *fn_, char *out_, int olen_)
{
	int i, len = (int)strlen(fn_);
	for (i = len - 1; i > 0; i--)
	{
		if (fn_[i] == '\\' || fn_[i] == '/')
		{
			sis_strncpy(out_, olen_, fn_ + i + 1, len - i - 1);
			return;
		}
	}
	sis_strncpy(out_, olen_, fn_, len);
}
bool sis_file_exists(const char *fn_)
{
	if (access(fn_, SIS_FILE_ACCESS_EXISTS) == 0) return true;
	else return false;
}
bool sis_path_exists(const char *path_)
{
	char path[SIS_PATH_LEN];
	sis_file_getpath(path_, path, SIS_PATH_LEN);
	if (access(path, SIS_FILE_ACCESS_EXISTS) == 0)  return true;
	else return false;
}
bool sis_path_mkdir(const char *path_)
{
	int len = (int)strlen(path_);
	if (len == 0) {
		return false;
	}
	char dir[SIS_PATH_LEN];
	for (int i = 0; i<len; i++)
	{
		if (path_[i] == '\\' || path_[i] == '/')
		{
			sis_strncpy(dir, SIS_PATH_LEN, path_, i + 1);
			if (sis_path_exists(dir)) {
				continue;
			}
			mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO);
		}
	}
	if (sis_path_exists(path_)) return true;
	else  return  false;
}

void sis_file_rename(char *oldn_, char *newn_)
{
	rename(oldn_, newn_);
}

void sis_file_delete(const char *fn_)
{
	unlink(fn_);
}

char sis_path_separator()
{
	return '/';
}

void sis_path_complete(char *path_,int maxlen_)
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

