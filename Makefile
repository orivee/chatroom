objects =
CFLAGS = -Wall -pthread -I/usr/include/mysql -I/usr/include/mysql/mysql
LDFLAGS = -pthread
LDLIBS = -L/usr/lib/ -lmariadb
CC=gcc

all: server client
.PHONEY: all

server: server.o msgprot.o serv_config.o log.o
client: client.o msgprot.o log.o

server.o: server.c
client.o: client.c

serv_config.o:  serv_config.c

msgprot.o: msgprot.c
log.o: log.c

.PHONEY: clean
clean:
	-rm -f *.o server client
