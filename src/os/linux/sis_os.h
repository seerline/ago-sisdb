
#ifndef _SIS_OS_H
#define _SIS_OS_H

// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>
// #include <string.h>
// #include <pthread.h>
// #include <errno.h>
// #include <fcntl.h>
// #include <ctype.h>
// #include <netdb.h>
// #include <unistd.h>
// #include <iconv.h>
// #include <signal.h>
// #include <arpa/inet.h>
// #include <netinet/in.h>
// #include <netinet/tcp.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <sys/ioctl.h>
// #include <sys/select.h>
// #include <sys/resource.h>
// #include <sys/stat.h>
// #include <sys/mman.h>
// #include <time.h>
// #include <sys/time.h>
// #include <dirent.h>
// #include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/types.h>
#include <stdint.h>
#include <sys/time.h>
#include <stddef.h>
#include <time.h>

#include <os_file.h>
#include <os_fork.h>
#include <os_malloc.h>
// #include <os_net.h>
#include <os_str.h>
#include <os_thread.h>
#include <os_time.h>
#include <os_types.h>

#define  LOWORD(l)         ( (unsigned short)(l) )
#define  HIWORD(l)         ( (unsigned short)(((unsigned int)(l) >> 16) & 0xFFFF) )

#define CLR_RED "\033[0;32;31m"
#define CLR_GREEN "\033[0;32;32m"
#define CLR_BLUE "\033[0;32;36m"
#define CLR_GOLD "\033[0;32;33m"

#define CLR_LRED "\033[1;31m"
#define CLR_LGREEN "\033[1;32m"
#define CLR_LBLUE "\033[1;36m"
#define CLR_LHOT "\033[1;35m"
#define CLR_YELLOW "\033[1;33m"
#define RESET "\033[0m"

#endif //_SIS_OS_H
