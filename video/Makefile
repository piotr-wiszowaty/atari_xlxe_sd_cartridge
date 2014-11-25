SRC_VIDEO = film.mp4

all: xexenc hip8000.obx hipa000.obx head.obx tail.obx $(SRC_VIDEO)
	./xexenc $(SRC_VIDEO)

xexenc: xexenc.c
	gcc -o $@ -s -O2 -Wall --std=c99 $< -l avcodec -l avfilter -l avformat -l avutil

hip8000.obx: hip.asx
	xasm -q -d main=\$$8006 -d mode=\$$40 -o $@ $<

hipa000.obx: hip.asx
	xasm -q -d main=\$$a006 -d mode=\$$80 -o $@ $<

head.obx: head.asx
	xasm -q -o $@ $<

tail.obx: tail.asx
	xasm -q -o $@ $<

clean:
	$(RM) film.xex xexenc hip8000.obx hipa000.obx head.obx tail.obx

.PHONY: all clean

.DELETE_ON_ERROR:
