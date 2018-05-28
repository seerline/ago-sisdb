
#include <sts_conf.h>

//////////////////////////////////////////////
//   inside parse function define
///////////////////////////////////////////////

static const char *skip(const char *in)
{
	while (in && *in && (unsigned char)*in <= ' ')
	{
		in++;
	}
	if (in && *in && (unsigned char)*in == STS_CONF_NOTE_SIGN)
	{
		while (in && *in && !strchr("\n\r", (unsigned char)*in))
		{
			in++;
		}
		in = skip(in);
	}
	return in;
}

static const char *_sts_parse_key(s_sts_conf_handle *handle_, s_sts_json_node *node_, const char *key_);
static const char *_sts_parse_value(s_sts_conf_handle *handle_, s_sts_json_node *node_, const char *value_);
static const char *_sts_parse_include(s_sts_conf_handle *handle_, s_sts_json_node *node_, const char *value_);

static const char *_sts_parse_string(s_sts_conf_handle *handle_, s_sts_json_node *node_, const char *value_)
{
	int len = 0;
	const char *ptr = value_;
	if (*ptr && *ptr == '"')
	{
		value_++;
		ptr++;
		while (*ptr && *ptr > ' ' && *ptr != STS_CONF_NOTE_SIGN)
		{
			if (*ptr == '"')
			{
				ptr++;
				break;
			}
			ptr++;
			len++;
		}
		while (*ptr && *ptr > ' ' && *ptr != STS_CONF_NOTE_SIGN)
		{
			if (*ptr == ',' || *ptr == ']' || *ptr == '}')
			{
				break;
			}
			ptr++;
		}
	}
	else
	{
		while (*ptr && *ptr > ' ' && *ptr != STS_CONF_NOTE_SIGN)
		{
			if (*ptr == ',' || *ptr == ']' || *ptr == '}')
			{
				break;
			}
			ptr++;
			len++;
		}
	}
	if (len <= 0)
	{
		printf("a string is null , [%.10s] %p type[%d]\n", ptr, node_, node_->type);
		return 0;
	}

	node_->type = STS_JSON_STRING;
	node_->value = sts_strdup(value_, len);

	return ptr;
}

static const char *_sts_parse_number(s_sts_conf_handle *handle_, s_sts_json_node *node_, const char *value_)
{
	const char *ptr = value_;
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

	if (!isfloat)
	{
		node_->type = STS_JSON_INT;
	}
	else
	{
		node_->type = STS_JSON_DOUBLE;
	}
	node_->value = sts_strdup(value_, len);
	// printf("vvv=%s\n", node_->value);
	return ptr;
}

