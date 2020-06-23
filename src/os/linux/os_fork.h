
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

int sis_get_signal();
void sis_set_signal(int sign_);

#define SIS_SIGNAL_WORK  0
#define SIS_SIGNAL_EXIT  1
#define SIGNAL_EXIT_FAST do { if(sis_get_signal() == SIS_SIGNAL_EXIT) break; } while(0);

#endif //_OS_FORK_H
