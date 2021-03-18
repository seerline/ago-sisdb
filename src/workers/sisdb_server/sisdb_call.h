
//******************************************************
// Copyright (C) 2018, Coollyer <48707400@qq.com>
//*******************************************************

#ifndef _SISDB_CALL_H
#define _SISDB_CALL_H

#include "sis_core.h"
#include "sis_conf.h"
#include "sis_str.h"
#include "sis_file.h"
#include "sis_obj.h"

#include "sisdb_server.h"

// #define SISDB_COM_LAST  "{\"search\":{\"start\":-1,\"count\":1}}"

int cmd_sisdb_server_inout(void *worker_, void *argv_);

#endif  /* _SISDB_CALL_H */
