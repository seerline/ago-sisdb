
#include "compress.day.h"
#include "cllog.h"
#include "cltime.h"



/////////////////////////////////////////////////////////////////////////////////////////
//BITCODE
//	编码	 编长 码后 方法		数据 数据偏移

static BITCODE dayTimeCode[] =		//成交时间
{
	{ 0, 1, 0, 'E', 0, 0 },			// 0				= m_baseTime
	{ 0x2, 2, 4, 'B', 4, 1 },		// 10	+4Bit		= m_baseTime + 4Bit + 1 = 16
	{ 0x6, 3, 8, 'B', 8, 17 },		// 110	+8Bit		= m_baseTime + 8Bit + 17
	{ 0x7, 3, 32, 'D', 0, 0 },		// 111	+32Bit		= 32Bit Org
};
static BITCODE dayPriceDiffCode[] = //相关压缩价格
{
	{ 0, 1, 0, 'E', 0, 0 },			// 0				= 基准
	{ 0x2, 2, 4, 'b', 4, 0 },			// 10	+4Bit		= 基准+4Bit
	{ 0x6, 3, 6, 'b', 6, 8 },			// 110	+6Bit		= 基准+6Bit+8
	{ 0xE, 4, 8, 'b', 8, 40 },			// 1110	+8Bit		= 基准+8Bit+32+8
	{ 0x1E, 5, 12, 'b', 12, 168 },			// 11110+12Bit		= 基准+12Bit+128+32+8
	{ 0x1F, 5, 32, 'D', 0, 0 },			// 11111+32Bit		= 32Bit Org
};

static BITCODE dayPriceCode[] =	//非相关压缩价格
{
	{ 0xE, 4, 0, 'E', 0, 0 },			// 1110				= 基准
	{ 0x2, 2, 4, 'b', 4, 0 },			// 10	+4Bit		= 基准+4Bit
	{ 0x0, 1, 6, 'b', 6, 8 },			// 0	+6Bit		= 基准+6Bit+8
	{ 0x6, 3, 8, 'b', 8, 40 },			// 110	+8Bit		= 基准+8Bit+32+8
	{ 0x1E, 5, 12, 'b', 12, 168 },			// 11110+12Bit		= 基准+12Bit+128+32+8
	{ 0x1F, 5, 32, 'D', 0, 0 },			// 11111+32Bit		= 32Bit Org
};
static BITCODE dayVolCode[] =	//非相关压缩成交量
{
	{ 0x6, 3, 12, 'B', 12, 0 },			// 110	+12Bit		= 基准+12Bit
	{ 0x0, 1, 16, 'B', 16, 4096 },		// 0	+16Bit		= 基准+16Bit+4096
	{ 0x2, 2, 24, 'B', 24, 69632 },		// 10	+24Bit		= 基准+24Bit+65536+4096
	{ 0x7, 3, 32, 'M', 0, 0 },			// 111  +32Bit		= 32Bit Org
};

static BITCODE dayAmountCode[] =	//非相关压缩 以成交量*最新*每手股数为基准(带符号数)
{
	{ 0x6, 3, 12, 'b', 12, 0 },			// 110	+12Bit		= 基准+12Bit
	{ 0x2, 2, 16, 'b', 16, 2048 },		// 10	+16Bit		= 基准+16Bit+2048
	{ 0x0, 1, 24, 'b', 24, 34816 },		// 0	+24Bit		= 基准+24Bit+32768+2048
	{ 0x7, 3, 32, 'M', 0, 0 },			// 111	+32Bit		= 32Bit float
};
/////////////////////////////
//  Tick
///////////////////////////////
static BITCODE tickTimeCode[] =
{
	{ 0, 1, 0, 'E', 0, 0 },			//0					= 基准
	{ 2, 2, 8, 'B', 8, 1 },			//10	+8Bit		= 基准 + 8Bit +1
	{ 6, 3, 16, 'B', 16, 257 },		//110	+16Bit		= 基准 + 16Bit + 256 + 1
	{ 0xE, 4, 24, 'B', 24, 65793 },	//1110	+24Bit		= 基准 + 24Bit + 65536 + 256 + 1
	{ 0xF, 4, 32, 'D', 0, 0 },		//1111	+32Bit		= 32Bit org
};
static BITCODE tickPriceCode[] = //首记录
{
	{ 0x2, 2, 8, 'B', 8, 0 },			//10   +8Bit	= 8Bit
	{ 0x0, 1, 12, 'B', 12, 256 },		//0   +12Bit	= 12Bit+256
	{ 0x6, 3, 16, 'B', 16, 4352 },		//110 +16Bit	= 16Bit+4096+256
	{ 0x7, 3, 32, 'D', 0, 0 },			//111 +32Bit	= uint32
};

static BITCODE tickPriceDiffCode[] = //之后以前一个价格为基准
{
	{ 0, 1, 0, 'E', 0, 0 },			//0				= 基准
	{ 2, 2, 2, 'b', 2, 0 },			//10			= 基准+2Bit; todo change to 100 +1, 101 -1
	{ 0x6, 3, 4, 'b', 4, 1 },			//110	+4Bit	= 基准+4Bit+1
	{ 0xE, 4, 8, 'b', 8, 9 },			//1110  +8Bit	= 基准+8Bit+9
	{ 0x1E, 5, 16, 'b', 16, 137 },		//11110 +16Bit	= 基准+16Bit+128+9
	{ 0x1F, 5, 32, 'D', 0, 0 },			//11111 +32Bit	= uint32
};
static BITCODE tickVolDiffCode[] = //相关压缩成交量
{
	{ 0x6, 3, 4, 'Z', 2, 1 },			// 110	+2Bit+2Bit	= 基准+2Bit*10^N
	{ 0xE, 4, 6, 'Z', 4, 5 },			// 1110	+4Bit+2Bit	= 基准+(4Bit+4)*10^N
	{ 0x0, 1, 6, 'B', 6, 0 },			// 0	+6Bit		= 基准+6Bit
	{ 0x2, 2, 12, 'B', 12, 64 },		// 10	+12Bit		= 基准+12Bit+64
	{ 0x1E, 5, 16, 'B', 16, 4160 },		// 11110+16Bit		= 基准+16Bit+4096+64
	{ 0x1F, 5, 32, 'M', 0, 0 },			// 11111+32Bit		= 32Bit Org
};
////---------------------------------------------------/////
static BITCODE dynaID[] =	//stockID
{
	{ 0, 1, 0, 'E', 1, 0 },			// 0				顺序
	{ 0x2, 2, 4, 'B', 4, 0 },			// 10	+4Bit		顺序,+2+4Bit
	{ 0x3, 2, 16, 'D', 0, 0 },			// 11	+16Bit		直接存放wStockID
};

static BITCODE dynaCode[] =		//代码
{
	{ 0, 1, 0, 'E', 1, 0 },			//0					顺序
	{ 0x4, 3, 4, 'B', 4, 2 },			//100  +4Bit		顺序,+2+4Bit
	{ 0x5, 3, 8, 'B', 8, 18 },			//101  +8Bit		顺序+16+2+8Bit
	{ 0x6, 3, 32, 'D', 0, 0 },			//110 +32Bit		直接等于数字Label
	{ 0x7, 3, 0, 's', 0, 0 },			//111 +字符串		非数字Label
};
static BITCODE mmpVolCode[] =	//非相关压缩买卖盘量
{
	{ 0x1, 2, 6, 'B', 6, 0 },			// 01	+6Bit		= 6Bit
	{ 0x0, 2, 8, 'B', 8, 64 },			// 00	+8Bit		= 8Bit+64
	{ 0x6, 3, 12, 'B', 12, 320 },		// 110	+12Bit		= 12Bit+256+64
	{ 0x2, 2, 16, 'B', 16, 4416 },		// 10	+16Bit		= 16Bit+4096+256+64
	{ 0xE, 4, 24, 'B', 24, 69952 },		// 1110 +24Bit		= 24Bit+65536+4096+256+64
	{ 0xF, 4, 32, 'D', 0, 0 },			// 1111 +32Bit		= 32Bit UINT32
};
////////////////////////////////////////////////////////////////
//
// 这里是日线的压缩算法
//
///////////////////////////////////////////////////////////////
void writehead(c_bit_stream &stream, CL_COMPRESS_TYPE cltype, int count, s_stock_static *info, bool isnow, bool islast)
{
	stream.MoveTo(0);
	stream.Put('R', 8);
	if (islast) stream.Put(1, 1);
	else  stream.Put(0, 1); //备用
	stream.Put(cltype, 7);
	stream.Put(count, 8);
	///////////////////头结构/////////////////////////////////////////
	if (info->m_type_by == STK_STATIC::INDEX) stream.Put(1, 1);
	else stream.Put(0, 1);
	stream.Put(info->m_price_digit_by, 3);
	int volpower = cl_sqrt10(info->m_volunit_w);
	stream.Put(volpower, 3);
	if (isnow) stream.Put(1, 1);
	else  stream.Put(0, 1); 

}

