#ifndef _OS_FORK_H
#define _OS_FORK_H

#ifdef __cplusplus
extern "C" {
#endif

#define sis_signal(a,b)
#define SIGPIPE  SIGBREAK

void sis_sigignore(int sign_);

int sis_fork_process();

int sis_get_signal();
void sis_set_signal(int sign_);

#ifdef __cplusplus
}
#endif

#define SIS_SIGNAL_WORK  0
#define SIS_SIGNAL_EXIT  1
#define SIS_EXIT_SIGNAL { if(sis_get_signal() == SIS_SIGNAL_EXIT) break; } 

#endif //_OS_FORK_H
