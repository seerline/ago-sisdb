
#include <sts_json.h>

//////////////////////////////////////////////
//   inside parse function define
///////////////////////////////////////////////

/* jump whitespace and cr/lf */
static const char *skip(const char *in_)
{
	while (in_ && *in_ && (unsigned char)*in_ <= 0x20)
	{
		in_++;
	}
	return in_;
}

static const char *_sts_parse_value(s_sts_json_handle *handle_, s_sts_json_node *node_, const char *value_);

static const char *_sts_parse_string(s_sts_json_handle *handle_, s_sts_json_node *node_, const char *str_)
{
	if (*str_ != '\"')
	{
		handle_->error = str_;
		return 0;
	}
	int len = 0;
	const char *ptr = str_ + 1;
	while (*ptr != '\"' && *ptr)
	{
		ptr++;
		len++;
	}
	if (!*ptr)
	{
		handle_->error = str_;
		return 0;
	}
	char *out = sts_strdup(str_ + 1, len);

	ptr++;

	if (!node_->key)
	{
		node_->key = out;
		while (*ptr && *ptr != ':')
		{
			ptr++;
		}
		if (!*ptr)
		{
			handle_->error = str_;
			return 0;
		}
		ptr++;
		ptr = skip(_sts_parse_value(handle_, node_, skip(ptr)));
	}
	else
	{
		node_->value = out;
		node_->type = STS_JSON_STRING;
	}
	return ptr;
}

static const char *_sts_parse_number(s_sts_json_handle *handle_, s_sts_json_node *node_, const char *num_)
{

	const char *ptr = num_;
	int len = 0;

	int isfloat = 0;
	while (strchr("-.0123456789", (unsigned char)*ptr) && *ptr)
	{
		if (*ptr == '.' && ptr[1] >= '0' && ptr[1] <= '9')
		{
			isfloat = 1;
		}
		ptr++;
		len++;
	}

	node_->value = sts_strdup(num_, len);

	if (!isfloat)
	{
		node_->type = STS_JSON_INT;
	}
	else
	{
		node_->type = STS_JSON_DOUBLE;
	}
	return ptr;
}

/* Build an array from input text. */
static const char *_sts_parse_array(s_sts_json_handle *handle_, s_sts_json_node *node_, const char *value_)
{
	// if (node_->key == NULL)
	// {
	// 	handle_->error = value_;
	// 	return 0;
	// }
	node_->type = STS_JSON_ARRAY;
	value_ = skip(value_ + 1);
	if (*value_ == ']')
	{
		return value_ + 1;
	} /* empty array. */

	struct s_sts_json_node *child;
	child = sts_json_create_node();
	node_->child = child;
	child->father = node_;
	int index = 0;
	while (value_ && *value_)
	{
		child->key = sts_str_sprintf(16, "%d", index++);
		value_ = skip(_sts_parse_value(handle_, child, skip(value_))); /* skip any spacing, get the value_. */
		if (!value_ || !*value_)
		{
			handle_->error = value_;
			return 0;
		}
		if (*value_ == ']') // 只有这里才能退出
		{
			return value_ + 1;
		}
		else if (*value_ == ',')
		{
			value_++;
			struct s_sts_json_node *new_node = sts_json_create_node();
			child->next = new_node;
			new_node->prev = child;
			child = new_node;
		}
	}
	return 0; /* malformed. */
}
/* Build an object from the text. */
static const char *_sts_parse_object(s_sts_json_handle *handle_, s_sts_json_node *node_, const char *value_)
{
	// if (node_->key == NULL)
	// {
	// 	handle_->error = value_;
	// 	return 0;
	// }
	node_->type = STS_JSON_OBJECT;
	value_ = skip(value_ + 1);
	if (*value_ == '}')
	{
		return value_ + 1;
	} /* empty array. */

	struct s_sts_json_node *child;
	child = sts_json_create_node();
	node_->child = child;
	child->father = node_;
	while (value_ && *value_)
	{
		value_ = skip(value_);
		value_ = skip(_sts_parse_value(handle_, child, value_));
		if (!value_ || !*value_)
		{
			handle_->error = value_;
			return 0;
		}
		if (*value_ == '}') // 只有这里才能退出
		{
			return value_ + 1;
		}
		else if (*value_ == ',')
		{
			value_++;
			struct s_sts_json_node *new_node = sts_json_create_node();
			child->next = new_node;
			new_node->prev = child;
			child = new_node;
		}
	}
	return 0; /* malformed. */
}
static const char *_sts_parse_value(s_sts_json_handle *handle_, s_sts_json_node *node_, const char *value_)
{
	// printf("val: |%.10s| \n", value_);
	if (!value_)
	{
		handle_->error = value_;
		return 0;
	} /* Fail on null. */
	if (*value_ == '\"')
	{
		return _sts_parse_string(handle_, node_, value_);
	}
	if (*value_ == '-' || (*value_ >= '0' && *value_ <= '9'))
	{
		return _sts_parse_number(handle_, node_, value_);
	}
	if (*value_ == '[')
	{
		return _sts_parse_array(handle_, node_, value_);
	}
	if (*value_ == '{')
	{
		return _sts_parse_object(handle_, node_, value_);
	}
	return 0;
}

