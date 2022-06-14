
#include <sis_conf.h>
#include <os_str.h>
//////////////////////////////////////////////
//   inside parse function define
///////////////////////////////////////////////

static const char *_sis_conf_skip(s_sis_conf_handle *handle_, const char *in)
{

	while (in && *in && (unsigned char)*in <= 0x20)
	{
		if ((unsigned char)*in == '\n')
		{
			handle_->err_lines++;
		}
		in++;
	}
	if (in && *in && (unsigned char)*in == SIS_CONF_NOTE_SIGN)
	{
		while (in && *in && !strchr("\n\r", (unsigned char)*in))
		{
			in++;
		}
		in = _sis_conf_skip(handle_,in);
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
	if (*ptr && *ptr == '\"')
	{
		value_++;
		ptr++;
		while (*ptr && (unsigned char)*ptr > 0x20 && *ptr != SIS_CONF_NOTE_SIGN)
		{
			if (*ptr == '\"')
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
		sis_strcpy(handle_->err_msg, 255, "incomplete value.\n");
		handle_->err_no = 5;
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
		handle_->err_no = 1;
		sis_strcpy(handle_->err_msg, 255, "no key.\n");
		return NULL;
	}
	node_->type = SIS_JSON_ARRAY;
	value_ = _sis_conf_skip(handle_, value_ + 1);
	if (*value_ == ']')
	{
		return value_ + 1;
	} /* empty array. */

	struct s_sis_json_node *child = NULL;
	node_->child = child = sis_json_create_node();
	child->father = node_;
	int index = 0;
	while (value_ && *value_)
	{
		value_ = _sis_conf_skip(handle_,value_);
		if (!sis_strcase_match(SIS_CONF_INCLUDE, value_))
		{
			sis_strcpy(handle_->err_msg, 255, "array cannot contain 'include'.\n");
			handle_->err_no = 2;
			return 0;
		}
		else
		{
			child->key = sis_malloc(32);
			sis_llutoa(index++, child->key, 32, 10);
			value_ = _sis_conf_skip(handle_,_sis_parse_value(handle_, child, value_)); /* skip any spacing, get the value_. */
		}
		if (!value_ || !*value_)
		{
			if (value_ && *value_ == '\n')
			{
				sis_sprintf(handle_->err_msg, 255, "array value is empty.\n");
				handle_->err_lines--;
			}
			else
			{
				sis_sprintf(handle_->err_msg, 255, "array incomplete, check ','.\n");
			}
			handle_->err_no = 2;
			return NULL;
		}
		// printf("::::%p, %s| %s | %c\n", child, child->key, child->value, *value_);
		if (*value_ == ']') 
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
	return NULL; 
}
static const char *_sis_parse_object(s_sis_conf_handle *handle_, s_sis_json_node *node_, const char *value_)
{

	node_->type = SIS_JSON_OBJECT;
	value_ = _sis_conf_skip(handle_,value_ + 1);
	if (*value_ == '}')
	{
		return value_ + 1;
	}
	struct s_sis_json_node *child = NULL;
	node_->child = child = sis_json_create_node();
	child->father = node_;
	while (value_ && *value_)
	{
		value_ = _sis_conf_skip(handle_,value_);
		if (!sis_strcase_match(SIS_CONF_INCLUDE, value_))
		{
			value_ = _sis_conf_skip(handle_,_sis_parse_include(handle_, child, value_));
			while (child->next)
			{	child = child->next;}
		}
		else
		{
			// sis_out_binary("1:",value_,16);
			value_ = _sis_conf_skip(handle_,_sis_parse_key(handle_, child, value_));
			value_ = _sis_conf_skip(handle_,_sis_parse_value(handle_, child, value_));
		}
		if (!value_ || !*value_)
		{
			if (value_ && *value_ == '\n')
			{
				sis_sprintf(handle_->err_msg, 255, "object value is empty.\n");
				handle_->err_lines--;
			}
			else
			{
				sis_sprintf(handle_->err_msg, 255, "object incomplete, check ','.\n");
			}
			// sis_strcpy(handle_->err_msg, 255, "value is empty.\n");
			// handle_->err_lines--;
			handle_->err_no = 2;
			return NULL;
		}
		// printf("::::%p, %s| %s | %.10s\n", child, child->key, child->value, value_);
		if (*value_ == '}') 
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
	return NULL; /* malformed. */
}
static const char *_sis_parse_value(s_sis_conf_handle *handle_, s_sis_json_node *node_, const char *value_)
{
	if (!value_)
	{
		return NULL;
	}
	if (*value_ == '-' || ((unsigned char)*value_ >= '0' && (unsigned char)*value_ <= '9'))
	{
		value_ = _sis_parse_number(handle_, node_, value_);
		// 跳过其他不是 数字的字符
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
	value_ = _sis_conf_skip(handle_,value_ + strlen(SIS_CONF_INCLUDE));
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
		sis_sprintf(handle_->err_msg, 255, "open include %s fail.\n", fn);
		handle_->err_no = 10;
		return 0; // fail
	}
	int old_lines = handle_->err_lines;
	handle_->err_lines = 1;
	
	struct s_sis_json_node *child = node_, *next = NULL;
	const char *sonptr = _sis_conf_skip(handle_, buffer);

	while (sonptr && *sonptr)
	{
		sonptr = _sis_conf_skip(handle_,sonptr);
		if (!sis_strcase_match(SIS_CONF_INCLUDE, sonptr))
		{
			sonptr = _sis_conf_skip(handle_,_sis_parse_include(handle_, child, sonptr));
			// printf("a include is %.10s \n", sonptr);
		}
		else
		{
			sonptr = _sis_conf_skip(handle_,_sis_parse_key(handle_, child, sonptr));
			sonptr = _sis_conf_skip(handle_,_sis_parse_value(handle_, child, sonptr));
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
	if (handle_->err_no) 
	{
		LOG(3)("[%3d]  include %s\n", old_lines, fn);
		handle_->err_no = 11;
		sis_sdsfree(buffer);
		return 0;
	}
	else
	{
		handle_->err_lines = old_lines;
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

	while (*ptr && (unsigned char)*ptr > 0x20 && *ptr != ':') 
	{
		ptr++;
		len++;
	}
	if (len <= 0)
	{
		sis_strcpy(handle_->err_msg, 255, "key is empty.\n");
		handle_->err_no = 1;
		return 0;
	}
	node_->key = sis_strdup(key_, len);
	while (*ptr && (unsigned char)*ptr >= 0x20 && *ptr != ':') 
	{
		ptr++;
	}
	if (*ptr && *ptr != ':')
	{
		if (*ptr == '\n')
		{
			sis_strcpy(handle_->err_msg, 255, "value is empty.\n");
			handle_->err_lines--;
		}
		else
		{
			sis_sprintf(handle_->err_msg, 255, "key no ':'.\n");
		}
		
		handle_->err_no = 2;
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
	handle_->err_no = 0;

	s_sis_json_node *node = sis_json_create_node();
	handle_->node = node;
	handle_->node->type = SIS_JSON_ROOT;

	struct s_sis_json_node *child = NULL, *next = NULL;
	const char *value = _sis_conf_skip(handle_,content_);

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
		value = _sis_conf_skip(handle_,value);
		if (!sis_strcase_match(SIS_CONF_INCLUDE, value))
		{
			value = _sis_conf_skip(handle_,_sis_parse_include(handle_, child, value));
			while (child->next)
			{	child = child->next;}
			// printf("a include is %.10s \n", value);
		}
		else
		{
			value = _sis_conf_skip(handle_,_sis_parse_key(handle_, child, value));
			value = _sis_conf_skip(handle_,_sis_parse_value(handle_, child, value));
		}
		// while(child->next){
		// 	child = child->next;
		// }
	}
	// printf("handle_->err_no is %d \n", handle_->err_no);
	if (handle_->err_no) 
	{
		return false;
	}
	return true;
}

///////////////////////////////////////////////
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

	handle->err_lines = 1;
	if (!_sis_conf_parse(handle, buffer))
	{

		LOG(1)("[%3d]  %s \n", handle->err_lines, handle->err_msg);
		sis_conf_close(handle);
		handle = NULL;
	};
	// size_t i;
	// printf("sis_conf_load : %s \n", sis_conf_to_json_zip(handle->node, &i));

fail:
	if (buffer)
	{
		sis_sdsfree(buffer);
	}
	return handle;
}
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
	s_sis_conf_handle *handle = SIS_MALLOC(s_sis_conf_handle, handle);
	// printf("sis_conf_load : %s\n", content_);
	char *content = (char *)sis_malloc(len_ + 1);
	memmove(content, content_, len_);
	content[len_] = 0;
	const char *value = _sis_conf_skip(handle, content);
	if (value && *value && *value == '{')
	{
		s_sis_json_node *node = sis_json_create_node();
		handle->node = node;
		value = _sis_conf_skip(handle, _sis_parse_value(handle, node, value));
	}
	sis_free(content);
	// size_t i;
	// printf("sis_conf_load : %s \n", sis_conf_to_json_zip(handle->node, &i));

	return handle;
}

s_sis_sds sis_conf_file_to_json_sds(const char *fn_)
{
	s_sis_conf_handle *handle = sis_conf_open(fn_);
	if (handle)
	{
		s_sis_sds o = NULL;
		char *str;
		size_t olen;
		str = sis_json_output_zip(handle->node, &olen);
		o = sis_sdsnewlen(str, olen);
		sis_free(str);
		sis_conf_close(handle);
		return o;
	}	
	return NULL;
}

// 寻到匹配的{}
size_t sis_conf_get_match_sign(s_sis_memory *m_)
{
	if (!m_)
	{
		return 0;
	}
	char *in = sis_memory(m_);
	size_t move = 0;
	int note = 0;
	int sign = 0;
	while ((m_->offset + move) < m_->size)
	{
		if (note)
		{
			if ((unsigned char)*in == '\n' || (unsigned char)*in == '\r')
			{
				note = 0; goto next;
			}
		}
		else
		{
			if (in && *in && (unsigned char)*in == SIS_CONF_NOTE_SIGN)
			{
				note = 1;  goto next;
			}
			if (sign == 0)
			{
				if (in && *in && (unsigned char)*in == '{')
				{
					sign = 1; goto next;
				}
			}
			else
			{
				if (in && *in && (unsigned char)*in == '{')
				{
					sign ++; goto next;
				}
				if (in && *in && (unsigned char)*in == '}')
				{
					sign --; 
					if (sign == 0)
					{
						return move + 1;
					}
					goto next;
				}
			}
		}
next:
		in++;
		move++;
	}
	return 0;
}
/**
 * @brief 从文件中读取JSON配置，并调用回调函数执行配置
 * @param fn_ 配置文件
 * @param source_ 传递给回调函数的第一个参数
 * @param cb_ 回调函数
 * @return int 成功返回0
 */
int sis_conf_sub(const char *fn_, void *source_, cb_sis_sub_json *cb_)
{
	s_sis_file_handle fp = sis_file_open(fn_, SIS_FILE_IO_READ, 0);
	if (!fp || !cb_)
	{
        LOG(5)("open conf file fail [%s].\n", fn_); 
		return -1;
	}
	sis_file_seek(fp, 0, SEEK_SET);
	s_sis_memory *mfile = sis_memory_create();
	while (1)
	{
		size_t bytes = sis_memory_readfile(mfile, fp, SIS_MEMORY_SIZE);
		if (bytes <= 0)
		{
			break;
		}
		// printf("----\n");
		size_t offset = sis_conf_get_match_sign(mfile);
		// 偏移位置包括回车字符 0 表示没有回车符号，需要继续读
		while (offset)
		{
			s_sis_conf_handle *handle = sis_conf_load(sis_memory(mfile), offset);
			// sis_out_binary("---",sis_memory(mfile), offset);
			if (handle)
			{
				cb_(source_, handle->node);
				sis_conf_close(handle);
			}
			sis_memory_move(mfile, offset);
			offset = sis_conf_get_match_sign(mfile);
		}
	}
	sis_memory_destroy(mfile);     
    sis_file_close(fp);
	return 0;
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
static int cb_json_sub(void *source, s_sis_json_node *node)
{
	size_t len = 0;
	char *str = sis_json_output_zip(node, &len);
	printf("[%zu]  |%s|\n", len, str);
	sis_free(str);
	return 0;
}
int main()
{
	// const char *fn = "../bin/sisdb.values.conf";
	const char *fn = "init.conf";
	// const char *fn = "../sisdb/test/sisa.conf";

	sis_conf_sub(fn, NULL, cb_json_sub);

}

int main1()
{
	// const char *fn = "../bin/sisdb.values.conf";
	// const char *fn = "market.conf";
	const char *fn = "../sisdb/test/test.conf";
	s_sis_conf_handle *h = sis_conf_open(fn);
	if (!h) return -1;
	printf("====================\n");
	int iii=1;
	json_printf(h->node,&iii);
	printf("====================\n");
	// printf("===|%s|\n", sis_conf_get_str(h->node, "aaa1"));
	// printf("|%s|\n", sis_conf_get_str(h->node, "bbb"));
	// printf("===|%s|\n", sis_conf_get_str(h->node, "aaa.hb"));
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
	printf("[%zu]  |%s|\n", len, str);
	sis_free(str);
	sis_conf_close(h);

	printf("I %.*s in command line\n", 5,"0123456789");

	return 0;
}
#endif