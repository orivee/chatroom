#include "commons.h"
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <mysql.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include "serv_config.h"
#include "handle_client.h"
#include "user_manage.h"
#include "log.h"

#define BACKLOG 5
#define EVMAX 100

typedef struct {
    char hostname[51];
    char username[51];
    char password[51];
    char database[51];
    int port;
} mysql_config_t;

mysql_config_t mysql_config =
{
    "localhost",
    "root",
    "root",
    "",
    3306
};

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
    cli->alive = 9;
    strcpy(cli->name, "anonymous");

    /* log_debug("accept uid: %d", uid); */

    return cli;
}

int become_daemon()
{
    int fd;

    switch (fork())                 /* become backgroud process */
    {
        case -1: return -1;
        case 0: break;
        default: exit(EXIT_SUCCESS);
    }

    if (-1 == setsid())             /* become leader of new session */
        return -1;

    umask(0);                       /* clear file mode creation mask */
    chdir("/");                     /* change to root directory */

    close(STDIN_FILENO);            /* reopen standard fd's to /dev/null */

    fd = open("/dev/null", O_RDWR);

    if (fd != STDIN_FILENO)
        return -1;
    if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
        return -1;
    if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
        return -1;

    return 0;
}

int main(int argc, char * argv[])
{
    int listenfd = 0, connfd = 0;
    pthread_t client_tid, alive_tid;

    /* read server config */
    read_server_config();

    /* load command arguments */
    load_arguments(argc, argv);

    if (configs.daemon == 1)
        become_daemon();

    /* open log file */
    log_set_fp(configs.logpath);
    log_set_quiet(configs.quiet);

    log_info("load server config from file");
    log_info("load server config from command arguments");
    log_info("config: addr: %s, port: %d, logpath: %s",
            configs.ip, configs.port, configs.logpath);

    /* user storage type */
    if (configs.storage == 'd')
    {
        log_info("user storing uses by MySQL");
        /* initialize database */
        mysql_set_connect();
        mysql_create_db_table();
        mysql_uid_init();
    }
    else
    {
        log_info("user storing uses by file");
        /* read user database file */
        uid_init();
        /* printf("init uid: %d\n", uid); */
    }

    setup_server_listen(&listenfd);

    log_info("<[ SERVER STARTED ]>");

    while (1)
    {
        client_t * cli = accept_client(&listenfd, &connfd);
        printf("%s: %p\n", __FUNCTION__, cli);

        /* add client to online list and fork thread */
        online_add(cli);
        pthread_create(&alive_tid, NULL, client_alive, (void *) cli);
        pthread_create(&client_tid, NULL, handle_client, (void *) cli);

        sleep(1);
    }

    if (configs.storage == 'f')
    {
        mysql_close_connect();
    }
    else
    {
        log_close_fp(configs.logpath);
    }
    return 0;
}