bool _sts_json_parse(s_sts_json_handle *handle_, const char *content_)
{
	if (!content_)
	{
		return false;
	}
	handle_->error = 0;

	struct s_sts_json_node *node = sts_json_create_node();

	_sts_parse_value(handle_, node, skip(content_));

	if (handle_->error)
	{
		sts_json_delete_node(node);
		return false;
	}
	handle_->node = node;
	return true;
}

//////////////////////////////////////////////
//   output main function define
///////////////////////////////////////////////

s_sts_json_handle *sts_json_open(const char *fn_)
{
	s_sts_json_handle *handle = NULL;

	s_sts_sds buffer = sts_file_read_to_sds(fn_);
	if (!buffer)
	{
		goto fail;
	}
	handle = sts_json_load(buffer, sts_sdslen(buffer));
fail:
	if (buffer)
	{
		sts_sdsfree(buffer);
	}
	return handle;
}

void sts_json_close(s_sts_json_handle *handle_)
{
	if (!handle_)
	{
		return;
	}
	sts_json_delete_node(handle_->node);
	sts_free(handle_);
}

s_sts_json_handle *sts_json_load(const char *content_, size_t len_)
{
	if (!content_ || len_ <= 0)
	{
		return NULL;
	}
	if (len_ <= 0)
	{
		len_ = strlen(content_);
	}
	if (len_ <= 0)
	{
		return NULL;
	}
	struct s_sts_json_handle *handle = (s_sts_json_handle *)sts_malloc(sizeof(s_sts_json_handle));
	if (!handle)
	{
		return NULL;
	}
	memset(handle, 0, sizeof(s_sts_json_handle));
	if (!_sts_json_parse(handle, content_))
	{
		int len = 0;
		handle->error = sts_str_getline(handle->error, &len, content_, len_);
		sts_out_error(3)("json parse fail : %.*s \n", len, handle->error);
		sts_json_close(handle);
		return NULL;
	};
	return handle;
}
//   save function define
void sts_json_save(s_sts_json_node *node_, const char *fn_)
{
	sts_file_handle fp = sts_file_open(fn_, STS_FILE_IO_WRITE | STS_FILE_IO_CREATE, 0);
	if (!fp)
	{
		sts_out_error(3)("cann't write json file [%s].\n", fn_);
		return;
	}
	size_t len;
	char *buffer = sts_json_output(node_, &len);
	sts_file_write(fp, buffer, 1, len);
	sts_file_close(fp);
	sts_free(buffer);
}

//   clone function define
s_sts_json_node *sts_json_clone(s_sts_json_node *node_, int child_)
{
	s_sts_json_node *newitem, *cptr, *nptr = 0, *newchild;
	/* Bail on bad ptr */
	if (!node_)
	{
		return 0;
	}
	/* Create new node_ */
	newitem = sts_json_create_node();
	if (!newitem)
	{
		return 0;
	}
	/* Copy over all vars */
	newitem->type = node_->type;
	if (node_->key)
	{
		newitem->key = sts_strdup(node_->key, 0);
	}
	if (node_->value)
	{
		newitem->value = sts_strdup(node_->value, 0);
	}
	/* If non-recursive, then we're done! */
	if (!child_)
	{
		return newitem;
	}
	/* Walk the ->next chain for the child. */
	cptr = node_->child;
	while (cptr)
	{
		newchild = sts_json_clone(cptr, 1);
		if (!newchild)
		{
			return newitem;
		}
		if (nptr)
		{
			nptr->next = newchild, newchild->prev = nptr;
			nptr = newchild;
		} /* If newitem->child already set, then crosswire ->prev and ->next and move on */
		else
		{
			newitem->child = newchild;
			newchild->father = newitem;
			nptr = newchild;
		} /* Set newitem->child and move to it */
		cptr = cptr->next;
	}
	return newitem;
}

