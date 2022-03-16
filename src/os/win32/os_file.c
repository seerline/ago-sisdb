
#include "os_file.h"

void sis_file_fixpath(char *in_)
{
	int size = strlen(in_);
	for (int i = size - 1; i > 0; i--)
	{
		if (in_[i] == '/')
		{
			in_[i] = SIS_PATH_SEPARATOR;
		}
	}
}
s_sis_handle sis_open(const char *fn_, int mode_, int access_)
{
	sis_file_fixpath((char *)fn_);
	// printf("%s %x %x\n", fn_, mode_, access_);
	return open(fn_, mode_, access_);
}
long long sis_seek(s_sis_handle fp_, long long offset_, int set_)
{
	// return lseek(fp_, offset_, set_);
	return _lseeki64(fp_, offset_, set_);
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
	}
	mode[index] = 'r';
	index++;
	mode[index] = 'b';
	index++;
	if (mode_ & SIS_FILE_IO_READ && mode_ & SIS_FILE_IO_WRITE)
	{
		mode[index] = '+';
		index++;
	}
	mode[index] = 0;
	fopen_s(&fp, fn_, mode);
	return fp;
}
/*long long sis_file_seek(s_sis_file_handle fp_, size_t offset_, int where_)
{
	long long o = fseek(fp_, offset_, where_);
	if ( o == 0)
	{
		return ftell(fp_);
	}
	return -1;
}*/

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
int sis_file_fsync(s_sis_file_handle fp_)
{
	return 0;//fsync(fileno(fp_));
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
			out_[i + 1] = 0;
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
			out_[len - i - 1] = 0;
			return;
		}
	}
	sis_strncpy(out_, olen_, fn_, len);
	out_[len] = 0;
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
			// _mkdir(dir);
			CreateDirectory((LPCSTR)dir, NULL);
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

int sis_file_rename(char *oldn_, char *newn_)
{
	sis_file_fixpath(oldn_);
	sis_file_fixpath(newn_);
	return rename(oldn_, newn_);
}

int sis_file_delete(const char *fn_)
{
	sis_file_fixpath((char *)fn_);
	return unlink(fn_);
	// remove(fn_);
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

char *sis_path_get_files(const char *path_, int mode_)
{
	char fname[255];
	strcpy(fname, path_);
	sis_file_fixpath(fname);

	char path[255];
	char findname[255];
	sis_file_getpath(fname, path, 255);
	sis_file_getname(fname, findname, 255);
	if (sis_strlen(findname) < 1)
	{
		findname[0] = '*';
		findname[1] = 0;
	}

	size_t size = 0;
	char *o = NULL;

	HANDLE done;
	BOOL rtn = FALSE;
	WIN32_FIND_DATA f;
	done = FindFirstFile((fname), &f);
	rtn = (done != INVALID_HANDLE_VALUE);
	while (rtn)
	{
		// if (((mode_ == SIS_FINDPATH) && S_ISDIR(statbuf.st_mode)) ||
		// 	((mode_ == SIS_FINDFILE) && S_ISREG(statbuf.st_mode)) ||
		// 	(mode_ == SIS_FINDALL))
		{
			if (o)
			{
				o = sis_strcat(o, &size, ":");
			}
			o = sis_strcat(o, &size, f.cFileName);
		}
		rtn = FindNextFile(done, &f);
	}
	FindClose(done);
	return o;
}
// int	limit_path_filecount(char *FindName, int limit)
// {
// 	list_filetimeinfo fileinfo;

// 	return 0;
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

// 	HANDLE done;

// 	BOOL bCntn = FALSE;

// 	WIN32_FIND_DATA f;

// 	done = FindFirstFile((fname), &f);

// 	bCntn = (done != INVALID_HANDLE_VALUE);

// 	while (bCntn)

// 	{
// 		//    sprintf(filename,"%s%s",path,f.cFileName );
// 		sprintf(filename, "%s", f.cFileName);
// 		FileList->add(filename, (int)strlen(filename));

// 		bCntn = FindNextFile(done, &f);

// 	}

// 	FindClose(done);

// 	return FileList->getsize();

// }

// void clear_path_file(char *Path)

// {

// 	char fname[255];

// 	strcpy(fname, Path);
// 	translate_dir(fname);

// 	char findname[255], filename[255];

// 	sprintf(findname, "%s%s", fname, "*.*");

// 	HANDLE done;

// 	BOOL bCntn = FALSE;

// 	WIN32_FIND_DATA f;

// 	done = FindFirstFile((findname), &f);

// 	bCntn = (done != INVALID_HANDLE_VALUE);

// 	while (bCntn)

// 	{

// 		sprintf(filename, "%s%s", Path, f.cFileName);

// 		remove(filename);

// 		bCntn = FindNextFile(done, &f);

// 	}

// 	FindClose(done);

// }
#if 0
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <stdio.h>

int main( void )
{
   int fh1, fh2;

   fh1 = open( "CRT_OPEN.C", _O_RDONLY ); // C4996
   // Note: _open is deprecated; consider using _sopen_s instead
   if( fh1 == -1 )
      perror( "Open failed on input file" );
   else
   {
      printf( "Open succeeded on input file\n" );
      close( fh1 );
   }

   fh2 = open( "CRT_OPEN.C", _O_WRONLY | _O_CREAT, _S_IREAD |
                            _S_IWRITE ); // C4996
   if( fh2 == -1 )
      perror( "Open failed on output file" );
   else
   {
      printf( "Open succeeded on output file\n" );
      close( fh2 );
   }

   fh1 = open("..\\..\\..\\..\\data\\sisdb\\snos\\20200407.sno", _O_RDONLY ); // C4996
   // Note: _open is deprecated; consider using _sopen_s instead
   if( fh1 == -1 )
      perror( "Open failed on input file" );
   else
   {
      printf( "Open succeeded on input file\n" );
      close( fh1 );
   }

}
#endif