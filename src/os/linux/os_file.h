
#ifndef _OS_FILE_H
#define _OS_FILE_H

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h> 

#include <os_types.h>
#include <os_str.h>

#define SIS_PATH_LEN 1024

#define SIS_FILE_ENTER_LEN   1
#define SIS_FILE_ENTER_SIGN "\n"

#define SIS_FILE_IO_READ    0x1
#define SIS_FILE_IO_WRITE   0x2
#define SIS_FILE_IO_CREATE  0x4
#define SIS_FILE_IO_TRUCT   0x8
#define SIS_FILE_IO_BINARY  0x10

#define SIS_FILE_ACCESS_EXISTS 0x0
#define SIS_FILE_ACCESS_EXE    0x1
#define SIS_FILE_ACCESS_WRITE  0x2
#define SIS_FILE_ACCESS_READ   0x4

#define SIS_FILE_IO_O_RDWR  O_RDWR

#define sis_file_handle FILE *

#ifdef __cplusplus
extern "C" {
#endif
int sis_open(const char *fn_, int mode_);

sis_file_handle sis_file_open(const char *fn_, int mode_, int access_);

#define sis_file_seek(a, b, c) fseek(a, b, c)
#define sis_file_close(a) fclose(a)

size_t sis_file_size(sis_file_handle fp_);
size_t sis_file_read(sis_file_handle fp_, const char *in_, size_t size_, size_t len_);
size_t sis_file_write(sis_file_handle fp_, const char *in_, size_t size_, size_t len_);

void  sis_file_getpath(const char *fn_, char *out_, int olen_);
void sis_file_getname(const char *fn_, char *out_, int olen_);

bool sis_file_exists(const char *fn_);
bool sis_path_exists(const char *path_);

void sis_file_rename(char *oldn_, char *newn_);

void sis_file_delete(const char *fn_);

char sis_path_separator();
void sis_path_complete(char *path_,int maxlen_);
bool sis_path_mkdir(const char *path_);
#ifdef __cplusplus
}
#endif
#endif //_SIS_FILE_H
