#ifndef _SIS_AI_SERIES_H
#define _SIS_AI_SERIES_H

#include <sis_core.h>
#include <sis_list.h>
#include <sis_map.h>
#include <sis_ai.h>

// 主要功能：
// 多指标多日期数据输入，
// 指标数值为 任意顺序值，
// 胜率 = （盈利数 + 1）/（总数 + 2） :: 避免样本过少数值为 0 或者偏大 默认不交易无盈利不是最差
// 计算出各个指标 N日最优收胜率（ = 最大收益率 x 胜率）时买卖的区间[minv, maxv]
//     （默认传入数据为顺序值，大于maxv越大对应最大买 小于minv越小对应最大卖 minv,maxv中间值为无信号）
//     计算时先求出N天最大最小值，然后分成10个区块，再求出每天的收胜率，取最大收胜率时的区间为结果;

// 得到各个指标的区间后，对M个指标分配权重， 10 20 30 40 50 60 70 80 90 100 得到测试权重组
// 并求出各个指标根据区间转化出 -1 .. 1 的归一化值，
// 然后 E（测试权重*指标值）/（权值） = 得到一个合并值，求该合并值的最优收胜率
// 根据所有权重组的最优收胜率可得到最优的权重组，保存该权重组和各指标对用的区间，就是学习后的最佳分配数据；

// 如果某个指标在每天的收胜率学习中，minv maxv是随机且离散的，权重会不断降低，代表的意义是该指标随机性强，因此不具备可参考性
// 例如 对 -100 .. 100 的连续数值，取收益排名前 5 的极值，如果集中在 某个区域内波动，代表权重增加，否则权重减小
// ------
// 求某个指标的有效值区间，把所有数据分成等分的 100 份(为筛选的10倍)，用最优的指标区间去匹配这些小的份额，完全匹配中就加1，
//     最后可以得到每个小块的最大命中数，合并周边10个区域，找到最大命中数的那个区域为目标区域 
//     目标区域的初始权重为 命中率 
//     求出买卖的主区域，若主区域有重叠，权重为 0, 无重叠 求区域间距离 / （区域间距离 + 买卖主区域） 为初始权重）
//     例： sum [0,100] ask [70,90] bid [30,40] 
//         dec [40,70] = 30, 30/(60)*100 = 50 权重为60
//         极限情况1 [0,50] [51,100] 1/100*100 = 1 
//         极限情况2 [40,50] [51,60] 1/20*100 =  5 
//         极限情况3 [0,10] [90,100] 80/100*100 = 80 
// 离散度如何求
// 1 0 1 0 1 0 1 1 1 0  -->   0.01
// 0 0 0 6 0 0 0 0 0 0  --> 100.0
// 0 0 0 3 0 0 0 0 0 0  --> 100.0
// 0 0 2 5 2 1 0 0 0 0  -->  60.0

// 再学习时，以此权重为基数，左右各取10%为参考范围，然后对多个指标进行递归调用，寻找最优的权重分配
// **** 最后得到的权重和指标区间回测时一定是盈利的，对新数据测试也应该大概率是盈利的 ****
//
// 用排名前5的最佳数据来判断新交易日的数据是否为盈利，如果是就说明训练成功；确定最佳参数方案

// ------

// 传入新数据进行重新学习，取最大20天数据进行训练学习

#define SIS_SERIES_MIN_DAYS   1    // 最少几天数据
#define SIS_SERIES_MAX_DAYS  21    // 取多少天样本数据
#define SIS_SERIES_GAPS      10    // 取值范围的分割区间数
#define SIS_SERIES_TOPS      10    // 收胜率排名最前的几名 取大于0 并且小于第一名不超过10倍的样本
                                   // 1% 0.9%(OK) 0.3%(OK) 0.1%(NO) -0.5%(NO)

#define SIS_MIN_ORDERS      (5)       // 最低交易次数
#define SIS_MIN_WINR        (0.50)    // 最低胜率
#define SIS_MIN_INCR        (0.05)    // 最低收胜率 * 100
#define SIS_MIN_COST        (0.001)  // 最低手续费

#define SIS_SERIES_STYLE_STUDY  0
#define SIS_SERIES_STYLE_CHECK  1
#define SIS_SERIES_STYLE_WORK   2

#define SIS_SERIES_OUT_NONE    0
#define SIS_SERIES_OUT_ASK1    1
#define SIS_SERIES_OUT_ASK2    2
#define SIS_SERIES_OUT_ASK3    4
#define SIS_SERIES_OUT_ASK4    8

#define SIS_SERIES_OUT_BID1    16
#define SIS_SERIES_OUT_BID2    32
#define SIS_SERIES_OUT_BID3    64
#define SIS_SERIES_OUT_BID4   128

#define SIS_SERIES_OUT_ASK    (SIS_SERIES_OUT_ASK1|SIS_SERIES_OUT_ASK2|SIS_SERIES_OUT_ASK3|SIS_SERIES_OUT_ASK4) 
#define SIS_SERIES_OUT_BID    (SIS_SERIES_OUT_BID1|SIS_SERIES_OUT_BID2|SIS_SERIES_OUT_BID3|SIS_SERIES_OUT_BID4)