listNode *cl_redis_compress_day(uint8 *in, int count_, s_stock_static *info)
{
	if (count_<1) return NULL;

	s_stock_day *lpCur = (s_stock_day*)in;

	uint8 out[CL_COMPRESS_MAX_RECS*sizeof(s_stock_day)];
	c_bit_stream stream(out, CL_COMPRESS_MAX_RECS*sizeof(s_stock_day));

	int nPage = (int)(count_ / CL_COMPRESS_MAX_RECS) + 1;
	cl_block_head head;
	//cl_last_info  last;

	listNode *result = NULL;
	try
	{
		int index = 0;
		int expand = cl_power10(info->m_price_digit_by);
		int outlen = 0;
		//////////////////基准值//////////////////////////////////////////
		uint32 tLastTime = 0;
		uint32 m_open_dw = 0;
		////////////////////////////////////////////////////////////
		for (int m = 0; m < nPage; m++)
		{
			stream.MoveTo(0);
			if (m == (nPage - 1)) head.count = count_ - m*CL_COMPRESS_MAX_RECS;
			else head.count = CL_COMPRESS_MAX_RECS;
			writehead(stream, CL_COMPRESS_DAY, head.count, info, false, false);
			//////////////////股票配置//////////////////////////////////////////
			tLastTime = (uint32)lpCur[index].m_time_t;
			stream.Put(tLastTime, 32);
			m_open_dw = lpCur[index].m_open_dw;
			stream.SETBITCODE(dayPriceCode);
			stream.EncodeData(m_open_dw);
			//m_open_dw = lpCur[index].m_open_dw;
			//stream.Put(m_open_dw, 32);
			//////////////////基准值//////////////////////////////////////////
			for (int k = 0; k < head.count; k++, index++)
			{
				stream.SETBITCODE(dayTimeCode);
				stream.EncodeData((uint32)lpCur[index].m_time_t, (uint32*)&tLastTime);
				tLastTime = (uint32)lpCur[index].m_time_t;

				stream.SETBITCODE(dayPriceCode);
				stream.EncodeData(lpCur[index].m_open_dw, &m_open_dw);
				m_open_dw = lpCur[index].m_open_dw;

				stream.SETBITCODE(dayPriceDiffCode);
				stream.EncodeData(lpCur[index].m_high_dw, &m_open_dw);
				stream.EncodeData(lpCur[index].m_low_dw, &m_open_dw);
				stream.EncodeData(lpCur[index].m_new_dw, &m_open_dw);

				stream.SETBITCODE(dayVolCode);
				stream.EncodeData(lpCur[index].m_volume_z);

				stream.SETBITCODE(dayAmountCode);
				zint32 mAmountBase(0, 0);
				if (info->m_type_by != STK_STATIC::INDEX)
					mAmountBase = lpCur[index].m_volume_z*(int)(info->m_volunit_w * lpCur[index].m_new_dw / expand);
				stream.EncodeData(lpCur[index].m_amount_z, &mAmountBase);

				//stream.SETBITCODE(dayInnerVolCode);
				//stream.EncodeData(lpCur[i].m_DayStock.m_dwInnerVol);
			} //k
			outlen = (stream.GetCurPos() + 7) / 8;
			//一个块压缩好了，生成sds包
			result = sdsnode_push(result, out, outlen);
		}//m
/*		last.offset = outlen;
		last.bit = (stream.GetCurPos() % 8) > 0 ? 8 - (stream.GetCurPos() % 8) : 0; //offset*8 - bit  
		//最后还需要增加基准数据块head.compress=1,block.count=0/1;
		writehead(stream, CL_COMPRESS_DAY, 0, info, false, true);
		stream.Put(last.offset, 16); 
		stream.Put(last.bit, 8);
		//回写数据长度等信息,最后一个块有这个数据,表示实际数据的偏移位置
		//////////////////股票配置//////////////////////////////////////////
		stream.Put(tLastTime, 32);
		stream.SETBITCODE(dayPriceCode);
		stream.EncodeData(m_open_dw);
		//////////////////基准值//////////////////////////////////////////
		outlen = (stream.GetCurPos() + 7) / 8;
		result = sdsnode_push(result, out, outlen);
*/
	}
	catch (...)
	{
		result = NULL;
	}
	return result;

}


s_memory_node * cl_redis_uncompress_day(uint8 *in, int inlen, OUT_COMPRESS_TYPE type)
{
	int i;
	char str[255];
	s_memory_node * result = malloc_memory();

	uint32 m_open_dw = 0;
	uint32 tLastTime = 0;
	cl_block_head head;
	//cl_last_info  last;
	c_bit_stream stream((uint8*)in, inlen);
	try
	{
		result->clear();
		head.hch=stream.Get(8);
		head.compress=stream.Get(1);
		head.type = stream.Get(7);
		head.count = stream.Get(8);
		if (head.count < 1) return result;
		///////////////////头结构/////////////////////////////////////////
		int isindex = stream.Get(1); //是不是指数
		int expand = stream.Get(3); //
		expand = cl_power10(expand);
		int vol-unit = stream.Get(3);//
		vol-unit = cl_power10(vol-unit);
		stream.Get(1); //=0 保存前收盘 =1 不保存
		if (head.compress){
			//stream.Move(sizeof(cl_last_info));
			//last.offset = 
				stream.Get(16);
			//last.bit = 
				stream.Get(8);
		}
		//////////////////股票配置//////////////////////////////////////////
		tLastTime = stream.Get(32);
		stream.SETBITCODE(dayPriceCode);
		m_open_dw = stream.DecodeData(&m_open_dw);
		//////////////////基准值//////////////////////////////////////////

		s_stock_day stkDay;
		for (i = 0; i<head.count; i++)
		{
			memset(&stkDay, 0, sizeof(s_stock_day));
			stream.SETBITCODE(dayTimeCode);
			stkDay.m_time_t = stream.DecodeData((uint32*)&tLastTime);
			tLastTime = (uint32)stkDay.m_time_t;

			stream.SETBITCODE(dayPriceCode);
			stkDay.m_open_dw = stream.DecodeData(&m_open_dw);
			m_open_dw = stkDay.m_open_dw;

			stream.SETBITCODE(dayPriceDiffCode);
			stkDay.m_high_dw = stream.DecodeData(&m_open_dw);
			stkDay.m_low_dw = stream.DecodeData(&m_open_dw);
			stkDay.m_new_dw = stream.DecodeData(&m_open_dw);

			stream.SETBITCODE(dayVolCode);
			stkDay.m_volume_z = stream.DecodeMWordData();

			stream.SETBITCODE(dayAmountCode);
			zint32 mAmountBase(0, 0);
			if (isindex == 0){
				mAmountBase = stkDay.m_volume_z*(int)(vol-unit*stkDay.m_new_dw / expand);
			}
			stkDay.m_amount_z = stream.DecodeMWordData(&mAmountBase);
			//stream.SETBITCODE(dayInnerVolCode);
			//stkDay.m_DayStock.m_dwInnerVol = stream.DecodeMWordData();
			if (type == OUT_COMPRESS_JSON)
			{
				sprintf(str, "[%d,%d,%d,%d,%d,"P64I","P64I"]", (uint32)stkDay.m_time_t, stkDay.m_open_dw, stkDay.m_high_dw,
					stkDay.m_low_dw, stkDay.m_new_dw, stkDay.m_volume_z.aslong(), stkDay.m_amount_z.aslong());
				*result += str;
				if (i < head.count - 1) *result += ",";
			}
			else
			{
				result->append((char *)&stkDay, sizeof(s_stock_day));
			}
		}
		//result += "]";
	}
	catch (...)
	{
		result->clear();
		if (type == OUT_COMPRESS_JSON){ *result = "[]"; }
	}
	return result;
}

