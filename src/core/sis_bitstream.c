
#include "sis_bitstream.h"
#include "sis_malloc.h"
#include "sis_math.h"
// 是否原始数据
bool _is_original_data(const s_sis_bit_code *bm)
{
	return (bm->m_method_c == 'D' || bm->m_method_c == 'F' || bm->m_method_c == 'M');
}

bool _is_bit_position(const s_sis_bit_code *bm)
{
	return (bm->m_method_c == 'P');
}
////////////////////////////////////////////////////////////////////////////////////////////
//s_sis_bit_stream
////////////////////////////////////////////////////////////////////////////////////////////

s_sis_bit_stream *sis_bitstream_create(uint8 *in_, size_t inLen_, uint32 initPos_)
{
	s_sis_bit_stream *s = sis_malloc(sizeof(s_sis_bit_stream));
	sis_bitstream_link(s, in_, (uint32)inLen_, initPos_);
	return s;
}
void sis_bitstream_destroy(s_sis_bit_stream *s_)
{
	sis_free(s_);
}
void sis_bitstream_link(s_sis_bit_stream *s_, uint8 *in_, uint32 inLen_, uint32 initPos_)
{
	if (initPos_ <= inLen_ * 8) { initPos_ = 0; }
	s_->m_stream = in_;
	s_->m_bit_size_i = inLen_ * 8;
	s_->m_nowpos_i = initPos_;
	s_->m_nowcode_p = NULL;
	s_->m_nowcode_count = 0;
}
int sis_bitstream_getbytelen(s_sis_bit_stream *s_)
{
	return (s_->m_nowpos_i + 7) / 8;
}
int sis_bitstream_get_nomove(s_sis_bit_stream *s_, int nBit, uint32 *out)
{
	*out = 0;
	if (nBit<0 || nBit>32 || s_->m_bit_size_i <= s_->m_nowpos_i) { return 0; }
	else if (nBit>0)
	{
		if ((uint32)nBit > s_->m_bit_size_i - s_->m_nowpos_i)
		{
			nBit = s_->m_bit_size_i - s_->m_nowpos_i;
		}
		int nPos = s_->m_nowpos_i / 8;
		*out = s_->m_stream[nPos++];
		int nGet = 8 - (s_->m_nowpos_i % 8);
		if (nGet != 8)
		{
			*out &= (0xFF >> (8 - nGet));
		}
		int nLeft = nBit - nGet;
		while (nLeft>0)
		{
			if (nLeft >= 8)
			{
				*out = (*out << 8) | s_->m_stream[nPos++];
				nLeft -= 8;
			}
			else
			{
				*out = (*out << nLeft) | (s_->m_stream[nPos++] >> (8 - nLeft));
				nLeft = 0;
			}
		}
		if (nLeft < 0){
			*out >>= -nLeft;
		}
	}
	return nBit;
}
uint32 sis_bitstream_get(s_sis_bit_stream *s_, int nBit)
{
	uint32 dw = 0;
	int nMove = sis_bitstream_get_nomove(s_, nBit, &dw);
	s_->m_nowpos_i += nMove;
	return dw;
}
void sis_bitstream_move(s_sis_bit_stream *s_, int byte)
{
	s_->m_nowpos_i += byte * 8;
}
int sis_bitstream_get_string(s_sis_bit_stream *s_, char* out_, int outLen_)
{
	uint32 dw = 0;
	int i;
	for (i = 0; i<outLen_; i++)
	{
		dw = sis_bitstream_get(s_, 8);
		if (!dw){	break;	}
		out_[i] = (char)dw;
	}
	for (; i < outLen_; i++)
	{
		out_[i] = '\0';
	}
	return i;
}

int sis_bitstream_moveto(s_sis_bit_stream *s_, int nPos)
{
	s_->m_nowpos_i += nPos;
	if (s_->m_nowpos_i<0)
	{
		s_->m_nowpos_i = 0;
	}
	else if (s_->m_nowpos_i>s_->m_bit_size_i)
	{
		s_->m_nowpos_i = s_->m_bit_size_i;
	}
	return s_->m_nowpos_i;
}

