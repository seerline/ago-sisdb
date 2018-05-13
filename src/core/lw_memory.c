

#include "lw_memory.h"

static struct s_memory_pool __memory_pool;
static struct s_safe_memory __auto_memory;

void memory_pool_start()
{
	mutex_create(&__memory_pool.memory_mutex);
	size_t size_ = MEMORY_POOL_STEP;
	__memory_pool.memory_total_count = size_;
	__memory_pool.memory_used_count = 0;

	__memory_pool.memory_node_list = listCreate();
	while (size_--)
	{
		s_memory_node *val = create_memory_node();
		__memory_pool.memory_node_list = listAddNodeTail(__memory_pool.memory_node_list, val);
	}
}
void memory_pool_stop()
{
	mutex_lock(&__memory_pool.memory_mutex);

	s_memory_node *val = NULL;
	unsigned long len;
	listNode *current, *next;

	current = __memory_pool.memory_node_list->head;
	len = __memory_pool.memory_node_list->len;
	while (len--) {
		next = current->next;
		val = (s_memory_node *)current->value;	
		destroy_memory_node(val);
		zfree(current);
		current = next;
	}
	zfree(__memory_pool.memory_node_list);

	mutex_unlock(&__memory_pool.memory_mutex);
	mutex_destroy(&__memory_pool.memory_mutex);
}

size_t memory_pool_free_count()
{
	return __memory_pool.memory_node_list->len;
};
size_t  memory_pool_used_count()
{
	return __memory_pool.memory_used_count;
}
size_t  memory_pool_total_count()
{
	return __memory_pool.memory_total_count;
}

s_memory_node *pmalloc(size_t len_)
{
	mutex_lock(&__memory_pool.memory_mutex);
	listNode *node = __memory_pool.memory_node_list->head;

	if (__memory_pool.memory_node_list->len < MEMORY_POOL_MIN){
		size_t size_ = MEMORY_POOL_STEP;
		__memory_pool.memory_total_count += size_;
		while (size_--)
		{
			s_memory_node *val = create_memory_node();
			__memory_pool.memory_node_list = listAddNodeTail(__memory_pool.memory_node_list, val);
		}
	}
	node = listPopNode(__memory_pool.memory_node_list);
	s_memory_node *mem = (s_memory_node *)node->value;
	mem->cite = 1;
	__memory_pool.memory_used_count ++;
	zfree(node);

	mutex_unlock(&__memory_pool.memory_mutex);
	return mem;
};
void incr_memory(s_memory_node *str_)
{
	str_->cite ++;
}
void decr_memory(s_memory_node *str_){
	if (str_->cite > 1) {
		str_->cite--; 
	} else if (str_->cite == 1) 
	{
		str_->cite = 0;
		str_->len = 0;
		mutex_lock(&__memory_pool.memory_mutex);
		__memory_pool.memory_node_list = listAddNodeTail(__memory_pool.memory_node_list, str_);
		__memory_pool.memory_used_count --;
		mutex_unlock(&__memory_pool.memory_mutex);
	}
};

////////////////////////////////////////
//  s_memory_node µÄ¶¨Òå
////////////////////////////////////////


void _check_length(s_memory_node *n_, size_t size_)
{
	if (size_ < n_->maxlen)	{ return; }
	n_->maxlen = size_;
	if (size_ < 128) { n_->maxlen = 128; }
	else if (size_ >= 128 && size_ < 256) { n_->maxlen = 256; }
	else if (size_ >= 256 && size_ < MEMORY_BLOCK_STEP) { n_->maxlen = MEMORY_BLOCK_STEP; }
	else {
		int mul = (int)(size_ / MEMORY_BLOCK_STEP * 0.618);
		mul = mul < 1 ? 1 : mul;
		n_->maxlen = size_ + mul * MEMORY_BLOCK_STEP;
	}
	char *tmp = (char *)zmalloc(n_->maxlen);
	if (n_->len > 0) { memmove(tmp, n_->str, n_->len); }
	zfree(n_->str);
	n_->str = tmp;
};
void smn_set(s_memory_node *n_, const char *in_, size_t inlen_)
{
	if (inlen_ <= 0){ inlen_ = strlen(in_); }
	if (inlen_ >= n_->maxlen)
	{
		zfree(n_->str);
		n_->maxlen = inlen_ + 1;
		n_->str = (char *)zmalloc(n_->maxlen);
	}
	memmove(n_->str, in_, inlen_);
	n_->len = inlen_;
	n_->str[n_->len] = 0;
};
void smn_add(s_memory_node *n_, const char *in_, size_t inlen_)
{
	_check_length(n_, n_->len + inlen_);
	memmove(n_->str + n_->len, in_, inlen_);
	n_->len += inlen_;
	n_->str[n_->len] = 0;
}
void smn_merge(s_memory_node *n_, s_memory_node *in_)
{
	_check_length(n_, n_->len + in_->len);
	//printf("append:%p [%d]\n",in_, in_->len);
	memmove(n_->str + n_->len, in_->str, in_->len);
	n_->len += in_->len;
	n_->str[n_->len] = 0;
}
void smn_clear(s_memory_node *n_)
{
	n_->len = 0;
}
size_t smn_getsize(s_memory_node *n_)
{
	return n_->len;
}
s_memory_node *smn_substr(s_memory_node *n_, size_t start, size_t len_)
{
	start = max(0, start);
	size_t stop = min(n_->len - 1, start + len_ - 1);
	if (start >= stop) {
		return NULL;
	}
	s_memory_node *m = pmalloc(stop - start + 1);
	smn_add(m, n_->str + start, stop - start + 1);
	return m;
}
void smn_erase(s_memory_node *n_, size_t start, size_t len_)
{
	start = max(0, start);
	size_t stop = min(n_->len - 1, start + len_ - 1);
	if (start > stop) { return; }
	if (n_->len > (stop + 1))	{ memmove(n_->str + start, n_->str + stop + 1, n_->len - (stop + 1)); }
	n_->len = start + n_->len - (stop + 1);
	n_->str[n_->len] = 0;
}
int smn_loadfile(s_memory_node *n_, const char *filename_)
{
	n_->len = 0;
	FILE *fp = fopen(filename_, "r");
	if (!fp)	 { return -1; }
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	_check_length(n_, size + 1);
	fseek(fp, 0, SEEK_SET);
	fread(n_->str, 1, size, fp);
	n_->len = size;
	return size;
}
size_t smn_readfile(s_memory_node *n_, FILE *fp, size_t len_, int isappend)
{
	size_t bytes;
	if (isappend){
		_check_length(n_, n_->len + len_);
		bytes = fread(n_->str + n_->len, 1, len_, fp);
		n_->len += bytes;
	}
	else {
		_check_length(n_, len_);
		bytes = fread(n_->str, 1, len_, fp);
		n_->len = bytes;
	}
	n_->str[n_->len] = 0;
	return bytes;
}
int smn_cmp(s_memory_node *n_, const char *in_)
{
	return !strcmp(n_->str, in_);
}

void smn_cat_char(s_memory_node *n_, const char in_) {
	_check_length(n_, n_->len + 1);
	n_->str[n_->len] = in_;
	n_->len += 1;
	n_->str[n_->len] = 0;
};
void smn_cat_int(s_memory_node *n_, int in_) {
	_check_length(n_, n_->len + sizeof(int));
	memmove(n_->str + n_->len, &in_, sizeof(int));
	n_->len += sizeof(int);
	n_->str[n_->len] = 0;
};

