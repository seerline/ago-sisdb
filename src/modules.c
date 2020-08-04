#include <sis_modules.h>

extern s_sis_modules sis_modules_sisdb_client;

s_sis_modules *__modules[] = {
    &sis_modules_sisdb_client,
    0
  };

const char *__modules_name[] = {
    "sisdb_client",
    0
  };
