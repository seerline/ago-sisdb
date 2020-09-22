
#include "sis_snappy.h"

////////////////////////////////////////////////////////
// snappy compress
////////////////////////////////////////////////////////

int sis_snappy_compress(char *in_, size_t ilen_, s_sis_memory *out_)
{
    // if (!__snappyc_env_inited)
    // {
    //     snappy_init_env(&__snappyc_env);
    //     __snappyc_env_inited = 1;
    // }
    sis_memory_clear(out_);
    size_t output_length = snappy_max_compressed_length(ilen_);
    if (output_length > 0)
    {
        sis_memory_set_maxsize(out_, sis_memory_get_size(out_) + output_length + 1);
        // if (snappy_compress(&__snappyc_env, in_, ilen_, sis_memory(out_), &output_length) == SNAPPY_OK)
        if (snappy_compress(in_, ilen_, sis_memory(out_), &output_length) == SNAPPY_OK)
        {
            if (output_length < ilen_)
            {
                sis_memory_set_size(out_, output_length);
                return 1;
            }
        }
    }
    // 压缩失败或长度更大
    return 0;
}

int sis_snappy_uncompress(char *in_, size_t ilen_, s_sis_memory *out_)
{
    // if (!__snappyc_env_inited)
    // {
    //     snappy_init_env(&__snappyc_env);
    //     __snappyc_env_inited = 1;
    // }
    sis_memory_clear(out_);
    size_t output_length = 0;
    if (snappy_uncompressed_length(in_, ilen_, &output_length) == SNAPPY_OK)
    // if (snappy_uncompressed_length(in_, ilen_, &output_length))
    {
        sis_memory_set_maxsize(out_, output_length + 1);
        if (snappy_uncompress(in_, ilen_, sis_memory(out_), &output_length) == SNAPPY_OK)
        // if (snappy_uncompress(in_, ilen_, sis_memory(out_)) == SNAPPY_OK)
        {
            // 成功
            sis_memory_set_size(out_, output_length);
            return 1;
        }
    }
    return 0;
}

bool sis_net_snappy_compress(s_sis_memory *in_, s_sis_memory *out_)
{
    return (bool)sis_snappy_compress(sis_memory(in_), sis_memory_get_size(in_), out_);
}
bool sis_net_snappy_uncompress(s_sis_memory *in_, s_sis_memory *out_)
{
    return (bool)sis_snappy_uncompress(sis_memory(in_), sis_memory_get_size(in_), out_);
}
