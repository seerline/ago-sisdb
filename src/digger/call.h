#ifndef _CALL_H
#define _CALL_H

#include <sts_core.h>
#include <comm.h>


int digger_create(s_sts_module_context *ctx_,const char *);
int digger_load_from_file(s_sts_module_context *ctx_,const char *dbname_, const char *filename_, size_t slen_);
int digger_get(s_sts_module_context *ctx_,const char *dbname_, const char *key_, const char *command);

#endif