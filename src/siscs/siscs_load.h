
//******************************************************
// Copyright (C) 2018, Martin <seerlinecoin@gmail.com>
//*******************************************************

#ifndef _SISCS_LOAD_H
#define _SISCS_LOAD_H

#include <siscs_io.h>
#include <os_thread.h>
#include <sis_comm.h>
#include <sis_str.h>

s_sis_sds siscs_send_local_sds(const char *command_, const char *key_, 
					     	   const char *val_, size_t ilen_);
#endif  /* _SISCS_LOAD_H */
