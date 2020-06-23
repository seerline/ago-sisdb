#include "os_fork.h"

int __work_signal = SIS_SIGNAL_WORK;

int sis_get_signal()
{
	return __work_signal;
}
void sis_set_signal(int sign_)
{
	__work_signal = sign_;
}

int sis_fork_process()
{
	return 0;
}
void sis_sigignore(int sign_)
{
}
