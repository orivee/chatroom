#include "commons.h"
#include <getopt.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>

#define BUFFER_SZ 2048
#define STRMAX 31
#define BACKLOG 5
#define EVMAX 100

enum {LOG_ERROR, LOG_INFO, LOG_DEBUG};

static const char * level_names[] = {
    "ERROR", "INFO", "DEBUG"
};

#define log_error(...) log_log(LOG_ERROR, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define log_info(...) log_log(LOG_INFO, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

FILE * log_fp = NULL;
pthread_mutex_t logfile_mutex = PTHREAD_MUTEX_INITIALIZER;

/* config structure */
typedef struct
{
    char config[STRMAX];    /* configuration file */
    char ip[STRMAX];        /* server socket bind ip address */
    char logpath[STRMAX];   /* log file pathname */
    int quiet;             /* quiet mode */
    int port;               /* server socket bind port */
} config_t;

/* init config paramters */
static config_t configs =
{
    "server.conf",
    "127.0.0.1",
    "",
    0,
    7000
};

/* user info structure */
typedef struct {
    int uid;
    char name[STRMAX];
    char passwd[STRMAX];
} user_info_t;

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

char * timenow()
{
    static char buffer[64];
    time_t rawtime;
    struct tm * timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    buffer[strftime(buffer, 64, "%Y-%m-%d %H:%M:%S", timeinfo)] = '\0';

    return buffer;
}

void log_set_fp(const char * file)
{
    /* printf("file: %s %ld\n", file, strlen(file)); */
    if (NULL == file || 0 == strlen(file))
        return;

    log_fp = fopen(file, "a+");
    if (log_fp == NULL)
    {
        fprintf(stderr, "log file openning failed\n");
    }
}

void log_close_fp()
{
    if (EOF == fclose(log_fp))
    {
        fprintf(stderr, "log file closing failed\n");
    }
}

void log_log(int level, const char * file, const char * func, int line, const char * str, ...)
{
    /* construct args string */
    va_list argp;
    va_start(argp, str);
    int max_va_list_size = 4146; /* 不能明确大小，所以定个大一点的数 */
    char * va_msg = (char *) malloc(strlen(str) + max_va_list_size);
    int va_string_size = vsnprintf(va_msg, strlen(str) + max_va_list_size, str, argp);
    va_end(argp);

    /* get current */
    char * date = timenow();

    /* construct log string expect args string */
    char pos[strlen(date) + 512];
    int pos_size = snprintf(pos, 1024, "%s | %-5s | %-10s | %s:%d |", date, level_names[level], file, func, line);
    /* printf("%s\n", pos); */

    /* construct final log string */
    int msgsize = va_string_size + pos_size + 50;
    char * msg = (char *) malloc(msgsize);
    sprintf(msg, "%s %s\n", pos, va_msg);

    if (!configs.quiet)
    {
        if (0 == level)
            fprintf(stderr, msg);
        else
            fprintf(stdout, msg);
    }

    if (configs.logpath && log_fp)
    {
        pthread_mutex_lock(&logfile_mutex);
        fprintf(log_fp, "%s", msg);
        fflush(log_fp);
        pthread_mutex_unlock(&logfile_mutex);
    }

    free(va_msg);
    free(msg);
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
        strcpy(configs.ip, value);
    else if (0 == strcmp(varname, "server_port"))
        configs.port = atoi(value);
    else if (0 == strcmp(varname, "log_file"))
        strcpy(configs.logpath, value);
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
            log_error("configuration syntax error");
            exit(EXIT_FAILURE);
        }
    }

    /* printf("config: addr: %s, port: %d, logpath: %s\n", */
    /*         configs.ip, configs.port, configs.logpath); */
}

