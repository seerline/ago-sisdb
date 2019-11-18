#ifndef _SIS_AI_BAYES_H
#define _SIS_AI_BAYES_H

#include <sis_core.h>
#include <sis_list.h>
#include <sis_time.h>
#include <sis_map.h>
#include <sis_math.h>

#pragma pack(push, 1)

#define AI_BAYES_TYPE_FACTOR    1   // 条件概率 处理多值
#define AI_BAYES_TYPE_SERIES    2   // 概率密度 处理 < 1连续值 
#define AI_BAYES_TYPE_RANDOM    3   // 随机特征值 处理 >=1 连续值

#define AI_BAYES_MID     0   // 横盘 
#define AI_BAYES_ADD     1   // 上涨 买入
#define AI_BAYES_DEC     2   // 下跌 卖出
// 贝叶斯属性
#define AI_BAYES_DIV1    0   // 1区
#define AI_BAYES_DIV2    1   // 2区
#define AI_BAYES_DIV3    2   // 3区

#define AI_BAYES_MAX_GAP    16   // 对于连续数据 最大设置的间隔数

typedef struct s_ai_bayes_input
{
	uint8  style;   // 要素类型
	uint8  class;   // 青绿 深黑
	double series;  // 密度 - < 1连续值
} s_ai_bayes_input;

typedef struct s_ai_bayes_value
{	
	uint8  class;
	uint32 count; 
} s_ai_bayes_value;

typedef struct s_ai_bayes_gain_factor
{	
	uint8  class;
	uint32 adds; 
	uint32 mids; 
	uint32 decs; 
	double gain;
} s_ai_bayes_gain_factor;

typedef struct s_ai_bayes_gain_series
{	
	uint8  style;
	double series;
} s_ai_bayes_gain_series;

typedef struct s_ai_bayes_box_random
{
	double left;   // AI_BAYES_TYPE_SERIES 的左值 三分
	double right;  // AI_BAYES_TYPE_SERIES 的右值 三分
}s_ai_bayes_box_random;

// 对单一结果的
typedef struct s_ai_bayes_unit
{
	char  name[32];  // 色泽
	s_sis_struct_list  *samples;  // s_ai_bayes_input 数据集

	s_sis_struct_list  *classify;   // s_ai_bayes_value 青绿 深黑 各有多少条记录 AI_BAYES_TYPE_FACTOR
	double avg;  // 均值 - 仅对连续值 AI_BAYES_TYPE_SERIES
	double vari; // 方差 - 仅对连续值 AI_BAYES_TYPE_SERIES
} s_ai_bayes_unit;

// 对全部结果的
typedef struct s_ai_bayes_style
{
	char   name[32];  // 色泽
	uint8  style;     // 要素类型
	bool   used;      // 是否使用该类型  
	double gain;      // 信息增益

	double serise_mid;    // AI_BAYES_TYPE_SERIES 的中值 二分
	double serise_left;   // AI_BAYES_TYPE_SERIES 的左值 三分
	double serise_right;  // AI_BAYES_TYPE_SERIES 的右值 三分
}s_ai_bayes_style;

// 贝叶斯类
typedef struct s_ai_bayes_class
{
	s_sis_map_buffer  *style_list;    // s_ai_bayes_style 特征类型

	s_sis_map_pointer *factor_add_lists;  // s_ai_bayes_unit 特征列表 key = 属性名 色泽 密度
	s_sis_map_pointer *factor_mid_lists;  // s_ai_bayes_unit 特征列表
	s_sis_map_pointer *factor_dec_lists;  // s_ai_bayes_unit 特征列表

	s_sis_map_buffer *factor;  // s_ai_bayes_input 特征列表 key = 属性名 色泽 密度

} s_ai_bayes_class;

// 先根据各个特征的增益率来计算各自的权重，再根据权重进行分类，

#pragma pack(pop)

s_ai_bayes_unit *sis_ai_bayes_unit_create(const char *);
void sis_ai_bayes_unit_destroy(s_ai_bayes_unit *unit_);

// void sis_ai_bayes_unit_clear(s_ai_bayes_unit *unit_);
int sis_ai_bayes_unit_push_factor(s_ai_bayes_unit *unit_, int8 value_);
int sis_ai_bayes_unit_push_series(s_ai_bayes_unit *unit_, double value_);
int sis_ai_bayes_unit_push_random(s_ai_bayes_unit *unit_, double value_);

int sis_ai_bayes_unit_get_num(s_ai_bayes_unit *unit_, uint8  class);

////////////////////////////////
//
////////////////////////////////
s_ai_bayes_class *sis_ai_bayes_create();
void sis_ai_bayes_destroy(s_ai_bayes_class *cls_);

void sis_ai_bayes_study_init(s_ai_bayes_class *cls_);
int sis_ai_bayes_study_push_factor(s_ai_bayes_class *unit_, 
	uint8 status_, const char *key_, int8 value_);
int sis_ai_bayes_study_push_series(s_ai_bayes_class *unit_, 
	uint8 status_, const char *key_, double value_);
int sis_ai_bayes_study_push_random(s_ai_bayes_class *unit_, 
	uint8 status_, const char *key_, double value_);
// 评估所有的类型， 低于最大值10倍的设置为不处理
int sis_ai_bayes_study_output(s_ai_bayes_class *cls_);

/////////////////////
// 对单一输入特征，求结果
void sis_ai_bayes_check_init(s_ai_bayes_class *cls_); 
// 写入 factor
void sis_ai_bayes_check_push_factor(s_ai_bayes_class *cls_, const char *key_, int8 value_); 
void sis_ai_bayes_check_push_series(s_ai_bayes_class *cls_, const char *key_, double value_); 
void sis_ai_bayes_check_push_random(s_ai_bayes_class *cls_, const char *key_, double value_); 

double sis_ai_bayes_check_output(s_ai_bayes_class *cls_, uint8 status_); // 得到综合概率

/////////////////////
//
/////////////////////
s_ai_bayes_style *sis_ai_bayes_get_style(s_ai_bayes_class *cls_, const char *key_);

// 以传入数组为分割线，返回对应的数组下标，用于非连续值的抽象化
int sis_ai_bayes_classify(double in_, int n, double ins[]);

#endif
//_SIS_LEVEL2_T0_H