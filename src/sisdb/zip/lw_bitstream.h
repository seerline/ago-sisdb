#ifndef _LW_BITSTREAM_H
#define _LW_BITSTREAM_H

#include "sis_core.h"
#include "lw_zint.h"

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

typedef struct s_bit_code{
	uint16	m_sign_w;	    //编码标记
	uint8	m_len_by;		//编码长度
	uint8	m_data_len_by;	//编码后跟随的数据长度,单位Bit，最大32Bit
	char	m_method_c;		//编码方法
	uint32	m_data_dw;	    /*编码数据*/
	uint32	m_data_offset_dw;	//编码数据偏移量
}s_bit_code;

typedef struct s_bit_stream{
	uint8*    m_stream;				//缓冲区
	uint32    m_bit_size_i;			//缓冲区大小，Bit

	uint32    m_nowpos_i;			//当前位置
	uint32    m_savepos_i;			//上一次保存的位置

	const s_bit_code  *m_nowcode_p;    //当前编解码码表
	int		m_nowcode_count;	 //当前编解码码表记录数
}s_bit_stream;

#pragma pack(pop)

bool is_original_data(const s_bit_code *bm);
// 是否原始数据
bool is_bit_position(const s_bit_code *bm);
// 是否表示1的位置

s_bit_stream *create_bit_stream(uint8 *in_, size_t inLen_, uint32 initPos_);
void destroy_bit_stream(s_bit_stream *);

void bit_stream_link(s_bit_stream *s_, uint8 *in_, uint32 inLen_, uint32 initPos_);
int bit_stream_getbytelen(s_bit_stream *s_);//得到当前Bit流Byte数

#endif //_LW_BITSTREAM_H

