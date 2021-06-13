
#include "sis_incrzip.h"
#include "sis_math.h"

////////////////////////////////////////////////////////
// s_sis_incrzip_class
////////////////////////////////////////////////////////
s_sis_incrzip_class *sis_incrzip_class_create()
{
	s_sis_incrzip_class *s = SIS_MALLOC(s_sis_incrzip_class, s); 
    s->dbinfos = sis_pointer_list_create();
    s->dbinfos->vfree = sis_free_call;
    s->cur_stream = sis_bits_stream_create(NULL, 0);
    s->cur_keys = 1;
    s->status = SIS_SIC_STATUS_NONE;
	return s;    
}
void _incrzip_memory_free(s_sis_incrzip_class *s_)
{
    if (s_->cur_memory)
    {
        sis_free(s_->cur_memory);
        s_->cur_memory = NULL;
    }    
}
void _incrzip_memory_init(s_sis_incrzip_class *s_)
{
    _incrzip_memory_free(s_);
    s_->cur_memory = (uint8 *)sis_malloc(s_->cur_keys * s_->sumdbsize);
    memset(s_->cur_memory, 0, s_->cur_keys * s_->sumdbsize);
}

void sis_incrzip_class_destroy(s_sis_incrzip_class *s_)
{
    sis_bits_stream_destroy(s_->cur_stream);
    _incrzip_memory_free(s_);
    sis_pointer_list_destroy(s_->dbinfos);
    if (s_->zip_memory)
    {
        sis_free(s_->zip_memory);
        s_->zip_memory = NULL;
    }
	sis_free(s_);
}
void sis_incrzip_class_clear(s_sis_incrzip_class *s_)
{
    s_->cb_source = NULL;
    s_->cb_compress = NULL;
    s_->cb_uncompress = NULL;

    s_->zip_size = 0;
    if (s_->zip_memory)
    {
        sis_free(s_->zip_memory);
        s_->zip_memory = NULL;
    }
    s_->zip_bags = 0;
    s_->zip_bagsize = 0;

    sis_bits_stream_init(s_->cur_stream);
    _incrzip_memory_free(s_);

    sis_pointer_list_clear(s_->dbinfos);
    s_->sumdbsize = 0;
    s_->maxdbsize = 0;
    s_->cur_keys = 1;

    s_->status = SIS_SIC_STATUS_NONE;
}
int sis_incrzip_set_key(s_sis_incrzip_class *s_, int maxkeys_)
{
    s_->cur_keys = maxkeys_ < 1 ? 1 : maxkeys_;    // 默认1个 可以动态增加 但动态增加效率低
    s_->status |= SIS_SIC_STATUS_INITKEY;
    return s_->cur_keys;
}
int sis_incrzip_set_sdb(s_sis_incrzip_class *s_, s_sis_dynamic_db *db_)
{
    s_sis_incrzip_dbinfo *info = SIS_MALLOC(s_sis_incrzip_dbinfo, info);
    info->lpdb = db_;
    info->fnums = sis_map_list_getsize(db_->fields);
    info->offset = s_->sumdbsize;
    s_->sumdbsize += (db_->size + 1);
    s_->maxdbsize = sis_max(s_->maxdbsize, db_->size);
    sis_pointer_list_push(s_->dbinfos, info);
    s_->status |= SIS_SIC_STATUS_INITSDB;
    return s_->dbinfos->count - 1;
}

size_t sis_incrzip_getsize(s_sis_incrzip_class *s_)
{
	return sis_bits_stream_getbytes(s_->cur_stream) + s_->zip_bagsize;
}

void sis_incrzip_set_bags(s_sis_incrzip_class *s_)
{
    sis_bits_stream_savepos(s_->cur_stream);
    int offset =  (8 - (s_->cur_stream->currpos % 8)) % 8;   
    if (offset)
    {
        sis_bits_stream_move(s_->cur_stream, offset);
    }
    s_->zip_bagsize = 0;
    if (s_->zip_bags <= 250 && s_->zip_bags > 0)
    {
        sis_bits_stream_put(s_->cur_stream, s_->zip_bags, 8);
        s_->zip_bagsize = 1;
    }
    else if (s_->zip_bags <= 0xFFFF)
    {
        sis_bits_stream_put(s_->cur_stream, s_->zip_bags, 16);
        sis_bits_stream_put(s_->cur_stream, 251, 8);
        s_->zip_bagsize = 3;
    }
    else
    {
        sis_bits_stream_put(s_->cur_stream, s_->zip_bags, 32);
        sis_bits_stream_put(s_->cur_stream, 252, 8);
        s_->zip_bagsize = 5;
    }
    sis_bits_stream_restore(s_->cur_stream);
}
uint8 *_sis_incrzip_get_curmem(s_sis_incrzip_class *s_, int kid_, s_sis_incrzip_dbinfo *info_)
{
    if (info_ && kid_ < (int)s_->cur_keys)
    {
        return s_->cur_memory + kid_ * s_->sumdbsize + info_->offset;
    }
    return NULL; 
}

