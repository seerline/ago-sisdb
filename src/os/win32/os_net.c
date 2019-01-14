
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