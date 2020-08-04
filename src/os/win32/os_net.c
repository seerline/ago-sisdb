
#include <os_net.h>

void sis_socket_init()
{
	WORD version = MAKEWORD(2, 2);
	WSADATA data;
	WSAStartup(version, &data);
}
void sis_socket_uninit()
{
	WSACleanup();
}
int sis_socket_getip4(const char *name_, char *ip_, size_t ilen_)
{
	// sis_socket_init()

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
	for (int i = 0; host->h_addr_list[i]; i++)
	{
		if (count == 0)
		{
			sis_strcpy(ip_, ilen_, inet_ntoa(*(struct in_addr *)host->h_addr_list[i]));
		}
		// printf("IP addr %d: %s\n", i+1, inet_ntoa( *(struct in_addr*)host->h_addr_list[i] ) );
		count++;
	}

	// sis_socket_uninit();
	return count;
}