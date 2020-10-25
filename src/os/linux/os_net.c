
#include <os_net.h>

static int __init_socket = 0;

void sis_socket_init()
{
	if (!__init_socket)
	{
		struct sigaction sa;
		sa.sa_handler = SIG_IGN;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(SIGPIPE, &sa, 0);
		__init_socket = 1;
	}
}
void sis_socket_uninit()
{
	
}

int sis_socket_getip4(const char *name_, char *ip_, size_t ilen_)
{
	// sis_socket_init();
	struct hostent *host = gethostbyname(name_);
	if (!host)
	{
		return 0;
	}
	// //别名
	// for(int i=0; host->h_aliases[i]; i++){
	//     printf("Aliases %d: %s\n", i+1, host->h_aliases[i]);
	// }
	//地址类型
	if (host->h_addrtype != AF_INET) // AF_INET6
	{
		return 0;
	}
	int count = 0;
	char destIP[128];
	char **phe = NULL;
	for (phe = host->h_addr_list; NULL != *phe; ++phe)
	{
		if (count == 0)
		{
			inet_ntop(host->h_addrtype, *phe, destIP, sizeof(destIP));
			sis_strcpy(ip_, ilen_, destIP);
		}
		// inet_ntop(he->h_addrtype,*phe,destIP,sizeof(destIP));
		// printf("%s\n",destIP);
		count++;
	}
	return count;
}