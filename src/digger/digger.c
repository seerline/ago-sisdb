
#include <call.h>
#include <stdbool.h>


bool _inited = false;
char _source[255];

int call_digger_init(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{
	int o = digger_create(ctx_, _source);
	if (o <= 0)
	{
		printf("create db [%s] fail. code = [%d]\n", _source, o);
	}
	else
	{
		printf("create db ok. count = [%d]\n", o);
	}
	_inited = true;
    sts_module_reply_with_simple_string(ctx_,"OK");
	return o;	
}
// _module 开头的函数仅仅在该文件应用，该文件仅仅处理逻辑问题，具体函数实现在call文件中
int call_digger_get(s_sts_module_context *ctx_, s_sts_module_string **argv_, int argc_)
{

	if (argc_ < 3)
	{
		return sts_module_wrong_arity(ctx_);
	}

	int out;
	if (argc_ == 4)
	{
		out = digger_get(ctx_,
						 sts_module_string_get(argv_[1], NULL),  // dbname : day
						 sts_module_string_get(argv_[2], NULL),  // key : sh600600
						 sts_module_string_get(argv_[3], NULL)); // command: json
	}
	else
	{
		out = digger_get(ctx_,
						 sts_module_string_get(argv_[1], NULL), // dbname : day
						 sts_module_string_get(argv_[2], NULL), // key : sh600600
						 NULL);
	}
	return out;
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
	if (argc_ == 1)
	{
		call_digger_init(sts_module_string_get(argv_[0], NULL));
	}
	else
	{
		call_digger_init("./digger.conf", NULL));
	}

	if (sts_module_create_command(ctx_, "digger.start",call_digger_start, 
		"write",
		0, 0, 0) == STS_MODULE_ERROR)
	{
		return STS_MODULE_ERROR;
	}
	if (sts_module_create_command(ctx_, "digger.get",call_digger_get, 
		"readonly",
		0, 0, 0) == STS_MODULE_ERROR)
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
// digger.get conf.[name]000012    获取当前策略运行的配置
// digger.get money.[name]000012   不带参数--最新的资金情况  
//                                 带参数--历史资金变动(需要增加开始和结束日期，若没有就返回全部)
// digger.get stock.[name]000012   不带参数--当前持股  带参数 -- 历史持股信息
// digger.get trade.[name]000012   不带参数--交易明细  带参数 -- 历史交易信息
// ...
// digger.sub order.[name]000012    订阅当前算法的交易指令
// digger.pub order.[name]000012 {} 发布一个交易指令，用于人工策略交易

// digger.set order.[name]000012 {} 实时调整和设置持仓情况，然后交由算法来判定交易指令
// 增加股票按当前的股价，当前时间，不然总市值会不确定，影响算法的计算
// 手动输入当天没有买入指令的股票不计入收益计算中，做好标记就可以了

//------------------------------------------------------------//

// digger.start [name] [command]  返回查询编码， [name]000012
// command中会有是realtime还是debug
// --- 获取当前策略最优的参数列表 

// digger.get status.[name]000012  获取计算状态，百分比，预计花费时间
// digger.get list.[name]000012    获取参数调整算法的结果列表，包括每种算法的收益率，成功率等信息，

// digger.get conf.[name]000012.000001    获取当前策略运行的配置
// digger.get money.[name]000012.000001   不带参数--最新的资金情况  
//                                 带参数--历史资金变动(需要增加开始和结束日期，若没有就返回全部)
// digger.get stock.[name]000012.000001   不带参数--当前持股  带参数 -- 历史持股信息
// digger.get trade.[name]000012.000001   不带参数--交易明细  带参数 -- 成对完成交易信息
//