void write_compress_count(c_bit_stream *stream, int count)
{
	if (count <= CL_COMPRESS_MAX_RECS){
		stream->Put(count, 8);
	}
	else if (count <= 0xFFFF){
		stream->Put(253, 8);
		stream->Put(count, 16);
	}
	else {
		stream->Put(254, 8);
		stream->Put(count, 32);
	}
}
int read_compress_count(c_bit_stream *stream)
{
	int count = stream->Get(8);

	if (count == 253){
		count = stream->Get(16);
	}
	else {
		count = stream->Get(32);
	}
	return count;
}
listNode *cl_redis_compress_close(uint8 *in, int count_, s_stock_static *info)
{
	if (count_<1) return NULL;
	uint8 *out = (uint8 *)zmalloc(10000 * 32);
	c_bit_stream stream(out, 10000 * 32);

	listNode *result = NULL;
	s_stock_day *lpCur = (s_stock_day*)in;
	try
	{
		int index = 0;
		int outlen = 0;
		//////////////////基准值//////////////////////////////////////////
		uint32 m_time_t = 0;
		uint32 m_new_dw = 0;
		////////////////////////////////////////////////////////////
		stream.MoveTo(0);
		stream.Put('R', 8);
		stream.Put(1, 1);
		stream.Put(CL_COMPRESS_CLOSE, 7);
		write_compress_count(&stream, count_);
		///////////////////头结构/////////////////////////////////////////
		if (info->m_type_by == STK_STATIC::INDEX) stream.Put(1, 1);
		else stream.Put(0, 1);
		stream.Put(info->m_price_digit_by, 3);
		int volpower = cl_sqrt10(info->m_volunit_w);
		stream.Put(volpower, 3);
		stream.Put(0, 1); 
		
		m_time_t = (uint32)lpCur[index].m_time_t;
		stream.Put(m_time_t, 32);
		m_new_dw = lpCur[index].m_new_dw;
		stream.SETBITCODE(dayPriceCode);
		stream.EncodeData(m_new_dw);

		for (int k = 0; k < count_; k++)
		{
			stream.SETBITCODE(dayTimeCode);
			stream.EncodeData((uint32)lpCur[index].m_time_t, (uint32*)&m_time_t);
			m_time_t = (uint32)lpCur[index].m_time_t;

			stream.SETBITCODE(dayPriceCode);
			stream.EncodeData(lpCur[index].m_new_dw, &m_new_dw);
			m_new_dw = lpCur[index].m_new_dw;
		} //k
		outlen = (stream.GetCurPos() + 7) / 8;
		result = sdsnode_create(out, outlen);
	}
	catch (...)
	{
		result = NULL;
	}
	zfree(out);
	return result;

}
////////////////////////////////////////////////////////
//  分钟线压缩 包括now
////////////////////////////////////////////////////////
#define MINSEC 60 //既然是分钟线，那就一定都要除以60
listNode *cl_redis_compress_min(uint8 *in, int count_, s_stock_static *info)
//info中类型区分指数，不能压缩计算成交金额
//isnow=0表示历史的分钟线5,15,30,60,vol为每分钟成交量;判断日期如果发生变化，第一个字段为前收盘信息
//isnow=1表示当天的分钟线5,15,30,60,vol为当日开市后累计成交量;第一个字段为原始前收盘信息
{
	if (count_<1) return NULL;
	s_stock_day *lpCur = (s_stock_day*)in;

	uint8 out[CL_COMPRESS_MAX_RECS*sizeof(s_stock_day)];
	c_bit_stream stream(out, CL_COMPRESS_MAX_RECS*sizeof(s_stock_day));

	int nPage = (int)(count_ / CL_COMPRESS_MAX_RECS) + 1;
	cl_block_head head;
	//cl_last_info  last;
	listNode *result = NULL;
	try
	{
		int index = 0;
		int expand = cl_power10(info->m_price_digit_by);
		int outlen = 0;
		//////////////////基准值//////////////////////////////////////////
		uint32 tLastTime = 0;
		uint32 m_open_dw = 0;
		////////////////////////////////////////////////////////////
		for (int m = 0; m < nPage; m++)
		{
			outlen = 0;
			stream.MoveTo(0); 
			if (m == (nPage - 1))
			{
				head.count = count_ - m*CL_COMPRESS_MAX_RECS;
			}
			else{
				head.count = CL_COMPRESS_MAX_RECS;
			}				
			writehead(stream, CL_COMPRESS_M5, head.count, info, false, false);

			//////////////////股票配置//////////////////////////////////////////
			tLastTime = (uint32)(lpCur[index].m_time_t / MINSEC);  
			stream.Put(tLastTime, 32);
			m_open_dw = lpCur[index].m_open_dw;
			stream.SETBITCODE(dayPriceCode);
			stream.EncodeData(m_open_dw);
			//////////////////基准值//////////////////////////////////////////
			for (int k = 0; k < head.count; k++, index++)
			{
				stream.SETBITCODE(tickTimeCode);
				stream.EncodeData((uint32)(lpCur[index].m_time_t / MINSEC), (uint32*)&tLastTime);
				tLastTime = (uint32)(lpCur[index].m_time_t / MINSEC);

				stream.SETBITCODE(dayPriceCode);
				stream.EncodeData(lpCur[index].m_open_dw, &m_open_dw);
				m_open_dw = lpCur[index].m_open_dw;

				stream.SETBITCODE(dayPriceDiffCode);
				stream.EncodeData(lpCur[index].m_high_dw, &m_open_dw);
				stream.EncodeData(lpCur[index].m_low_dw, &m_open_dw);
				stream.EncodeData(lpCur[index].m_new_dw, &m_open_dw);

				stream.SETBITCODE(dayVolCode);
				stream.EncodeData(lpCur[index].m_volume_z);

				stream.SETBITCODE(dayAmountCode);
				zint32 mAmountBase(0, 0);
				if (info->m_type_by != STK_STATIC::INDEX)
					mAmountBase = lpCur[index].m_volume_z*(int)(info->m_volunit_w * lpCur[index].m_new_dw / expand);
				stream.EncodeData(lpCur[index].m_amount_z, &mAmountBase);

				//stream.SETBITCODE(dayInnerVolCode);
				//stream.EncodeData(lpCur[i].m_DayStock.m_dwInnerVol);
			} //k

			outlen = (stream.GetCurPos() + 7) / 8;
			//一个块压缩好了，生成sdsnode包
			result = sdsnode_push(result, out, outlen);
		}//m
	}
	catch (...)
	{
		result = NULL;
	}
	return result;
}
s_memory_node *cl_redis_uncompress_min(uint8 *in, int inlen, OUT_COMPRESS_TYPE type)
{
	int i;
	char str[255];
	s_memory_node * result = malloc_memory();;

	uint32 open_dw = 0;
	uint32 tLastTime = 0;
	zint32  volume_z(0, 0);
	cl_block_head head;
	//cl_last_info last;
	c_bit_stream stream((uint8*)in, inlen);
	try
	{
		result->clear();
		head.hch = stream.Get(8);
		head.compress = stream.Get(1);
		head.type = stream.Get(7);
		head.count = stream.Get(8);
		if (head.count < 1) return result;
		///////////////////头结构/////////////////////////////////////////
		int isindex = stream.Get(1); //是不是指数
		int expand = stream.Get(3); //
		expand = cl_power10(expand);
		int vol-unit = stream.Get(3);//
		vol-unit = cl_power10(vol-unit);
		int isnow = stream.Get(1); //=0 历史分钟 =1 实时分钟
		if (head.compress){
			//last.offset = 
			stream.Get(16);
			//last.bit = 
			stream.Get(8);
		}
		//////////////////股票配置//////////////////////////////////////////
		tLastTime = stream.Get(32);
		stream.SETBITCODE(dayPriceCode);
		open_dw = stream.DecodeData(&open_dw);
		if (isnow)
		{
			stream.SETBITCODE(dayVolCode);
			volume_z = stream.DecodeMWordData(&volume_z);
		}
		//////////////////基准值//////////////////////////////////////////
		s_stock_day stkDay;
		result->clear();
		for (i = 0; i<head.count; i++)
		{
			memset(&stkDay, 0, sizeof(s_stock_day));
			stream.SETBITCODE(tickTimeCode);
			stkDay.m_time_t = stream.DecodeData((uint32*)&tLastTime);
			tLastTime = (uint32)stkDay.m_time_t;
			stkDay.m_time_t = stkDay.m_time_t*MINSEC;

			stream.SETBITCODE(dayPriceCode);
			stkDay.m_open_dw = stream.DecodeData(&open_dw);
			open_dw = stkDay.m_open_dw;

			stream.SETBITCODE(dayPriceDiffCode);
			stkDay.m_high_dw = stream.DecodeData(&open_dw);
			stkDay.m_low_dw = stream.DecodeData(&open_dw);
			stkDay.m_new_dw = stream.DecodeData(&open_dw);

			if (isnow)
			{
				stream.SETBITCODE(tickVolDiffCode);
				stkDay.m_volume_z = stream.DecodeMWordData(&volume_z);
				volume_z = stkDay.m_volume_z;
			}
			else
			{
				stream.SETBITCODE(dayVolCode);
				stkDay.m_volume_z = stream.DecodeMWordData();
			}
			stream.SETBITCODE(dayAmountCode);
			zint32 mAmountBase(0, 0);
			if (isindex == 0){
				mAmountBase = stkDay.m_volume_z*(int)(vol-unit*stkDay.m_new_dw / expand);
			}
			stkDay.m_amount_z = stream.DecodeMWordData(&mAmountBase);

			//stream.SETBITCODE(dayInnerVolCode);
			//stkDay.m_DayStock.m_dwInnerVol = stream.DecodeMWordData();
			if (type == OUT_COMPRESS_JSON)
			{
				sprintf(str, "[%d,%d,%d,%d,%d,"P64I","P64I"]", (uint32)stkDay.m_time_t, stkDay.m_open_dw, stkDay.m_high_dw,
					stkDay.m_low_dw, stkDay.m_new_dw, stkDay.m_volume_z.aslong(), stkDay.m_amount_z.aslong());
				*result += str;
				if (i < head.count - 1) *result += ",";
			}
			else{
				result->append((char *)&stkDay, sizeof(s_stock_day));
			}
		}
		//result += "]";
	}
	catch (...)
	{
		result->clear();
		if (type == OUT_COMPRESS_JSON) *result = "[]";
	}
	return result;
}


