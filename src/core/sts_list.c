
#include "sts_list.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  s_sts_struct_list
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

s_sts_struct_list *sts_struct_list_create(int len_, void *in_, int inlen_)
{
	if (len_ < 1) {
		return NULL;
	}
	s_sts_struct_list *sbl = (s_sts_struct_list*)zmalloc(sizeof(s_sts_struct_list));
	sbl->len = len_;
	sbl->maxcount = 0;
	sbl->count = 0;
	sbl->buffer = NULL;
	sbl->free = NULL;
	sbl->mode = STRUCT_LIST_NORMAL;
	if (in_ &&inlen_ > 0){
		sts_struct_list_set(sbl, in_, inlen_);
	}
	return sbl;
}
void sts_struct_list_destroy(s_sts_struct_list *list_){
	sts_struct_list_clear(list_);
	if (list_->buffer) zfree(list_->buffer);
	list_->buffer = NULL;
	list_->maxcount = 0;
	zfree(list_);
}
void sts_struct_list_clear(s_sts_struct_list *list_)
{
	if (list_->mode == STRUCT_LIST_POINTER) {
		char **ptr = (char **)list_->buffer;
		for (int i = 0; i < list_->count; i++)
		{
			if (list_->free) {
				list_->free(ptr[i]);
				ptr[i] = NULL;
			}
		}
	}
	list_->count = 0;
}
void struct_list_grow(s_sts_struct_list *list_, int len_){
	if (len_ < list_->maxcount) {
		return;
	}
	int maxlen = len_;
	if (len_ < 8) { maxlen = 8; }
	else if (len_ >= 8 && len_ < 64)  { maxlen = 64; }
	else if (len_ >= 64 && len_ < 256) { maxlen = 256; }
	else { maxlen = len_ + BUFFLIST_STEP_ROW; }

	list_->buffer = zrealloc(list_->buffer, maxlen * list_->len);
	list_->maxcount = maxlen;
}
void struct_list_setsize(s_sts_struct_list *list_, int len_)
{
	if (len_ < list_->maxcount) {
		return;
	}
	list_->buffer = zrealloc(list_->buffer, len_ * list_->len);
	list_->maxcount = len_;
}

int sts_struct_list_push(s_sts_struct_list *list_, void *in_){
	struct_list_grow(list_, list_->count + 1);
	if (list_->mode == STRUCT_LIST_POINTER) {
		char **ptr = (char **)list_->buffer;
		ptr[list_->count] = (char *)in_;
	}
	else
	{
		memmove((char *)list_->buffer + (list_->count * list_->len), in_, list_->len);
	}
	list_->count++;
	return list_->count;
}
int sts_struct_list_update(s_sts_struct_list *list_, int index_, void *in_)
{
	if (index_ >= 0 && index_ < list_->count) 
	{
		if (list_->mode == STRUCT_LIST_POINTER) 
		{
			char **ptr = (char **)list_->buffer;
			if (list_->free)
			{
				list_->free(ptr[index_]);
			}
			ptr[index_] = (char *)in_;
		}
		else {
			memmove((char *)list_->buffer + (index_*list_->len), in_, list_->len);
		}
		return index_;
	} 
#if 0
	else
	{
		if (index_ < 0) {
			index_ = list_->count;
		}
		struct_list_grow(list_, index_ + 1);    // 这里可能会跳跃最后一条记录 
		if (list_->mode == STRUCT_LIST_POINTER) 
		{
			char **ptr = (char **)list_->buffer;
			ptr[index_] = (char *)in_;
		}
		else
		{
			memmove((char *)list_->buffer + (index_ * list_->len), in_, list_->len);
		}
		list_->count = index_ + 1;
	}
#endif
	return -1;
}
int sts_struct_list_insert(s_sts_struct_list *list_, int index_, void *in_)
{
	if (index_ < 0 || index_ > list_->count - 1) {
		return -1;
	}
	struct_list_grow(list_, list_->count + 1);
	memmove((char *)list_->buffer + ((index_ + 1) * list_->len), (char *)list_->buffer + (index_ * list_->len),
		(list_->count - index_)*list_->len);

	if (list_->mode == STRUCT_LIST_POINTER)
	{
		char **ptr = (char **)list_->buffer;
		ptr[index_] = (char *)in_;
	}
	else
	{
		memmove((char *)list_->buffer + (index_ * list_->len), in_, list_->len);
	}
	list_->count++;
	return index_;

}
void *sts_struct_list_get(s_sts_struct_list *list_, int index_)
{
	char *rtn = NULL;
	if (index_ >= 0 && index_ < list_->count) {
		if (list_->mode == STRUCT_LIST_POINTER) {
			char **ptr = (char **)list_->buffer;
			rtn = ptr[index_];
		}
		else{
			rtn = (char *)list_->buffer + (index_*list_->len);
		}
	}
	return rtn;
}
void *sts_struct_list_next(s_sts_struct_list *list_, void *current_, int offset)
{
	if (list_->mode == STRUCT_LIST_POINTER) {
		return current_;
	}
	char *rtn = (char *)current_ + offset*list_->len;
	if (rtn >= (char *)list_->buffer && rtn <= (char *)list_->buffer + (list_->count - 1)*list_->len)
	{
		return rtn;
	}
	else{
		return NULL;
	}
}

int sts_struct_list_set(s_sts_struct_list *list_, void *in_, int inlen_){
	if (list_->mode == STRUCT_LIST_POINTER) {
		return 0;
	}
	int count = inlen_ / list_->len;
	struct_list_setsize(list_, count);
	if (in_){
		memmove(list_->buffer, in_, inlen_);
	}
	else {
		memset(list_->buffer, 0, count * list_->len);
	}
	list_->count = count;
	return count;
}

void sts_struct_list_limit(s_sts_struct_list *list_, int limit_)
{
	if (list_->mode == STRUCT_LIST_POINTER) {
		return;
	}
	if (limit_ < 1 || limit_ > list_->count) {
		return; 
	}
	int offset = list_->count - limit_;
	memmove(list_->buffer, (char *)list_->buffer + (offset*list_->len), limit_ * list_->len);
	list_->count = limit_;
}
int sts_struct_list_clone(s_sts_struct_list *src_, s_sts_struct_list *list_, int limit_)
{
	if (list_->mode == STRUCT_LIST_POINTER) {
		return 0;
	}
	int count;
	int offset;
	if (limit_ < 1 || limit_ > src_->count) {
		count = src_->count;
	}
	else {
		count = limit_;
	}
	offset = src_->count - count;
	sts_struct_list_set(list_, (char *)src_->buffer + (offset*src_->len), count * list_->len);
	return count;
}
int sts_struct_list_pack(s_sts_struct_list *list_)
{
	if (list_->mode == STRUCT_LIST_POINTER) {
		return 0;
	}
	if (!list_) {
		return 0;
	}
	char *tmp = (char *)zmalloc(list_->count* list_->len);
	memmove(tmp, list_->buffer, list_->count*list_->len);
	zfree(list_->buffer);
	list_->buffer = tmp;
	list_->maxcount = list_->count;
	return list_->count;
}
int sts_struct_list_delete(s_sts_struct_list *list_, int start_, int count_)
{
	if (start_ < 0 || count_ < 1 || start_ + count_ > list_->count) {
		return 0;
	}

	if (list_->mode == STRUCT_LIST_POINTER) {
		char **ptr = (char **)list_->buffer;
		for (int i = start_; i < start_ + count_; i++){
			if (list_->free)
			{
				list_->free(ptr[i]);
			}
		}
	}
	memmove((char *)list_->buffer + (start_*list_->len), (char *)list_->buffer + ((start_ + count_)*list_->len),
		(list_->count - count_ - start_) * list_->len);

	list_->count = list_->count - count_;
	return count_;
}

