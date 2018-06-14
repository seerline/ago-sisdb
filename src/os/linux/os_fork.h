
#ifndef _OS_FORK_H
#define _OS_FORK_H

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include <sys/types.h>  
#include <unistd.h>
#include <os_file.h>
#include <signal.h>

//#include <semaphore.h>

#define sts_signal    signal
// #define sts_sigignore sigignore

void sts_sigignore(int sign_);

int sts_fork_process();

#endif //_OS_FORK_H
