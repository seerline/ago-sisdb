
#include <os_file.h>

void sis_file_fixpath(char *in_)
{
	int size = strlen(in_);
	for (int i = size - 1; i > 0; i--)
	{
		if (in_[i] == '\\')
		{
			in_[i] = SIS_PATH_SEPARATOR;
		}
	}
}

s_sis_handle sis_open(const char *fn_, int mode_, int access_)
{
	sis_file_fixpath((char *)fn_);
	return open(fn_, mode_, access_);
}

int sis_getpos(s_sis_handle fp_, size_t *offset_)
{
    long long size = sis_seek(fp_, 0, SEEK_CUR);
	if (size >=0 )
	{	
		*offset_ = size;
		return 0;
	}
	return -1;
}
int sis_setpos(s_sis_handle fp_, size_t *offset_)
{
    long long size = sis_seek(fp_, *offset_, SEEK_SET);
	if (size >= 0 )
	{	
		*offset_ = size;
		return 0;
	}
	return -1;
}
size_t sis_size(s_sis_handle fp_)
{
    return sis_seek(fp_, 0, SEEK_END);
}
size_t sis_read(s_sis_handle fp_, char *in_, size_t len_)
{
	return read(fp_, in_, len_);
}
size_t sis_write(s_sis_handle fp_, const char *in_, size_t len_)
{
	return write(fp_, in_, len_);
}

s_sis_file_handle sis_file_open(const char *fn_, int mode_, int access_)
{
	sis_file_fixpath((char *)fn_);
	s_sis_file_handle fp = NULL;
	char mode[5];
	int index = 0;
	if (mode_ & SIS_FILE_IO_TRUNC)
	{
		mode[index] = 'w';
		index++;
	}
	else
	{
		if (mode_ & SIS_FILE_IO_CREATE)
		{
			mode[index] = 'a';
			index++;
		}
		mode[index] = 'r';
		index++;
		// if (mode_ & SIS_FILE_IO_READ)
		// {
		// 	mode[index] = 'r';
		// 	index++;
		// }
	}
	mode[index] = 'b';
	index++;
	// if ((mode_ & SIS_FILE_IO_READ && mode_ & SIS_FILE_IO_WRITE) || mode_ & SIS_FILE_IO_RDWR)
	if (mode_ & SIS_FILE_IO_WRITE || mode_ & SIS_FILE_IO_RDWR)
	{
		mode[index] = '+';
		index++;
	}
	mode[index] = 0;

	fp = fopen(fn_, mode);

	// printf("[%d] %s %s \n ",(int)fp,  fn_, mode);

	return fp;
}
long long sis_file_seek(s_sis_file_handle fp_, size_t offset_, int where_)
{
	long long o = fseek(fp_, offset_, where_);
	if ( o == 0)
	{
		return ftell(fp_);
	}
	return -1;
}
#ifdef __APPLE__
int sis_file_getpos(s_sis_file_handle fp_, size_t *offset_)
{
	fpos_t pos;
    int o = fgetpos(fp_, &pos);
	*offset_ = pos;
	return o;
}
int sis_file_setpos(s_sis_file_handle fp_, size_t *offset_)
{
	fpos_t pos;
	pos = *offset_;
    int o = fsetpos(fp_, &pos);
	*offset_ = pos;
	return o;
}
#else
int sis_file_getpos(s_sis_file_handle fp_, size_t *offset_)
{
	fpos_t pos;
    int o = fgetpos(fp_, &pos);
	*offset_ = pos.__pos;
	return o;
}
int sis_file_setpos(s_sis_file_handle fp_, size_t *offset_)
{
	fpos_t pos;
	pos.__pos = *offset_;
    int o = fsetpos(fp_, &pos);
	*offset_ = pos.__pos;
	return o;
}
#endif
size_t sis_file_size(s_sis_file_handle fp_)
{
	fseek(fp_, 0, SEEK_END);
	return ftell(fp_);
}
size_t sis_file_read(s_sis_file_handle fp_, char *in_, size_t len_)
{
	return fread((char *)in_, 1, len_, fp_);
}
size_t sis_file_write(s_sis_file_handle fp_, const char *in_, size_t len_)
{
	size_t size = fwrite((char *)in_, 1, len_, fp_);
	// fflush(fp_);
	return size;
}

