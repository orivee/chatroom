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

#define STRMAX 31
#define MSGMAX 1024 

typedef struct online_user
{
    char username[STRMAX];
    int fd;
} online_t;

typedef struct send_message
{
    /* online_t target; */
    int fd;
    char message[MSGMAX];
} message_t;

typedef struct protocol
{
    uint32_t size; /* 发送的数据包大小 */
    // uint16_t type; /* TODO: 消息类型 */
    char * datap[0];

} protocol_t;

#endif
