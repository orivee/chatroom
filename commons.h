#ifndef COMMONS_H_
#define COMMONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

typedef struct msgprot
{
    uint32_t length; /* 发送消息的长度 */
    char * msgp[0];
} msgprot_t;

#endif