void sis_file_getpath(const char *fn_, char *out_, int olen_)
{
	sis_file_fixpath((char *)fn_);
	out_[0] = 0;
	int i, len = (int)strlen(fn_);
	for (i = len - 1; i > 0; i--)
	{
		if (fn_[i] == SIS_PATH_SEPARATOR)
		{
			sis_strncpy(out_, olen_, fn_, i + 1);
			return;
		}
	}
}
void sis_file_getname(const char *fn_, char *out_, int olen_)
{
	sis_file_fixpath((char *)fn_);
	int i, len = (int)strlen(fn_);
	for (i = len - 1; i > 0; i--)
	{
		if (fn_[i] == SIS_PATH_SEPARATOR)
		{
			sis_strncpy(out_, olen_, fn_ + i + 1, len - i - 1);
			
			return;
		}
	}
	sis_strncpy(out_, olen_, fn_, len);
}
bool sis_file_exists(const char *fn_)
{
	sis_file_fixpath((char *)fn_);
	if (access(fn_, SIS_FILE_ACCESS_EXISTS) == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}
bool sis_path_exists(const char *path_)
{
	char path[SIS_PATH_LEN];
	sis_file_getpath(path_, path, SIS_PATH_LEN);
	if (access(path, SIS_FILE_ACCESS_EXISTS) == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}
bool sis_path_mkdir(const char *path_)
{
	int len = (int)strlen(path_);
	if (len == 0)
	{
		return false;
	}
	sis_file_fixpath((char *)path_);
	char dir[SIS_PATH_LEN];
	for (int i = 0; i < len; i++)
	{
		if (path_[i] == SIS_PATH_SEPARATOR)
		{
			sis_strncpy(dir, SIS_PATH_LEN, path_, i + 1);
			if (sis_path_exists(dir))
			{
				continue;
			}
			mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO);
		}
	}
	if (sis_path_exists(path_))
	{
		return true;
	}
	else
	{
		return false;
	}
}

void sis_file_rename(char *oldn_, char *newn_)
{
	sis_file_fixpath((char *)oldn_);
	sis_file_fixpath((char *)newn_);
	rename(oldn_, newn_);
}

void sis_file_delete(const char *fn_)
{
	sis_file_fixpath((char *)fn_);
	unlink(fn_);
}

void sis_path_complete(char *path_, int maxlen_)
{
	sis_file_fixpath(path_);
	size_t len = strlen(path_);
	int last = len - 1;
	if (path_[last] != SIS_PATH_SEPARATOR)
	{
		if (maxlen_ > (int)len)
		{
			path_[last + 1] = SIS_PATH_SEPARATOR;
		}
		else
		{
			path_[last] = SIS_PATH_SEPARATOR;
		}
	}
}

// #include <fnmatch.h>

// int limit_path_filecount(char *FindName, int limit)
// {
// 	if (limit < 1) return 0;

// 	list_filetimeinfo fileinfo;

// 	char fname[255];
// 	strcpy(fname, FindName);
// 	translate_dir(fname);
// 	char path[255], findname[255], filename[255];
// 	get_file_path(fname, path);
// 	get_file_name(fname, findname);
// 	DIR *dirp = NULL;
// 	struct dirent *direntp = NULL;
// 	struct stat statbuf;
// 	if ((dirp = opendir(path)) != NULL)
// 	{
// 		while ((direntp = readdir(dirp)))
// 		{
// 			if (!fnmatch(findname, direntp->d_name, FNM_FILE_NAME | FNM_CASEFOLD | FNM_PERIOD))
// 			{
// 				sprintf(filename, "%s%s", path, direntp->d_name);
// 				if (stat(filename, &statbuf) == -1) continue;
// 				fileinfo.push_back(std::make_pair(statbuf.st_mtime, std::string(filename) ));
// 			}
// 		}
// 	}
// 	closedir(dirp);
// 	//if((int)fileinfo.size()<limit) return 0;
// 	time_t mintime;
// 	while ((int)fileinfo.size() > limit){
// 		int nIndex=-1;
// 		for(int i=0;i<(int)fileinfo.size();i++)
// 		{
// 			//LOG(1)("%d,%s\n",fileinfo[i].first,fileinfo[i].second.c_str());
// 			if(i==0)
// 			{
// 				mintime=fileinfo[i].first;
// 				nIndex=0;
// 				continue;
// 			}
// 			if(fileinfo[i].first<mintime)
// 			{
// 				mintime=fileinfo[i].first;
// 				nIndex=i;
// 			}
// 		}
// 		if(nIndex>=0)
// 		{
// 			//LOG(1)("remove %s\n",fileinfo[nIndex].second.c_str());
// 			remove(fileinfo[nIndex].second.c_str());
// 			fileinfo.erase(fileinfo.begin()+nIndex);
// 		}
// 	}
// 	return fileinfo.size();
// }

// int get_path_filelist(char *FindName, c_list_string *FileList)

// {

// 	if (FileList == NULL) return 0;

// 	char fname[255];

// 	strcpy(fname, FindName);

// 	translate_dir(fname);

// 	char path[255], findname[255], filename[255];

// 	get_file_path(fname, path);

// 	get_file_name(fname, findname);

// 	DIR *dirp = NULL;

// 	struct dirent *direntp = NULL;

// 	struct stat statbuf;

// 	if ((dirp = opendir(path)) != NULL)

// 	{

// 		while ((direntp = readdir(dirp)))

// 		{

// 			if (!fnmatch(findname, direntp->d_name, FNM_FILE_NAME | FNM_CASEFOLD | FNM_PERIOD))

// 			{

// 				sprintf(filename, "%s%s", path, direntp->d_name);

// 				if (stat(filename, &statbuf) == -1) continue;

// 				FileList->add(direntp->d_name, strlen(direntp->d_name));

// 			}

// 		}

// 	}

// 	closedir(dirp);

// 	return FileList->getsize();

// }

// void clear_path_file(char *Path)

// {

// 	char fname[255];

// 	strcpy(fname, Path);

// 	translate_dir(fname);
// 	char filename[255];

// 	DIR *dirp = NULL;

// 	struct dirent *direntp = NULL;

// 	struct stat statbuf;

// 	if ((dirp = opendir(fname)) != NULL)

// 	{

// 		while ((direntp = readdir(dirp)))

// 		{

// 			if (!fnmatch("*.*", direntp->d_name, FNM_FILE_NAME | FNM_CASEFOLD | FNM_PERIOD))

// 			{

// 				sprintf(filename, "%s%s", Path, direntp->d_name);

// 				if (stat(filename, &statbuf) == -1) continue;

// 				//LOG(1)("%s\n",filename);

// 				remove(filename);

// 			}

// 		}

// 	}

// 	closedir(dirp);

// }
