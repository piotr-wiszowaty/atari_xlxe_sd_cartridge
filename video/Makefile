SRC_VIDEO = film.mp4

all: film.xex

film.xex: yuv2hip stream.yuv audiodump.pcm hip8000.obx hipa000.obx head.obx tail.obx
	./yuv2hip

yuv2hip: yuv2hip.c
	gcc -s -O2 -o $@ $<

hip8000.obx: hip.asx
	xasm -q -d main=\$$8006 -d mode=\$$40 -o $@ $<

hipa000.obx: hip.asx
	xasm -q -d main=\$$a006 -d mode=\$$80 -o $@ $<

head.obx: head.asx
	xasm -o $@ $<

tail.obx: tail.asx
	xasm -o $@ $<

stream.yuv: $(SRC_VIDEO)
	mplayer -benchmark -ao pcm:nowaveheader -af channels=1,resample=15600,format=u8 -vf scale=160:90 -vo yuv4mpeg $<

clean:
	$(RM) film.xex yuv2hip hip8000.obx hipa000.obx stream.yuv audiodump.pcm head.obx tail.obx

.PHONY: all clean

.DELETE_ON_ERROR:
