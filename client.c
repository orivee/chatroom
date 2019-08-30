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
    printf("DEBUG mesage: %s\n", message.message);

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

int read_from_client(int cfd)
{
    int retval;
    protocol_t * pprot;

    pprot = malloc(sizeof(protocol_t) + sizeof(message_t));
    memset(pprot, 0x00, sizeof(protocol_t) + sizeof(message_t));

    printf("DEBUG read()\n");
    retval = read(cfd, pprot, sizeof(protocol_t));
    if (retval <= 0)
    {
        if (0 == retval)
        {
            perror("read()");
            /* 写端断开，应该从 epoll 中移除此 fd */

            return 0;
        }
        else
        {
            perror("read()");
            return -1;
        }
    }
    else
    {
        printf("message size: %d\n", pprot->size);
    }

    message_t * pmsg = (message_t *) malloc(sizeof(message_t));

    retval = read(cfd, pmsg, sizeof(message_t));
    if (retval <= 0)
    {
        if (0 == retval)
        {
            perror("read()");
            free(pmsg);
            return 0;
        }
        else
        {
            perror("read()");
            free(pmsg);
            return -1;
        }
    }
    else
    {
        printf("target: %d, message: %s\n", pmsg->fd, pmsg->message);
        /* free(pmsg); */
    }

    return 1;
}

int 
main(int argc, char *argv[])
{
    int cfd, epfd;
    struct epoll_event ev, events[20];
    int nfound;
    protocol_t * pprotocol;

    client_sock_init(&cfd);
    epfd = epoll_create(20);
    perror("epoll_create()");

    ev.events = EPOLLIN;
    ev.data.fd = cfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
    perror("epoll_ctl()");

    /* 把 stdin 加入到 epoll */
    ev.events = EPOLLIN;
    ev.data.fd = 0;
    epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &ev);

    printf("DEBUG read()\n");

    while (1)
    {
        printf("DEBUG epoll_wait() ...\n");
        nfound = epoll_wait(epfd, events, 20, -1);
        perror("epoll_wait()");

        for (int i = 0; i < nfound; i++)
        {
            // Check inputs of client
            if (0 == events[i].data.fd)
            {
                pprotocol = provide_message();
                write_to_server(cfd, pprotocol);
            }
            else // Check received from server
            {
                read_from_client(events[i].data.fd);
            }
        }
    }

    /* while (1) */
    /* { */
    /*     pprotocol = provide_message(); */
    /*     write_to_server(cfd, pprotocol); */
    /*     read_from_server(cfd, pprotocol); */
    /* } */

    return 0;
}
