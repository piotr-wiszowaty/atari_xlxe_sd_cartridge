NTSC = 0
SRC_VIDEO = ../../fr-08.avi
DEST_VIDEO = film.xex

all: $(DEST_VIDEO)

$(DEST_VIDEO): xexenc gr88000.obx gr8a000.obx gr98000.obx gr9a000.obx hip8000.obx hipa000.obx head.obx tail.obx $(SRC_VIDEO)
	./xexenc --output=$@ $(SRC_VIDEO)

xexenc: xexenc.c
	gcc -o $@ -s -O2 -Wall --std=gnu99 -D NTSC=$(NTSC) $< -l avcodec -l avfilter -l avformat -l avutil

gr88000.obx: gr8.asx
	xasm -q -d NTSC=$(NTSC) -d main=\$$8006 -o $@ $<

gr8a000.obx: gr8.asx
	xasm -q -d NTSC=$(NTSC) -d main=\$$a006 -o $@ $<

gr98000.obx: gr9.asx
	xasm -q -d NTSC=$(NTSC) -d main=\$$8006 -o $@ $<

gr9a000.obx: gr9.asx
	xasm -q -d NTSC=$(NTSC) -d main=\$$a006 -o $@ $<

hip8000.obx: hip.asx
	xasm -q -d NTSC=$(NTSC) -d main=\$$8006 -d mode=\$$40 -o $@ $<

hipa000.obx: hip.asx
	xasm -q -d NTSC=$(NTSC) -d main=\$$a006 -d mode=\$$80 -o $@ $<

head.obx: head.asx
	xasm -q -o $@ $<

tail.obx: tail.asx
	xasm -q -o $@ $<

clean:
	$(RM) $(DEST_VIDEO) xexenc gr88000.obx gr8a000.obx gr98000.obx gr9a000.obx hip8000.obx hipa000.obx head.obx tail.obx

.PHONY: all clean

.DELETE_ON_ERROR:
