
#include <sts_net.h>
#include <sts_redis.h>

s_sts_socket *sts_socket_create(s_sts_url *url_,int rale_)
{
	int style = sts_str_subcmp(url_->protocol,STS_NET_PROTOCOL_TYPE,',');
	if (style < 0) {
		return NULL;
	}
	s_sts_socket *sock = sts_malloc(sizeof(s_sts_socket));
	memmove(&sock->url, url_, sizeof(s_sts_url));
	sock->rale = rale_;
	
	sts_redis_open(NULL);

	sock->message_send = s_sts_list_create();
	sock->message_send->free = sts_message_node_destroy;

	sock->message_recv = s_sts_list_create();
	sock->message_recv->free = sts_message_node_destroy;

	sock->sub_keys = s_sts_list_create();
	sock->sub_keys->free = sts_message_node_destroy;
	sock->pub_keys = s_sts_list_create();
	sock->pub_keys->free = sts_message_node_destroy;

	sock->handle_read = NULL;
	sock->status_read = STS_NET_WAITCONNECT;

	sock->handle_write = NULL;
	sock->status_write = STS_NET_WAITCONNECT;

	sts_mutex_rw_create(&sock->mutex_message_send);
	sts_mutex_rw_create(&sock->mutex_message_recv);

	sts_mutex_rw_create(&sock->mutex_read);
	sts_mutex_rw_create(&sock->mutex_write);

	sock->fastwork_mode = false;
	sock->fastwork_count = 0;
	sock->fastwork_bytes = 0;

	sock->open = NULL;
	sock->close = NULL;

	sock->socket_send_message = NULL;
	sock->socket_query_message = NULL;

	switch (style)
	{
		case STS_NET_PROTOCOL_REDIS:
			sock->open = sts_redis_open;
			sock->close = sts_redis_close;

			sock->socket_send_message = sts_redis_send_message;
			sock->socket_query_message = sts_redis_query_message;
			break;	
		default:
			sock->status = STS_NET_ERROR;
			break;
	}
	sock->status = STS_NET_INIT;
	return sock;
}

void sts_socket_destroy(s_sts_socket *sock_)
{
	if (!sock_) return;
	if (sock_->status != STS_NET_INIT) return;
	sock_->status = STS_NET_EXIT;
	//这里需要 等线程结束,必须加在这里，不能换顺序，否则退出时会有异常
	sts_socket_close(sock_);

	sts_mutex_rw_destroy(&sock_->mutex_message_send);
	sts_mutex_rw_destroy(&sock_->mutex_message_recv);

	sts_mutex_rw_destroy(&sock_->mutex_read);
	sts_mutex_rw_destroy(&sock_->mutex_write);

	s_sts_list_destroy(sock_->message_send);
	s_sts_list_destroy(sock_->message_recv);
	s_sts_list_destroy(sock_->sub_keys);
	s_sts_list_destroy(sock_->pub_keys);

	sts_free(sock_);
}

void sts_socket_open(s_sts_socket *sock_){
	sock_->status_read = STS_NET_WAITCONNECT;
	sock_->status_write = STS_NET_WAITCONNECT;
	if (sock_->open) sock_->open(sock_);
}
void sts_socket_close(s_sts_socket *sock_){
	if (sock_->close) sock_->close(sock_);
	sock_->status_read = STS_NET_CONNECTSTOP;
	sock_->status_write = STS_NET_CONNECTSTOP;
}

bool sts_socket_send_message(s_sts_socket *sock_, s_sts_message_node *mess_)
{
	if (sock_->socket_send_message) 
	{
		return sock_->socket_send_message(sock_, mess_);
	}
	return false;
}
s_sts_message_node *sts_socket_query_message(s_sts_socket *sock_, s_sts_message_node *mess_)
{
	if (sock_->socket_query_message) 
	{
		return sock_->socket_query_message(sock_, mess_);
	}
	return false;
}
