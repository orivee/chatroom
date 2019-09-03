objects =
CFLAGS = -Wall -pthread
LDFLAGS = -pthread
CC=gcc

all: server client
.PHONEY: all

server: server.o msgprot.o
client: client.o msgprot.o

server.o: server.c
client.o: client.c
msgprot.o: msgprot.c

.PHONEY: clean
clean:
	-rm -f *.o server client
