
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _STS_DB_IO_H
#define _STS_DB_IO_H

#include "sts_table.h"

int stsdb_get(s_sts_module_context *ctx_,const char *db_, const char *key_, const char *com_);

int stsdb_set_json(s_sts_module_context *ctx_,const char *db_, const char *key_, const char *val_);
int stsdb_set_struct(s_sts_module_context *ctx_,const char *db_, const char *key_, const char *val_);




#endif  /* _STS_DB_IO_H */
