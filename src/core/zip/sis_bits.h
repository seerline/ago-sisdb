#ifndef _SIS_BITS_H
#define _SIS_BITS_H

#include "sis_core.h"
#include "sis_dynamic.h"
#include "sis_sds.h"
#include "sis_math.h"

/////////////////////////////////////
// *** 专用于网络结构化数据压缩传输 *** //
/////////////////////////////////////

// 小整数 正整数 0 - 0xFF
// 00  --> 0       --> 0
// 01  --> 1       --> 1
// 10  --> 0000-1111   --> 16+2  = 0~17
// 110 --> 00000-11111 --> 32+18 = 0~49
// 111 --> 00000000-11111111 --> 256+50 = 0~305

// 短正整数 正整数 0 - 0xFFFF 
// 00  --> 0 --> 0
// 01  --> 1 --> 1
// 10  --> 00-FF     --> 256+2 = 0~257
// 110 --> 000-FFF   --> 4096+258 = 0~4353 
// 111 --> 0000-FFFF --> 65536+4354 = 0~69889 

// 索引整数 正整数 0 - 0xFFFFFFFF
// 00 --> 0-F
// 01 --> 10-FF
// 100 --> 100-FFF 
// 101 --> 1000-FFFF  
// 110 --> 10000-FFFFFF 
// 111 --> 1000000-FFFFFFFF  

// 递增整数 带符号位
// 0表示和前值一样
// 10 --> 0-F
// 11 + 000-010
//    000 :  -FF…FF
//    001 :  -FFF…FFF
//    010 :  -FFFF…FFFF
//    011 :  -FFFFF…FFFFF
// 111 + 000-010
//    000 :  -FFFFFF…FFFFFF
//    001 :  -FFFFFFF…FFFFFFF
//    010 :  -FFFFFFFF…FFFFFFFF
//    011 :  -FFFFFFFFF…FFFFFFFFF
//    100 :  -FFFFFFFFFF…FFFFFFFFFF
//    101 :  -FFFFFFFFFFFF…FFFFFFFFFFFF
//    110 :  -FFFFFFFFFFFFFF…FFFFFFFFFFFFFF
//    111 :  -7FFFFFFFFFFFFFFF…7FFFFFFFFFFFFFFF

// 浮点数
// 用小数位乘后得到整数压缩

// 字符串 
// 0 表示和前值一样
// 1 + size长度+字符 ….

#pragma pack(push,1)

// 压缩数据流
typedef struct s_sis_bits_stream{
	uint8     *lpstream;	 // 目标缓冲区仅仅是指针 外部要有实体 并保证实体大小<=maxsize 
	uint32     maxsize;      // 缓冲区大小，Bit

	uint32     currpos;      // 当前操作位置
	uint32     savepos;	     // 上一次保存的位置 Bit
}s_sis_bits_stream;

#pragma pack(pop)


////////////////////////////////////////////////////////
// s_sis_bits_stream
////////////////////////////////////////////////////////
static inline s_sis_bits_stream *sis_bits_stream_create(uint8 *in_, size_t ilen_)
{ 
	s_sis_bits_stream *s = SIS_MALLOC(s_sis_bits_stream, s); 
    s->lpstream = in_;
    s->maxsize = ilen_ * 8;
    s->currpos = 0;
    s->savepos = 0;
	return s;
}
static inline void sis_bits_stream_destroy(s_sis_bits_stream *s_)
{
    if (s_)
    {
    	sis_free(s_);
    }
}
static inline void sis_bits_stream_init(s_sis_bits_stream *s_)
{
    // 不清理结构信息 
    s_->lpstream = NULL;
    s_->maxsize = 0;
    s_->currpos = 0;
    s_->savepos = 0;
}
static inline void sis_bits_stream_link(s_sis_bits_stream *s_, uint8 *in_, size_t ilen_)
{
    s_->lpstream = in_;
    s_->maxsize = ilen_ * 8;
    s_->currpos = 0;
    s_->savepos = 0;
}
static inline size_t sis_bits_stream_getbytes(s_sis_bits_stream *s_)
{
	return (s_->currpos + 7) / 8;
}

