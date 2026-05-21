#pragma once

#include <stdint.h>

typedef struct
{
    uint64_t pid;

    uint64_t rip;
    uint64_t rsp;

    void *pagemap;

    char name[64];
} process_t;
