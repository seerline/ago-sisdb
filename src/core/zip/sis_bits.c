
#include "sis_bits.h"
#include "sis_math.h"

////////////////////////////////////////////////////////
// s_sis_bits_stream
////////////////////////////////////////////////////////

s_sis_bits_stream *sis_bits_stream_create(uint8 *in_, size_t ilen_)
{ 
	s_sis_bits_stream *s = SIS_MALLOC(s_sis_bits_stream, s); 
    s->units = sis_pointer_list_create();
    s->units->vfree = sis_free_call;
    s->cur_stream = in_;
    s->bit_maxsize = ilen_ * 8;
	return s;
}
void sis_bits_stream_destroy(s_sis_bits_stream *s_)
{
    if (!s_)
    {
        return ;
    }
    if (s_->units)
    {
        sis_pointer_list_destroy(s_->units);
    }
    if (s_->ago_memory)
    {
        sis_free(s_->ago_memory);
    }
	sis_free(s_);
}

void sis_bits_stream_clear(s_sis_bits_stream *s_)
{
    s_->cur_stream = NULL;
    s_->bit_maxsize = 0;
    s_->bit_currpos = 0;
    s_->bit_savepos = 0;
    s_->inited = 0;
    if (s_->units)
    {
        sis_pointer_list_clear(s_->units);
    }
    s_->sdbsize = 0;
    s_->max_keynum = 0;
    if (s_->ago_memory)
    {
        sis_free(s_->ago_memory);
        s_->ago_memory = NULL;
    }   
    s_->bags = 0;
    s_->bags_curpos = 0;
    s_->bags_bytes = 0;
}
size_t sis_bits_stream_getbytes(s_sis_bits_stream *s_)
{
	return (s_->bit_currpos + 7) / 8;
}

// 直接定位到新的位置
int sis_bits_stream_moveto(s_sis_bits_stream *s_, int bitpos_)	
{
    s_->bit_currpos = bitpos_;
    // if (s_->bit_currpos < 0)
    // {
    //     s_->bit_currpos = 0;
    // }
    // else if (s_->bit_currpos > s_->bit_maxsize)
    // {
    //     s_->bit_currpos = s_->bit_maxsize;
    // }
    return s_->bit_currpos;
}
//移动内部指针bits_位,可以为负，返回新的位置
int sis_bits_stream_move(s_sis_bits_stream *s_, int bits_)
{ 
    return sis_bits_stream_moveto(s_, s_->bit_currpos + bits_); 
}
int sis_bits_stream_move_bytes(s_sis_bits_stream *s_, int bytes_)
{
    return sis_bits_stream_moveto(s_, s_->bit_currpos + bytes_ * 8); 
}
//保存当前位置
void sis_bits_stream_savepos(s_sis_bits_stream *s_)
{ 
    s_->bit_savepos = s_->bit_currpos; 
}		
//回卷到上一次保存的位置
int  sis_bits_stream_restore(s_sis_bits_stream *s_)
{ 
    return sis_bits_stream_moveto(s_, s_->bit_savepos); 
}		

uint64 sis_bits_stream_get(s_sis_bits_stream *s_, int bits_)
{
    uint64 u64 = 0;
    if (bits_ <= 0 || bits_ > 64 || s_->bit_maxsize <= s_->bit_currpos)
    {
        return 0;
    }
    else
    {
        if ((uint64)bits_ > s_->bit_maxsize - s_->bit_currpos)
        {
            bits_ = s_->bit_maxsize - s_->bit_currpos;
        }
        int curpos = s_->bit_currpos / 8;
        u64 = s_->cur_stream[curpos++];
        int decpos = 8 - (s_->bit_currpos % 8);
        if (decpos != 8)
        {
            u64 &= (0xFF >> (8 - decpos));
        }
        int incpos = bits_ - decpos;
        while (incpos > 0)
        {
            if (incpos >= 8)
            {
                u64 = (u64 << 8) | s_->cur_stream[curpos++];
                incpos -= 8;
            }
            else
            {
                u64 = (u64 << incpos) | (s_->cur_stream[curpos++] >> (8 - incpos));
                incpos = 0;
            }
        }
        if (incpos < 0)
        {
            u64 >>= -incpos;
        }
    }
    s_->bit_currpos += bits_;
    return u64;
}

