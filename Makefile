objects =
CFLAGS = -Wall -pthread -I/usr/include/mysql -I/usr/include/mysql/mysql
LDFLAGS = -pthread
LDLIBS = -L/usr/lib/ -lmariadb
CC=gcc

all: server client
.PHONEY: all

server: server.o msgprot.o log.o
client: client.o msgprot.o log.o

server.o: server.c
client.o: client.c
msgprot.o: msgprot.c
log.o: log.c

.PHONEY: clean
clean:
	-rm -f *.o server client