//////////////////////////////////////////////
//   write function define
///////////////////////////////////////////////

s_sts_json_node *sts_json_create_array(void)
{
	s_sts_json_node *n = sts_json_create_node();
	if (n)
	{
		n->type = STS_JSON_ARRAY;
	}
	return n;
}
s_sts_json_node *sts_json_create_object(void)
{
	s_sts_json_node *n = sts_json_create_node();
	if (n)
	{
		n->type = STS_JSON_OBJECT;
	}
	return n;
}
// 从只读模式切换到写入模式需要让所有的key和value重新单独申请内存
// void _sts_json_malloc(s_sts_json_node *node_)
// {
// 	if (!node_)
// 	{
// 		return;
// 	}
// 	if (node_->child)
// 	{
// 		s_sts_json_node *first = sts_json_first_node(node_);
// 		while (first)
// 		{
// 			_sts_json_malloc(first);
// 			first = first->next;
// 		}
// 	}
// 	if (node_->key)
// 	{
// 		node_->key = sts_strdup(node_->key, 0);
// 	}
// 	if (node_->value)
// 	{
// 		node_->value = sts_strdup(node_->value, 0);
// 	}
// }
// void _sts_json_check_write(s_sts_json_handle *handle_)
// {
// 	if (handle_->readonly)
// 	{
// 		handle_->readonly = false;
// 		_sts_json_malloc(handle_->node);
// 		if (handle_->content)
// 		{
// 			sts_free(handle_->content);
// 			handle_->content = NULL;
// 		}
// 		handle_->position = 0;
// 	}
// }
void sts_json_array_add_node(s_sts_json_node *source_, s_sts_json_node *node_)
{
	// _sts_json_check_write(h_);
	struct s_sts_json_node *last = sts_json_last_node(source_);
	if (!last)
	{
		source_->child = node_;
		node_->father = source_;
	}
	else
	{
		last->next = node_;
		node_->prev = last;
	}
}
void sts_json_object_add_node(s_sts_json_node *source_, const char *key_, s_sts_json_node *node_)
{
	if (!node_)
		return;
	if (node_->key)
	{
		sts_free(node_->key);
	}
	node_->key = sts_strdup(key_, 0);
	sts_json_array_add_node(source_, node_);
}
void sts_json_object_add_int(s_sts_json_node *node_, const char *key_, long value_)
{
	// _sts_json_check_write(h_);
	s_sts_json_node *c = sts_json_create_node();
	if (c)
	{
		c->type = STS_JSON_INT;
		c->key = sts_strdup(key_, 0);
		c->value = sts_str_sprintf(21, "%ld", value_);
		sts_json_array_add_node(node_, c);
	}
}
void sts_json_object_set_int(s_sts_json_node *node_, const char *key_, long value_)
{
	// _sts_json_check_write(h_);
	s_sts_json_node *c = sts_json_cmp_child_node(node_, key_);
	if (!c)
	{
		sts_json_object_add_int(node_, key_, value_);
	}
	else
	{
		sts_free(c->value);
		c->value = sts_str_sprintf(21, "%ld", value_);
	}
}
void sts_json_object_add_uint(s_sts_json_node *node_, const char *key_, unsigned long value_)
{
	// _sts_json_check_write(h_);
	s_sts_json_node *c = sts_json_create_node();
	if (c)
	{
		c->type = STS_JSON_INT;
		c->key = sts_strdup(key_, 0);
		c->value = sts_str_sprintf(21, "%lu", value_);
		sts_json_array_add_node(node_, c);
	}
}
void sts_json_object_set_uint(s_sts_json_node *node_, const char *key_, unsigned long value_)
{
	// _sts_json_check_write(h_);
	s_sts_json_node *c = sts_json_cmp_child_node(node_, key_);
	if (!c)
	{
		sts_json_object_add_int(node_, key_, value_);
	}
	else
	{
		sts_free(c->value);
		c->value = sts_str_sprintf(21, "%lu", value_);
	}
}
void sts_json_object_add_double(s_sts_json_node *node_, const char *key_, double value_, int demical)
{
	// _sts_json_check_write(h_);
	s_sts_json_node *c = sts_json_create_node();
	if (c)
	{
		c->type = STS_JSON_DOUBLE;
		c->key = sts_strdup(key_, 0);
		c->value = sts_str_sprintf(64, "%.*f", demical, value_);
		sts_json_array_add_node(node_, c);
	}
}
void sts_json_object_set_double(s_sts_json_node *node_, const char *key_, double value_, int demical)
{
	// _sts_json_check_write(h_);
	s_sts_json_node *c = sts_json_cmp_child_node(node_, key_);
	if (!c)
	{
		sts_json_object_add_double(node_, key_, value_, demical);
	}
	else
	{
		sts_free(c->value);
		c->value = sts_str_sprintf(64, "%.*f", demical, value_);
	}
}
void sts_json_object_add_string(s_sts_json_node *node_, const char *key_, const char *value_, size_t len_)
{
	// _sts_json_check_write(h_);
	s_sts_json_node *c = sts_json_create_node();
	if (c)
	{
		c->type = STS_JSON_STRING;
		c->key = sts_strdup(key_, 0);
		c->value = sts_strdup(value_, len_);
		sts_json_array_add_node(node_, c);
	}
}
void sts_json_object_set_string(s_sts_json_node *node_, const char *key_, const char *value_, size_t len_)
{
	// _sts_json_check_write(h_);
	s_sts_json_node *c = sts_json_cmp_child_node(node_, key_);
	if (!c)
	{
		sts_json_object_add_string(node_, key_, value_, len_);
	}
	else
	{
		sts_free(c->value);
		c->value = sts_strdup(value_, len_);
	}
}
//////////////////////////////////////////////////////
void sts_json_array_add_int(s_sts_json_node *node_, long value_)
{
	char key[16];
	sts_sprintf(key, 10, "%d", sts_json_get_size(node_));
	sts_json_object_add_int(node_, key, value_);
}
void sts_json_array_set_int(s_sts_json_node *node_, int index_, long value_)
{
	char key[16];
	sts_sprintf(key, 10, "%d", index_);
	sts_json_object_set_int(node_, key, value_);
}
void sts_json_array_add_uint(s_sts_json_node *node_, unsigned long value_)
{
	char key[16];
	sts_sprintf(key, 10, "%d", sts_json_get_size(node_));
	sts_json_object_add_uint(node_, key, value_);
}
void sts_json_array_set_uint(s_sts_json_node *node_, int index_, unsigned long value_)
{
	char key[16];
	sts_sprintf(key, 10, "%d", index_);
	sts_json_object_set_uint(node_, key, value_);
}
void sts_json_array_add_double(s_sts_json_node *node_, double value_, int demical_)
{
	char key[16];
	sts_sprintf(key, 10, "%d", sts_json_get_size(node_));
	sts_json_object_add_double(node_, key, value_, demical_);
}
void sts_json_array_set_double(s_sts_json_node *node_, int index_, double value_, int demical_)
{
	char key[16];
	sts_sprintf(key, 10, "%d", index_);
	sts_json_object_set_double(node_, key, value_, demical_);
}
void sts_json_array_add_string(s_sts_json_node *node_, const char *value_, size_t len_)
{
	char key[16];
	sts_sprintf(key, 10, "%d", sts_json_get_size(node_));
	sts_json_object_add_string(node_, key, value_, len_);
}
void sts_json_array_set_string(s_sts_json_node *node_, int index_, const char *value_, size_t len_)
{
	char key[16];
	sts_sprintf(key, 10, "%d", index_);
	sts_json_object_set_string(node_, key, value_, len_);
}
//////////////////////////////////////////////
// output function & other same json files
/////////////////////////////////////////////

