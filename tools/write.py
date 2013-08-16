#!/usr/bin/env python

import config
import serial
import sys

def to_num(s):
    if s.startswith("0x"):
        return int(s[2:], 16)
    else:
        return int(s)

if len(sys.argv) < 2:
    print "usage: write.py FILE [START-ADDRESS [COUNT]]"
    sys.exit(1)

with open(sys.argv[1], "rb") as f:
    data = f.read()
if len(sys.argv) > 2:
    start_address = to_num(sys.argv[2])
else:
    start_address = 0xbf00
if len(sys.argv) > 3:
    count = to_num(sys.argv[3])
else:
    count = len(data)

s = serial.Serial(config.DEV_PATH, baudrate=config.BAUD)

s.write("w")
s.write(chr(start_address & 0xff))
s.write(chr((start_address >> 8) & 0xff))
s.write(chr(count & 0xff))
s.write(chr((count >> 8) & 0xff))
s.write(data[:count])
if s.read(1) != "w":
    print "error"

s.close()