///////////////////

int sis_incrzip_compress_start(s_sis_incrzip_class *s_, int maxsize_, void *source_, cb_incrzip_encode *cb_)
{
    if (s_->status != SIS_SIC_STATUS_INITED)
    {
        return -1;
    }
    if (!cb_ )
    {
        return -2;
    }
    s_->zip_size = maxsize_ < 16384 ? 16384 : maxsize_; 
    s_->zip_memory = sis_malloc(s_->zip_size + s_->maxdbsize + 256);
    s_->cb_source = source_;
    s_->cb_compress = cb_; 

    _incrzip_memory_init(s_);

    sis_bits_stream_link(s_->cur_stream, s_->zip_memory, s_->zip_size);

    s_->status = SIS_SIC_STATUS_ENCODE;
    return 0;
}
// 生成新的数据包
void _incrzip_compress_next(s_sis_incrzip_class *s_)
{
    s_->zip_bagsize = 0;
    s_->zip_bags = 0;
    sis_bits_stream_link(s_->cur_stream, s_->zip_memory, s_->zip_size);
}
// 生成新的数据包
void sis_incrzip_compress_restart(s_sis_incrzip_class *s_)
{
    if (s_->status == SIS_SIC_STATUS_ENCODE)
    {
        int size = sis_incrzip_getsize(s_);
        if (size > 0)
        {
            // printf("--1.0-- %d\n", size);
            s_->cb_compress(s_->cb_source, (char *)s_->zip_memory, size);
            // 传递后就从头开始写数据了
            _incrzip_compress_next(s_);
        }  
        // 生成新的数据包前需要把可能老的数据传递出去  
        memset(s_->cur_memory, 0, s_->cur_keys * s_->sumdbsize);
    }  
}

int sis_incrzip_compress_stop(s_sis_incrzip_class *s_)
{
    if (s_->status == SIS_SIC_STATUS_ENCODE)
    {
        int size = sis_incrzip_getsize(s_);
        if (size > 0)
        {
            s_->cb_compress(s_->cb_source, (char *)s_->zip_memory, size);
        }    
        sis_free(s_->zip_memory);
        s_->zip_memory = NULL;
        sis_bits_stream_init(s_->cur_stream);
        _incrzip_memory_free(s_);
        s_->cb_source = NULL;
        s_->cb_compress = NULL;
        s_->status = SIS_SIC_STATUS_NONE;
    }
    return 0;
}

static inline void _sis_incrzip_compress_one(s_sis_incrzip_class *s_, 
    const char *memory_, s_sis_dynamic_field *infield_, 
    const char *in_)
{
    for(int index = 0; index < infield_->count; index++)
    {
        switch (infield_->style)
        {
        case SIS_DYNAMIC_TYPE_INT:
            {
                sis_bits_stream_put_incr_int(s_->cur_stream, 
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
                sis_bits_stream_put_incr_int(s_->cur_stream, 
                    _sis_field_get_uint(infield_, in_, index),
                    _sis_field_get_uint(infield_, memory_, index));
            }
            break;
        case SIS_DYNAMIC_TYPE_FLOAT:
        case SIS_DYNAMIC_TYPE_PRICE:
            {
                sis_bits_stream_put_incr_float(s_->cur_stream, 
                    _sis_field_get_float(infield_, in_, index),
                    _sis_field_get_float(infield_, memory_, index), infield_->dot);
            }
            break;
        case SIS_DYNAMIC_TYPE_CHAR:
            {
                sis_bits_stream_put_incr_chars(s_->cur_stream, 
                    (char *)in_ + infield_->offset + index * infield_->len, infield_->len,
                    (char *)memory_ + infield_->offset + index * infield_->len, infield_->len);
            }
            break;
        default:
            break;
        }
    }
}
void _incrzip_compress(s_sis_incrzip_class *s_, uint8 *agomem_, int kidx_, int sidx_, int nums_,
    s_sis_incrzip_dbinfo *info_, char *in_, size_t ilen_)
{
    sis_bits_stream_savepos(s_->cur_stream);
    char *memory = (char *)&agomem_[1];
    sis_bits_stream_put(s_->cur_stream, agomem_[0] == 0 ? 0 : 1, 1);
    sis_bits_stream_put_uint(s_->cur_stream, kidx_);
    sis_bits_stream_put_uint(s_->cur_stream, sidx_);
    sis_bits_stream_put_uint(s_->cur_stream, nums_);

    size_t cur_size = sis_bits_stream_getbytes(s_->cur_stream);
    char *in = in_;
	for (int k = 0; k < nums_; k++)
	{
		for (int i = 0; i < info_->fnums; i++)
		{
			s_sis_dynamic_field *infield = (s_sis_dynamic_field *)sis_map_list_geti(info_->lpdb->fields, i);
			_sis_incrzip_compress_one(s_, memory, infield, in);
		}
        memmove(memory, in, info_->lpdb->size);
        agomem_[0] = 1;
		in += info_->lpdb->size;
	}
    // 压缩了反而大就用原文
    if (sis_bits_stream_getbytes(s_->cur_stream) - cur_size > ilen_ + nums_)
    {
        // printf("encode too big. %d %d\n", sis_bits_stream_getbytes(s_->cur_stream) - cur_size, ilen_ + nums_);
        sis_bits_stream_restore(s_->cur_stream);
        sis_bits_stream_put(s_->cur_stream, 0, 1);
        sis_bits_stream_put_uint(s_->cur_stream, kidx_);
        sis_bits_stream_put_uint(s_->cur_stream, sidx_);
        sis_bits_stream_put_uint(s_->cur_stream, nums_);
        sis_bits_stream_put_buffer(s_->cur_stream, in_, ilen_);
    }
    s_->zip_bags++;
    sis_incrzip_set_bags(s_);
}
int sis_incrzip_compress(s_sis_incrzip_class *s_, char *in_, size_t ilen_, s_sis_memory *out_)
{
    if (!out_)
    {
        return -1;
    }
    s_sis_incrzip_dbinfo *info = (s_sis_incrzip_dbinfo *)sis_pointer_list_get(s_->dbinfos, 0);
    if (!info)
    {
        return -2;
    }
    s_->zip_bagsize = 0;
    s_->zip_bags = 0;
    _incrzip_memory_init(s_);
    uint8 *buffer = _sis_incrzip_get_curmem(s_, 0, info);
    if (!buffer)
    {
        return -3;
    }
    if (ilen_ % info->lpdb->size)
    {
        return -4;
    }
    int count = ilen_ / info->lpdb->size;
    size_t zipsize = ilen_ + 256;
    sis_memory_set_maxsize(out_, zipsize);
    sis_bits_stream_link(s_->cur_stream, (uint8 *)sis_memory(out_), zipsize);
    // 开始压缩
    _incrzip_compress(s_, buffer, 0, 0, count, info, in_, ilen_);

    sis_memory_set_size(out_, sis_incrzip_getsize(s_));
    // sis_out_binary("one", sis_memory(out_), sis_incrzip_getsize(s_));

    sis_bits_stream_init(s_->cur_stream);
    _incrzip_memory_free(s_);
    return s_->zip_bags;
}

// 如果压缩长度大于原始数据长度 就用原始数据 第一位0表示原始数据 1 表示压缩数据
int sis_incrzip_compress_step(s_sis_incrzip_class *s_, int kid_, int sid_, char *in_, size_t ilen_)
{
    if (s_->status != SIS_SIC_STATUS_ENCODE)
    {
        return -1;
    }
    s_sis_incrzip_dbinfo *info = (s_sis_incrzip_dbinfo *)sis_pointer_list_get(s_->dbinfos, sid_);
    if (!info)
    {
        return -2;
    }
    uint8 *buffer = _sis_incrzip_get_curmem(s_, kid_, info);
    if (!buffer)
    {
        return -3;
    }
    if (ilen_ % info->lpdb->size)
    {
        return -4;
    }
    int count = ilen_ / info->lpdb->size;

    int newsize = ilen_ + count * 8 + 64; // 每条记录加8个字节 再加包数量4个字节
    if (newsize > s_->zip_size)
    {
        LOG(5)("catch too small. %d < %d\n", s_->zip_size, newsize);
        return -5;
    }
    int size = sis_incrzip_getsize(s_);
    if (size + newsize > s_->zip_size)
    {
        s_->cb_compress(s_->cb_source, (char *)s_->zip_memory, size);
        // 传递后就从头开始写数据了
        _incrzip_compress_next(s_);
    } 

    _incrzip_compress(s_, buffer, kid_, sid_, count, info, in_, ilen_);
    // sis_out_binary("one", s_->zip_memory, sis_incrzip_getsize(s_));

    return s_->zip_bags;
}


///////////////////
// 解压缩相关
///////////////////
int  sis_incrzip_get_bags(s_sis_incrzip_class *s_)
{
    // 解压时 maxsize就是数据大小
    sis_bits_stream_savepos(s_->cur_stream);
    sis_bits_stream_moveto(s_->cur_stream, s_->cur_stream->maxsize - 8);
    int count = sis_bits_stream_get(s_->cur_stream, 8);
    if (count <= 250)
    {

    }
    else if (count == 251)
    {
        sis_bits_stream_moveto(s_->cur_stream, s_->cur_stream->maxsize - 3*8);
        count = sis_bits_stream_get(s_->cur_stream, 16);
    }
    else 
    {
        sis_bits_stream_moveto(s_->cur_stream, s_->cur_stream->maxsize - 5*8);
        count = sis_bits_stream_get(s_->cur_stream, 32);
    }
    sis_bits_stream_restore(s_->cur_stream);
    s_->zip_bags= count;
    return count;
}
int sis_incrzip_uncompress_start(s_sis_incrzip_class *s_, void *source_, cb_incrzip_decode *cb_)
{
    if (s_->status != SIS_SIC_STATUS_INITED)
    {
        return -1;
    }
    if (!cb_ )
    {
        return -2;
    }
    s_->cb_source = source_;
    s_->cb_uncompress = cb_;
    _incrzip_memory_init(s_);
    s_->unzip_catch = (char *)sis_malloc(s_->maxdbsize);
    s_->status = SIS_SIC_STATUS_DECODE;
    return 0;
}
int sis_incrzip_uncompress_stop(s_sis_incrzip_class *s_)
{
    if (s_->status == SIS_SIC_STATUS_DECODE)
    {
        sis_free(s_->unzip_catch);
        sis_bits_stream_init(s_->cur_stream);
        _incrzip_memory_free(s_);
        s_->cb_source = NULL;
        s_->cb_uncompress = NULL;
        s_->status = SIS_SIC_STATUS_NONE;
    }
    return 0;
}


static inline void _sis_incrzip_uncompress_one(s_sis_incrzip_class *s_,
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
                    sis_bits_stream_get_incr_int(s_->cur_stream, memory_ ? _sis_field_get_int(infield_, memory_, index) : 0),
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
                    sis_bits_stream_get_incr_int(s_->cur_stream, memory_ ? _sis_field_get_uint(infield_, memory_, index) : 0),
                    index);
            }
            break;
        case SIS_DYNAMIC_TYPE_FLOAT:
        case SIS_DYNAMIC_TYPE_PRICE:
            {
                _sis_field_set_float(infield_, in_, 
                    sis_bits_stream_get_incr_float(s_->cur_stream, memory_ ? _sis_field_get_float(infield_, memory_, index) : 0.0, infield_->dot),
                    index);
            }
            break;
        case SIS_DYNAMIC_TYPE_CHAR:
            {
                if (memory_)
                {
                    sis_bits_stream_get_incr_chars(s_->cur_stream, 
                        in_ + infield_->offset + index * infield_->len, infield_->len,
                        (char *)memory_ + infield_->offset + index * infield_->len, infield_->len);
                }
                else
                {
                    sis_bits_stream_get_chars(s_->cur_stream, 
                        in_ + infield_->offset + index * infield_->len, infield_->len);
                }               
            }
            break;
        default:
            break;
        }
    }
}
int sis_incrzip_uncompress(s_sis_incrzip_class *s_, char *in_, size_t ilen_, s_sis_memory *out_)
{
    if (!out_)
    {
        return -1;
    }
    s_sis_incrzip_dbinfo *info = (s_sis_incrzip_dbinfo *)sis_pointer_list_get(s_->dbinfos, 0);
    if (!info)
    {
        return -2;
    }    
    sis_bits_stream_link(s_->cur_stream, (uint8 *)in_, ilen_);
    // 去尾部取标志位
    int bags = sis_incrzip_get_bags(s_);
    if (bags != 1)
    {
        LOG(5)("decode fail 1 != %d\n", bags);
        return 0;
    }    

    sis_bits_stream_get(s_->cur_stream, 1);
    sis_bits_stream_get_uint(s_->cur_stream);
    sis_bits_stream_get_uint(s_->cur_stream);
    int count = sis_bits_stream_get_uint(s_->cur_stream);
    
    sis_memory_set_maxsize(out_, info->lpdb->size * count);
    s_->unzip_catch = sis_malloc(info->lpdb->size + 1);
    memset(s_->unzip_catch, 0 ,info->lpdb->size + 1);
    char *memory = (char *)(s_->unzip_catch + 1);
    size_t offset = 0;
    for (int k = 0; k < count; k++)
    {
        for (int i = 0; i < info->fnums; i++)
        {
            s_sis_dynamic_field *infield = (s_sis_dynamic_field *)sis_map_list_geti(info->lpdb->fields, i);
            _sis_incrzip_uncompress_one(s_, k == 0 ? NULL : memory, infield, sis_memory(out_) + offset);
        }
        memmove(memory, sis_memory(out_) + offset, info->lpdb->size);
        offset += info->lpdb->size;
    }
    sis_memory_set_size(out_, offset);
    sis_free(s_->unzip_catch);
    sis_bits_stream_init(s_->cur_stream);
    return count;
}

// 用回调来返回数据
int sis_incrzip_uncompress_step(s_sis_incrzip_class *s_, char *in_, size_t ilen_)
{
    if (s_->status != SIS_SIC_STATUS_DECODE)
    {
        return -1;
    }
    sis_bits_stream_link(s_->cur_stream, (uint8 *)in_, ilen_);
    // 去尾部取标志位
    int bags = sis_incrzip_get_bags(s_);
    if (bags < 1)
    {
        LOG(5)("decode fail bags= %d\n", bags);
        return 0;
    }    
    int nums = 0;
    while (nums < bags)
    {        
        int zip = sis_bits_stream_get(s_->cur_stream, 1);
        int kidx = sis_bits_stream_get_uint(s_->cur_stream);
        int sidx = sis_bits_stream_get_uint(s_->cur_stream);
        int count = sis_bits_stream_get_uint(s_->cur_stream);

        s_sis_incrzip_dbinfo *info = (s_sis_incrzip_dbinfo *)sis_pointer_list_get(s_->dbinfos, sidx);
        uint8 *buffer = _sis_incrzip_get_curmem(s_, kidx, info);
        if (!buffer)
        {
            printf("decode fail %d %d\n", kidx, sidx);
            return 0;
        }
        char *memory = (char *)&buffer[1];
        for (int k = 0; k < count; k++)
        {
            for (int i = 0; i < info->fnums; i++)
            {
                s_sis_dynamic_field *infield = (s_sis_dynamic_field *)sis_map_list_geti(info->lpdb->fields, i);
                _sis_incrzip_uncompress_one(s_, zip == 0 ? NULL : memory, infield, s_->unzip_catch);
            }
            // sis_out_binary("mm", memory, info->lpdb->size);
            // sis_out_binary("ii", in, info->lpdb->size);
            s_->cb_uncompress(s_->cb_source, kidx, sidx, s_->unzip_catch, info->lpdb->size);    
            memmove(memory, s_->unzip_catch, info->lpdb->size);
        }
        nums++;
    }
    return nums;
}
#if 0

