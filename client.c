#include "commons.h"
#include "log.h"
#include <getopt.h>
#include <stdarg.h>
#include <errno.h>

#define BUFFER_SZ 2048

/* config structure */
typedef struct
{
    char config[51];        /* configuration file */
    char ip[51];            /* server socket bind ip address */
    int port;               /* server socket bind port */
} config_t;

config_t configs =
{
    "",
    "127.0.0.1",
    7777
};

/* parse a row config */
int parse_line(char * buf)
{
    if (buf == NULL)
        return -1;

    char * varname, * value, * cmnt;
    const char * sep = " ";

    varname = strtok(buf, sep);
    if (varname == NULL)
        return -1;
    if ('#' == varname[0])
        return 0;

    value = strtok(NULL, sep);
    if (value == NULL)
        return -1;
    int slen = strlen(value);
    if ('\n' == value[slen-1])
        value[slen-1] = '\0';

    cmnt = strtok(NULL, sep);
    if (cmnt != NULL && cmnt[0] != '#')
        return -1;
    else if (0 == strcmp(varname, "server_address"))
        strcpy(configs.ip, value);
    else if (0 == strcmp(varname, "server_port"))
        configs.port = atoi(value);
    else
        return -1;

    return 0;
}

/* read client config */
void read_client_config()
{
    FILE * fp = fopen("./client.conf", "r");
    if (NULL == fp)
    {
        log_error("opening config failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    char fconfig[BUFFER_SZ];

    while (fgets(fconfig, BUFFER_SZ, fp) != NULL)
    {
        /* printf("config: %ld %s", strlen(fconfig), fconfig); */
        if (1 == strlen(fconfig) && '\n' == fconfig[0])
            continue;

        if (-1 == parse_line(fconfig))
        {
            log_error("configuration syntax or value error");
            exit(EXIT_FAILURE);
        }
    }

    /* printf("config: addr: %s, port: %d\n", */
    /*         configs.ip, configs.port); */
}

void load_arguments(int argc, char ** argv)
{
    struct option long_options[] =
    {
        {"help", no_argument, 0, 'h'},
        {"config", required_argument, 0, 'f'},
        {"bind_ip", required_argument, 0, 'i'},
        {"port", required_argument, 0, 'p'},
    };

    int c;
    int option_index = 0;

    while (1)
    {
        c = getopt_long(argc, argv, "hf:i:p:", long_options, &option_index);

        if (-1 == c)
            break;

        switch (c)
        {
            case 'h':
                printf("Usage: %s [options]\n", argv[0]);
                printf("options:\n");
                printf("\t--help, -h\n\t\tshow help information\n");
                printf("\t--config <filename>, -f <filename>\n\t\tspecify configure file\n");
                printf("\t--server_ip <ipaddress>, -i <ipaddress>\n\t\tspecify server ip\n");
                printf("\t--port <port>, -p <port>\n\t\tspecify server port\n");
                exit(EXIT_FAILURE);
            case 'f':
                break;
            case 'i':
                if (optarg)
                    strcpy(configs.ip, optarg);
                break;
            case 'p':
                if (optarg)
                    configs.port = atoi(optarg);
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }
}

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
    serv_addr.sin_port = htons(configs.port);
    if (0 == inet_pton(AF_INET, configs.ip, &serv_addr.sin_addr))
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


void send_message_server(char * msg, int connfd)
{
    msgprot_t * pmsgprot = message_pack(msg);
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

    /* read client config */
    read_client_config();

    /* load command arguments */
    load_arguments(argc, argv);

    /* printf("config: addr: %s, port: %d\n", */
    /*         configs.ip, configs.port); */

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
        ev_avail = epoll_wait(epfd, events, 2, 3000);

        /* 超时向客户端发送消息，证明自己活着 */
        if (0 == ev_avail)
        {
            send_message_server("/alive", connfd);
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
                if (!strcmp(buffer_out, "/quit"))
                {
                    printf("[DEBUG] close ...\n");
                    close(connfd);
                    exit(EXIT_FAILURE);
                }
                send_message_server(buffer_out, connfd);
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
