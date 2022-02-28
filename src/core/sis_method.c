
#include <sis_method.h>

/////////////////////////////////
//  s_sis_methods
/////////////////////////////////

/**
 * @brief 创建sis_methods列表
 * @param {s_sis_method} *methods_
 * @param {int} count_
 * @return {*}
 */
s_sis_methods *sis_methods_create(s_sis_method *methods_, int count_)
{
	s_sis_map_pointer *map = sis_map_pointer_create();
	for (int i = 0; i < count_; i++)
	{
		s_sis_method *c = methods_ + i;
		sis_methods_register(map, c);
	}
	return map;
}
void sis_methods_destroy(s_sis_methods *map_)
{
    sis_map_pointer_destroy(map_);
}
// 将methods_添加到HASH表map_
int sis_methods_register(s_sis_methods *map_, s_sis_method *methods_)
{
	s_sis_method *method = (s_sis_method *)sis_map_pointer_get(map_, methods_->name); 
	if (!method) 
	{
		sis_map_pointer_set(map_, methods_->name, methods_);
	}  
	return sis_map_pointer_getsize(map_);
}

s_sis_method *sis_methods_get(s_sis_methods *map_, const char *name_)
{
	s_sis_method *method = NULL;
	if (map_)
	{
		method = (s_sis_method *)sis_map_pointer_get(map_, name_);
	}
	return method;
}


//////////////////////////////////////////////
//   sis_method_map function define
///////////////////////////////////////////////
s_sis_map_pointer *sis_method_map_create(s_sis_method *methods_, int count_)
{
	s_sis_map_pointer *map = sis_map_pointer_create();
	for (int i = 0; i < count_; i++)
	{
		struct s_sis_method *c = methods_ + i;
		// s_sis_sds key = sis_sdsnew(c->access);
		// key = sis_sdscatfmt(key, ".%s", c->name);
		int o = sis_map_pointer_set(map, c->name, c);
		assert(o == SIS_DICT_OK);
	}
	return map;
}
s_sis_method *sis_method_map_find(s_sis_map_pointer *map_, const char *name_)
{
	s_sis_method *val = NULL;
	if (map_)
	{
		val = (s_sis_method *)sis_map_pointer_get(map_, name_);
	}
	return val;
}
void sis_method_map_destroy(s_sis_map_pointer *map_)
{
	sis_map_pointer_destroy(map_);
}

//////////////////////////////////////////////
//   sis_method_node function define
///////////////////////////////////////////////

s_sis_method_node *_sis_method_node_load(
	s_sis_map_pointer *map_,
	s_sis_method_node *first_,
	s_sis_method_node *father_,
	s_sis_json_node *node_)
{
	// printf("new = %p\n", node_);
	s_sis_method_node *new = (s_sis_method_node *)sis_malloc(sizeof(s_sis_method_node));
	memset(new, 0, sizeof(s_sis_method_node));

	new->method = sis_method_map_find(map_, (const char *)&node_->key[1]);
	if (!new->method)
	{
		LOG(5)("no find method %s\n", (const char *)&node_->key[1]);
		sis_free(new);
		return first_;
	}

	if (first_)
	{
		// while (first_->next) {
		// 	first_ = first_->next;
		// }
		first_->next = new;
		new->prev = first_;
	}
	if (father_)
	{
		father_->child = new;
		new->father = father_;
	}

	new->argv = sis_json_create_object();
	
	// printf("%s| %p, ^ %p,v %p < %p, >%p \n", new->method->name,
	// 		new, new->father,new->child, new->prev, new->next);

	switch (node_->type)
	{
	case SIS_JSON_INT:
	case SIS_JSON_DOUBLE:
	case SIS_JSON_STRING:
		sis_json_object_add_string(new->argv, SIS_METHOD_ARGV, node_->value, strlen(node_->value));
		break;
	case SIS_JSON_ARRAY:
	case SIS_JSON_OBJECT:
	{
		s_sis_method_node *first = NULL;
		s_sis_json_node *next = sis_json_first_node(node_);
		while (next)
		{
			if (next->key[0] == '$')
			{
				first = _sis_method_node_load(map_, first, new, next);
			}
			else
			{
				// printf("key=%s, v =%s\n", next->key, next->value);
				sis_json_object_add_node(new->argv, next->key, sis_json_clone(next, 1));
			}
			next = next->next;
		}
	}
	break;
	default:
		break;
	}

	return new;
}
s_sis_method_node *sis_method_node_first(s_sis_method_node *node_)
{
	if (!node_)
	{
		return NULL;
	}
	while (node_->prev)
	{
		node_ = node_->prev;
	}
	return node_;
}
// 起点，最外层为一个或者关系的方法列表，
s_sis_method_node *sis_method_node_create(s_sis_map_pointer *map_, s_sis_json_node *node_)
{
	if (!map_ || !node_)
	{
		return NULL;
	}

	s_sis_method_node *first = NULL;

	s_sis_json_node *next = sis_json_first_node(node_);
	while (next)
	{
		if (next->key[0] == '$')
		{
			first = _sis_method_node_load(map_, first, NULL, next);
		}
		next = next->next;
	}
	first = sis_method_node_first(first);
	// printf("+++ = %p\n", first);
	return first;
}