listNode *cl_redis_compress_tmin(uint8 *in, int count_, s_stock_static *info)
//info中类型区分指数，不能压缩计算成交金额
{
	if (count_<1) return NULL;
	s_stock_day *lpCur = (s_stock_day*)in;

	uint8 out[CL_COMPRESS_MAX_RECS*sizeof(s_stock_day)];
	c_bit_stream stream(out, CL_COMPRESS_MAX_RECS*sizeof(s_stock_day));

	int nPage = (int)(count_ / CL_COMPRESS_MAX_RECS) + 1;
	cl_block_head head;
	cl_last_info  last;
	listNode *result = NULL;
	try
	{
		int index = 0;
		int expand = cl_power10(info->m_price_digit_by);
		int outlen = 0;
		//////////////////基准值//////////////////////////////////////////
		uint32 start_min = 0;
		uint32 m_open_dw = 0;
		zint32  m_volume_z(0, 0);
		////////////////////////////////////////////////////////////
		for (int m = 0; m < nPage; m++)
		{
			outlen = 0;
			stream.MoveTo(0);
			if (m == (nPage - 1))
			{
				head.count = count_ - m*CL_COMPRESS_MAX_RECS;
				head.count--;
			}
			else{
				head.count = CL_COMPRESS_MAX_RECS;
			}
			writehead(stream, CL_COMPRESS_TMIN, head.count, info, true, false); 

			//////////////////股票配置//////////////////////////////////////////
			start_min = (uint32)lpCur[index].m_time_t;
			stream.SETBITCODE(dynaID);
			stream.EncodeData(start_min);
			m_open_dw = lpCur[index].m_open_dw;
			stream.SETBITCODE(dayPriceCode);
			stream.EncodeData(m_open_dw);
			m_volume_z = lpCur[index].m_volume_z;
			stream.SETBITCODE(dayVolCode);
			stream.EncodeData(m_volume_z);
			//////////////////基准值//////////////////////////////////////////
			for (int k = 0; k < head.count; k++, index++)
			{
				stream.SETBITCODE(dynaID);
				stream.EncodeData((uint32)lpCur[index].m_time_t, &start_min);
				start_min = (uint32)lpCur[index].m_time_t;

				stream.SETBITCODE(dayPriceCode);
				stream.EncodeData(lpCur[index].m_open_dw, &m_open_dw);
				m_open_dw = lpCur[index].m_open_dw;

				stream.SETBITCODE(dayPriceDiffCode);
				stream.EncodeData(lpCur[index].m_high_dw, &m_open_dw);
				stream.EncodeData(lpCur[index].m_low_dw, &m_open_dw);
				stream.EncodeData(lpCur[index].m_new_dw, &m_open_dw);

				stream.SETBITCODE(tickVolDiffCode);
				stream.EncodeData(lpCur[index].m_volume_z, &m_volume_z);
				m_volume_z = lpCur[index].m_volume_z;

				stream.SETBITCODE(dayAmountCode);
				zint32 mAmountBase(0, 0);
				if (info->m_type_by != STK_STATIC::INDEX)
					mAmountBase = lpCur[index].m_volume_z*(int)(info->m_volunit_w * lpCur[index].m_new_dw / expand);
				stream.EncodeData(lpCur[index].m_amount_z, &mAmountBase);

			} //k

			outlen = (stream.GetCurPos() + 7) / 8;
			//一个块压缩好了，生成sdsnode包
			result = sdsnode_push(result, out, outlen);
		}//m
		last.offset = outlen;
		last.bit = (stream.GetCurPos() % 8) > 0 ? 8 - (stream.GetCurPos() % 8) : 0; //offset*8 - bit  
		//最后还需要增加基准数据块head.compress=1,block.count=0/1;
		writehead(stream, CL_COMPRESS_TMIN, 1, info, true, true); 
		//LOG(5)("com offset=%d,bit=%d\n", last.offset, last.bit);
		stream.Put(last.offset, 16);
		stream.Put(last.bit, 8);
		//////////////////股票配置//////////////////////////////////////////
		stream.SETBITCODE(dynaID);
		stream.EncodeData(start_min);
		stream.SETBITCODE(dayPriceCode);
		stream.EncodeData(m_open_dw);

			stream.SETBITCODE(dayVolCode);
			stream.EncodeData(m_volume_z);
			//这里保存最后一条记录
			index = count_ - 1;
			stream.SETBITCODE(dynaID);
			stream.EncodeData((uint32)lpCur[index].m_time_t, &start_min);
			stream.SETBITCODE(dayPriceCode);
			stream.EncodeData(lpCur[index].m_open_dw, &m_open_dw);
			m_open_dw = lpCur[index].m_open_dw;
			stream.SETBITCODE(dayPriceDiffCode);
			stream.EncodeData(lpCur[index].m_high_dw, &m_open_dw);
			stream.EncodeData(lpCur[index].m_low_dw, &m_open_dw);
			stream.EncodeData(lpCur[index].m_new_dw, &m_open_dw);
			stream.SETBITCODE(tickVolDiffCode);
			stream.EncodeData(lpCur[index].m_volume_z, &m_volume_z);
			stream.SETBITCODE(dayAmountCode);
			zint32 mAmountBase(0, 0);
			if (info->m_type_by != STK_STATIC::INDEX)
				mAmountBase = lpCur[index].m_volume_z*(int)(info->m_volunit_w * lpCur[index].m_new_dw / expand);
			stream.EncodeData(lpCur[index].m_amount_z, &mAmountBase);
		//////////////////基准值//////////////////////////////////////////
		outlen = (stream.GetCurPos() + 7) / 8;
		result = sdsnode_push(result, out, outlen);
	}
	catch (...)
	{
		result = NULL;
	}
	return result;
}

