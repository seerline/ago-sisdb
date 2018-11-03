#ifndef COMPRESS_DAY_H
#define COMPRESS_DAY_H

#include "clpub.h"
#include "compress.h"
#include "memory.h"


#ifdef __cplusplus
extern "C" {
#endif

	listNode *cl_redis_compress_day(uint8 *in, int count, s_stock_static *info);
	listNode *cl_redis_compress_close(uint8 *in, int count_, s_stock_static *info);

	listNode *cl_redis_compress_min(uint8 *in, int count, s_stock_static *info);

	listNode *cl_redis_compress_tmin(uint8 *in, int count, s_stock_static *info); //---当日分钟线数据为连续的int，需要特殊处理
	listNode *cl_redis_compress_tmin_new(listNode *src, uint8 *in, int count, s_stock_static *info);
	//这里必须传入整个实时分钟线数据，否则需要加很多判断，不划算；即使传入全部数据，也只会修改最后的数据，不会太浪费资源

	listNode *cl_redis_compress_tick(uint8 *in, int count, s_stock_static *info, bool isnow);
	listNode *cl_redis_compress_tick_to_tick(uint8 *in, int count, s_stock_static *info, bool isnow);

	//以下解压因为牵涉到多包，所以没有外层括号，[],[],[],[]
	s_memory_node *cl_redis_uncompress_day(uint8 *in, int inlen, OUT_COMPRESS_TYPE type);
	s_memory_node *cl_redis_uncompress_min(uint8 *in, int inlen, OUT_COMPRESS_TYPE type); //历史分钟线，
	s_memory_node *cl_redis_uncompress_tmin(uint8 *in, int inlen, OUT_COMPRESS_TYPE type); //当日分钟线，
	s_memory_node *cl_redis_uncompress_tick(uint8 *in, int inlen, OUT_COMPRESS_TYPE type);//5日 tick
	s_memory_node *cl_redis_uncompress_tick_to_day(uint8 *in, int inlen, OUT_COMPRESS_TYPE type);//5日 day

	listNode *cl_redis_compress_dyna(uint8 *in, int count, s_stock_static *info, bool isnow);
	s_memory_node *cl_redis_uncompress_dyna(uint8 *in, int inlen, OUT_COMPRESS_TYPE type);//5日 tick

#ifdef __cplusplus
}
#endif


#endif //_DAY_COMPRESS_H_INCLUDE
