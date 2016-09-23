
CC = gcc
CFLAGS = -c -Wall -O2

OBJS = pkt_comm.o word_gen.o word_list.o

default: $(OBJS)
all: $(OBJS)


pkt_comm.o: pkt_comm.c pkt_comm.h
	$(CC) $(CFLAGS) pkt_comm.c
	
word_gen.o: word_gen.c word_gen.h pkt_comm.h
	$(CC) $(CFLAGS) word_gen.c 

word_list.o: word_list.c word_list.h pkt_comm.h
	$(CC) $(CFLAGS) word_list.c 


clean:
	rm -f $(OBJS)
	