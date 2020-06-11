
#ifndef _SIS_OS_H
#define _SIS_OS_H

#pragma warning(disable: 4200 4267 4047 4996 4819 4244)

#define random rand
#define inline __inline
#define __func__  __FUNCTION__

#define __WIN32__

// ***  注意顺序不能变  *** //
#include <winsock2.h>
#include <windows.h>
#pragma comment (lib, "ws2_32.lib")
// ***  注意顺序不能变  *** //
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h> 
#include <stdint.h>

#include <os_malloc.h>
#include <os_types.h>

/*#include <winsock2.h>
#include <process.h>
#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <sys/stat.h>
#include <tchar.h>
#include <atltime.h>
#include <fcntl.h>
#include <string>
#include <cctype>
#include <ctime>
#include <vector>
#include <map>
#include <parser.h>*/


#endif //_SIS_OS_H