///////////////////////////////////////////////////////////////////////////
// 单个因子的参数
///////////////////////////////////////////////////////////////////////////

// 根据 minv 和 maxv 的值转换序列值为 -100 .. 0 .. 100 值；
typedef struct s_sis_ai_factor_wins {
    double  incr;                 // 收益率
    double  winr;                 // 胜率
    double  inwinr;               // 收胜率
    s_sis_double_sides  sides;    // 当日最优收胜率的阀值
} s_sis_ai_factor_wins;

typedef struct s_sis_ai_factor_unit {
    int     index;     // 索引值
	char    name[64];  // 因子的名字
    double  weight;    // 权重的需回硕历史数据 求的最优的配比
    double  dispersed; // 离散度 买卖点取值范围离的越远，离散度越大
    s_sis_double_sides  sides;      // 最优收胜率的阀值    
    s_sis_struct_list  *optimals;   // 最优的参数列表, 由此得到因子阀值的参考值 s_sis_ai_factor_wins
} s_sis_ai_factor_unit;

s_sis_ai_factor_unit *sis_ai_factor_unit_create(const char *);
void sis_ai_factor_unit_destroy(void *);

void sis_ai_factor_unit_calc_optimals(s_sis_ai_factor_unit *);

typedef struct s_sis_ai_factor_weights {
    double   incr;      // 收益率
    double   winr;      // 胜率
    double   inwinr;    // 收胜率
    double  *weights;    // 权重
} s_sis_ai_factor_weights;

s_sis_ai_factor_weights *sis_ai_factor_weights_create(int count);
void sis_ai_factor_weights_destroy(void *);

typedef struct s_sis_ai_factor {
    s_sis_map_int       *fields_map;  // 字段映射表 - 对应 list 的索引值
    s_sis_pointer_list  *fields;      // s_sis_ai_factor_unit   

    // s_sis_pointer_list   *woptimals;   // 权重参数列表, 由此得到权重参考值 s_sis_ai_factor_weights
    s_sis_double_sides   orders;           // 最优收胜率的阀值  
    s_sis_struct_list   *ooptimals;        // 保存最优的指令阀值 s_sis_ai_factor_wins
} s_sis_ai_factor;

s_sis_ai_factor *sis_ai_factor_create(); 
void sis_ai_factor_destroy(s_sis_ai_factor *);

void sis_ai_factor_clone(s_sis_ai_factor *src_, s_sis_ai_factor *des_);

int sis_ai_factor_inc_factor(s_sis_ai_factor *, const char *fn_);
s_sis_ai_factor_unit *sis_ai_factor_get(s_sis_ai_factor *, const char *fn_);

double sis_ai_factor_get_normalization(s_sis_ai_factor *, s_sis_ai_factor_unit *, double in_);

void sis_ai_factor_calc_optimals(s_sis_ai_factor_unit *);

///////////////////////////////////////////////////////////////////////////
//  单个样本的数据(通常指一天的数据)
//////////////////////////////////////////////////////////////////////////

typedef struct s_sis_ai_series_unit_key {
	int    sno;    // 以此为顺序排列
	int    cmd;    // 输出
	double newp;   // 价格
} s_sis_ai_series_unit_key;

typedef struct s_sis_ai_series_unit {
    int                 mark;        // 数据编号 - 通常为日期   
    s_sis_ai_factor    *cur_factors; // 计算前的因子表 当期以此为计算标准
    s_sis_ai_factor    *out_factors; // 计算后的因子表 下期以此为计算标准
    s_sis_struct_list  *klists;      // 结构类型 s_sis_ai_series_unit_key
	s_sis_pointer_list *vlists;      // 动态数据结构类型指针 double *value 单条数据个数为fields的个数
} s_sis_ai_series_unit;

#ifdef __cplusplus
extern "C" {
#endif
s_sis_ai_series_unit *sis_ai_series_unit_create(); 
void sis_ai_series_unit_destroy(void *);
void sis_ai_series_unit_clear(s_sis_ai_series_unit *);

void sis_ai_series_unit_init_factors(s_sis_ai_series_unit *, s_sis_ai_factor *factors_);

// // 根据时间插入对应的数据
int sis_ai_series_unit_inc_key(s_sis_ai_series_unit *,  int sno_, double newp_);
int sis_ai_series_unit_set_val(s_sis_ai_series_unit *, int index_, const char *fn_, double value_);

s_sis_ai_series_unit_key *sis_ai_series_unit_get_key(s_sis_ai_series_unit *, int index_);
double sis_ai_series_unit_get(s_sis_ai_series_unit *, int fidx_, int kidx_);
int sis_ai_series_unit_get_size(s_sis_ai_series_unit *);

#ifdef __cplusplus
}
#endif

////////////////////////////////////////////////////
//  这里是判断交易最优化参数的部分
//  可以传入任意数字列表，然后求出最大收益的双边值
////////////////////////////////////////////////////
// 每笔买卖的最小费用
#define SIS_AI_MIN_VOLS  (100)
#define SIS_AI_MIN_COST  (0.0015)