int sis_bitstream_put(s_sis_bit_stream *s_, uint32 dw, int nBit)
{
	int out = 0;
	if (nBit>0)
	{
		if (s_->m_nowpos_i + nBit <= s_->m_bit_size_i)
		{
			dw <<= 32 - nBit;
			uint8* pHighByte = (uint8*)&dw + 3;

			out = nBit;
			int nLastBit = s_->m_nowpos_i % 8;
			if (nLastBit)
			{
				int nSpace = 8 - nLastBit;
				uint8 c = s_->m_stream[s_->m_nowpos_i / 8] & (0xFF << nSpace);
				int nAdd = sis_min(nBit, nSpace);
				c |= (dw >> (32 - nSpace)) & (0xFF >> nLastBit);
				s_->m_stream[s_->m_nowpos_i / 8] = c;
				nBit -= nSpace;
				dw <<= nSpace;
				s_->m_nowpos_i += nAdd;
			}

			if (nBit)
			{
				while (nBit>0)
				{
					s_->m_stream[s_->m_nowpos_i / 8] = *pHighByte;
					s_->m_nowpos_i += sis_min(8, nBit);
					nBit -= 8;
					dw <<= 8;
				}
			}
		}
		else{
			return -1;
		}
	}
	return out;
}

int sis_bitstream_put_string(s_sis_bit_stream *s_, const char * str)
{
	int nLen = (int)strlen(str);
	for (int i = 0; i < nLen; i++)
	{
		sis_bitstream_put(s_, str[i], 8);
	}
	sis_bitstream_put(s_, 0, 8);
	return (nLen + 1) * 8;
}
int sis_bitstream_put_buffer(s_sis_bit_stream *s_, char *in, size_t len)
{
	// for (int i = 0; i <= (int)len; i++)
	for (int i = 0; i < (int)len; i++)
	{
		sis_bitstream_put(s_, in[i], 8);
	}
	return (int)len  * 8;
	// 2019-3-12 +1会多出来一个0结束符
	// return (int)(len + 1) * 8;
}


const s_sis_bit_code* sis_bitstream_find_original_data(s_sis_bit_stream *s_)
{
	const s_sis_bit_code* pRet = NULL;
	if(s_->m_nowcode_p)
	{
		for (int i = 0; i<s_->m_nowcode_count; i++)
		{
			if (s_->m_nowcode_p[i].m_method_c == 'M')
			{
				pRet = s_->m_nowcode_p + i;
				break;
			}
		}
	}
	return pRet;
}

const s_sis_bit_code* sis_bitstream_find_string_data(s_sis_bit_stream *s_)
{
	const s_sis_bit_code* pRet = NULL;
	if(s_->m_nowcode_p)
	{
		for (int i = 0; i<s_->m_nowcode_count; i++)
		{
			if (s_->m_nowcode_p[i].m_method_c == 's')
			{
				pRet = s_->m_nowcode_p + i;
				break;
			}
		}
	}
	return pRet;
}