s_sts_json_node *sts_json_create_node(void)
{
	s_sts_json_node *node = (s_sts_json_node *)sts_malloc(sizeof(s_sts_json_node));
	memset(node, 0, sizeof(s_sts_json_node));
	return node;
}

void sts_json_delete_node(s_sts_json_node *node_)
{
	if (!node_)
	{
		return;
	}
	if (node_->prev)
	{
		node_->prev->next = node_->next;
	}
	if (node_->next)
	{
		
		node_->next->prev = node_->prev;
	}	
	if (node_->father)
	{
		// printf("-%p %p %p---\n",node_->father,node_->father->child,node_->next);
		node_->father->child = node_->next;
		if(node_->next) node_->next->father = node_->father;
	}
	if (node_->child)
	{

		s_sts_json_node *next, *node = node_->child;
		while (node)
		{
			next = node->next;
			sts_json_delete_node(node);
			node = next;
		}
	}
	if (node_->key)
	{
		sts_free(node_->key);
	}
	if (node_->value)
	{
		sts_free(node_->value);
	}
	sts_free(node_);
}

//////////////////////////////////////////////
// output inside function
/////////////////////////////////////////////

static char *_sts_json_to_value(s_sts_json_node *node_, int depth_, int fmt_);
static char *_sts_json_to_number(s_sts_json_node *node_)
{
	return sts_strdup(node_->value, 0);
}

