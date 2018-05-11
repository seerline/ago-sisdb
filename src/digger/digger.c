
#include <call.h>

int call_digger_init(const char *conf_)
{
	if (!sts_file_exists(conf_)) {
		printf("%s no finded.\n", conf_);
		return STS_MODULE_ERROR;
	}
	return digger_create(conf_);    
}
int call_digger_start(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{

	if (argc_ != 3)
	{
		return sts_module_wrong_arity(ctx_);
	}
	int o = digger_start(ctx_,
						 sts_module_string_get(argv_[1], NULL),  // name : me
						 sts_module_string_get(argv_[2], NULL)); // command: json
	return o;
}
int call_digger_cancel(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ != 2)
	{
		return sts_module_wrong_arity(ctx_);
	}
	int o = digger_cancel(ctx_,
						  sts_module_string_get(argv_[1], NULL)); // id : me000012
	return o;
}

int call_digger_stop(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ != 2)
	{
		return sts_module_wrong_arity(ctx_);
	}
	int o = digger_stop(ctx_,
						sts_module_string_get(argv_[1], NULL)); // id : me000012
	return o;
}

int call_digger_get(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ < 3)
	{
		return sts_module_wrong_arity(ctx_);
	}

	int o;

	if (argc_ == 4)
	{
		o = digger_get(ctx_,
					   sts_module_string_get(argv_[1], NULL),  // dbname : day
					   sts_module_string_get(argv_[2], NULL),  // key : sh600600
					   sts_module_string_get(argv_[3], NULL)); // command: json
	}
	else
	{
		o = digger_get(ctx_,
					   sts_module_string_get(argv_[1], NULL), // dbname : day
					   sts_module_string_get(argv_[2], NULL), // key : sh600600
					   NULL);
	}
	return o;
}
int call_digger_set(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ != 3)
	{
		return sts_module_wrong_arity(ctx_);
	}
	int o = digger_set(ctx_,
					   "order",
					   sts_module_string_get(argv_[1], NULL),  // id : me000012
					   sts_module_string_get(argv_[2], NULL)); // command: json
	return o;
}
int call_digger_sub(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ != 2)
	{
		return sts_module_wrong_arity(ctx_);
	}
	int o = digger_sub(ctx_,
					   "order",
					   sts_module_string_get(argv_[1], NULL)); // id : me000012
	return o;
}

int call_digger_pub(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (argc_ != 3)
	{
		return sts_module_wrong_arity(ctx_);
	}
	int o = digger_pub(ctx_,
					   "order",
					   sts_module_string_get(argv_[1], NULL),  // id : me000012
					   sts_module_string_get(argv_[2], NULL)); // command: json
	return o;
}

int sts_module_on_load(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	if (sts_module_init(ctx_, "digger", 1, STS_MODULE_VER) == STS_MODULE_ERROR)
		return STS_MODULE_ERROR;

	/* Log the list of parameters passing loading the module. */
	for (int k = 0; k < argc_; k++)
	{
		const char *s = sts_module_string_get(argv_[k], NULL);
		printf("module loaded with argv_[%d] = %s\n", k, s);
	}
	int o;
	if (argc_ == 1)
	{
		o = call_digger_init(sts_module_string_get(argv_[0], NULL));
	}
	else
	{
		o = call_digger_init("../conf/digger.conf");
	}
	if (o != STS_MODULE_OK) {
		return STS_MODULE_ERROR;
	}

	if (sts_module_create_command(ctx_, "digger.start", call_digger_start,
								  "write",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	if (sts_module_create_command(ctx_, "digger.cancel", call_digger_cancel,
								  "write",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	if (sts_module_create_command(ctx_, "digger.stop", call_digger_stop,
								  "write",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	if (sts_module_create_command(ctx_, "digger.get", call_digger_get,
								  "readonly",
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	if (sts_module_create_command(ctx_, "digger.set", call_digger_set,
								  "write", // deny-oom
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	if (sts_module_create_command(ctx_, "digger.sub", call_digger_sub,
								  "readonly", // deny-oom
								  0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	if (sts_module_create_command(ctx_, "digger.pub", call_digger_pub,
								  "write deny-oom", //
								  1, 1, 1) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	return STS_MODULE_OK;
}

//------------------------------------------------------------//

// digger.start [name] [command] 返回查询编码 000012
// command中会有是realtime还是debug

// digger.cancel [name]000012
// digger.stop [name]000012

// digger.get status.[name]000012  获取当前策略运行的状态
// digger.get config.[name]000012    获取当前策略运行的配置
// digger.get money.[name]000012   不带参数--最新的资金情况
//                                 带参数--历史资金变动(需要增加开始和结束日期，若没有就返回全部)
// digger.get stock.[name]000012   不带参数--当前持股  带参数 -- 历史持股信息
// digger.get trade.[name]000012   不带参数--交易明细  带参数 -- 历史交易信息
// ...
// digger.sub [name]000012    订阅当前算法的交易指令
// digger.pub [name]000012 {} 发布一个交易指令，用于人工策略交易

// digger.set [name]000012 {} 实时调整和设置持仓情况，然后交由算法来判定交易指令
// 增加股票按当前的股价，当前时间，不然总市值会不确定，影响算法的计算
// 手动输入当天没有买入指令的股票不计入收益计算中，做好标记就可以了

//------------------------------------------------------------//

// digger.start [name] [command]  返回查询编码， [name]000012
// command中会有是realtime还是debug
// --- 获取当前策略最优的参数列表

// digger.get status.[name]000012  获取计算状态，百分比，预计花费时间
// digger.get list.[name]000012    获取参数调整算法的结果列表，包括每种算法的收益率，成功率等信息，

// digger.get config.[name]000012.000001    获取当前策略运行的配置
// digger.get money.[name]000012.000001   不带参数--最新的资金情况
//                                 带参数--历史资金变动(需要增加开始和结束日期，若没有就返回全部)
// digger.get stock.[name]000012.000001   不带参数--当前持股  带参数 -- 历史持股信息
// digger.get trade.[name]000012.000001   不带参数--交易明细  带参数 -- 成对完成交易信息
//
