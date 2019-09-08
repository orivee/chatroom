#ifndef CONFIG_H_
#define CONFIG_H_

#define STRMAX 51

/* config structure */
typedef struct
{
    char config[STRMAX];    /* configuration file */
    char ip[STRMAX];        /* server socket bind ip address */
    char logpath[STRMAX];   /* log file pathname */
    int port;               /* server socket bind port */
    int quiet;              /* quiet mode */
    int daemon;             /* run as daemon*/
    char storage;           /* register user storage type f: file; d: mysql*/
} config_t;

extern config_t configs;
void read_server_config();
void load_arguments(int argc, char ** argv);

#endif
