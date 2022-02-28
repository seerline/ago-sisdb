#include "sis_method.h"
#include <sis_file.h>
#include <sis_math.h>

// #include "stk_def.h"
// #include "stk_struct.v0.h"

#include "worker.h"
#include "server.h"
#include "sis_memory.h"
#include "sis_utils.h"

#if 0
// #ifdef TEST_DEBUG

// const char *config = "{ ip : 127.0.0.1, port : 7329, username: admin, password:admin5678, classname:sisdb_client }";
const char *config = "{ ip : 192.168.3.119, port : 7329, username: admin, password:admin5678, classname:sisdb_client }";

static int _write_nums = 10;  // 写入数量
static int _space_nums = 3;  // 间隔数量
static int _start_minu = 0;
static int _start_year = 0;

static int _start_date = 1001;

static int _make_date(int syear, int mday, int offset)
{
	return sis_time_get_offset_day(syear * 10000 + mday, offset);
}

void test_save(s_sis_worker *worker, s_sis_message *msg)
{
    printf("-------save----------\n");
    sis_net_message_clear(msg);
	sis_message_set(msg, "cmd", "after.save", NULL);
	int o = sis_worker_command(worker, "ask-bytes-wait", msg); 
	printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
}
// 测试无时序数据
void test_nots_write(s_sis_worker *worker, s_sis_message *msg)
{
    printf("-------w nots----------\n");
	sis_net_message_clear(msg);
	sis_message_set(msg, "key", "SH600600.stk_info", NULL);
	for (int i = 0; i < _write_nums; i++)
	{
		s_v4_stk_info info;
		memset(&info, 0, sizeof(s_v4_stk_info));
		sis_strcpy(info.market, 4, "SH");
		sis_strcpy(info.code, 16, "600600");
		sis_strcpy(info.name, 64, "600600-1");
		info.vunit = (i + 1)% 255;
		sis_message_set(msg, "val", &info, NULL);
		sis_message_set_int(msg, "vlen", sizeof(s_v4_stk_info));
		sis_message_set(msg, "cmd", "after.bset", NULL);
		int o = sis_worker_command(worker, "ask-bytes-wait", msg);
		printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
	}
}
void test_nots_read(s_sis_worker *worker, s_sis_message *msg)
{
    printf("-------r nots 1 ----------\n");
	sis_net_message_clear(msg);
	sis_message_set(msg, "key", "SH600600.stk_info", NULL);
	sis_message_set(msg, "cmd", "after.get", NULL);
	sis_worker_command(worker, "ask-chars-wait", msg);
	printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");

    printf("-------r nots 2 ----------\n");
	char *ask = "{\"format\":\"struct\"}";
	sis_message_set(msg, "val", ask, NULL);
	sis_message_set_int(msg, "vlen", sis_strlen(ask));
	sis_worker_command(worker, "ask-chars-wait", msg);
	if (msg->info)
	{
		sis_out_binary("ans", msg->info, sis_sdslen(msg->info));
	}
}
void read_whole_year(s_sis_worker *worker)
{
	printf("-------read_whole_year ----------\n");
	s_sis_message *msg = sis_message_create();
	sis_message_set(msg, "key", "SH600600.stk_day", NULL);
	sis_message_set(msg, "cmd", "after.get", NULL);
	char ask[1024];
	sis_sprintf(ask, 1024, "{\"format\":\"json\", \"range\":{\"start\":0,\"stop\":-1}}");
	sis_message_set(msg, "val", ask, NULL);
	sis_message_set_int(msg, "vlen", sis_strlen(ask));
	sis_worker_command(worker, "ask-chars-wait", msg);
	printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");
	sis_message_destroy(msg);
}
void test_year_insert(s_sis_worker *worker, s_sis_message *msg)
{
	sis_net_message_clear(msg);
	sis_message_set(msg, "key", "SH600600.stk_day", NULL);
    s_stock_day info;
    memset(&info, 0, sizeof(s_stock_day));
    info.open   = sis_double_to_zint32(110.10,2,1);
    info.high   = sis_double_to_zint32(111.11,2,1);
    info.low    = sis_double_to_zint32(112.12,2,1);
    info.newp   = sis_double_to_zint32(113.13,2,1);
    info.volume = 0x115;
    info.money  = 0x116;
    sis_message_set(msg, "val", &info, NULL);
    sis_message_set_int(msg, "vlen", sizeof(s_stock_day));
    sis_message_set(msg, "cmd", "after.bset", NULL);

    // {
	// 	// read_whole_year(worker);
    //     // 重写最后记录
    //     info.time   = _make_date(_start_year , _start_date, (_write_nums - 1) * _space_nums);
	// 	int o = sis_worker_command(worker, "ask-bytes-wait", msg);
	// 	printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
	// 	// read_whole_year(worker);
	// }
    // {
    //     // 重写中间记录
    //     info.time   = _make_date(_start_year , _start_date, _space_nums);
	// 	int o = sis_worker_command(worker, "ask-bytes-wait", msg);
	// 	printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
	// 	// read_whole_year(worker);
	// }
    // {
    //     // 插入第一条 + 1
    //     info.time   = _make_date(_start_year , _start_date, -_space_nums);
	// 	int o = sis_worker_command(worker, "ask-bytes-wait", msg);
	// 	printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
	// 	// read_whole_year(worker);
	// }
    // {
    //     // 插入中间 + 1
    //     info.time   = _make_date(_start_year , _start_date, _space_nums / 2);
	// 	int o = sis_worker_command(worker, "ask-bytes-wait", msg);
	// 	printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
	// 	// read_whole_year(worker);
	// }
    {
        // 插入跨年 - 1
        info.time   = _make_date(_start_year - 1, _start_date, 0);
		int o = sis_worker_command(worker, "ask-bytes-wait", msg);
		printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
		// read_whole_year(worker);
	}
    {
        // 填补跨年 + 1
        info.time   = _make_date(_start_year  + 1, _start_date, 0);
		int o = sis_worker_command(worker, "ask-bytes-wait", msg);
		printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
	}
	read_whole_year(worker);
}
// 20211223 存盘后 重启server 此时直接增补跨年数据 数据写入会出错
void test_year_write(s_sis_worker *worker, s_sis_message *msg)
{
	sis_net_message_clear(msg);
	sis_message_set(msg, "key", "SH600600.stk_day", NULL);
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
		sis_message_set(msg, "val", &info, NULL);
		sis_message_set_int(msg, "vlen", sizeof(s_stock_day));
		sis_message_set(msg, "cmd", "after.bset", NULL);
		int o = sis_worker_command(worker, "ask-bytes-wait", msg);
		printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
	}
	
}

