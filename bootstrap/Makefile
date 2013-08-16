OBXS = player.obx
HDRS = player.h

all: $(HDRS)

%.obx: %.asx
	xasm /o:$@ $^

%.h: %.obx
	./bin2h $^ | sed -e 's/char array/char bootstrap/' > $@

clean:
	rm -f $(OBXS) $(HDRS)
