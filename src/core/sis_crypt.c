
#include <sis_crypt.h>

bool sis_net_ssl_encrypt(s_sis_memory *in_, s_sis_memory *out_)
{
    sis_memory_swap(in_, out_);
    return true;
}
bool sis_net_ssl_decrypt(s_sis_memory *in_, s_sis_memory *out_)
{
    sis_memory_swap(in_, out_);
    return true;
}
