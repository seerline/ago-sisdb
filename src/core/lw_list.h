#ifndef _LW_LIST_H
#define _LW_LIST_H

#include "lw_base.h"
#include "lw_string.h"
#include "adlist.h"
#include "sds.h"

// 定义一个结构体链表
// 定义一个指针读写链表
// 定义一个字符串读写链表

///////////////////////////////////////////////////////////////////////////
//------------------------s_struct_list ---------------------------------//
//  固定长度的列表
//////////////////////////////////////////////////////////////////////////
#define STRUCT_LIST_NORMAL   0
#define STRUCT_LIST_POINTER  1

typedef struct s_struct_list {
	int		     maxcount; //总数
	int		     count;    //当前个数
	int          len;      //每条记录的长度
	int          mode;     //区分什么类型的链表
	void        *buffer;   //必须是mallco申请的char*类型
	void(*free)(void *);   //==NULL 不释放对应内存
} s_struct_list;

#define BUFFLIST_STEP_ROW 128  //默认增加的记录数

s_struct_list *create_struct_list(int len_, void *in_, int inlen_); //len_为每条记录长度
void destroy_struct_list(s_struct_list *list_);
void clear_struct_list(s_struct_list *list_);
void struct_list_setsize(s_struct_list *list_, int len_);

int struct_list_push(s_struct_list *, void *in_);
int struct_list_insert(s_struct_list *, int index_, void *in_);
int struct_list_update(s_struct_list *, int index_, void *in_);
void *struct_list_get(s_struct_list *, int index_);
void *struct_list_next(s_struct_list *list_, void *, int offset);

int struct_list_set(s_struct_list *, void *in_, int inlen_);

void struct_list_limit(s_struct_list *, int limit_);
int struct_list_clone(s_struct_list *src_, s_struct_list *dst_, int limit_);
int struct_list_delete(s_struct_list *src_, int start_, int count_);
int struct_list_pack(s_struct_list *list_);

int struct_list_pto_recno(s_struct_list *list_,void *);

///////////////////////////////////////////////////////////////////////////
//------------------------s_pointer_list --------------------------------//
//  存储指针的列表,依赖于struct_list,记录长度为sizeof(char *)
///////////////////////////////////////////////////////////////////////////
//!!!要测试
s_struct_list *create_pointer_list(); 

#define destroy_pointer_list destroy_struct_list
#define clear_pointer_list clear_struct_list
#define pointer_list_setlen struct_list_setlen
#define pointer_list_push struct_list_push
#define pointer_list_update struct_list_update
#define pointer_list_get struct_list_get
#define pointer_list_insert struct_list_insert
#define pointer_list_delete struct_list_delete

int pointer_list_indexof(s_struct_list *list_, void *in_);
int pointer_list_find_and_update(s_struct_list *, void *finder_, void *in_);
int pointer_list_find_and_delete(s_struct_list *list_, void *finder_);

///////////////////////////////////////////////////////////////////////////
//------------------------s_string_list --------------------------------//
//  存储不定长字符串的列表，
///////////////////////////////////////////////////////////////////////////
#define STRING_LIST_RD  1
#define STRING_LIST_WR  2

typedef struct s_string_list {
	int    permissions;     //权限
	char*  m_ptr_r;         // 保存的只读字符串
	s_struct_list *strlist; //存储指针列表 --free为空只读 不为空可写
} s_string_list;

s_string_list *create_string_list_r(); //只读
s_string_list *create_string_list_w(); //读写
void destroy_string_list(s_string_list *list_);
void clear_string_list(s_string_list *list_);

// read & write
int string_list_load(s_string_list *list_, const char *in_, size_t inlen_, const char *sign);
const char *string_list_get(s_string_list *list_, int index_);
int string_list_getsize(s_string_list *list_);

int string_list_indexof(s_string_list *list_, const char *in_);
int string_list_indeofcase(s_string_list *list_, const char *in_);

int string_list_update(s_string_list *list_, int index_, const char *in_, size_t inlen);
//比较字符串地址并修改，字符串比较应该使用string_list_indexof&string_list_update
int string_list_find_and_update(s_string_list *list_, char *finder_, const char *in_, size_t inlen);
int string_list_insert(s_string_list *list_, int index_, const char *in_, size_t inlen);
int string_list_push(s_string_list *list_, const char *in_, size_t inlen);

int string_list_delete(s_string_list *list_, int index_);
//比较字符串地址并删除，字符串比较应该使用string_list_indexof&string_list_delete
int string_list_find_and_delete(s_string_list *list_, const char *finder_);


///////////////////////////////////////////////////////////////////////////
//------------------------listNode --------------------------------//
//  操作listNode列表的函数
///////////////////////////////////////////////////////////////////////////
listNode *create_sdsnode(const void *in, size_t inlen);
void destroy_sdsnode(listNode *node);

listNode *sdsnode_offset_node(listNode *node_, int offset);
listNode *sdsnode_last_node(listNode *node_);
listNode *sdsnode_first_node(listNode *node_);
listNode *sdsnode_push(listNode *node_, const void *in, size_t inlen);
listNode *sdsnode_update(listNode *node_, const void *in, size_t inlen);
listNode *sdsnode_clone(listNode *node_);
int sdsnode_get_size(listNode *node_);
int sdsnode_get_count(listNode *node_);

#endif //_LW_LIST_H