typedef struct s_sis_ai_order
{
	int8   cmd;    // 买卖方向
	double newp;   // 当前价格
	int    vols;   // 买卖数量
} s_sis_ai_order;

typedef struct s_sis_ai_order_census
{
	int    asks;     // 多单次数
	int    bids;     // 空单次数

	int    ask_oks;  // 盈利多单次数
	int    bid_oks;  // 盈利空单次数

	double ask_income;  // 多单总收益
	double bid_income;  // 空单总收益

	// int    cur_asknums; // 当前多单数量
	// int    cur_bidnums; // 当前空单数量
	int    cur_askv;    // 当前多单持仓量
	int    cur_bidv;    // 当前空单持仓量
	double cur_avgp;    // 当前持仓均价

	// 初始 use_money为 x bid_money 为 y ask_money 为 z
	// 买 开多 x -= m : z = max(-x) || 平空 x -= m : 
	//  ::: B 1000  x:-1000 z:1000
	//    : B 1000  x:-2000 z:2000
	//    : S 1000  x:-1000 z:2000 y:1000
	//    : B 1000  x:-2000 z:2000
	//  ::: S 1000  x: 1000 y:1000
	//    : S 1000  x: 2000 y:2000
	//    : B 1000  x: 1000 y:2000
	//    : B 1000  x:    0 y:2000
	//    : B 1000  x:-1000 y:2000 z:1000

	// 卖 开空 x += m : y += m || 平多 x += m : y += m
	double use_money;   // 可用资金
	double ask_money;   // 买入最大冻结资金 如果 cur_money > 当前资金
	double bid_money;   // 卖出最大资金 - T+1日交易 累计卖出的所有金额

	double ask_rate;    // 买入收益率
	double bid_rate;    // 卖出收益率

	double sum_incr;      // 相对最大收益率 取ask_money和bid_money的最大值
	double sum_money;     // 合计交易金额 - 计算手续费等
	double sum_winr;      // 胜率
	double sum_inwinr;      // 收胜率
} s_sis_ai_order_census;

///////////////////////////////////////////////////////////////////////////
//  多个样本的管理类
//////////////////////////////////////////////////////////////////////////

typedef struct s_sis_optimal_split {
	double ups;      
	double dns;
    double sum_ups;  // 合并后的数量级
    double sum_dns;  // 合并后的数量级
    s_sis_double_split split; // 合并后的区域
} s_sis_optimal_split;

typedef struct s_sis_ai_series_class {

	s_sis_ai_factor      *factors;  // 因子映射 最新计算后的数据
    s_sis_pointer_list   *studys;   // s_sis_ai_series_unit 结构 原始数据
    // 保存最近 21 天数据，需要保存，方便新数据来替换

    // s_sis_ai_series_unit  *check_unit;  // 当前测试的节点  
    // s_sis_pointer_list   *checks;   // s_sis_ai_series_unit 结构 对原始数据归一化后的数据，筛除所有指标归零的记录
    // s_sis_struct_list    *ranges;   // s_sis_ai_series_range 学习样本中每个因子的取值范围
} s_sis_ai_series_class;

#ifdef __cplusplus
extern "C" {
#endif
s_sis_ai_series_class *sis_ai_series_class_create(); 
void sis_ai_series_class_destroy(s_sis_ai_series_class *);

// 仅仅清理掉日线数据，并不清理因子文字数据，可以重复使用因子数据
void sis_ai_series_class_clear(s_sis_ai_series_class *);

// 初始化因子,必须一次性初始化所有因子，然后才能添加后续数据
int sis_ai_series_class_inc_factor(s_sis_ai_series_class *, const char *fn_);

// --------  这里开始输入数据 ---------- //
// 需要分解成类别（日期）和索引号（时间）
s_sis_ai_series_unit *sis_ai_series_class_get_study(s_sis_ai_series_class *, int mark_);

// ---------  这里开始处理数据 ---------- // 
// 数据准备 根据 sources 中的数据 计算出各个指标的阀值和权重
// 采集某日的最优解, 继承agof 并传导到 curf 中
// *** 单因子用收益率 权重用胜率 买卖力道用综合 ***
void sis_ai_factor_optimal_step1(s_sis_ai_series_unit *);
void sis_ai_factor_optimal_step2(s_sis_ai_series_unit *);
void sis_ai_factor_optimal_step3(s_sis_ai_series_unit *);

// 返回最后的特征数据，用于测试check的计算
s_sis_ai_factor *sis_ai_series_class_study(s_sis_ai_series_class *);


// 申请一个新的数据区用于测试数据
s_sis_ai_series_unit *sis_ai_series_class_new_check(int mark_, s_sis_ai_factor *factor_);
// 仅仅根据前期 factor_ 求出某日的收益率 和 胜率
int sis_ai_series_class_calc_check(s_sis_ai_series_unit *, s_sis_ai_order_census *);

#ifdef __cplusplus
}
#endif

#endif /* _SIS_AI_SERIES_H */