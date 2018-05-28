
#include <sts_memory.h>

s_sts_memory *sts_memory_create()
{
	s_sts_memory *m = (s_sts_memory *)sts_malloc(sizeof(s_sts_memory));
	m->buffer = (char *)sts_malloc(STS_DB_MEMORY_SIZE);
	m->size = 0;
	m->maxsize = STS_DB_MEMORY_SIZE;
	m->val = m->buffer;
	return m;
} 
void sts_memory_destroy(s_sts_memory *m_)
{
	if (m->buffer) {
		sts_free(m->buffer);
	}
	sts_free(m_);
}
// void sts_memory_pack(s_sts_memory *m_)
// {
// 	if (m_->val > m_->buffer) 
// 	{
// 		m_->size = sts_memory_get_size(m_);
// 		memmove(m_->buffer, m_->val, m_->size);
// 		m_->val = m_->buffer;
// 	}
// }

size_t sts_memory_cat(s_sts_memory *m_, char *in_, size_t ilen_)
{
	if (ilen_ + m_->size > m_->maxsize) 
	{
		m_->maxsize = ilen_ + m_->size + STS_DB_MEMORY_SIZE;
		char *new = (char *)sts_malloc(m_->maxsize);
		memmove(new, m_->buffer, m_->size);
		memmove(new + m_->size, in_, ilen_);
		m_->val = new + (m_->val - m_->buffer);
		m_->size += ilen_; 
		m_->buffer = new;	
	} else {
		memmove(m_->buffer + m_->size, in_, ilen_);
		m_->size += ilen_; 
	}
}

size_t sts_memory_readfile(s_sts_memory *m_, sts_file_handle fp_, size_t len_)
{
	char *mem = (char *)sts_malloc(len_+1);
	size_t bytes = sts_file_read(fp_,mem,1,len_);

	if (bytes + m_->size > m_->maxsize) {
		m_->maxsize = bytes + m_->size + STS_DB_MEMORY_SIZE;
		char *new = (char *)sts_malloc(m_->maxsize);
		memmove(new, m_->buffer, m_->size);
		memmove(new + m_->size, mem, bytes);
		m_->val = new + (m_->val - m_->buffer);
		m_->size += bytes; 
		m_->buffer = new;	
	} else {
		memmove(m_->buffer + m_->size, mem, bytes);
		m_->size += bytes; 
	}
	sts_free(mem);
}

size_t sts_memory_get_size(s_sts_memory *m_)
{
	return (m_->size - (m_->val - m_->buffer));
}
void sts_memory_move(s_sts_memory *m_, size_t len_)
{
	if ((m_->val + len_) < m_->buffer + m_->size)
	{
		m_->val += len_;
	} else {
		m_->val = m_->buffer + m_->size;
	}
}