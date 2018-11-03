
#include "compress.h"
#include "compress.day.h"
#include "cllog.h"
#include "cltime.h"

int c_compress_base::check_indata_len(int type_, size_t inlen)
{
	switch (type_)
	{
	case SDT_NONE:
	case SDT_INFO:
	case SDT_CQ:
		return (int)inlen;
	case SDT_NOW:
	case SDT_MAP:
	case SDT_DYNA:
		return (int)(inlen / sizeof(s_stock_dynamic));
	case SDT_TICK:
		return (int)(inlen / sizeof(s_stock_tick));
	case SDT_DAY5:
		return (int)(inlen / sizeof(s_stock_day));
	case SDT_MIN:
	case SDT_MIN5:
	case SDT_MIN15:
	case SDT_MIN30:
	case SDT_MIN60:
	case SDT_MIN120:
	case SDT_DAY:
	case SDT_WEEK:
	case SDT_MON:
	case SDT_CLOSE:
		return (int)(inlen / sizeof(s_stock_day));
	default:
		return 0;
	}
	return 0;
}

s_memory_node *c_compress_base::uncompress(listNode *innode, OUT_COMPRESS_TYPE type) //
{
	//这里innode只处理单结点，并默认不为空
	s_memory_node *in = malloc_memory();
	SDS_TO_STRING(in, innode);

	cl_block_head *head;
	SDS_MAP_BLOCK_HEAD(head, innode);
	//LOG(5)("%c[%d] com=%d ,count=%d\n", head->hch, head->type, head->compress, head->count);
	if (head->hch != 'R') return in;

	s_memory_node *out = NULL;

	switch (head->type)
	{
	case CL_COMPRESS_D5:
		//返回s_stock_day
		out = cl_redis_uncompress_tick_to_day((uint8 *)in->str, in->size(), type);
		break;
	case CL_COMPRESS_TICK:
		//返回s_stock_tick xxxxx
		out = cl_redis_uncompress_tick((uint8 *)in->str, in->size(), type);
		break;
	case CL_COMPRESS_TMIN:
		out = cl_redis_uncompress_tmin((uint8 *)in->str, in->size(), type);
		break;
	case CL_COMPRESS_MIN:
	case CL_COMPRESS_M5:
		out = cl_redis_uncompress_min((uint8 *)in->str, in->size(), type);
		break;
	case CL_COMPRESS_DAY:
		out = cl_redis_uncompress_day((uint8 *)in->str, in->size(), type);
		//LOG(5)("%s[%d] ==> %p \n", in->str, in->size(), out);
		break;
	case CL_COMPRESS_DYNA:
		out = cl_redis_uncompress_dyna((uint8 *)in->str, in->size(), type);
		break;
	default:
		break;
	}
	decr_memory(in);
	return out;
}

listNode *c_compress_base::compress(int type_, uint8 *in, size_t inlen, s_stock_static *info)
{
	listNode * out = NULL;
	int count = check_indata_len(type_, inlen);
	if (count < 1) return  NULL;
	switch (type_)
	{
	case SDT_NONE:
	case SDT_NOW:
	case SDT_INFO:
	case SDT_CQ:
		return sdsnode_create(in, inlen);
		break;
	case SDT_DYNA:
		out = cl_redis_compress_dyna(in, count, info, false);
		break;
	case SDT_MAP:
		out = cl_redis_compress_dyna(in, count, info, true);
		break;
	case SDT_TICK:
		out = cl_redis_compress_tick_to_tick(in, count, info, true);
		break;
	case SDT_DAY5:
		out = cl_redis_compress_tick(in, count, info, false);
		break;
	case SDT_MIN:
		out = cl_redis_compress_tmin(in, count, info);
		break;
	case SDT_MIN5:
	case SDT_MIN15:
	case SDT_MIN30:
	case SDT_MIN60:
	case SDT_MIN120:
		out = cl_redis_compress_min(in, count, info);
		break;
	case SDT_DAY:
	case SDT_WEEK:
	case SDT_MON:
		out = cl_redis_compress_day(in, count, info);
		break;
	case SDT_CLOSE:
		out = cl_redis_compress_close(in, count, info);
		break;
	default:
		break;
	}
	return out;
};
//使用增量压缩方式，需要检查原数据包类型是否匹配，listNode *srcnode和返回值必须是同一个类,否则释放内存可能会产生泄露
listNode *c_compress_base::compress_onenew_min(listNode *srcnode, uint8 *in, int inlen, s_stock_static *info)
{
	//默认处理当日一分钟线
	int count = check_indata_len(SDT_MIN, inlen);
	if (count < 1) return srcnode;
	listNode * out = NULL;
	out = cl_redis_compress_tmin_new(srcnode, in, count, info);
	return out;
};
s_memory_node * c_compress_base::uncompress_to_mem(listNode *innode)
{
	s_memory_node * out = malloc_memory();
	int count = sdsnode_get_count(innode);
	if (count < 1) return out;

	listNode *node = innode;
	cl_block_head *head;
	out->clear();
	while (node != NULL)
	{
		SDS_MAP_BLOCK_HEAD(head, node);
		if (head->hch != 'R')  //没有压缩
		{
			out->append((sds)node->value, sdslen((sds)node->value));
		}
		else
		{
			if (head->count > 0)
			{
				s_memory_node * rtn = uncompress(node, OUT_COMPRESS_MEM);
				out->append(rtn->str, rtn->size());
				decr_memory(rtn);
			}
		}
		node = node->next;
	}
	return out;
};
s_memory_node * c_compress_base::uncompress_to_string(listNode *innode)
{
	s_memory_node * out = malloc_memory();
	int count = sdsnode_get_count(innode);
	if (count < 1) return out;

	listNode *node = innode;
	cl_block_head *head;
	int index = 0;
	bool iscompress = false;
	out->clear();
	while (node != NULL)
	{
		SDS_MAP_BLOCK_HEAD(head, node);
		//if (head->hch == 'R'&&head->compress == 1 && node->next == NULL)  //最后一个节点，
		if (head->hch != 'R')  //没有压缩
		{
			out->append((sds)(node->value), sdslen((sds)node->value));
		}
		else
		{
			if (head->count > 0)  //最后一个节点，
			{
				if (index == 0) { *out += "["; iscompress = true; }
				if (index > 0) { *out += ","; }
				s_memory_node *rtn = uncompress(node, OUT_COMPRESS_JSON);
				if (rtn->size() > 0)
				{
					out->append(rtn->str, rtn->size());
					index++;
				}
				decr_memory(rtn);
			}
		}
		node = node->next;
	}
	if (iscompress)
	{
		*out += "]";
	}
	return out;
};