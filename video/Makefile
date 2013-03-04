emu: ../atari800/src/atari800 player.bin film.xex
	$< -console -avic player.bin film.xex

all: film.xex send

send: send.exe player.bin
	./send.exe player.bin COM19

send.exe: send.cs
	csc -out:$@ $<

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

../atari800/src/atari800:
	$(MAKE) -C ../atari800/src

atari800-avic.patch:
	diff -ruN -x Makefile -x '*.o' -x 'atari800*' -x autom4te.cache -x 'config*' -x atari.ro ../../emusrc/atari800 ../atari800 >$@ || exit 0

clean:
	rm send.exe player.bin film.xex yuv2hip.exe hip8000.obx hipa000.obx

.PHONY: emu all send ../atari800/src/atari800 atari800-avic.patch clean

.DELETE_ON_ERROR:
