
#include <sis_conf.h>
#include <os_str.h>
//////////////////////////////////////////////
//   inside parse function define
///////////////////////////////////////////////

static const char *skip(const char *in)
{
	while (in && *in && (unsigned char)*in <= 0x20)
	{
		in++;
	}
	if (in && *in && (unsigned char)*in == SIS_CONF_NOTE_SIGN)
	{
		while (in && *in && !strchr("\n\r", (unsigned char)*in))
		{
			in++;
		}
		in = skip(in);
	}
	return in;
}

static const char *_sis_parse_key(s_sis_conf_handle *handle_, s_sis_json_node *node_, const char *key_);
static const char *_sis_parse_value(s_sis_conf_handle *handle_, s_sis_json_node *node_, const char *value_);
static const char *_sis_parse_include(s_sis_conf_handle *handle_, s_sis_json_node *node_, const char *value_);

static const char *_sis_parse_string(s_sis_conf_handle *handle_, s_sis_json_node *node_, const char *value_)
{
	int len = 0;
	const char *ptr = value_;
	if (*ptr && *ptr == '"')
	{
		value_++;
		ptr++;
		while (*ptr && (unsigned char)*ptr > 0x20 && *ptr != SIS_CONF_NOTE_SIGN)
		{
			if (*ptr == '"')
			{
				ptr++;
				break;
			}
			ptr++;
			len++;
		}
		while (*ptr && (unsigned char)*ptr > 0x20 && *ptr != SIS_CONF_NOTE_SIGN)
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
		while (*ptr && (unsigned char)*ptr > 0x20 && *ptr != SIS_CONF_NOTE_SIGN)
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
		printf("a string is null , [%.12s] %p type[%d]\n", ptr, node_, node_->type);
		return 0;
	}

	node_->type = SIS_JSON_STRING;
	node_->value = sis_strdup(value_, len);

	return ptr;
}

static const char *_sis_parse_number(s_sis_conf_handle *handle_, s_sis_json_node *node_, const char *value_)
{
	const char *ptr = value_;
	int len = 0;

	int isfloat = 0;

	while (strchr("-.0123456789", (unsigned char)*ptr) && *ptr)
	{
		if (*ptr == '.' && (unsigned char)ptr[1] >= '0' && (unsigned char)ptr[1] <= '9')
		{
			isfloat = 1;
		}
		ptr++;
		len++;
	}

	if (!isfloat)
	{
		node_->type = SIS_JSON_INT;
	}
	else
	{
		node_->type = SIS_JSON_DOUBLE;
	}
	node_->value = sis_strdup(value_, len);
	// printf("vvv=%s\n", node_->value);
	return ptr;
}

static const char *_sis_parse_array(s_sis_conf_handle *handle_, s_sis_json_node *node_, const char *value_)
{
	if (node_->key == NULL)
	{
		handle_->error = value_;
		return 0;
	}
	node_->type = SIS_JSON_ARRAY;
	value_ = skip(value_ + 1);
	if (*value_ == ']')
	{
		return value_ + 1;
	} /* empty array. */

	struct s_sis_json_node *child = NULL;
	node_->child = child = sis_json_create_node();
	int index = 0;
	while (value_ && *value_)
	{
		value_ = skip(value_);
		if (!sis_strcase_match(SIS_CONF_INCLUDE, value_))
		{
			printf("array cannot include!\n");
			handle_->error = value_;
			return 0;
			//value_ = skip(_sis_parse_include(handle_, child, value_));
			//while(child->next) child = child->next;
		}
		else
		{
			child->key = sis_str_sprintf(16, "%d", index++);
			value_ = skip(_sis_parse_value(handle_, child, value_)); /* skip any spacing, get the value_. */
		}
		if (!value_ || !*value_)
		{
			return 0;
		}
		// printf("::::%p, %s| %s | %c\n", child, child->key, child->value, *value_);
		if (*value_ == ']') // ֻ����������˳�
		{
			return value_ + 1;
		}
		else if (*value_ == ',')
		{
			value_++;
			struct s_sis_json_node *new_node = sis_json_create_node();
			child->next = new_node;
			new_node->prev = child;
			child = new_node;
		}
	}
	return 0; /* malformed. */
}
static const char *_sis_parse_object(s_sis_conf_handle *handle_, s_sis_json_node *node_, const char *value_)
{
	// if (node_->key == NULL)
	// {
	// 	handle_->error = value_;
	// 	return 0;
	// }
	node_->type = SIS_JSON_OBJECT;
	value_ = skip(value_ + 1);
	if (*value_ == '}')
	{
		return value_ + 1;
	}
	struct s_sis_json_node *child = NULL;
	node_->child = child = sis_json_create_node();

	while (value_ && *value_)
	{
		value_ = skip(value_);
		if (!sis_strcase_match(SIS_CONF_INCLUDE, value_))
		{
			value_ = skip(_sis_parse_include(handle_, child, value_));
			while (child->next)
				child = child->next;
		}
		else
		{
			value_ = skip(_sis_parse_key(handle_, child, value_));
			value_ = skip(_sis_parse_value(handle_, child, value_));
		}
		if (!value_ || !*value_)
		{
			return 0;
		}
		// printf("::::%p, %s| %s | %.10s\n", child, child->key, child->value, value_);
		if (*value_ == '}') // ֻ����������˳�
		{
			return value_ + 1;
		}
		else if (*value_ == ',')
		{
			value_++;
			struct s_sis_json_node *new_node = sis_json_create_node();
			child->next = new_node;
			new_node->prev = child;
			child = new_node;
		}
	}
	return 0; /* malformed. */
}
static const char *_sis_parse_value(s_sis_conf_handle *handle_, s_sis_json_node *node_, const char *value_)
{
	if (!value_)
	{
		return 0;
	}
	if (*value_ == '-' || ((unsigned char)*value_ >= '0' && (unsigned char)*value_ <= '9'))
	{
		value_ = _sis_parse_number(handle_, node_, value_);
	}
	else if (*value_ == '[')
	{
		value_ = _sis_parse_array(handle_, node_, value_);
	}
	else if (*value_ == '{')
	{
		value_ = _sis_parse_object(handle_, node_, value_);
	}
	else
	{
		value_ = _sis_parse_string(handle_, node_, value_);
	}
	// printf("|=|%d| %p %p %p %p | %s : %s\n", node_->type, node_, node_->child, node_->prev, node_->next, node_->key,node_->value);
	return value_;
}

static const char *_sis_parse_include(s_sis_conf_handle *handle_, s_sis_json_node *node_, const char *value_)
{
	// ������Ĭ��Ŀ¼������򲻿���
	value_ = skip(value_ + strlen(SIS_CONF_INCLUDE));
	const char *ptr = value_;
	// printf("read include is  %.10s \n", ptr);
	int len = 0;
	while (*ptr && (unsigned char)*ptr > 0x20 && *ptr != SIS_CONF_NOTE_SIGN)
	{
		if (*ptr == ',' || *ptr == ']' || *ptr == '}')
		{
			break;
		}
		ptr++;
		len++;
	}
	char fn[SIS_PATH_LEN];
	sis_sprintf(fn , SIS_PATH_LEN, "%s%.*s", handle_->path, len, value_);
	// printf("read include is %.10s \n", ptr);
	// printf("--- path : %s \n", handle_->path);
	// printf("--- fn : %s \n", fn);
	// fn = sis_lstrcat(fn, );

	s_sis_sds buffer = sis_file_read_to_sds(fn);
	if (!buffer)
	{
		handle_->error = value_;
		return 0; // fail
	}

	struct s_sis_json_node *child = node_, *next = NULL;
	const char *sonptr = skip(buffer);

	while (sonptr && *sonptr)
	{
		sonptr = skip(sonptr);
		if (!sis_strcase_match(SIS_CONF_INCLUDE, sonptr))
		{
			sonptr = skip(_sis_parse_include(handle_, child, sonptr));
			// printf("a include is %.10s \n", sonptr);
		}
		else
		{
			sonptr = skip(_sis_parse_key(handle_, child, sonptr));
			sonptr = skip(_sis_parse_value(handle_, child, sonptr));
		}
		// printf("in::::%p, %s| %s | %.10s\n", child, child->key, child->value, sonptr);
		if (sonptr && *sonptr)
		{
			next = sis_json_create_node();
			child->next = next;
			next->prev = child;
			child = next;
		}
	}
	if (handle_->error) // ��Ҫ����Ϊ0������error��ֵ
	{
		int len = 0;
		handle_->error = sis_str_getline(handle_->error, &len, buffer, sis_sdslen(buffer));
		sis_out_log(3)("parse conf [%s] fail : \n %.*s \n", fn, len, handle_->error);
		handle_->error = value_;
		sis_sdsfree(buffer);
		return 0;
	}

	sis_sdsfree(buffer);

	return ptr;
}

