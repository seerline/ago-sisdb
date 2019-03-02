
#include <sis_net.h>

s_sis_socket *sis_socket_create(s_sis_url *url_,int rale_)
{

	s_sis_socket *sock = sis_malloc(sizeof(s_sis_socket));
	memmove(&sock->url, url_, sizeof(s_sis_url));
	sock->rale = rale_;
	
	sock->message_send = s_sis_list_create();
	sock->message_send->free = sis_message_node_destroy;

	sock->message_recv = s_sis_list_create();
	sock->message_recv->free = sis_message_node_destroy;

	sock->sub_keys = s_sis_list_create();
	sock->sub_keys->free = sis_message_node_destroy;
	sock->pub_keys = s_sis_list_create();
	sock->pub_keys->free = sis_message_node_destroy;

	sock->handle_read = NULL;
	sock->status_read = SIS_NET_WAITCONNECT;

	sock->handle_write = NULL;
	sock->status_write = SIS_NET_WAITCONNECT;

	sis_mutex_rw_create(&sock->mutex_message_send);
	sis_mutex_rw_create(&sock->mutex_message_recv);

	sis_mutex_rw_create(&sock->mutex_read);
	sis_mutex_rw_create(&sock->mutex_write);

	sock->fastwork_mode = false;
	sock->fastwork_count = 0;
	sock->fastwork_bytes = 0;

	sock->open = NULL;
	sock->close = NULL;

	sock->socket_send_message = NULL;
	sock->socket_query_message = NULL;

	sock->status = SIS_NET_INIT;
	return sock;
}

void sis_socket_destroy(void *sock_)
{
	if (!sock_) {return;}
	s_sis_socket *sock = (s_sis_socket *)sock_;

	// printf("sis_socket_destroy----%d  %d\n",sock->status, SIS_NET_INIT);
	// if (sock->status != SIS_NET_INIT) {return;}
	// 无论任何时候都要释放
	sock->status = SIS_NET_EXIT;
	//这里需要 等线程结束,必须加在这里，不能换顺序，否则退出时会有异常
	sis_socket_close(sock);

	sis_mutex_rw_destroy(&sock->mutex_message_send);
	sis_mutex_rw_destroy(&sock->mutex_message_recv);

	sis_mutex_rw_destroy(&sock->mutex_read);
	sis_mutex_rw_destroy(&sock->mutex_write);

	s_sis_list_destroy(sock->message_send);
	s_sis_list_destroy(sock->message_recv);
	s_sis_list_destroy(sock->sub_keys);
	s_sis_list_destroy(sock->pub_keys);

	sis_free(sock);
}

void sis_socket_open(s_sis_socket *sock_){
	sock_->status_read = SIS_NET_WAITCONNECT;
	sock_->status_write = SIS_NET_WAITCONNECT;
	if (sock_->open) {sock_->open(sock_);}
}
void sis_socket_close(s_sis_socket *sock_){
	if (sock_->status_read != SIS_NET_CONNECTSTOP ||sock_->status_write != SIS_NET_CONNECTSTOP)
	{
		if (sock_->close) {sock_->close(sock_);}
	} 
	sock_->status_read = SIS_NET_CONNECTSTOP;
	sock_->status_write = SIS_NET_CONNECTSTOP;
}

bool sis_socket_send_message(s_sis_socket *sock_, s_sis_message_node *mess_)
{
	if (sock_->socket_send_message) 
	{
		return sock_->socket_send_message(sock_, mess_);
	}
	return false;
}
s_sis_message_node *sis_socket_query_message(s_sis_socket *sock_, s_sis_message_node *mess_)
{
	if (sock_->socket_query_message) 
	{
		return sock_->socket_query_message(sock_, mess_);
	}
	return NULL;
}