// 直接定位到新的位置
static inline int sis_bits_stream_moveto(s_sis_bits_stream *s_, int bitpos_)	
{
    // s_->currpos = bitpos_;
    s_->currpos = bitpos_ < 0 ? 0 : bitpos_ > (int)s_->maxsize ? s_->maxsize : bitpos_;
    return s_->currpos;
}
//移动内部指针bits_位,可以为负，返回新的位置
static inline int sis_bits_stream_move(s_sis_bits_stream *s_, int bits_)
{ 
    return sis_bits_stream_moveto(s_, s_->currpos + bits_); 
}
static inline int sis_bits_stream_move_bytes(s_sis_bits_stream *s_, int bytes_)
{
    return sis_bits_stream_moveto(s_, s_->currpos + bytes_ * 8); 
}
//保存当前位置
static inline void sis_bits_stream_savepos(s_sis_bits_stream *s_)
{ 
    s_->savepos = s_->currpos; 
}		
//回卷到上一次保存的位置
static inline int  sis_bits_stream_restore(s_sis_bits_stream *s_)
{ 
    return sis_bits_stream_moveto(s_, s_->savepos); 
}		

// static inline uint64 sis_bits_stream_get(s_sis_bits_stream *s_, int bits_)
// {
//     uint64 u64 = 0;
//     if (bits_ <= 0 || bits_ > 64 || s_->maxsize <= s_->currpos)
//     {
//         return 0;
//     }
//     else
//     {
//         if ((uint64)bits_ > s_->maxsize - s_->currpos)
//         {
//             bits_ = s_->maxsize - s_->currpos;
//         }
//         int curpos = s_->currpos / 8;
//         u64 = s_->lpstream[curpos++];
//         int decpos = 8 - (s_->currpos % 8);
//         if (decpos != 8)
//         {
//             u64 &= (0xFF >> (8 - decpos));
//         }
//         int incpos = bits_ - decpos;
//         while (incpos > 0)
//         {
//             if (incpos >= 8)
//             {
//                 u64 = (u64 << 8) | s_->lpstream[curpos++];
//                 incpos -= 8;
//             }
//             else
//             {
//                 u64 = (u64 << incpos) | (s_->lpstream[curpos++] >> (8 - incpos));
//                 incpos = 0;
//             }
//         }
//         if (incpos < 0)
//         {
//             u64 >>= -incpos;
//         }
//     }
//     s_->currpos += bits_;
//     return u64;
// }

static inline uint64 sis_bits_stream_get(s_sis_bits_stream *s_, int bits_)
{
	// if (s_->currpos + bits_ > s_->maxsize)
	// {
	// 	bits_ = s_->maxsize - s_->currpos;
	// }
    uint64 u64 = 0;
	int curpos = s_->currpos / 8;
	u64 = s_->lpstream[curpos++];
	int decpos = 8 - (s_->currpos % 8);
	if (decpos != 8)
	{
		u64 &= (0xFF >> (8 - decpos));
	}
	int incpos = bits_ - decpos;
	while (incpos > 0)
	{
		if (incpos >= 8)
		{
			u64 = (u64 << 8) | s_->lpstream[curpos++];
			incpos -= 8;
		}
		else
		{
			u64 = (u64 << incpos) | (s_->lpstream[curpos++] >> (8 - incpos));
			incpos = 0;
		}
	}
	if (incpos < 0)
	{
		u64 >>= -incpos;
	}
    s_->currpos += bits_;
    return u64;
}

