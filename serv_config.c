#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>

#include "serv_config.h"
#include "log.h"

static int parse_line(char * buf);

/* init config paramters */
config_t configs = 
{
    "server.conf",
    "127.0.0.1",
    "",
    7000,
    0,
    0,
    'f'
};

/* read server config */
void read_server_config()
{
    FILE * fp = fopen(configs.config, "r");
    if (NULL == fp)
    {
        log_error("opening config failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    char fconfig[2048];

    while (fgets(fconfig, sizeof(fconfig), fp) != NULL)
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

    /* printf("config: addr: %s, port: %d, logpath: %s\n", */
    /*         configs.ip, configs.port, configs.logpath); */
}

/* load command arguments */
void load_arguments(int argc, char ** argv)
{
    struct option long_options[] =
    {
        {"help", no_argument, 0, 'h'},
        {"config", required_argument, 0, 'f'},
        {"bind_ip", required_argument, 0, 'i'},
        {"port", required_argument, 0, 'p'},
        {"storage", required_argument, 0, 's'},
        {"quiet", no_argument, 0, 'q'},
        {"logpath", required_argument, 0, 'l'},
        {"daemon", no_argument, 0, 'd'}
    };

    int c;
    int option_index = 0;

    while (1)
    {
        c = getopt_long(argc, argv, "hf:p:s:qi:l:d", long_options, &option_index);

        if (-1 == c)
            break;

        switch (c)
        {
            case 'h':
                printf("Usage: %s [options]\n", argv[0]);
                printf("options:\n");
                printf("\t--help, -h\n\t\tshow help information\n");
                printf("\t--config <filename>, -f <filename>\n\t\tspecify configure file\n");
                printf("\t--bind_ip <ipaddress>, -i <ipaddress>\n\t\tspecify server bind ip\n");
                printf("\t--port <port>, -p <port>\n\t\tspecify server bind port\n");
                printf("\t--storage <type>, -s <type>\n\t\tspecfiy storage type for storing register users\n");
                printf("\t--quiet, -q\n\t\trun as quiet mode\n");
                printf("\t--logpath <path>, -l <path>\n\t\tspecify log path\n");
                printf("\t--daemon, -d\n\t\trun as daemon\n");
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
            case 's':
                if (optarg)
                {
                    if (!strcmp(optarg, "file"))
                        configs.storage = 'f';
                    else if (!strcmp(optarg, "mysql"))
                        configs.storage = 'd';
                    else
                        exit(EXIT_FAILURE);
                }
                break;
            case 'q':
                configs.quiet = 1;
            case 'l':
                if (optarg)
                    strcpy(configs.logpath, optarg);
                break;
            case 'd':
                configs.daemon = 1;
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }
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
    else if (0 == strcmp(varname, "storage_type"))
    {
        if (!strcmp(value, "file"))
            configs.storage = 'f';
        else if (!strcmp(value, "mysql"))
            configs.storage = 'd';
        else
            return -1;
    }
    else if (0 == strcmp(varname, "daemon"))
    {
        if (!strcmp(value, "1"))
            configs.daemon = 1;
        else if (!strcmp(value, "0"))
        {
            configs.daemon = 0;
        }
        else
            return -1;
    }
    else
        return -1;

    return 0;
}
