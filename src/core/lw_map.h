#ifndef _LW_MAP_H
#define _LW_MAP_H

#include "lw_base.h"
#include "sds.h"
#include "dict.h"

// 定义一个指针类型的字典  string -- void*
// 定义一个整数类型的字典  string -- int

#pragma pack(push,1)

typedef struct s_kv_int{
	const char *key;
	uint64_t	val;
}s_kv_int;
typedef struct s_kv_pair{
	const char *key;
	const char *val;
}s_kv_pair;

#pragma pack(pop)

//////////////////////////////////////////
//  s_map_buffer 基础定义
///////////////////////////////////////////////

#define s_map_buffer dict
#define s_map_pointer s_map_buffer
#define s_map_int s_map_buffer
#define s_map_sds s_map_buffer

s_map_buffer *create_map_buffer();
void destroy_map_buffer(s_map_buffer *);
void clear_map_buffer(s_map_buffer *);
void *get_map_buffer(s_map_buffer *, const char *key_);
int  set_map_buffer(s_map_buffer *, const char *key_, void *value_); 
//设置key对应的数据引用，必须为一个指针，并不提供实体，

s_map_pointer *create_map_pointer();
#define destroy_map_pointer destroy_map_buffer
#define clear_map_pointer clear_map_buffer
#define get_map_pointer get_map_buffer
#define set_map_pointer(a,b,c) set_map_buffer(a,b,c)

s_map_int *create_map_int();
#define destroy_map_int destroy_map_buffer
#define clear_map_int clear_map_buffer
uint64_t get_map_int(s_map_int *, const char *key_);
int set_map_int(s_map_int *, const char *key_, uint64_t value_);

s_map_sds *create_map_sds();
#define destroy_map_sds destroy_map_buffer
#define clear_map_sds clear_map_buffer
#define get_map_sds get_map_buffer
#define set_map_sds set_map_buffer

#if 0

void release_key_table();
int create_key_table();

char *match_key_table_belong(const char *key_);
int match_key_table_type(const char *key_);
sds merge_key_table( const char *code, int type_);

int read_dict_from_disk(dict *db_, char *filename_);
int write_dict_to_disk(dict *db_, char *filename_);

unsigned int dictSdsCaseHash(const void *key);
int dictSdsKeyCaseCompare(void *privdata, const void *key1, const void *key2);
void dictSdsDestructor(void *privdata, void *val);
/*
class c_sds {
	sds str;
public:
	c_sds(const char *s_){ str = sdsnew(s_); };
	~c_sds(){ sdsfree(str); };
	operator sds() const { return str; };
};*/


enum SAVE_DISK_DEF {
	SAVE_DISK_NONE = 0,
	SAVE_DISK_CFG = (1 << 0),
	SAVE_DISK_CODE = (1 << 1),
	SAVE_DISK_MAP = (1 << 2),
	SAVE_DISK_MIN = (1 << 3),
	SAVE_DISK_TICK = (1 << 4),

	SAVE_DISK_INFO = (1 << 5),  //包含除权信息
	SAVE_DISK_HIS = (1 << 6),
};


#define DATA_TYPE_STRING    0
#define DATA_TYPE_SDS       1
#define DATA_TYPE_NODE      2
#define DATA_TYPE_INT       3
#define DATA_TYPE_JSON      4
#define DATA_TYPE_BUFFER    5

listNode *get_data_of_node(int datatype_, void *value_, size_t len_ = 0);

class c_stock_data_table {
	char m_market[8];
public:
	dict *m_db_s;
	c_stock_data_table();
	~c_stock_data_table();

	int set_value(int datatype_, int type_, char *code_, void *value_, size_t len_ = 0); //设置市场变量
	void set_market(const char *market_);
	
	listNode *get_stock_node_by_sds(sds key);
	listNode *get_stock_node(int type_, const char *code_);

	int read_dict_from_disk(char *filename_);
	int write_dict_to_disk(char *filename_);

	int set(char *key_, s_buffer_list *value_);
	listNode *get(char *key_);
};
class c_fixed_table {  //通用的字典，取代map string -- void *, 只能明确知道val的长度才能使用
public:
	dict *m_db_s;
	c_fixed_table();
	~c_fixed_table();
	int   set(char *key_, void *value_); //设置key对应的数据引用，必须为一个指针，并不提供实体，
	void *get(char *key_); //获取

	//void(*clear)(void *);
};
class c_int_table {  //通用的字典，取代map string -- int, 
public:
	dict *m_db_s;
	c_int_table();
	~c_int_table();
	int  set(char *key_, int value_); //设置key对应的数据引用，
	int  get(char *key_); //获取

	//void(*clear)(void *);
};
class c_list_table {  //val 为 s_buffer_list * 
public:
	dict *m_db_s;
	c_list_table();
	~c_list_table();
	int   set(char *key_, s_buffer_list *value_); 
	s_buffer_list *get(char *key_); //获取

};
#endif
#endif