#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "utils.h"
#define PATHLEN 256

void getprogpath(char * path, int len)
{
    int length = readlink("/proc/self/exe", path, len);
    /* printf("%d\n", length); */

    if (length < 0)
    {
        perror("resolving symlink /proc/self/exe.");
        exit(-1);
    }
    if (length >= len) {
        fprintf(stderr, "Path too long.\n");
        exit(-1);
    }

    path[length] = '\0';
    char * p;
    if((p = strrchr(path, '/')))
        *(p) = '\0';
    if((p = strrchr(path, '/')))
        *(p+1) = '\0';

}