// 用基数加随机数 
// incrzip 601000000 --> 281624252 zip  5179  unzip  3530 共22M条记录
// snappy  601000000 --> 345707938 zip  9059  unzip  2365
// incrzip 60100000  --> 28162215  zip   529  unzip   379 共2.2M条记录
// snappy  60100000  --> 34567537  zip   895  unzip   239
// incrzip 6010000   --> 2596189   zip    55  unzip    35 共220K条记录
// snappy  6010000   --> 3456267   zip    89  unzip    22
// incrzip 60100     --> 25991     zip 1.272  unzip 0.645 共2.2K条记录
// snappy  60100     --> 34583     zip 1.276  unzip 0.315
//  --------  //
// 对数据压缩 incrzip 有明显优势 压缩率高出 0.575 - 0.468 = 10个百分点 
//                压缩速度 快1.7倍 
//                解压速度 慢1.5倍（有待优化）
//                压缩解压一个周期 只有snappy的80%
// 当数据包小于100K时 两种压缩算法snappy有优势 但对于小数据压缩并没有什么意义
// 对存在大量相同数据时 incrzip 没有 snappy 效率高 压缩比也弱于 snappy 
// 对于需要实时压缩的流式数据 incrzip 比 snappy 具备无可比拟的优势
////////////////////////////
// 单一数据结构单一股票压缩比较
// incrzip 432000000 --> 171060195 zip  2151  unzip  2510 10M条记录
// snappy  432000000 --> 240607259 zip  5986  unzip  1453
// 对数据压缩 incrzip 有明显优势 压缩率高出 0.557 - 0.396 = 16个百分点 
//                压缩速度 快2.7倍 
//                解压速度 慢1.7倍（有待优化）
//                压缩解压一个周期 只有snappy的62%
 

#include "sis_memory.h"
#include "sis_snappy.h"
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

int zipnnums = 100*10000;//*100;//0*1000;

int __unsize = 0;
int cb_sis_decode(void *src, int kid,int sid, char *in, size_t ilen)
{
    // sis_out_binary("read", in, ilen);
    ++__unsize;
    if (__unsize % zipnnums == 0)
    {
        s_sis_incrzip_class *zip = (s_sis_incrzip_class *)src;
        s_sis_incrzip_dbinfo *unit=(s_sis_incrzip_dbinfo *)sis_pointer_list_get(zip->dbinfos, sid);
        s_sis_sds out = sis_dynamic_db_to_array_sds(unit->lpdb, unit->lpdb->name, in, ilen);
        printf("%d %s = %s\n", __unsize, unit->lpdb->name, out);
        sis_sdsfree(out);
    }
    return 0;
}
int __zipsize = 0;
int cb_sis_encode(void *src, char *in, size_t ilen)
{
    s_sis_pointer_list *memorys = (s_sis_pointer_list *)src;

    s_sis_memory *memory = sis_memory_create();
    sis_memory_cat(memory, in, ilen);
    sis_pointer_list_push(memorys, memory);
    // sis_memory_set_size(memory, __size);
    // sis_out_binary("encode", in, ilen);
    __zipsize += ilen;
    if (memorys->count % 1000 == 0)
    {
        printf("%d %d %d\n", __zipsize, (int)ilen, memorys->count);
    }
    return 0;
}
// 测试单结构体单股票的压缩
int main()
{
	const char *db_snap = "{snap:{fields:{name:[C,4],open:[I,4],high:[U,4],lowp:[F,4,1,2],newp:[F,8,1,3],vols:[I,8],mony:[U,8],ask:[I,4,2]}}}";

	s_sis_conf_handle *injson = sis_conf_load(db_snap, strlen(db_snap));
	s_sis_dynamic_db *snap = sis_dynamic_db_create(sis_json_first_node(injson->node));
	sis_conf_close(injson);

    int snap_nums = sizeof(snaps) / sizeof(_snap_);
    {
        s_sis_sds in = sis_dynamic_db_to_array_sds(snap, "snap", &snaps[0], snap_nums * sizeof(_snap_));
        printf("%s\n", in);
        sis_sdsfree(in);
    } 
    // 生成数据
    sis_init_random();
    s_sis_memory *memory = sis_memory_create();
    sis_memory_set_maxsize(memory, zipnnums * snap_nums * sizeof(_snap_) + 16*1024*1024);
    for (int k = 0; k < zipnnums; k++)
    {
        for (int i = 0; i < snap_nums; i++)
        {
            _snap_ cursnap;
            memmove(&cursnap, &snaps[i],sizeof(_snap_));
            cursnap.open += sis_int_random(-100,100);
            cursnap.high += sis_int_random(-100,100);
            cursnap.lowp += sis_int_random(-10,10);
            cursnap.newp += sis_int_random(-10,10);
            cursnap.vols += sis_int_random(-100000,1000000);
            cursnap.mony += sis_int_random(-1000000,10000000);
            cursnap.askp[0] += sis_int_random(-1,1);
            cursnap.askp[1] += sis_int_random(-1,1);
            sis_memory_cat(memory, (char *)&cursnap, sizeof(_snap_));
        }
    }
    s_sis_memory *zipmemory = sis_memory_create();
    sis_memory_set_maxsize(zipmemory, zipnnums * snap_nums * sizeof(_snap_) + 16*1024*1024);
    s_sis_memory *unzipmemory = sis_memory_create();
    sis_memory_set_maxsize(unzipmemory, zipnnums * snap_nums * sizeof(_snap_) + 16*1024*1024);

    ////////// incrzip 对比
    msec_t nowmsec = sis_time_get_now_usec();
    printf("incrzip start: %lld\n", nowmsec);
    s_sis_incrzip_class *zip = sis_incrzip_class_create();
    sis_incrzip_set_sdb(zip, snap);
    sis_incrzip_compress(zip, sis_memory(memory), sis_memory_get_size(memory), zipmemory);
    printf("incrzip compress stop : cost :%lld\n", sis_time_get_now_usec() - nowmsec);
    printf("zip : from %zu --> %zu\n", sis_memory_get_size(memory), sis_memory_get_size(zipmemory));

    nowmsec = sis_time_get_now_usec();
    int count = sis_incrzip_uncompress(zip, sis_memory(zipmemory), sis_memory_get_size(zipmemory), unzipmemory);
    printf("incrzip uncompress stop: cost : %zu %lld\n", sis_memory_get_size(unzipmemory), sis_time_get_now_usec() - nowmsec);

    for (int i = 0; i < count; i++)
    {
        cb_sis_decode(zip, 0, 0, sis_memory(unzipmemory), sizeof(_snap_));
        sis_memory_move(unzipmemory, sizeof(_snap_));
    }
    // s_sis_incrzip_class *unzip = sis_incrzip_class_create();
    // sis_incrzip_set_key(unzip, 1);
    // sis_incrzip_set_sdb(unzip, snap);
    // sis_incrzip_uncompress_start(unzip, unzip, cb_sis_decode);
    // sis_incrzip_uncompress_step(unzip, sis_memory(zipmemory), sis_memory_get_size(zipmemory));
    // sis_incrzip_uncompress_stop(unzip);
    // sis_incrzip_class_destroy(unzip);

    sis_memory_destroy(unzipmemory);
    sis_incrzip_class_destroy(zip);
    ////////// snappy 对比
    unzipmemory = sis_memory_create();
    sis_memory_set_maxsize(unzipmemory, zipnnums * snap_nums * sizeof(_snap_) + 16*1024*1024);
    nowmsec = sis_time_get_now_usec();
    printf("snappy start: %lld\n", nowmsec);
    sis_snappy_compress(sis_memory(memory), sis_memory_get_size(memory), zipmemory);
    printf("snappy compress stop : cost :%lld\n", sis_time_get_now_usec() - nowmsec);
    printf("zip : from %zu --> %zu\n", sis_memory_get_size(memory), sis_memory_get_size(zipmemory));

    nowmsec = sis_time_get_now_usec();
    sis_snappy_uncompress(sis_memory(zipmemory), sis_memory_get_size(zipmemory), unzipmemory);
    printf("snappy uncompress stop: cost : %zu %lld\n", sis_memory_get_size(unzipmemory), sis_time_get_now_usec() - nowmsec);

    sis_memory_destroy(unzipmemory);
    ////   --- 结束 --- ///
    sis_memory_destroy(zipmemory);
    sis_memory_destroy(memory);
    
	sis_dynamic_db_destroy(snap);
	return 0;
}