int sis_bits_stream_put(s_sis_bits_stream *s_, uint64 in_, int bits_)
{
    if (bits_ > 0)
    {
        in_ <<= 64 - bits_;
        uint8 *phigh_byte = (uint8 *)&in_ + 7;
        int offset = bits_;
        int lastbit = s_->bit_currpos % 8;
        if (lastbit)
        {
            int space = 8 - lastbit;
            uint8 c = s_->cur_stream[s_->bit_currpos / 8] & (0xFF << space);
            int incr = sis_min(offset, space);
            c |= (in_ >> (64 - space)) & (0xFF >> lastbit);
            s_->cur_stream[s_->bit_currpos / 8] = c;
            offset -= space;
            in_ <<= space;
            s_->bit_currpos += incr;
        }
        if (offset)
        {
            while (offset > 0)
            {
                s_->cur_stream[s_->bit_currpos / 8] = *phigh_byte;
                s_->bit_currpos += sis_min(8, offset);
                offset -= 8;
                in_ <<= 8;
            }
        }
    }
    return bits_;
}
int sis_bits_stream_put_buffer(s_sis_bits_stream *s_, char *in_, size_t bytes_)
{
	for (int i = 0; i < (int)bytes_; i++)
	{
		sis_bits_stream_put(s_, in_[i], 8);
	}
	return (int)bytes_  * 8;
}
int sis_bits_stream_put_count(s_sis_bits_stream *s_, uint32 in_)
{
// 索引整数 正整数 1 - 0xFFFF
// 0 --> 0 + 1
// 10 --> 1 + FF 
// 11 --> 1 + FFFF  
    int offset = 0;
    uint32 dw = in_ - 1;
    if (dw & 0xFF00)
    {
        dw |= 0x30000;
        sis_bits_stream_put(s_, dw, 18);
    }
    else
    {
        if (dw & 0xFF)
        {
            dw |= 0x200;
            sis_bits_stream_put(s_, dw, 10);
        }
        else
        {
            sis_bits_stream_put(s_, 0, 1);
        }        
    } 
    return offset; 
}
int sis_bits_stream_put_uint(s_sis_bits_stream *s_, uint32 in_)
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
int sis_bits_stream_put_int(s_sis_bits_stream *s_, int64 in_)
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
int sis_bits_stream_put_incr_int(s_sis_bits_stream *s_, int64 in_, int64 ago_)
{
    return sis_bits_stream_put_int(s_, (in_ - ago_));
}

int sis_bits_stream_put_float(s_sis_bits_stream *s_, double in_, int dot_)
{
    int64 dw = in_ * pow(10, dot_);
    return sis_bits_stream_put_int(s_, dw);
}
int sis_bits_stream_put_incr_float(s_sis_bits_stream *s_, double in_, double ago_, int dot_)
{
    double m = pow(10, dot_);
    return sis_bits_stream_put_int(s_, (in_ - ago_) * m);
}

int sis_bits_stream_put_chars(s_sis_bits_stream *s_, char *in_, size_t ilen_)
{
// 0 表示和前值一样
// 1 + size长度+字符
    // sis_out_binary("...", sis_memory(s_->cur_stream), 16);
    int offset = 0;
    if (ilen_ > 0 && in_)
    {
        sis_bits_stream_put(s_, 1, 1);
        // sis_out_binary("...", sis_memory(s_->cur_stream), 16);
        sis_bits_stream_put_uint(s_, ilen_);
        // sis_out_binary("...", sis_memory(s_->cur_stream), 16);
        for (int i = 0; i < (int)ilen_; i++)
        {
            sis_bits_stream_put(s_, in_[i], 8);
        }
        // sis_out_binary("...", sis_memory(s_->cur_stream), 16);
    }
    else
    {
        sis_bits_stream_put(s_, 0, 1);
    }
    return offset;
}
int sis_bits_stream_put_incr_chars(s_sis_bits_stream *s_, char *in_, size_t ilen_, char *ago_, size_t alen_)
{
    int offset = 0;
    // printf("put char :%s, %s, %d, %d\n", in_, ago_, ilen_, alen_);
    if (ilen_ == alen_ && !sis_strncasecmp(in_, ago_, ilen_))
    {
        sis_bits_stream_put_chars(s_, NULL, 0);
    }
    else
    {
        sis_bits_stream_put_chars(s_, in_, ilen_);
    }
    return offset;
}

// 短正整数
int sis_bits_stream_get_count(s_sis_bits_stream *s_)
{
// 索引整数 正整数 1 - 0xFFFF
// 0 --> 0 + 1
// 10 --> 1 + FF 
// 11 --> 1 + FFFF  
    uint32 level = sis_bits_stream_get(s_, 1);
    if (!level)
    {
        return 1;
    }
    level = sis_bits_stream_get(s_, 1);
    if (level == 0)
    {
        return 1 + sis_bits_stream_get(s_, 8);
    }
    return 1 + sis_bits_stream_get(s_, 16);
}
// 短正整数
uint32 sis_bits_stream_get_uint(s_sis_bits_stream *s_)
{
// 索引整数 正整数 0 - 0xFFFF
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
int64 sis_bits_stream_get_int(s_sis_bits_stream *s_)
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
            if (level == 0) dw = sis_bits_stream_get(s_, 8);
            else if (level == 1) dw = sis_bits_stream_get(s_, 12);
            else if (level == 2) dw = sis_bits_stream_get(s_, 16);
            else dw = sis_bits_stream_get(s_, 20);
        }
        else
        {
            level = sis_bits_stream_get(s_, 3);
            if (level == 0) dw = sis_bits_stream_get(s_, 24);
            else if (level == 1) dw = sis_bits_stream_get(s_, 28);
            else if (level == 2) dw = sis_bits_stream_get(s_, 32);
            else if (level == 3) dw = sis_bits_stream_get(s_, 36);
            else if (level == 4) dw = sis_bits_stream_get(s_, 40);
            else if (level == 5) dw = sis_bits_stream_get(s_, 48);
            else if (level == 6) dw = sis_bits_stream_get(s_, 56);
            else dw = sis_bits_stream_get(s_, 63);
        }       
    }
    return signbit ? -1 * dw : dw;
}
int64 sis_bits_stream_get_incr_int(s_sis_bits_stream *s_, int64 ago_)
{
    int64 dw = sis_bits_stream_get_int(s_);
    return dw + ago_;
}

double sis_bits_stream_get_float(s_sis_bits_stream *s_, int dot_)
{
    double dw = (double)sis_bits_stream_get_int(s_);
    return dw / pow(10, dot_);
}
double sis_bits_stream_get_incr_float(s_sis_bits_stream *s_, double ago_, int dot_)
{
    double dw = sis_bits_stream_get_float(s_, dot_);
    return dw + ago_;
}

int sis_bits_stream_get_chars(s_sis_bits_stream *s_, char *in_, size_t ilen_)
{
// 0 表示和前值一样
// 1 + size长度+字符
    uint8 signbit = sis_bits_stream_get(s_, 1);
    if (signbit == 0)
    {
        return 0;
    }
    int size = sis_bits_stream_get_uint(s_);
    int cursize = sis_min(size, ilen_);
    for (int i = 0; i < cursize; i++)
    {
        in_[i] = sis_bits_stream_get(s_, 8);
    }
    if (size > ilen_)
    {
        in_[ilen_] = 0;
    }
    return cursize;
}
int sis_bits_stream_get_incr_chars(s_sis_bits_stream *s_, char *in_, size_t ilen_, char *ago_, size_t alen_)
{
    int size = sis_bits_stream_get_chars(s_, in_, ilen_);
    if (size == 0)
    {
        memmove(in_, ago_, ilen_);
        size = ilen_;
    }
    return size;
}

