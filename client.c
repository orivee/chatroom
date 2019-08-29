#include "commons.h"

void eatinput()
{
    while(getchar() != '\n')
        continue;
}

char * s_gets(char * st, int n)
{
    char * retval;
    int i = 0;

    retval = fgets(st, n, stdin);
    if (retval)
    {
        while (st[i] != '\n' && st[i] != '\0')
            i++;
        if (st[i] == '\n')
            st[i] = '\0';
        else
            while (getchar() != '\n')
                continue;
    }
    return retval;
}

/* description: initiliaze client socket */
/* precondition: poi*pnt to a socket fd */
/* condition: client socket fd connects peer socket */
void client_sock_init(int * pcfd)
{
    int retval;
    struct sockaddr_in saddr;

    *pcfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == *pcfd)
    {
        /* TODO: log */
        perror("socket()");
        exit(-1);
    }

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(30000);
    if (inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr) <= 0)
    {
        /* TODO log */
        fprintf(stderr, "fail to convert IPv4 address!\n");
        exit(-1);
    }

    retval = connect(*pcfd, (struct sockaddr *) &saddr, sizeof(saddr));
    if (-1 == retval)
    {
        /* TODO log */
        perror("connect()");
        exit(-1);
    }

}

/* description: 准备要发送的消息 */
/* postcondtion: */
protocol_t * provide_message()
{
    message_t message;
    protocol_t * pprot = NULL;

    pprot = malloc(sizeof(protocol_t) + sizeof(message_t));
    memset(pprot, 0, sizeof(protocol_t) + sizeof(message_t));
    pprot->size = sizeof(message_t);

    puts("enter a client fd");
    scanf("%d", &message.fd);
    eatinput();
    puts("enter your messgae");
    s_gets(message.message, MSGMAX);
    /* printf("DEBUG mesage: %s\n", message.message); */

    memcpy(pprot->datap, &message, sizeof(message_t));

    return pprot;
}

/* description: 发送准备好的消息 */
void write_to_server(int fd, protocol_t * pprot)
{
    int retval;

    printf("DEBUG write() ...\n");
    retval = write(fd, pprot, sizeof(protocol_t) + sizeof(message_t));
    if (-1 == retval)
    {
        /* TODO: log */
        perror("write()");
    }
    free(pprot);
}

int 
main(int argc, char *argv[])
{
    int cfd;
    protocol_t * pprotocol;

    client_sock_init(&cfd);

    while (1)
    {
        pprotocol = provide_message();
        write_to_server(cfd, pprotocol);
    }

    return 0;
}