// 测试多结构体多股票的压缩
int main1()
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
    s_sis_incrzip_class *zip = sis_incrzip_class_create();

    sis_incrzip_set_key(zip, 2);
    int snapidx = sis_incrzip_set_sdb(zip, snap);
    int tickidx = sis_incrzip_set_sdb(zip, tick);
    
    printf("[%d,%d] %p %p nums : %d %d\n", snapidx, tickidx, snap, tick, snap_nums, tick_nums);
    

    s_sis_pointer_list *memorys = sis_pointer_list_create();
    memorys->vfree = sis_memory_destroy;

    msec_t nowmsec = sis_time_get_now_usec();
    printf("incrzip start: %lld\n", nowmsec);
    sis_incrzip_compress_start(zip, 16384, memorys, cb_sis_encode);
    sis_init_random();
    int srcsize = 0;
    for (int k = 0; k < zipnnums; k++)
    {
        for (int i = 0; i < sis_max(snap_nums, tick_nums); i++)
        {
            if (i < snap_nums)
            {
                _snap_ cursnap;
                memmove(&cursnap, &snaps[i],sizeof(_snap_));
                cursnap.open += sis_int_random(-100,100);
                cursnap.high += sis_int_random(-100,100);
                cursnap.lowp += sis_int_random(-10,10);
                cursnap.newp += sis_int_random(-10,10);
                cursnap.vols += sis_int_random(-100000,1000000);
                cursnap.mony += sis_int_random(-1000000,10000000);
                cursnap.askp[0] += sis_int_random(-1,1);
                cursnap.askp[1] += sis_int_random(-1,1);
                sis_incrzip_compress_step(zip, 0, snapidx, (char *)&cursnap, sizeof(_snap_));
    // {  "SH", 1000, 2000, 21.3456, 31.4567,   50000000, 1789000000, {1, 2}},
                // sis_incrzip_compress_step(zip, 0, snapidx, &snaps[i], sizeof(_snap_));
                srcsize+=sizeof(_snap_);
            }
            if (i < tick_nums)
            {
                _tick_ curtick;
                memmove(&curtick, &ticks[i],sizeof(_tick_));
                curtick.newp += sis_int_random(-100,100);
                curtick.askn += sis_int_random(1,3);
                curtick.vols += sis_int_random(-100,100);
                sis_incrzip_compress_step(zip, 1, tickidx, (char *)&curtick, sizeof(_tick_));
                // sis_out_binary("in tick", (const char *)&ticks[i], sizeof(_tick_));
    // {  'S', -1200, 2000, 3000},
                // sis_incrzip_compress_step(zip, 1, tickidx, &ticks[i], sizeof(_tick_));
                srcsize+=sizeof(_tick_);
            }
        }
        if (sis_incrzip_getsize(zip) > 64*1024)
        {
            sis_incrzip_compress_restart(zip);
        }
    }
    sis_incrzip_compress_stop(zip);

    printf("incrzip compress stop : cost :%lld\n", sis_time_get_now_usec() - nowmsec);
    printf("zip : from %d --> %d\n", srcsize, __zipsize);
    
    // sis_out_binary("out", sis_memory(out), size);
    sis_incrzip_class_destroy(zip);

    s_sis_incrzip_class *unzip = sis_incrzip_class_create();
    // 解压
    sis_incrzip_set_key(unzip, 10);
    sis_incrzip_set_sdb(unzip, snap);
    sis_incrzip_set_sdb(unzip, tick);

    nowmsec = sis_time_get_now_usec();
    sis_incrzip_uncompress_start(unzip, unzip, cb_sis_decode);
    for (int i = 0; i < memorys->count; i++)
    {
        s_sis_memory *mem = sis_pointer_list_get(memorys, i);
        sis_incrzip_uncompress_step(unzip, sis_memory(mem), sis_memory_get_size(mem));
    }
    sis_incrzip_uncompress_stop(unzip);
    sis_incrzip_class_destroy(unzip);

    printf("incrzip uncompress stop: cost : %lld\n", sis_time_get_now_usec() - nowmsec);

    sis_pointer_list_destroy(memorys);
