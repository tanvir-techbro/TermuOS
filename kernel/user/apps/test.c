#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(void)
{
    printf("Hello from userspace!\n");
    printf("PID: %d\n", getpid());

    /* Test malloc */
    char *buf = malloc(64);
    if (buf)
    {
        strcpy(buf, "malloc works!\n");
        printf("%s", buf);
        free(buf);
    }
    else
    {
        printf("malloc failed\n");
    }

    return 0;
}