static const char *_sis_parse_key(s_sis_conf_handle *handle_, s_sis_json_node *node_, const char *key_)
{
	if (!key_)
	{
		return 0;
	}
	int len = 0;
	const char *ptr = key_;

	while (*ptr && (unsigned char)*ptr > 0x20 && *ptr != ':') // �ո�Ϳ����ַ���ð������
	{
		ptr++;
		len++;
	}
	if (len <= 0)
	{
		// printf("a key is null , [%.10s] type[%d]\n", ptr, node_->type);
		handle_->error = ptr;
		return 0;
	}
	node_->key = sis_strdup(key_, len);
	while (*ptr && (unsigned char)*ptr >= 0x20 && *ptr != ':') // �����ո�ð��
	{
		ptr++;
	}
	if (*ptr && *ptr != ':')
	{
		// printf("a line no find ':' [%x] %.10s\n", *ptr, key_);
		handle_->error = key_;
		return 0;
	}
	ptr++;
	// printf("|=|%d| %p %p %p %p | %s : %s\n", node_->type, node_, node_->child, node_->prev, node_->next, node_->key,node_->value);
	return ptr;
}

bool _sis_conf_parse(s_sis_conf_handle *handle_, const char *content_)
{
	if (!content_)
	{
		return false;
	}
	handle_->error = 0;

	s_sis_json_node *node = sis_json_create_node();
	handle_->node = node;
	handle_->node->type = SIS_JSON_ROOT;

	struct s_sis_json_node *child = NULL, *next = NULL;
	const char *value = skip(content_);

	while (value && *value)
	{
		if (!child)
		{
			node->child = child = sis_json_create_node();
		}
		else
		{
			next = sis_json_create_node();
			child->next = next;
			next->prev = child;
			child = next;
		}
		value = skip(value);
		if (!sis_strcase_match(SIS_CONF_INCLUDE, value))
		{
			value = skip(_sis_parse_include(handle_, child, value));
			while (child->next)
				child = child->next;
			printf("a include is %.10s \n", value);
		}
		else
		{
			value = skip(_sis_parse_key(handle_, child, value));
			value = skip(_sis_parse_value(handle_, child, value));
		}
		// while(child->next){
		// 	child = child->next;
		// }
	}

	if (handle_->error) // error��ֵ,����ʧ��
	{
		// sis_conf_delete_node(handle_->node);
		// handle_->node = NULL;
		return false;
	}
	return true;
}

//////////////////////////////////////////////
//   output main function define
///////////////////////////////////////////////

