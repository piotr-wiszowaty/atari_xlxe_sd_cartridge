OBXS = player.obx boot.obx test.obx test3.obx boot2.obx test4.obx test5.obx test6.obx test7.obx
HDRS = player.h

all: $(HDRS) $(OBXS)

%.obx: %.asx
	xasm /o:$@ /l:$@.lst $<

%.h: %.obx
	./bin2h $^ | sed -e 's/char array/char bootstrap/' > $@

boot2.asx: boot2.forth
	foco65 -p '$$700' $^ > $@

clean:
	rm -f $(OBXS) $(HDRS)