static inline int sis_bits_stream_put(s_sis_bits_stream *s_, uint64 in_, int bits_)
{
    if (bits_ > 0)
    {
        in_ <<= 64 - bits_;
        uint8 *phigh_byte = (uint8 *)&in_ + 7;
        int offset = bits_;
        int lastbit = s_->currpos % 8;
        if (lastbit)
        {
            int space = 8 - lastbit;
            uint8 c = s_->lpstream[s_->currpos / 8] & (0xFF << space);
            int incr = sis_min(offset, space);
            c |= (in_ >> (64 - space)) & (0xFF >> lastbit);
            s_->lpstream[s_->currpos / 8] = c;
            offset -= space;
            in_ <<= space;
            s_->currpos += incr;
        }
        if (offset)
        {
            while (offset > 0)
            {
                s_->lpstream[s_->currpos / 8] = *phigh_byte;
                s_->currpos += sis_min(8, offset);
                offset -= 8;
                in_ <<= 8;
            }
        }
    }
    return bits_;
}
static inline int sis_bits_stream_put_buffer(s_sis_bits_stream *s_, char *in_, size_t bytes_)
{
	for (int i = 0; i < (int)bytes_; i++)
	{
		sis_bits_stream_put(s_, in_[i], 8);
	}
	return (int)bytes_  * 8;
}
static inline int sis_bits_stream_put_sint(s_sis_bits_stream *s_, uint32 in_)
{
// 小整数 正整数 0 - 0xFF
// 00  --> 0       --> 0
// 01  --> 1       --> 1
// 10  --> 0000-1111   --> 16+2  = 0~17
// 110 --> 00000-11111 --> 32+18 = 0~49
// 111 --> 00000000-11111111 --> 256+50 = 0~305

    uint32 dw = in_;
    if (dw < 2)
    {   
        sis_bits_stream_put(s_, dw, 2);
        return 0;
    }
    dw -= 2;
    if (dw < 16)
    {
        sis_bits_stream_put(s_,  2, 2);
        sis_bits_stream_put(s_, dw, 4);
        return 0;
    }
    dw -= 16;
    if (dw < 32)
    {
        sis_bits_stream_put(s_,  6, 3);
        sis_bits_stream_put(s_, dw, 5);
        return 0;
    }
    dw -= 32;
    if (dw < 256)
    {
        sis_bits_stream_put(s_,  7, 3);
        sis_bits_stream_put(s_, dw, 8);
        return 0;
    }
    return -1; 
}
static inline int sis_bits_stream_put_short(s_sis_bits_stream *s_, uint32 in_)
{
// 短正整数 正整数 0 - 0xFFFF 
// 00  --> 0 --> 0
// 01  --> 1 --> 1
// 10  --> 00-FF     --> 256+2 = 0~257
// 110 --> 000-FFF   --> 4096+258 = 0~4353 
// 111 --> 0000-FFFF --> 65536+4354 = 0~69889 
    uint32 dw = in_;
    if (dw < 2)
    {   
        sis_bits_stream_put(s_, dw, 2);
        return 0;
    }
    dw -= 2;
    if (dw < 256)
    {
        sis_bits_stream_put(s_,  2, 2);
        sis_bits_stream_put(s_, dw, 8);
        return 0;
    }
    dw -= 256;
    if (dw < 4096)
    {
        sis_bits_stream_put(s_,  6, 3);
        sis_bits_stream_put(s_, dw, 12);
        return 0;
    }
    dw -= 4096;
    if (dw < 65536)
    {
        sis_bits_stream_put(s_,  7, 3);
        sis_bits_stream_put(s_, dw, 16);
        return 0;
    }
    return -1; 
}
static inline int sis_bits_stream_put_uint(s_sis_bits_stream *s_, uint32 in_)
{
// 索引整数 正整数 0 - 0xFFFFFFFF
// 00 --> 0-F
// 01 --> 10-FF
// 100 --> 100-FFF 
// 101 --> 1000-FFFF  
// 110 --> 10000-FFFFFF 
// 111 --> 1000000-FFFFFFFF  
    int offset = 0;
    uint64 dw = in_;
    if (dw & 0xFFFFFF00)
    {
        if (dw & 0xFF000000)
        {
            dw |= 0x700000000;
            sis_bits_stream_put(s_, dw, 35);
        }
        else if (dw & 0xFF0000)
        {
            dw |= 0x6000000;
            sis_bits_stream_put(s_, dw, 27);
        }
        else if (dw & 0xFF00)
        {
            dw |= 0x50000;
            sis_bits_stream_put(s_, dw, 19);
        }
        else
        {
            dw |= 0x4000;
            sis_bits_stream_put(s_, dw, 15);
        }
    }
    else
    {
        if (dw & 0xF0)
        {
            dw |= 0x100;
            sis_bits_stream_put(s_, dw, 10);
        }
        else
        {
            sis_bits_stream_put(s_, dw, 6);
        }        
    } 
    return offset; 
}
// 带符号长整数
static inline int sis_bits_stream_put_int(s_sis_bits_stream *s_, int64 in_)
{
// 递增整数 带符号位
// 0表示和前值一样
// 10 --> 0-F
// 11 + 000-010
//    000 :  -FF…FF
//    001 :  -FFF…FFF
//    010 :  -FFFF…FFFF
//    011 :  -FFFFF…FFFFF
// 111 + 000-010
//    000 :  -FFFFFF…FFFFFF
//    001 :  -FFFFFFF…FFFFFFF
//    010 :  -FFFFFFFF…FFFFFFFF
//    011 :  -FFFFFFFFF…FFFFFFFFF
//    100 :  -FFFFFFFFFF…FFFFFFFFFF
//    101 :  -FFFFFFFFFFFF…FFFFFFFFFFFF
//    110 :  -FFFFFFFFFFFFFF…FFFFFFFFFFFFFF
//    111 :  -7FFFFFFFFFFFFFFF…7FFFFFFFFFFFFFFF
    int offset = 0;
    int64 dw = in_ < 0 ? -1 * in_ : in_;
    if (in_ < 0)
    {
        sis_bits_stream_put(s_, 1, 1);
    }
    else
    {
        sis_bits_stream_put(s_, 0, 1);
    }   
    if (dw & 0x7FFFFFFFFFF00000)
    {
        if (dw & 0x7F00000000000000)
        {
            sis_bits_stream_put(s_, 0x3F, 6);
            sis_bits_stream_put(s_, dw, 63);
        }
        else if (dw & 0xFF000000000000)
        {
            dw |= 0x3E00000000000000;
            sis_bits_stream_put(s_, dw, 62);
        }
        else if (dw & 0xFF0000000000)
        {
            dw |= 0x3D000000000000;
            sis_bits_stream_put(s_, dw, 54);
        }
        else if (dw & 0xFF00000000)
        {
            dw |= 0x3C0000000000;
            sis_bits_stream_put(s_, dw, 46);
        }
        else if (dw & 0xF00000000)
        {
            dw |= 0x3B000000000;
            sis_bits_stream_put(s_, dw, 42);
        }
        else if (dw & 0xF0000000)
        {
            dw |= 0x3A00000000;
            sis_bits_stream_put(s_, dw, 38);
        }
        else if (dw & 0xF000000)
        {
            dw |= 0x390000000;
            sis_bits_stream_put(s_, dw, 34);
        }
        else
        {
            dw |= 0x38000000;
            sis_bits_stream_put(s_, dw, 30);
        }
    }
    else if (dw & 0xFFFF0)
    {
        if (dw & 0xF0000)
        {
            dw |= 0x1B00000;
            sis_bits_stream_put(s_, dw, 25);
        }
        else if (dw & 0xF000)
        {
            dw |= 0x1A0000;
            sis_bits_stream_put(s_, dw, 21);
        }
        else if (dw & 0xF00)
        {
            dw |= 0x19000;
            sis_bits_stream_put(s_, dw, 17);
        }
        else
        {
            dw |= 0x1800;
            sis_bits_stream_put(s_, dw, 13);
        }
    }
    else
    {
        if (dw & 0xF)
        {
            dw |= 0x20;
            sis_bits_stream_put(s_, dw, 6);
        }
        else
        {
            sis_bits_stream_put(s_, 0, 1);
        }        
    }
    return offset; 
}
static inline int sis_bits_stream_put_incr_int(s_sis_bits_stream *s_, int64 in_, int64 ago_)
{
    return sis_bits_stream_put_int(s_, (in_ - ago_));
}

static inline int sis_bits_stream_put_float(s_sis_bits_stream *s_, double in_, int dot_)
{
    int64 dw = in_ * pow(10, dot_);
    return sis_bits_stream_put_int(s_, dw);
}
static inline int sis_bits_stream_put_incr_float(s_sis_bits_stream *s_, double in_, double ago_, int dot_)
{
    double m = pow(10, dot_);
    return sis_bits_stream_put_int(s_, (in_ - ago_) * m);
}

static inline int sis_bits_stream_put_chars(s_sis_bits_stream *s_, char *in_, size_t ilen_)
{
// 0 表示和前值一样
// 1 + size长度+字符
    // sis_out_binary("...", sis_memory(s_->lpstream), 16);
    int offset = 0;
    if (ilen_ > 0 && in_)
    {
        sis_bits_stream_put(s_, 1, 1);
        // sis_out_binary("...", sis_memory(s_->lpstream), 16);
        sis_bits_stream_put_uint(s_, ilen_);
        // sis_out_binary("...", sis_memory(s_->lpstream), 16);
        for (int i = 0; i < (int)ilen_; i++)
        {
            sis_bits_stream_put(s_, in_[i], 8);
        }
        // sis_out_binary("...", sis_memory(s_->lpstream), 16);
    }
    else
    {
        sis_bits_stream_put(s_, 0, 1);
    }
    return offset;
}
// 返回 0 表示字符串一样 -1 表示有一个为 NULL > 0 表示字符有效部分相同 
static inline int sis_bits_stream_charcmp(const char *s1_, size_t l1_, const char *s2_, size_t l2_)
{
	if (!s2_ || l2_ == 0 || !s1_ || l1_ == 0)
	{
		return l1_;
	}
    int isize = 0;
    const char *ptr = s1_;
    while (*ptr && isize < l1_)
    {
        ptr++; isize++;
    } 
    int size = 0;
    while(*s1_ == *s2_)
    {
        if (*s1_ == 0 && *s2_ == 0)
        {
            break;
        }
        size++;
		if (size == isize)
		{
            if (size == l2_ || *(s2_ + 1) == 0)
			{
                return 0;
            }
            else
            {
                return isize;
            }
		}
        s1_++; s2_++;
    }
	return isize;
	//tolower(*(const unsigned char *)s1_) - tolower(*(const unsigned char *)s2_);
}
static inline int sis_bits_stream_put_incr_chars(s_sis_bits_stream *s_, char *in_, size_t ilen_, char *ago_, size_t alen_)
{
    int size = sis_bits_stream_charcmp(in_, ilen_, ago_, alen_);
    // printf("put char : %d [%s], [%s], %zu, %zu\n", size, in_, ago_, ilen_, alen_);
    if (size == 0)
    {
        sis_bits_stream_put_chars(s_, NULL, 0);
    }
    else 
    {
        sis_bits_stream_put_chars(s_, in_, size);
    }
    return size;
}

static inline int sis_bits_stream_get_buffer(s_sis_bits_stream *s_, char *in_, size_t ilen_)
{
    for (int i = 0; i < ilen_; i++)
    {
        in_[i] = sis_bits_stream_get(s_, 8);
    }
    return ilen_;
}
// 短正整数
static inline uint32 sis_bits_stream_get_sint(s_sis_bits_stream *s_)
{
// 小整数 正整数 0 - 0xFF
// 00  --> 0       --> 0
// 01  --> 1       --> 1
// 10  --> 0000-1111   --> 16+2  = 0~17
// 110 --> 00000-11111 --> 32+18 = 0~49
// 111 --> 00000000-11111111 --> 256+50 = 0~305
    uint32 level = sis_bits_stream_get(s_, 2);
    if (level == 0||level == 1)
    {
        return level;
    }
    else if (level == 2)
    {
        return 2 + sis_bits_stream_get(s_, 4);
    }
    // if (level = 3)
    level = sis_bits_stream_get(s_, 1);
    return level == 0 ? 18 + sis_bits_stream_get(s_, 5) : 50 + sis_bits_stream_get(s_, 8);
}
static inline uint32 sis_bits_stream_get_short(s_sis_bits_stream *s_)
{
// 短正整数 正整数 0 - 0xFFFF 
// 00  --> 0 --> 0
// 01  --> 1 --> 1
// 10  --> 00-FF     --> 256+2 = 0~257
// 110 --> 000-FFF   --> 4096+258 = 0~4353 
// 111 --> 0000-FFFF --> 65536+4354 = 0~69889 
    uint32 level = sis_bits_stream_get(s_, 2);
    if (level == 0||level == 1)
    {
        return level;
    }
    else if (level == 2)
    {
        return 2 + sis_bits_stream_get(s_, 8);
    }
    // if (level = 3)
    level = sis_bits_stream_get(s_, 1);
    return level == 0 ? 258 + sis_bits_stream_get(s_, 12) : 4354 + sis_bits_stream_get(s_, 16);
}   
// 短正整数
static inline uint32 sis_bits_stream_get_uint(s_sis_bits_stream *s_)
{
// 索引整数 正整数 0 - 0xFFFFFFFF
// 00 --> 0-F
// 01 --> 10-FF
// 100 --> 100-FFF 
// 101 --> 1000-FFFF  
// 110 --> 10000-FFFFFF 
// 111 --> 1000000-FFFFFFFF  
    uint32 level = sis_bits_stream_get(s_, 1);
    if (!level)
    {
        level = sis_bits_stream_get(s_, 1);
        return sis_bits_stream_get(s_, level == 0 ? 4 : 8);
    }
    level = sis_bits_stream_get(s_, 2);
    if (level == 0)
    {
        return sis_bits_stream_get(s_, 12);
    }
    if (level == 1)
    {
        return sis_bits_stream_get(s_, 16);
    }
    if (level == 2)
    {
        return sis_bits_stream_get(s_, 24);
    }
    return sis_bits_stream_get(s_, 32);
}
// 带符号长整数
static inline int64 sis_bits_stream_get_int(s_sis_bits_stream *s_)
{
// 0表示和前值一样
// 10 --> 0-F
// 11 + 000-010
//    000 :  -FF…FF
//    001 :  -FFF…FFF
//    010 :  -FFFF…FFFF
//    011 :  -FFFFF…FFFFF
// 111 + 000-010
//    000 :  -FFFFFF…FFFFFF
//    001 :  -FFFFFFF…FFFFFFF
//    010 :  -FFFFFFFF…FFFFFFFF
//    011 :  -FFFFFFFFF…FFFFFFFFF
//    100 :  -FFFFFFFFFF…FFFFFFFFFF
//    101 :  -FFFFFFFFFFFF…FFFFFFFFFFFF
//    110 :  -FFFFFFFFFFFFFF…FFFFFFFFFFFFFF
//    111 :  -7FFFFFFFFFFFFFFF…7FFFFFFFFFFFFFFF
    uint8 signbit = sis_bits_stream_get(s_, 1);
    int64 dw = 0;

    uint32 level = sis_bits_stream_get(s_, 1);
    if (!level)
    {
        return 0;
    }
    level = sis_bits_stream_get(s_, 1);
    if (level == 0)
    {
        dw = sis_bits_stream_get(s_, 4);
    }
    else
    {
        level = sis_bits_stream_get(s_, 1);
        if (level == 0)
        {
            level = sis_bits_stream_get(s_, 2);
			switch (level)
			{
			case 0:   dw = sis_bits_stream_get(s_, 8);  break;
			case 1:   dw = sis_bits_stream_get(s_, 12); break;
			case 2:   dw = sis_bits_stream_get(s_, 16); break;
			default:  dw = sis_bits_stream_get(s_, 20); break;
			}
        }
        else
        {
            level = sis_bits_stream_get(s_, 3);
			switch (level)
			{
			case 0:   dw = sis_bits_stream_get(s_, 24);  break;
			case 1:   dw = sis_bits_stream_get(s_, 28); break;
			case 2:   dw = sis_bits_stream_get(s_, 32); break;
			case 3:   dw = sis_bits_stream_get(s_, 36); break;
			case 4:   dw = sis_bits_stream_get(s_, 40); break;
			case 5:   dw = sis_bits_stream_get(s_, 48); break;
			case 6:   dw = sis_bits_stream_get(s_, 56); break;
			default:  dw = sis_bits_stream_get(s_, 63); break;
			}
        }     
    }
    return signbit ? -1 * dw : dw;
}
static inline int64 sis_bits_stream_get_incr_int(s_sis_bits_stream *s_, int64 ago_)
{
    int64 dw = sis_bits_stream_get_int(s_);
    return dw + ago_;
}

