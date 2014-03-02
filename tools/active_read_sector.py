#!/usr/bin/env python

import config
import serial
import sys

def read_int(s):
    return ord(s.read()) | (ord(s.read()) << 8) | (ord(s.read()) << 16) | (ord(s.read()) << 24)

s = serial.Serial(config.DEV_PATH, baudrate=config.BAUD)

s.write("a")
if s.read(1) != "a":
    print "error"
    s.close()
    sys.exit(1)
active_read_sector = read_int(s)
print "sector: %08X" % active_read_sector

s.close()
