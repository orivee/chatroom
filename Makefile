objects =
CFLAGS = -Wall -pthread
CC=gcc

all: server client
.PHONEY: all

# server: server.o

server: server.c

# client: client.o

client: client.c

.PHONEY: clean
clean:
	-rm -f *.o server client