/* uid initialization */
void uid_init()
{
    FILE * fp = fopen("./users.db", "r");
    if (NULL == fp)
    {
        log_error("users database open failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    user_info_t user_info;

    while (fread(&user_info, sizeof(user_info_t), 1, fp)) /* stop after an EOF or a newline */
    {
        /* printf("saved user: %d %s %s\n", user_info.uid, user_info.passwd, user_info.name); */
        if (user_info.uid > uid)
            uid = user_info.uid;
    }

    uid = uid + 1;
    /* log_debug("init uid: %d", uid); */
}

void setup_server_listen(int * plistenfd)
{
    int status;
    struct sockaddr_in serv_addr;

    /* socket settings */
    *plistenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == *plistenfd)
    {
        log_error("socket creating failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(configs.port);
    if (0 == inet_pton(AF_INET, configs.ip, &serv_addr.sin_addr))
    {
        log_error("converting %s to IPv4 address failed!", configs.ip);
        close(*plistenfd);
        exit(EXIT_FAILURE);
    }

    /* ignore SIGPIPE signal */
    signal(SIGPIPE, SIG_IGN);

    /* bind */
    status = bind(*plistenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (-1 == status)
    {
        log_error("socket binding failed: %s", strerror(errno));
        close(*plistenfd);
        exit(EXIT_FAILURE);
    }

    /* listen */
    status = listen(*plistenfd, BACKLOG);
    if (-1 == status)
    {
        log_error("socket listening failed: %s", strerror(errno));
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
        log_error("client accepting failed: %s", strerror(errno));
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

    /* printf("[DEBUG]: add %p | %p\n", ol_uids, ol_uids->next); */
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
        /* printf("[DEBUG]: delete %p | %p\n", pprev, pprev->next); */
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

online_t * online_modify(int uid, int newuid, const char * newname)
{
    pthread_mutex_lock(&clients_mutex);
    online_t * online = ol_uids;
    online_t * found = NULL;

    while (online != NULL)
    {
        /* printf("[DEBUG] (%d) %p | %p\n", online->client.uid, online, online->next); */
        if (online->client.uid == uid)
        {
            if (found == NULL)
            {
                found = online;
            }
        }
        if (online->client.uid == newuid)
        {
            pthread_mutex_unlock(&clients_mutex);
            return NULL; /* already login */
        }
        online = online->next;
    }

    /* only found once */
    if (newuid != 0)
    {
        found->client.uid = newuid;
    }
    if (newname != NULL)
    {
        strcpy(found->client.name, newname);
    }
    log_info("<< uid %d with name %s modify successfuly",
            found->client.uid, found->client.name);
    pthread_mutex_unlock(&clients_mutex);
    return found;
}

/* print client info */

char * print_client_addr(struct sockaddr_in addr)
{
    char addr_str[INET_ADDRSTRLEN], addr_port[20];
    inet_ntop(AF_INET, (void *) &addr.sin_addr, addr_str, sizeof(addr_str));
    sprintf(addr_port, "%d", ntohs(addr.sin_port));

    char * cli_info = (char *) malloc(strlen(addr_str) + strlen(addr_port) + 10);
    strcat(cli_info, addr_str);
    strcat(cli_info, ":");
    strcat(cli_info, addr_port);
    cli_info[strlen(cli_info)] = '\0';
    return cli_info;
}

/* send message to sender */
void send_message_self(const char * msg, int connfd)
{
    msgprot_t * pmsgprot = message_pack(msg);
    if (write(connfd, pmsgprot, sizeof(msgprot_t) + pmsgprot->length) < 0)
    {
        log_error("writing to descriptor failed: %s", strerror(errno));
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
                log_error("fowarding message to client failed: %s", strerror(errno));
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
                log_error("writing to descriptor failed: %s", strerror(errno));
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
    msgprot_t * pmsgprot;
    char msg[BUFFER_SZ];

    while (pnode != NULL)
    {
        sprintf(msg, "%s (%d)\n", pnode->client.name, pnode->client.uid);
        pmsgprot = message_pack(msg);
        if (write(connfd, pmsgprot, sizeof(msgprot_t) + pmsgprot->length) < 0)
        {
            log_error("writing to descriptor failed: %s", strerror(errno));
            break;
        }
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
    char * cli_info;
    msgprot_t msgprot;
    int msglen;
    int is_login = 0; /* 登入标志 0 未登入；1 登入 */

    client_t * pcli = (client_t *) arg;

    cli_info = print_client_addr(pcli->addr);
    log_info("<< accept %s logged in with annoymous and referenced by %d", cli_info, pcli->uid);
    free(cli_info);

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
            /* printf("command: %s\n", command); */
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
                    send_message_self("<<already have other users logged in\n", pcli->connfd);
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
                    /* 需要修改在线列表中的 uid 和 name，列表 client_t 是复制了一份 */
                    if (NULL == online_modify(pcli->uid, uid, pcli->name))
                    {
                        sprintf(buffer_out, "<< %s (%d) has logged in\n", pcli->name, uid);
                        send_message_self(buffer_out, pcli->connfd);
                        continue;
                    }
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
                    log_error("cannot allocate memory for name: %s", strerror(errno));
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

    log_info("<< %s (%d) has left", pcli->name, pcli->uid);
    online_delete(pcli->uid);
    close(pcli->connfd);
    free(pcli);

    return (void *) EXIT_SUCCESS;
}

void load_arguments(int argc, char ** argv)
{
    struct option long_options[] =
    {
        {"help", no_argument, 0, 'h'},
        {"config", required_argument, 0, 'f'},
        {"bind_ip", required_argument, 0, 'i'},
        {"port", required_argument, 0, 'p'},
        {"logpath", required_argument, 0, 'l'},
        {"daemon", required_argument, 0, 'd'}
    };

    int c;
    int option_index = 0;

    while (1)
    {
        c = getopt_long(argc, argv, "hf:p:i:l:d", long_options, &option_index);

        if (-1 == c)
            break;

        switch (c)
        {
            case 'h':
                printf("Usage: %s [options]\n", argv[0]);
                printf("options:\n");
                printf("\t--help, -h\n\t\tshow help information\n");
                printf("\t--config <filename>, -f <filename>\n\t\tspecify configure file\n");
                printf("\t--bind_ip <ipaddress>, -i <ipaddress>\n");
                printf("\t--port <port>, -p <port>\n");
                printf("\t--logpath <path>\n");
                printf("\t--daemon, -d\n");
                exit(EXIT_FAILURE);
            case 'f':
                break;
            case 'p':
                if (optarg)
                    configs.port = atoi(optarg);
                break;
            case 'i':
                if (optarg)
                    strcpy(configs.ip, optarg);
                break;
            case 'l':
                if (optarg)
                    strcpy(configs.logpath, optarg);
                break;
            case 'd':
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char * argv[])
{
    int listenfd = 0, connfd = 0;
    pthread_t tid;

    /* read server config */
    read_server_config();

    /* load command arguments */
    load_arguments(argc, argv);

    /* open log file */
    log_set_fp(configs.logpath);

    log_info("load server config from file");
    log_info("load server config from command arguments");
    log_info("config: addr: %s, port: %d, logpath: %s",
            configs.ip, configs.port, configs.logpath);

    /* read user database file */
    uid_init();
    /* printf("init uid: %d\n", uid); */

    setup_server_listen(&listenfd);

    log_info("<[ SERVER STARTED ]>");

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
