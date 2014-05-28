all: film.xex

player.bin: player.asx
	xasm -q -o $@ $<

film.xex: yuv2hip.exe stream.yuv audiodump.pcm hip8000.obx hipa000.obx
	./yuv2hip.exe

yuv2hip.exe: yuv2hip.c
	gcc -s -O2 -o $@ $<

hip8000.obx: hip.asx
	xasm -q -d main=\$$8006 -d mode=\$$40 -o $@ $<

hipa000.obx: hip.asx
	xasm -q -d main=\$$a006 -d mode=\$$80 -o $@ $<

clean:
	rm player.bin film.xex yuv2hip.exe hip8000.obx hipa000.obx

.PHONY: all clean

.DELETE_ON_ERROR:
