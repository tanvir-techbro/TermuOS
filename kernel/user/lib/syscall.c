#include "syscall.h"

#define SYS_GETPID 5

int getpid(void)
{
    uint64_t ret;

    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(SYS_GETPID)
        : "rcx", "r11", "memory"
    );

    return (int)ret;
}