listNode *cl_redis_compress_tmin_new(listNode *src, uint8 *in, int count_, s_stock_static *info)
{
	if (src == NULL){
		return cl_redis_compress_tmin(in, count_, info);
	}
	int oCount = get_compress_node_count(src);
	//printf("count_=%d -> %d\n", oCount, count_);
	if (count_<1 || count_<oCount) return NULL;

	if ((count_ - oCount) > 1 || oCount <= 1){  
		//防止一次传入多条记录,并且只对数量大于2的进行增量压缩
		//这里的算法很恶心，有空再改！
		//sdsnode_destroy(src);不能释放，否则字典修改时也会释放
		return cl_redis_compress_tmin(in, count_, info);
	}

	s_stock_day *lpCur = (s_stock_day*)in;

	//如果nCount==oCount，只修改最后一条记录，其他不动
	//如果nCount>oCount,表示有新的数据到了，用倒数第二条压缩放到1块中，最后一条压缩到结尾块中；

	listNode *lastnode = sdsnode_last_node(src);
	sds ls, s;
	int index;
	int outlen;
	uint32 start_min = 0;
	uint32 open_dw = 0;
	zint32  volume_z(0, 0);
	cl_last_info last;
	cl_block_head head;
	//第一步,从最后一个结点获取上一节点的偏移位置和最终的基准值
	ls = (sds)(lastnode->value);
	c_bit_stream stream((uint8*)ls, sdslen(ls));
	head.hch = stream.Get(8);
	head.compress = stream.Get(1);
	head.type = stream.Get(7);
	head.count = stream.Get(8);
	//stream.MoveTo(sizeof(cl_block_head)* 8);
	//int isindex = 
	stream.Get(1); //是不是指数
	int expand = stream.Get(3); //
	expand = cl_power10(expand);
	int vol-unit = stream.Get(3);//
	vol-unit = cl_power10(vol-unit);
	int isnow = stream.Get(1); //=0 历史分钟 =1 实时分钟
	if (head.count < 1 || head.compress != 1 || isnow != 1){
		LOG(5)("compress min error.[%d,%d,%d]\n", head.count ,head.compress , isnow);
		return NULL;
	}
	last.offset = stream.Get(16);
	last.bit = stream.Get(8);
	//LOG(5)("[%d]offset=%d,bit=%d\n", stream.GetCurPos(), last.offset, last.bit);

	stream.SETBITCODE(dynaID);
	start_min = stream.DecodeData();
	stream.SETBITCODE(dayPriceCode);
	open_dw = stream.DecodeData(&open_dw);
	stream.SETBITCODE(dayVolCode);
	volume_z = stream.DecodeMWordData(&volume_z);

	uint8 out[CL_COMPRESS_MAX_RECS*sizeof(s_stock_day)];
	if (count_>oCount)
	{
		listNode *node = lastnode->prev;
		//把倒数第二条数据增加到node最后记录的偏移位置，count+1，更新偏移位置
		//生成lastnode的基准值，然后把最后一条记录写入；
		s = (sds)(node->value);
		memmove(out, (uint8*)s, sdslen(s));
		c_bit_stream stream(out, CL_COMPRESS_MAX_RECS*sizeof(s_stock_day));
		//修改数量
		stream.MoveTo(16);
		int num = stream.Get(8);
		stream.MoveTo(16);
		stream.Put(num + 1, 8);
		//LOG(5)("offset=%d,bit=%d\n", last.offset, last.bit);
		stream.MoveTo(last.offset * 8 - last.bit);

		index = count_ - 2;

		stream.SETBITCODE(dynaID);
		stream.EncodeData((uint32)lpCur[index].m_time_t, &start_min);
		start_min = (uint32)lpCur[index].m_time_t;

		stream.SETBITCODE(dayPriceCode);
		stream.EncodeData(lpCur[index].m_open_dw, &open_dw);
		open_dw = lpCur[index].m_open_dw;

		stream.SETBITCODE(dayPriceDiffCode);
		stream.EncodeData(lpCur[index].m_high_dw, &open_dw);
		stream.EncodeData(lpCur[index].m_low_dw, &open_dw);
		stream.EncodeData(lpCur[index].m_new_dw, &open_dw);

		stream.SETBITCODE(tickVolDiffCode);
		stream.EncodeData(lpCur[index].m_volume_z, &volume_z);
		volume_z = lpCur[index].m_volume_z;
		stream.SETBITCODE(dayAmountCode);
		zint32 mAmountBase(0, 0);
		if (info->m_type_by != STK_STATIC::INDEX)
			mAmountBase = lpCur[index].m_volume_z*(int)(info->m_volunit_w * lpCur[index].m_new_dw / expand);
		stream.EncodeData(lpCur[index].m_amount_z, &mAmountBase);

		outlen = (stream.GetCurPos() + 7) / 8;
		last.offset = outlen;
		last.bit = (stream.GetCurPos() % 8) > 0 ? 8 - (stream.GetCurPos() % 8) : 0; //offset*8 - bit  
		//需要保证返回值相同，否则出错
		node = sdsnode_update(node, (char *)out, outlen);
	}
	//else  //不管怎么样,最后一条记录都需要更新
	{
		//仅仅修改最后一条行情信息，其他不动，需要解压基准值，
		s = (sds)(lastnode->value);
		memmove(out, (uint8*)s, sdslen(s));
		c_bit_stream stream(out, CL_COMPRESS_MAX_RECS*sizeof(s_stock_day));
		stream.MoveTo(sizeof(cl_block_head)* 8 + 8);
		//修改一下偏移位置
		stream.Put(last.offset, 16);
		stream.Put(last.bit, 8);
		//////////////////股票配置//////////////////////////////////////////
		stream.SETBITCODE(dynaID);
		stream.EncodeData(start_min);
		stream.SETBITCODE(dayPriceCode);
		stream.EncodeData(open_dw);
		stream.SETBITCODE(dayVolCode);
		stream.EncodeData(volume_z);
		/////////////////解压基准值，然后重新写入新值////////////////////////
		index = count_ - 1;
		stream.SETBITCODE(dynaID);
		stream.EncodeData((uint32)lpCur[index].m_time_t, &start_min);
		stream.SETBITCODE(dayPriceCode);
		stream.EncodeData(lpCur[index].m_open_dw, &open_dw);
		open_dw = lpCur[index].m_open_dw;
		stream.SETBITCODE(dayPriceDiffCode);
		stream.EncodeData(lpCur[index].m_high_dw, &open_dw);
		stream.EncodeData(lpCur[index].m_low_dw, &open_dw);
		stream.EncodeData(lpCur[index].m_new_dw, &open_dw);
		stream.SETBITCODE(tickVolDiffCode);
		stream.EncodeData(lpCur[index].m_volume_z, &volume_z);
		stream.SETBITCODE(dayAmountCode);
		zint32 mAmountBase(0, 0);
		if (info->m_type_by != STK_STATIC::INDEX)
			mAmountBase = lpCur[index].m_volume_z*(int)(info->m_volunit_w * lpCur[index].m_new_dw / expand);
		stream.EncodeData(lpCur[index].m_amount_z, &mAmountBase);

		outlen = (stream.GetCurPos() + 7) / 8;
		//需要保证返回值相同，否则出错
		lastnode = sdsnode_update(lastnode, (char *)out, outlen);
	}

	return src;
}

s_memory_node *cl_redis_uncompress_tmin(uint8 *in, int inlen, OUT_COMPRESS_TYPE type)
{
	int i;
	char str[255];
	s_memory_node * result = malloc_memory();;

	uint32 open_dw = 0;
	uint32 start_min = 0;
	zint32  volume_z(0, 0);
	cl_block_head head;
	c_bit_stream stream((uint8*)in, inlen);
	try
	{
		result->clear();
		head.hch = stream.Get(8);
		head.compress = stream.Get(1);
		head.type = stream.Get(7);
		head.count = stream.Get(8);
		if (head.count < 1) return result;
		///////////////////头结构/////////////////////////////////////////
		int isindex = stream.Get(1); //是不是指数
		int expand = stream.Get(3); //
		expand = cl_power10(expand);
		int vol-unit = stream.Get(3);//
		vol-unit = cl_power10(vol-unit);
		int isnow = stream.Get(1); //=0 历史分钟 =1 实时分钟
		if (isnow == 0) return result;
		if (head.compress){
			//last.offset = 
			stream.Get(16);
			//last.bit = 
			stream.Get(8);
		}
		//////////////////股票配置//////////////////////////////////////////
		stream.SETBITCODE(dynaID);
		start_min = stream.DecodeData();
		stream.SETBITCODE(dayPriceCode);
		open_dw = stream.DecodeData(&open_dw);
		stream.SETBITCODE(dayVolCode);
		volume_z = stream.DecodeMWordData(&volume_z);
		//////////////////基准值//////////////////////////////////////////
		s_stock_day stkDay;
		result->clear();
		for (i = 0; i<head.count; i++)
		{
			memset(&stkDay, 0, sizeof(s_stock_day));

			stream.SETBITCODE(dynaID);
			uint32 tt = 0;
			stream.GetNoMove(32, tt);
			printf("tt=%x\n", tt);
			stkDay.m_time_t = stream.DecodeData(&start_min);
			start_min = (uint32)stkDay.m_time_t;

			stream.SETBITCODE(dayPriceCode);
			stkDay.m_open_dw = stream.DecodeData(&open_dw);
			open_dw = stkDay.m_open_dw;

			stream.SETBITCODE(dayPriceDiffCode);
			stkDay.m_high_dw = stream.DecodeData(&open_dw);
			stkDay.m_low_dw = stream.DecodeData(&open_dw);
			stkDay.m_new_dw = stream.DecodeData(&open_dw);

			stream.SETBITCODE(tickVolDiffCode);
			stkDay.m_volume_z = stream.DecodeMWordData(&volume_z);
			volume_z = stkDay.m_volume_z;

			stream.SETBITCODE(dayAmountCode);
			zint32 mAmountBase(0, 0);
			if (isindex == 0){
				mAmountBase = stkDay.m_volume_z*(int)(vol-unit*stkDay.m_new_dw / expand);
			}
			stkDay.m_amount_z = stream.DecodeMWordData(&mAmountBase);
			if (type == OUT_COMPRESS_JSON)
			{
				sprintf(str, "[%d,%d,%d,%d,%d,"P64I","P64I"]", (uint32)stkDay.m_time_t, stkDay.m_open_dw, stkDay.m_high_dw,
					stkDay.m_low_dw, stkDay.m_new_dw, stkDay.m_volume_z.aslong(), stkDay.m_amount_z.aslong());
				*result += str;
				if (i < head.count - 1) *result += ",";
			}
			else{
				result->append((char *)&stkDay, sizeof(s_stock_day));
			}
		}
		//result += "]";
	}
	catch (...)
	{
		result->clear();
		if (type == OUT_COMPRESS_JSON) *result = "[]";
	}
	return result;
}

