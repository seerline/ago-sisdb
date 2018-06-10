
#include "sts_catch.h"

s_sts_sds sts_catch_get_info(s_digger_server *s_,const char *com_)
{
	s_sts_sds o = get_stsdb_info_sds(s_, "sh600600","now", NULL);
    return o;
}
