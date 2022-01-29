
#include <sis_net.ws.h>
#include <sis_net.rds.h>
#include <sis_net.h>
#include <sis_net.msg.h>

#if 0
// 测试打包数据的网络流量

#define TEST_PORT 7329
#define TEST_SIP "0.0.0.0"
#define TEST_CIP "127.0.0.1"

typedef struct s_net_client
{
	int cid;
	int status; // 0 stop 1 work
	s_sis_thread send_thread;
} s_net_client;

s_sis_net_class *session = NULL;
s_sis_struct_list *clients;

int64 maxbags = 0;
// 3*1000*1000;  // 最大数据包数量 0 无限数量
int64 bagssize = 16000; // 16K

int64 recv_nums = 0;
size_t recv_size = 0;
size_t recv_curr = 0;
msec_t recv_msec = 0;

int sendnums = 0;
int64 sendsize = 0;

static void cb_recv_data(void *socket_, s_sis_net_message *msg)
{
	s_sis_net_class *socket = (s_sis_net_class *)socket_;

	if (!msg->switchs.sw_tag && msg->switchs.sw_info)
	{
		s_sis_sds reply = msg->info;

		recv_nums++;
		recv_size += sis_sdslen(reply);
		recv_curr += sis_sdslen(reply);

		msec_t *recv_time = (msec_t *)reply;
		msec_t curr_time = sis_time_get_now_msec();
		// sis_out_binary(".1.", reply, 16);
		if (recv_nums % 10000 == 0)// || recv_nums > bagssize - 10)
		{
			printf("=%d=recv: %lld delay = %lld \n", socket->url->role == SIS_NET_ROLE_REQUEST ? 1 : 2, recv_nums, curr_time - *recv_time);
		}
		if (recv_msec == 0)
		{
			recv_msec = curr_time;
		}
		else
		{
			int offset = sis_time_get_now_msec() - recv_msec;
			if (offset > 1000)
			{
				recv_msec = sis_time_get_now_msec();
				printf("[%d] recv %lld %12zu --- cost : %5d recv %10zu speed(M/s): %zu\n",
					   msg->cid, recv_nums, recv_size,
					   offset, recv_curr, recv_curr / offset / 1000);
				recv_curr = 0;
			}
		}
		// sendnums++;
		// sendsize+= 8;//sis_sdslen(reply);
		// memmove(reply, &now_time, sizeof(msec_t));
		// // sis_out_binary(".2.", reply, 16);
		// s_sis_net_message *newmsg = sis_net_message_create();
		// newmsg->cid = connectid;
		// // sis_net_message_set_byte(newmsg, reply, sis_sdslen(reply));
		// sis_net_message_set_byte(newmsg, reply, 8);
		// sis_net_class_send(socket, newmsg);
		// sis_net_message_destroy(newmsg);
	}
	else
	{
		printf("=%d=recv no type. %x\n", socket->url->role == SIS_NET_ROLE_REQUEST ? 1 : 2, msg->format);
	}
}

static void *thread_send_data(void *arg)
{
	s_net_client *pclient = (s_net_client *)arg;
	int64 send_nums = 0;
	size_t send_size = 0;
	size_t send_curr = 0;
	msec_t send_msec = sis_time_get_now_msec();
	;

	char imem[bagssize];
	// memset(imem, 1, bagssize);
	while (((maxbags == 0) || (send_nums < maxbags)) && pclient->status == 1)
	{
		send_nums++;
		send_size += bagssize;
		send_curr += bagssize;

		msec_t curr_time = sis_time_get_now_msec();
		memmove(imem, &curr_time, sizeof(msec_t));
		// sis_out_binary(".1.", imem, 16);
		s_sis_net_message *msg = sis_net_message_create();
		msg->cid = pclient->cid;
		sis_net_message_set_byte(msg, imem, bagssize);
		sis_net_class_send(session, msg);
		sis_net_message_destroy(msg);
		// sis_sleep(1);

		int offset = curr_time - send_msec;
		if (offset > 1000)
		{
			send_msec = sis_time_get_now_msec();
			printf("[%d] send %lld %12zu --- cost : %5d send %10zu speed(M/s): %zu\n",
				   pclient->cid, send_nums, send_size,
				   offset, send_curr, send_curr / offset / 1000);
			send_curr = 0;
		}
		if (send_curr > 2 * 1000 * 1000000)
		{
			sis_sleep(10);
		}
	}
	pclient->status = 0;
	return NULL;
}

static void _cb_connected(void *handle_, int sid)
{
	s_sis_net_class *socket = (s_sis_net_class *)handle_;

	sis_net_class_set_cb(socket, sid, socket, cb_recv_data);

	LOG(5)("==connect [%d]\n", sid);

	s_net_client client = {0};
	client.cid = sid;
	client.status = 1;
	sis_struct_list_push(clients, &client);
	s_net_client *pclient = sis_struct_list_last(clients);

	if (socket->url->role == SIS_NET_ROLE_REQUEST)
	{
		// 创建发送线程
		LOG(5)
		("==start send data to [%d]\n", sid);
		sis_thread_create(thread_send_data, pclient, &pclient->send_thread);
	}
	else
	{
		LOG(5)
		("==server start. [%d]\n", sid);
	}
}
static void _cb_disconnect(void *handle_, int sid)
{
	s_sis_net_class *socket = (s_sis_net_class *)handle_;
	if (sid != -1)
	{
		for (int i = 0; i < clients->count; i++)
		{
			s_net_client *client = sis_struct_list_get(clients, i);
			if (client->cid == sid)
			{
				client->status = 2;
				while (client->status == 2)
				{
					sis_sleep(300);
				}
				break;
			}
		}
	}
	if (socket->url->role == SIS_NET_ROLE_REQUEST)
	{
		LOG(5)
		("==client stop.\n");
	}
	else
	{
		LOG(5)
		("==server stop.\n");
	}
}

// 单CPU 40M左右 多CPU 60M左右 基本够用 有时间再优化
int exit_ = 0;
void exithandle(int sig)
{
	if (exit_ == 1)
	{
		abort();
	}
	exit_ = 1;
}

int main(int argc, const char **argv)
{
	if (argc < 2)
	{
		return 0;
	}
	sis_socket_init();
	signal(SIGINT, exithandle);

	safe_memory_start();

	s_sis_url url_srv = {SIS_NET_IO_WAITCNT, SIS_NET_ROLE_REQUEST, 1, SIS_NET_PROTOCOL_WS, 0, 0, TEST_SIP, "", TEST_PORT, NULL};
	s_sis_url url_cli = {SIS_NET_IO_CONNECT, SIS_NET_ROLE_ANSWER, 1, SIS_NET_PROTOCOL_WS, 0, 0, TEST_CIP, "", TEST_PORT, NULL};

	clients = sis_struct_list_create(sizeof(s_net_client));

	if (argv[1][0] == 's')
	{
		session = sis_net_class_create(&url_srv);
	}
	else
	{
		session = sis_net_class_create(&url_cli);
	}
	session->cb_connected = _cb_connected;
	session->cb_disconnect = _cb_disconnect;

	sis_net_class_open(session);

	while (!exit_)
	{
		sis_sleep(100);
	}
	sis_net_class_close(session);
	sis_net_class_destroy(session);
	sis_struct_list_destroy(clients);
	safe_memory_stop();
	return 0;
}
#endif