#include <sis_modules.h>

extern s_sis_modules sis_modules_demo_method;
extern s_sis_modules sis_modules_demo_service;
extern s_sis_modules sis_modules_memdb;
extern s_sis_modules sis_modules_sisdb;
extern s_sis_modules sis_modules_sisdb_client;
extern s_sis_modules sis_modules_sisdb_rsdb;
extern s_sis_modules sis_modules_sisdb_rsno;
extern s_sis_modules sis_modules_sisdb_server;
extern s_sis_modules sis_modules_sisdb_wlog;
extern s_sis_modules sis_modules_sisdb_wsdb;
extern s_sis_modules sis_modules_sisdb_wsno;
extern s_sis_modules sis_modules_snodb;

s_sis_modules *__modules[] = {
    &sis_modules_demo_method,
    &sis_modules_demo_service,
    &sis_modules_memdb,
    &sis_modules_sisdb,
    &sis_modules_sisdb_client,
    &sis_modules_sisdb_rsdb,
    &sis_modules_sisdb_rsno,
    &sis_modules_sisdb_server,
    &sis_modules_sisdb_wlog,
    &sis_modules_sisdb_wsdb,
    &sis_modules_sisdb_wsno,
    &sis_modules_snodb,
    0
  };

const char *__modules_name[] = {
    "demo_method",
    "demo_service",
    "memdb",
    "sisdb",
    "sisdb_client",
    "sisdb_rsdb",
    "sisdb_rsno",
    "sisdb_server",
    "sisdb_wlog",
    "sisdb_wsdb",
    "sisdb_wsno",
    "snodb",
    0
  };
