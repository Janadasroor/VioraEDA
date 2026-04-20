import ctypes
import os

ng = ctypes.CDLL("/home/jnd/cpp_projects/VioMATRIXC/releasesh/src/.libs/libngspice.so")

def cbSendChar(msg, id, udata):
    print("CHAR:", msg.decode('utf-8'))
    return 0

def cbSendStat(msg, id, udata):
    # print("STAT:", msg.decode('utf-8'))
    return 0

def cbControlledExit(status, is_unload, quit, id, udata):
    print("EXIT:", status)
    return 0

def cbSendData(vecs, num, id, udata):
    return 0

def cbSendInitData(info, id, udata):
    return 0

def cbBGThreadRunning(finished, id, udata):
    return 0

cb_char = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p)(cbSendChar)
cb_stat = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p)(cbSendStat)
cb_exit = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_int, ctypes.c_bool, ctypes.c_bool, ctypes.c_int, ctypes.c_void_p)(cbControlledExit)
cb_data = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p)(cbSendData)
cb_init = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p)(cbSendInitData)
cb_bg = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_bool, ctypes.c_int, ctypes.c_void_p)(cbBGThreadRunning)

ng.ngSpice_Init(cb_char, cb_stat, cb_exit, cb_data, cb_init, cb_bg, None)

os.environ["SPICE_SCRIPTS"] = "/home/jnd/.ngspice"
ng.ngSpice_Command(b"codemodel /home/jnd/.ngspice/spice2poly.cm")
ng.ngSpice_Command(b"codemodel /home/jnd/.ngspice/analog.cm")
ng.ngSpice_Command(b"codemodel /home/jnd/.ngspice/digital.cm")
ng.ngSpice_Command(b"codemodel /home/jnd/.ngspice/xtradev.cm")
ng.ngSpice_Command(b"codemodel /home/jnd/.ngspice/xtraevt.cm")

ng.ngSpice_Command(b"set ngbehavior=ltps")
ng.ngSpice_Command(b"set vicompat=ltps")

lines = [
    "* test subckt",
    "V1 1 0 5",
    "XMM_DAC_AUTO_Net7 Net7 __MM_DAC_AUTO_Net7 __viospice_dac_wrap",
    ".model __viospice_dac_bridge dac_bridge(out_low=0 out_high=5 out_undef=2.5 input_load=1e-12 t_rise=1e-09 t_fall=1e-09)",
    ".subckt __viospice_dac_wrap DIG ANA",
    "A_DAC [DIG] [ANA] __viospice_dac_bridge",
    ".ends __viospice_dac_wrap",
    ".control",
    "op",
    ".endc",
    ".end"
]

for line in lines:
    ng.ngSpice_Command(f"circbyline {line}".encode('utf-8'))

ng.ngSpice_Command(b"bg_run")

import time
time.sleep(1)
