import sys

with open("test3.obx", "rb") as f:
    data = f.read()

dir = sys.argv[1]
antic_digits = [0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26]

for i in range(int(sys.argv[2])):
    with open("%s/%04X.obx" % (dir, i), "wb") as f:
        n = "".join(map(lambda c: chr(antic_digits[int(c, 16)]), "%04X" % i))
        f.write(data.replace("\xff\xff\xff\xff", n))