// \n, new line，另起一行
// \t, tab，表格
// \b, word boundary，词边界
// \r, return，回车
// \f, form feed，换页
// \" 引号
// \\ 斜杠
static char *_sts_json_to_string(const char *str_)
{
	const char *ptr;
	char *ptr2, *out;
	int len = 0;
	unsigned char token;

	if (!str_)
	{
		return sts_strdup("", 0);
	}
	ptr = str_;
	while ((token = *ptr) && ++len)
	{
		if (strchr("\"\\\b\f\n\r\t", token))
		{
			len++;
		}
		else if (token < 0x20)
		{
			len += 5;
		}
		ptr++;
	}

	out = (char *)sts_malloc(len + 3);
	if (!out)
	{
		return 0;
	}

	ptr2 = out;
	ptr = str_;
	*ptr2++ = '\"';
	while (*ptr)
	{
		if ((unsigned char)*ptr >= 0x20 && *ptr != '\"' && *ptr != '\\')
		{
			*ptr2++ = *ptr++;
		}
		else
		{
			*ptr2++ = '\\';
			switch (token = *ptr++)
			{
			case '\\':
				*ptr2++ = '\\';
				break;
			case '\"':
				*ptr2++ = '\"';
				break;
			case '\b':
				*ptr2++ = 'b';
				break;
			case '\f':
				*ptr2++ = 'f';
				break;
			case '\n':
				*ptr2++ = 'n';
				break;
			case '\r':
				*ptr2++ = 'r';
				break;
			case '\t':
				*ptr2++ = 't';
				break;
			default:
				sprintf(ptr2, "u%04x", token);
				ptr2 += 5;
				break; /* escape and print */
			}
		}
	}
	*ptr2++ = '\"';
	*ptr2++ = 0;
	return out;
}

static char *_sts_json_to_array(s_sts_json_node *node_, int depth_, int fmt_)
{
	char **entries;
	char *out = 0, *ptr, *ret;
	size_t len = 5;
	s_sts_json_node *child = node_->child;
	int numentries = 0, i = 0, fail = 0;

	/* How many entries in the array? */
	while (child)
	{
		numentries++;
		child = child->next;
	}

	/* Explicitly handle numentries==0 */
	if (!numentries)
	{
		out = (char *)sts_malloc(3);
		if (out)
		{
			// if (!node_->value) memmove(out, "  ", 2); else
			memmove(out, "[]", 2);
			out[2] = 0;
		}
		return out;
	}
	/* Allocate an array to hold the values for each */
	entries = (char **)sts_malloc(numentries * sizeof(char *));
	if (!entries)
	{
		return 0;
	}
	memset(entries, 0, numentries * sizeof(char *));
	/* Retrieve all the results: */
	child = node_->child;
	while (child && !fail)
	{
		ret = _sts_json_to_value(child, depth_ + 1, fmt_);
		entries[i++] = ret;
		if (ret)
		{
			len += strlen(ret) + 2 + (fmt_ ? 1 : 0);
		}
		else
		{
			fail = 1;
		}
		child = child->next;
	}

	/* If we didn't fail, try to sts_malloc the output string */
	if (!fail)
	{
		out = (char *)sts_malloc(len);
	}
	/* If that fails, we fail. */
	if (!out)
	{
		fail = 1;
	}

	/* Handle failure. */
	if (fail)
	{
		for (i = 0; i < numentries; i++)
		{
			if (entries[i])
			{
				sts_free(entries[i]);
			}
		}
		sts_free(entries);
		return 0;
	}

	/* Compose the output array. */
	*out = '[';
	ptr = out + 1;
	*ptr = 0;
	for (i = 0; i < numentries; i++)
	{
		int el = (int)strlen(entries[i]);
		memmove(ptr, entries[i], el);
		ptr += el;
		if (i != numentries - 1)
		{
			*ptr++ = ',';
			if (fmt_)
			{
				*ptr++ = ' ';
			}
			*ptr = 0;
		}
		sts_free(entries[i]);
	}
	sts_free(entries);
	*ptr++ = ']';
	*ptr++ = 0;
	return out;
}