////////////////////////////
// 以下是结构化读取和写入的函数
////////////////////////////
int sis_bits_struct_set_sdb(s_sis_bits_stream *s_, s_sis_dynamic_db *db_)
{
    s_sis_struct_unit *unit = SIS_MALLOC(s_sis_struct_unit, unit);
    unit->sdb = db_;
    unit->offset = s_->sdbsize;
    s_->sdbsize += (db_->size + 1);
    sis_pointer_list_push(s_->units, unit);
    return s_->units->count - 1;
}
int sis_bits_struct_set_key(s_sis_bits_stream *s_, int keynum_)
{
    s_->max_keynum = keynum_;
    return keynum_;
}
int  sis_bits_struct_get_bags(s_sis_bits_stream *s_, bool isread_)
{
    sis_bits_stream_savepos(s_);
    int count = 0;
    // printf("1.1 = %d  %d %d\n",s_->bit_currpos, s_->bit_maxsize, sis_bits_stream_getbytes(s_));
    if (isread_)
    {
        sis_bits_stream_moveto(s_, s_->bit_maxsize - 16);
        count = sis_bits_stream_get(s_, 32);
    }
    else
    {
        int offset =  (8 - (s_->bit_currpos % 8)) % 8;   
        if (offset)
        {
            sis_bits_stream_move(s_, offset);
        }
        count = sis_bits_stream_get(s_, 32);
    }
    sis_bits_stream_restore(s_);
    s_->bags= count;
    return count;
}
void sis_bits_struct_set_bags(s_sis_bits_stream *s_)
{
    sis_bits_stream_savepos(s_);
    // printf("3.1 = %d  %d\n",s_->bit_currpos, s_->bit_maxsize);
    int offset =  (8 - (s_->bit_currpos % 8)) % 8;   
    if (offset)
    {
        sis_bits_stream_move(s_, offset);
    }
    sis_bits_stream_put(s_, s_->bags, 32);
    s_->bags_bytes = 4;
    sis_bits_stream_restore(s_);
}
size_t sis_bits_struct_getsize(s_sis_bits_stream *s_)
{
	return (s_->bit_currpos + 7) / 8 + s_->bags_bytes;
}
void sis_bits_struct_link(s_sis_bits_stream *s_, uint8 *in_, size_t ilen_)
{
    s_->cur_stream = in_;
    s_->bit_maxsize = ilen_ * 8;
    s_->bit_currpos = 0;
    s_->bit_savepos = 0;
    s_->bags = 0;
    s_->bags_bytes = 0;
}
void sis_bits_struct_flush(s_sis_bits_stream *s_)
{
    if (s_->inited == 1)
    {
        memset(s_->ago_memory, 0, s_->max_keynum * s_->sdbsize);
    }   
    else
    {
        
    }   
}
void _sis_bits_struct_init(s_sis_bits_stream *s_)
{
    if (s_->inited == 0)
    {
        if (s_->ago_memory)
        {
            sis_free(s_->ago_memory);
        }  
        s_->ago_memory = (uint8 *)sis_malloc(s_->max_keynum * s_->sdbsize);
        memset(s_->ago_memory, 0, s_->max_keynum * s_->sdbsize);
        s_->inited = 1;
    }    
}
uint8 *_sis_bits_struct_get_ago(s_sis_bits_stream *s_, int kid_, s_sis_struct_unit *unit_)
{
    if (unit_ && kid_ < (int)s_->max_keynum)
    {
        return s_->ago_memory + kid_ * s_->sdbsize + unit_->offset;
    }
    return NULL;
}

// 
static inline void _sis_bits_struct_encode_one(s_sis_bits_stream *s_, 
    const char *memory_, s_sis_dynamic_field *infield_, 
    const char *in_)
{
    for(int index = 0; index < infield_->count; index++)
    {
        switch (infield_->style)
        {
        case SIS_DYNAMIC_TYPE_INT:
            {
                sis_bits_stream_put_incr_int(s_, 
                    _sis_field_get_int(infield_, in_, index),
                    _sis_field_get_int(infield_, memory_, index));
            }
            break;
        case SIS_DYNAMIC_TYPE_SEC:
        case SIS_DYNAMIC_TYPE_TICK:
        case SIS_DYNAMIC_TYPE_MINU:
        case SIS_DYNAMIC_TYPE_DATE:
        case SIS_DYNAMIC_TYPE_UINT:
            {
                sis_bits_stream_put_incr_int(s_, 
                    _sis_field_get_uint(infield_, in_, index),
                    _sis_field_get_uint(infield_, memory_, index));
            }
            break;
        case SIS_DYNAMIC_TYPE_FLOAT:
        case SIS_DYNAMIC_TYPE_PRICE:
            {
                sis_bits_stream_put_incr_float(s_, 
                    _sis_field_get_float(infield_, in_, index),
                    _sis_field_get_float(infield_, memory_, index), infield_->dot);
            }
            break;
        case SIS_DYNAMIC_TYPE_CHAR:
            {
                sis_bits_stream_put_incr_chars(s_, 
                    (char *)in_ + infield_->offset + index * infield_->len, infield_->len,
                    (char *)memory_ + infield_->offset + index * infield_->len, infield_->len);
            }
            break;
        default:
            break;
        }
    }
}

