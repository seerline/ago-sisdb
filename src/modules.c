#include <sis_modules.h>

extern s_sis_modules sis_modules_digger_bayes;
extern s_sis_modules sis_modules_digger_core;
extern s_sis_modules sis_modules_digger_ding;
extern s_sis_modules sis_modules_digger_inout_factor;
extern s_sis_modules sis_modules_digger_inout_signal;
extern s_sis_modules sis_modules_digger_inout_study;
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
extern s_sis_modules sis_modules_market_file2;
extern s_sis_modules sis_modules_market_link;
extern s_sis_modules sis_modules_market_rfile;
extern s_sis_modules sis_modules_market_seer;
extern s_sis_modules sis_modules_market_seer_rt;
extern s_sis_modules sis_modules_market_server;
extern s_sis_modules sis_modules_market_sisdb;
extern s_sis_modules sis_modules_market_wfile;
extern s_sis_modules sis_modules_net_rs;
extern s_sis_modules sis_modules_net_vpn;
extern s_sis_modules sis_modules_net_ws;
extern s_sis_modules sis_modules_source_datayes;
extern s_sis_modules sis_modules_source_demo;
extern s_sis_modules sis_modules_source_server;
extern s_sis_modules sis_modules_trade_drive_demo;
extern s_sis_modules sis_modules_trade_drive_easy;
extern s_sis_modules sis_modules_trade_drive_fast;
extern s_sis_modules sis_modules_trade_drive_real;
extern s_sis_modules sis_modules_trade_drive_wb;
extern s_sis_modules sis_modules_trade_maker_wb;
extern s_sis_modules sis_modules_trade_out_csv;
extern s_sis_modules sis_modules_trade_out_sisdb;
extern s_sis_modules sis_modules_trade_service;
extern s_sis_modules sis_modules_trade_signal_mq;
extern s_sis_modules sis_modules_trade_signal_wb;
extern s_sis_modules sis_modules_work_east2day;
extern s_sis_modules sis_modules_work_sno2after;
extern s_sis_modules sis_modules_work_zh2cw;
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
    &sis_modules_digger_bayes,
    &sis_modules_digger_core,
    &sis_modules_digger_ding,
    &sis_modules_digger_inout_factor,
    &sis_modules_digger_inout_signal,
    &sis_modules_digger_inout_study,
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
    &sis_modules_market_file2,
    &sis_modules_market_link,
    &sis_modules_market_rfile,
    &sis_modules_market_seer,
    &sis_modules_market_seer_rt,
    &sis_modules_market_server,
    &sis_modules_market_sisdb,
    &sis_modules_market_wfile,
    &sis_modules_net_rs,
    &sis_modules_net_vpn,
    &sis_modules_net_ws,
    &sis_modules_source_datayes,
    &sis_modules_source_demo,
    &sis_modules_source_server,
    &sis_modules_trade_drive_demo,
    &sis_modules_trade_drive_easy,
    &sis_modules_trade_drive_fast,
    &sis_modules_trade_drive_real,
    &sis_modules_trade_drive_wb,
    &sis_modules_trade_maker_wb,
    &sis_modules_trade_out_csv,
    &sis_modules_trade_out_sisdb,
    &sis_modules_trade_service,
    &sis_modules_trade_signal_mq,
    &sis_modules_trade_signal_wb,
    &sis_modules_work_east2day,
    &sis_modules_work_sno2after,
    &sis_modules_work_zh2cw,
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
    "digger_bayes",
    "digger_core",
    "digger_ding",
    "digger_inout_factor",
    "digger_inout_signal",
    "digger_inout_study",
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
    "market_file2",
    "market_link",
    "market_rfile",
    "market_seer",
    "market_seer_rt",
    "market_server",
    "market_sisdb",
    "market_wfile",
    "net_rs",
    "net_vpn",
    "net_ws",
    "source_datayes",
    "source_demo",
    "source_server",
    "trade_drive_demo",
    "trade_drive_easy",
    "trade_drive_fast",
    "trade_drive_real",
    "trade_drive_wb",
    "trade_maker_wb",
    "trade_out_csv",
    "trade_out_sisdb",
    "trade_service",
    "trade_signal_mq",
    "trade_signal_wb",
    "work_east2day",
    "work_sno2after",
    "work_zh2cw",
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
