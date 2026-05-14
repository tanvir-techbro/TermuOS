#pragma once
#include <stdint.h>

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_OPEN    3
#define SYS_CLOSE   4
#define SYS_GETPID  5
#define SYS_SLEEP   6
#define SYS_UPTIME  7
#define SYS_YIELD   8

void syscall_init(void);
uint64_t syscall_dispatch(uint64_t num, uint64_t a, uint64_t b, uint64_t c);
