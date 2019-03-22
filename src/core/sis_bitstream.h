#ifndef _SIS_BITSTREAM_H
#define _SIS_BITSTREAM_H

#include "sis_core.h"
#include "sis_zint.h"

//m_method_c
//'B':数据不超过m_data_dw位, 
//'b':数据不超过m_data_dw位, 带符号数
//'m':数据不超过m_data_dw位, 负数
//'S':数据右移HIWORD(m_data_dw)后不超过LOWORD(m_data_dw)位
//'E':数据等于CodeData
//'Z':数据为x*10^N, x<CodeData,N==1-4
//'P':在m_data_dw位数据中，存在一个1，这个1的位置
//'D':原始整形数据，原始数据放入Bit流,总是放在最后面，不进行匹配
//'F':原始浮点数据，原始数据放入Bit流,总是放在最后面，不进行匹配
//'M':原始zint32数据，原始数据放入Bit流,总是放在最后面，不进行匹配

#pragma pack(push,1)

typedef struct s_sis_bit_code{
	uint16	m_sign_w;	    //编码标记
	uint8	m_len_by;		//编码长度
	uint8	m_data_len_by;	//编码后跟随的数据长度,单位Bit，最大32Bit
	char	m_method_c;		//编码方法
	uint32	m_data_dw;	         //编码数据
	uint32	m_data_offset_dw;	//编码数据偏移量
}s_sis_bit_code;

typedef struct s_sis_bit_stream{
	uint8*    m_stream;				//缓冲区
	uint32    m_bit_size_i;			//缓冲区大小，Bit

	uint32    m_nowpos_i;			//当前位置
	uint32    m_savepos_i;			//上一次保存的位置

	const   s_sis_bit_code  *m_nowcode_p;    //当前编解码码表
	int		m_nowcode_count;	     //当前编解码码表记录数
}s_sis_bit_stream;

#pragma pack(pop)

#define SETBITCODE(_s_,_b_) { ((s_sis_bit_stream *)_s_)->m_nowcode_p = &_b_[0]; ((s_sis_bit_stream *)_s_)->m_nowcode_count = sizeof(_b_)/sizeof(_b_[0]); }

// #define SETBITCODE(_s_,_b_) {  }

s_sis_bit_stream *sis_bitstream_create(uint8 *in_, size_t inLen_, uint32 initPos_);
void sis_bitstream_destroy(s_sis_bit_stream *);

void sis_bitstream_link(s_sis_bit_stream *s_, uint8 *in_, uint32 inLen_, uint32 initPos_);
int  sis_bitstream_getbytelen(s_sis_bit_stream *s_);//得到当前Bit流Byte数

int    sis_bitstream_get_nomove(s_sis_bit_stream *s_, int nBit, uint32 *out);
uint32 sis_bitstream_get(s_sis_bit_stream *s_, int nBit);
void   sis_bitstream_move(s_sis_bit_stream *s_, int byte);
int    sis_bitstream_get_string(s_sis_bit_stream *s_, char* out_, int outLen_);
int    sis_bitstream_get_buffer(s_sis_bit_stream *s_, char* out_, int outLen_);
int    sis_bitstream_moveto(s_sis_bit_stream *s_, int nPos);

int sis_bitstream_put(s_sis_bit_stream *s_, uint32 dw, int nBit);
int sis_bitstream_put_string(s_sis_bit_stream *s_, const char * str);
int sis_bitstream_put_buffer(s_sis_bit_stream *s_, char *in, size_t len);

const s_sis_bit_code* sis_bitstream_find_original_data(s_sis_bit_stream *s_);
const s_sis_bit_code* sis_bitstream_find_string_data(s_sis_bit_stream *s_);
const s_sis_bit_code* sis_bitstream_decode_find_match(s_sis_bit_stream *s_, uint32 *out_);

int sis_bitstream_encode_uint32(s_sis_bit_stream *s_, uint32 dwData, const uint32 *pdwLastData, bool bReverse);
int sis_bitstream_encode_zint32(s_sis_bit_stream *s_, zint32 mData, const zint32* pmLastData);
int sis_bitstream_encode_float(s_sis_bit_stream *s_, float f, bool check0);
int sis_bitstream_encode_string(s_sis_bit_stream *s_, const char * strData, uint32 *base);

uint32 sis_bitstream_decode_uint32(s_sis_bit_stream *s_, const uint32* base, bool bReverse);
void sis_bitstream_decode_zint32(s_sis_bit_stream *s_, const zint32* base, zint32 *out, bool bReverse);
float sis_bitstream_decode_float(s_sis_bit_stream *s_, bool check0);
uint32 sis_bitstream_decode_string(s_sis_bit_stream *s_, char* in_, int inLen_, uint32 *base_);						//如果解出数字Label，则返回该数字，否则返回0xFFFFFFFF,更新dwLastData

#endif //_LW_BITSTREAM_H

