#ifndef MSGPROT_H_
#define MSGPROT_H_

#include <stdint.h>

typedef struct msgprot
{
    uint32_t length; /* 发送消息的长度 */
    char * msgp[0];
} msgprot_t;

extern msgprot_t * message_pack(const char * msg);
extern int message_unpack(int connfd, char ** pmsg, size_t size);

#endif