static inline double sis_bits_stream_get_float(s_sis_bits_stream *s_, int dot_)
{
    double dw = (double)sis_bits_stream_get_int(s_);
    return dw / pow(10, dot_);
}
static inline double sis_bits_stream_get_incr_float(s_sis_bits_stream *s_, double ago_, int dot_)
{
    double dw = sis_bits_stream_get_float(s_, dot_);
    return dw + ago_;
}

static inline int sis_bits_stream_get_chars(s_sis_bits_stream *s_, char *in_, size_t ilen_)
{
// 0 表示和前值一样
// 1 + size长度+字符
    uint8 signbit = sis_bits_stream_get(s_, 1);
    // printf("signbit : %d\n", signbit);
    if (signbit == 0)
    {
        return 0;
    }
    int len = 0;
    int size = sis_bits_stream_get_uint(s_);
    for (int i = 0; i < size; i++)
    {
        char ch = sis_bits_stream_get(s_, 8);
        // printf("readchar --  : %d\n", s_->currpos);
        if (i < ilen_)
        {
            in_[i] = ch;
            len++;
        }
    }
    if (size > 0)
    {
        // if (ilen_ > 2)
        // {
        //     printf("------2------ : %d %d %s\n", size, ilen_, in_);
        // }
        for (size_t i = size; i < ilen_; i++)
        {
            in_[i] = 0;
        }
    }
    // printf("readchar : %d %s\n", size, in_);
    return size;
}
static inline int sis_bits_stream_get_incr_chars(s_sis_bits_stream *s_, char *in_, size_t ilen_, char *ago_, size_t alen_)
{
    int size = sis_bits_stream_get_chars(s_, in_, ilen_);
    if (size == 0)
    {
        size = sis_min(ilen_, alen_);
        memmove(in_, ago_, size);

        // if (ilen_ > 2)
        // {
        //     printf("------1------ : %d %d %s\n", size, ilen_, in_);
        // }
        for (size_t i = size; i < ilen_; i++)
        {
            in_[i] = 0;
        }
    }
    return size;
}


#endif