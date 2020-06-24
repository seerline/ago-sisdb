#include <sis_modules.h>

extern s_sis_modules sis_modules_digger_bayes;
extern s_sis_modules sis_modules_digger_core;
extern s_sis_modules sis_modules_digger_ding;
extern s_sis_modules sis_modules_digger_nbt;
extern s_sis_modules sis_modules_digger_series;
extern s_sis_modules sis_modules_factor_drift;
extern s_sis_modules sis_modules_factor_near;
extern s_sis_modules sis_modules_macd;
extern s_sis_modules sis_modules_sign_money;
extern s_sis_modules sis_modules_sign_mp;
extern s_sis_modules sis_modules_sign_newp;
extern s_sis_modules sis_modules_market_api_wb;
extern s_sis_modules sis_modules_market_api_wbtop;
extern s_sis_modules sis_modules_market_collect;
extern s_sis_modules sis_modules_market_file2;
extern s_sis_modules sis_modules_market_rfile;
extern s_sis_modules sis_modules_market_wfile;
extern s_sis_modules sis_modules_net_rs;
extern s_sis_modules sis_modules_net_vpn;
extern s_sis_modules sis_modules_net_ws;
extern s_sis_modules sis_modules_trade_api_wb;
extern s_sis_modules sis_modules_trade_drive_easy;
extern s_sis_modules sis_modules_trade_drive_fast;
extern s_sis_modules sis_modules_trade_drive_real;

s_sis_modules *__modules[] = {
    &sis_modules_digger_bayes,
    &sis_modules_digger_core,
    &sis_modules_digger_ding,
    &sis_modules_digger_nbt,
    &sis_modules_digger_series,
    &sis_modules_factor_drift,
    &sis_modules_factor_near,
    &sis_modules_macd,
    &sis_modules_sign_money,
    &sis_modules_sign_mp,
    &sis_modules_sign_newp,
    &sis_modules_market_api_wb,
    &sis_modules_market_api_wbtop,
    &sis_modules_market_collect,
    &sis_modules_market_file2,
    &sis_modules_market_rfile,
    &sis_modules_market_wfile,
    &sis_modules_net_rs,
    &sis_modules_net_vpn,
    &sis_modules_net_ws,
    &sis_modules_trade_api_wb,
    &sis_modules_trade_drive_easy,
    &sis_modules_trade_drive_fast,
    &sis_modules_trade_drive_real,
    0
  };

const char *__modules_name[] = {
    "digger_bayes",
    "digger_core",
    "digger_ding",
    "digger_nbt",
    "digger_series",
    "factor_drift",
    "factor_near",
    "macd",
    "sign_money",
    "sign_mp",
    "sign_newp",
    "market_api_wb",
    "market_api_wbtop",
    "market_collect",
    "market_file2",
    "market_rfile",
    "market_wfile",
    "net_rs",
    "net_vpn",
    "net_ws",
    "trade_api_wb",
    "trade_drive_easy",
    "trade_drive_fast",
    "trade_drive_real",
    0
  };
