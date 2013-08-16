#!/usr/bin/env python

import config
import serial
import sys

def read_int(s):
    return ord(s.read()) | (ord(s.read()) << 8) | (ord(s.read()) << 16) | (ord(s.read()) << 24)

s = serial.Serial(config.DEV_PATH, baudrate=config.BAUD)

s.write("t")
if s.read(1) != "t":
    print "error"
    s.close()
    sys.exit(1)
first = read_int(s)
max_first = read_int(s)
second = read_int(s)
max_second = read_int(s)
total = read_int(s)
max_total = read_int(s)
print "1st (max):   %d (%d)" % (first, max_first)
print "2nd (max):   %d (%d)" % (second, max_second)
print "2nd-1st:     %d" % (second - first)
print "total (max): %d (%d)" % (total, max_total)

s.close()
