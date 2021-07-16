
#ifndef _OS_FILE_H
#define _OS_FILE_H

// #include <sis_os.h>
#include <os_str.h>

#define SIS_PATH_LEN 1024

#define SIS_FILE_ENTER_LEN   2
#define SIS_FILE_ENTER_SIGN "\r\n"

#define SIS_FILE_IO_READ    O_RDONLY   // │读文件         
#define SIS_FILE_IO_WRITE   O_WRONLY   // │写文件       
#define SIS_FILE_IO_RDWR    O_RDWR     // │即读也写   
#define SIS_FILE_IO_CREATE  O_CREAT    // │若文件存在,此标志无用;若不存在,建新文件  
#define SIS_FILE_IO_TRUNC   O_TRUNC    // │若文件存在,则长度被截为0,属性不变  
#define SIS_FILE_IO_APPEND  O_APPEND   // │即读也写,但每次写总是在文件尾添加   
#define SIS_FILE_IO_TEXT    O_TEXT     // │此标志可用于显示地给出以文本方式打开文件   
#define SIS_FILE_IO_BINARY  O_BINARY   // │此标志可显示地给出以二进制方式打开文件  
#define SIS_FILE_IO_DSYNC   0  
#define SIS_FILE_IO_RSYNC   0  
#define SIS_FILE_IO_SYNC    0  // FILE_FLAG_NO_BUFFERING   


#define SIS_FILE_ACCESS_EXISTS 0x0

#define SIS_FILE_MODE_NORMAL    S_IREAD|S_IWRITE  // 0x0180

#define s_sis_file_handle FILE *

#define SIS_PATH_SEPARATOR  '\\'
#ifdef __cplusplus
extern "C" {
#endif

// 鉴于fwrite写入时不安全 重要文件启用write来操作文件
// 在64位机器中 seek 是能准确定位到大于4G的文件位置的
#define s_sis_handle int

void sis_file_fixpath(char *in_);

s_sis_handle sis_open(const char *fn_, int mode_, int access_);
long long sis_seek(s_sis_handle fp_, long long offset_, int set_);
#define sis_close(a) close(a)

int sis_getpos(s_sis_handle fp_, size_t *offset_);
int sis_setpos(s_sis_handle fp_, size_t *offset_);

size_t sis_size(s_sis_handle fp_);
size_t sis_read(s_sis_handle fp_, char *in_, size_t len_);
size_t sis_write(s_sis_handle fp_, const char *in_, size_t len_);

#define sis_fsync(a) /*FlushFileBuffers(a)*/
#define sis_fdatasync(a) 

#define s_sis_file_handle FILE *

s_sis_file_handle sis_file_open(const char *fn_, int mode_, int access_);

#define sis_file_close(a) fclose(a)

#define sis_file_seek(a, b, c) fseek(a, b, c)

size_t sis_file_size(s_sis_file_handle fp_);
size_t sis_file_read(s_sis_file_handle fp_, char *in_, size_t len_);
size_t sis_file_write(s_sis_file_handle fp_, const char *in_, size_t len_);

int sis_file_fsync(s_sis_file_handle fp_);

void  sis_file_getpath(const char *fn_, char *out_, int olen_);
void sis_file_getname(const char *fn_, char *out_, int olen_);

bool sis_file_exists(const char *fn_);
bool sis_path_exists(const char *path_);

int sis_file_rename(char *oldn_, char *newn_);

int sis_file_delete(const char *fn_);

void sis_path_complete(char *path_,int maxlen_);
bool sis_path_mkdir(const char *path_);

#define SIS_FINDALL  0
#define SIS_FINDPATH 1
#define SIS_FINDFILE 2

char *sis_path_get_files(const char *path_, int mode_);

#ifdef __cplusplus
}
#endif

#endif //_SIS_FILE_H
