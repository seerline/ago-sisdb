
#include "sts_node.h"
///////////////////////////////////////////////////////////////////////////
//------------------------s_sts_list_node --------------------------------//
//  操作 s_sts_list_node 列表的函数
///////////////////////////////////////////////////////////////////////////
s_sts_list_node *sts_sdsnode_create(const void *in, size_t inlen)
{
	s_sts_list_node *node = (s_sts_list_node *)sts_malloc(sizeof(*node));
	node->value = NULL;
	node->prev = NULL;
	node->next = NULL;
	if (in == NULL || inlen < 1)
	{
		return node;
	}
	s_sts_sds ptr = sts_sdsnewlen(in, inlen);
	node->value = ptr;
	return node;
}
void sts_sdsnode_destroy(s_sts_list_node *node)
{
	s_sts_list_node *next;
	while (node != NULL)
	{
		next = node->next;
		sts_sdsfree((s_sts_sds)(node->value));
		sts_free(node);
		node = next;
	}
}

s_sts_list_node *sts_sdsnode_offset_node(s_sts_list_node *node_, int offset)
{
	if (node_ == NULL || offset == 0)
	{
		return NULL;
	}
	s_sts_list_node *node = NULL;
	if (offset > 0)
	{
		node = sts_sdsnode_first_node(node_);
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
		node = sts_sdsnode_last_node(node_);
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

s_sts_list_node *sts_sdsnode_last_node(s_sts_list_node *node_)
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
s_sts_list_node *sts_sdsnode_first_node(s_sts_list_node *node_)
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
s_sts_list_node *sts_sdsnode_push_node(s_sts_list_node *node_, const void *in, size_t inlen)
{
	if (node_ == NULL)
	{
		return sts_sdsnode_create(in, inlen);
	}
	s_sts_list_node *last = sts_sdsnode_last_node(node_); //这里一定返回真
	s_sts_list_node *node = (s_sts_list_node *)sts_malloc(sizeof(s_sts_list_node));
	s_sts_sds ptr = sts_sdsnewlen(in, inlen);
	node->value = ptr;
	node->prev = last;
	node->next = NULL;
	last->next = node;
	return sts_sdsnode_first_node(node);
}
s_sts_list_node *sts_sdsnode_update(s_sts_list_node *node_, const void *in, size_t inlen)
{
	if (node_ == NULL)
	{
		return NULL;
	}
	if (node_->value == NULL)
	{
		node_->value = sts_sdsnewlen(in, inlen);
		return node_;
	}
	else
	{
		s_sts_sds ptr = sts_sdscpylen((s_sts_sds)node_->value, (const char *)in, inlen);
		node_->value = ptr;
	}
	return node_;
}
s_sts_list_node *sts_sdsnode_clone(s_sts_list_node *node_)
{
	s_sts_list_node *newnode = NULL;
	s_sts_list_node *node = node_;
	while (node != NULL)
	{
		newnode = sts_sdsnode_push_node(newnode, node->value, sts_sdslen((s_sts_sds)node->value));
		node = node->next;
	};
	return newnode;
}
int sts_sdsnode_get_size(s_sts_list_node *node_)
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
			k += sts_sdslen((s_sts_sds)node_->value);
		}
		node_ = node_->next;
	}
	return k;
}
int sts_sdsnode_get_count(s_sts_list_node *node_)
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
//------------------------s_sts_list_node --------------------------------//
//  操作 s_sts_message_node 列表的函数
///////////////////////////////////////////////////////////////////////////

s_sts_message_node *sts_message_node_create()
{
	s_sts_message_node *o = (s_sts_message_node *)sts_malloc(sizeof(*o));
	memset(o,0,sizeof(s_sts_message_node));
	// o->links = sts_sdsnode_create(NULL,0);
	// o->nodes = sts_sdsnode_create(NULL,0);
	return o;
}

void sts_message_node_destroy(void *in_) {
	if (in_==NULL) return;
	s_sts_message_node *in = (s_sts_message_node *)in_;
	sts_sdsnode_destroy(in->links);
	sts_sdsnode_destroy(in->nodes);

	sts_sdsfree(in->command);
	sts_sdsfree(in->key);
	sts_sdsfree(in->argv);
	sts_sdsfree(in->address);

	sts_free(in);
}

s_sts_message_node *sts_message_node_clone(s_sts_message_node *in_)
{
	if (in_==NULL) return NULL;
	s_sts_message_node *o = sts_message_node_create();

	if (in_->command) o->command = sts_sdsdup(in_->command);
	if (in_->key) o->key = sts_sdsdup(in_->key);
	if (in_->argv) o->argv = sts_sdsdup(in_->argv);
	if (in_->address) o->address = sts_sdsdup(in_->address);

	o->links = sts_sdsnode_clone(in_->links);
	o->nodes = sts_sdsnode_clone(in_->nodes);
	
	return o;
}
