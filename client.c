#include "commons.h"

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 7000
#define BUFFER_SZ 2048

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


void setup_client_connect(int * pconnfd)
{
    struct sockaddr_in serv_addr;

    *pconnfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == *pconnfd)
    {
        perror("socket creating failed");
        exit(EXIT_FAILURE);
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (0 == inet_pton(AF_INET, SERVER_ADDR, &serv_addr.sin_addr))
    {
        fprintf(stderr, "server address converting failed");
        close(*pconnfd);
        exit(EXIT_FAILURE); 
    }

    if (-1 == connect(*pconnfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)))
    {
        perror("server connecting failed");
        close(*pconnfd);
        exit(EXIT_FAILURE);
    }
}

void rdev_add(int epfd, int fd)
{
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;

    if (-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev))
    {
        perror("epoll adding failed");
        exit(EXIT_FAILURE);
    }
}


void send_message_server(msgprot_t * pmsgprot, int connfd)
{
    if (write(connfd, pmsgprot, sizeof(msgprot_t) + pmsgprot->length) < 0)
    {
        perror("writing to descriptor failed");
        close(connfd);
        exit(EXIT_FAILURE);
    }
}

char * accept_message(int connfd)
{
    msgprot_t msgprot;
    char * pmsg;

    if (read(connfd, &msgprot, sizeof(msgprot_t)) <= 0)
    {
        perror("reading from server failed");
        close(connfd);
        exit(EXIT_FAILURE);
    }
    message_unpack(connfd, &pmsg, msgprot.length);

    return pmsg;
}

int main(int argc, char *argv[])
{
    int connfd, epfd;
    struct epoll_event events[2];
    int ev_avail = 0; /* event ready */
    char buffer_out[BUFFER_SZ];
    char * buffer_in = NULL;
    msgprot_t * pmsgprot = NULL;

    setup_client_connect(&connfd);

    epfd = epoll_create(20);
    if (-1 == epfd)
    {
        perror("epoll creating failed");
        close(connfd);
        exit(EXIT_FAILURE); 
    }

    rdev_add(epfd, connfd);
    rdev_add(epfd, STDIN_FILENO); /* 让 epoll 监控 STDIN 的读 */

    while (1)
    {
        ev_avail = epoll_wait(epfd, events, 2, -1);

        /* 超时向客户端发送消息，证明自己活着 */
        if (0 == ev_avail)
        {
            continue;
        }

        for (int i = 0; i < ev_avail; i++)
        {
            if (events[i].data.fd == STDIN_FILENO) /* 终端输入发送到服务端 */
            {
                s_gets(buffer_out, BUFFER_SZ);
                if (0 == strlen(buffer_out)) /* 不处理直接回车的 buffer */
                    break;
                printf("[DEBUG] buffer_out: %s\n", buffer_out);
                pmsgprot = message_pack(buffer_out);
                if (!strcmp(buffer_out, "/quit"))
                {
                    printf("[DEBUG] close ...\n");
                    close(connfd);
                    exit(EXIT_FAILURE);
                }
                send_message_server(pmsgprot, connfd);
            }
            else /* 从服务端读入数据 */
            {
                buffer_in = accept_message(connfd);
                printf("%s", buffer_in);
            }
        }
    }
    printf("[DEBUG] close ...\n");
    close(connfd);
    return 0;
}
