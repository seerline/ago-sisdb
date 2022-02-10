#include "sis_method.h"
#include <sis_file.h>
#include <sis_math.h>

#include "stk_def.h"
#include "stk_struct.v0.h"

#include "worker.h"
#include "server.h"
#include "sis_memory.h"
#include "sis_utils.h"

#if 1

const char *config = "{ wlog : { classname : sisdb_flog }, classname:frwdb }";

static int _write_nums = 10;  // 写入数量
static int _space_nums = 3;  // 间隔数量
static int _start_minu = 0;
static int _start_year = 0;

static int _start_date = 1001;

static int _make_date(int syear, int mday, int offset)
{
	return sis_time_get_offset_day(syear * 10000 + mday, offset);
}

// 测试无时序数据
void test_nots_write(s_sis_worker *worker, s_sis_message *msg)
{
    printf("-------w nots----------\n");
	sis_net_message_clear(msg);
	sis_net_message_set_subject(msg, "SH600600", "stk_info");
	for (int i = 0; i < _write_nums; i++)
	{
		s_v4_stk_info info;
		memset(&info, 0, sizeof(s_v4_stk_info));
		sis_strcpy(info.market, 4, "SH");
		sis_strcpy(info.code, 16, "600600");
		sis_strcpy(info.name, 64, "600600-1");
		info.vunit = (i + 1)% 255;
		sis_net_message_set_info(msg, &info, sizeof(s_v4_stk_info));
		int o = sis_worker_command(worker, "write", msg);
		printf("[%s] %s.\n", msg->subject, o == SIS_METHOD_OK ? "ok" : "fail");
	}
}
void test_nots_read(s_sis_worker *worker, s_sis_message *msg)
{
    printf("-------r nots 1 ----------\n");
	sis_net_message_clear(msg);
	sis_net_message_set_subject(msg, "SH600600", "stk_info");
	int o = sis_worker_command(worker, "get", msg);
	printf("[%s] %s.\n", msg->subject, o == SIS_METHOD_OK ? "ok" : "fail");

	if (msg->info)
	{
		sis_out_binary("ans", msg->info, sis_sdslen(msg->info));
	}
}

// 20211223 存盘后 重启server 此时直接增补跨年数据 数据写入会出错
void test_year_write(s_sis_worker *worker, s_sis_message *msg)
{
	sis_net_message_clear(msg);
	sis_net_message_set_subject(msg, "SH600600", "stk_day");
	for (int i = 0; i < _write_nums; i++)
	{
		s_stock_day info;
		memset(&info, 0, sizeof(s_stock_day));
		info.time   = _make_date(_start_year , _start_date,  i * _space_nums);
		info.open   = sis_double_to_zint32(10.10,2,1);
		info.high   = sis_double_to_zint32(11.11,2,1);
		info.low    = sis_double_to_zint32(12.12,2,1);
		info.newp   = sis_double_to_zint32(13.13,2,1);
		info.volume = 0x15;
		info.money  = 0x16;
		sis_net_message_set_info(msg, &info, sizeof(s_stock_day));
		int o = sis_worker_command(worker, "write", msg);
		printf("[%s] %s.\n",  msg->subject, o == SIS_METHOD_OK ? "ok" : "fail");
	}
	
}

void test_year_read(s_sis_worker *worker, s_sis_message *msg)
{
	printf("-------r year 0 ----------\n");
    
	{
		sis_net_message_clear(msg);
		sis_net_message_set_subject(msg, "SH600600", "stk_day");
		int o = sis_worker_command(worker, "get", msg);
		printf("[%s] %s.\n", msg->subject, o == SIS_METHOD_OK ? "ok" : "fail");
	}
}


void test_date_write(s_sis_worker *worker, s_sis_message *msg)
{
	sis_net_message_clear(msg);
	sis_net_message_set_subject(msg, "SH600600", "stk_min");
	for (int i = 0; i < _write_nums; i++)
	{
		s_stock_day info;
		memset(&info, 0, sizeof(s_stock_day));
		info.time   = _start_minu + i * _space_nums;
		info.open   = sis_double_to_zint32(0.10,2,1);
		info.high   = sis_double_to_zint32(1.11,2,1);
		info.low    = sis_double_to_zint32(2.12,2,1);
		info.newp   = sis_double_to_zint32(3.13,2,1);
		info.volume = 0x15;
		info.money  = 0x16;
		sis_net_message_set_info(msg, &info, sizeof(s_stock_day));
		int o = sis_worker_command(worker, "write", msg);
		printf("[%s] %s.\n", msg->subject, o == SIS_METHOD_OK ? "ok" : "fail");
	}
	
}
void test_date_read(s_sis_worker *worker, s_sis_message *msg)
{
	printf("-------r date 0 ----------\n");
    {
		sis_net_message_clear(msg);
		sis_net_message_set_subject(msg, "SH600600", "stk_min");
		int o = sis_worker_command(worker, "get", msg);
		printf("[%s] %s.\n", msg->subject, o == SIS_METHOD_OK ? "ok" : "fail");
	}
}