static const char *_sts_parse_array(s_sts_conf_handle *handle_, s_sts_json_node *node_, const char *value_)
{
	if (node_->key == NULL)
	{
		handle_->error = value_;
		return 0;
	}
	node_->type = STS_JSON_ARRAY;
	value_ = skip(value_ + 1);
	if (*value_ == ']')
	{
		return value_ + 1;
	} /* empty array. */

	struct s_sts_json_node *child = NULL;
	node_->child = child = sts_json_create_node();
	int index = 0;
	while (value_ && *value_)
	{
		value_ = skip(value_);
		if (!sts_strcase_match(STS_CONF_INCLUDE, value_))
		{
			printf("array cannot include!\n");
			handle_->error = value_;
			return 0;
			//value_ = skip(_sts_parse_include(handle_, child, value_));
			//while(child->next) child = child->next;
		}
		else
		{
			child->key = sts_str_sprintf(16, "%d", index++);
			value_ = skip(_sts_parse_value(handle_, child, value_)); /* skip any spacing, get the value_. */
		}
		if (!value_ || !*value_)
		{
			return 0;
		}
		// printf("::::%p, %s| %s | %c\n", child, child->key, child->value, *value_);
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
static const char *_sts_parse_object(s_sts_conf_handle *handle_, s_sts_json_node *node_, const char *value_)
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
	}
	struct s_sts_json_node *child = NULL;
	node_->child = child = sts_json_create_node();

	while (value_ && *value_)
	{
		value_ = skip(value_);
		if (!sts_strcase_match(STS_CONF_INCLUDE, value_))
		{
			value_ = skip(_sts_parse_include(handle_, child, value_));
			while (child->next)
				child = child->next;
		}
		else
		{
			value_ = skip(_sts_parse_key(handle_, child, value_));
			value_ = skip(_sts_parse_value(handle_, child, value_));
		}
		if (!value_ || !*value_)
		{
			return 0;
		}
		// printf("::::%p, %s| %s | %.10s\n", child, child->key, child->value, value_);
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
static const char *_sts_parse_value(s_sts_conf_handle *handle_, s_sts_json_node *node_, const char *value_)
{
	if (!value_)
	{
		return 0;
	}
	if (*value_ == '-' || (*value_ >= '0' && *value_ <= '9'))
	{
		value_ = _sts_parse_number(handle_, node_, value_);
	}
	else if (*value_ == '[')
	{
		value_ = _sts_parse_array(handle_, node_, value_);
	}
	else if (*value_ == '{')
	{
		value_ = _sts_parse_object(handle_, node_, value_);
	}
	else
	{
		value_ = _sts_parse_string(handle_, node_, value_);
	}
	// printf("|=|%d| %p %p %p %p | %s : %s\n", node_->type, node_, node_->child, node_->prev, node_->next, node_->key,node_->value);
	return value_;
}

static const char *_sts_parse_include(s_sts_conf_handle *handle_, s_sts_json_node *node_, const char *value_)
{
	// 先搜索默认目录，如果打不开就
	value_ = skip(value_ + strlen(STS_CONF_INCLUDE));
	const char *ptr = value_;
	// printf("read include is  %.10s \n", ptr);
	int len = 0;
	while (*ptr && *ptr > ' ' && *ptr != STS_CONF_NOTE_SIGN)
	{
		if (*ptr == ',' || *ptr == ']' || *ptr == '}')
		{
			break;
		}
		ptr++;
		len++;
	}
	char *fn = sts_str_sprintf(STS_PATH_LEN, "%s%.*s", handle_->path, len, value_);
	// printf("read include is %.10s \n", ptr);
	// printf("--- path : %s \n", handle_->path);
	// printf("--- fn : %s \n", fn);
	// fn = sts_lstrcat(fn, );

	size_t size = 0;
	char *buffer = sts_file_direct_read_sds(fn, &size);
	if (!buffer || size == 0)
	{
		sts_free(fn);
		handle_->error = value_;
		return 0; // fail
	}

	struct s_sts_json_node *child = node_, *next = NULL;
	const char *sonptr = skip(buffer);

	while (sonptr && *sonptr)
	{
		sonptr = skip(sonptr);
		if (!sts_strcase_match(STS_CONF_INCLUDE, sonptr))
		{
			sonptr = skip(_sts_parse_include(handle_, child, sonptr));
			// printf("a include is %.10s \n", sonptr);
		}
		else
		{
			sonptr = skip(_sts_parse_key(handle_, child, sonptr));
			sonptr = skip(_sts_parse_value(handle_, child, sonptr));
		}
		// printf("in::::%p, %s| %s | %.10s\n", child, child->key, child->value, sonptr);
		if (sonptr && *sonptr)
		{
			next = sts_json_create_node();
			child->next = next;
			next->prev = child;
			child = next;
		}
	}
	if (handle_->error) // 既要返回为0，并且error有值
	{
		int len = 0;
		handle_->error = sts_str_getline(handle_->error, &len, buffer, size);
		sts_out_error(3)("parse conf fail : %.*s \n", len, handle_->error);
		handle_->error = value_;
		sts_free(fn);
		sts_free(buffer);
		return 0;
	}

	sts_free(fn);
	sts_free(buffer);

	return ptr;
}

static const char *_sts_parse_key(s_sts_conf_handle *handle_, s_sts_json_node *node_, const char *key_)
{
	if (!key_)
	{
		return 0;
	}
	int len = 0;
	const char *ptr = key_;

	while (*ptr && *ptr > ' ' && *ptr != ':') // 空格和控制字符或冒号跳出
	{
		ptr++;
		len++;
	}
	if (len <= 0)
	{
		printf("a key is null , [%.10s] type[%d]\n", ptr, node_->type);
		handle_->error = ptr;
		return 0;
	}
	node_->key = sts_strdup(key_, len);
	while (*ptr && *ptr >= ' ' && *ptr != ':') // 跳过空格到冒号
	{
		ptr++;
	}
	if (*ptr && *ptr != ':')
	{
		printf("a line no find ':' [%x] %.10s\n", *ptr, key_);
		handle_->error = key_;
		return 0;
	}
	ptr++;
	// printf("|=|%d| %p %p %p %p | %s : %s\n", node_->type, node_, node_->child, node_->prev, node_->next, node_->key,node_->value);
	return ptr;
}

bool _sts_conf_parse(s_sts_conf_handle *handle_, const char *content_)
{
	if (!content_)
	{
		return false;
	}
	handle_->error = 0;

	s_sts_json_node *node = sts_json_create_node();
	handle_->node = node;
	handle_->node->type = STS_JSON_ROOT;

	struct s_sts_json_node *child = NULL, *next = NULL;
	const char *value = skip(content_);

	while (value && *value)
	{
		if (!child)
		{
			node->child = child = sts_json_create_node();
		}
		else
		{
			next = sts_json_create_node();
			child->next = next;
			next->prev = child;
			child = next;
		}
		value = skip(value);
		if (!sts_strcase_match(STS_CONF_INCLUDE, value))
		{
			value = skip(_sts_parse_include(handle_, child, value));
			while (child->next)
				child = child->next;
			printf("a include is %.10s \n", value);
		}
		else
		{
			value = skip(_sts_parse_key(handle_, child, value));
			value = skip(_sts_parse_value(handle_, child, value));
		}
		// while(child->next){
		// 	child = child->next;
		// }
	}

	if (handle_->error) // error有值,就是失败
	{
		// sts_conf_delete_node(handle_->node);
		// handle_->node = NULL;
		return false;
	}
	return true;
}

//////////////////////////////////////////////
//   output main function define
///////////////////////////////////////////////

s_sts_conf_handle *sts_conf_open(const char *fn_)
{

	s_sts_conf_handle *handle = NULL;

	size_t size = 0;
	char *buffer = sts_file_direct_read_sds(fn_, &size);
	if (size == 0)
	{
		goto fail;
	}
	handle = (s_sts_conf_handle *)sts_malloc(sizeof(s_sts_conf_handle));
	if (!handle)
	{
		goto fail;
	}
	memset(handle, 0, sizeof(s_sts_conf_handle));
	sts_file_getpath(fn_, handle->path, 255);

	if (!_sts_conf_parse(handle, buffer))
	{
		int len = 0;
		handle->error = sts_str_getline(handle->error, &len, buffer, size);
		sts_out_error(3)("parse conf fail : %.*s \n", len, handle->error);
		sts_conf_close(handle);
		handle = NULL;
	};
fail:
	if (buffer)
	{
		sts_free(buffer);
	}
	return handle;
}
// 这个函数只会删除所有的子节点，并不删除自己，原因是结构中没有father，如果删除自己，可能对father->child产生影响
void sts_conf_delete_node(s_sts_json_node *node_)
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
	if (node_->child)
	{
		s_sts_json_node *next, *node = node_->child;
		while (node)
		{
			next = node->next;
			sts_conf_delete_node(node);
			node = next;
		}
	}
	if (node_->key)
	{
		sts_free(node_->key);
		node_->key = NULL;
	}
	if (node_->value)
	{
		sts_free(node_->value);
		node_->value = NULL;
	}
	sts_free(node_);
}

void sts_conf_close(s_sts_conf_handle *handle_)
{
	if (!handle_)
	{
		return;
	}

	sts_conf_delete_node(handle_->node);
	sts_free(handle_);
}
// 必须为{...}格式，其他格式错误
s_sts_conf_handle *sts_conf_load(const char *content_, size_t len_)
{
	s_sts_conf_handle *handle = NULL;
	printf("sts_conf_load : %s\n", content_);
	const char *value = skip(content_);
	if (value && *value && *value == '{')
	{
		handle = (s_sts_conf_handle *)sts_malloc(sizeof(s_sts_conf_handle));
		memset(handle, 0, sizeof(s_sts_conf_handle));
		s_sts_json_node *node = sts_json_create_node();
		handle->node = node;
		value = skip(_sts_parse_value(handle, node, value));
	}
	size_t i;
	printf("sts_conf_load : %s \n", sts_conf_to_json_zip(handle->node, &i));

	return handle;
}