////////////////////////////////////////////////////////
//  day - tick  压缩
////////////////////////////////////////////////////////
listNode *cl_redis_compress_tick(uint8 *in, int count_, s_stock_static *info, bool isnow)
{

	if (count_<1) return NULL;
	s_stock_day *lpCur = (s_stock_day*)in;

	uint8 out[CL_COMPRESS_MAX_RECS*sizeof(s_stock_day)];
	c_bit_stream stream(out, CL_COMPRESS_MAX_RECS*sizeof(s_stock_day));

	int nPage = (int)(count_ / CL_COMPRESS_MAX_RECS) + 1;
	cl_block_head head;
	listNode *result = NULL;
	try
	{
		int index = 0;
		int outlen = 0;
		//int expand = cl_power10(info->m_wPriceDigit);
		//////////////////基准值//////////////////////////////////////////
		uint32 tLastTime = 0;
		uint32 new_dw = 0;
		zint32  volume_z(0, 0);
		////////////////////////////////////////////////////////////
		for (int m = 0; m < nPage; m++)
		{
			if (m == (nPage - 1)) head.count = count_ - m*CL_COMPRESS_MAX_RECS;
			else head.count = CL_COMPRESS_MAX_RECS;

			if (isnow) writehead(stream, CL_COMPRESS_TICK, head.count, info, true, false);
			else writehead(stream, CL_COMPRESS_D5, head.count, info, false, false);
			//////////////////股票配置//////////////////////////////////////////
			if (!isnow){ //5日线
				tLastTime = (uint32)(lpCur[index].m_time_t / MINSEC);
			}
			else{
				tLastTime = (uint32)(lpCur[index].m_time_t);
			}
			stream.Put(tLastTime, 32);
			new_dw = lpCur[index].m_new_dw;
			stream.SETBITCODE(tickPriceCode); /********/
			stream.EncodeData(new_dw);
			if (isnow)
			{
				volume_z = lpCur[index].m_volume_z;
				stream.SETBITCODE(dayVolCode);
				stream.EncodeData(volume_z);
			}
			//////////////////基准值//////////////////////////////////////////
			for (int k = 0; k < head.count; k++, index++)
			{
				stream.SETBITCODE(tickTimeCode);
				if (!isnow){ //5日线
					stream.EncodeData((uint32)(lpCur[index].m_time_t / MINSEC), (uint32*)&tLastTime);
					tLastTime = (uint32)(lpCur[index].m_time_t / MINSEC);
				}
				else{
					stream.EncodeData((uint32)(lpCur[index].m_time_t), (uint32*)&tLastTime);
					tLastTime = (uint32)(lpCur[index].m_time_t);
				}

				stream.SETBITCODE(tickPriceDiffCode);
				stream.EncodeData(lpCur[index].m_new_dw, &new_dw);
				new_dw = lpCur[index].m_new_dw;

				if (isnow)
				{
					stream.SETBITCODE(tickVolDiffCode);
					stream.EncodeData(lpCur[index].m_volume_z, &volume_z);
					volume_z = lpCur[index].m_volume_z;
				}
				else
				{
					stream.SETBITCODE(dayVolCode);
					stream.EncodeData(lpCur[index].m_volume_z);
				}

				/*stream.SETBITCODE(dayAmountCode);
				zint32 mAmountBase(0, 0);
				if (info->m_wType != STK_STATIC::INDEX)
					mAmountBase = lpCur[index].m_volume_z*(int)(info->m_wVolUnit * lpCur[index].m_new_dw / expand);
				stream.EncodeData(lpCur[index].m_amount_z, &mAmountBase);*/
				//stream.SETBITCODE(dayInnerVolCode);
				//stream.EncodeData(lpCur[i].m_DayStock.m_dwInnerVol);
			} //k
			outlen = (stream.GetCurPos() + 7) / 8;
			//一个块压缩好了，生成sds包
			//printf("2.1.1 %d\n", outlen);
			result = sdsnode_push(result, out, outlen);
		}//m
		//最后还需要增加基准数据块head.compress=1,block.count=0/1;
	}
	catch (...)
	{
		result = NULL;
	}
	return result;
}

////////////////////////////////////////////////////////
//  tick - tick  压缩
////////////////////////////////////////////////////////
listNode *cl_redis_compress_tick_to_tick(uint8 *in, int count_, s_stock_static *info, bool isnow)
{

	if (count_<1) return NULL;
	s_stock_tick *lpCur = (s_stock_tick*)in;

	uint8 out[CL_COMPRESS_MAX_RECS*sizeof(s_stock_tick)];
	c_bit_stream stream(out, CL_COMPRESS_MAX_RECS*sizeof(s_stock_tick));

	int nPage = (int)(count_ / CL_COMPRESS_MAX_RECS) + 1;
	cl_block_head head;
	listNode *result = NULL;
	try
	{
		int index = 0;
		int outlen = 0;
		//////////////////基准值//////////////////////////////////////////
		uint32 tLastTime = 0;
		uint32 new_dw = 0;
		zint32  volume_z(0, 0);
		////////////////////////////////////////////////////////////
		for (int m = 0; m < nPage; m++)
		{
			if (m == (nPage - 1)) head.count = count_ - m*CL_COMPRESS_MAX_RECS;
			else head.count = CL_COMPRESS_MAX_RECS;

			if (isnow) writehead(stream, CL_COMPRESS_TICK, head.count, info, true, false);
			else writehead(stream, CL_COMPRESS_D5, head.count, info, false, false);
			//////////////////股票配置//////////////////////////////////////////
			if (!isnow){ //5日线
				tLastTime = (uint32)(lpCur[index].m_time_t / MINSEC);
			}
			else{
				tLastTime = (uint32)(lpCur[index].m_time_t);
			}
			stream.Put(tLastTime, 32);
			new_dw = lpCur[index].m_new_dw;
			stream.SETBITCODE(tickPriceCode); /********/
			stream.EncodeData(new_dw);
			if (isnow)
			{
				volume_z = lpCur[index].m_volume_z;
				stream.SETBITCODE(dayVolCode);
				stream.EncodeData(volume_z);
			}
			//////////////////基准值//////////////////////////////////////////
			for (int k = 0; k < head.count; k++, index++)
			{
				stream.SETBITCODE(tickTimeCode);
				if (!isnow){ //5日线
					stream.EncodeData((uint32)(lpCur[index].m_time_t / MINSEC), (uint32*)&tLastTime);
					tLastTime = (uint32)(lpCur[index].m_time_t / MINSEC);
				}
				else{
					stream.EncodeData((uint32)(lpCur[index].m_time_t), (uint32*)&tLastTime);
					tLastTime = (uint32)(lpCur[index].m_time_t);
				}

				stream.SETBITCODE(tickPriceDiffCode);
				stream.EncodeData(lpCur[index].m_new_dw, &new_dw);
				new_dw = lpCur[index].m_new_dw;

				if (isnow)
				{
					stream.SETBITCODE(tickVolDiffCode);
					stream.EncodeData(lpCur[index].m_volume_z, &volume_z);
					volume_z = lpCur[index].m_volume_z;
				}
				else
				{
					stream.SETBITCODE(dayVolCode);
					stream.EncodeData(lpCur[index].m_volume_z);
				}

				/*stream.SETBITCODE(dayAmountCode);
				zint32 mAmountBase(0, 0);
				if (info->m_wType != STK_STATIC::INDEX)
				mAmountBase = lpCur[index].m_volume_z*(int)(info->m_wVolUnit * lpCur[index].m_new_dw / expand);
				stream.EncodeData(lpCur[index].m_amount_z, &mAmountBase);*/
				//stream.SETBITCODE(dayInnerVolCode);
				//stream.EncodeData(lpCur[i].m_DayStock.m_dwInnerVol);
			} //k
			outlen = (stream.GetCurPos() + 7) / 8;
			//一个块压缩好了，生成sds包
			result = sdsnode_push(result, out, outlen);
		}//m
	}
	catch (...)
	{
		result = NULL;
	}
	return result;
}