void test_year_read(s_sis_worker *worker, s_sis_message *msg)
{
	printf("-------r year 0 ----------\n");
    {
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_day", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		char ask[1024];
		sis_sprintf(ask, 1024, "{\"format\":\"json\", \"range\":{\"start\":0,\"stop\":-1}}");
		sis_message_set(msg, "val", ask, NULL);
		sis_message_set_int(msg, "vlen", sis_strlen(ask));
		sis_worker_command(worker, "ask-chars-wait", msg);
		printf("-- %s --\n", ask);
		printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");
	}
	printf("-------r year 1 ----------\n");
    {
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_day", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		sis_worker_command(worker, "ask-chars-wait", msg);
		printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");
	}
	printf("-------r year where 1 ----------\n");
	{
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_day", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		char ask[1024];
		sis_sprintf(ask, 1024, "{\"format\":\"struct\", \"where\":{\"start\":%d}}", 
            _make_date(_start_year , _start_date, _space_nums));
		sis_message_set(msg, "val", ask, NULL);
		sis_message_set_int(msg, "vlen", sis_strlen(ask));
		sis_worker_command(worker, "ask-chars-wait", msg);
		printf("-- %s --\n", ask);
		if (msg->info)
		{
			sis_out_binary("ans", msg->info, sis_sdslen(msg->info));
		}
	}
	printf("-------r year where 2 ----------\n");
    {
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_day", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		char ask[1024];
		sis_sprintf(ask, 1024, "{\"format\":\"json\", \"where\":{\"start\":%d,\"offset\":-1}}", 
            _make_date(_start_year , _start_date, _space_nums));
		sis_message_set(msg, "val", ask, NULL);
		sis_message_set_int(msg, "vlen", sis_strlen(ask));
		sis_worker_command(worker, "ask-chars-wait", msg);
		printf("-- %s --\n", ask);
		printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");
	}
	printf("-------r year where 3 ----------\n");
    {
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_day", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		char ask[1024];
		sis_sprintf(ask, 1024, "{\"format\":\"json\", \"where\":{\"start\":%d,\"offset\":-1}}", 
            _make_date(_start_year , _start_date, _space_nums - 1));
		sis_message_set(msg, "val", ask, NULL);
		sis_message_set_int(msg, "vlen", sis_strlen(ask));
		sis_worker_command(worker, "ask-chars-wait", msg);
		printf("-- %s --\n", ask);
		printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");
	}
	printf("-------r year 3 ----------\n");
	{
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_day", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		char ask[1024];
		sis_sprintf(ask, 1024, "{\"format\":\"json\", \"range\":{\"start\":%d,\"stop\":-1,\"ifprev\":1}}", 
            _make_date(_start_year , _start_date,_space_nums + 1));
		sis_message_set(msg, "val", ask, NULL);
		sis_message_set_int(msg, "vlen", sis_strlen(ask));
		sis_worker_command(worker, "ask-chars-wait", msg);
		printf("-- %s --\n", ask);
		printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");
	}
	printf("-------r year 4 ----------\n");
	{
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_day", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		char ask[1024];
		sis_sprintf(ask, 1024, "{\"format\":\"json\", \"range\":{\"start\":%d,\"stop\":-1,\"ifprev\":0}}", 
            _make_date(_start_year , _start_date, _space_nums + 1));
		sis_message_set(msg, "val", ask, NULL);
		sis_message_set_int(msg, "vlen", sis_strlen(ask));
		sis_worker_command(worker, "ask-chars-wait", msg);
		printf("-- %s --\n", ask);
		printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");
	}
	printf("-------r year 5 ----------\n");
	{
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_day", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		char ask[1024];
		sis_sprintf(ask, 1024, "{\"format\":\"json\", \"range\":{\"start\":%d,\"stop\":-1,\"ifprev\":1}}", 
            _make_date(_start_year , _start_date, _write_nums * _space_nums));
		sis_message_set(msg, "val", ask, NULL);
		sis_message_set_int(msg, "vlen", sis_strlen(ask));
		sis_worker_command(worker, "ask-chars-wait", msg);
		printf("-- %s --\n", ask);
		printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");
	}
	printf("-------r year 6 ----------\n");
	{
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_day", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		char ask[1024];
		sis_sprintf(ask, 1024, "{\"format\":\"json\", \"range\":{\"start\":%d,\"stop\":0,\"ifprev\":1}}", 
            _make_date(_start_year , _start_date, -1 * _write_nums * _space_nums));
		sis_message_set(msg, "val", ask, NULL);
		sis_message_set_int(msg, "vlen", sis_strlen(ask));
		sis_worker_command(worker, "ask-chars-wait", msg);
		printf("-- %s --\n", ask);
		printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");
	}
	printf("-------r year 7 ----------\n");
	{
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_day", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		char ask[1024];
		sis_sprintf(ask, 1024, "{\"format\":\"json\", \"range\":{\"start\":%d,\"stop\":%d,\"ifprev\":1}}", 
            _make_date(_start_year , _start_date, 2),
            _make_date(_start_year , _start_date, (_write_nums / 2) * _space_nums));
		sis_message_set(msg, "val", ask, NULL);
		sis_message_set_int(msg, "vlen", sis_strlen(ask));
		sis_worker_command(worker, "ask-chars-wait", msg);
		printf("-- %s --\n", ask);
		printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");
	}
	printf("-------r year 8 ----------\n");
	{
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_day", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		char ask[1024];
        sis_sprintf(ask, 1024, "{\"format\":\"json\", \"range\":{\"start\":%d,\"stop\":%d,\"ifprev\":0}}", 
            _make_date(_start_year , _start_date, 2),
            _make_date(_start_year , _start_date, (_write_nums / 2) * _space_nums));
		sis_message_set(msg, "val", ask, NULL);
		sis_message_set_int(msg, "vlen", sis_strlen(ask));
		sis_worker_command(worker, "ask-chars-wait", msg);
		printf("-- %s --\n", ask);
		printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");
	}

}

