

#include <sts_conf.h>
#include <sts_json.h>

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
	printf("%d| %d| %p,%p,%p,%p| k=%s v=%s \n", *i, node_->type, node_, 
			node_->child, node_->prev, node_->next,
			node_->key, node_->value);
}

int main1()
{
	const char *fn = "../conf/digger.conf";
	// const char *fn = "../conf/sts.conf";
	s_sts_conf_handle *h = sts_conf_open(fn);
	if (!h) return -1;
	printf("====================\n");
	int iii=1;
	json_printf(h->node,&iii);
	printf("====================\n");
	printf("===|%s|\n", sts_conf_get_str(h->node, "aaa1"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "bbb"));
	printf("===|%s|\n", sts_conf_get_str(h->node, "aaa.hb"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "aaa.sssss"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "xp.0"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "xp.2"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "xp.4"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "ding.0.a1"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "ding.1.a1"));
	// printf("|%s|\n", sts_conf_get_str(h->node, "ding.1.b1"));

	// return 0;

	size_t len = 0;
	char *str = sts_conf_to_json(h->node, &len);
	printf("[%ld]  |%s|\n", len, str);
	sts_free(str);
	sts_conf_close(h);

	printf("I %.*s in command line\n", 5,"0123456789");

	return 0;
}

int main()
{
	const char *fn = "../conf/select.json";
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