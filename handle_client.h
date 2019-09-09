#ifndef HANDLE_CLIENT_H_
#define HANDLE_CLIENT_H_

#define BUFFER_SZ 2048

#include <netinet/in.h>

/* client structure */
typedef struct {
    struct sockaddr_in addr; /* Client remote address */
    int connfd;              /* Connection file descriptor */
    int uid;                 /* Client unique identifier */
    char name[51];           /* Client name */
    int alive;               /* 存活计数 */
                             /* 等于 0 时关闭 fd，并从在线列表中删除客户端 */
} client_t;

/* online users list */
typedef struct online {
    client_t * pclient;
    struct online * next;
} online_t;

typedef online_t * ol_uids_t;

extern ol_uids_t ol_uids; /* online list head */
void * handle_client(void * arg);
void * client_alive(void * arg);
void online_add(client_t * pcli);
void online_delete(int uid);
void send_message_self(const char * msg, int connfd);
int send_message_client(const char * msg, int uid);
void send_message(char * msg, int uid);
void send_active_clients(int connfd);
char * print_client_addr(struct sockaddr_in addr);
#endif
