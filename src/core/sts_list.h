#ifndef _STS_LIST_H
#define _STS_LIST_H

#include "sts_core.h"
#include "sts_str.h"
#include "adlist.h"
#include "sds.h"
#include "zmalloc.h"
// 定义一个结构体链表
// 定义一个指针读写链表
// 定义一个字符串读写链表

///////////////////////////////////////////////////////////////////////////
//------------------------s_sts_struct_list ---------------------------------//
//  固定长度的列表
//////////////////////////////////////////////////////////////////////////
#define STRUCT_LIST_NORMAL   0
#define STRUCT_LIST_POINTER  1

typedef struct s_sts_struct_list {
	int		     maxcount; //总数
	int		     count;    //当前个数
	int          len;      //每条记录的长度
	int          mode;     //区分什么类型的链表
	void        *buffer;   //必须是mallco申请的char*类型
	void(*free)(void *);   //==NULL 不释放对应内存
} s_sts_struct_list;

#define BUFFLIST_STEP_ROW 128  //默认增加的记录数

s_sts_struct_list *sts_struct_list_create(int len_, void *in_, int inlen_); //len_为每条记录长度
void sts_struct_list_destroy(s_sts_struct_list *list_);
void sts_struct_list_clear(s_sts_struct_list *list_);
void struct_list_setsize(s_sts_struct_list *list_, int len_);

int sts_struct_list_push(s_sts_struct_list *, void *in_);
int sts_struct_list_insert(s_sts_struct_list *, int index_, void *in_);
int sts_struct_list_update(s_sts_struct_list *, int index_, void *in_);
void *sts_struct_list_get(s_sts_struct_list *, int index_);
void *sts_struct_list_next(s_sts_struct_list *list_, void *, int offset);

int sts_struct_list_set(s_sts_struct_list *, void *in_, int inlen_);

void sts_struct_list_limit(s_sts_struct_list *, int limit_);
int sts_struct_list_clone(s_sts_struct_list *src_, s_sts_struct_list *dst_, int limit_);
int sts_struct_list_delete(s_sts_struct_list *src_, int start_, int count_);
int sts_struct_list_pack(s_sts_struct_list *list_);

// 获取指针的位置编号
int sts_struct_list_pto_recno(s_sts_struct_list *list_,void *);

///////////////////////////////////////////////////////////////////////////
//------------------------s_pointer_list --------------------------------//
//  存储指针的列表,依赖于struct_list,记录长度为sizeof(char *)
///////////////////////////////////////////////////////////////////////////
//!!!要测试
s_sts_struct_list *sts_pointer_list_create(); 

#define sts_pointer_list_destroy sts_struct_list_destroy
#define sts_pointer_list_clear sts_struct_list_clear
#define sts_pointer_list_setlen struct_list_setlen
#define sts_pointer_list_push sts_struct_list_push
#define sts_pointer_list_update sts_struct_list_update
#define sts_pointer_list_get sts_struct_list_get
#define sts_pointer_list_insert sts_struct_list_insert
#define sts_pointer_list_delete sts_struct_list_delete

int sts_pointer_list_indexof(s_sts_struct_list *list_, void *in_);
int sts_pointer_list_find_and_update(s_sts_struct_list *, void *finder_, void *in_);
int sts_pointer_list_find_and_delete(s_sts_struct_list *list_, void *finder_);

///////////////////////////////////////////////////////////////////////////
//------------------------s_sts_string_list --------------------------------//
//  存储不定长字符串的列表，
///////////////////////////////////////////////////////////////////////////
#define STRING_LIST_RD  1
#define STRING_LIST_WR  2

typedef struct s_sts_string_list {
	int    permissions;     //权限
	char*  m_ptr_r;         // 保存的只读字符串
	s_sts_struct_list *strlist; //存储指针列表 --free为空只读 不为空可写
} s_sts_string_list;

s_sts_string_list *sts_string_list_create_r(); //只读
s_sts_string_list *sts_string_list_create_w(); //读写
void sts_string_list_destroy(s_sts_string_list *list_);
void sts_string_list_clear(s_sts_string_list *list_);

// read & write
int sts_string_list_load(s_sts_string_list *list_, const char *in_, size_t inlen_, const char *sign);
const char *sts_string_list_get(s_sts_string_list *list_, int index_);
int sts_string_list_getsize(s_sts_string_list *list_);

int sts_string_list_indexof(s_sts_string_list *list_, const char *in_);
int sts_string_list_indexofcase(s_sts_string_list *list_, const char *in_);

int sts_string_list_update(s_sts_string_list *list_, int index_, const char *in_, size_t inlen);
//比较字符串地址并修改，字符串比较应该使用string_list_indexof&sts_string_list_update
int sts_string_list_find_and_update(s_sts_string_list *list_, char *finder_, const char *in_, size_t inlen);
int sts_string_list_insert(s_sts_string_list *list_, int index_, const char *in_, size_t inlen);
int sts_string_list_push(s_sts_string_list *list_, const char *in_, size_t inlen);

int sts_string_list_delete(s_sts_string_list *list_, int index_);
//比较字符串地址并删除，字符串比较应该使用string_list_indexof&sts_string_list_delete
int sts_string_list_find_and_delete(s_sts_string_list *list_, const char *finder_);


///////////////////////////////////////////////////////////////////////////
//------------------------listNode --------------------------------//
//  操作listNode列表的函数
///////////////////////////////////////////////////////////////////////////
listNode *sts_sdsnode_create(const void *in, size_t inlen);
void sts_sdsnode_destroy(listNode *node);

listNode *sts_sdsnode_offset_node(listNode *node_, int offset);
listNode *sts_sdsnode_last_node(listNode *node_);
listNode *sts_sdsnode_first_node(listNode *node_);
listNode *sts_sdsnode_push(listNode *node_, const void *in, size_t inlen);
listNode *sts_sdsnode_update(listNode *node_, const void *in, size_t inlen);
listNode *sts_sdsnode_clone(listNode *node_);
int sts_sdsnode_get_size(listNode *node_);
int sts_sdsnode_get_count(listNode *node_);

#endif //_STS_LIST_H