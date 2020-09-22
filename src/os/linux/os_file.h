
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

#define SIS_FILE_IO_READ       /*0x1      */   O_RDONLY 
#define SIS_FILE_IO_WRITE      /*0x2      */   O_WRONLY 
#define SIS_FILE_IO_RDWR       /*0x4      */   O_RDWR   
#define SIS_FILE_IO_CREATE     /*0x0100   */   O_CREAT  
#define SIS_FILE_IO_TRUNC      /*0x0200   */   O_TRUNC  
#define SIS_FILE_IO_APPEND     /*0x0800   */   O_APPEND 
#define SIS_FILE_IO_TEXT       /*0x4000   */   0x4000   
#define SIS_FILE_IO_BINARY     /*0x8000   */   0x8000 
#define SIS_FILE_IO_DSYNC      /*0x400000 */   O_DSYNC  
#ifdef __APPLE__
#define SIS_FILE_IO_RSYNC      /*0x101000 */   O_DSYNC 
#else
#define SIS_FILE_IO_RSYNC      /*0x101000 */   O_RSYNC 
#endif 
#define SIS_FILE_IO_SYNC       /*0x80     */   O_SYNC   

// access 访问模式，宏定义和含义如下：
//     O_RDONLY         1    只读打开                           
//     O_WRONLY         2    只写打开                           
//     O_RDWR           4    读写打开                       
//     还可选择以下模式与以上3种基本模式相与：                      
//         O_CREAT     0x0100   创建一个文件并打开                  
//         O_TRUNC     0x0200   打开一个已存在的文件并将文件长度设置为0，其他属性保持           
//         O_EXCL      0x0400   未使用                              
//         O_APPEND    0x0800   追加打开文件                       
//         O_TEXT      0x4000   打开文本文件翻译CR-LF控制字符       
//         O_BINARY    0x8000   打开二进制字符，不作CR-LF翻译                                                          
//         O_DSYNC 等待物理 I/O 结束后再 write。在不影响读取新写入的数据的前提下，不等待文件属性更新。
//         O_RSYNC read 等待所有写入同一区域的写操作完成后再进行
//         O_SYNC
// mode 该参数仅在 access = O_CREAT方式下使用，其取值如下：        
//     S_IFMT      0xF000   文件类型掩码                        
//     S_IFDIR     0x4000   目录                                
//     S_IFIFO     0x1000   FIFO 专用                           
//     S_IFCHR     0x2000   字符专用                            
//     S_IFBLK     0x3000   块专用                              
//     S_IFREG     0x8000   只为0x0000                          
//     S_IREAD     0x0100   可读                                
//     S_IWRITE    0x0080   可写                                
//     S_IEXEC     0x0040   可执行  
#define SIS_FILE_ACCESS_EXISTS    0x0

#define SIS_FILE_MODE_NORMAL    S_IREAD|S_IWRITE  // 0x0180

#define SIS_PATH_SEPARATOR  '/'

#ifdef __cplusplus
extern "C" {
#endif

// 鉴于fwrite写入时不安全 重要文件启用write来操作文件
// 在64位机器中 seek 是能准确定位到大于4G的文件位置的
#define s_sis_handle int

void sis_file_fixpath(char *in_);

s_sis_handle sis_open(const char *fn_, int mode_, int access_);
#define sis_seek(a, b, c) lseek(a, b, c)
#define sis_close(a) close(a)

int sis_getpos(s_sis_handle fp_, size_t *offset_);
int sis_setpos(s_sis_handle fp_, size_t *offset_);

size_t sis_size(s_sis_handle fp_);
size_t sis_read(s_sis_handle fp_, char *in_, size_t len_);
size_t sis_write(s_sis_handle fp_, const char *in_, size_t len_);


#define s_sis_file_handle FILE *

s_sis_file_handle sis_file_open(const char *fn_, int mode_, int access_);
#define sis_file_close(a) fclose(a)
long long sis_file_seek(s_sis_file_handle fp_, size_t offset_, int where_);

int sis_file_getpos(s_sis_file_handle fp_, size_t *offset_);
int sis_file_setpos(s_sis_file_handle fp_, size_t *offset_);

size_t sis_file_size(s_sis_file_handle fp_);
size_t sis_file_read(s_sis_file_handle fp_, char *in_, size_t len_);
size_t sis_file_write(s_sis_file_handle fp_, const char *in_, size_t len_);

void sis_file_getpath(const char *fn_, char *out_, int olen_);
void sis_file_getname(const char *fn_, char *out_, int olen_);

bool sis_file_exists(const char *fn_);
bool sis_path_exists(const char *path_);

void sis_file_rename(char *oldn_, char *newn_);

void sis_file_delete(const char *fn_);

void sis_path_complete(char *path_,int maxlen_);
bool sis_path_mkdir(const char *path_);

#define SIS_FINDALL  0
#define SIS_FINDPATH 1
#define SIS_FINDFILE 2

char *sis_path_get_files(const char *path_, int mode_);
void sis_path_del_files(char *path_);

#ifdef __cplusplus
}
#endif
#endif //_SIS_FILE_H
