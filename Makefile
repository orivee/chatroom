objects =
CFLAGS = -Wall -pthread -I/usr/include/mysql -I/usr/include/mysql/mysql
LDFLAGS = -pthread
LDLIBS = -L/usr/lib/ -lmariadb
CC=gcc

all: server client
.PHONEY: all

server: server.o msgprot.o serv_config.o handle_client.o user_manage.o log.o
client: client.o msgprot.o log.o

server.o: server.c
client.o: client.c

serv_config.o:  serv_config.c
handle_client.o: handle_client.c
user_manage.o:	user_manage.c

msgprot.o: msgprot.c
log.o: log.c

.PHONEY: clean
clean:
	-rm -f *.o server client