const s_sis_bit_code* sis_bitstream_encode_find_match(s_sis_bit_stream *s_, uint32 *out_)
{
	const s_sis_bit_code* pRet = NULL;
	int i,j;
	if (s_->m_nowcode_p)
	{
		for (i = 0; i<s_->m_nowcode_count; i++)
		{
			const s_sis_bit_code* pCur = s_->m_nowcode_p + i;
			uint32 dw = *out_;
			if(pCur->m_method_c=='b')
			{
				if (*out_ & 0x80000000)	//负数
				{
					dw += pCur->m_data_offset_dw;
					uint32 dwMask = (0xFFFFFFFF<<(pCur->m_data_dw-1));
					if ((dw&dwMask) == dwMask)
					{
						pRet = pCur;
					}
				}
				else
				{
					dw -= pCur->m_data_offset_dw;
					if ((dw >> (pCur->m_data_dw - 1)) == 0)
					{
						pRet = pCur;
					}
				}
			}
			else if(pCur->m_method_c=='Z')
			{
				if(dw && (dw%10==0))
				{
					for(j=0;j<4;j++)
					{
						if (dw % 10)
						{
							break;
						}
						dw /= 10;
					}
					dw -= pCur->m_data_offset_dw;
					if((dw>>pCur->m_data_dw)==0)
					{
						dw = (dw<<2)|(j-1);
						pRet = pCur;
					}
				}
			}
			else if(pCur->m_method_c=='m')
			{
				if (*out_ & 0x80000000)	//负数
				{
					dw += pCur->m_data_offset_dw;
					uint32 dwMask = (0xFFFFFFFF<<pCur->m_data_dw);
					if ((dw&dwMask) == dwMask)
					{
						pRet = pCur;
					}
				}
			}
			else
			{
				dw -= pCur->m_data_offset_dw;
				switch (pCur->m_method_c)
				{
				case 'B':
					if ((dw >> pCur->m_data_dw) == 0)
					{
						pRet = pCur;
					}
					break;
				case 'S':
					if ((dw&(0xFFFFFFFF >> (32 - HIWORD(pCur->m_data_dw)))) == 0 &&
						(dw >> (HIWORD(pCur->m_data_dw) + LOWORD(pCur->m_data_dw))) == 0)
					{
						pRet = pCur;
					}
					break;
				case 'E':
					if (dw == pCur->m_data_dw)
					{
						pRet = pCur;
					}
					break;
				case 'P':
					for (j = 0; j<(int)pCur->m_data_dw; j++)
					{
						if ((dw >> j) & 1)
						{
							if ((dw >> j) == 1)
							{
								dw = j;		//返回位置
								pRet = pCur;
							}
							else
							{
								break;
							}
						}
					}
					break;
				case 'D':
				case 'M':
				case 's':
					pRet = pCur;
					break;
				default:
					break;
				}
			}
			if (pRet)
			{
				*out_ = dw;
				break;
			}
		}
	}
	return pRet;
}

const s_sis_bit_code* sis_bitstream_decode_find_match(s_sis_bit_stream *s_, uint32 *out_)
{
	const s_sis_bit_code* pCode = NULL;
	*out_ = 0;
	if (s_->m_nowcode_p)
	{
		uint32 dwNextCode = 0;
		int nCodeLen = sis_bitstream_get_nomove(s_, 16, &dwNextCode);
		int i;
		for (i = 0; i<s_->m_nowcode_count; i++)
		{
			const s_sis_bit_code* pCur = s_->m_nowcode_p + i;
			if (pCur->m_sign_w == (dwNextCode >> (nCodeLen - pCur->m_len_by)))	//找到
			{
				pCode = pCur;
				break;
			}
		}
		if (pCode)
		{
			sis_bitstream_moveto(s_, pCode->m_len_by);
			if (pCode->m_data_len_by)
			{
				*out_ = sis_bitstream_get(s_, pCode->m_data_len_by);
			}
			switch (pCode->m_method_c)
			{
			case 'B':
			case 'D':
			case 'M':
			case 's':
				break;
			case 'b':
				if (*out_&(1 << (pCode->m_data_len_by - 1)))	//负数
				{
					*out_ |= (0xFFFFFFFF << pCode->m_data_len_by);
				}
				break;
			case 'm':
				*out_ |= (0xFFFFFFFF << pCode->m_data_len_by);
				break;
			case 'S':
				*out_ <<= HIWORD(pCode->m_data_dw);
				break;
			case 'E':
				*out_ = pCode->m_data_dw;
				break;
			case 'Z':
			{
						int nExp = *out_ & 3;
						*out_ >>= 2;
						*out_ += pCode->m_data_offset_dw;
						for (i = 0; i <= nExp; i++)
						{
							*out_ *= 10;
						}
			}
				break;
			case 'P':
				*out_ = (1 << *out_);
				break;
			default:
				break;
			}
			if ((*out_ & 0x80000000) && (pCode->m_method_c == 'b'))
			{
				*out_ -= pCode->m_data_offset_dw;
			}
			else if (!_is_original_data(pCode) && pCode->m_method_c != 'Z' && pCode->m_method_c != 's')
			{
				*out_ += pCode->m_data_offset_dw;
			}
		}
		else
		{
			return NULL;
		}
	}
	return pCode;
}

