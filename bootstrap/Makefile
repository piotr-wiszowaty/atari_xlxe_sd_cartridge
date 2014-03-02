OBXS = player.obx boot.obx test.obx
HDRS = player.h

all: $(HDRS) $(OBXS)

%.obx: %.asx
	xasm /o:$@ /l:$@.lst $<

%.h: %.obx
	./bin2h $^ | sed -e 's/char array/char bootstrap/' > $@

clean:
	rm -f $(OBXS) $(HDRS)
