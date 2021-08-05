#ifndef _SIS_SNAPPY_H
#define _SIS_SNAPPY_H

#include "sis_memory.h"
#include "snappy-c.h"

#include "sis_net.msg.h"
///////////////////////////
//  snappy compress
///////////////////////////
int sis_snappy_compress(char *in_, size_t ilen_, s_sis_memory *out_);
int sis_snappy_uncompress(char *in_, size_t ilen_, s_sis_memory *out_);

bool sis_net_snappy_compress(s_sis_memory *in_, s_sis_memory *out_);
bool sis_net_snappy_uncompress(s_sis_memory *in_, s_sis_memory *out_);


#endif