int sis_bitstream_encode_uint32(s_sis_bit_stream *s_, uint32 dwData, const uint32 *pdwLastData, bool bReverse)
{
	int nRet = 0;
	uint32 dw = dwData;
	if (pdwLastData)
	{
		if (bReverse)
		{
			dw = *pdwLastData - dw;
		}
		else
		{
			dw -= *pdwLastData;
		}
	}
	const s_sis_bit_code* pCode = sis_bitstream_encode_find_match(s_, &dw);
	if (pCode)
	{
		nRet = sis_bitstream_put(s_, pCode->m_sign_w, pCode->m_len_by);
		if (_is_original_data(pCode))
		{
			if (pCode->m_method_c == 'D')
			{
				nRet += sis_bitstream_put(s_, dwData, pCode->m_data_len_by);
			}
			else{
				return -1;
			}
		}
		else if (pCode->m_data_len_by)
		{
			nRet += sis_bitstream_put(s_, dw, pCode->m_data_len_by);
		}
	}
	else
	{
		return -1;
	}
	return nRet;
}

int sis_bitstream_encode_zint32(s_sis_bit_stream *s_, zint32 mData, const zint32* pmLastData)
{
	int nRet = 0;
	uint32 dw = 0;
	bool bDirectOrg = FALSE;
	int64 n = zint32_to_int64(mData);
	if (pmLastData)
	{
		n -= zint32_to_int64(*pmLastData);
	}
	if (n <= ZINT32_MAXBASE && n >= -1 * ZINT32_MAXBASE)
	{
		dw = (uint32)n;
	}
	else
	{
		bDirectOrg = TRUE;
	}
	const s_sis_bit_code* pCode = NULL;
	if (bDirectOrg)
	{
		pCode = sis_bitstream_find_original_data(s_);
	}
	else
	{
		pCode = sis_bitstream_encode_find_match(s_, &dw);
	}
	if (pCode)
	{
		nRet = sis_bitstream_put(s_, pCode->m_sign_w, pCode->m_len_by);
		if (_is_original_data(pCode))
		{
			if (pCode->m_method_c == 'M')
			{
				nRet += sis_bitstream_put(s_, zint32_out_uint32(mData), pCode->m_data_len_by);
			}
			else{
				return -1;
			}
		}
		else if (pCode->m_data_len_by)
		{
			nRet += sis_bitstream_put(s_, dw, pCode->m_data_len_by);
		}
	}
	else
	{
		return -1;
	}
	return nRet;
}

int sis_bitstream_encode_float(s_sis_bit_stream *s_, float f, bool check0)
{
	int nRet = 0;
	uint32* pdw = (uint32*)&f;

	if (check0)
	{
		if (f == 0.0f)
		{
			nRet = sis_bitstream_put(s_, 0, 1);
		}
		else
		{
			sis_bitstream_put(s_, 1, 1);
			nRet = sis_bitstream_put(s_, *pdw, 32) + 1;
		}
	}
	else
	{
		nRet = sis_bitstream_put(s_, *pdw, 32);
	}

	return nRet;
}

int sis_bitstream_encode_string(s_sis_bit_stream *s_, const char * strData, uint32 *base)
{
	int nRet = 0;
	uint32 dw = 0, dwCode = 0;
	int nLen = (int)strlen(strData);
	int i;
	for (i = 0; i<nLen; i++)
	{
		if (!isdigit(strData[i]))
		{
			break;
		}
	}
	const s_sis_bit_code* pCode = NULL;
	if (i == nLen)
	{
		dwCode = atoi(strData);
		dwCode |= (nLen << 24);
		dw = dwCode - *base;
		pCode = sis_bitstream_encode_find_match(s_, &dw);
	}
	else
	{
		pCode = sis_bitstream_find_string_data(s_);
	}
	if (pCode)
	{
		nRet = sis_bitstream_put(s_, pCode->m_sign_w, pCode->m_len_by);
		if (_is_original_data(pCode))
		{
			if (pCode->m_method_c == 'D')
			{
				nRet += sis_bitstream_put(s_, dwCode, pCode->m_data_len_by);
			}
			else
			{
				return -1;
			}
		}
		else if (pCode->m_method_c == 's')
		{
			nRet += sis_bitstream_put_string(s_, strData);
		}
		else if (pCode->m_data_len_by)
		{
			nRet += sis_bitstream_put(s_, dw, pCode->m_data_len_by);
		}
	}
	else
	{
		return -1;
	}
	*base = dwCode;
	return nRet;
}