void sis_method_node_destroy(void *node_)
{
	// SIS_NOTUSED(other_);
	s_sis_method_node *node = (s_sis_method_node *)node_;
	while (node)
	{
		s_sis_method_node *next = NULL;
		if (node->child)
		{
			s_sis_method_node *child = sis_method_node_first(node->child);
			sis_method_node_destroy(child);
		}
		next = node->next;
		sis_json_delete_node(node->argv);
		// printf("free = %s %p\n",node->method->name, node);

		sis_free(node);
		node = next;
	}
}

s_sis_method_class *sis_method_class_create(s_sis_map_pointer *map_, s_sis_json_node *node_)
{
	s_sis_method_node *node = sis_method_node_create(map_, node_);
	if (!node)
	{
		return NULL;
	}
	s_sis_method_class *class = (s_sis_method_class *)sis_malloc(sizeof(s_sis_method_class));
	memset(class, 0, sizeof(s_sis_method_class));
	class->node = node;
	return class;
}

void sis_method_class_destroy(s_sis_method_class *class_)
{
	if(!class_)
	{
		return;
	}
	s_sis_method_class *class = (s_sis_method_class *)class_;
	if (class->free)
	{
		if (class->obj)
		{
			class->free(class->obj);
		}
		if (class->style == SIS_METHOD_CLASS_FILTER)
		{
			if (class->node->out)
			{
				class->free(class->node->out);
			}
		}
	}
	sis_method_node_destroy(class->node);
	sis_free(class);
}

void _sis_method_class_marking(s_sis_method_class *cls_, s_sis_method_node *node_)
{
	if (!node_)
	{
		return;
	}

	if (node_->method->proc)
	{
		node_->method->proc(cls_->obj, node_->argv);
		// void *out = node_->method->proc(cls_->obj, node_->argv);
		// if (out && cls_->free)
		// {
		// 	cls_->free(out);
		// }
	}
	if (node_->child)
	{
		s_sis_method_node *first = sis_method_node_first(node_->child);
		_sis_method_class_marking(cls_, first);
	}
	_sis_method_class_marking(cls_, node_->next);
}

void _sis_method_class_filter(
	s_sis_method_class *cls_,  // 方法
	void *in_,				   // 来源数据，每一层不同
	s_sis_method_node *onode_, // node和之运算的节点,每层第一个节点或者父节点
	s_sis_method_node *node_)  // 当前节点
{
	if (!node_)
	{
		return;
	}

	node_->method->proc(in_, node_->argv);

	// node_->out = node_->method->proc(in_, node_->argv);
	// if (!node_->out)
	// {
	// 	node_->out = cls_->malloc();
	// }

	// {
	// 	size_t len;
	// 	char *str = sis_json_output_zip(node_->argv, &len);
	// 	s_sis_sds sss = sis_string_list_sds((s_sis_string_list *)node_->out);
	// 	printf(" >>> %s : %s  %s\n  ", node_->method->name, sss, str);
	// 	sis_sdsfree(sss);
	// 	sis_free(str);
	// }

	if (node_->child)
	{
		s_sis_method_node *first = sis_method_node_first(node_->child);
		_sis_method_class_filter(cls_, node_->out, node_, first);
	}

	if (onode_ != node_)
	{
		if (sis_method_node_first(node_) == onode_)
		{
			cls_->merge(onode_->out, node_->out);
			if (node_->out)
			{
				cls_->free(node_->out);
			}
		}
	}
	if (node_->next)
	{
		_sis_method_class_filter(cls_, in_, sis_method_node_first(node_), node_->next);
	}
	else
	{
		if (node_->father)
		{
			s_sis_method_node *first = sis_method_node_first(node_);
			cls_->across(node_->father->out, first->out);
			if (first->out)
			{
				cls_->free(first->out);
			}
		}
	}
}