static char *_sts_json_to_object(s_sts_json_node *node_, int depth_, int fmt_)
{
	char **entries = 0, **names = 0;
	char *out = 0, *ptr, *ret, *str;
	size_t len = 7;
	int i = 0, j;
	s_sts_json_node *child = node_->child;
	int numentries = 0, fail = 0;
	/* Count the number of entries. */
	while (child)
	{
		numentries++;
		child = child->next;
	}
	/* Explicitly handle empty object case */

	if (!numentries)
	{
		out = (char *)sts_malloc(fmt_ ? depth_ + 3 : 3);
		if (out)
		{
			ptr = out;
			*ptr++ = '{';
			if (fmt_)
			{
				*ptr++ = '\n';
				for (i = 0; i < depth_ - 1; i++)
				{
					*ptr++ = '\t';
				}
			}
			*ptr++ = '}';
			*ptr++ = 0;
		}
		return out;
	}
	/* Allocate space for the names and the objects */
	entries = (char **)sts_malloc(numentries * sizeof(char *));
	if (!entries)
	{
		return 0;
	}
	names = (char **)sts_malloc(numentries * sizeof(char *));
	if (!names)
	{
		sts_free(entries);
		return 0;
	}
	memset(entries, 0, sizeof(char *) * numentries);
	memset(names, 0, sizeof(char *) * numentries);

	/* Collect all the results into our arrays: */
	child = node_->child;
	depth_++;
	if (fmt_)
	{
		len += depth_;
	}
	while (child)
	{
		names[i] = str = _sts_json_to_string(child->key);
		entries[i++] = ret = _sts_json_to_value(child, depth_, fmt_);
		if (str && ret)
		{
			len += strlen(ret) + strlen(str) + 2 + (fmt_ ? 2 + depth_ : 0);
		}
		else
		{
			fail = 1;
		}
		child = child->next;
	}

	/* Try to allocate the output string */
	if (!fail)
	{
		out = (char *)sts_malloc(len);
	}
	if (!out)
	{
		fail = 1;
	}

	/* Handle failure */
	if (fail)
	{
		for (i = 0; i < numentries; i++)
		{
			if (names[i])
			{
				sts_free(names[i]);
			}
			if (entries[i])
			{
				sts_free(entries[i]);
			};
		}
		sts_free(names);
		sts_free(entries);
		return 0;
	}

	/* Compose the output: */
	*out = '{';
	ptr = out + 1;
	if (fmt_)
		*ptr++ = '\n';
	*ptr = 0;
	for (i = 0; i < numentries; i++)
	{
		if (fmt_)
		{
			for (j = 0; j < depth_; j++)
			{
				*ptr++ = '\t';
			}
		}
		size_t ll = strlen(names[i]);
		memmove(ptr, names[i], ll);
		ptr += ll;
		*ptr++ = ':';
		if (fmt_)
		{
			*ptr++ = '\t';
		}
		ll = strlen(entries[i]);
		memmove(ptr, entries[i], ll);
		ptr += ll;
		if (i != numentries - 1)
		{
			*ptr++ = ',';
		}
		if (fmt_)
		{
			*ptr++ = '\n';
		}
		*ptr = 0;
		sts_free(names[i]);
		sts_free(entries[i]);
	}

	sts_free(names);
	sts_free(entries);
	if (fmt_)
	{
		for (i = 0; i < depth_ - 1; i++)
		{
			*ptr++ = '\t';
		}
	}
	*ptr++ = '}';
	*ptr++ = 0;
	return out;
}

