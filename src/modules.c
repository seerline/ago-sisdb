#include <sis_modules.h>

extern s_sis_modules sis_modules_memdb;
extern s_sis_modules sis_modules_sisdb;
extern s_sis_modules sis_modules_sisdb_catch;
extern s_sis_modules sis_modules_sisdb_disk;
extern s_sis_modules sis_modules_sisdb_server;

s_sis_modules *__modules[] = {
    &sis_modules_memdb,
    &sis_modules_sisdb,
    &sis_modules_sisdb_catch,
    &sis_modules_sisdb_disk,
    &sis_modules_sisdb_server,
    0
  };

const char *__modules_name[] = {
    "memdb",
    "sisdb",
    "sisdb_catch",
    "sisdb_disk",
    "sisdb_server",
    0
  };
