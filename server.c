#include "commons.h"
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#define BUFFER_SZ 2048
#define STRMAX 31
#define BACKLOG 5
#define EVMAX 100

static int uid = 0;

/* config structure */
typedef struct {
    char address[STRMAX];
    int port;
    char logfile[STRMAX];
} config_t;

static config_t configs;

/* user info structure */
typedef struct {
    int uid;
    char name[STRMAX];
    char passwd[STRMAX];
} user_info_t;

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

/* string duplication */
char * s_strdup(const char * s)
{
    size_t size = strlen(s);
    char * p = malloc(size);
    if (p)
    {
        memcpy(p, s, size);
    }
    return p;
}

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
        strcpy(configs.address, value);
    else if (0 == strcmp(varname, "server_port"))
        configs.port = atoi(value);
    else if (0 == strcmp(varname, "log_file"))
        strcpy(configs.logfile, value);
    else
        return -1;

    return 0;
}

/* read server config */
void read_server_config()
{
    FILE * fp = fopen("./server.conf", "r");
    if (NULL == fp)
    {
        perror("opening config failed");
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
            fprintf(stderr, "configuration syntax error\n");
            exit(EXIT_FAILURE);
        }
    }

    printf("config: addr: %s, port: %d, logfile: %s\n",
            configs.address, configs.port, configs.logfile);
}

/* uid initialization */
void uid_init()
{
    FILE * fp = fopen("./users.db", "r");
    user_info_t user_info;

    while (fread(&user_info, sizeof(user_info_t), 1, fp)) /* stop after an EOF or a newline */
    {
        /* printf("saved user: %d %s %s\n", user_info.uid, user_info.passwd, user_info.name); */
        if (user_info.uid > uid)
            uid = user_info.uid;
    }

    uid = uid + 1;
}

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
    serv_addr.sin_port = htons(configs.port);
    if (0 == inet_pton(AF_INET, configs.address, &serv_addr.sin_addr))
    {
        fprintf(stderr, "converting %s to IPv4 address failed!\n", configs.address);
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
    /* printf("[DEBUG]: add %p\n", pcli); */
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

void online_modify(int uid, int newuid, const char * newname)
{
    pthread_mutex_lock(&clients_mutex);
    online_t * online = ol_uids;
    while (online != NULL)
    {
        printf("[DEBUG] %p | %p\n", online, online->next);
        if (online->client.uid == uid)
        {
            if (newuid != 0)
            {
                online->client.uid = newuid;
            }
            if (newname != NULL)
            {
                strcpy(online->client.name, newname);
            }
            fprintf(stdout, "<< uid %d with name %s modify successfuly\n",
                    online->client.uid, online->client.name);
            break;
        }
        online = online->next;
    }
    pthread_mutex_unlock(&clients_mutex);
}

/* print client info */

void print_client_addr(struct sockaddr_in addr)
{
    char addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, (void *) &addr.sin_addr, addr_str, sizeof(addr_str));
    printf("%s:%d", addr_str, ntohs(addr.sin_port));
}

/* send message to sender */
void send_message_self(const char * msg, int connfd)
{
    msgprot_t * pmsgprot = message_pack(msg);
    if (write(connfd, pmsgprot, sizeof(msgprot_t) + pmsgprot->length) < 0)
    {
        perror("writing to descriptor failed");
        close(connfd);
        /* 如果当前进程中 mutex 变量被加锁了， 就在退出前解锁，防止 deadlock*/
        /* if (EBUSY == pthread_mutex_trylock(&clients_mutex))*/
        /*     pthread_mutex_unlock(&clients_mutex);*/
        exit(EXIT_FAILURE); 
    }
}

/* send message to client */
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
                /* close(pscan->client.connfd); */
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

/* Send message to all clients but the sender */
void send_message(char * msg, int uid)
{
    pthread_mutex_lock(&clients_mutex);
    online_t * pnode = ol_uids;
    msgprot_t * pmsgprot = message_pack(msg);

    while (pnode != NULL)
    {
        if (pnode->client.uid != uid)
        {
            if (write(pnode->client.connfd, pmsgprot, sizeof(msgprot_t) + pmsgprot->length) < 0)
            {
                perror("writing to descriptor failed");
                break;
            }
        }
        pnode = pnode->next;
    }
    pthread_mutex_unlock(&clients_mutex);
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

/* verify uid and password */
/* 0: login successfully 1: uid and password not match; */
/* -1: uid not found */
int verify_uid_pwd(const int uid, const char * pwd, char * name)
{
    user_info_t user_info;

    FILE * fp = fopen("./users.db", "rb");
    while (fread(&user_info, sizeof(user_info_t), 1, fp)) /* stop after an EOF or a newline */
    {
        /* printf("saved user: %d %s %s\n", user_info.uid, user_info.passwd, user_info.name); */
        if (user_info.uid== uid)
        {
            if (!strcmp(user_info.passwd, pwd))
            {
                strcpy(name, user_info.name);
                return 0; /* login successfully */
            }
            else
            {
                return 1; /* uid and pwd not match */
            }
        }
    }

    return -1; /* uid not found*/
}

void save_uid_pwd(const int uid, const char * pwd, const char * name)
{
    user_info_t user_info;
    user_info.uid = uid;
    strcpy(user_info.passwd, pwd);
    if (name)
    {
        strcpy(user_info.name, name);
    }
    else
    {
        strcpy(user_info.name, "annoymous");
    }

    /* printf("saved user: %d %s %s\n", user_info.uid, user_info.passwd, user_info.name); */
    FILE * fp = fopen("./users.db", "a+b");
    fwrite(&user_info, sizeof(user_info_t), 1, fp);
    fclose(fp);
}

void modify_pwd_name(const int uid, const char * pwd, const char * name)
{
    user_info_t user_info;

    FILE * fp = fopen("./users.db", "r+b");
    while (fread(&user_info, sizeof(user_info_t), 1, fp)) /* stop after an EOF or a newline */
    {
        /* printf("saved user: %d %s %s\n", user_info.uid, user_info.passwd, user_info.name); */
        if (user_info.uid == uid)
            break;
    }
    fseek(fp, -sizeof(user_info_t), SEEK_CUR);
    if (pwd)
    {
        strcpy(user_info.passwd, pwd);
    }
    if (name)
    {
        strcpy(user_info.name, name);
    }
    fwrite(&user_info, sizeof(user_info_t), 1, fp);
    fseek(fp, 0, SEEK_CUR);
    fclose(fp);
}

void * handle_client(void * arg)
{
    char buffer_out[BUFFER_SZ];
    char * buffer_in;
    msgprot_t msgprot;
    int msglen;
    int is_login = 0; /* 登入标志 0 未登入；1 登入 */

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

        if (0 == strcmp(buffer_in, "/alive"))
        {
            /* printf("[DEBUG] %d, alive\n", pcli->connfd); */
            continue;
        }

        if (buffer_in[0] == '/')
        {
            char * command, * param1, * param2;
            command = strtok(buffer_in, " ");
            printf("command: %s\n", command);
            if (!strcmp(command, "/quit"))
            {
                break;
            }
            else if (!strcmp(command, "/register"))
            {
                param1 = strtok(NULL, " ");
                param2 = strtok(NULL, " ");
                if (param1 == NULL)
                {
                    send_message_self("<< password cannot be null\n", pcli->connfd);
                }

                save_uid_pwd(pcli->uid, param1, param2); /* TODO: 错误检查 */
                sprintf(buffer_out, "<< resgister successfully with %d\n", pcli->uid);
                send_message_self(buffer_out, pcli->connfd);
            }
            else if (!strcmp(command, "/login"))
            {
                if (1 == is_login)
                {
                    send_message_self("<< already login\n", pcli->connfd);
                    continue;
                }
                param1 = strtok(NULL, " "); /* uid */
                param2 = strtok(NULL, " "); /* pwd */
                if (param1 == NULL || param2 == NULL)
                {
                    send_message_self("<< uid or password cannot be null\n", pcli->connfd);
                    continue;
                }

                int uid = atoi(param1); /* TODO: 输入检查 */

                int status = verify_uid_pwd(uid, param2, pcli->name);
                if (0 == status)
                {
                    online_modify(pcli->uid, uid, pcli->name); /* 需要修改在线列表中的 uid 和 name，列表 client_t 是复制了一份 */
                    pcli->uid = uid; /* 修改完列表，再修改当前 client_t 信息 */
                    is_login = 1; /* 登入成功 */
                    sprintf(buffer_out, "<< login successfully with %s (%d)\n", pcli->name, pcli->uid);
                    send_message_self(buffer_out, pcli->connfd);
                }
                else if (1 == status)
                {
                    send_message_self("<< uid and password do not match\n", pcli->connfd);
                }
                else
                {
                    send_message_self("<< uid not found\n", pcli->connfd);
                }

            }
            else if (!strcmp(command, "/msg"))
            {
                if (!is_login)
                {
                    send_message_self("<< login first or register\n", pcli->connfd);
                    continue;
                }
                param1 = strtok(NULL, " ");
                param2 = strtok(NULL, " ");
                if (param1 == NULL || param2 == NULL)
                {
                    send_message_self("<< uid or message cannot be null\n", pcli->connfd);
                    continue;
                }

                int uid = atoi(param1); /* 检查 */
                sprintf(buffer_out, "[PM] [%s (%d)] ", pcli->name, pcli->uid);
                while (param2 != NULL)
                {
                    strcat(buffer_out, param2);
                    param2 = strtok(NULL, " ");
                }
                strcat(buffer_out, "\n");
                if (1 == send_message_client(buffer_out, uid))
                {
                    sprintf(buffer_out, "uid %d is not online\n", uid);
                    send_message_self(buffer_out, pcli->connfd);
                }
            }
            else if (!strcmp(command, "/list"))
            {
                if (!is_login)
                {
                    send_message_self("<< login first or register\n", pcli->connfd);
                    continue;
                }
                send_active_clients(pcli->connfd);
            }
            else if (!strcmp(command, "/nick"))
            {
                if (!is_login)
                {
                    send_message_self("<< login first or register\n", pcli->connfd);
                    continue;
                }
                param1 = strtok(NULL, " ");
                if (param1 == NULL)
                {
                    send_message_self("<< name cannot be null\n", pcli->connfd);
                }
                char * oldname = s_strdup(pcli->name);
                if (!oldname)
                {
                    perror("cannot allocate memory for name");
                    continue;
                }
                modify_pwd_name(pcli->uid, NULL, param1); /* TODO: 错误检查 */
                online_modify(pcli->uid, 0, param1);
                strcpy(pcli->name, param1);
                sprintf(buffer_out, "<< %s is now konwn as %s\n", oldname, pcli->name);
                free(oldname);
                send_message_self(buffer_out, pcli->connfd);
            }
            else if (!strcmp(command, "/pwd"))
            {
                if (!is_login)
                {
                    send_message_self("<< login first or register\n", pcli->connfd);
                    continue;
                }
                param1 = strtok(NULL, " ");
                if (param1 == NULL)
                {
                    send_message_self("<< name cannot be null\n", pcli->connfd);
                }
                modify_pwd_name(pcli->uid, param1, NULL); /* TODO: 错误检查 */
                send_message_self("<< reset password successfully\n", pcli->connfd);
            }
            else if (!strcmp(command, "/help"))
            {
                strcat(buffer_out, "<< /quit      Quit chatroom\n");
                strcat(buffer_out, "<< /msg       <uid> <message> Send message to <uid>\n");
                strcat(buffer_out, "<< /list      Show online clients\n");
                strcat(buffer_out, "<< /login     <uid> <password> Login chatroom with <uid>\n");
                strcat(buffer_out, "<< /register  <password> [name] Register with name\n");
                strcat(buffer_out, "<< /nick      <name> Change nickname\n");
                strcat(buffer_out, "<< /pwd       <password> Reset password\n");
                send_message_self(buffer_out, pcli->connfd);
            }
            else
            {
                send_message_self("<< unkown command\n", pcli->connfd);
            }
        }
        else
        {
            if (!is_login)
            {
                send_message_self("<< login first or register\n", pcli->connfd);
                continue;
            }
            snprintf(buffer_out, sizeof(buffer_out), "[%d] (%s) %s\n", pcli->uid, pcli->name, buffer_in);
            send_message(buffer_out, pcli->uid);
        }
    }

    fprintf(stdout, "<< %s (%d) has left\r\n", pcli->name, pcli->uid);
    online_delete(pcli->uid);
    close(pcli->connfd);
    free(pcli);

    return (void *) EXIT_SUCCESS;
}

int main(int argc, char * argv[])
{
    int listenfd = 0, connfd = 0;
    pthread_t tid;

    /* read server config */
    read_server_config();

    /* read user database */
    uid_init();
    printf("init uid: %d\n", uid);

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