// 如果压缩长度大于原始数据长度 就用原始数据 第一位0表示原始数据 1 表示压缩数据
int sis_bits_struct_encode(s_sis_bits_stream *s_, int kid_, int sid_, void *in_, size_t ilen_)
{
    _sis_bits_struct_init(s_);

    s_sis_struct_unit *unit = (s_sis_struct_unit *)sis_pointer_list_get(s_->units, sid_);
    uint8 *buffer = _sis_bits_struct_get_ago(s_, kid_, unit);
    if (!buffer)
    {
        return 0;
    }
    if (ilen_ % unit->sdb->size)
    {
        return 0;
    }
    char *memory = (char *)&buffer[1];
    int count = ilen_ / unit->sdb->size;
    // sis_out_binary("buffer", (char *)buffer, unit->sdb->size + 1);

    sis_bits_stream_put(s_, buffer[0] == 0 ? 0 : 1, 1);
    // printf("[%d %d] %s \n",kid_, sid_, unit->sdb->name);
    sis_bits_stream_put_uint(s_, kid_);
    // sis_out_binary("1", sis_memory(s_->cur_stream), sis_bits_struct_getsize(s_));
    sis_bits_stream_put_uint(s_, sid_);
    // sis_out_binary("2", sis_memory(s_->cur_stream), sis_bits_struct_getsize(s_));
    sis_bits_stream_put_count(s_, count);
    // sis_out_binary("3", sis_memory(s_->cur_stream), sis_bits_struct_getsize(s_));

	int fnums = sis_map_list_getsize(unit->sdb->fields);
    const char *in = (const char *)in_;
	for (int k = 0; k < count; k++)
	{
		for (int i = 0; i < fnums; i++)
		{
			s_sis_dynamic_field *infield = (s_sis_dynamic_field *)sis_map_list_geti(unit->sdb->fields, i);
            if (!infield)
            {
                continue;
            }
			_sis_bits_struct_encode_one(s_, memory, infield, in);
            // printf("[%d:%d] %d -- %d\n", k, i, sis_bits_struct_getsize(s_),s_->bit_currpos);
		}
        memmove(memory, in, unit->sdb->size);
        buffer[0] = 1;
        // sis_out_binary("in", in, unit->sdb->size);
        // sis_out_binary("memory", memory, unit->sdb->size);
		in += unit->sdb->size;
	}
    s_->bags++;
    // printf("encode nums= %d bytes %d\n", s_->bags, sis_bits_stream_getbytes(s_));
    sis_bits_struct_set_bags(s_);
    // printf("encode nums= %d bytes %d\n", s_->bags, sis_bits_stream_getbytes(s_));
    return 1;
}

