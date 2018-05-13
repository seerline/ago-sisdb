
#ifndef _LW_BASE_H
#define _LW_BASE_H
/////////////////////////////////////////////
// 头文件定义
/////////////////////////////////////////////
#ifdef _MSC_VER
#include <winsock2.h>
#include <process.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/stat.h>
//#include <tchar.h>
//#include <atltime.h>
#include <fcntl.h>
#include <string.h>
#include <direct.h>
#include <io.h>

//#include <cctype>
//#include <ctime>
//#include <vector>
//#include <map>
//#include <parser.h>
#else
#include <cstdio> 
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <netdb.h>
#include <unistd.h>
#include <iconv.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/time.h>
#include <dirent.h> 
#include <fnmatch.h> 
#endif

/////////////////////////////////////////////
// 数据类型定义
/////////////////////////////////////////////
#ifdef _MSC_VER
#ifndef socklen_t 
typedef int socklen_t;
#endif
#ifndef int64_addr
typedef unsigned __int64  int64_addr;
#endif
#ifndef int64
typedef long long int64;
#endif
#else
#ifndef SOCKET
typedef int SOCKET;
#endif
#ifndef LPCSTR
typedef const char * LPCSTR;
#endif
#ifndef LPSTR
typedef char * LPSTR;
#endif
#ifndef int64
typedef long int int64;
#endif
#endif  //_MSC_VER

#ifndef TRUE
#define TRUE         1
#endif
#ifndef FALSE
#define FALSE        0
#endif
#ifndef int8 
typedef char int8;
#endif
#ifndef uint8 
typedef unsigned char uint8;
#endif
#ifndef int16 
typedef short int16;
#endif
#ifndef uint16 
typedef unsigned short uint16;
#endif
#ifndef int32 
typedef int int32;
#endif
#ifndef uint32 
typedef unsigned int uint32;
#endif
#ifndef int64
typedef long long int64;
#endif
#ifndef uint64
typedef unsigned long long uint64;
#endif
#endif //_LW_HEAD_H