uint32 sis_bitstream_decode_uint32(s_sis_bit_stream *s_, const uint32* base, bool bReverse)
{
	uint32 dw = 0;
	const s_sis_bit_code* pCode = sis_bitstream_decode_find_match(s_, &dw);
	if (pCode)
	{
		if (!_is_original_data(pCode) && base)
		{
			if (bReverse)
			{
				dw = *base - dw;
			}
			else
			{
				dw += *base;
			}
		}
	}
	return dw;
}

void sis_bitstream_decode_zint32(s_sis_bit_stream *s_, const zint32* base, zint32 *out, bool bReverse)
{
	int64_to_zint32(0, out);
	uint32 dw = 0;
	const s_sis_bit_code* pCode = sis_bitstream_decode_find_match(s_, &dw);
	if (pCode)
	{
		if (pCode->m_method_c == 'M')
		{
			uint32_out_zint32(dw, out);
		}
		else
		{
			//	LOG(1)("end compress _is_original_data=%d \n", pCode->_is_original_data());
			if (!_is_original_data(pCode) && base)
			{
				int64 dLast = zint32_to_int64(*base);
				int64 dThis = (int)dw;
				if (bReverse)
				{
					int64_to_zint32(dLast - dThis, out);
				}
				else
				{
					int64_to_zint32(dLast + dThis, out);
				}
			}
			else{
				int64_to_zint32(dw, out);
			}
		}
	}
}

float sis_bitstream_decode_float(s_sis_bit_stream *s_, bool check0)
{
	float fRet = 0.0f;
	uint32 b = TRUE;
	if (check0)
	{
		b = sis_bitstream_get(s_, 1);
	}
	if (b)
	{
		uint32* pdw = (uint32*)&fRet;
		*pdw = sis_bitstream_get(s_, 32);
	}
	return fRet;
}

uint32 sis_bitstream_decode_string(s_sis_bit_stream *s_, char* in_, int inLen_, uint32 *base_)						//如果解出数字Label，则返回该数字，否则返回0xFFFFFFFF,更新dwLastData
{
	uint32 dw = 0;
	const s_sis_bit_code* pCode = sis_bitstream_decode_find_match(s_, &dw);
	if (pCode)
	{
		if (pCode->m_method_c == 's')
		{
			*base_ = 0;
			dw = 0xFFFFFFFF;
			sis_bitstream_get_string(s_, in_, inLen_);
		}
		else
		{
			if (!_is_original_data(pCode))
			{
				dw += *base_;
			}
			*base_ = dw;
			char fmt[16];
			sprintf(fmt, "%%0%dd", dw >> 24);
			sprintf(in_, fmt, dw & 0xFFFFFF);
		}
	}
	return dw;
}

#if 0

	int GetBitLength(){ return m_nowpos_i; }						//得到当前Bit流Bit数
	int GetByteLength(){ return (m_nowpos_i + 7) / 8; }				//得到当前Bit流Byte数
	int GetCurPos(){ return m_nowpos_i; }
	int GetBitSize(){ return m_bit_size_i; }

	virtual uint32 Get(int nBit);				//取得nBit数据，最多32Bit
	virtual int GetNoMove(int nBit, uint32& dw);	//不移动内部指针，取得nBit数据，最多32Bit，返回实际取得的长度
	int GetString(char* pBuf, int inLen_);

	virtual int Put(uint32 dw, int nBit);			//向当前位置添加nBit数据
	int PutBuffer(char *in, size_t len);
	int PutString(const char * str);

	int Move(int nBit){ return MoveTo(m_nowpos_i + nBit); }		//移动内部指针，nBit可以为负，返回新的位置
	int MoveTo(int nPos);					//移动内部指针到nPos处

	void SaveCurrentPos(){ m_savepos_i = m_nowpos_i; }		//保存当前位置
	int  RestoreToSavedPos(){ return MoveTo(m_savepos_i); }		//回卷到上一次保存的位置

	void SetBitCode(const BITCODE* pCodes, int nNumCode){ m_nowcode_p = pCodes; m_nowcode_count = nNumCode; }

	int EncodeData(uint32 dwData, const uint32* pdwLastData = NULL, bool bReverse = FALSE);	//根据情况编码，返回编码Bit长度
	int EncodeData(zint32 mData, const zint32* pmLastData = NULL);	//根据情况编码，返回编码Bit长度
	int EncodeFloat(float f, bool bCheck0 = FALSE);				//返回编码Bit长度,不需要编码
	int EncodeStringData(const char * strData, uint32& dwLastData);							//根据情况编码，返回编码Bit长度,更新dwLastData

	uint32 DecodeData(const uint32* pdwLastData = NULL, bool bReverse = FALSE);
	zint32 DecodeMWordData(const zint32* pmLastData = NULL, bool bReverse = FALSE);
	float DecodeFloat(bool bCheck0 = FALSE);
	uint32 DecodeStringData(char* pBuf, int nBufSize, uint32& dwLastData);						//如果解出数字Label，则返回该数字，否则返回0xFFFFFFFF,更新dwLastData

	int GetNumZero(uint32& dw);
	int GetNumZero(zint32& m);

