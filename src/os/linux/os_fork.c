#include "os_fork.h"
#include "os_file.h"

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
	pid_t pid = fork();
	if (pid < 0)
	{
		return -1;
	}
	else if (pid > 0)
	{
		printf("PID : [ %d ] \n", pid);
		exit(0);
	}
	setsid();
	umask(0);
	int i, maxfd;
	maxfd = sysconf(_SC_OPEN_MAX);
	for (i = 0; i < maxfd; i++)
	{
		close(i);
	}
	sis_open("/dev/null", SIS_FILE_IO_RDWR, 0);
	dup(0);
	dup(1);
	dup(2);
	return (0);
}
void sis_sigignore(int sign_)
{
	struct sigaction sa;
	sa.sa_handler = SIG_IGN; //设定接受到指定信号后的动作为忽略
	sa.sa_flags = 0;
	if (sigemptyset(&sa.sa_mask) == -1 || //初始化信号集为空
		sigaction(sign_, &sa, 0) == -1)
	{ //屏蔽SIGPIPE信号
		printf("failed to ignore SIGPIPE; sigaction");
	}
}

// size_t sis_init_cpu()
// {
// 	return 1;
// }
// int sis_set_cpu(int id_, size_t cpus, cpu_set_t *mask)
// {
// 	return 0;
// }
// int sis_get_cpu(int id_, size_t cpus, cpu_set_t *mask)
// {
// 	return 0;
// }
