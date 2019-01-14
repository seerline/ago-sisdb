
#include <sis_memory.h>

s_sis_memory *sis_memory_create()
{
	s_sis_memory *m = (s_sis_memory *)sis_malloc(sizeof(s_sis_memory));
	m->buffer = (char *)sis_malloc(SIS_DB_MEMORY_SIZE);
	m->size = 0;
	m->maxsize = SIS_DB_MEMORY_SIZE;
	m->offset = 0;

	return m;
}
void sis_memory_destroy(s_sis_memory *m_)
{
	if (m_->buffer)
	{
		sis_free(m_->buffer);
	}
	sis_free(m_);
}
// void sis_memory_pack(s_sis_memory *m_)
// {
// 	if (m_->val > m_->buffer)
// 	{
// 		m_->size = sis_memory_get_size(m_);
// 		memmove(m_->buffer, m_->val, m_->size);
// 		m_->val = m_->buffer;
// 	}
// }
void sis_memory_clear(s_sis_memory *m_)
{
	m_->size = 0;
	m_->offset = 0;
}
void sis_memory_pack(s_sis_memory *m_)
{
	if (m_->offset <= 0)
	{
		return;
	}
	if (m_->offset > m_->size - 1)
	{
		m_->size = 0;
		m_->offset = 0;
	}
	else
	{
		m_->size = sis_memory_get_size(m_);
		memmove(m_->buffer, m_->buffer + m_->offset, m_->size);
		m_->offset = 0;
	}
}

char *sis_memory(s_sis_memory *m_)
{
	if (!m_->buffer)
	{
		return NULL;
	}
	return m_->buffer + m_->offset;
}
size_t sis_memory_cat(s_sis_memory *m_, char *in_, size_t ilen_)
{
	if (ilen_ + m_->size > m_->maxsize)
	{
		m_->maxsize = ilen_ + m_->size + SIS_DB_MEMORY_SIZE;
		m_->buffer = (char *)sis_realloc(m_->buffer, m_->maxsize);
	}
	memmove(m_->buffer + m_->size, in_, ilen_);
	m_->size += ilen_;
	return sis_memory_get_size(m_);
}

size_t sis_memory_readfile(s_sis_memory *m_, sis_file_handle fp_, size_t len_)
{
	char *mem = (char *)sis_malloc(len_ + 1);
	size_t bytes = sis_file_read(fp_, mem, 1, len_);
	if (bytes <= 0)
	{
		sis_free(mem);
		return 0;
	}
	sis_memory_pack(m_);

	if (bytes + m_->size > m_->maxsize)
	{
		m_->maxsize = bytes + m_->size + SIS_DB_MEMORY_SIZE;
		m_->buffer = (char *)sis_realloc(m_->buffer, m_->maxsize);
	}
	memmove(m_->buffer + m_->size, mem, bytes);
	m_->size += bytes;
	// m_->buffer[m_->size] = 0;

	sis_free(mem);

	return sis_memory_get_size(m_);
}

size_t sis_memory_get_size(s_sis_memory *m_)
{
	return (m_->size - m_->offset);
}

size_t sis_memory_get_line_sign(s_sis_memory *m_)
{
	if (!m_)
	{
		return 0;
	}
	char *ptr = sis_memory(m_);
	size_t len = 0;
	while (*ptr && (unsigned char)*ptr != '\n')
	{
		if ((m_->offset + len) < m_->size - 1)
		{
			ptr++;
			len++;
		}
		else
		{
			// len = 0;
			return 0;
		}
	}
	if ((unsigned char)*ptr == '\n')
	{
		len++;
	} // skip return key
	return len;
}

void sis_memory_move(s_sis_memory *m_, size_t len_)
{
	if ((m_->offset + len_) <= m_->size)
	{
		m_->offset += len_;
	}
	else
	{
		m_->offset = m_->size;
	}
}