
CC = gcc
CFLAGS = -c -Wall -O2
CFLAGS_TEST = -O
LD = ld
EXTRA_LIBS = -lusb-1.0

SUBDIRS = pkt_comm

OBJS = device.o inouttraffic.o ztex.o ztex_scan.o

TESTS = simple_test test pkt_test

EXTRA_OBJS = pkt_comm/*.o

default: $(SUBDIRS) $(OBJS) $(TESTS)
all: $(SUBDIRS) $(OBJS) $(TESTS)

.PHONY: subdirs $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ all
	
device.o: device.c device.h
	$(CC) $(CFLAGS) device.c

inouttraffic.o: inouttraffic.c inouttraffic.h
	$(CC) $(CFLAGS) inouttraffic.c
	
ztex.o: ztex.c ztex.h
	$(CC) $(CFLAGS) ztex.c

ztex_scan.o: ztex_scan.c ztex_scan.h
	$(CC) $(CFLAGS) ztex_scan.c


simple_test: simple_test.c $(SUBDIRS) $(OBJS)
	$(CC) $(CFLAGS_TEST) simple_test.c $(OBJS) $(EXTRA_OBJS) $(EXTRA_LIBS) -o simple_test
	
test: test.c $(SUBDIRS) $(OBJS)
	$(CC) $(CFLAGS_TEST) test.c $(OBJS) $(EXTRA_OBJS) $(EXTRA_LIBS) -o test

pkt_test: pkt_test.c $(SUBDIRS) $(OBJS)
	$(CC) $(CFLAGS_TEST) pkt_test.c $(OBJS) $(EXTRA_OBJS) $(EXTRA_LIBS) -o pkt_test


clean:
	find . -name \*.o -exec rm -f "{}" \;
	rm -f *.exe simple_test test pkt_test
