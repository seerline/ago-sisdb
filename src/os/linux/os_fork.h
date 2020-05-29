
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

#define sis_signal    signal
// #define sis_sigignore sigignore

void sis_sigignore(int sign_);

int sis_fork_process();

#endif //_OS_FORK_H