static inline void _sis_bits_struct_decode_one(s_sis_bits_stream *s_,
    const char *memory_, s_sis_dynamic_field *infield_, 
    char *in_)
{
    for(int index = 0; index < infield_->count; index++)
    {
        switch (infield_->style)
        {
        case SIS_DYNAMIC_TYPE_INT:
            {
                _sis_field_set_int(infield_, in_, 
                    sis_bits_stream_get_incr_int(s_, memory_ ? _sis_field_get_int(infield_, memory_, index) : 0),
                    index);
            }
            break;
        case SIS_DYNAMIC_TYPE_SEC:
        case SIS_DYNAMIC_TYPE_TICK:
        case SIS_DYNAMIC_TYPE_MINU:
        case SIS_DYNAMIC_TYPE_DATE:
        case SIS_DYNAMIC_TYPE_UINT:
            {
                _sis_field_set_uint(infield_, in_, 
                    sis_bits_stream_get_incr_int(s_, memory_ ? _sis_field_get_uint(infield_, memory_, index) : 0),
                    index);
            }
            break;
        case SIS_DYNAMIC_TYPE_FLOAT:
        case SIS_DYNAMIC_TYPE_PRICE:
            {
                _sis_field_set_float(infield_, in_, 
                    sis_bits_stream_get_incr_float(s_, memory_ ? _sis_field_get_float(infield_, memory_, index) : 0.0, infield_->dot),
                    index);
            }
            break;
        case SIS_DYNAMIC_TYPE_CHAR:
            {
                if (memory_)
                {
                    sis_bits_stream_get_incr_chars(s_, 
                        in_ + infield_->offset + index * infield_->len, infield_->len,
                        (char *)memory_ + infield_->offset + index * infield_->len, infield_->len);
                }
                else
                {
                    sis_bits_stream_get_chars(s_, 
                        in_ + infield_->offset + index * infield_->len, infield_->len);
                }               
            }
            break;
        default:
            break;
        }
    }
}
void _unzip_unit_free(s_sis_struct_list *list)
{
    for (int i = 0; i < list->count; i++)
    {
        s_unzip_unit *unit = (s_unzip_unit *)sis_struct_list_get(list, i);
        sis_free(unit->data);
    }
    sis_struct_list_destroy(list);
}
// 用回调来返回数据
int sis_bits_struct_decode(s_sis_bits_stream *s_, void *cb_source_, cb_sis_struct_decode *cb_read_)
{
    _sis_bits_struct_init(s_);
    // 去尾部取标志位
    int nums = sis_bits_struct_get_bags(s_, true);
    // printf("decode nums= %d\n", nums);
    if (nums < 1)
    {
        return 0;
    }    
    sis_bits_stream_savepos(s_);
    s_sis_struct_list *list = sis_struct_list_create(sizeof(s_unzip_unit));
    sis_struct_list_set_size(list, nums);
    
    while (list->count < nums)
    {        
        int zip = sis_bits_stream_get(s_, 1);
        // if(!zip)
        // {
        //     printf("...\n");
        // }
        s_unzip_unit unzip;
        unzip.kidx = sis_bits_stream_get_uint(s_);
        unzip.sidx = sis_bits_stream_get_uint(s_);
        int count = sis_bits_stream_get_count(s_);

        s_sis_struct_unit *unit = (s_sis_struct_unit *)sis_pointer_list_get(s_->units, unzip.sidx);
        uint8 *buffer = _sis_bits_struct_get_ago(s_, unzip.kidx, unit);
        if (!buffer)
        {
            // 如果解析失败
            _unzip_unit_free(list);
            sis_bits_stream_restore(s_);
            return 0;
        }
        char *memory = (char *)&buffer[1];
        int fnums = sis_map_list_getsize(unit->sdb->fields);
        unzip.size = unit->sdb->size * count;
        unzip.data = (char *)sis_calloc(unzip.size);
        char *in = unzip.data;
        for (int k = 0; k < count; k++)
        {
            for (int i = 0; i < fnums; i++)
            {
                s_sis_dynamic_field *infield = (s_sis_dynamic_field *)sis_map_list_geti(unit->sdb->fields, i);
                if (!infield)
                {
                    continue;
                }
                _sis_bits_struct_decode_one(s_, zip == 0 ? NULL : memory, infield, in);
            }
            // sis_out_binary("mm", memory, unit->sdb->size);
            // sis_out_binary("ii", in, unit->sdb->size);
            memmove(memory, in, unit->sdb->size);
            in += unit->sdb->size;
        }
        sis_struct_list_push(list, &unzip);
    }
    if (cb_read_)
    {
        // printf("count=%d\n", list->count);
        for (int i = 0; i < list->count; i++)
        {
            s_unzip_unit *punzip = (s_unzip_unit *)sis_struct_list_get(list, i);    
            cb_read_(cb_source_ ?  cb_source_ : s_, punzip->kidx, punzip->sidx, punzip->data, punzip->size);
        }
    }
    _unzip_unit_free(list);
    sis_bits_stream_restore(s_);
    return nums;
}

// int sis_snappy_struct(char *in_, size_t ilen_, s_sis_memory *out_)
// {
//     // if (!__snappyc_env_inited)
//     // {
//     //     snappy_init_env(&__snappyc_env);
//     //     __snappyc_env_inited = 1;
//     // }
//     sis_memory_clear(out_);
//     size_t output_length = snappy_max_structed_length(ilen_);
//     if (output_length > 0)
//     {
//         sis_memory_set_maxsize(out_, sis_memory_get_size(out_) + output_length + 1);
//         // if (snappy_struct(&__snappyc_env, in_, ilen_, sis_memory(out_), &output_length) == SNAPPY_OK)
//         if (snappy_struct(in_, ilen_, sis_memory(out_), &output_length) == SNAPPY_OK)
//         {
//             if (output_length < ilen_)
//             {
//                 sis_memory_set_size(out_, output_length);
//                 return 1;
//             }
//         }
//     }
//     // 压缩失败或长度更大
//     return 0;
// }

