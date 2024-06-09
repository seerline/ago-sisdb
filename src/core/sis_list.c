
#include "sis_list.h"
#include "sis_math.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  s_sis_struct_list
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

s_sis_struct_list *sis_struct_list_create(int len_)
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
	return sbl;
}
void sis_struct_list_destroy(void *list_)
{
	s_sis_struct_list *list = (s_sis_struct_list *)list_;
	sis_struct_list_clear(list);
	if (list->buffer)
	{
		sis_free(list->buffer);
	}
	list->buffer = NULL;
	list->maxcount = 0;
	sis_free(list);
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
	if (newlen == 1)
	{
		maxlen = 1;
	}
	else if (newlen < 16)
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
	else if (newlen >= 256 && newlen < 1024)
	{
		maxlen = 1024;
	}
	else
	{
		// maxlen = newlen + STRUCT_LIST_STEP_ROW;
		maxlen = newlen * 2;
	}
	// printf("new list: %d\n", maxlen);
	void *newbuffer = sis_malloc(maxlen * list_->len);
	// void *newbuffer = sis_realloc(newbuffer, maxlen * list_->len);
	if (list_->buffer)
	{
		if (list_->count > 0)
		{
			memmove(newbuffer, (char *)list_->buffer + (list_->start * list_->len), list_->count * list_->len);
		}
		list_->start = 0;
		sis_free(list_->buffer);
	}
	list_->buffer = newbuffer;
	list_->maxcount = maxlen;
}
int sis_struct_list_pushs(s_sis_struct_list *list_, void *in_, int count_)
{
	_struct_list_grow(list_, count_);

	int offset = list_->count + list_->start;
	memmove((char *)list_->buffer + (offset * list_->len), in_, count_ * list_->len);

	list_->count += count_;
	return list_->count - 1;
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
	if (list_->count < 1 || index_ > list_->count - 1)
	{
		return sis_struct_list_push(list_, in_);
	}
	if (index_ < 0)
	{
		index_ = 0;
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
int sis_struct_list_inserts(s_sis_struct_list *list_, int index_, void *in_, int count_)
{
	if (list_->count < 1 || index_ > list_->count - 1)
	{
		return sis_struct_list_pushs(list_, in_, count_);
	}
	if (index_ < 0)
	{
		index_ = 0;
	}
	_struct_list_grow(list_, count_);
	int offset = index_ + list_->start;
	memmove((char *)list_->buffer + ((offset + count_) * list_->len),
			(char *)list_->buffer + (offset * list_->len),
			(list_->count - index_) * list_->len);

	memmove((char *)list_->buffer + (offset * list_->len), in_, count_ * list_->len);
	list_->count += count_;
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
void *sis_struct_list_empty(s_sis_struct_list *list_)
{
	_struct_list_grow(list_, 1);
	int offset = list_->count + list_->start;
	memset((char *)list_->buffer + (offset * list_->len), 0, list_->len);

	list_->count++;
	return (char *)list_->buffer + (offset * list_->len);
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
void *sis_struct_list_offset(s_sis_struct_list *list_, void *current_, int offset_)
{
	char *o = (char *)current_ + offset_ * list_->len;
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
void sis_struct_list_set_size(s_sis_struct_list *list_, int len_)
{
	if (list_->buffer)
	{
		sis_free(list_->buffer);
	}
	list_->buffer = sis_malloc(len_ * list_->len);
	list_->count = 0;
	list_->start = 0;
	list_->maxcount = len_;
}
void sis_struct_list_set_maxsize(s_sis_struct_list *list_, int maxlen_)
{
	if (maxlen_ < list_->maxcount)
	{
		return ;
	}
	sis_struct_list_set_size(list_, maxlen_);
}

int sis_struct_list_set(s_sis_struct_list *list_, void *in_, int inlen_)
{
	int count = inlen_ / list_->len;
	if (count < 1 || !in_)
	{
		return 0;
	}
	sis_struct_list_set_size(list_, count);
	memmove(list_->buffer, in_, list_->len * count);
	list_->count = count;
	list_->start = 0;
	return count;
}

int sis_struct_list_setone(s_sis_struct_list *list_, int index_, void *in_)
{
	if (index_ < 0)
	{
		return sis_struct_list_push(list_, in_);
	}
	if (index_ >= 0 && index_ < list_->count)
	{
		int offset = index_ + list_->start;
		memmove((char *)list_->buffer + (offset * list_->len), in_, list_->len);
		return index_;
	}
	else
	{
		_struct_list_grow(list_, index_ - list_->count + 1);
		int offset = index_ + list_->start;
		memmove((char *)list_->buffer + (offset * list_->len), in_, list_->len);

		list_->count = index_ + 1;
	}
	return index_;
	
}
void sis_struct_list_rect(s_sis_struct_list *list_, int rows_)
{
	if (rows_ < 1 || rows_ > list_->count)
	{
		return;
	}
	list_->start = list_->start + list_->count - rows_;
	// if (list_->count == rows_ && list_->start > rows_)
	// {
	// 	memmove(list_->buffer, (char *)list_->buffer + (list_->start * list_->len), list_->count * list_->len);		
	// 	list_->start = 0;
	// }
	if (list_->start > rows_)
	{
		memmove(list_->buffer, (char *)list_->buffer + (list_->start * list_->len), rows_ * list_->len);		
		list_->start = 0;
	}
	list_->count = rows_;
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
int sis_struct_list_append(s_sis_struct_list *src_, s_sis_struct_list *dst_)
{
	for (int i = 0; i < src_->count; i++)
	{
		sis_struct_list_push(dst_, sis_struct_list_get(src_,i));
	}
	return dst_->count;
}

int sis_struct_list_pack(s_sis_struct_list *list_)
{
	if (list_->count > 0)
	{
		char *tmp = (char *)sis_malloc(list_->count * list_->len);
		memmove(tmp, (char *)list_->buffer + (list_->start * list_->len), list_->count * list_->len);
		sis_free(list_->buffer);
		list_->buffer = tmp;
		list_->start = 0;
		list_->maxcount = list_->count;
	}
	else
	{
		sis_free(list_->buffer);
		list_->buffer = NULL;
		list_->start = 0;
		list_->count = 0;
		list_->maxcount = 0;
	}
	return list_->count;
}

// 弹出一条记录 只改变start不拷贝内存
void *sis_struct_list_pop(s_sis_struct_list *list_)
{
	char *o = NULL;
	if (list_->count > 0)
	{
		o = (char *)list_->buffer + (list_->start * list_->len);
		list_->start++;
		list_->count--;
	}
	return o;
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
//------------------------s_sis_node ---------------------------------//
//////////////////////////////////////////////////////////////////////////
// 第一个节点不可被删除
s_sis_node *sis_node_create()
{
	s_sis_node *o = SIS_MALLOC(s_sis_node, o);
	o->index = -1;
	return o;
} 
void sis_node_destroy(void *node_)
{
	s_sis_node *node = (s_sis_node *)node_;
	while (node->prev)
	{
		node = node->prev;
	}
	s_sis_node *next = node->next;
	while (next)
	{
		s_sis_node *prev = next;
		next = next->next;
		sis_free(prev);	
	}
	sis_free(node);	
}
void sis_node_clear(void *node_)
{
	s_sis_node *node = (s_sis_node *)node_;
	while (node->prev)
	{
		node = node->prev;
	}
	// while(node)
	// {
	// 	s_sis_node *next = node->next;
	// 	sis_free(node);	
	// 	node = next;
	// } 
	s_sis_node *next = node->next;
	while (next)
	{
		s_sis_node *prev = next;
		next = next->next;
		sis_free(prev);	
	}	
}

int sis_node_push(s_sis_node *node_, void *data_)
{
	while (node_->next)
	{
		node_ = node_->next;
	}
	s_sis_node *newnode = sis_node_create();
	newnode->value = data_;
	newnode->prev = node_;
	newnode->index = node_->index + 1;
	node_->next = newnode;
	return newnode->index;
}

s_sis_node *sis_node_get(s_sis_node *node_, int index_)
{
	if (!node_ || index_ < 0)
	{
		return NULL;
	}
	if (node_->index < index_)
	{
		return sis_node_get(node_->next, index_);
	}
	if (node_->index > index_)
	{
		return sis_node_get(node_->prev, index_);
	}
	return node_;
}
s_sis_node *sis_node_next(s_sis_node *node_)
{
	if (!node_)
	{
		return NULL;
	}
	return node_->next;
}

void *sis_node_set(s_sis_node *node_, int index_, void *data_)
{
	s_sis_node *node = sis_node_get(node_, index_);
	if (node)
	{
		void *ago = node->value;
		node->value = data_;
		return ago;
	}
	return NULL;
}

void *sis_node_del(s_sis_node *node_, int index_)
{
	s_sis_node *node = sis_node_get(node_, index_);
	if (node)
	{	
		s_sis_node *next = node->next;
		s_sis_node *prev = node->prev;
		if (prev)
		{
			prev->next = next;
		}
		if (next)
		{
			next->prev = prev;
		}
		while (next)
		{
			next->index--;
			next = next->next;
		}
		void *ago = node->value;
		sis_free(node);
		return ago;
	}
	return NULL;
}

int sis_node_get_size(s_sis_node *node_)
{
	while (node_->next)
	{
		node_ = node_->next;
	}	
	return node_->index + 1;
}
///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_node_list ---------------------------------//
//////////////////////////////////////////////////////////////////////////

s_sis_node_list *sis_node_list_create(int count_, int len_)
{
	s_sis_node_list *o = SIS_MALLOC(s_sis_node_list, o);
	o->nodes = sis_pointer_list_create();
	o->nodes->vfree = sis_struct_list_destroy;
	o->node_size = len_;
	o->node_count = count_;
	o->count = 0;
	o->nouse = 0;

	s_sis_struct_list *node = sis_struct_list_create(o->node_size);
	sis_struct_list_set_size(node, o->node_count);
	sis_pointer_list_push(o->nodes, node);
	return o;
} 
void sis_node_list_destroy(void *list_)
{
	s_sis_node_list *list = (s_sis_node_list *)list_;
	sis_pointer_list_destroy(list->nodes);
	sis_free(list);
}
void sis_node_list_clear(s_sis_node_list *list_)
{
	int count = list_->nodes->count - 1;
	if (count > 0)
	{
		sis_pointer_list_delete(list_->nodes, 1, count);
	}
	sis_struct_list_clear(sis_pointer_list_first(list_->nodes));
	list_->count = 0;
	list_->nouse = 0;
}

int   sis_node_list_push(s_sis_node_list *list_, void *in_)
{
	s_sis_struct_list *last = (s_sis_struct_list *)sis_pointer_list_get(list_->nodes, list_->nodes->count - 1);
	if (!last || (last->start + last->count) >= list_->node_count)
	{
		s_sis_struct_list *node = sis_struct_list_create(list_->node_size);
		sis_struct_list_set_size(node, list_->node_count);
		sis_pointer_list_push(list_->nodes, node);
		sis_struct_list_push(node, in_);
	}
	else
	{
		sis_struct_list_push(last, in_);
	}	
	list_->count++;
	return list_->count - 1;
}


void *sis_node_list_get(s_sis_node_list *list_, int index_)
{
	if (index_ < 0 || index_ > list_->count - 1)
	{
		return NULL;
	}
	int offset = index_ + list_->nouse;
	int nodeidx = offset / list_->node_count;
	s_sis_struct_list *node = (s_sis_struct_list *)sis_pointer_list_get(list_->nodes, nodeidx);
	// if (node->count == 0)
	// {
	// 	sis_pointer_list_delete(list_->nodes, nodeidx, 1);
	// 	list_->nouse -= list_->node_count;
	// 	return sis_node_list_get(list_, index_);
	// }
	void *o = NULL;
	if (node)
	{
		o = sis_struct_list_get(node, index_ % (list_->node_count - list_->nouse % list_->node_count));
	}
	// if (!o)
	// {
	// 	printf(":===1: %d %d | %d %d %d %d %d\n", nodeidx, offset, list_->nodes->count, 
	// 		list_->node_count, list_->count, list_->nouse, node ? node->count : -1);
	// }
	return o;
}
void *sis_node_list_empty(s_sis_node_list *list_)
{
	void *v = NULL;
	s_sis_struct_list *last = (s_sis_struct_list *)sis_pointer_list_get(list_->nodes, list_->nodes->count - 1);
	if (!last || (last->start + last->count) >= list_->node_count)
	{
		s_sis_struct_list *node = sis_struct_list_create(list_->node_size);
		sis_struct_list_set_size(node, list_->node_count);
		// LOG(1)("++ nouse:%d %d\n", list_->nouse, list_->node_count);
		sis_pointer_list_push(list_->nodes, node);
		v = sis_struct_list_empty(node);
	}
	else
	{
		v = sis_struct_list_empty(last);
	}	
	list_->count++;
	return v;
}

void *sis_node_list_pop(s_sis_node_list *list_)
{
	s_sis_struct_list *node = NULL;
	int nodeidx = 0;
	while (nodeidx < list_->nodes->count)
	{
		node = (s_sis_struct_list *)sis_pointer_list_get(list_->nodes, nodeidx);
		if (node->count == 0)
		{
			sis_pointer_list_delete(list_->nodes, nodeidx, 1);
			list_->nouse -= list_->node_count;
			// LOG(1)("-- nouse:%d %d\n", list_->nouse, list_->node_count);
		}
		else
		{
			break;
		}
	}
	if (!node)
	{
		// printf(":1: %d %d\n", nodeidx, list_->nodes->count);
		return NULL;
	}
	void *o = sis_struct_list_pop(node);
	if (o)
	{ // 有实际数据弹出
		list_->count--;
		list_->nouse++;
		// LOG(1)("nouse:%d\n", list_->nouse);
	}
	else
	{
		// printf(":2: %d %d\n", nodeidx, list_->nodes->count);
		return NULL;
	}
	return o;
}

int   sis_node_list_getsize(s_sis_node_list *list_)
{
	return list_->count;
}

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_sort_list --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sis_sort_list *sis_sort_list_create(int len_)
{
	s_sis_sort_list *o = SIS_MALLOC(s_sis_sort_list, o);
	o->isascend = 0;
	o->key = sis_struct_list_create(sizeof(int64));
	o->value = sis_struct_list_create(len_);
	return o;
}
void sis_sort_list_destroy(void *list_)
{
	s_sis_sort_list *list = (s_sis_sort_list *)list_;
	sis_struct_list_destroy(list->key);
	sis_struct_list_destroy(list->value);
	sis_free(list);
}
void sis_sort_list_clear(s_sis_sort_list *list_)
{
	sis_struct_list_clear(list_->key);
	sis_struct_list_clear(list_->value);
}
void sis_sort_list_clone(s_sis_sort_list *src_,s_sis_sort_list *des_)
{
	sis_sort_list_clear(des_);
	sis_struct_list_clone(src_->key, des_->key);
	sis_struct_list_clone(src_->value, des_->value);
}

void *sis_sort_list_set(s_sis_sort_list *list_, int64 key_, void *in_)
{
	void *unit = sis_sort_list_find(list_, key_);
	if (unit)
	{
		memmove(unit, in_, list_->value->len);
		return unit;
	}
	for (int i = 0; i < list_->key->count; i++)
	{
		int64 *key = (int64 *)sis_struct_list_get(list_->key, i);
		if ((list_->isascend && *key > key_) || (!list_->isascend && *key < key_))
		{
			sis_struct_list_insert(list_->key, i, &key_);
			sis_struct_list_insert(list_->value, i, in_);
			return sis_struct_list_get(list_->value, i);
		}
	}
	sis_struct_list_push(list_->key, &key_);
	sis_struct_list_push(list_->value, in_);
	return sis_struct_list_get(list_->value, list_->key->count - 1);
}
void *sis_sort_list_first(s_sis_sort_list *list_)
{
	return sis_struct_list_first(list_->value);
}
void *sis_sort_list_last(s_sis_sort_list *list_)
{
	return sis_struct_list_last(list_->value);
}
void *sis_sort_list_next(s_sis_sort_list *list_, void *value_)
{
	return sis_struct_list_offset(list_->value, value_, 1);
}
void *sis_sort_list_prev(s_sis_sort_list *list_, void *value_)
{
	return sis_struct_list_offset(list_->value, value_, -1);
}
// 二分法查找 从高向低
int _sort_list_find(s_sis_sort_list *list, int64 key, int start, int stop)
{	
	while(start <= stop)
	{
		int mid = (stop - start) / 2 + start;
		int64 *midv = (int64 *)sis_struct_list_get(list->key, mid);
		if (list->isascend)
		{
			if (*midv > key)
			{
				stop = mid - 1;
			}
			else if (*midv < key)
			{
				start = mid + 1;
			}
			else
			{
				return mid;
			}
		}
		else
		{
			if (*midv < key)
			{
				stop = mid - 1;
			}
			else if (*midv > key)
			{
				start = mid + 1;
			}
			else
			{
				return mid;
			}		
		}
	}
	return -1;
}
void *sis_sort_list_find(s_sis_sort_list *list_, int64 key_)
{
	int index = _sort_list_find(list_, key_, 0, list_->key->count - 1);
	if (index >= 0)
	{
		return sis_struct_list_get(list_->value, index);
	}
	// for (int i = 0; i < list_->key->count; i++)
	// {
	// 	int *key = (int *)sis_struct_list_get(list_->key, i);
	// 	if (*key == key_)
	// 	{
	// 		return sis_struct_list_get(list_->value, i);
	// 	}
	// }
	return NULL;
}
void *sis_sort_list_get(s_sis_sort_list *list_, int index_)
{
	return sis_struct_list_get(list_->value, index_);
}
void *sis_sort_list_del(s_sis_sort_list *list_, void *value_)
{
	int index = (int)(((char *)value_ - (char *)list_->value->buffer) / list_->value->len);
	sis_struct_list_delete(list_->key, index, 1);
	sis_struct_list_delete(list_->value, index, 1);
	return sis_struct_list_get(list_->value, index);
}
void sis_sort_list_deli(s_sis_sort_list *list_, int index_)
{
	sis_struct_list_delete(list_->key, index_, 1);
	sis_struct_list_delete(list_->value, index_, 1);
}
int sis_sort_list_getsize(s_sis_sort_list *list_)
{
	return sis_min(list_->key->count, list_->value->count);
}
///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_pint_slist --------------------------------//
///////////////////////////////////////////////////////////////////////////
s_sis_pint_slist *sis_pint_slist_create() 
{
	s_sis_pint_slist *o = SIS_MALLOC(s_sis_pint_slist, o);
	o->isascend = 0;
	o->maxcount = 0;
	o->count = 0;
	return o;
}
void sis_pint_slist_destroy(void *list_)
{
	s_sis_pint_slist *list = (s_sis_pint_slist *)list_;
	if (list->keys)
	{
		sis_free(list->keys);
	}
	list->keys = NULL;
	if (list->value)
	{
		sis_free(list->value);
	}
	list->value = NULL;
	list->maxcount = 0;
	sis_free(list);
}
void sis_pint_slist_clear(s_sis_pint_slist *list_)
{
	list_->count = 0;
}
void sis_pint_slist_set_maxsize(s_sis_pint_slist *list_, int rows_)
{
	if (rows_ > list_->maxcount)
	{
		list_->maxcount = rows_;
		int  *keys  = sis_malloc(sizeof(int) * list_->maxcount);
		void *value = sis_malloc(sizeof(char *) * list_->maxcount);
		if (list_->count > 0)
		{
			memmove(keys,  list_->keys,  sizeof(int)    * list_->count);
			memmove(value, list_->value, sizeof(char *) * list_->count);
		}
		if (list_->keys)
		{
			sis_free(list_->keys);
		}
		// printf("::::: %d %p ", rows_, list_->value);
		if (list_->value)
		{
			sis_free(list_->value);
		}
		list_->keys = keys;
		list_->value= value;
		// printf(":= %p \n", list_->value);
		return ;	
	}
	// if (rows_ < list_->maxcount)
	// {

	// }
}

int sis_pint_slist_set(s_sis_pint_slist *list_, int key_, void *in_)
{
	int finded = -1;
	int index = list_->count;
	if (list_->isascend)
	{
		for (int i = 0; i < list_->count; i++)
		{
			if (list_->keys[i] == key_)
			{
				finded = i;
			}
			if (list_->keys[i] > key_)
			{
				index = i;
				// 插入
				break;
			}
		}
	}
	else
	{
		for (int i = 0; i < list_->count; i++)
		{
			if (list_->keys[i] == key_)
			{
				finded = i;
			}
			if (list_->keys[i] < key_)
			{
				index = i;
				// 插入
				break;
			}
		}
	}
	if (finded >= 0)
	{
		list_->keys[finded] = key_;
		char **ptr = (char **)list_->value;
		ptr[finded] = (char *)in_;
		return finded;
	}
	if (index >= list_->count)
	{
		if (list_->count < list_->maxcount)
		{
			list_->keys[list_->count] = key_;
			char **ptr = (char **)list_->value;
			ptr[list_->count] = (char *)in_;
			list_->count ++;
		}
		else
		{
			// 表示设置的值在区间范围外了
			return -1;
		}
	}
	else
	{
		// char **ptr = (char **)list_->value;
		int count = list_->count;
		if (list_->count >= list_->maxcount)
		{
			count = list_->maxcount - 1;
		}
		// for (int i = index; i < count; i++)
		// {
		// 	list_->keys[index + 1]
		// }
		

		// 向后方移动内存
		memmove((char *)list_->keys + ((index + 1) * sizeof(int)),
			(char *)list_->keys + (index * sizeof(int)),
			(count - index) * sizeof(int));
		memmove((char *)list_->value + ((index + 1) * sizeof(char *)),
			(char *)list_->value + (index * sizeof(char *)),
			(count - index) * sizeof(char *));

		list_->keys[index] = key_;
		char **ptr = (char **)list_->value;
		ptr[index] = in_;
		if (list_->count < list_->maxcount)
		{
			list_->count ++;
		}
	}
	return index;
}

void *sis_pint_slist_get(s_sis_pint_slist *list_, int index_)
{
	if (index_ < 0 || index_ > list_->count - 1)
	{
		return NULL;
	}
	char **ptr = (char **)list_->value;
	return ptr[index_];
}
int sis_pint_slist_delk(s_sis_pint_slist *list_, int key_)
{
	int index = -1;
	for (int i = 0; i < list_->count; i++)
	{
		if (list_->keys[i] == key_)
		{
			index = i;
			// 插入
			break;
		}
	}
	sis_pint_slist_del(list_, index);
	return index;
}
void sis_pint_slist_del(s_sis_pint_slist *list_, int index_)
{
	if (index_ < 0 || index_ > list_->count - 1)
	{
		return ;
	}
	if (index_ < list_->count - 1)
	{
		memmove((char *)list_->keys + (index_ * sizeof(int)), (char *)list_->keys + ((index_ + 1) * sizeof(int)),
				(list_->count - 1 - index_) * sizeof(int));
		memmove((char *)list_->value + (index_ * sizeof(void *)), (char *)list_->value + ((index_ + 1) * sizeof(void *)),
				(list_->count - 1 - index_) * sizeof(void *));
	}
	list_->count --;
}
int sis_pint_slist_getsize(s_sis_pint_slist *list_)
{
	return list_->count;
}
///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_double_list --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sis_double_list *sis_double_list_create()
{
	s_sis_double_list *o = SIS_MALLOC(s_sis_double_list, o);
	o->value = sis_struct_list_create(sizeof(double));
	return o;
}
void sis_double_list_destroy(void *list)
{
	s_sis_double_list *list_ = (s_sis_double_list *)list;
	sis_struct_list_destroy(list_->value);
	sis_free(list_);
}
void sis_double_list_clear(s_sis_double_list *list_)
{
	list_->avgv = 0.0;
	list_->maxv = 0.0;
	list_->minv = 0.0;
	sis_struct_list_clear(list_->value);
}

int sis_double_list_push(s_sis_double_list *list_, double in_)
{
	int count = list_->value->count;
	double sumv = count * list_->avgv;
	list_->avgv = (sumv + in_) / (count + 1);
	list_->maxv = SIS_MAXF(list_->maxv, in_);
	list_->minv = SIS_MINF(list_->minv, in_);

	return sis_struct_list_push(list_->value, &in_);
}
void sis_double_list_calc(s_sis_double_list *list_)
{
	list_->avgv = 0.0;
	list_->maxv = 0.0;
	list_->minv = 0.0;
	int count = list_->value->count;
	double sumv = 0.0;
	for (int i = 0; i < count; i++)
	{
		double v = sis_double_list_get(list_, i);
		sumv += v;
		list_->maxv = SIS_MAXF(list_->maxv, v);
		list_->minv = SIS_MINF(list_->minv, v);
	}
	list_->avgv = (sumv) / (count);
}

int sis_double_list_push_limit(s_sis_double_list *list_, int maxnums_, double in_)
{
	s_sis_struct_list *vlist = list_->value;
	if (vlist->maxcount < 2 * maxnums_)
	{
		sis_struct_list_set_maxsize(vlist, 2 * maxnums_);
	}
	if (vlist->count < maxnums_)
	{
		int offset = vlist->count + vlist->start;
		memmove((char *)vlist->buffer + (offset * vlist->len), &in_, vlist->len);
		vlist->count++;
	}
	else
	{
		if (vlist->start < maxnums_)
		{
			int offset = vlist->count + vlist->start;
			memmove((char *)vlist->buffer + (offset * vlist->len), &in_, vlist->len);
			vlist->start ++; 			
		}
		else
		{
			int offset = (vlist->start + 1) * vlist->len;
			memmove((char *)vlist->buffer,  (char *)vlist->buffer + offset, (vlist->count - 1) * vlist->len);
			vlist->start = 0;
			memmove((char *)vlist->buffer + ((vlist->count - 1) * vlist->len), &in_, vlist->len);
		}
	}
	return vlist->count;
}

double sis_double_list_get(s_sis_double_list *list_, int index_)
{
	double *o = (double *)sis_struct_list_get(list_->value, index_);
	if (o)
	{
		return *o;
	}
	return 0.0;
}
double *sis_double_list_gets(s_sis_double_list *list_, int index_)
{
	return (double *)sis_struct_list_get(list_->value, index_);
}

int sis_double_list_getsize(s_sis_double_list *list_)
{
	return list_->value->count;
}

int sis_sort_double_list(const void *arg1, const void *arg2 ) 
{ 
    return *(double *)arg1 > *(double *)arg2 ? 1 : -1;
}
int sis_sort_int32_list(const void *arg1, const void *arg2 ) 
{ 
    return *(int32 *)arg1 == *(int32 *)arg2 ? 0 : *(int32 *)arg1 > *(int32 *)arg2 ? 1 : -1;
}
void sis_double_list_sort(s_sis_double_list *list_)
{
   qsort(sis_struct_list_first(list_->value), list_->value->count, sizeof(double), sis_sort_double_list);
}

int sis_double_list_value_split(s_sis_double_list *list_, int nums_, double split[])
{
	sis_double_list_sort(list_);
	double sumv = list_->avgv * list_->value->count;
	double curv = sumv / nums_;
	int idx = 0;
	for (int i = 0; i < list_->value->count; i++)
	{
		double v = sis_double_list_get(list_, i);
		sumv += v;
		if (sumv >= curv)
		{
			split[idx] = v;
			idx++;
			if (idx >= nums_ - 1)
			{
				break;
			}
			sumv = 0.0;
		}
	}
	return idx;	
}
int sis_double_list_split(s_sis_double_list *list_, int nums_, double split[])
{
	if (nums_ >= list_->value->count || nums_ < 1)
	{
		return 0;
	}
	sis_double_list_sort(list_);
	double step = (double)list_->value->count / (double)(nums_ + 1);
	for (int i = 0; i < nums_; i++)
	{
		int stop  = (int)(step * (i + 1));
		split[i] = (sis_double_list_get(list_, stop - 1) + sis_double_list_get(list_, stop)) / 2.0;
	}
	return nums_;	
}
int sis_double_list_count_nozero_split(s_sis_double_list *list_, s_sis_struct_list *splits_, int nums_)
{
	double minimum = 0.001;
	s_sis_double_list *vlist = sis_double_list_create();
	for (int i = 0; i < list_->value->count; i++)
	{
		double v = sis_double_list_get(list_, i);
		if ( v > -1 * minimum && v < minimum )
		{
			continue;
		}
		sis_double_list_push(vlist, v);
	}		
	int o = sis_double_list_count_split(vlist, splits_, nums_);

	sis_double_list_destroy(vlist);
	
	return o;
}
int sis_double_list_count_zero_pair_nosort(s_sis_double_list *list_, s_sis_struct_list *splits_, int nums_)
{
	double minimum = 0.0001;
	s_sis_double_list *up_list = sis_double_list_create();
	s_sis_double_list *dn_list = sis_double_list_create();
	for (int i = 0; i < list_->value->count; i++)
	{
		double v = sis_double_list_get(list_, i);
		if ( v > minimum)
		{
			sis_double_list_push(up_list, v);
		}
		if ( v < -1 * minimum)
		{
			sis_double_list_push(dn_list, v);
		}
	}	
	int upnums = up_list->value->count > dn_list->value->count ? nums_ / 2 + nums_ % 2 : nums_ / 2;
	int dnnums = dn_list->value->count > up_list->value->count ? nums_ / 2 + nums_ % 2 : nums_ / 2;
	// printf("%d %d %d %d\n", upnums, dnnums, up_list->value->count, dn_list->value->count);
	if (upnums + dnnums < nums_)
	{
		dnnums += 1;
	}
	s_sis_double_split split;
	int dncount = sis_double_list_getsize(dn_list);
	if (dnnums < dncount && dnnums > 1)
	{
		double step = (double)dncount / (double)dnnums;
		double agov = sis_double_list_get(dn_list, 0) - minimum;
		for (int i = 0; i < dnnums; i++)
		{
			if (i == 0)
			{
				split.minv = agov;
				split.maxv = sis_double_list_get(dn_list, (int)step);
				agov = split.maxv;
			}
			else if (i == dnnums - 1)
			{
				split.minv = agov;
				split.maxv = -1 * minimum;
			}
			else
			{
				split.minv = agov;
				split.maxv = sis_double_list_get(dn_list, (int)((i + 1) * step));
				agov = split.maxv;
			}
			// printf("== %d %f | %f %f\n", dnnums, step, split.minv, split.maxv);
			sis_struct_list_push(splits_, &split);
		}
	}
	else
	{
		double minv = sis_double_list_get(dn_list, 0) - minimum;
		double maxv = -1 * minimum;
		double step = (maxv - minv) / (double)dnnums;
		for (int i = 0; i < dnnums; i++)
		{
			if (i == dnnums - 1)
			{
				split.minv = minv +  i * step;
				split.maxv = maxv;
			}
			else
			{
				split.minv = minv +  i * step; 
				split.maxv = minv + (i + 1) * step;
			}
			sis_struct_list_push(splits_, &split);
		}
	}
	int upcount = sis_double_list_getsize(up_list);
	if (upnums < upcount && upnums > 1)
	{
		double step = (double)upcount / (double)upnums;
		double agov = minimum;
		for (int i = 0; i < upnums; i++)
		{
			if (i == 0)
			{
				split.minv = minimum;
				split.maxv = sis_double_list_get(up_list, (int)step);
				agov = split.maxv;
			}
			else if (i == upnums - 1)
			{
				split.minv = agov;
				split.maxv = sis_double_list_get(up_list, upcount - 1) + minimum;
			}
			else
			{
				split.minv = agov;
				split.maxv = sis_double_list_get(up_list, (int)((i + 1) * step));
				agov = split.maxv;
			}
			sis_struct_list_push(splits_, &split);
		}
	}
	else
	{
		double minv = minimum;
		double maxv = sis_double_list_get(up_list, upcount - 1) + minimum;
		double step = (maxv - minv) / (double)upnums;
		for (int i = 0; i < upnums; i++)
		{
			if (i == upnums - 1)
			{
				split.minv = minv +  i * step;
				split.maxv = maxv;
			}
			else
			{
				split.minv = minv +  i * step; 
				split.maxv = minv + (i + 1) * step;
			}
			sis_struct_list_push(splits_, &split);
		}
	}
	sis_double_list_destroy(up_list);
	sis_double_list_destroy(dn_list);

	return splits_->count;	
}
int sis_double_list_count_zero_pair(s_sis_double_list *list_, s_sis_struct_list *splits_, int nums_)
{
	double minimum = 0.000000001;
	s_sis_double_list *up_list = sis_double_list_create();
	s_sis_double_list *dn_list = sis_double_list_create();

	for (int i = 0; i < list_->value->count; i++)
	{
		double v = sis_double_list_get(list_, i);
		if ( v > minimum)
		{
			sis_double_list_push(up_list, v);
		}
		if ( v < -1 * minimum)
		{
			sis_double_list_push(dn_list, v);
		}
	}	
	if (nums_ > up_list->value->count || nums_ > dn_list->value->count)
	{
		sis_double_list_destroy(up_list);
		sis_double_list_destroy(dn_list);
		return 0;
	}
	sis_double_list_sort(up_list);
	sis_double_list_sort(dn_list);

	s_sis_double_split split;
	{
		double step = (double)sis_double_list_getsize(up_list) / (double)nums_;
		double ago = minimum;
		for (int i = 0; i < nums_; i++)
		{
			if (i == 0)
			{
				split.minv = ago;
				split.maxv = sis_double_list_get(up_list, (int)step) - minimum;
				ago = split.maxv;
			}
			else if (i == nums_ - 1)
			{
				split.minv = ago + minimum; // sis_double_list_get(up_list, (int)(i * step));
				split.maxv = sis_double_list_get(up_list, up_list->value->count - 1) + minimum;
			}
			else
			{
				split.minv = ago + minimum; // sis_double_list_get(up_list, (int)(i * step));
				split.maxv = sis_double_list_get(up_list, (int)((i + 1) * step)) - minimum;
				ago = split.maxv;
			}
			sis_struct_list_push(splits_, &split);
		}
	}
	{
		double step = (double)sis_double_list_getsize(dn_list) / (double)nums_;
		double ago = sis_double_list_get(dn_list, 0) - minimum;
		for (int i = 0; i < nums_; i++)
		{
			if (i == 0)
			{
				split.minv = ago; // sis_double_list_get(dn_list, 0) - minimum;
				split.maxv = sis_double_list_get(dn_list, (int)step) - minimum;
				ago = split.maxv;
			}
			else if (i == nums_ - 1)
			{
				split.minv = ago + minimum; // sis_double_list_get(dn_list, (int)(i * step));
				split.maxv = -1 * minimum; // sis_double_list_get(dn_list, dn_list->value->count - 1) + minimum;
			}
			else
			{
				split.minv = ago + minimum; // sis_double_list_get(dn_list, (int)(i * step));
				split.maxv = sis_double_list_get(dn_list, (int)((i + 1) * step)) - minimum;
				ago = split.maxv;
			}
			sis_struct_list_push(splits_, &split);
		}
	}
	sis_double_list_destroy(up_list);
	sis_double_list_destroy(dn_list);

	return splits_->count;	
}

int sis_double_list_count_split(s_sis_double_list *list_, s_sis_struct_list *splits_, int nums_)
{
	double minimum = 0.000000001;
	sis_struct_list_clear(splits_);
	int count = list_->value->count;
	if (nums_ > count)
	{
		return 0;
	}
	double *ins = sis_malloc(sizeof(double)*count);
	memmove(ins, sis_struct_list_first(list_->value), sizeof(double)*count);

    qsort(ins, count, sizeof(double), sis_sort_double_list);

	double step = (double)count / (double)nums_;
	s_sis_double_split split;
	int start = 0;
	int stop = 0;
	for (int i = 0; i < nums_; i++)
	{
		if (i == 0)
		{	
			stop = (int)step;
			split.minv = ins[0] - minimum;
			split.maxv = ins[stop] - minimum;			
		}
		else 
		if (i == nums_ - 1)
		{
			stop = count - 1;
			split.minv = ins[start];
			split.maxv = ins[stop] + minimum;			
		}
		else
		{
			stop = (int)(step * (i + 1));
			split.minv = ins[start];
			split.maxv = ins[stop] - minimum;
		}
		start = stop;	
		sis_struct_list_push(splits_, &split);
	}
	sis_free(ins);
	return splits_->count;
}

int sis_double_list_count_sides(s_sis_double_list *list_, s_sis_struct_list *sides_, int steps_)
{
	s_sis_struct_list *splits = sis_struct_list_create(sizeof(s_sis_double_split));
	sis_double_list_count_split(list_, splits, steps_);
	if (splits->count < 1)
	{
		sis_struct_list_destroy(splits);
		return 0;
	}
	sis_struct_list_clear(sides_);
	for (int m = steps_ - 1; m > 0; m--)
	{
		s_sis_double_split *upmin = (s_sis_double_split *)sis_struct_list_get(splits, m);  
		for (int n = m; n < steps_; n++)
		{
			s_sis_double_split *upmax = (s_sis_double_split *)sis_struct_list_get(splits, n);  
			for (int i = 0; i < m; i++)
			{
				s_sis_double_split *dnmin = (s_sis_double_split *)sis_struct_list_get(splits, i);  
				for (int j = i; j < m; j++)
				{
					s_sis_double_split *dnmax = (s_sis_double_split *)sis_struct_list_get(splits, j);  
	
					s_sis_double_sides sides;
					sides.up_minv = upmin->minv;
					sides.up_maxv = upmax->maxv;
					sides.dn_minv = dnmin->minv;
					sides.dn_maxv = dnmax->maxv;
					// printf("[%3d] [%d.%d.%d.%d]: %7.2f %7.2f | %7.2f %7.2f\n",
					// 	list_->sides->count, m,n,i,j,
					// 	sides.up_minv, sides.up_maxv, sides.dn_minv, sides.dn_maxv);
					sis_struct_list_push(sides_, &sides);
				}
			}
		}
	}
	sis_struct_list_destroy(splits);
	return sides_->count;
}

int sis_get_simple_split(s_sis_struct_list *out_, double minv_, double maxv_, int nums_)
{
	double minimum = 0.001;
	sis_struct_list_clear(out_);
	if (!out_ || (maxv_ - minv_) < (nums_ + 1) * minimum)
	{
		return 0;
	}	
	s_sis_double_split split;
	double gapv = (maxv_ - minv_) / (double)nums_;
	for (int i = 0; i < nums_; i++)
	{
		if (i == 0)
		{	
			split.minv = minv_ - minimum;
			split.maxv = split.minv + gapv;			
		}
		else 
		if (i == nums_ - 1)
		{
			split.minv = split.maxv + minimum;
			split.maxv = maxv_ + minimum;			
		}
		else
		{
			split.minv = split.maxv + minimum;
			split.maxv = split.minv + gapv;
		}
		sis_struct_list_push(out_, &split);
	}
	return out_->count;
}


int sis_double_list_simple_split(s_sis_double_list *list_, s_sis_struct_list *splits_, int nums_)
{	
	if ((list_->maxv - list_->minv) < (nums_ + 1) * 0.001)
	{
		return 0;
	}	
	sis_get_simple_split(splits_, list_->minv, list_->maxv, nums_);
	return splits_->count;
}

int sis_double_list_simple_sides(s_sis_double_list *list_, s_sis_struct_list *sides_, int steps_)
{
	s_sis_struct_list *splits = sis_struct_list_create(sizeof(s_sis_double_split));
	sis_double_list_simple_split(list_, splits, steps_);
	if (splits->count < 1)
	{
		sis_struct_list_destroy(splits);
		return 0;
	}
	sis_struct_list_clear(sides_);
	for (int m = steps_ - 1; m > 0; m--)
	{
		s_sis_double_split *upmin = (s_sis_double_split *)sis_struct_list_get(splits, m);  
		for (int n = m; n < steps_; n++)
		{
			s_sis_double_split *upmax = (s_sis_double_split *)sis_struct_list_get(splits, n);  
			for (int i = 0; i < m; i++)
			{
				s_sis_double_split *dnmin = (s_sis_double_split *)sis_struct_list_get(splits, i);  
				for (int j = i; j < m; j++)
				{
					s_sis_double_split *dnmax = (s_sis_double_split *)sis_struct_list_get(splits, j);  
	
					s_sis_double_sides sides;
					sides.up_minv = upmin->minv;
					sides.up_maxv = upmax->maxv;
					sides.dn_minv = dnmin->minv;
					sides.dn_maxv = dnmax->maxv;
					// printf("[%3d] [%d.%d.%d.%d]: %7.2f %7.2f | %7.2f %7.2f\n",
					// 	list_->sides->count, m,n,i,j,
					// 	sides.up_minv, sides.up_maxv, sides.dn_minv, sides.dn_maxv);
					sis_struct_list_push(sides_, &sides);
				}
			}
		}
	}
	sis_struct_list_destroy(splits);
	return sides_->count;
}
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
	sbl->vfree = NULL;
	return sbl;
}
void sis_pointer_list_destroy(void *list_)
{
	s_sis_pointer_list *list = (s_sis_pointer_list *)list_;
	sis_pointer_list_clear(list);
	if (list->buffer)
	{
		sis_free(list->buffer);
	}
	list->buffer = NULL;
	list->maxcount = 0;
	sis_free(list);
}
void sis_pointer_list_clear(s_sis_pointer_list *list_)
{
	char **ptr = (char **)list_->buffer;
	for (int i = 0; i < list_->count; i++)
	{
		if (list_->vfree && ptr[i])
		{
			list_->vfree(ptr[i]);
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
	void *newbuffer = sis_malloc(maxlen * list_->len);
	if (list_->buffer)
	{
		if (list_->maxcount > 0)
		{
			memmove(newbuffer, list_->buffer, list_->maxcount * list_->len);
		}
		sis_free(list_->buffer);
	}
	list_->buffer = newbuffer;
	// list_->buffer = sis_realloc(list_->buffer, maxlen * list_->len);
	list_->maxcount = maxlen;
}
int sis_pointer_list_clone(s_sis_pointer_list *src_, s_sis_pointer_list *des_)
{
	sis_pointer_list_clear(des_);
	_pointer_list_grow(des_, src_->count);
	memmove(des_->buffer, src_->buffer, src_->count*src_->len);
	des_->count = src_->count;
	return des_->count;
}

int sis_pointer_list_push(s_sis_pointer_list *list_, void *in_)
{
	_pointer_list_grow(list_, list_->count + 1);
	char **ptr = (char **)list_->buffer;
	ptr[list_->count] = (char *)in_;
	list_->count++;
	return list_->count;
}
int sis_pointer_list_set(s_sis_pointer_list *list_, void *in_)
{
	int index = sis_pointer_list_indexof(list_, in_);
	if ( index < 0)
	{
		sis_pointer_list_push(list_, in_);
	}
	else
	{
		sis_pointer_list_update(list_, index, in_);
	}
	return index;
}

int sis_pointer_list_update(s_sis_pointer_list *list_, int index_, void *in_)
{
	if (index_ >= 0 && index_ < list_->count)
	{
		char **ptr = (char **)list_->buffer;
		if (list_->vfree && ptr[index_])
		{
			list_->vfree(ptr[index_]);
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
// void *sis_pointer_list_next(s_sis_pointer_list *list_, void *current_)
// {
// 	int offset = 1;
// 	char *o = (char *)current_ + offset * list_->len;
// 	if (o >= (char *)list_->buffer + list_->start * list_->len &&
// 		o <= (char *)list_->buffer + (list_->start + list_->count - 1) * list_->len)
// 	{
// 		return o;
// 	}
// 	else
// 	{
// 		return NULL;
// 	}
// }
int sis_pointer_list_delete(s_sis_pointer_list *list_, int start_, int count_)
{
	if (start_ < 0 || count_ < 1 || start_ + count_ > list_->count)
	{
		return 0;
	}

	char **ptr = (char **)list_->buffer;
	for (int i = start_; i < start_ + count_; i++)
	{
		if (list_->vfree && ptr[i])
		{
			list_->vfree(ptr[i]);
		}
	}
	if ((list_->count > count_ + start_))
	{
		memmove((char *)list_->buffer + (start_ * list_->len), (char *)list_->buffer + ((start_ + count_) * list_->len),
			(list_->count - count_ - start_) * list_->len);
	}

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
			if (list_->vfree && ptr[i])
			{
				list_->vfree(ptr[i]);
			}
			memmove(&ptr[i], &ptr[i + 1], (list_->count - 1 - i) * list_->len);
			list_->count--;
			return i;
		}
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_fsort_list --------------------------------//
///////////////////////////////////////////////////////////////////////////
int sis_int_list_indexof(s_sis_int_list *list_, int64 in_)
{
	int64 *ptr = (int64 *)list_->buffer;
	for (int i = 0; i < list_->count; i++)
	{
		if (ptr[i] == in_)
		{
			return i;
		}
	}
	return -1;
}

int   sis_int_list_push(s_sis_int_list *list_, int64 in_)
{
	_pointer_list_grow(list_, list_->count + 1);
	int64 *ptr = (int64 *)list_->buffer;
	ptr[list_->count] = in_;
	list_->count++;
	return list_->count;
}
int   sis_int_list_update(s_sis_int_list *list_, int index_, int64 in_)
{
	if (index_ >= 0 && index_ < list_->count)
	{
		int64 *ptr = (int64 *)list_->buffer;
		ptr[index_] = in_;
		return index_;
	}
	return -1;	
}
int   sis_int_list_insert(s_sis_int_list *list_, int index_, int64 in_)
{
	if (list_->count < 1)
	{
		return sis_int_list_push(list_, in_);
	}
	if (index_ < 0 || index_ > list_->count - 1)
	{
		return -1;
	}
	_pointer_list_grow(list_, list_->count + 1);
	memmove((char *)list_->buffer + ((index_ + 1) * list_->len), (char *)list_->buffer + (index_ * list_->len),
			(list_->count - index_) * list_->len);

	int64 *ptr = (int64 *)list_->buffer;
	ptr[index_] = in_;

	list_->count++;
	return index_;
}
int64 sis_int_list_get(s_sis_int_list *list_, int index_)
{
	int i64 = -1;
	if (index_ >= 0 && index_ < list_->count)
	{
		int64 *ptr = (int64 *)list_->buffer;
		i64 = ptr[index_];
	}
	return i64;
}
int   sis_int_list_delete(s_sis_int_list *list_, int start_, int count_)
{
	if (start_ < 0 || count_ < 1 || start_ + count_ > list_->count)
	{
		return 0;
	}
	if ((list_->count > count_ + start_))
	{
		memmove((char *)list_->buffer + (start_ * list_->len), (char *)list_->buffer + ((start_ + count_) * list_->len),
			(list_->count - count_ - start_) * list_->len);
	}

	list_->count = list_->count - count_;
	return count_;
}

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_fsort_list --------------------------------//
///////////////////////////////////////////////////////////////////////////

s_sis_fsort_list *sis_fsort_list_create(void *vfree_)
{
	s_sis_fsort_list *o = SIS_MALLOC(s_sis_fsort_list, o);
	o->key = sis_struct_list_create(sizeof(double));
	o->value = sis_pointer_list_create();
	o->value->vfree = vfree_;
	return o;
}
void sis_fsort_list_destroy(void *list_)
{
	s_sis_fsort_list *list = (s_sis_fsort_list *)list_;
	sis_struct_list_destroy(list->key);
	sis_pointer_list_destroy(list->value);
	sis_free(list);
}
void sis_fsort_list_clear(s_sis_fsort_list *list_)
{
	sis_struct_list_clear(list_->key);
	sis_pointer_list_clear(list_->value);
}
int _fsort_list_find(s_sis_fsort_list *list_, double key_)
{
	int index = -1;
	for (int i = 0; i < list_->key->count; i++)
	{
		double *midv = (double *)sis_struct_list_get(list_->key, i);
		if (*midv < key_)
		{
			return i;
		}
		index = i + 1;
	}
	return index;
}
int sis_fsort_list_set(s_sis_fsort_list *list_, double key_, void *in_)
{
	int index = _fsort_list_find(list_, key_);
	if (index < 0 || index > list_->key->count - 1)
	{
		index = list_->key->count;
		sis_struct_list_push(list_->key, &key_);
		sis_pointer_list_push(list_->value, in_);
	}
	else
	{
		sis_struct_list_insert(list_->key, index, &key_);
		sis_pointer_list_insert(list_->value, index, in_);
	}
	return index;
}
int sis_fsort_list_find(s_sis_fsort_list *list_, void *value_)
{
	for (int i = 0; i < list_->value->count; i++)
	{
		void *v = sis_pointer_list_get(list_->value, i);
		if (v == value_)
		{
			return i;
		}
	}
	return -1;
}
void *sis_fsort_list_get(s_sis_fsort_list *list_, int index_)
{
	return sis_pointer_list_get(list_->value, index_);
}
double sis_fsort_list_getkey(s_sis_fsort_list *list_, int index_)
{
	return *(double *)sis_struct_list_get(list_->key, index_);
}
void sis_fsort_list_del(s_sis_fsort_list *list_, int index_)
{
	sis_struct_list_delete(list_->key, index_, 1);
	sis_pointer_list_delete(list_->value, index_, 1);
}
int sis_fsort_list_getsize(s_sis_fsort_list *list_)
{
	return sis_min(list_->key->count, list_->value->count);
}
///////////////////////////////////////////////////////////////////////////
//----------------------s_sis_index_list --------------------------------//
//  以整数为索引 存储指针的列表
//  如果有wait就说明释放index需要等待一个超时
///////////////////////////////////////////////////////////////////////////
s_sis_index_list *sis_index_list_create(int count_)
{
	s_sis_index_list *o = (s_sis_index_list *)sis_malloc(sizeof(s_sis_index_list));
	o->count = count_ < 1024 ? 1024 : count_;
	o->used = sis_malloc(o->count);
	memset(o->used, 0 ,o->count);
	o->stop_sec = sis_malloc(sizeof(fsec_t) * o->count);
	memset(o->stop_sec, 0 ,sizeof(fsec_t) * o->count);
	o->buffer = sis_malloc(sizeof(void *) * o->count);
	memset(o->buffer, 0 ,sizeof(void *) * o->count);
	o->vfree = NULL;
	// o->wait_sec == 0 表示立即释放 否则等待时间到再释放
	return o;
}
void sis_index_list_destroy(s_sis_index_list *list_)
{
	sis_index_list_clear(list_);
	sis_free(list_->used);
	sis_free(list_->buffer);
	sis_free(list_->stop_sec);
	sis_free(list_);
}
void sis_index_list_clear(s_sis_index_list *list_)
{
	char **ptr = (char **)list_->buffer;
	fsec_t *stop_sec = (fsec_t *)list_->stop_sec;
	for (int i = 0; i < list_->count; i++)
	{
		if (list_->vfree && ptr[i])
		{
			list_->vfree(ptr[i]);
			ptr[i] = NULL;
		}
		list_->used[i] = 0;
		stop_sec[i] = 0;
	}
	list_->count = 0;
}
int sis_index_list_set(s_sis_index_list *list_, int index_, void *in_)
{
	if (index_ < 0 || index_ >= list_->count)
	{
		return -1;
	}
	sis_index_list_del(list_, index_);
	char **ptr = (char **)list_->buffer;
	ptr[index_] = (char *)in_;
	list_->used[index_] = 1;
	return index_;
}
void _index_list_free(s_sis_index_list *list_, int index_)
{
	char **ptr = (char **)list_->buffer;
	if (list_->vfree && ptr[index_])
	{
		list_->vfree(ptr[index_]);
		ptr[index_] = NULL;
	}
	list_->used[index_] = 0;
}
int sis_index_list_new(s_sis_index_list *list_)
{
	// 先检查超时的
    if(list_->wait_sec > 0)
	{
		fsec_t *stop_sec = (fsec_t *)list_->stop_sec;
		fsec_t now_sec = sis_time_get_now();
		for (int i = 0; i < list_->count; i++)
		{
			if (list_->used[i] == 2 && (now_sec - stop_sec[i]) > list_->wait_sec)
			{
				_index_list_free(list_, i);
			}
		}
	}
	int index = -1;
	for (int i = 0; i < list_->count; i++)
	{
		if (list_->used[i] == 0)
		{
			index = i;
			break;
		}
	}
	return index;
}
void *sis_index_list_get(s_sis_index_list *list_, int index_)
{
	if (index_ < 0 || index_ >= list_->count)
	{
		return NULL;
	}	
	if (list_->used[index_] != 1)
	{
		return NULL;
	}
	// printf("list_->used[%d] = %d\n", index_, list_->used[index_]);
	char **ptr = (char **)list_->buffer;
	return ptr[index_];;
}
int sis_index_list_first(s_sis_index_list *list_)
{
	int index = -1;
	for (int i = 0; i < list_->count; i++)
	{
		if (list_->used[i] == 1)
		{
			index = i;
			break;
		}
	}
	return index;
}
int sis_index_list_next(s_sis_index_list *list_, int index_)
{
	int index = -1;
	for (int i = index_ + 1; i < list_->count; i++)
	{
		if (list_->used[i] == 1)
		{
			index = i;
			break;
		}
	}
	return index;
}
int sis_index_list_uses(s_sis_index_list *list_)
{
	int count = 0;
	for (int i = 0; i < list_->count; i++)
	{
		if (list_->used[i] == 1)
		{
			count++;
		}
	}
	return count;	
}

int sis_index_list_del(s_sis_index_list *list_, int index_)
{
	if (index_ < 0 || index_ >= list_->count)
	{
		return -1;
	}	
	if (list_->used[index_] != 1)
	{
		return -1;
	}
	// char **ptr = (char **)list_->buffer;
	// if (list_->vfree && ptr[index_])
	// {
	// 	list_->vfree(ptr[index_]);
	// 	ptr[index_] = NULL;
	// }
	if (list_->wait_sec > 0)
	{
		fsec_t *stop_sec = (fsec_t *)list_->stop_sec;
		stop_sec[index_] = sis_time_get_now();
		list_->used[index_] = 2;
	}
	else
	{
		_index_list_free(list_, index_);
	}
	// printf("list_->used[%d] = %d\n", index_, list_->used[index_]);
	return index_;
}

///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_string_list --------------------------------//
//  存储不定长字符串的列表，
///////////////////////////////////////////////////////////////////////////

s_sis_string_list *sis_string_list_create() //读写
{
	s_sis_string_list *l = (s_sis_string_list *)sis_malloc(sizeof(s_sis_string_list));
	memset(l, 0, sizeof(s_sis_string_list));
	l->strlist = sis_pointer_list_create();
	l->permissions = STRING_LIST_WR;
	l->strlist->vfree = sis_free_call;
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

int sis_string_list_load(s_sis_string_list *list_, const char *in_, size_t inlen_, const char *sign)
{
	sis_string_list_clear(list_);

	int signsize = strlen(sign);

	char *inptr = (char *)sis_malloc(inlen_ + 1);
	memmove(inptr, in_, inlen_);
	inptr[inlen_] = 0;

	if (list_->permissions == STRING_LIST_RD)
	{
		list_->m_ptr_r = inptr;
	}
	int size = 0;
	char *curr = (char *)inptr;
	char *move = (char *)inptr;
	char *newp = NULL;
	while (size < inlen_)
	{
		if (!memcmp(curr, sign, signsize))
		{
			*curr = 0;
			if (list_->permissions == STRING_LIST_WR)
			{
				int len = curr - move;
				newp = (char *)sis_malloc(len + 1);
				memmove(newp, move, len);
				newp[len] = 0;
			}
			else
			{
				newp = move;
			}
			curr += signsize;
			size += signsize;
			move = curr;
			sis_trim(newp);
			sis_pointer_list_push(list_->strlist, newp);
		}
		else
		{
			curr++;
			size++;
		}
	}
	if (curr > move)
	{
		if (list_->permissions == STRING_LIST_WR)
		{
			int len = curr - move;
			newp = (char *)sis_malloc(len + 1);
			memmove(newp, move, len);
			newp[len] = 0;
		}
		else
		{
			newp = move;
		}
		sis_trim(newp);
		sis_pointer_list_push(list_->strlist, newp);
	}
	if (list_->permissions == STRING_LIST_WR)
	{
		sis_free(inptr);
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
		return sis_pointer_list_push(list_->strlist, str) - 1;
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
	char buff[100];// = (char *)sis_malloc(100);
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
	s_sis_string_list *list = sis_string_list_create();
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
	s_sis_string_list *list = sis_string_list_create();
	sis_string_list_load(list, src, strlen(src),",");

	class->in = list;
	class->out = sis_string_list_create();
	//////////////
	// const char *des = "1,3,5,7,9,11";
	// s_sis_string_list *deslist = sis_string_list_create();
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

#if 0
int main()
{
	s_sis_double_list *vlist = sis_double_list_create(); 

    for (int i = 0; i < 20; i++)
    {
        sis_double_list_push(vlist, i + 1);
    }
    sis_double_list_count_sides(vlist, 5);
    for (int i = 0; i < vlist->sides->count; i++)
    {
        s_sis_double_sides *sides =(s_sis_double_sides *)sis_struct_list_get(vlist->sides, i);
        printf("count_sides [%3d] : %7.3f %7.3f | %7.3f %7.3f\n",
            i, sides->up_minv, sides->up_maxv, sides->dn_minv, sides->dn_maxv);
    }
    sis_double_list_simple_sides(vlist, 5);
    for (int i = 0; i < vlist->sides->count; i++)
    {
        s_sis_double_sides *sides =(s_sis_double_sides *)sis_struct_list_get(vlist->sides, i);
        printf("simple_sides [%3d] : %7.3f %7.3f | %7.3f %7.3f\n",
            i, sides->up_minv, sides->up_maxv, sides->dn_minv, sides->dn_maxv);
    }
    sis_double_list_count_split(vlist, 5);
    for (int i = 0; i < vlist->split->count; i++)
    {
        s_sis_double_split *split =(s_sis_double_split *)sis_struct_list_get(vlist->split, i);
        printf("count_split [%3d] : %7.3f %7.3f \n",i, split->minv, split->maxv);
    }
    sis_double_list_simple_split(vlist, 5);
    for (int i = 0; i < vlist->split->count; i++)
    {
        s_sis_double_split *split =(s_sis_double_split *)sis_struct_list_get(vlist->split, i);
        printf("simple_split [%3d] : %7.3f %7.3f \n",i, split->minv, split->maxv);
    }

    sis_double_list_destroy(vlist);
	return 0;
}
#endif

#if 0
// test s_sis_pint_slist
int main()
{
	s_sis_pint_slist *v = sis_pint_slist_create(); 

	sis_pint_slist_set_maxsize(v, 5);
	
	for (int i = 0; i < 20; i++)
	{
		sis_pint_slist_set(v, i, (void *)i);
		printf("===%d===\n", i);
		for (int k = 0; k < v->count; k++)
		{
			void *s = sis_pint_slist_get(v, k);
			printf("::===%d %d===%lld\n", v->count, k, (int64)s);
		}
	}
	printf("======\n");
	sis_pint_slist_delk(v, 16);
	for (int k = 0; k < v->count; k++)
	{
		void *s = sis_pint_slist_get(v, k);
		printf("::===%d %d===%lld\n", v->count, k, (int64)s);
	}
	printf("======\n");
	sis_pint_slist_set(v, 18, (void *)1001);
	for (int k = 0; k < v->count; k++)
	{
		void *s = sis_pint_slist_get(v, k);
		printf("::===%d %d===%lld\n", v->count, k, (int64)s);
	}
	sis_pint_slist_set_maxsize(v, 10);
	printf("======\n");
	sis_pint_slist_set(v, 13, (void *)1002);
	for (int k = 0; k < v->count; k++)
	{
		void *s = sis_pint_slist_get(v, k);
		printf("::===%d %d===%lld\n", v->count, k, (int64)s);
	}
	printf("======\n");
	sis_pint_slist_set(v, 16, (void *)1003);
	for (int k = 0; k < v->count; k++)
	{
		void *s = sis_pint_slist_get(v, k);
		printf("::===%d %d===%lld\n", v->count, k, (int64)s);
	}

	/////
	printf("===isascend===\n");
	v->isascend = 1;
	sis_pint_slist_clear(v);

	for (int i = 0; i < 20; i = i + 2)
	{
		sis_pint_slist_set(v, i, (void *)i);
		printf("===%d===\n", i);
		for (int k = 0; k < v->count; k++)
		{
			void *s = sis_pint_slist_get(v, k);
			printf("::===%d %d===%lld\n", v->count, k, (int64)s);
		}
	}
	printf("======\n");
	sis_pint_slist_delk(v, 2);
	for (int k = 0; k < v->count; k++)
	{
		void *s = sis_pint_slist_get(v, k);
		printf("::===%d %d===%lld\n", v->count, k, (int64)s);
	}
	printf("======\n");
	sis_pint_slist_set(v, 3, (void *)1001);
	for (int k = 0; k < v->count; k++)
	{
		void *s = sis_pint_slist_get(v, k);
		printf("::===%d %d===%lld\n", v->count, k, (int64)s);
	}

	sis_pint_slist_destroy(v);

	return 0;
}
#endif