s_sis_conf_handle *sis_conf_open(const char *fn_)
{

	s_sis_conf_handle *handle = NULL;

	s_sis_sds buffer = sis_file_read_to_sds(fn_);
	if (!buffer)
	{
		goto fail;
	}
	handle = (s_sis_conf_handle *)sis_malloc(sizeof(s_sis_conf_handle));
	if (!handle)
	{
		goto fail;
	}
	memset(handle, 0, sizeof(s_sis_conf_handle));
	sis_file_getpath(fn_, handle->path, 255);

	if (!_sis_conf_parse(handle, buffer))
	{
		int len = 0;
		handle->error = sis_str_getline(handle->error, &len, buffer, sis_sdslen(buffer));
		sis_out_log(3)("parse conf [%s] fail : \n %.*s \n", fn_, len, handle->error);
		sis_conf_close(handle);
		handle = NULL;
	};
fail:
	if (buffer)
	{
		sis_sdsfree(buffer);
	}
	return handle;
}
// �������ֻ��ɾ�����е��ӽڵ㣬����ɾ���Լ���ԭ���ǽṹ��û��father�����ɾ���Լ������ܶ�father->child����Ӱ��
void _sis_conf_delete_node(s_sis_json_node *node_)
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
		s_sis_json_node *next, *node = node_->child;
		while (node)
		{
			next = node->next;
			_sis_conf_delete_node(node);
			node = next;
		}
	}
	if (node_->key)
	{
		sis_free(node_->key);
		node_->key = NULL;
	}
	if (node_->value)
	{
		sis_free(node_->value);
		node_->value = NULL;
	}
	sis_free(node_);
}

void sis_conf_close(s_sis_conf_handle *handle_)
{
	if (!handle_)
	{
		return;
	}

	_sis_conf_delete_node(handle_->node);
	sis_free(handle_);
}

s_sis_conf_handle *sis_conf_load(const char *content_, size_t len_)
{
	s_sis_conf_handle *handle = NULL;
	printf("sis_conf_load : %s\n", content_);
	const char *value = skip(content_);
	if (value && *value && *value == '{')
	{
		handle = (s_sis_conf_handle *)sis_malloc(sizeof(s_sis_conf_handle));
		memset(handle, 0, sizeof(s_sis_conf_handle));
		s_sis_json_node *node = sis_json_create_node();
		handle->node = node;
		value = skip(_sis_parse_value(handle, node, value));
	}
	size_t i;
	printf("sis_conf_load : %s \n", sis_conf_to_json_zip(handle->node, &i));

	return handle;
}

#if 0
void json_printf(s_sis_json_node *node_, int *i)
{
	if (!node_)
	{
		return;
	}
	if (node_->child)
	{
		s_sis_json_node *first = sis_json_first_node(node_);
		while (first)
		{
			int iii = *i+1;
			json_printf(first,&iii);
			first = first->next;
		}
	}
	printf("%d| %d| %p,%p,%p,%p| k=%s v=%s \n", *i, node_->type, node_, 
			node_->child, node_->prev, node_->next,
			node_->key, node_->value);
}

int main()
{
	// const char *fn = "../bin/sisdb.values.conf";
	const char *fn = "test.conf";
	s_sis_conf_handle *h = sis_conf_open(fn);
	if (!h) return -1;
	printf("====================\n");
	int iii=1;
	json_printf(h->node,&iii);
	printf("====================\n");
	printf("===|%s|\n", sis_conf_get_str(h->node, "aaa1"));
	// printf("|%s|\n", sis_conf_get_str(h->node, "bbb"));
	printf("===|%s|\n", sis_conf_get_str(h->node, "aaa.hb"));
	// printf("|%s|\n", sis_conf_get_str(h->node, "aaa.sssss"));
	// printf("|%s|\n", sis_conf_get_str(h->node, "xp.0"));
	// printf("|%s|\n", sis_conf_get_str(h->node, "xp.2"));
	// printf("|%s|\n", sis_conf_get_str(h->node, "xp.4"));
	// printf("|%s|\n", sis_conf_get_str(h->node, "ding.0.a1"));
	// printf("|%s|\n", sis_conf_get_str(h->node, "ding.1.a1"));
	// printf("|%s|\n", sis_conf_get_str(h->node, "ding.1.b1"));

	// return 0;

	size_t len = 0;
	char *str = sis_conf_to_json(h->node, &len);
	printf("[%ld]  |%s|\n", len, str);
	sis_free(str);
	sis_conf_close(h);

	printf("I %.*s in command line\n", 5,"0123456789");

	return 0;
}
#endif