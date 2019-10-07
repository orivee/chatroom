#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "msgprot.h"

msgprot_t * message_pack(const char * msg)
{
    msgprot_t * pmsgprot = malloc(sizeof(msgprot_t) + strlen(msg));
    pmsgprot->length = strlen(msg);
    memcpy(pmsgprot->msgp, msg, strlen(msg));
    return pmsgprot;
}

/* 返回消息长度 */
int message_unpack(int connfd, char ** pmsg, size_t size)
{
        *pmsg = malloc(size + 1);
        int rlen = read(connfd, *pmsg, size);
        (*pmsg)[rlen] = '\0';
        if (rlen <= 0)
        {
            perror("message reading failed");
        }
        return rlen;
}

