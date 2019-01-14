#ifndef _OS_FORK_H
#define _OS_FORK_H

#ifdef __cplusplus
extern "C" {
#endif

#define sis_signal(a,b)

void sis_sigignore(int sign_);

int sis_fork_process();

#ifdef __cplusplus
}
#endif

#endif //_OS_FORK_H