void _sis_method_class_judge(s_sis_method_class *cls_, s_sis_method_node *onode_, s_sis_method_node *node_)
{
	if (!node_)
	{
		return;
	}

	node_->ok = node_->method->proc(cls_->obj, node_->argv);

	// void *ok = node_->method->proc(cls_->obj, node_->argv);
	// node_->ok = (ok != NULL);

	// {
	// 	printf(" >>> %s : %d\n  ", node_->method->name, node_->ok);
	// }
	//  归到第一个，只要父亲为false就不再检查儿子
	if (node_->child && node_->ok)
	{
		s_sis_method_node *first = sis_method_node_first(node_->child);
		_sis_method_class_judge(cls_, node_, first);
	}

	if (onode_ != node_)
	{
		if (sis_method_node_first(node_) == onode_)
		{
			onode_->ok |= node_->ok;
			node_->ok = false;
		}
	}
	if (node_->next)
	{
		_sis_method_class_judge(cls_, sis_method_node_first(node_), node_->next);
	}
	else
	{
		if (node_->father)
		{
			s_sis_method_node *first = sis_method_node_first(node_);
			node_->father->ok &= first->ok;
			first->ok = false;
		}
	}
}

void _sis_method_class_clear(s_sis_method_class *class_)
{
	if (class_->free)
	{
		if (class_->style == SIS_METHOD_CLASS_FILTER)
		{
			if (class_->node->out)
			{
				class_->free(class_->node->out);
			}
			class_->node->out = NULL;
		}
		if (class_->style == SIS_METHOD_CLASS_JUDGE)
		{
			class_->node->ok = false;
		}
	}
}

void *sis_method_class_loop(s_sis_method_class *class_)
{
	s_sis_method_node *node = class_->node;
	while(node)
	{
		if (node->method->proc)
		{
			node->method->proc(class_->obj, node->argv);
		}		
		node = node->next;
	}
	return NULL;	
}

void *sis_method_class_execute(s_sis_method_class *class_)
{
	if (class_->style == SIS_METHOD_CLASS_MARKING)
	{
		_sis_method_class_marking(class_, class_->node);
		return class_->obj;
	}
	else if (class_->style == SIS_METHOD_CLASS_JUDGE)
	{
		_sis_method_class_judge(class_, class_->node, class_->node);
		return &class_->node->ok;
	}
	else
	{
		// 清楚所有临时变量out，对class_->out地址只设置NULL，不释放
		_sis_method_class_clear(class_);
		// 先执行
		_sis_method_class_filter(class_, class_->obj, class_->node, class_->node);
		return class_->node->out;
	}
}

#if 0

void show(s_sis_method_node *node_, int *depth)
{
	if (!node_)
	{
		return;
	}
	if (node_->child)
	{
		s_sis_method_node *first = sis_method_node_first(node_->child);
		int iii = *depth + 1;
		show(first,&iii);
	} 

	printf("%d| %p, ^ %p,v %p < %p, >%p ", *depth, 
			node_, node_->father,node_->child, node_->prev, node_->next);

	size_t len;
	char *str = sis_json_output_zip(node_->argv, &len);
	printf(" %s.%s| argv=%s", node_->method->style, node_->method->name, str);
	sis_free(str);			

	printf("\n");

	show(node_->next, depth);
}

