
#include <sts_memory.h>

s_sts_memory *sts_memory_create()
{
	s_sts_memory *m = (s_sts_memory *)sts_malloc(sizeof(s_sts_memory));
	m->buffer = (char *)sts_malloc(STS_DB_MEMORY_SIZE);
	m->size = 0;
	m->maxsize = STS_DB_MEMORY_SIZE;
	m->offset = 0;
	return m;
} 
void sts_memory_destroy(s_sts_memory *m_)
{
	if (m_->buffer) {
		sts_free(m_->buffer);
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
void sts_memory_pack(s_sts_memory *m_)
{
	if (m_->offset <= 0) return;
	if (m_->offset > m_->size-1 ) {
		m_->size = 0;
		m_->offset = 0;
	} else {
		m_->size = sts_memory_get_size(m_);
		memmove(m_->buffer, m_->buffer+m_->offset, m_->size);
		m_->offset = 0;
	}
}

char * sts_memory(s_sts_memory *m_)
{
	if (!m_->buffer) return NULL;
	return m_->buffer+m_->offset;
}
size_t sts_memory_cat(s_sts_memory *m_, char *in_, size_t ilen_)
{
	if (ilen_ + m_->size > m_->maxsize) 
	{
		m_->maxsize = ilen_ + m_->size + STS_DB_MEMORY_SIZE;
		m_->buffer = (char *)sts_realloc(m_->buffer, m_->maxsize);
	} 
	memmove(m_->buffer + m_->size, in_, ilen_);
	m_->size += ilen_; 
	return sts_memory_get_size(m_);
}

size_t sts_memory_readfile(s_sts_memory *m_, sts_file_handle fp_, size_t len_)
{
	char *mem = (char *)sts_malloc(len_+1);
	size_t bytes = sts_file_read(fp_,mem,1,len_);

	if (bytes + m_->size > m_->maxsize) {
		m_->maxsize = bytes + m_->size + STS_DB_MEMORY_SIZE;
		m_->buffer = (char *)sts_realloc(m_->buffer, m_->maxsize);
	}
	memmove(m_->buffer + m_->size, mem, bytes);
	m_->size += bytes; 

	sts_free(mem);
	return sts_memory_get_size(m_);
}

size_t sts_memory_get_size(s_sts_memory *m_)
{
	return (m_->size - m_->offset);
}
void sts_memory_move(s_sts_memory *m_, size_t len_)
{
	if ((m_->offset + len_) <= m_->size)
	{
		m_->offset += len_;
	} else {
		m_->offset = m_->size;
	}
}