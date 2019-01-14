
#include "sis_node.h"
///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_list_node --------------------------------//
//  操作 s_sis_list_node 列表的函数
///////////////////////////////////////////////////////////////////////////
s_sis_list_node *sis_sdsnode_create(const void *in, size_t inlen)
{
	s_sis_list_node *node = (s_sis_list_node *)sis_malloc(sizeof(*node));
	node->value = NULL;
	node->prev = NULL;
	node->next = NULL;
	if (in == NULL || inlen < 1)
	{
		return node;
	}
	s_sis_sds ptr = sis_sdsnewlen(in, inlen);
	node->value = ptr;
	return node;
}
void sis_sdsnode_destroy(s_sis_list_node *node)
{
	s_sis_list_node *next;
	while (node != NULL)
	{
		next = node->next;
		sis_sdsfree((s_sis_sds)(node->value));
		sis_free(node);
		node = next;
	}
}

s_sis_list_node *sis_sdsnode_offset_node(s_sis_list_node *node_, int offset)
{
	if (node_ == NULL || offset == 0)
	{
		return NULL;
	}
	s_sis_list_node *node = NULL;
	if (offset > 0)
	{
		node = sis_sdsnode_first_node(node_);
		while (node->next != NULL)
		{
			node = node->next;
			offset--;
			if (offset == 0)
			{
				break;
			}
		};
	}
	else
	{
		node = sis_sdsnode_last_node(node_);
		while (node->prev != NULL)
		{
			node = node->prev;
			offset++;
			if (offset == 0)
			{
				break;
			}
		};
	}
	return node;
}

s_sis_list_node *sis_sdsnode_last_node(s_sis_list_node *node_)
{
	if (node_ == NULL)
	{
		return NULL;
	}
	while (node_->next != NULL)
	{
		node_ = node_->next;
	};
	return node_;
}
s_sis_list_node *sis_sdsnode_first_node(s_sis_list_node *node_)
{
	if (node_ == NULL)
	{
		return NULL;
	}
	while (node_->prev != NULL)
	{
		node_ = node_->prev;
	};
	return node_;
}
s_sis_list_node *sis_sdsnode_next_node(s_sis_list_node *node_)
{
	return node_->next;
}
s_sis_list_node *sis_sdsnode_push_node(s_sis_list_node *node_, const void *in, size_t inlen)
{
	if (node_ == NULL)
	{
		return sis_sdsnode_create(in, inlen);
	}
	s_sis_list_node *last = sis_sdsnode_last_node(node_); //这里一定返回真
	s_sis_list_node *node = (s_sis_list_node *)sis_malloc(sizeof(s_sis_list_node));
	s_sis_sds ptr = sis_sdsnewlen(in, inlen);
	node->value = ptr;
	node->prev = last;
	node->next = NULL;
	last->next = node;
	return sis_sdsnode_first_node(node);
}
s_sis_list_node *sis_sdsnode_update(s_sis_list_node *node_, const void *in, size_t inlen)
{
	if (node_ == NULL)
	{
		return NULL;
	}
	if (node_->value == NULL)
	{
		node_->value = sis_sdsnewlen(in, inlen);
		return node_;
	}
	else
	{
		s_sis_sds ptr = sis_sdscpylen((s_sis_sds)node_->value, (const char *)in, inlen);
		node_->value = ptr;
	}
	return node_;
}
s_sis_list_node *sis_sdsnode_clone(s_sis_list_node *node_)
{
	s_sis_list_node *newnode = NULL;
	s_sis_list_node *node = node_;
	while (node != NULL)
	{
		newnode = sis_sdsnode_push_node(newnode, node->value, sis_sdslen((s_sis_sds)node->value));
		node = node->next;
	};
	return newnode;
}
int sis_sdsnode_get_size(s_sis_list_node *node_)
{
	if (node_ == NULL)
	{
		return 0;
	}
	int k = 0;
	while (node_)
	{
		if (node_->value)
		{
			k += sis_sdslen((s_sis_sds)node_->value);
		}
		node_ = node_->next;
	}
	return k;
}
int sis_sdsnode_get_count(s_sis_list_node *node_)
{
	if (node_ == NULL)
	{
		return 0;
	}
	int k = 0;
	while (node_)
	{
		if (node_->value)
		{
			k++;
		}
		node_ = node_->next;
	}
	return k;
}
///////////////////////////////////////////////////////////////////////////
//------------------------s_sis_list_node --------------------------------//
//  操作 s_stsis_ssage_node 列表的函数
///////////////////////////////////////////////////////////////////////////

s_sis_message_node *sis_message_node_create()
{
	s_sis_message_node *o = (s_sis_message_node *)sis_malloc(sizeof(*o));
	memset(o, 0, sizeof(s_sis_message_node));
	// o->links = sis_sdsnode_create(NULL,0);
	// o->nodes = sis_sdsnode_create(NULL,0);
	return o;
}

void sis_message_node_destroy(void *in_)
{
	if (in_ == NULL)
	{
		return;
	}
	s_sis_message_node *in = (s_sis_message_node *)in_;
	sis_sdsnode_destroy(in->links);
	sis_sdsnode_destroy(in->nodes);

	sis_sdsfree(in->command);
	sis_sdsfree(in->key);
	sis_sdsfree(in->argv);
	sis_sdsfree(in->address);

	sis_free(in);
}

s_sis_message_node *sis_message_node_clone(s_sis_message_node *in_)
{
	if (in_ == NULL)
	{
		return NULL;
	}
	s_sis_message_node *o = sis_message_node_create();

	if (in_->command)
	{
		o->command = sis_sdsdup(in_->command);
	}
	if (in_->key)
	{
		o->key = sis_sdsdup(in_->key);
	}
	if (in_->argv)
	{
		o->argv = sis_sdsdup(in_->argv);
	}
	if (in_->address)
	{
		o->address = sis_sdsdup(in_->address);
	}

	o->links = sis_sdsnode_clone(in_->links);
	o->nodes = sis_sdsnode_clone(in_->nodes);

	return o;
}
