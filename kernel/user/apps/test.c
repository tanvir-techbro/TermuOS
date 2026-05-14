#include "../lib/syscall.h"
#include "../../lib/printf.h"

void user_main(void)
{
    int pid = getpid();

    if (pid == 1)
    {
        // success
        kprintf("PID OK\n");
    }

    while (1);
}