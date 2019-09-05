#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "log.h"

FILE * log_fp = NULL;
pthread_mutex_t logfile_mutex = PTHREAD_MUTEX_INITIALIZER;

int quiet = 0;

static const char * level_names[] = {
    "ERROR", "INFO", "DEBUG"
};

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

void log_set_quiet(int enable)
{
    quiet = enable;
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

    if (!quiet)
    {
        if (0 == level)
            fprintf(stderr, msg);
        else
            fprintf(stdout, msg);
    }

    if (log_fp)
    {
        pthread_mutex_lock(&logfile_mutex);
        fprintf(log_fp, "%s", msg);
        fflush(log_fp);
        pthread_mutex_unlock(&logfile_mutex);
    }

    free(va_msg);
    free(msg);
}