void test_date_insert(s_sis_worker *worker, s_sis_message *msg)
{
	sis_net_message_clear(msg);
	sis_message_set(msg, "key", "SH600600.stk_min", NULL);
    s_stock_day info;
    memset(&info, 0, sizeof(s_stock_day));
    info.open   = sis_double_to_zint32(310.10,2,1);
    info.high   = sis_double_to_zint32(311.11,2,1);
    info.low    = sis_double_to_zint32(312.12,2,1);
    info.newp   = sis_double_to_zint32(313.13,2,1);
    info.volume = 0x315;
    info.money  = 0x316;
    sis_message_set(msg, "val", &info, NULL);
    sis_message_set_int(msg, "vlen", sizeof(s_stock_day));
    sis_message_set(msg, "cmd", "after.bset", NULL);
    {
        // 重写最后记录
        info.time   = _start_minu + (_write_nums - 1) * _space_nums;
		int o = sis_worker_command(worker, "ask-bytes-wait", msg);
		printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
	}
    {
        // 重写中间记录
        info.time   = _start_minu + _space_nums;
		int o = sis_worker_command(worker, "ask-bytes-wait", msg);
		printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
	}
    {
        // 插入第一条 + 1
        info.time   = _start_minu - _space_nums;
		int o = sis_worker_command(worker, "ask-bytes-wait", msg);
		printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
	}
    {
        // 插入中间 + 1
        info.time   = _start_minu + _space_nums / 2;
		int o = sis_worker_command(worker, "ask-bytes-wait", msg);
		printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
	}
    {
        // 插入10天前 + 1
        info.time   = (_start_minu - 10 * 24 * 60);
		int o = sis_worker_command(worker, "ask-bytes-wait", msg);
		printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
	}
    {
        // 填补10天后 + 1
        info.time   = (_start_minu + 10 * 24 * 60);
		int o = sis_worker_command(worker, "ask-bytes-wait", msg);
		printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
	}	
}


