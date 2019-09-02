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

#if 0
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
#endif


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

msgprot_t * message_pack(char * msg)
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


void send_message_server(msgprot_t * pmsgprot, int connfd)
{
    if (write(connfd, pmsgprot, sizeof(msgprot_t) + pmsgprot->length) < 0)
    {
        perror("writing to descriptor failed");
        close(connfd);
        exit(EXIT_FAILURE);
    }
}

void send_message_private()
{

}

char * accept_message(int connfd)
{
    msgprot_t msgprot;
    char * pmsg;

    read(connfd, &msgprot, sizeof(msgprot_t));
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