void calc( s_sis_method_class *cls_, s_sis_method_node *node_)
{
	if (!node_) return;

	if (node_->child)
	{
		s_sis_method_node *first = sis_method_node_first(node_->child);
		calc(cls_, first);
	} 
	if (node_->method->proc)
	{
		void * out =node_->method->proc(NULL, node_->argv);
		if (out) cls_->free(out);
	}
	s_sis_sds sss = sis_string_list_sds((s_sis_string_list *)cls_->obj);
	printf(">>> %s : %s\n  ", node_->method->name, sss);
	sis_sdsfree(sss);

	calc(cls_, node_->next);
}
#if 1
void *demo_f1(void *c, s_sis_json_node *node_)
{	return SIS_METHOD_VOID_TRUE;  }
void *demo_f2(void *c, s_sis_json_node *node_)
{	return SIS_METHOD_VOID_FALSE;  }
void *demo_f3(void *c, s_sis_json_node *node_)
{	return SIS_METHOD_VOID_FALSE;  }
void *demo_f11(void *c, s_sis_json_node *node_)
{	return SIS_METHOD_VOID_FALSE;  }
void *demo_f12(void *c, s_sis_json_node *node_)
{	return SIS_METHOD_VOID_TRUE;  }
void *demo_f121(void *c, s_sis_json_node *node_)
{	return SIS_METHOD_VOID_TRUE;  }
void *demo_f122(void *c, s_sis_json_node *node_)
{	return SIS_METHOD_VOID_FALSE;  }
void *demo_f31(void *c, s_sis_json_node *node_)
{	return SIS_METHOD_VOID_TRUE;  }
#else
void *demo_f1(void *c, s_sis_json_node *node_)
{	
	s_sis_string_list *list = sis_string_list_create();
	sis_string_list_push(list,"1",1);
	sis_string_list_push(list,"2",1);
	sis_string_list_push(list,"3",1);
	sis_string_list_push(list,"4",1);
	sis_string_list_push(list,"5",1);
	sis_string_list_push(list,"6",1);
	sis_string_list_push(list,"7",1);
	sis_string_list_push(list,"9",1);
	s_sis_sds sss = sis_string_list_sds((s_sis_string_list *)c);
	printf("[ %s ] ", sss);
	sis_sdsfree(sss);
	printf("i am demo_f1\n");	return list; }
void *demo_f2(void *c, s_sis_json_node *node_)
{	
	s_sis_string_list *list = sis_string_list_create();
	sis_string_list_push(list,"7",1);
	s_sis_sds sss = sis_string_list_sds((s_sis_string_list *)c);
	printf("[ %s ] ", sss);
	sis_sdsfree(sss);
	printf("i am demo_f2\n");	return list; }
void *demo_f3(void *c, s_sis_json_node *node_)
{	
	s_sis_string_list *list = sis_string_list_create();
	sis_string_list_push(list,"8",1);
	sis_string_list_push(list,"9",1);
	s_sis_sds sss = sis_string_list_sds((s_sis_string_list *)c);
	printf("[ %s ] ", sss);
	sis_sdsfree(sss);
	printf("i am demo_f3\n");	return list; }
void *demo_f11(void *c, s_sis_json_node *node_)
{	
	s_sis_string_list *list = sis_string_list_create();
	sis_string_list_push(list,"4",1);
	sis_string_list_push(list,"5",1);
	s_sis_sds sss = sis_string_list_sds((s_sis_string_list *)c);
	printf("[ %s ] ", sss);
	sis_sdsfree(sss);
	printf("i am demo_f11\n");	return list; }
void *demo_f12(void *c, s_sis_json_node *node_)
{	
	s_sis_string_list *list = sis_string_list_create();
	sis_string_list_push(list,"1",1);
	sis_string_list_push(list,"2",1);
	sis_string_list_push(list,"3",1);
	printf("i am demo_f12\n");	return list; }
void *demo_f121(void *c, s_sis_json_node *node_)
{	
	s_sis_string_list *list = sis_string_list_create();
	sis_string_list_push(list,"1",1);
	s_sis_sds sss = sis_string_list_sds((s_sis_string_list *)c);
	printf("[ %s ] ", sss);
	sis_sdsfree(sss);
	printf("i am demo_f121\n");	return list; }
void *demo_f122(void *c, s_sis_json_node *node_)
{	
	s_sis_string_list *list = sis_string_list_create();
	sis_string_list_push(list,"3",1);
	s_sis_sds sss = sis_string_list_sds((s_sis_string_list *)c);
	printf("[ %s ] ", sss);
	sis_sdsfree(sss);
	printf("i am demo_f122\n");	return list; }
void *demo_f31(void *c, s_sis_json_node *node_)
{	
	s_sis_string_list *list = sis_string_list_create();
	sis_string_list_push(list,"9",1);
	s_sis_sds sss = sis_string_list_sds((s_sis_string_list *)c);
	printf("[ %s ] ", sss);
	sis_sdsfree(sss);
	printf("i am demo_f31\n");	return list; }
