#!/usr/bin/env python

import config
import serial
import sys

def to_num(s):
    if s.startswith("0x"):
        return int(s[2:], 16)
    else:
        return int(s)

if len(sys.argv) < 3:
    print "usage: read.py [-o FILE] START-ADDRESS COUNT"
    sys.exit(1)

k = 1
if sys.argv[k] == "-o":
    output_path = sys.argv[k+1]
    k = 3
else:
    output_path = None
start_address = to_num(sys.argv[k])
count = to_num(sys.argv[k+1])

s = serial.Serial(config.DEV_PATH, baudrate=config.BAUD)

s.write("r")
s.write(chr(start_address & 0xff))
s.write(chr((start_address >> 8) & 0xff))
s.write(chr(count & 0xff))
s.write(chr((count >> 8) & 0xff))
data = s.read(count)

s.close()

i = 0
a = start_address
for b in data:
    if (i & 0x0f) == 0: print "%04X " % a,
    a += 1
    print "%02X" % ord(b),
    if (i & 0x0f) in [0x03, 0x0b]: print "",
    elif (i & 0x0f) == 0x07: print "-",
    elif (i & 0x0f) == 0x0f: print ""
    i += 1

if output_path is not None:
    with open(output_path, "wb") as f:
        f.write(data)