int sts_struct_list_pto_recno(s_sts_struct_list *list_, void *p_)
{
	if (list_->mode == STRUCT_LIST_POINTER) { return 0; } 
	if ((char *)p_<(char *)list_->buffer ||
		(char *)p_>((char *)list_->buffer + list_->len*(list_->count - 1))){
		return -1;
	}
	return (int)(((char *)p_ - (char *)list_->buffer) / list_->len);
}
///////////////////////////////////////////////////////////////////////////
//------------------------s_pointer_list --------------------------------//
//  存储指针的列表,依赖于struct_list,记录长度为sizeof(char *)
///////////////////////////////////////////////////////////////////////////
s_sts_struct_list *sts_pointer_list_create()
{
	s_sts_struct_list *sbl = (s_sts_struct_list*)zmalloc(sizeof(s_sts_struct_list));
	sbl->len = sizeof(char *);
	sbl->maxcount = 0;
	sbl->count = 0;
	sbl->buffer = NULL;
	sbl->free = NULL;
	sbl->mode = STRUCT_LIST_POINTER;
	return sbl;
}
int sts_pointer_list_indexof(s_sts_struct_list *list_, void *in_)
{
	if (list_->mode != STRUCT_LIST_POINTER) return -1;
	char **ptr = (char **)list_->buffer;
	for (int i = 0; i < list_->count; i++)
	{
		if (ptr[i] == in_){
			return i;
		}
	}
	return -1;
}
int sts_pointer_list_find_and_update(s_sts_struct_list *list_, void *finder_, void *in_)
{
	if (list_->mode != STRUCT_LIST_POINTER) return  -1;
	int index = sts_pointer_list_indexof(list_, finder_);
	return sts_struct_list_update(list_, index, in_);
}
int sts_pointer_list_find_and_delete(s_sts_struct_list *list_, void *finder_)
{
	if (list_->mode != STRUCT_LIST_POINTER) return  -1;
	char **ptr = (char **)list_->buffer;
	for (int i = 0; i < list_->count; i++)
	{
		if (ptr[i] == finder_){
			if (list_->free){
				list_->free(ptr[i]);
			}
			memmove(&ptr[i], &ptr[i + 1], (list_->count - 1 - i) * list_->len);
			list_->count--;
			return i;
		}
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////
//------------------------s_sts_string_list --------------------------------//
//  存储不定长字符串的列表，
///////////////////////////////////////////////////////////////////////////
s_sts_string_list *sts_string_list_create_r() //只读
{
	s_sts_string_list *l = (s_sts_string_list*)zmalloc(sizeof(s_sts_string_list));
	l->strlist = sts_pointer_list_create();
	l->permissions = STRING_LIST_RD;
	return l;
}
s_sts_string_list *sts_string_list_create_w() //读写
{
	s_sts_string_list *l = (s_sts_string_list*)zmalloc(sizeof(s_sts_string_list));
	l->strlist = sts_pointer_list_create();
	l->permissions = STRING_LIST_WR;
	l->strlist->free = zfree;
	return l;
}
void sts_string_list_destroy(s_sts_string_list *list_)
{
	sts_string_list_clear(list_);
	zfree(list_);
}
void sts_string_list_clear(s_sts_string_list *list_)
{
	if (list_->permissions == STRING_LIST_RD && list_->m_ptr_r){
		zfree(list_->m_ptr_r);
		list_->m_ptr_r = NULL;
	}
	sts_pointer_list_clear(list_->strlist);
}
// read & write
int sts_string_list_load(s_sts_string_list *list_, const char *in_, size_t inlen_, const char *sign)
{
	sts_string_list_clear(list_);

	if (strlen(in_) == 0) return 0;
	char *token = NULL;
	char *src = (char *)zmalloc(inlen_+1);
	sts_strncpy(src, inlen_ + 1, in_, inlen_);

	if (list_->permissions == STRING_LIST_RD){
		list_->m_ptr_r = src;
	}
	char *des = NULL;
	size_t len;
	while ((token = strsep(&src, sign)) != NULL)
	{
		sts_trim(token);		
		if (list_->permissions == STRING_LIST_WR){
			len = strlen(token);
			des = (char *)zmalloc(len + 1);
			sts_strncpy(des, len + 1, token, len);
		}
		else
		{ 
			des = token;
		}
		sts_pointer_list_push(list_->strlist, des);
	}
	if (list_->permissions == STRING_LIST_WR){
		zfree(src);
	}
	return list_->strlist->count;
}
const char *sts_string_list_get(s_sts_string_list *list_, int index_)
{
	return (const char *)sts_pointer_list_get(list_->strlist, index_);
}
int sts_string_list_getsize(s_sts_string_list *list_)
{
	return list_->strlist->count;
}

int sts_string_list_indexof(s_sts_string_list *list_, const char *in_)
{
	if (!in_) return -1;
	for (int i = 0; i < list_->strlist->count; i++)
	{
		if (!strcmp(sts_string_list_get(list_, i), in_)) return i;
	}
	return -1;
}
int sts_string_list_indexofcase(s_sts_string_list *list_, const char *in_)
{
	if (!in_) return -1;
	for (int i = 0; i < list_->strlist->count; i++)
	{
		if (!strcasecmp(sts_string_list_get(list_, i), in_)) return i;
	}
	return -1;
}
int sts_string_list_update(s_sts_string_list *list_, int index_, const char *in_, size_t inlen)
{
	if (list_->permissions != STRING_LIST_WR) return -1;

	char *str = (char *)zmalloc(inlen + 1);
	sts_strncpy(str, inlen + 1, in_, inlen);
	int index = sts_pointer_list_update(list_->strlist, index_, str);
	if (index < 0) zfree(str);
	return index;
}
int sts_string_list_find_and_update(s_sts_string_list *list_, char *finder_, const char *in_, size_t inlen)
{
	if (list_->permissions != STRING_LIST_WR) return -1;

	char *str = (char *)zmalloc(inlen + 1);
	sts_strncpy(str, inlen + 1, in_, inlen);
	int index = sts_pointer_list_find_and_update(list_->strlist, finder_, str);
	if (index < 0) zfree(str);
	return index;
}
int sts_string_list_insert(s_sts_string_list *list_, int index_, const char *in_, size_t inlen)
{
	if (list_->permissions != STRING_LIST_WR) return -1;

	char *str = (char *)zmalloc(inlen + 1);
	sts_strncpy(str, inlen + 1, in_, inlen);
	int index = sts_pointer_list_insert(list_->strlist, index_, str);
	if (index < 0) zfree(str);
	return index;
}
int sts_string_list_delete(s_sts_string_list *list_, int index_)
{
	return sts_pointer_list_delete(list_->strlist, index_, 1);
}
int sts_string_list_find_and_delete(s_sts_string_list *list_, const char *finder_)
{
	return sts_pointer_list_find_and_delete(list_->strlist, (void *)finder_);
}
int sts_string_list_push(s_sts_string_list *list_, const char *in_, size_t inlen)
{
	if (list_->permissions != STRING_LIST_WR) return -1;

	char *str = (char *)zmalloc(inlen + 1);
	sts_strncpy(str, inlen + 1, in_, inlen);
	return sts_pointer_list_push(list_->strlist, str);
}

///////////////////////////////////////////////////////////////////////////
//------------------------listNode --------------------------------//
//  操作listNode列表的函数
///////////////////////////////////////////////////////////////////////////
listNode *sts_sdsnode_create(const void *in, size_t inlen)
{
	listNode *node = (listNode *)zmalloc(sizeof(listNode));
	node->value = NULL;
	node->prev = NULL;
	node->next = NULL;
	if (in == NULL || inlen < 1) { return node; }
	sds ptr = sdsnewlen(in, inlen);
	node->value = ptr;
	return node;
}
void sts_sdsnode_destroy(listNode *node) {
	listNode *next;
	while (node != NULL)
	{
		next = node->next;
		sdsfree((sds)(node->value));
		zfree(node);
		node = next;
	}
}

listNode *sts_sdsnode_offset_node(listNode *node_, int offset)
{
	if (node_ == NULL || offset == 0) { return NULL; }
	listNode *node = NULL;
	if (offset > 0){
		node = sts_sdsnode_first_node(node_);
		while (node->next != NULL)
		{
			node = node->next;
			offset--;
			if (offset == 0)  { break; }
		};
	}
	else {
		node = sts_sdsnode_last_node(node_);
		while (node->prev != NULL)
		{
			node = node->prev;
			offset++;
			if (offset == 0) { break; }
		};
	}
	return node;
}

listNode *sts_sdsnode_last_node(listNode *node_)
{
	if (node_ == NULL) { return NULL; }
	while (node_->next != NULL)
	{
		node_ = node_->next;
	};
	return node_;
}
listNode *sts_sdsnode_first_node(listNode *node_)
{
	if (node_ == NULL) { return NULL; }
	while (node_->prev != NULL)
	{
		node_ = node_->prev;
	};
	return node_;
}
listNode *sts_sdsnode_push_node(listNode *node_, const void *in, size_t inlen)
{
	if (node_==NULL){
		return sts_sdsnode_create(in, inlen);
	}
	listNode *last = sts_sdsnode_last_node(node_); //这里一定返回真
	listNode *node = (listNode *)zmalloc(sizeof(listNode));
	sds ptr = sdsnewlen(in, inlen);
	node->value = ptr;
	node->prev = last;
	node->next = NULL;
	last->next = node;
	return sts_sdsnode_first_node(node);
}
listNode *sts_sdsnode_update(listNode *node_, const void *in, size_t inlen)
{
	if (node_ == NULL) { return NULL; }
	if (node_->value==NULL)
	{
		node_->value = sdsnewlen(in, inlen);
		return node_;
	}
	else{
		sds ptr = sdscpylen((sds)node_->value, (const char *)in, inlen);
		node_->value = ptr;
	}
	return node_;
}
listNode *sts_sdsnode_clone(listNode *node_)
{
	listNode *newnode = NULL;
	listNode *node = node_;
	while (node != NULL)
	{
		newnode = sts_sdsnode_push_node(newnode, node->value, sdslen((sds)node->value));
		node = node->next;
	};
	return newnode;
}
int sts_sdsnode_get_size(listNode *node_)
{
	if (node_ == NULL) { return 0; }
	int k = 0;
	while (node_)
	{
		if (node_->value)
		{
			k += sdslen((sds)node_->value);
		}
		node_ = node_->next;
	}
	return k;
}
int sts_sdsnode_get_count(listNode *node_)
{
	if (node_ == NULL) { return 0; }
	int k = 0;
	while (node_)
	{
		if (node_->value)
		{
			k++;
		}
		node_ = node_->next;
	}
	return k;
}

#if 0
///////test
#include <stdio.h>
//#include <malloc.h>
int char_to_int(void *str1, void *str2)
{
	printf("str1=%s %p %c\n", (char *)str1, (char *)str1, *(char *)(str1 + 1));
	printf("str2=%s %p %c\n", (char *)str2, (char *)str2, *(char *)(str2 + 1));
	return;
	char buff[100];// = (char *)malloc(100);
	char **ptr = (char **)buff;
	printf("buff = %p, ptr=%p %p %p\n", buff, ptr, &ptr[0], &ptr[1]);
	//ptr[0]=(char *)str1;
	memmove((char *)buff, &str1, sizeof(char *));
	ptr[1] = (char *)str2;
	printf("ptr=%p %p address=%p %p\n", ptr[0], ptr[1], &ptr[0], &ptr[1]);

	printf("ptr1 = %s,%s \n", ptr[0], ptr[0]);
	printf("ptr2 = %s,%s \n", ptr[1], ptr[1]);
	//free(buff);
	return 1;
}
int main(void) {
	// your code goes here
	char *str[] = { "SH", "SZ" };
	char sss[100];
	sss[0] = '5'; sss[1] = '6'; sss[2] = 0;
	int ii = char_to_int(sss, str[0]);
	printf("str=%s %p %p\n", str[0], str[0], sss);
	return 0;
}
#endif