#endif
void demo_merge(void *out, void *obj)
{
	if (!out||!obj) return ;

	sis_string_list_merge(
		(s_sis_string_list *)out,
		(s_sis_string_list *)obj);

	{
		s_sis_sds sss = sis_string_list_sds((s_sis_string_list *)out);
		printf(" merge :  %s \n  ", sss);
		sis_sdsfree(sss);
	}	
	return ;
}
void demo_across(void *out, void *obj)
{
	if (!out||!obj) return ;

	sis_string_list_across(
		(s_sis_string_list *)out,
		(s_sis_string_list *)obj);
	// 求交集
	{
		s_sis_sds sss = sis_string_list_sds((s_sis_string_list *)out);
		printf(" across : %s\n  ", sss);
		sis_sdsfree(sss);
	}	
	return ;
}
void demo_free(void *val)
{
	sis_string_list_destroy((s_sis_string_list *)val);
}
void *demo_malloc()
{
	s_sis_string_list *list = sis_string_list_create();
	return list;
}
static struct s_sis_method _sis_method_table[] = {
	{"f1",     "append", demo_f1},
	{"f2",     "append", demo_f2},
	{"f3",     "append", demo_f3},
	{"f11",    "append", demo_f11},
	{"f12",    "append", demo_f12},
	{"f121",   "append", demo_f121},
	{"f122",   "append", demo_f122},
	{"f31",    "append", demo_f31},

	{"fx1",    "subscribe", NULL},
};


int main()
{
	safe_memory_start();
#if 1
	const char *command = 
			"{  \"$f1\" :  \
					{ 	\"fields\" : \"f1\" , \
						\"sets\" : { \"money\": 100 } , \
						\"$f11\": { \"fields\" : \"f11\" }, \
						\"$f12\":   \
							{ 	\"fields\" : \"f12\", \
								\"$f121\": { \"fields\" : \"f121\" }, \
								\"$f122\": { \"fields\" : \"f122\" } \
							} \
					},  \
					\"$f2\" : { \"fields\" : \"f2\" }, \
					\"$f3\" : { \"fields\" : \"f3\", \
						\"$f31\": [31,32,33], \
						\"$f31\": \"f31\", \
						\"$f31\": { \"fields\" : \"f31\" } \
					 } \
			}";
#else	
	const char *command = 
			"{  \"$f1\" :  \
					{ 	\"fields\" : \"f1\" , \
						\"sets\" : { \"money\": 100 }, \
						\"$f11\": { \"fields\" : \"f11\" } \
					},  \
					\"$f2\" : { \"fields\" : \"f2\" }, \
					\"$f3\" : { \"fields\" : \"f3\", \
						\"$f31\": { \"fields\" : \"f31\" } \
					 } \
			}";
#endif
	// const char *fn = "./select.json";
	// s_sis_json_handle *handel = sis_json_open(fn);
	s_sis_json_handle *handel = sis_json_load(command, strlen(command));
	if (!handel) return -1;

	// s_sis_json_node *node = sis_json_clone(handel->node,1);
	// sis_json_delete_node(node);

	// sis_json_close(handel);
	// safe_memory_stop();
	// return 0;

	s_sis_map_pointer *map = sis_method_map_create(_sis_method_table, 9);

	size_t len;
	char *str = sis_json_output_zip(handel->node, &len);
	printf("%s\n",str);
	sis_free(str);

	
	s_sis_method_class *class = sis_method_class_create(map,"append",handel->node);
	
	printf("%p\n",class->node);

	// int iii=1;
	// show(class->node,&iii);

	const char *src = "1,2,3,4,5,6,7,8,9,0";
	
	class->obj = sis_string_list_create();
	sis_string_list_load(class->obj, src, strlen(src),",");

	///////////////
	// class->style = SIS_METHOD_CLASS_FILTER;
	class->style = SIS_METHOD_CLASS_JUDGE;
	if (class->style == SIS_METHOD_CLASS_FILTER)
	{
		class->merge = demo_merge;
		class->across = demo_across;
		class->free = demo_free;
		class->malloc = demo_malloc;
	} else 
	{
		class->free = demo_free;
	}
	if (class->style != SIS_METHOD_CLASS_JUDGE)
	{
		s_sis_string_list *out= (s_sis_string_list *)sis_method_class_execute(class);

		s_sis_sds sss = sis_string_list_sds(out);
		printf("::: %s\n  ", sss);
		sis_sdsfree(sss);
	} else {
		bool *ok= (bool *)sis_method_class_execute(class);
		printf("::: ok = %d\n  ", *ok);
	}	


	sis_json_close(handel);

	sis_method_class_destroy(class, NULL);

	sis_method_map_destroy(map);

	safe_memory_stop();
	return 0;
}
#endif