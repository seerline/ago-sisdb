
#include "sis_list.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  s_sis_struct_list
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

s_sis_struct_list *sis_struct_list_create(int len_, void *in_, int inlen_)
{
	if (len_ < 1)
	{
		return NULL;
	}
	s_sis_struct_list *sbl = (s_sis_struct_list *)sis_malloc(sizeof(s_sis_struct_list));
	sbl->len = len_;
	sbl->maxcount = 0;
	sbl->count = 0;
	sbl->start = 0;
	sbl->buffer = NULL;
	if (in_ && inlen_ > 0)
	{
		sis_struct_list_set(sbl, in_, inlen_);
	}
	return sbl;
}
void sis_struct_list_destroy(s_sis_struct_list *list_)
{
	sis_struct_list_clear(list_);
	if (list_->buffer)
	{
		sis_free(list_->buffer);
	}
	list_->buffer = NULL;
	list_->maxcount = 0;
	sis_free(list_);
}
void sis_struct_list_clear(s_sis_struct_list *list_)
{
	list_->count = 0;
	list_->start = 0;
}
void _struct_list_grow(s_sis_struct_list *list_, int addlen_)
{
	int newlen = list_->start + list_->count + addlen_;
	if (newlen > list_->maxcount && list_->start > 0)
	{
		memmove((char *)list_->buffer,
				(char *)list_->buffer + (list_->start * list_->len),
				list_->count * list_->len);
		list_->start = 0;
		newlen = list_->count + addlen_;
	}
	if (newlen <= list_->maxcount)
	{
		return;
	}

	int maxlen = newlen;
	if (newlen < 16)
	{
		maxlen = 16;
	}
	else if (newlen >= 16 && newlen < 64)
	{
		maxlen = 64;
	}
	else if (newlen >= 64 && newlen < 256)
	{
		maxlen = 256;
	}
	else
	{
		maxlen = newlen + STRUCT_LIST_STEP_ROW;
	}
	list_->buffer = sis_realloc(list_->buffer, maxlen * list_->len);
	list_->maxcount = maxlen;
}

int sis_struct_list_push(s_sis_struct_list *list_, void *in_)
{
	_struct_list_grow(list_, 1);

	int offset = list_->count + list_->start;
	memmove((char *)list_->buffer + (offset * list_->len), in_, list_->len);

	list_->count++;
	return list_->count - 1;
}
int sis_struct_list_update(s_sis_struct_list *list_, int index_, void *in_)
{
	if (index_ >= 0 && index_ < list_->count)
	{
		int offset = index_ + list_->start;
		memmove((char *)list_->buffer + (offset * list_->len), in_, list_->len);
		return index_;
	}
	return -1;
}
int sis_struct_list_insert(s_sis_struct_list *list_, int index_, void *in_)
{
	if (list_->count < 1)
	{
		return sis_struct_list_push(list_, in_);
	}
	if (index_ < 0 || index_ > list_->count - 1)
	{
		return -1;
	}
	_struct_list_grow(list_, 1);
	int offset = index_ + list_->start;
	memmove((char *)list_->buffer + ((offset + 1) * list_->len),
			(char *)list_->buffer + (offset * list_->len),
			(list_->count - index_) * list_->len);

	memmove((char *)list_->buffer + (offset * list_->len), in_, list_->len);
	list_->count++;
	return index_;
}
void *sis_struct_list_first(s_sis_struct_list *list_)
{
	return sis_struct_list_get(list_, 0);
}
void *sis_struct_list_last(s_sis_struct_list *list_)
{
	return sis_struct_list_get(list_, list_->count - 1);
}
void *sis_struct_list_get(s_sis_struct_list *list_, int index_)
{
	char *o = NULL;
	if (index_ >= 0 && index_ < list_->count)
	{
		int offset = index_ + list_->start;
		o = (char *)list_->buffer + (offset * list_->len);
	}
	return o;
}
void *sis_struct_list_next(s_sis_struct_list *list_, void *current_)
{
	int offset = 1;
	char *o = (char *)current_ + offset * list_->len;
	if (o >= (char *)list_->buffer + list_->start * list_->len &&
		o <= (char *)list_->buffer + (list_->start + list_->count - 1) * list_->len)
	{
		return o;
	}
	else
	{
		return NULL;
	}
}
// 仅仅设置尺寸，不初始化
void struct_list_set_size(s_sis_struct_list *list_, int len_)
{
	if (len_ <= list_->maxcount)
	{
		return;
	}
	list_->buffer = sis_realloc(list_->buffer, len_ * list_->len);
	list_->maxcount = len_;
}

