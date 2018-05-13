#ifndef COMPRESS_H
#define COMPRESS_H

#include "clpub.h"
#include "bitstream.h"
#include "clmath.h"
#include "sdsnode.h"
#include "stkdef.h"
#include "keymap.h"
#include "memory.h"

#pragma pack(push,1)

//如果数据区第一个字符不是R，就代表该数据没有被压缩，可以直接打印出来，
#define CL_COMPRESS_MAX_RECS 250     //每个块最多250条记录，251..255 预留
//*****注意*****//
//每个数据块大小不能超过 SDS_NODE_SIZE//

/*这里要注意，结构定义顺序和写入顺序是相反的*/
typedef struct cl_block_head{
	unsigned char hch; //默认R
	unsigned type : 7; //数据类型
	unsigned compress : 1; //0:正常数据 1:表示最后一个数据块
	unsigned char count;   /* 记录数，本数据块的数据个数，最大254条记录 如果==255，表示后面2位表示记录数，最多2^16条记录*/
}cl_block_head;
//cl_block_head 之后就是数据区，一般会有一个头基准和各种单位定义，后面的记录就根据头基准来推算数据；
//对于需要动态压缩的实时数据,会多出一个块来,记录数有可能为0或1
//      compress 如果为0,正常数据；
//      compress 如果为1,表示最后一个数据块，cl_block_head结构后多出来一个cl_last_info结构,表示上一数据块数据偏移位，
//				 后面一个头基准,用于作为最新数据的基准数据，如果记录数为0，后面就不跟数据了，
//				 如果记录数为1，就跟最新的数据，比如实时的分钟线，来新数据了需判断是不是新数据，如果是就需修改上一个块的数据，
//				 如果不是新增数据，那就只修改最后一个数据块的数据；
//        (采用该方案，可以把最新数据当成链表数据发给客户)这样每个块都可以独立解压
typedef struct cl_last_info{
	uint16_t offset;/* 数据块的结束偏移字节 --> 增加新记录A时，判断offset是否大于SDS_NODE_SIZE，
					如果大于，就开始下一数据块，
					如果不大，就压缩当前记录A，
					只在最后一个数据块保存该结构体*/
	uint8_t bit;       //offset*8 - bit 为实际的数据位长度
}cl_last_info;

#pragma pack(pop)


enum CL_COMPRESS_TYPE{
	CL_COMPRESS_DAY, //[day,o,h,l,n,v,m] 
	CL_COMPRESS_D5,  //[t,n,v] 
	CL_COMPRESS_TICK,//[t,n,v] v为增量 
	CL_COMPRESS_M5,  //[t,o,h,l,n,v,m] 
	CL_COMPRESS_MIN, //[t,o,h,l,n,v,m] v，m为增量 
	CL_COMPRESS_TMIN, //[i,o,h,l,n,v,m] v，m为增量 
	CL_COMPRESS_DYNA, //[id,t,o,h,l,n,v,m,....] map
	CL_COMPRESS_STATIC, //[id,code,name,preclose,....]  code
	CL_COMPRESS_CLOSE, //[day,o] 
};

enum OUT_COMPRESS_TYPE{
	OUT_COMPRESS_MEM,
	OUT_COMPRESS_JSON,
};

#define SDS_TO_STRING(s,r) if(r->value) { s->set((sds)(r->value), sdslen((sds)(r->value))); }

#define SDS_MAP_BLOCK_HEAD(h,r) if(r->value) { h=(cl_block_head *)((sds)(r->value)); }

inline int get_compress_node_count(listNode *node)
{
	if (node == NULL) return 0;
	int k = 0;
	struct listNode *n = node;
	while (n != NULL)
	{
		if (n->value)
		{
			sds s = (sds)(n->value);
			if (s[0] == 'R'){
				c_bit_stream stream((uint8*)s, sdslen(s));
				stream.MoveTo(16);
				k += stream.Get(8);
			}
		}
		n = n->next;
	}
	return k;
}
class c_compress_base
{
	//利用std::string 的缓存功能，既能返回字符串，也能返回数据缓存；
	//if (str.capacity<inlen) str.setsize(inlen,0); memcpy(str.data(),in,inlen);
private:
	int check_indata_len(int type_, size_t inlen);
	s_memory_node * uncompress(listNode *innode, OUT_COMPRESS_TYPE type);
public:
	c_compress_base(){ };
	~c_compress_base(){ };
	//使用该函数总是生成新数据包
	listNode *compress(int type_, uint8 *in, size_t inlen, s_stock_static *info);

	//listNode *make_empty(int type_);
	//使用增量压缩方式，需要检查原数据包类型是否匹配，listNode *srcnode和返回值必须是同一个类,否则释放内存可能会产生泄露
	listNode *compress_onenew_min(listNode *srcnode, uint8 *in, int inlen, s_stock_static *info);

	s_memory_node * uncompress_to_mem(listNode *innode);

	s_memory_node * uncompress_to_string(listNode *innode);

};


#endif //BASE_COMPRESS_H