char *_sts_json_to_value(s_sts_json_node *node_, int depth_, int fmt_)
{
	char *out = 0;
	if (!node_)
	{
		return 0;
	}
	switch (node_->type)
	{
	case STS_JSON_INT:
		out = _sts_json_to_number(node_);
		break;
	case STS_JSON_DOUBLE:
		out = _sts_json_to_number(node_);
		break;
	case STS_JSON_STRING:
		out = _sts_json_to_string(node_->value);
		break;
	case STS_JSON_ARRAY:
		out = _sts_json_to_array(node_, depth_, fmt_);
		break;
	case STS_JSON_ROOT:
	case STS_JSON_OBJECT:
		out = _sts_json_to_object(node_, depth_, fmt_);
		break;
	}
	// if (out) printf("%s\n",out);
	return out;
}

//////////////////////////////////////////////////////////////////////////
///   output json
/////////////////////////////////////////////////////////////////////////

char *sts_json_output(s_sts_json_node *node_, size_t *len_)
{
	// printf("node= %p\n",node_);
	char *ptr = _sts_json_to_value(node_, 0, 1);
	if (ptr)
	{
		*len_ = strlen(ptr);
	}
	return ptr;
}
char *sts_json_output_zip(s_sts_json_node *node_, size_t *len_)
{
	char *ptr = _sts_json_to_value(node_, 0, 0);
	if (ptr)
	{
		*len_ = strlen(ptr);
	}
	return ptr;
}
void sts_json_printf(s_sts_json_node *node_, int *i)
{
	if (!node_)
	{
		return;
	}
	if (node_->child)
	{
		s_sts_json_node *first = sts_json_first_node(node_);
		while (first)
		{
			int iii = *i + 1;
			sts_json_printf(first, &iii);
			first = first->next;
		}
	}
	// printf("%d| %d| %p,%p,%p,%p| k=%s v=%s \n", *i, node_->type, node_,
	// 		node_->child, node_->prev, node_->next,
	// 		node_->key, node_->value);
}

//////////////////////////////////////////////
//   read function define
///////////////////////////////////////////////

int64 sts_json_get_int(s_sts_json_node *root_, const char *key_, int64 defaultvalue_)
{
	s_sts_json_node *c = sts_json_find_node(root_, key_);
	if (c)
	{
		// return atoi(c->value);
		return atoll(c->value);
	}
	return defaultvalue_;
}
double sts_json_get_double(s_sts_json_node *root_, const char *key_, double defaultvalue_)
{
	s_sts_json_node *c = sts_json_find_node(root_, key_);
	if (c)
	{
		return atof(c->value);
	}
	return defaultvalue_;
}
const char *sts_json_get_str(s_sts_json_node *root_, const char *key_)
{
	s_sts_json_node *c = sts_json_find_node(root_, key_);
	if (c)
	{
		return c->value;
	}
	return NULL;
}
bool sts_json_get_bool(s_sts_json_node *root_, const char *key_, bool defaultvalue_)
{
	s_sts_json_node *c = sts_json_find_node(root_, key_);
	if (c)
	{
		if (!sts_strcasecmp(c->value, "1") || !sts_strcasecmp(c->value, "true") || !sts_strcasecmp(c->value, "yes"))
		{
			return true;
		}
		return false;
	}
	return defaultvalue_;
}
int64 sts_array_get_int(s_sts_json_node *root_, int index_, int64 defaultvalue_)
{
    char key[16];
    sts_sprintf(key, 10, "%d", index_);
	return sts_json_get_int(root_, key, defaultvalue_);
}
double sts_array_get_double(s_sts_json_node *root_, int index_, double defaultvalue_)
{
    char key[16];
    sts_sprintf(key, 10, "%d", index_);
	return sts_json_get_double(root_, key, defaultvalue_);
}
const char *sts_array_get_str(s_sts_json_node *root_, int index_)
{
    char key[16];
    sts_sprintf(key, 10, "%d", index_);
	return sts_json_get_str(root_, key);
}
bool sts_json_array_bool(s_sts_json_node *root_, int index_, bool defaultvalue_)
{
    char key[16];
    sts_sprintf(key, 10, "%d", index_);
	return sts_json_get_double(root_, key, defaultvalue_);
}