int sis_struct_list_set(s_sis_struct_list *list_, void *in_, int inlen_)
{

	int count = inlen_ / list_->len;
	struct_list_set_size(list_, count);
	if (in_)
	{
		memmove(list_->buffer, in_, inlen_);
	}
	else
	{
		memset(list_->buffer, 0, count * list_->len);
	}
	list_->count = count;
	list_->start = 0;
	return count;
}

void sis_struct_list_limit(s_sis_struct_list *list_, int limit_)
{
	if (limit_ < 1 || limit_ > list_->count)
	{
		return;
	}
	list_->start = list_->start + list_->count - limit_;
	// int offset = list_->count - limit_;
	// memmove(list_->buffer, (char *)list_->buffer + (offset * list_->len), limit_ * list_->len);
	list_->count = limit_;
}
int sis_struct_list_clone(s_sis_struct_list *src_, s_sis_struct_list *list_)
{
	sis_struct_list_set(list_, (char *)src_->buffer + (src_->start * src_->len), src_->count * list_->len);
	return src_->count;
}
int sis_struct_list_pack(s_sis_struct_list *list_)
{
	char *tmp = (char *)sis_malloc(list_->count * list_->len);
	memmove(tmp, (char *)list_->buffer + (list_->start * list_->len), list_->count * list_->len);
	sis_free(list_->buffer);
	list_->buffer = tmp;
	list_->start = 0;
	list_->maxcount = list_->count;
	return list_->count;
}
int sis_struct_list_delete(s_sis_struct_list *list_, int start_, int count_)
{
	if (start_ < 0 || count_ < 1 || start_ + count_ > list_->count)
	{
		return 0;
	}
	// 删除时
	if (list_->start > 0)
	{
		memmove((char *)list_->buffer, (char *)list_->buffer + (list_->start * list_->len), list_->count * list_->len);
		list_->start = 0;
	}
	memmove((char *)list_->buffer + (start_ * list_->len), (char *)list_->buffer + ((start_ + count_) * list_->len),
			(list_->count - count_ - start_) * list_->len);

	list_->count = list_->count - count_;
	return count_;
}

// int sis_struct_list_pto_recno(s_sis_struct_list *list_, void *p_)
// {
// 	if ((char *)p_ < (char *)list_->buffer + list_->start * list_->len ||
// 		(char *)p_ > ((char *)list_->buffer + (list_->start + list_->count - 1) * list_->len))
// 	{
// 		return -1;
// 	}
// 	return (int)(((char *)p_ - (char *)list_->buffer) / list_->len) - list_->start;
// }
///////////////////////////////////////////////////////////////////////////
//------------------------s_pointer_list --------------------------------//
//  存储指针的列表,依赖于struct_list,记录长度为sizeof(char *)
///////////////////////////////////////////////////////////////////////////
s_sis_pointer_list *sis_pointer_list_create()
{
	s_sis_pointer_list *sbl = (s_sis_pointer_list *)sis_malloc(sizeof(s_sis_pointer_list));
	sbl->len = sizeof(char *);
	sbl->maxcount = 0;
	sbl->count = 0;
	sbl->buffer = NULL;
	sbl->free = NULL;
	return sbl;
}
void sis_pointer_list_destroy(s_sis_pointer_list *list_)
{
	sis_pointer_list_clear(list_);
	if (list_->buffer)
	{
		sis_free(list_->buffer);
	}
	list_->buffer = NULL;
	list_->maxcount = 0;
	sis_free(list_);
}
void sis_pointer_list_clear(s_sis_pointer_list *list_)
{
	char **ptr = (char **)list_->buffer;
	for (int i = 0; i < list_->count; i++)
	{
		if (list_->free)
		{
			list_->free(ptr[i]);
			ptr[i] = NULL;
		}
	}
	list_->count = 0;
}