// int sis_snappy_unstruct(char *in_, size_t ilen_, s_sis_memory *out_)
// {
//     // if (!__snappyc_env_inited)
//     // {
//     //     snappy_init_env(&__snappyc_env);
//     //     __snappyc_env_inited = 1;
//     // }
//     sis_memory_clear(out_);
//     size_t output_length = 0;
//     if (snappy_unstructed_length(in_, ilen_, &output_length) == SNAPPY_OK)
//     // if (snappy_unstructed_length(in_, ilen_, &output_length))
//     {
//         sis_memory_set_maxsize(out_, output_length + 1);
//         if (snappy_unstruct(in_, ilen_, sis_memory(out_), &output_length) == SNAPPY_OK)
//         // if (snappy_unstruct(in_, ilen_, sis_memory(out_)) == SNAPPY_OK)
//         {
//             // 成功
//             sis_memory_set_size(out_, output_length);
//             return 1;
//         }
//     }
//     return 0;
// }

// bool sis_net_snappy_struct(s_sis_memory *in_, s_sis_memory *out_)
// {
//     return (bool)sis_snappy_struct(sis_memory(in_), sis_memory_get_size(in_), out_);
// }
// bool sis_net_snappy_unstruct(s_sis_memory *in_, s_sis_memory *out_)
// {
//     return (bool)sis_snappy_unstruct(sis_memory(in_), sis_memory_get_size(in_), out_);
// }

#if 0
#include "sis_memory.h"
#pragma pack(push,1)
typedef struct _snap_ {
	char    name[4];
	int32   open;
    uint32  high;
	float   lowp;
	double  newp;
    int64   vols;
    uint64  mony;
	int     askp[2];
} _snap_;

typedef struct _tick_ {
    char    flag;
	int32   newp;
    uint32  askn;
    uint32  vols;
} _tick_;

#pragma pack(pop)

static struct _snap_ snaps[] = {
    {  "SH", 1000, 2000, 21.3456, 31.4567,   50000000, 1789000000, {1, 2}},
    {  "SH", 1000, 2000, 22.3456, 32.4567,   50000000, 2789000000, {1, 2}},
    {  "SH", 1000, 2000, 23.3456, 33.4567,  -50000000, 3789000000, {1, 2}},
    {  "SH", 1000, 2000, 24.3456, 34.4567,   50000000, 4789000000, {1, 2}},
    {  "SH", 1000, 2000, 25.3456, 35.4567,  -50000000, 5789000000, {1, 2}},
    {  "SZ", 1000, 2000, 26.3456, 36.4567,   50000000, 6789000000, {1, 2}},
    {  "SZ", 1000, 2000, 27.3456, 37.4567,   50000000, 7789000000, {1, 2}},
    {  "SH", 1000, 2000, 28.3456, 38.4567,   50000000, 8789000000, {1, 2}},
    {  "SH", 1000, 2000, 29.3456, 39.4567,   50000000, 9789000000, {1, 2}},
}; 

static struct _tick_ ticks[] = {
    {  'B',  1000, 2000, 3000},
    {  'B',  1000, 2000, 3000},
    {  'B',  1000, 2000, 3000},
    {  'B',  1000, 2000, 3000},
    {  'S', -1000, 2000, 3000},
    {  'S', -1100, 2000, 3000},
    {  'S', -1200, 2000, 3000},
    {  'S', -1300, 2000, 3000},
    {  'B',  1000, 2000, 3000},
    {  'B',  1100, 2000, 3000},
    {  'B',  1200, 2000, 3000},
    {  'B',  1300, 2000, 3000},
    {  'B',  1400, 2000, 3000},
};

