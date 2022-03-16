
#ifndef _SIS_CRYPT_H
#define _SIS_CRYPT_H

#include <sis_net.msg.h>
#include <sis_memory.h>
#include <sis_obj.h>

bool sis_net_ssl_encrypt(s_sis_memory *in_, s_sis_memory *out_);
bool sis_net_ssl_decrypt(s_sis_memory *in_, s_sis_memory *out_);

#endif //_SIS_CRYPT_H