void _pointer_list_grow(s_sis_pointer_list *list_, int len_)
{
	if (len_ < list_->maxcount)
	{
		return;
	}
	int maxlen = len_;
	if (len_ < 256)
	{
		maxlen = 256;
	}
	else
	{
		maxlen = len_ + POINTER_LIST_STEP_ROW;
	}
	// void *buffer = sis_malloc(maxlen * list_->len);
	// memmove(buffer, list_->buffer, list_->maxcount*list_->len);
	// sis_free(list_->buffer);
	// list_->buffer = buffer;
	list_->buffer = sis_realloc(list_->buffer, maxlen * list_->len);
	list_->maxcount = maxlen;
}
int sis_pointer_list_push(s_sis_pointer_list *list_, void *in_)
{
	_pointer_list_grow(list_, list_->count + 1);
	char **ptr = (char **)list_->buffer;
	ptr[list_->count] = (char *)in_;
	list_->count++;
	return list_->count;
}
int sis_pointer_list_update(s_sis_pointer_list *list_, int index_, void *in_)
{
	if (index_ >= 0 && index_ < list_->count)
	{
		char **ptr = (char **)list_->buffer;
		if (list_->free)
		{
			list_->free(ptr[index_]);
		}
		ptr[index_] = (char *)in_;
		return index_;
	}
	return -1;
}
int sis_pointer_list_insert(s_sis_pointer_list *list_, int index_, void *in_)
{
	if (list_->count < 1)
	{
		return sis_pointer_list_push(list_, in_);
	}
	if (index_ < 0 || index_ > list_->count - 1)
	{
		return -1;
	}
	_pointer_list_grow(list_, list_->count + 1);
	memmove((char *)list_->buffer + ((index_ + 1) * list_->len), (char *)list_->buffer + (index_ * list_->len),
			(list_->count - index_) * list_->len);

	char **ptr = (char **)list_->buffer;
	ptr[index_] = (char *)in_;

	list_->count++;
	return index_;
}

void *sis_pointer_list_get(s_sis_pointer_list *list_, int index_)
{
	char *rtn = NULL;
	if (index_ >= 0 && index_ < list_->count)
	{
		char **ptr = (char **)list_->buffer;
		rtn = ptr[index_];
	}
	return rtn;
}
void *sis_pointer_list_first(s_sis_pointer_list *list_)
{
	return sis_pointer_list_get(list_, 0);
}

int sis_pointer_list_delete(s_sis_pointer_list *list_, int start_, int count_)
{
	if (start_ < 0 || count_ < 1 || start_ + count_ > list_->count)
	{
		return 0;
	}

	char **ptr = (char **)list_->buffer;
	for (int i = start_; i < start_ + count_; i++)
	{
		if (list_->free)
		{
			list_->free(ptr[i]);
		}
	}
	memmove((char *)list_->buffer + (start_ * list_->len), (char *)list_->buffer + ((start_ + count_) * list_->len),
			(list_->count - count_ - start_) * list_->len);

	list_->count = list_->count - count_;
	return count_;
}