int cb_sis_decode(void *src, int kid,int sid, char *in, size_t ilen)
{
    // sis_out_binary("read", in, ilen);
    s_sis_bits_stream *zip = (s_sis_bits_stream *)src;
    s_sis_struct_unit *unit=(s_sis_struct_unit *)sis_pointer_list_get(zip->units, sid);

    s_sis_sds out = sis_dynamic_db_to_array_sds(unit->sdb, unit->sdb->name, in, ilen);
    printf("%s = %s\n", unit->sdb->name, out);
    sis_sdsfree(out);

    return 0;
}
int main()
{
	const char *db_snap = "{snap:{fields:{name:[C,4],open:[I,4],high:[U,4],lowp:[F,4,1,2],newp:[F,8,1,3],vols:[I,8],mony:[U,8],ask:[I,4,2]}}}";
	const char *db_tick = "{tick:{fields:{flag:[C,1],newp:[I,4],askn:[U,4],vols:[U,4]}}}";

	s_sis_conf_handle *injson = sis_conf_load(db_snap, strlen(db_snap));
	s_sis_dynamic_db *snap = sis_dynamic_db_create(sis_json_first_node(injson->node));
	sis_conf_close(injson);

	s_sis_conf_handle *outjson = sis_conf_load(db_tick, strlen(db_tick));
	s_sis_dynamic_db *tick = sis_dynamic_db_create(sis_json_first_node(outjson->node));
	sis_conf_close(outjson);

    int snap_nums = sizeof(snaps) / sizeof(_snap_);
    int tick_nums = sizeof(ticks) / sizeof(_tick_);
    {
        s_sis_sds in = sis_dynamic_db_to_array_sds(snap, "snap", &snaps[0], snap_nums * sizeof(_snap_));
        printf("%s\n", in);
        sis_sdsfree(in);
    } 
    {
        s_sis_sds in = sis_dynamic_db_to_array_sds(tick, "tick", &ticks[0], tick_nums* sizeof(_tick_));
        printf("%s\n", in);
        sis_sdsfree(in);
    }
    // 压缩
    s_sis_memory *out = sis_memory_create();
    sis_memory_set_maxsize(out, 1000*1000*256);
    s_sis_bits_stream *zip = sis_bits_stream_create((uint8 *)sis_memory(out), sis_memory_get_size(out));

    sis_bits_struct_set_key(zip, 2);
    int snapidx = sis_bits_struct_set_sdb(zip, snap);
    int tickidx = sis_bits_struct_set_sdb(zip, tick);

    // for (int i = 0; i < zip->units->count; i++)
    // {
    //     s_sis_struct_unit *unit=(s_sis_struct_unit *)sis_pointer_list_get(zip->units, i);
    //     printf("%d : %p, %p\n",i, unit->sdb, unit->sdb->fields);
    // }
    
    printf("[%d,%d] %p %p nums : %d %d\n", snapidx, tickidx, snap, tick, snap_nums, tick_nums);
    printf("start: %lld\n", sis_time_get_now_msec());
    int zipnnums = 2;//*1000;
    for (int k = 0; k < zipnnums; k++)
    {
        for (int i = 0; i < sis_max(snap_nums, tick_nums); i++)
        {
            if (i < snap_nums)
            {
                sis_bits_struct_encode(zip, 0, snapidx, &snaps[i], sizeof(_snap_));
            }
            if (i < tick_nums)
            {
                // sis_out_binary("in tick", (const char *)&ticks[i], sizeof(_tick_));
                sis_bits_struct_encode(zip, 1, tickidx, &ticks[i], sizeof(_tick_));
            }
        }
        sis_bits_struct_flush(zip);
    }
    // sis_bits_struct_encode(zip, 3, tickidx, &ticks[1], 5*sizeof(_tick_));
    // 77700000 --> 24962531
    // 77700000 --> 23587531
    sis_memory_set_size(out, sis_bits_struct_getsize(zip));
    printf("stop: %lld %d\n", sis_time_get_now_msec(), sis_bits_stream_getbytes(zip));
    size_t size = sis_bits_struct_getsize(zip);
    printf("zip : from %d --> %d %d\n", 
        zipnnums * (int)(snap_nums * sizeof(_snap_) + tick_nums* sizeof(_tick_) + 8 * (snap_nums + tick_nums)), 
        (int)size, (int)sis_memory_get_size(out));
    
    sis_out_binary("out", sis_memory(out), size);
    sis_bits_stream_destroy(zip);

    s_sis_bits_stream *unzip = sis_bits_stream_create((uint8 *)sis_memory(out), sis_memory_get_size(out));
    // 解压
    sis_bits_struct_set_key(unzip, 10);
    sis_bits_struct_set_sdb(unzip, snap);
    sis_bits_struct_set_sdb(unzip, tick);

    sis_bits_struct_decode(unzip, NULL, cb_sis_decode);
    sis_bits_stream_destroy(unzip);

    sis_memory_destroy(out);
    

	sis_dynamic_db_destroy(snap);
	sis_dynamic_db_destroy(tick);
	return 0;
}

#endif