#ifndef _CALL_H
#define _CALL_H

#include <sts_core.h>
#include <sts_comm.h>
#include <server.h>

int digger_create(const char *conf_);

int digger_start(s_sts_module_context *ctx_,const char *name_, const char *com_);
int digger_cancel(s_sts_module_context *ctx_,const char *key_);
int digger_stop(s_sts_module_context *ctx_,const char *key_);

int digger_get(s_sts_module_context *ctx_,const char *key_, const char *com_);
int digger_set(s_sts_module_context *ctx_,const char *db_, const char *key_, const char *com_);

int digger_sub(s_sts_module_context *ctx_,const char *db_, const char *key_);
int digger_pub(s_sts_module_context *ctx_,const char *db_, const char *key_, const char *com_);

int stsdb_get(s_sts_module_context *ctx_,const char *db_, const char *key_, const char *com_);

#endif