static int _isread = 1;
static int _ismerge = 0;
static int _iswrite = 0;  // 0 nowrite 1 normal 2 insert

void test_start(s_sis_worker *worker, s_sis_message *msg)
{
    printf("-------start----------\n");
	sis_net_message_set_info_i(msg, _start_year * 10000 + _start_date);
	int o = sis_worker_command(worker, "start", msg); 
	printf("%s.\n", o == SIS_METHOD_OK ? "ok" : "fail");
}
void test_stop(s_sis_worker *worker, s_sis_message *msg)
{
    printf("-------stop----------\n");
	sis_net_message_set_info_i(msg, _start_year * 10000 + _start_date);
	int o = sis_worker_command(worker, "stop", msg); 
	printf("%s.\n", o == SIS_METHOD_OK ? "ok" : "fail");
}
void test_merge(s_sis_worker *worker, s_sis_message *msg)
{
    printf("-------merge----------\n");
	sis_net_message_set_info_i(msg, _start_year * 10000 + _start_date);
	int o = sis_worker_command(worker, "merge", msg); 
	printf("%s.\n", o == SIS_METHOD_OK ? "ok" : "fail");
}
int main(int argc, const char **argv)
{
    if (argc < 2)
    {
        goto help;
    }
	int c = 1;
	while (c < argc)
	{
		if (argv[c][0] == '-' && argv[c][1] == 'w')
		{
            _iswrite = 1;
		}
		else if (argv[c][0] == '-' && argv[c][1] == 'r')
		{
			_isread = 1;
            if (argv[c][2] && (argv[c][2] == 'n' || argv[c][2] == 'y' || argv[c][2] == 'd'))
            {
                _isread = argv[c][2];
            }
			if (argv[c][2] && (argv[c][2] == 't'))
			{
				_isread = 0;
			}
		}
		else if (argv[c][0] == '-' && argv[c][1] == 'm')
		{
			_ismerge = 1;
		}
		else if (argv[c][0] == '-' && argv[c][1] == 'h')
		{
help:
            printf("command format:\n");
            printf("		-w      : write normal data. \n");
            printf("		-rt     : no read. \n");
            printf("		-r      : read  all. \n");
            printf("		-rn     : read  nots. \n");
            printf("		-ry     : read  year. \n");
            printf("		-rd     : read  date. \n");
            printf("		-m      : merge. \n");
            printf("		-h      : help. \n");
			return 0;
		}
		c++;
	}
	sis_log_open(NULL, 10, 100000);
    sis_server_init();
    s_sis_conf_handle *handle = sis_conf_load(config, sis_strlen(config)); 

    s_sis_worker *worker = sis_worker_create_of_name(NULL, "myclient", handle->node);
    sis_conf_close(handle);

	if (!worker)
	{
		printf("create fail.\n");
		return 0;
	}
    s_sis_message *msg = sis_message_create();

	_start_year = 2010;
	_start_date = 1225;
	_start_minu = sis_time_make_time(_start_year * 10000 + _start_date, 103000) / 60;
    if (_iswrite == 1)
    {
		test_start(worker, msg);
		test_nots_write(worker, msg);
		test_year_write(worker, msg);
		test_date_write(worker, msg);
		test_stop(worker, msg);
    }

    if (_ismerge)
    {
        test_merge(worker, msg);
    }
    if (_isread == 1 || _isread == 'n')
    {
        test_nots_read(worker, msg);
    }
    if (_isread == 1 || _isread == 'y')
    {
        test_year_read(worker, msg);
    }
    if (_isread == 1 || _isread == 'd')
    {
        test_date_read(worker, msg);
    }

    while(1)
    {
        sis_sleep(1000);
    }
	sis_message_destroy(msg);

    sis_worker_destroy(worker);
    sis_server_uninit();
    return 0;
}

#endif