s_memory_node * cl_redis_uncompress_tick(uint8 *in, int inlen, OUT_COMPRESS_TYPE type)
//isnow=0表示历史的tick主要提供5日线的数据,vol为每分钟成交量;
//isnow=1表示当天的tick包括当日1分钟线和分笔成交的数据,vol为当日开市后累计成交量;
{
	int i;
	char str[255];
	s_memory_node * result = malloc_memory();;

	uint32 new_dw = 0;
	uint32 tLastTime = 0;
	zint32  volume_z(0, 0);
	cl_block_head head;
	c_bit_stream stream((uint8*)in, inlen);
	try
	{
		result->clear();
		head.hch = stream.Get(8);
		head.compress = stream.Get(1);
		head.type = stream.Get(7);
		head.count = stream.Get(8);
		if (head.count < 1) return result;
		///////////////////头结构/////////////////////////////////////////
		//int isindex = 
		stream.Get(1); //是不是指数
		int expand = stream.Get(3); //
		expand = cl_power10(expand);
		int vol-unit = stream.Get(3);//
		vol-unit = cl_power10(vol-unit);
		int isnow = stream.Get(1); //=0 历史分钟 =1 实时分钟
		if (head.compress){
			//last.offset = 
				stream.Get(16);
			//last.bit = 
				stream.Get(8);
		}
		//////////////////股票配置//////////////////////////////////////////
		tLastTime = stream.Get(32);
		stream.SETBITCODE(tickPriceCode);
		new_dw = stream.DecodeData(&new_dw);
		if (isnow)
		{
			stream.SETBITCODE(dayVolCode);
			volume_z = stream.DecodeMWordData(&volume_z);
		}
		//////////////////基准值//////////////////////////////////////////
		//s_stock_day stkDay;
		s_stock_tick stkTick;
		result->clear();
		for (i = 0; i<head.count; i++)
		{
			//memset(&stkDay, 0, sizeof(s_stock_day));
			memset(&stkTick, 0, sizeof(s_stock_tick));
			stream.SETBITCODE(tickTimeCode);
			stkTick.m_time_t = stream.DecodeData((uint32*)&tLastTime);
			tLastTime = (uint32)stkTick.m_time_t;
			if (!isnow)
			{
				stkTick.m_time_t = stkTick.m_time_t*MINSEC;
			}
			stream.SETBITCODE(tickPriceDiffCode);
			stkTick.m_new_dw = stream.DecodeData(&new_dw);
			new_dw = stkTick.m_new_dw;

			if (isnow)
			{
				stream.SETBITCODE(tickVolDiffCode);
				stkTick.m_volume_z = stream.DecodeMWordData(&volume_z);
				volume_z = stkTick.m_volume_z;
			}
			else
			{
				stream.SETBITCODE(dayVolCode);
				stkTick.m_volume_z = stream.DecodeMWordData();
			}
			if (type == OUT_COMPRESS_JSON)
			{
				sprintf(str, "[%d,%d,"P64I"]", (uint32)stkTick.m_time_t, stkTick.m_new_dw, stkTick.m_volume_z.aslong());
				*result += str;
				if (i < head.count - 1) *result += ",";
			}
			else{
				//result->append((char *)&stkTick, sizeof(s_stock_day));
				result->append((char *)&stkTick, sizeof(s_stock_tick));
			}
		}
	}
	catch (...)
	{
		result->clear();
		if (type == OUT_COMPRESS_JSON) *result = "[]";
	}
	return result;
}
s_memory_node * cl_redis_uncompress_tick_to_day(uint8 *in, int inlen, OUT_COMPRESS_TYPE type)
{
	int i;
	char str[255];
	s_memory_node * result = malloc_memory();

	uint32 new_dw = 0;
	uint32 tLastTime = 0;
	zint32  volume_z(0, 0);
	cl_block_head head;
	c_bit_stream stream((uint8*)in, inlen);
	try
	{
		result->clear();
		head.hch = stream.Get(8);
		head.compress = stream.Get(1);
		head.type = stream.Get(7);
		head.count = stream.Get(8);
		if (head.count < 1) return result;
		///////////////////头结构/////////////////////////////////////////
		//int isindex = 
		stream.Get(1); //是不是指数
		int expand = stream.Get(3); //
		expand = cl_power10(expand);
		int vol-unit = stream.Get(3);//
		vol-unit = cl_power10(vol-unit);
		int isnow = stream.Get(1); //=0 历史分钟 =1 实时分钟
		if (head.compress){
			//last.offset = 
			stream.Get(16);
			//last.bit = 
			stream.Get(8);
		}
		//////////////////股票配置//////////////////////////////////////////
		tLastTime = stream.Get(32);
		stream.SETBITCODE(tickPriceCode);
		new_dw = stream.DecodeData(&new_dw);
		if (isnow)
		{
			stream.SETBITCODE(dayVolCode);
			volume_z = stream.DecodeMWordData(&volume_z);
		}
		//////////////////基准值//////////////////////////////////////////
		s_stock_day stkDay;
		result->clear();
		for (i = 0; i<head.count; i++)
		{
			memset(&stkDay, 0, sizeof(s_stock_day));
			stream.SETBITCODE(tickTimeCode);
			stkDay.m_time_t = stream.DecodeData((uint32*)&tLastTime);
			tLastTime = (uint32)stkDay.m_time_t;
			if (!isnow)
			{
				stkDay.m_time_t = stkDay.m_time_t*MINSEC;
			}
			stream.SETBITCODE(tickPriceDiffCode);
			stkDay.m_new_dw = stream.DecodeData(&new_dw);
			new_dw = stkDay.m_new_dw;

			if (isnow)
			{
				stream.SETBITCODE(tickVolDiffCode);
				stkDay.m_volume_z = stream.DecodeMWordData(&volume_z);
				volume_z = stkDay.m_volume_z;
			}
			else
			{
				stream.SETBITCODE(dayVolCode);
				stkDay.m_volume_z = stream.DecodeMWordData();
			}
			if (type == OUT_COMPRESS_JSON)
			{
				sprintf(str, "[%d,%d,"P64I"]", (uint32)stkDay.m_time_t, stkDay.m_new_dw, stkDay.m_volume_z.aslong());
				*result += str;
				if (i < head.count - 1) *result += ",";
			}
			else{
				result->append((char *)&stkDay, sizeof(s_stock_day));
			}
		}
	}
	catch (...)
	{
		result->clear();
		if (type == OUT_COMPRESS_JSON) *result = "[]";
	}
	return result;

}