int sts_json_get_size(s_sts_json_node *node_)
{
	if (!node_||!node_->child) {
		return 0;
	}
	s_sts_json_node *c = node_->child;
	int i = 0;
	while (c)
	{
		i++, c = c->next;
	}
	return i;
}
s_sts_json_node *sts_json_first_node(s_sts_json_node *node_)
{
	s_sts_json_node *c = node_->child;
	while (c && c->prev)
	{
		c = c->prev;
	}
	return c;
}
s_sts_json_node *sts_json_next_node(s_sts_json_node *node_)
{
	return node_->next;
}
s_sts_json_node *sts_json_last_node(s_sts_json_node *node_)
{
	s_sts_json_node *c = node_->child;
	while (c && c->next)
	{
		c = c->next;
	}
	return c;
}

s_sts_json_node *sts_json_cmp_child_node(s_sts_json_node *object_, const char *key_)
{
	s_sts_json_node *c = object_->child;
	while (c && sts_strcasecmp(c->key, key_))
	{
		c = c->next;
	}
	return c;
}
s_sts_json_node *sts_json_find_node(s_sts_json_node *node_, const char *path_)
{
	if (node_ == NULL)
	{
		return NULL;
	}

	size_t len = 0;
	const char *str = sts_str_split(path_, &len, '.');
	if (str && *str && len)
	{
		char key[128];
		sts_strncpy(key, 128, path_, len);
		node_ = sts_json_cmp_child_node(node_, key);
		str++;
		return sts_json_find_node(node_, str);
	}
	return sts_json_cmp_child_node(node_, path_);
}

#if 0
void json_printf(s_sts_json_node *node_, int *i)
{
	if (!node_)
	{
		return;
	}
	if (node_->child)
	{
		s_sts_json_node *first = sts_json_first_node(node_);
		while (first)
		{
			int iii = *i+1;
			json_printf(first,&iii);
			first = first->next;
		}
	}
	printf("%d| %d| %p, ^ %p,v %p < %p, >%p| k=%s v=%s \n", *i, node_->type, 
			node_, 
			node_->father,node_->child, node_->prev, node_->next,
			node_->key, node_->value);
}
int main1()
{
	const char *fn = "./select.json";
	// const char *fn = "../conf/sts.conf";
	s_sts_json_handle *h = sts_json_open(fn);
	if (!h) return -1;
	
	int iii=1;
	json_printf(h->node,&iii);
	printf("|%s|\n", sts_json_get_str(h->node, "referzs"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "bbb"));
	printf("|%s|\n", sts_json_get_str(h->node, "algorithm.peroid"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "aaa.sssss"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "xp.0"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "xp.2"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "xp.4"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "ding.0.a1"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "ding.1.a1"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "ding.1.b1"));

	// return 0;

	size_t len = 0;
	char *str = sts_json_output(h->node, &len);
	printf("[%ld]  |%s|\n", len, str);
	sts_free(str);
	sts_json_close(h);

	printf("I %s in command line\n", "0123456\n789");

	return 0;
}
int main()
{
	//const char *command = "{\"format\":\"array\",\"count\":20,\"range\":{\"start\":-100,\"count\":1}}";
	const char *command = "{\"format\":\"array\",\"count\":20,\"range\":{\"start\":-100,\"count\":1}}";
	// const char *fn = "../conf/sts.conf";
	s_sts_json_handle *h = sts_json_load(command,strlen(command));
	if (!h) return -1;

	int iii=1;
	json_printf(h->node,&iii);

	s_sts_json_node *jone = sts_json_create_array();
	s_sts_json_node *jval = NULL;

		jval = sts_json_create_array();
		sts_json_array_add_string(jval, "111", 3);
		sts_json_array_add_string(jval, "222", 3);
		sts_json_array_add_node(jone, jval);

	sts_json_object_add_node(h->node, "values", jone);

	sts_json_delete_node(sts_json_cmp_child_node(h->node,"format"));
	sts_json_delete_node(sts_json_cmp_child_node(h->node,"count"));
	// printf("----------------\n");
	// json_printf(h->node,&iii);

	size_t len = 0;
	char *str = sts_json_output(h->node, &len);
	printf("%p [%ld]  |%s|\n", h->node, len, str);
	sts_free(str);
	sts_json_close(h);
	return 0;
}
#endif