int sis_pointer_list_indexof(s_sis_pointer_list *list_, void *in_)
{
	char **ptr = (char **)list_->buffer;
	for (int i = 0; i < list_->count; i++)
	{
		if (ptr[i] == in_)
		{
			return i;
		}
	}
	return -1;
}
int sis_pointer_list_find_and_update(s_sis_pointer_list *list_, void *finder_, void *in_)
{
	int index = sis_pointer_list_indexof(list_, finder_);
	return sis_pointer_list_update(list_, index, in_);
}
int sis_pointer_list_find_and_delete(s_sis_pointer_list *list_, void *finder_)
{
	char **ptr = (char **)list_->buffer;
	for (int i = 0; i < list_->count; i++)
	{
		if (ptr[i] == finder_)
		{
			if (list_->free)
			{
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
//------------------------s_sis_string_list --------------------------------//
//  存储不定长字符串的列表，
///////////////////////////////////////////////////////////////////////////
void sis_free_call(void *p)
{
	sis_free(p);
}
s_sis_string_list *sis_string_list_create_r() //只读
{
	s_sis_string_list *l = (s_sis_string_list *)sis_malloc(sizeof(s_sis_string_list));
	memset(l, 0, sizeof(s_sis_string_list));
	l->strlist = sis_pointer_list_create();
	l->permissions = STRING_LIST_RD;
	return l;
}

s_sis_string_list *sis_string_list_create_w() //读写
{
	s_sis_string_list *l = (s_sis_string_list *)sis_malloc(sizeof(s_sis_string_list));
	memset(l, 0, sizeof(s_sis_string_list));
	l->strlist = sis_pointer_list_create();
	l->permissions = STRING_LIST_WR;
	l->strlist->free = sis_free_call;
	return l;
}
void sis_string_list_destroy(void *list)
{
	s_sis_string_list *list_ = (s_sis_string_list *)list;
	sis_string_list_clear(list_);
	sis_pointer_list_destroy(list_->strlist);
	sis_free(list_);
}
void sis_string_list_clear(s_sis_string_list *list_)
{
	if (list_->permissions == STRING_LIST_RD && list_->m_ptr_r)
	{
		sis_free(list_->m_ptr_r);
		list_->m_ptr_r = NULL;
	}
	sis_pointer_list_clear(list_->strlist);
}
// read & write
int sis_string_list_load(s_sis_string_list *list_, const char *in_, size_t inlen_, const char *sign)
{
	sis_string_list_clear(list_);

	if (strlen(in_) == 0)
	{
		return 0;
	}
	char *src = (char *)sis_malloc(inlen_ + 1);
	sis_strncpy(src, inlen_ + 1, in_, inlen_);

	if (list_->permissions == STRING_LIST_RD)
	{
		list_->m_ptr_r = src;
	}
	char *ptr = src;
	char *des = NULL;
	char *token = NULL;
	size_t len;
	while ((token = sis_strsep(&ptr, sign)) != NULL)
	{
		sis_trim(token);
		if (list_->permissions == STRING_LIST_WR)
		{
			len = strlen(token);
			des = (char *)sis_malloc(len + 1);
			sis_strncpy(des, len + 1, token, len);
		}
		else
		{
			des = token;
		}
		sis_pointer_list_push(list_->strlist, des);
	}
	if (list_->permissions == STRING_LIST_WR)
	{
		sis_free(src);
	}
	return list_->strlist->count;
}

const char *sis_string_list_get(s_sis_string_list *list_, int index_)
{
	return (const char *)sis_pointer_list_get(list_->strlist, index_);
}
int sis_string_list_getsize(s_sis_string_list *list_)
{
	return list_->strlist->count;
}
s_sis_sds sis_string_list_sds(s_sis_string_list *list_)
{
	if (list_->strlist->count < 1)
	{
		return NULL;
	}
	s_sis_sds o = sis_sdsnew(sis_pointer_list_get(list_->strlist, 0));
	for (int i = 1; i < list_->strlist->count; i++)
	{
		o = sis_sdscatfmt(o, ",%s", sis_pointer_list_get(list_->strlist, i));
	}
	return o;
}
int sis_string_list_clone(
	s_sis_string_list *src_,
	s_sis_string_list *des_)
{
	if (!src_ || !des_)
	{
		return 0;
	}
	sis_string_list_clear(des_);
	for (int i = 0; i < src_->strlist->count; i++)
	{
		const char *str = (const char *)sis_pointer_list_get(src_->strlist, i);
		sis_string_list_push(des_, str, strlen(str));
	}
	return des_->strlist->count;
}
int sis_string_list_merge(
	s_sis_string_list *list_,
	s_sis_string_list *other_)
{
	if (!list_ || !other_)
	{
		return 0;
	}

	for (int i = 0; i < other_->strlist->count; i++)
	{
		const char *str = (const char *)sis_pointer_list_get(other_->strlist, i);
		sis_string_list_push_only(list_, str, strlen(str));
	}
	return list_->strlist->count;
}

int sis_string_list_across(
	s_sis_string_list *list_,
	s_sis_string_list *other_)
{
	if (!list_ || !other_)
	{
		return 0;
	}
	for (int i = 0; i < list_->strlist->count;)
	{
		const char *str = (const char *)sis_pointer_list_get(list_->strlist, i);
		int index = sis_string_list_indexofcase(other_, str);
		if (index < 0)
		{
			sis_string_list_delete(list_, i);
		}
		else
		{
			i++;
		}
	}
	return list_->strlist->count;
}

int sis_string_list_indexof(s_sis_string_list *list_, const char *in_)
{
	if (!in_)
	{
		return -1;
	}
	for (int i = 0; i < list_->strlist->count; i++)
	{
		if (!strcmp(sis_string_list_get(list_, i), in_))
		{
			return i;
		}
	}
	return -1;
}
int sis_string_list_indexofcase(s_sis_string_list *list_, const char *in_)
{
	if (!in_)
	{
		return -1;
	}
	for (int i = 0; i < list_->strlist->count; i++)
	{
		if (!sis_strcasecmp(sis_string_list_get(list_, i), in_))
		{
			return i;
		}
	}
	return -1;
}
int sis_string_list_update(s_sis_string_list *list_, int index_, const char *in_, size_t inlen)
{
	if (list_->permissions != STRING_LIST_WR)
	{
		return -1;
	}

	char *str = (char *)sis_malloc(inlen + 1);
	sis_strncpy(str, inlen + 1, in_, inlen);
	int index = sis_pointer_list_update(list_->strlist, index_, str);
	if (index < 0)
	{
		sis_free(str);
	}
	return index;
}
int sis_string_list_find_and_update(s_sis_string_list *list_, char *finder_, const char *in_, size_t inlen)
{
	if (list_->permissions != STRING_LIST_WR)
	{
		return -1;
	}

	char *str = (char *)sis_malloc(inlen + 1);
	sis_strncpy(str, inlen + 1, in_, inlen);
	int index = sis_pointer_list_find_and_update(list_->strlist, finder_, str);
	if (index < 0)
	{
		sis_free(str);
	}
	return index;
}
int sis_string_list_insert(s_sis_string_list *list_, int index_, const char *in_, size_t inlen)
{
	if (list_->permissions != STRING_LIST_WR)
	{
		return -1;
	}

	char *str = (char *)sis_malloc(inlen + 1);
	sis_strncpy(str, inlen + 1, in_, inlen);
	int index = sis_pointer_list_insert(list_->strlist, index_, str);
	if (index < 0)
	{
		sis_free(str);
	}
	return index;
}
int sis_string_list_delete(s_sis_string_list *list_, int index_)
{
	return sis_pointer_list_delete(list_->strlist, index_, 1);
}
int sis_string_list_find_and_delete(s_sis_string_list *list_, const char *finder_)
{
	return sis_pointer_list_find_and_delete(list_->strlist, (void *)finder_);
}
int sis_string_list_push(s_sis_string_list *list_, const char *in_, size_t inlen)
{
	if (list_->permissions != STRING_LIST_WR)
	{
		return -1;
	}

	char *str = (char *)sis_malloc(inlen + 1);
	sis_strncpy(str, inlen + 1, in_, inlen);
	return sis_pointer_list_push(list_->strlist, str);
}
int sis_string_list_push_only(s_sis_string_list *list_, const char *in_, size_t inlen)
{
	if (list_->permissions != STRING_LIST_WR)
	{
		return -1;
	}

	char *str = (char *)sis_malloc(inlen + 1);
	sis_strncpy(str, inlen + 1, in_, inlen);
	int index = sis_string_list_indexofcase(list_, str);
	if (index < 0)
	{
		return sis_pointer_list_push(list_->strlist, str);
	}
	else
	{
		sis_free(str);
	}
	return index;
}
void sis_string_list_limit(s_sis_string_list *list_, int limit_)
{
	if (limit_ < 1 || (!list_->strlist) || limit_ > list_->strlist->count)
	{
		return;
	}
	int offset = list_->strlist->count - limit_;
	sis_pointer_list_delete(list_->strlist, 0, offset);
}

#if 0
///////test
#include <stdio.h>
//#include <malloc.h>
int char_to_int(void *str1, void *str2)
{
	printf("str1=%s %p %c\n", (char *)str1, (char *)str1, *(char *)(str1 + 1));
	printf("str2=%s %p %c\n", (char *)str2, (char *)str2, *(char *)(str2 + 1));
	return 0;
	char buff[100];// = (char *)malloc(100);
	char **ptr = (char **)buff;
	printf("buff = %p, ptr=%p %p %p\n", buff, ptr, &ptr[0], &ptr[1]);
	//ptr[0]=(char *)str1;
	memmove((char *)buff, &str1, sizeof(void *));
	ptr[1] = (char *)str2;
	printf("ptr=%p %p address=%p %p\n", ptr[0], ptr[1], &ptr[0], &ptr[1]);

	printf("ptr1 = %s,%s \n", ptr[0], ptr[0]);
	printf("ptr2 = %s,%s \n", ptr[1], ptr[1]);
	//free(buff);
	return 1;
}
int main1(void) {
	// your code goes here
	char *str[] = { "SH", "SZ" };
	char sss[100];
	sss[0] = '5'; sss[1] = '6'; sss[2] = 0;
	int ii = char_to_int(sss, str[0]);
	printf("str=%s %p %p  ii=%d\n", str[0], str[0], sss, ii);
	return 0;
}
int main(void) {
	s_sis_string_list *list = sis_string_list_create_w();
	sis_string_list_insert(list, 0, "1", 1);
	sis_string_list_push(list,  "2", 1);
	sis_string_list_push(list,  "3", 1);
	sis_string_list_push(list,  "4", 1);
	sis_string_list_insert(list, 1, "5", 1);
	for (int i=0;i<sis_string_list_getsize(list);i++) {
		printf("%s\n",sis_string_list_get(list, i));
	}

	sis_string_list_destroy(list);

	const char *src = "1,2,3,4,5,6,7,8,9,0";
	s_sis_string_list *list = sis_string_list_create_w();
	sis_string_list_load(list, src, strlen(src),",");

	class->in = list;
	class->out = sis_string_list_create_w();
	//////////////
	// const char *des = "1,3,5,7,9,11";
	// s_sis_string_list *deslist = sis_string_list_create_w();
	// sis_string_list_load(deslist, des, strlen(des),",");

	// sis_string_list_clone(class->in, class->out);
	// {
	// 	s_sis_sds sss = sis_string_list_sds(class->out);
	// 	printf("::: %s\n  ", sss);
	// 	sis_sdsfree(sss);		
	// }	
	// sis_string_list_merge(class->out, class->in, deslist);
	// {
	// 	s_sis_sds sss = sis_string_list_sds(class->out);
	// 	printf("::: %s\n  ", sss);
	// 	sis_sdsfree(sss);		
	// }	
	// sis_string_list_across(class->out, class->in, deslist);
	// {
	// 	s_sis_sds sss = sis_string_list_sds(class->out);
	// 	printf("::: %s\n  ", sss);
	// 	sis_sdsfree(sss);		
	// }	

	return 0;
}
#endif