#if 1
    ////////// snappy 对比
    s_sis_memory *memory = sis_memory_create();
    s_sis_memory *zipmemory = sis_memory_create();

    sis_memory_set_maxsize(memory, srcsize + 16*1024*1024);
    sis_memory_set_maxsize(zipmemory, srcsize + 16*1024*1024);

    nowmsec = sis_time_get_now_usec();
    printf("snappy start: %lld\n", nowmsec);
    for (int k = 0; k < zipnnums; k++)
    {
        for (int i = 0; i < sis_max(snap_nums, tick_nums); i++)
        {
            if (i < snap_nums)
            {
                _snap_ cursnap;
                memmove(&cursnap, &snaps[i],sizeof(_snap_));
                cursnap.open += sis_int_random(-100,100);
                cursnap.high += sis_int_random(-100,100);
                cursnap.lowp += sis_int_random(-10,10);
                cursnap.newp += sis_int_random(-10,10);
                cursnap.vols += sis_int_random(-100000,1000000);
                cursnap.mony += sis_int_random(-1000000,10000000);
                cursnap.askp[0] += sis_int_random(-1,1);
                cursnap.askp[1] += sis_int_random(-1,1);
                sis_memory_cat(memory, (char *)&cursnap, sizeof(_snap_));
                // sis_memory_cat(memory, (char *)&snaps[i], sizeof(_snap_));
            }
            if (i < tick_nums)
            {
                // sis_out_binary("in tick", (const char *)&ticks[i], sizeof(_tick_));
                _tick_ curtick;
                memmove(&curtick, &ticks[i],sizeof(_tick_));
                curtick.newp += sis_int_random(-100,100);
                curtick.askn += sis_int_random(1,3);
                curtick.vols += sis_int_random(-100,100);
                sis_memory_cat(memory, (char *)&curtick, sizeof(_tick_));
                // sis_memory_cat(memory, (char *)&ticks[i], sizeof(_tick_));
            }
        }
    }
    printf("snappy write OK: %lld\n", sis_time_get_now_usec() - nowmsec);
    sis_snappy_compress(sis_memory(memory), sis_memory_get_size(memory), zipmemory);
    printf("snappy compress stop : cost :%lld\n", sis_time_get_now_usec() - nowmsec);
    printf("zip : from %zu --> %zu\n", sis_memory_get_size(memory), sis_memory_get_size(zipmemory));

    nowmsec = sis_time_get_now_usec();

    sis_snappy_uncompress(sis_memory(zipmemory), sis_memory_get_size(zipmemory), memory);
    printf("snappy uncompress stop: cost : %zu %lld\n", sis_memory_get_size(memory), sis_time_get_now_usec() - nowmsec);

    sis_memory_destroy(zipmemory);
    sis_memory_destroy(memory);

    ////   --- 结束 --- ///
#endif
    
	sis_dynamic_db_destroy(snap);
	sis_dynamic_db_destroy(tick);
	return 0;
}

#endif