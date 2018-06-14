#include <os_fork.h>

int sts_fork_process()
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
	sts_open("/dev/null", STS_FILE_IO_O_RDWR);
	dup(0);
	dup(1);
	dup(2);
	return (0);
}
void sts_sigignore(int sign_)
{
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;//设定接受到指定信号后的动作为忽略
	sa.sa_flags = 0;
	if (sigemptyset(&sa.sa_mask) == -1 ||   //初始化信号集为空
		sigaction(sign_, &sa, 0) == -1) {   //屏蔽SIGPIPE信号
		printf("failed to ignore SIGPIPE; sigaction");
	}
}
