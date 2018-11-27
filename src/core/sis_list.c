
#include "sis_list.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  s_sis_struct_list
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

s_sis_struct_list *sis_struct_list_create(int len_, void *in_, int inlen_)
{
	// printf("new list...%d\n", len_);
	if (len_ < 1)
	{
		return NULL;
	}
	s_sis_struct_list *sbl = (s_sis_struct_list *)sis_malloc(sizeof(s_sis_struct_list));
	sbl->len = len_;
	sbl->maxcount = 0;
	sbl->count = 0;
	sbl->buffer = NULL;
	sbl->free = NULL;
	sbl->mode = STRUCT_LIST_NORMAL;
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
	if (list_->mode == STRUCT_LIST_POINTER)
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
	}
	list_->count = 0;
}
void struct_list_grow(s_sis_struct_list *list_, int len_)
{
	if (len_ < list_->maxcount)
	{
		return;
	}
	int maxlen = len_;
	if (len_ < 8)
	{
		maxlen = 8;
	}
	else if (len_ >= 8 && len_ < 64)
	{
		maxlen = 64;
	}
	else if (len_ >= 64 && len_ < 256)
	{
		maxlen = 256;
	}
	else
	{
		maxlen = len_ + BUFFLIST_STEP_ROW;
	}

	// void *buffer = sis_malloc(maxlen * list_->len);
	// memmove(buffer, list_->buffer, list_->maxcount*list_->len);
	// sis_free(list_->buffer);
	// list_->buffer = buffer;
	list_->buffer = sis_realloc(list_->buffer, maxlen * list_->len);
	list_->maxcount = maxlen;
}
void struct_list_setsize(s_sis_struct_list *list_, int len_)
{
	if (len_ < list_->maxcount)
	{
		return;
	}
	// void *buffer = sis_malloc(len_ * list_->len);
	// memmove(buffer, list_->buffer, list_->maxcount*list_->len);
	// sis_free(list_->buffer);
	// list_->buffer = buffer;
	list_->buffer = sis_realloc(list_->buffer, len_ * list_->len);
	list_->maxcount = len_;
}

