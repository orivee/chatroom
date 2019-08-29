#include "commons.h"

#define BACKLOG 5
#define EVMAX 100

/* description: */
/* precondition: */
/* postcondition: */
void server_tcp_init(int * pfd)
{
    int retval;
    struct sockaddr_in saddr;

    *pfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == *pfd)
    {
        /* TODO: log */
        perror("socket()");
        exit(-1);
    }

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(30000);
    if (inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr) <= 0)
    {
        /* TODO: log */
        fprintf(stderr, "server fails to convert IPv4 address!\n");
        exit(-1);
    }
    retval = bind(*pfd, (struct sockaddr *) &saddr, sizeof(saddr));
    if (-1 == retval)
    {
        /* TODO: log */
        perror("bind()");
        exit(-1);
    }

    retval = listen(*pfd, BACKLOG);
    if (-1 == retval)
    {
        /* TODO: log */
        perror("listen()");
        exit(-1);
    }

}

void server_epoll_init(int * pfd, int * pepfd)
{
    *pepfd = epoll_create(20);
    if (-1 == *pepfd)
    {
        /* TODO: log */
        perror("epoll_create()");
        close(*pfd);
        exit(-1);
    }
}

int server_accept_client(int lfd)
{
    int cfd;

    /* TODO 暂时不需要连接端地址 */
    cfd = accept(lfd, NULL, NULL);
    printf("DEBUG cfd: %d\n", cfd);

    return cfd;
}

/* description: */
/* precondition: */
/* postcondition: */
/* 如果是 lfd，需要退出程序
 * 如果是 nfd，关闭 nfd，并推出客户端 */
int server_add_fd_to_epoll(int epfd, int fd)
{
    int retval;
    struct epoll_event ev;

    ev.events = EPOLLIN;
    ev.data.fd = fd;
    retval = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    if (-1 == retval)
        return -1;

    /* TODO: nfd list */

    return 0;
}

int server_del_fd_from_epoll(int epfd, int fd)
{
    int retval;

    retval = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    if (-1 == retval)
        return -1;

    /* TODO: nfd list */

    return 0;
}

/* description: */
/* precondition: */
/* postcondition: */
/* 0：读段断开 */
/* -1：read 失败 */
int server_read_from_client(int cfd)
{
    int retval;
    protocol_t * pprot;

    pprot = malloc(sizeof(protocol_t) + sizeof(message_t));
    memset(pprot, 0x00, sizeof(protocol_t) + sizeof(message_t));

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
        free(pmsg);
    }

    return 1;
}

void write_to_client(int fd, protocol_t * pprot)
{
    int retval;

    printf("DEBUG write() ...\n");
    retval = write(fd, pprot, sizeof(protocol_t) + sizeof(message_t));
    perror("write()");
    if (-1 == retval)
    {
        /* TODO: log */
        perror("write()");
    }
    free(pprot);
}

int
main(int argc, char * argv[])
{
    int lfd, epfd, cfd;
    struct epoll_event events[EVMAX];
    protocol_t * pprot = NULL;
    message_t * pmsg;
    int nfound = 0;

    int retval;

    server_tcp_init(&lfd);
    server_epoll_init(&lfd, &epfd);
    retval = server_add_fd_to_epoll(epfd, lfd);
    if (-1 == retval)
    {
        /* TODO: log */
        fprintf(stderr, "lfd fails to add to epoll.\n");
        close(lfd);
        close(epfd);
        exit(-1);
    }

    while (1)
    {
        printf("DEBUG epoll wait ...\n");
        nfound = epoll_wait(epfd, events, EVMAX, -1);

        printf("DEBUG nfound = %d\n", nfound);

        for (int i = 0; i < nfound; i++)
        {
            printf("DEBUG fd ready: %d\n", events[i].data.fd);
            /* listen fd */
            if (events[i].data.fd == lfd)
            {
                cfd = server_accept_client(lfd);
                if (-1 == cfd)
                {
                    /* TODO: log */
                    fprintf(stderr, "client fails to connect.!\n");
                    continue;
                }

                server_add_fd_to_epoll(epfd, cfd);
            }
            else /* connected fd */
            {
                retval = server_read_from_client(events[i].data.fd);
                if (0 == retval)
                {
                    server_del_fd_from_epoll(epfd, events[i].data.fd);
                }
            }
        }

    }

    return 0;
}
