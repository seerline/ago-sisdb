import sys
import time
import ctypes
import platform

sysstr = platform.system()
if(sysstr =="Windows"):
	print ("load windows lib....")
	sisdb_client = ctypes.cdll.LoadLibrary("../api_sisdb/lib/api_sisdb.dll")
elif(sysstr == "Linux"):
	print ("load linux lib....")
	sisdb_client = ctypes.cdll.LoadLibrary("../api_sisdb/lib/api_sisdb.so")
elif(sysstr == "Darwin"):
    print ("load apple lib....")
    sisdb_client = ctypes.cdll.LoadLibrary("../api_sisdb/lib/api_sisdb.dylib")
else:
	print ("other system ! exit")
	sys.exit(0)

def cb_sub_command(source, value):
    if not value is None:
        print("cb_sub_command : %s" % value.decode())
    return 0

def cb_sub_start(source, value):
    if not value is None:
        print("cb_sub_start : %s" % value.decode())
    return 0

def cb_sub_stop(source, value):
    if not value is None:
        print("cb_sub_stop : %s" % value.decode())
    return 0

def cb_stk_snapshot(source, value):
    if not value is None:
        print("cb_stk_snapshot : %s" % value.decode())
    return 0

def cb_idx_snapshot(source, value):
    if not value is None:
        print("cb_idx_snapshot : %s" % value.decode())
    return 0

def cb_stk_transact(source, value):
    if not value is None:
        print("cb_stk_transact : %s" % value.decode())
    return 0

CMPFUNC = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p) 
_cb_sub_command  = CMPFUNC(cb_sub_command)
_cb_sub_start    = CMPFUNC(cb_sub_start)
_cb_sub_stop     = CMPFUNC(cb_sub_stop)
_cb_stk_snapshot = CMPFUNC(cb_stk_snapshot)
_cb_idx_snapshot = CMPFUNC(cb_idx_snapshot)
_cb_stk_transact = CMPFUNC(cb_stk_transact)

# handle = sisdb_client.api_sisdb_client_create(b"woan2007.ticp.io", 7329, b"", b"");
# handle = sisdb_client.api_sisdb_client_create(b"223.166.74.203", 7329, b"", b"");
handle = sisdb_client.api_sisdb_client_create(b"192.168.3.118", 7329, b"", b"");
print("handle : ", handle);

stock = "*.stk_snapshot"
sisdb_client.api_sisdb_command_ask(handle, 
    b"sdb.get", 
    stock.encode(), 
    # b"{\"date\":20200702,\"format\":\"csv\"}",
    b"{\"date\":20200511,\"format\":\"csv\"}",
    # b"{\"date\":20200801,\"format\":\"csv\"}",
    _cb_stk_snapshot);

# sisdb_client.api_sisdb_command_ask(handle, 
#     b"show", 
#     b"", 
#     b"",
#     _cb_stk_command);

for i in range(1,100):
	time.sleep(1)
	pass

sisdb_client.api_sisdb_client_destroy(handle);