int sis_struct_list_push(s_sis_struct_list *list_, void *in_)
{
	struct_list_grow(list_, list_->count + 1);
	if (list_->mode == STRUCT_LIST_POINTER)
	{
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
int sis_struct_list_update(s_sis_struct_list *list_, int index_, void *in_)
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
		else
		{
			// if (index_==0)
			// printf("---2---value =%p %p len=%d\n", list_->buffer, list_, list_->len);
			memmove((char *)list_->buffer + (index_ * list_->len), in_, list_->len);
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
	struct_list_grow(list_, list_->count + 1);
	memmove((char *)list_->buffer + ((index_ + 1) * list_->len), (char *)list_->buffer + (index_ * list_->len),
			(list_->count - index_) * list_->len);

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
	char *rtn = NULL;
	if (index_ >= 0 && index_ < list_->count)
	{
		if (list_->mode == STRUCT_LIST_POINTER)
		{
			char **ptr = (char **)list_->buffer;
			rtn = ptr[index_];
		}
		else
		{
			rtn = (char *)list_->buffer + (index_ * list_->len);
		}
	}
	return rtn;
}
void *sis_struct_list_next(s_sis_struct_list *list_, void *current_, int offset)
{
	if (list_->mode == STRUCT_LIST_POINTER)
	{
		return current_;
	}
	char *rtn = (char *)current_ + offset * list_->len;
	if (rtn >= (char *)list_->buffer && rtn <= (char *)list_->buffer + (list_->count - 1) * list_->len)
	{
		return rtn;
	}
	else
	{
		return NULL;
	}
}
int sis_struct_list_setlen(s_sis_struct_list *list_, int count_)
{
	if (list_->mode == STRUCT_LIST_POINTER)
	{
		return 0;
	}
	struct_list_setsize(list_, count_);
	memset(list_->buffer, 0, count_ * list_->len);
	list_->count = count_;
	return count_;	
}

int sis_struct_list_set(s_sis_struct_list *list_, void *in_, int inlen_)
{
	if (list_->mode == STRUCT_LIST_POINTER)
	{
		return 0;
	}
	int count = inlen_ / list_->len;
	struct_list_setsize(list_, count);
	if (in_)
	{
		memmove(list_->buffer, in_, inlen_);
	}
	else
	{
		memset(list_->buffer, 0, count * list_->len);
	}
	list_->count = count;
	return count;
}

void sis_struct_list_limit(s_sis_struct_list *list_, int limit_)
{
	if (list_->mode == STRUCT_LIST_POINTER)
	{
		return;
	}
	if (limit_ < 1 || limit_ > list_->count)
	{
		return;
	}
	int offset = list_->count - limit_;
	memmove(list_->buffer, (char *)list_->buffer + (offset * list_->len), limit_ * list_->len);
	list_->count = limit_;
}
int sis_struct_list_clone(s_sis_struct_list *src_, s_sis_struct_list *list_, int limit_)
{
	if (list_->mode == STRUCT_LIST_POINTER)
	{
		return 0;
	}
	int count;
	int offset;
	if (limit_ < 1 || limit_ > src_->count)
	{
		count = src_->count;
	}
	else
	{
		count = limit_;
	}
	offset = src_->count - count;
	sis_struct_list_set(list_, (char *)src_->buffer + (offset * src_->len), count * list_->len);
	return count;
}
int sis_struct_list_pack(s_sis_struct_list *list_)
{
	if (list_->mode == STRUCT_LIST_POINTER)
	{
		return 0;
	}
	if (!list_)
	{
		return 0;
	}
	char *tmp = (char *)sis_malloc(list_->count * list_->len);
	memmove(tmp, list_->buffer, list_->count * list_->len);
	sis_free(list_->buffer);
	list_->buffer = tmp;
	list_->maxcount = list_->count;
	return list_->count;
}
int sis_struct_list_delete(s_sis_struct_list *list_, int start_, int count_)
{
	if (start_ < 0 || count_ < 1 || start_ + count_ > list_->count)
	{
		return 0;
	}

	if (list_->mode == STRUCT_LIST_POINTER)
	{
		char **ptr = (char **)list_->buffer;
		for (int i = start_; i < start_ + count_; i++)
		{
			if (list_->free)
			{
				list_->free(ptr[i]);
			}
		}
	}
	memmove((char *)list_->buffer + (start_ * list_->len), (char *)list_->buffer + ((start_ + count_) * list_->len),
			(list_->count - count_ - start_) * list_->len);

	list_->count = list_->count - count_;
	return count_;
}

int sis_struct_list_pto_recno(s_sis_struct_list *list_, void *p_)
{
	if (list_->mode == STRUCT_LIST_POINTER)
	{
		return 0;
	}
	if ((char *)p_ < (char *)list_->buffer ||
		(char *)p_ > ((char *)list_->buffer + list_->len * (list_->count - 1)))
	{
		return -1;
	}
	return (int)(((char *)p_ - (char *)list_->buffer) / list_->len);
}
///////////////////////////////////////////////////////////////////////////
//------------------------s_pointer_list --------------------------------//
//  存储指针的列表,依赖于struct_list,记录长度为sizeof(char *)
///////////////////////////////////////////////////////////////////////////
s_sis_struct_list *sis_pointer_list_create()
{
	s_sis_struct_list *sbl = (s_sis_struct_list *)sis_malloc(sizeof(s_sis_struct_list));
	sbl->len = sizeof(char *);
	sbl->maxcount = 0;
	sbl->count = 0;
	sbl->buffer = NULL;
	sbl->free = NULL;
	sbl->mode = STRUCT_LIST_POINTER;
	return sbl;
}
int sis_pointer_list_indexof(s_sis_struct_list *list_, void *in_)
{
	if (list_->mode != STRUCT_LIST_POINTER)
		return -1;
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
int sis_pointer_list_find_and_update(s_sis_struct_list *list_, void *finder_, void *in_)
{
	if (list_->mode != STRUCT_LIST_POINTER)
		return -1;
	int index = sis_pointer_list_indexof(list_, finder_);
	return sis_struct_list_update(list_, index, in_);
}
int sis_pointer_list_find_and_delete(s_sis_struct_list *list_, void *finder_)
{
	if (list_->mode != STRUCT_LIST_POINTER)
		return -1;
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
	memset(l,0,sizeof(s_sis_string_list));
	l->strlist = sis_pointer_list_create();
	l->permissions = STRING_LIST_RD;
	return l;
}

s_sis_string_list *sis_string_list_create_w() //读写
{
	s_sis_string_list *l = (s_sis_string_list *)sis_malloc(sizeof(s_sis_string_list));
	memset(l,0,sizeof(s_sis_string_list));
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

int sis_string_list_indexof(s_sis_string_list *list_, const char *in_)
{
	if (!in_)
		return -1;
	for (int i = 0; i < list_->strlist->count; i++)
	{
		if (!strcmp(sis_string_list_get(list_, i), in_))
			return i;
	}
	return -1;
}
int sis_string_list_indexofcase(s_sis_string_list *list_, const char *in_)
{
	if (!in_)
		return -1;
	for (int i = 0; i < list_->strlist->count; i++)
	{
		if (!strcasecmp(sis_string_list_get(list_, i), in_))
			return i;
	}
	return -1;
}
int sis_string_list_update(s_sis_string_list *list_, int index_, const char *in_, size_t inlen)
{
	if (list_->permissions != STRING_LIST_WR)
		return -1;

	char *str = (char *)sis_malloc(inlen + 1);
	sis_strncpy(str, inlen + 1, in_, inlen);
	int index = sis_pointer_list_update(list_->strlist, index_, str);
	if (index < 0)
		sis_free(str);
	return index;
}
int sis_string_list_find_and_update(s_sis_string_list *list_, char *finder_, const char *in_, size_t inlen)
{
	if (list_->permissions != STRING_LIST_WR)
		return -1;

	char *str = (char *)sis_malloc(inlen + 1);
	sis_strncpy(str, inlen + 1, in_, inlen);
	int index = sis_pointer_list_find_and_update(list_->strlist, finder_, str);
	if (index < 0)
		sis_free(str);
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
		return -1;

	char *str = (char *)sis_malloc(inlen + 1);
	sis_strncpy(str, inlen + 1, in_, inlen);
	return sis_pointer_list_push(list_->strlist, str);
}
int sis_string_list_push_only(s_sis_string_list *list_, const char *in_, size_t inlen)
{
	if (list_->permissions != STRING_LIST_WR)
		return -1;

	char *str = (char *)sis_malloc(inlen + 1);
	sis_strncpy(str, inlen + 1, in_, inlen);
	int index = sis_string_list_indexofcase(list_, str);
	if (index < 0)
	{
		return sis_pointer_list_push(list_->strlist, str);
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
	return 0;
}
#endif
