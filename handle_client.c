#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "handle_client.h"
#include "serv_config.h"
#include "user_manage.h"
#include "msgprot.h"
#include "log.h"

static char * s_strdup(const char * s);

ol_uids_t ol_uids = NULL; /* online list head */
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t alive_mutex = PTHREAD_MUTEX_INITIALIZER;

void * handle_client(void * arg)
{
    char buffer_out[BUFFER_SZ];
    char * buffer_in = NULL; /* attention: 在用后 free() */
    char * cli_info;
    msgprot_t msgprot;
    int msglen;
    int is_login = 0; /* 登入标志 0 未登入；1 登入 */

    client_t * pcli = (client_t *) arg;
    /* printf("%s: %p\n", __FUNCTION__, pcli); */

    cli_info = print_client_addr(pcli->addr);
    log_info("<< accept %s client", cli_info);
    free(cli_info);

    /* pthread_mutex_lock(&clients_mutex); */
    /* sprintf(buffer_out, "<< accept [%s (%d)] login\n", pcli->name, pcli->uid); */
    /* write(pcli->connfd, buffer_out, sizeof(buffer_out)); */
    /* perror("write to client"); */
    /* pthread_mutex_unlock(&clients_mutex); */

    while (read(pcli->connfd, &msgprot, sizeof(msgprot_t)) > 0)
    {
        pthread_mutex_lock(&alive_mutex);
        pcli->alive = 9;
        pthread_mutex_unlock(&alive_mutex);

        msglen = message_unpack(pcli->connfd, &buffer_in, msgprot.length);
        if (msglen <= 0)
        {
            free(buffer_in);
            break;
        }
        /* 读取的数据长度不一致，舍弃 */
        if (msglen != msgprot.length)
        {
            free(buffer_in);
            continue;
        }

        if (0 == strcmp(buffer_in, "/alive"))
        {
            /* printf("[DEBUG] %d, alive\n", pcli->connfd); */
            free(buffer_in);
            continue;
        }

        if (buffer_in[0] == '/')
        {
            char * command, * param1, * param2;
            command = strtok(buffer_in, " ");
            /* printf("command: %s\n", command); */
            if (!strcmp(command, "/quit"))
            {
                free(buffer_in);
                break;
            }
            else if (!strcmp(command, "/register"))
            {
                param1 = strtok(NULL, " ");
                param2 = strtok(NULL, " ");
                if (param1 == NULL)
                {
                    send_message_self("<< password cannot be null\n", pcli->connfd);
                    free(buffer_in);
                    break;
                }

                if (configs.storage == 'd')
                {
                    mysql_insert_user(pcli->uid, param1, param2);
                }
                else
                {
                    save_uid_pwd(pcli->uid, param1, param2); /* TODO: 错误检查 */
                }
                sprintf(buffer_out, "<< resgister successfully with %d\n", pcli->uid);
                send_message_self(buffer_out, pcli->connfd);
                free(buffer_in);
            }
            else if (!strcmp(command, "/login"))
            {
                /* if (1 == is_login) */
                /* { */
                /*     send_message_self("<< already have other users logged in\n", pcli->connfd); */
                /*     free(buffer_in); */
                /*     continue; */
                /* } */
                param1 = strtok(NULL, " "); /* uid */
                param2 = strtok(NULL, " "); /* pwd */
                if (param1 == NULL || param2 == NULL)
                {
                    send_message_self("<< uid or password cannot be null\n", pcli->connfd);
                    free(buffer_in);
                    continue;
                }

                int uid = atoi(param1); /* TODO: 输入检查 */
                char login_name[STRMAX]; /* 临时存储登入用户的名字 */

                int status;
                if (configs.storage == 'd')
                {
                    /* printf("login mysql query\n"); */
                    status = mysql_verify_uid_pwd(uid, param2, login_name);
                    /* printf("stauts: %d\n", status); */
                }
                else
                {
                    status = verify_uid_pwd(uid, param2, login_name);
                }
                printf("login name: %s\n", login_name);

                if (0 == status)
                {
#if 0
                    /* 需要修改在线列表中的 uid 和 name，列表 client_t 是复制了一份 */
                    if (NULL == online_modify(pcli->uid, uid, login_name)) /* 这个 uid 已经登入了 */
                    {
                        sprintf(buffer_out, "<< %s (%d) has logged in\n", login_name, uid);
                        send_message_self(buffer_out, pcli->connfd);
                        free(buffer_in);
                        continue;
                    }
#endif
                    pcli->uid = uid; /* 修改完列表，再修改当前 client_t 信息 */
                    strcpy(pcli->name, login_name);
                    is_login = 1; /* 登入成功 */
                    sprintf(buffer_out, "<< login successfully with %s (%d)\n", pcli->name, pcli->uid);
                    send_message_self(buffer_out, pcli->connfd);
                    free(buffer_in);
                }
                else if (1 == status)
                {
                    send_message_self("<< uid and password do not match\n", pcli->connfd);
                    free(buffer_in);
                }
                else if (-1 == status)
                {
                    send_message_self("<< uid not found\n", pcli->connfd);
                    free(buffer_in);
                }
            }
            else if (!strcmp(command, "/msg"))
            {
                if (!is_login)
                {
                    send_message_self("<< login first or register\n", pcli->connfd);
                    free(buffer_in);
                    continue;
                }
                param1 = strtok(NULL, " ");
                param2 = strtok(NULL, " ");
                if (param1 == NULL || param2 == NULL)
                {
                    send_message_self("<< uid or message cannot be null\n", pcli->connfd);
                    free(buffer_in);
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
                free(buffer_in);
            }
            else if (!strcmp(command, "/list"))
            {
                if (!is_login)
                {
                    send_message_self("<< login first or register\n", pcli->connfd);
                    free(buffer_in);
                    continue;
                }
                send_active_clients(pcli->connfd);
                free(buffer_in);
            }
            else if (!strcmp(command, "/nick"))
            {
                if (!is_login)
                {
                    send_message_self("<< login first or register\n", pcli->connfd);
                    free(buffer_in);
                    continue;
                }
                param1 = strtok(NULL, " ");
                if (param1 == NULL)
                {
                    send_message_self("<< name cannot be null\n", pcli->connfd);
                    free(buffer_in);
                    break;
                }
                char * oldname = s_strdup(pcli->name);
                if (!oldname)
                {
                    log_error("cannot allocate memory for name: %s", strerror(errno));
                    free(buffer_in);
                    continue;
                }

                if (configs.storage == 'd')
                {
                    mysql_update_pwd_name(pcli->uid, NULL, param1);
                }
                else
                {
                    modify_pwd_name(pcli->uid, NULL, param1); /* TODO: 错误检查 */
                }
                strcpy(pcli->name, param1);
                sprintf(buffer_out, "<< %s is now konwn as %s\n", oldname, pcli->name);
                free(oldname);
                send_message_self(buffer_out, pcli->connfd);
                free(buffer_in);
            }
            else if (!strcmp(command, "/pwd"))
            {
                if (!is_login)
                {
                    send_message_self("<< login first or register\n", pcli->connfd);
                    free(buffer_in);
                    continue;
                }
                param1 = strtok(NULL, " ");
                if (param1 == NULL)
                {
                    send_message_self("<< password cannot be null\n", pcli->connfd);
                    free(buffer_in);
                    break;
                }

                if (configs.storage == 'd')
                {
                    mysql_update_pwd_name(pcli->uid, param1, NULL);
                }
                else
                {
                    modify_pwd_name(pcli->uid, param1, NULL); /* TODO: 错误检查 */
                }

                send_message_self("<< reset password successfully\n", pcli->connfd);
                free(buffer_in);
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
                free(buffer_in);
            }
            else
            {
                send_message_self("<< unkown command\n", pcli->connfd);
                free(buffer_in);
            }
        }
        else
        {
            if (!is_login)
            {
                send_message_self("<< login first or register\n", pcli->connfd);
                free(buffer_in);
                continue;
            }
            snprintf(buffer_out, sizeof(buffer_out), "[%d] (%s) %s\n", pcli->uid, pcli->name, buffer_in);
            send_message(buffer_out, pcli->uid);
            free(buffer_in);
        }
    }
    log_info("<< %s (%d) has left", pcli->name, pcli->uid);
    online_delete(pcli->uid); /* 这里只是释放了节点空间 */
    if (pcli->alive > 0)
    {
        close(pcli->connfd); /* 需要先关闭 fd, 再删除 client_t */
        pcli->connfd = -1;
    }
    printf("free pcli\n");

    return (void *) EXIT_SUCCESS;
}

/* add new client to  online uids */
void online_add(client_t * pcli)
{
    pthread_mutex_lock(&clients_mutex);
    online_t * ponline = (online_t *) malloc(sizeof(online_t));
    ponline->pclient = pcli;

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
    if (pnode->pclient->uid == uid)
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
        if (pnode->pclient->uid == uid)
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

void * client_alive(void * arg)
{
    client_t * pcli = (client_t *) arg;
    while (pcli->connfd != -1)
    {
        pthread_mutex_lock(&alive_mutex);
        --(pcli->alive);
        /* printf("alive: %d\n", pcli->alive); */
        pthread_mutex_unlock(&alive_mutex);

        if (0 == pcli->alive)
            break;
        sleep(1);
    }
    if (pcli->connfd != -1)
    {
        close(pcli->connfd);
    }

    return (void *) EXIT_SUCCESS;
}

#if 0
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
                found = online;
        }
        if (online->client.uid == newuid)
        {
            if (uid == newuid)
                break;
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
#endif

/* send message to sender */
void send_message_self(const char * msg, int connfd)
{
    msgprot_t * pmsgprot = message_pack(msg);
    if (write(connfd, pmsgprot, sizeof(msgprot_t) + pmsgprot->length) < 0)
    {
        log_error("writing to descriptor failed: %s", strerror(errno));
        close(connfd);

        free(pmsgprot);
        /* 如果当前进程中 mutex 变量被加锁了， 就在退出前解锁，防止 deadlock*/
        /* if (EBUSY == pthread_mutex_trylock(&clients_mutex))*/
        /*     pthread_mutex_unlock(&clients_mutex);*/
        exit(EXIT_FAILURE); 
    }
    free(pmsgprot);
}

/* send message to client */
int send_message_client(const char * msg, int uid)
{
    msgprot_t * pmsgprot = message_pack(msg);
    pthread_mutex_lock(&clients_mutex);
    online_t * pscan = ol_uids ;
    while (pscan != NULL)
    {
        if (pscan->pclient->uid == uid)
        {
            if (write(pscan->pclient->connfd, pmsgprot, sizeof(msgprot_t) + pmsgprot->length) < 0)
            {
                log_error("fowarding message to client failed: %s", strerror(errno));
                /* close(pscan->client.connfd); */
                free(pmsgprot);
                pthread_mutex_unlock(&clients_mutex);
                return -1; /* 消息发送不成功 */
            }
            free(pmsgprot);
            pthread_mutex_unlock(&clients_mutex);
            return 0; /* 消息发送成功 */
        }
        pscan = pscan->next;
    }
    free(pmsgprot);
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
        if (pnode->pclient->uid != uid)
        {
            if (write(pnode->pclient->connfd, pmsgprot, sizeof(msgprot_t) + pmsgprot->length) < 0)
            {
                log_error("send to uid %d failed: %s", pnode->pclient->uid, strerror(errno));
                continue;
            }
        }
        pnode = pnode->next;
    }
    free(pmsgprot);
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
        sprintf(msg, "%s (%d)\n", pnode->pclient->name, pnode->pclient->uid);
        pmsgprot = message_pack(msg);
        if (write(connfd, pmsgprot, sizeof(msgprot_t) + pmsgprot->length) < 0)
        {
            log_error("writing to descriptor failed: %s", strerror(errno));
            free(pmsgprot);
            break;
        }
        pnode = pnode->next;
    }
    free(pmsgprot);
    pthread_mutex_unlock(&clients_mutex);
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