listNode *cl_redis_compress_dyna(uint8 *in, int count_, s_stock_static *info_, bool ismap)
{
	if (count_<1) return NULL;
	s_stock_dynamic *lpCur = (s_stock_dynamic*)in;

	uint8 out[CL_COMPRESS_MAX_RECS*sizeof(s_stock_dynamic)];
	c_bit_stream stream(out, CL_COMPRESS_MAX_RECS*sizeof(s_stock_dynamic));

	int nPage = (int)(count_ / CL_COMPRESS_MAX_RECS) + 1;
	listNode *result = NULL;
	try
	{
		int index = 0;
		int outlen = 0;
		int count = 0;
		uint32 start_index = 0;
		//////////////////基准值//////////////////////////////////////////
		uint32 tLastTime = 0;
		////////////////////////////////////////////////////////////
		for (int m = 0; m < nPage; m++)
		{
			if (m == (nPage - 1)) count = count_ - m*CL_COMPRESS_MAX_RECS;
			else count = CL_COMPRESS_MAX_RECS;

			stream.MoveTo(0);
			stream.Put('R', 8);
			if (ismap) stream.Put(1, 1);
			else stream.Put(0, 1); 
			stream.Put(CL_COMPRESS_DYNA, 7);
			stream.Put(count, 8);
			//////////////////股票配置//////////////////////////////////////////
			tLastTime = (uint32)(lpCur[index].m_time_t);
			stream.Put(tLastTime, 32);
			if (ismap){
				stream.SETBITCODE(dynaID);
				start_index = lpCur[index].m_id_w;
				stream.EncodeData(start_index);
			}
			//////////////////基准值//////////////////////////////////////////
			//printf("[%d] %d  %d\n", index, count, count_);
			for (int k = 0; k < count; k++, index++)
			{
				uint32  price_dw = 0;

				s_stock_static *info = info_ + lpCur[index].m_id_w;
				//printf("[%d] %d  %d\n", index, count,lpCur[index].m_id_w);
				if (info->m_type_by == STK_STATIC::INDEX) stream.Put(1, 1);
				else stream.Put(0, 1); //不是指数，
				stream.Put(info->m_price_digit_by, 3);
				int volpower = cl_sqrt10(info->m_volunit_w);
				stream.Put(volpower, 3);
				//if (isnow) stream.Put(1, 1);
				stream.Put(0, 1);//备用
				int expand = cl_power10(info->m_price_digit_by);

				if (!ismap){
					stream.SETBITCODE(dynaID);
					stream.EncodeData(lpCur[index].m_id_w, &start_index);
					start_index = lpCur[index].m_id_w;
				} //map是全部行情，顺序存放，不需要id

				stream.SETBITCODE(tickTimeCode);
				stream.EncodeData((uint32)(lpCur[index].m_time_t), (uint32*)&tLastTime);

				price_dw = lpCur[index].m_new_dw;
				stream.SETBITCODE(tickPriceCode); 
				stream.EncodeData(price_dw);

				stream.SETBITCODE(dayVolCode);
				stream.EncodeData(lpCur[index].m_volume_z);
				
				stream.SETBITCODE(dayAmountCode);
				zint32 mAmountBase(0, 0);
				if (info->m_type_by != STK_STATIC::INDEX)
					mAmountBase = lpCur[index].m_volume_z*(int)(info->m_volunit_w * lpCur[index].m_new_dw / expand);
				stream.EncodeData(lpCur[index].m_amount_z, &mAmountBase);

				stream.SETBITCODE(tickPriceDiffCode);
				stream.EncodeData(lpCur[index].m_open_dw, &price_dw);
				stream.EncodeData(lpCur[index].m_high_dw, &price_dw);
				stream.EncodeData(lpCur[index].m_low_dw, &price_dw);

				for (int nn = 0; nn < 5; nn++){
					if (info->m_type_by == STK_STATIC::INDEX){
						stream.SETBITCODE(dynaID);
						stream.EncodeData(lpCur[index].m_buy_price_dw[nn]);
						stream.EncodeData(lpCur[index].m_sell_price_dw[nn]);
					}
					else{
						stream.SETBITCODE(tickPriceDiffCode);
						stream.EncodeData(lpCur[index].m_buy_price_dw[nn], &price_dw);
						stream.EncodeData(lpCur[index].m_sell_price_dw[nn], &price_dw);
					}
				}
				stream.SETBITCODE(mmpVolCode);
				for (int nn = 0; nn < 5; nn++){
					stream.EncodeData(lpCur[index].m_buy_vol_dw[nn]);
					stream.EncodeData(lpCur[index].m_sell_vol_dw[nn]);
				}

			} //k
			outlen = (stream.GetCurPos() + 7) / 8;
			//printf("[%s] outlen=%d\n", outlen);
			//一个块压缩好了，生成sds包
			result = sdsnode_push(result, out, outlen);
		}//m
	}
	catch (...)
	{
		result = NULL;
	}
	return result;
}
s_memory_node *cl_redis_uncompress_dyna(uint8 *in, int inlen, OUT_COMPRESS_TYPE type){
	int i;
	char str[255];
	s_memory_node * result = malloc_memory();;

	cl_block_head head;
	uint32 tLastTime = 0;
	c_bit_stream stream((uint8*)in, inlen);
	try
	{
		result->clear();
		head.hch = stream.Get(8);
		head.compress = stream.Get(1); //ismap
		head.type = stream.Get(7);
		head.count = stream.Get(8);
		if (head.count < 1) return result;
		///////////////////头结构/////////////////////////////////////////
		tLastTime = stream.Get(32);
		stream.SETBITCODE(dynaID);
		uint32 index = 0;
		if (head.compress)
		{
			index = stream.DecodeData();
		}
		//////////////////股票配置//////////////////////////////////////////
		s_stock_dynamic dyna;
		result->clear();
		for (i = 0; i<head.count; i++)
		{
			//uint32  id_dw = 0;
			uint32  price_dw = 0;
	
			int isindex = stream.Get(1);//STK_STATIC::INDEX
			int expand = cl_power10(stream.Get(3));
			int vol-unit = cl_power10(stream.Get(3));
			stream.Get(1);

			memset(&dyna, 0, sizeof(s_stock_dynamic));
			if (!head.compress){
				stream.SETBITCODE(dynaID);
				dyna.m_id_w = stream.DecodeData(&index);
				//printf("====%d,--%d\n", index, dyna.m_id_w);
				index = dyna.m_id_w;
			}
			else{ //map是全部行情，顺序存放，不需要id
				dyna.m_id_w = index;
				index++;
			}
			stream.SETBITCODE(tickTimeCode);
			dyna.m_time_t = stream.DecodeData((uint32*)&tLastTime);

			stream.SETBITCODE(tickPriceCode);
			dyna.m_new_dw = stream.DecodeData();
			price_dw = dyna.m_new_dw;

			stream.SETBITCODE(dayVolCode);
			dyna.m_volume_z = stream.DecodeMWordData();

			stream.SETBITCODE(dayAmountCode);
			zint32 mAmountBase(0, 0);
			if (isindex == 0){
				mAmountBase = dyna.m_volume_z*(int)(vol-unit*dyna.m_new_dw / expand);
			}
			dyna.m_amount_z = stream.DecodeMWordData(&mAmountBase);

			stream.SETBITCODE(tickPriceDiffCode);
			dyna.m_open_dw = stream.DecodeData(&price_dw);
			dyna.m_high_dw = stream.DecodeData(&price_dw);
			dyna.m_low_dw = stream.DecodeData(&price_dw);

			for (int nn = 0; nn < 5; nn++){
				if (isindex == 1){
					stream.SETBITCODE(dynaID);
					dyna.m_buy_price_dw[nn] = stream.DecodeData();
					dyna.m_sell_price_dw[nn] = stream.DecodeData();
				}
				else{
					stream.SETBITCODE(tickPriceDiffCode);
					dyna.m_buy_price_dw[nn] = stream.DecodeData(&price_dw);
					dyna.m_sell_price_dw[nn] = stream.DecodeData(&price_dw);
				}
			}
			stream.SETBITCODE(mmpVolCode);
			for (int nn = 0; nn < 5; nn++){
				dyna.m_buy_vol_dw[nn] = stream.DecodeData();
				dyna.m_sell_vol_dw[nn] = stream.DecodeData();
			}
			if (type == OUT_COMPRESS_JSON)
			{
				sprintf(str, "[%d,%d,%d,%d,%d,%d,"P64I","P64I",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
					dyna.m_id_w, (uint32)dyna.m_time_t, dyna.m_open_dw, dyna.m_high_dw, dyna.m_low_dw,
					dyna.m_new_dw, dyna.m_volume_z.aslong(), dyna.m_amount_z.aslong(),
					dyna.m_buy_price_dw[0], dyna.m_buy_vol_dw[0], dyna.m_sell_price_dw[0], dyna.m_sell_vol_dw[0],
					dyna.m_buy_price_dw[1], dyna.m_buy_vol_dw[1], dyna.m_sell_price_dw[1], dyna.m_sell_vol_dw[1],
					dyna.m_buy_price_dw[2], dyna.m_buy_vol_dw[2], dyna.m_sell_price_dw[2], dyna.m_sell_vol_dw[2],
					dyna.m_buy_price_dw[3], dyna.m_buy_vol_dw[3], dyna.m_sell_price_dw[3], dyna.m_sell_vol_dw[3],
					dyna.m_buy_price_dw[4], dyna.m_buy_vol_dw[4], dyna.m_sell_price_dw[4], dyna.m_sell_vol_dw[4]);			
				*result += str;
				if (i < head.count - 1) *result += ",";
			}
			else{
				result->append((char *)&dyna, sizeof(s_stock_dynamic));
			}
		}
	}
	catch (...)
	{
		result->clear();
		if (type == OUT_COMPRESS_JSON) *result = "[]";
	}
	return result;

}
