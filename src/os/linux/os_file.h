
#ifndef _OS_FILE_H
#define _OS_FILE_H

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <os_types.h>

#define STS_NAME_LEN      32
#define STS_FILE_PATH_LEN 1024

#define STS_FILE_ENTER_LEN   1
#define STS_FILE_ENTER_SIGN "\n"

#define STS_FILE_IO_READ    0x1
#define STS_FILE_IO_WRITE   0x2
#define STS_FILE_IO_CREATE  0x4
#define STS_FILE_IO_TRUCT   0x8

#define STS_FILE_ACCESS_EXISTS 0x0
#define STS_FILE_ACCESS_EXE    0x1
#define STS_FILE_ACCESS_WRITE  0x2
#define STS_FILE_ACCESS_READ   0x4

#define sts_file_handle FILE *

sts_file_handle sts_file_open(const char *fn_, int mode_, int access_);

#define sts_file_seek(a, b, c) fseek(a, b, c)
#define sts_file_close(a) fclose(a)

size_t sts_file_size(sts_file_handle fp_);
size_t sts_file_read(sts_file_handle fp_, const char *in_, size_t size_, size_t len_);
size_t sts_file_write(sts_file_handle fp_, const char *in_, size_t size_, size_t len_);

bool sts_file_exists(const char *fn_);

#endif //_STS_FILE_H
