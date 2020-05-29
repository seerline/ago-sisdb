#include <sis_modules.h>

extern s_sis_modules sis_modules_memdb;
extern s_sis_modules sis_modules_sisdb;
extern s_sis_modules sis_modules_trade_drive_fast;
extern s_sis_modules sis_modules_trade_drive_real;

s_sis_modules *__modules[] = {
    &sis_modules_memdb,
    &sis_modules_sisdb,
    &sis_modules_trade_drive_fast,
    &sis_modules_trade_drive_real,
    0
  };

const char *__modules_name[] = {
    "memdb",
    "sisdb",
    "trade_drive_fast",
    "trade_drive_real",
    0
  };