void test_date_write(s_sis_worker *worker, s_sis_message *msg)
{
	sis_net_message_clear(msg);
	sis_message_set(msg, "key", "SH600600.stk_min", NULL);
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
		sis_message_set(msg, "val", &info, NULL);
		sis_message_set_int(msg, "vlen", sizeof(s_stock_day));
		sis_message_set(msg, "cmd", "after.bset", NULL);
		int o = sis_worker_command(worker, "ask-bytes-wait", msg);
		printf("[%s] %s.\n", (char *)sis_message_get(msg, "cmd"), o == SIS_METHOD_OK ? "ok" : "fail");
	}
	
}
void test_date_read(s_sis_worker *worker, s_sis_message *msg)
{
	printf("-------r date 0 ----------\n");
    {
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_min", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		char ask[1024];
		sis_sprintf(ask, 1024, "{\"format\":\"json\", \"range\":{\"start\":0,\"stop\":-1}}");
		sis_message_set(msg, "val", ask, NULL);
		sis_message_set_int(msg, "vlen", sis_strlen(ask));
		sis_worker_command(worker, "ask-chars-wait", msg);
		printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");
	}
    printf("-------r date 1 ----------\n");
	{
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_min", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		sis_worker_command(worker, "ask-chars-wait", msg);
		printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");
	}
	printf("-------r date 2 ----------\n");
	{
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_min", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		char ask[1024];
		sis_sprintf(ask, 1024, "{\"format\":\"struct\", \"where\":{\"start\":%d}}", 
            _start_minu + _space_nums);
		sis_message_set(msg, "val", ask, NULL);
		sis_message_set_int(msg, "vlen", sis_strlen(ask));
		sis_worker_command(worker, "ask-chars-wait", msg);
		if (msg->info)
		{
			sis_out_binary("ans", msg->info, sis_sdslen(msg->info));
		}
	}
	printf("-------r date 3 ----------\n");
	{
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_min", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		char ask[1024];
		sis_sprintf(ask, 1024, "{\"format\":\"json\", \"range\":{\"start\":%d,\"stop\":-1,\"ifprev\":1}}", 
            _start_minu + _space_nums + 1);
		sis_message_set(msg, "val", ask, NULL);
		sis_message_set_int(msg, "vlen", sis_strlen(ask));
		sis_worker_command(worker, "ask-chars-wait", msg);
		printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");
	}
	printf("-------r date 4 ----------\n");
	{
		sis_net_message_clear(msg);
		sis_message_set(msg, "key", "SH600600.stk_min", NULL);
		sis_message_set(msg, "cmd", "after.get", NULL);
		char ask[1024];
		sis_sprintf(ask, 1024, "{\"format\":\"json\", \"range\":{\"start\":%d,\"stop\":-1,\"ifprev\":0}}", 
            _start_minu - 1);
		sis_message_set(msg, "val", ask, NULL);
		sis_message_set_int(msg, "vlen", sis_strlen(ask));
		sis_worker_command(worker, "ask-chars-wait", msg);
		printf("[%s] %d : %s.\n", (char *)sis_message_get(msg, "cmd"), msg->tag, msg->info ? msg->info : "nil");
	}
}

