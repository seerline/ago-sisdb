#include <sis_modules.h>

extern s_sis_modules sis_modules_memdb;
extern s_sis_modules sis_modules_sisdb;
extern s_sis_modules sis_modules_sisdb_client;
extern s_sis_modules sis_modules_sisdb_disk;
extern s_sis_modules sis_modules_sisdb_server;
extern s_sis_modules sis_modules_sisdb_wlog;

s_sis_modules *__modules[] = {
    &sis_modules_memdb,
    &sis_modules_sisdb,
    &sis_modules_sisdb_client,
    &sis_modules_sisdb_disk,
    &sis_modules_sisdb_server,
    &sis_modules_sisdb_wlog,
    0
  };

const char *__modules_name[] = {
    "memdb",
    "sisdb",
    "sisdb_client",
    "sisdb_disk",
    "sisdb_server",
    "sisdb_wlog",
    0
  };