protected:
	const BITCODE* EncodeFindMatch(uint32& dw);							//找到配合的编码,编码后的数据通过dw带回
	const BITCODE* DecodeFindMatch(uint32& dw);							//找到配合的编码，并通过dw返回

	const BITCODE* FindOrgMData();
	const BITCODE* FindStringData();
};

#define SETBITCODE(x)	SetBitCode(x,sizeof(x)/sizeof(x[0]))

#ifdef _DEBUG
float GetAvgCodeLen(BITCODE* pCode, int nNum);
#define AVGCODELEN(x)	GetAvgCodeLen(x,sizeof(x)/sizeof(x[0]))
#endif

#ifdef _DEBUG
void PrintOptimizedCode(LPCSTR name, BITCODE* pCodes, int nNum);
#define PRINTOPTCODE(y,x)	PrintOptimizedCode(x,y,sizeof(y)/sizeof(y[0]))
#endif




int c_bit_stream::GetNumZero(uint32& dw)
{
	int nRet = 0;
	if (dw)
	{
		int n = dw;
		while (n % 10 == 0)
		{
			n /= 10;
			nRet++;
		}
		dw = n;
	}
	return nRet;
}

int c_bit_stream::GetNumZero(zint32& m)
{
	int64 n = m;
	int nRet = 0;
	if (n)
	{
		while (n % 10 == 0)
		{
			n /= 10;
			nRet++;
		}
	}
	m = n;
	return nRet;
}

#ifdef _DEBUG
//_DEBUG定义后会产生warning LNK4098: 默认库“MSVCRT”与其他库的使用冲突；请使用 /NODEFAULTLIB:library错误
float GetAvgCodeLen(BITCODE* pCode,int nNum)
{
	float f = 0.0f;
	int n = 0;
	for(int i=0;i<nNum;i++)  
	{
		f += (pCode[i].m_len_by+pCode[i].m_data_len_by)*pCode[i].m_dwCodeCount;
		n += pCode[i].m_dwCodeCount;
	}
	return f/n;
}
#include "cllog.h"
void PrintOptimizedCode(LPCSTR strName,BITCODE* pCodes,int nNum)
{
	char s[256];
	uint32 nCount = 0;
	int i;
	double accu = 0;
	for(i=0;i<nNum;i++)
	{
		nCount += pCodes[i].m_dwCodeCount;
		accu += pCodes[i].m_dwCodeCount*(pCodes[i].m_len_by+pCodes[i].m_data_len_by);
	}
//	LOG(30)("-------%s------count=%d,avgLen=%.2fBit\n",strName,nCount,accu/nCount);
	if(nCount==0)
		nCount = 1;
	for(i=0;i<nNum;i++)
	{
//		sprintf(s,"%c:CodeLen=%d,DataLen=%d,Percent=%d%%\n",pCodes[i].m_method_c,pCodes[i].m_len_by,pCodes[i].m_data_len_by,pCodes[i].m_dwCodeCount*1000/nCount);
		LOG(20)(s);
	}
}

#endif

#endif