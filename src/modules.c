#include <sis_modules.h>

extern s_sis_modules sis_modules_sisdb_disk;

s_sis_modules *__modules[] = {
    &sis_modules_sisdb_disk,
    0
  };

const char *__modules_name[] = {
    "sisdb_disk",
    0
  };
