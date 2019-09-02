#include "commons.h"
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#define BUFFER_SZ 2048
#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 7000
#define BACKLOG 5
#define EVMAX 100

static int uid = 0;

/* client structure */
typedef struct {
    struct sockaddr_in addr; /* Client remote address */
    int connfd;              /* Connection file descriptor */
    int uid;                 /* Client unique identifier */
    char name[32];           /* Client name */
} client_t;

/* online users list */
typedef struct online {
    client_t client;
    struct online * next;
} online_t;

typedef online_t * ol_uids_t;
ol_uids_t ol_uids = NULL; /* online list head */

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void setup_server_listen(int * plistenfd)
{
    int status;
    struct sockaddr_in serv_addr;

    /* socket settings */
    *plistenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == *plistenfd)
    {
        perror("socket creating failed");
        exit(EXIT_FAILURE);
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (0 == inet_pton(AF_INET, SERVER_ADDR, &serv_addr.sin_addr))
    {
        fprintf(stderr, "converting %s to IPv4 address failed!\n", SERVER_ADDR);
        close(*plistenfd);
        exit(EXIT_FAILURE);
    }

    /* ignore SIGPIPE signal */
    signal(SIGPIPE, SIG_IGN);

    /* bind */
    status = bind(*plistenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (-1 == status)
    {
        perror("socket binding failed");
        close(*plistenfd);
        exit(EXIT_FAILURE);
    }

    /* listen */
    status = listen(*plistenfd, BACKLOG);
    if (-1 == status)
    {
        perror("socket listening failed");
        close(*plistenfd);
        exit(EXIT_FAILURE);
    }
}

/* accept a client */
client_t * accept_client(int * plistenfd, int * pconnfd)
{
    struct sockaddr_in cli_addr;

    socklen_t clilen = sizeof(cli_addr);
    *pconnfd = accept(*plistenfd, (struct sockaddr *) &cli_addr, &clilen);
    if (-1 == *pconnfd)
    {
        perror("client accepting failed");
        close(*plistenfd);
        exit(EXIT_FAILURE);
    }

    client_t * cli = (client_t *) malloc(sizeof(client_t));
    cli->addr = cli_addr;
    cli->connfd = *pconnfd;
    cli->uid = uid++;
    strcpy(cli->name, "anonymous");

    return cli;
}

/* add new client to  online uids */
void online_add(client_t * pcli)
{
    pthread_mutex_lock(&clients_mutex);
    online_t * ponline = (online_t *) malloc(sizeof(online_t));
    ponline->client = *pcli;

    ponline->next = ol_uids;
    ol_uids = ponline;

    printf("[DEBUG]: add %p | %p\n", ol_uids, ol_uids->next);
    pthread_mutex_unlock(&clients_mutex);
}

/* delete offline client from online uids */
void online_delete(int uid)
{
    pthread_mutex_lock(&clients_mutex);
    ol_uids_t pnode = ol_uids;
    if (pnode->client.uid == uid)
    {
        ol_uids = pnode->next;
        free(pnode);
        pthread_mutex_unlock(&clients_mutex);
        return;
    }

    pnode = pnode->next;
    ol_uids_t pprev = ol_uids;
    while (pnode != NULL)
    {
        printf("[DEBUG]: delete %p | %p\n", pprev, pprev->next);
        if (pnode->client.uid == uid)
        {
            pprev->next = pnode->next;
            free(pnode);
            break;
        }
        pprev = pnode;
        pnode = pnode->next;
    }
    pthread_mutex_unlock(&clients_mutex);
}

/* Print ip address */

void print_client_addr(struct sockaddr_in addr)
{
    char addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, (void *) &addr.sin_addr, addr_str, sizeof(addr_str));
    printf("%s:%d", addr_str, ntohs(addr.sin_port));
}

char * read_client(int connfd, int len)
{
    char * buffer_in = malloc(1024);
    read(connfd, buffer_in, 1024);

    return buffer_in;
}

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

void send_message_self(const char * msg, int connfd)
{
    msgprot_t * pmsgprot = message_pack(msg);
    if (write(connfd, pmsgprot, sizeof(msgprot_t) + pmsgprot->length) < 0)
    {
        perror("writing to descriptor failed");
        close(connfd);
        /* 如果当前进程中 mutex 变量被加锁了， 就在退出前解锁，防止 deadlock*/
        if (EBUSY == pthread_mutex_trylock(&clients_mutex))
            pthread_mutex_unlock(&clients_mutex);
        exit(EXIT_FAILURE); 
    }
}

int send_message_client(const char * msg, int uid)
{
    msgprot_t * pmsgprot = message_pack(msg);
    pthread_mutex_lock(&clients_mutex);
    online_t * pscan = ol_uids ;
    while (pscan != NULL)
    {
        if (pscan->client.uid == uid)
        {
            if (write(pscan->client.connfd, pmsgprot, sizeof(msgprot_t) + pmsgprot->length) < 0)
            {
                perror("fowarding message to client failed");
                close(pscan->client.connfd);
                pthread_mutex_unlock(&clients_mutex);
                return -1; /* 消息发送不成功 */
            }
            pthread_mutex_unlock(&clients_mutex);
            return 0; /* 消息发送成功 */
        }
        pscan = pscan->next;
    }
    pthread_mutex_unlock(&clients_mutex);
    return 1; /* 消息对象不存在 */
}

void send_active_clients(int connfd)
{
    pthread_mutex_lock(&clients_mutex);
    online_t * pnode = ol_uids;
    char buffer_out[BUFFER_SZ];

    while (pnode != NULL)
    {
        sprintf(buffer_out, "%s (%d)\n", pnode->client.name, pnode->client.uid);
        send_message_self(buffer_out, connfd);
        pnode = pnode->next;
    }
    pthread_mutex_unlock(&clients_mutex);
}

void * handle_client(void * arg)
{
    char buffer_out[BUFFER_SZ];
    char * buffer_in;
    msgprot_t msgprot;
    int msglen;

    client_t * pcli = (client_t *) arg;

    printf("<< accept ");
    print_client_addr(pcli->addr);
    printf(" logged in with annoymous and referenced by %d\n", pcli->uid);

    /* pthread_mutex_lock(&clients_mutex); */
    /* sprintf(buffer_out, "<< accept [%s (%d)] login\n", pcli->name, pcli->uid); */
    /* write(pcli->connfd, buffer_out, sizeof(buffer_out)); */
    /* perror("write to client"); */
    /* pthread_mutex_unlock(&clients_mutex); */

    while (read(pcli->connfd, &msgprot, sizeof(msgprot_t)) > 0)
    {
        msglen = message_unpack(pcli->connfd, &buffer_in, msgprot.length);
        if (msglen <= 0)
            break;
        /* 读取的数据长度不一致，舍弃 */
        if (msglen != msgprot.length)
            continue;

        if (buffer_in[0] == '/')
        {
            char * command, * param;
            command = strtok(buffer_in, " ");
            printf("command: %s\n", command);
            if (!strcmp(command, "/quit"))
            {
                break;
            }
            else if (!strcmp(command, "/msg"))
            {
                param = strtok(NULL, " ");
                if (param)
                {
                    int uid = atoi(param); /* TODO: 添加输入检查 */
                    param = strtok(NULL, " ");
                    if (param)
                    {
                        sprintf(buffer_out, "[PM] [%s (%d)] ", pcli->name, pcli->uid);
                        while (param != NULL)
                        {
                            strcat(buffer_out, param);
                            param = strtok(NULL, " ");
                        }
                        strcat(buffer_out, "\n");
                        if (1 == send_message_client(buffer_out, uid))
                        {
                            printf("[DEBUG]");
                            sprintf(buffer_out, "%d reference is not online\n", uid);
                            send_message_self(buffer_out, pcli->connfd);
                        }
                    }
                    else
                    {
                        send_message_self("<< message cannot be null\n", pcli->connfd);
                    }
                }
                else
                {
                    send_message_self("<< reference cannot be null\n", pcli->connfd);
                }
            }
            else if (!strcmp(command, "/list"))
            {
                send_active_clients(pcli->connfd);
            }
            else if (!strcmp(command, "/help"))
            {
                strcat(buffer_out, "<< /quit      Quit chatroom\n");
                strcat(buffer_out, "<< /msg       <uid> <message> Send message to <uid>\n");
                strcat(buffer_out, "<< /list      Show online clients\n");
                strcat(buffer_out, "<< /login     <uid> <password> Login chatroom with <uid>\n");
                strcat(buffer_out, "<< /register  <password> Register in chatroom\n");
                strcat(buffer_out, "<< /nick      <name> Change nickname\n");
                send_message_self(buffer_out, pcli->connfd);
            }
            else
            {
                send_message_self("<< unkown command\n", pcli->connfd);
            }
        }
        else
        {
            printf("[DEBUG] << accept %s\n", buffer_in);
        }
    }

    fprintf(stdout, "<< %s (%d) has left\r\n", pcli->name, pcli->uid);
    online_delete(pcli->uid);
    close(pcli->connfd);

    return (void *) EXIT_SUCCESS;
}

int main(int argc, char * argv[])
{
    int listenfd = 0, connfd = 0;
    pthread_t tid;

    /* read server config */
    /* TODO: func */

    /* read user database */
    /* TODO: func */
    uid = 10;

    setup_server_listen(&listenfd);

    printf("<[ SERVER STARTED ]>\n");

    while (1)
    {
        client_t * cli = accept_client(&listenfd, &connfd);

        /* add client to online list and fork thread */
        online_add(cli);
        pthread_create(&tid, NULL, handle_client, (void *) cli);

        sleep(1);
    }

    return 0;
}