static int _isread = 1;
static int _issave = 0;
static int _iswrite = 0;  // 0 nowrite 1 normal 2 insert

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
            if (argv[c][2] && (argv[c][2] == 'i' || argv[c][2] == 'n' || argv[c][2] == 'y' || argv[c][2] == 'd'))
            {
                _iswrite = argv[c][2];
            }
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
		else if (argv[c][0] == '-' && argv[c][1] == 's')
		{
			_issave = 1;
		}
		else if (argv[c][0] == '-' && argv[c][1] == 'h')
		{
help:
            printf("command format:\n");
            printf("		-w      : write normal data. \n");
            printf("		-wi     : write insert all. \n");
            printf("		-wn     : write insert nots. \n");
            printf("		-wy     : write insert year. \n");
            printf("		-wd     : write insert date. \n");
            printf("		-rt     : no read. \n");
            printf("		-r      : read  all. \n");
            printf("		-rn     : read  nots. \n");
            printf("		-ry     : read  year. \n");
            printf("		-rd     : read  date. \n");
            printf("		-s      : save. \n");
            printf("		-h      : help. \n");
			return 0;
		}
		c++;
	}
	sis_log_open(NULL, 10, 100000);
    sis_server_init();
    s_sis_conf_handle *handle = sis_conf_load(config, sis_strlen(config)); 
	// s_sis_sds str = sis_json_to_sds(handle->node, true);

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
		test_nots_write(worker, msg);
		test_year_write(worker, msg);
		test_date_write(worker, msg);
    }
    if (_iswrite == 'i')
    {
		test_year_insert(worker, msg);
		test_date_insert(worker, msg);
    }
	if (_iswrite == 'n')
    {
		test_nots_write(worker, msg);
    }
	if (_iswrite == 'y')
    {
		test_year_insert(worker, msg);
    }
	if (_iswrite == 'd')
    {
		test_date_insert(worker, msg);
    }
    if (_issave)
    {
        test_save(worker, msg);
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