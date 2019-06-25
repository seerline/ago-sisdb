
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
void sis_memory_jumpto(s_sis_memory *m_, size_t off_)
{
	if (off_ < m_->size)
	{
		m_->offset = off_;
	}
}
size_t sis_memory_get_address(s_sis_memory *m_)
{
	return m_->offset;
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

	// return sis_memory_get_size(m_);
	// 这里应该返回实际读的字符，
	return bytes;
}

size_t sis_memory_get_size(s_sis_memory *m_)
{
	return (m_->size - m_->offset);
}
void sis_memory_set_size(s_sis_memory *m_, size_t len_)
{
	// printf("set size :%zu -- %zu\n", len_, m_->offset);
	sis_memory_set_maxsize(m_, len_ + m_->offset);
	m_->size = len_ + m_->offset;
}
size_t sis_memory_get_maxsize(s_sis_memory *m_)
{
	return m_->maxsize;
}
size_t sis_memory_set_maxsize(s_sis_memory *m_, size_t len_)
{
	// printf("%zu -- %zu\n", len_, m_->maxsize);
	if (len_ > m_->maxsize)
	{
		m_->maxsize = len_ + 256;
		m_->buffer = (char *)sis_realloc(m_->buffer, m_->maxsize);
	}
	return m_->maxsize;
}

static char *_seek_new_line(char *s, size_t len) {
    int pos = 0;
    int _len = len - 1;

    /* position should be < len-1 because the character at "pos" should be
     * followed by a \n. Note that strchr cannot be used because it doesn't
     * allow to search a limited length and the buffer that is being searched
     * might not have a trailing NULL character. */
    while (pos < _len) {
        while(pos < _len && s[pos] != '\r') pos++;
        if (pos==_len) {
            /* Not found. */
            return NULL;
        } else {
            if (s[pos + 1] == '\n') {
                /* found. */
                return s + pos;
            } else {
                /* continue searching. */
                pos++;
            }
        }
    }
    return NULL;
}
char *sis_memory_read_line(s_sis_memory *m_, size_t *len_) 
{
    char *p, *s;
    size_t len;

    p = sis_memory(m_);
    s = _seek_new_line(p, sis_memory_get_size(m_));
    if (s != NULL) 
	{
        len = s - sis_memory(m_);
        if (len_) *len_ = len;
        return p;
    }
    return NULL;
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