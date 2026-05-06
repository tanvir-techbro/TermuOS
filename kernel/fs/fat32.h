#pragma once
#include "vfs.h"
#include <stdint.h>

vfs_node_t *fat32_create_root();

void fat32_init(uint32_t lba_start);
void fat32_list_root();
void fat32_read_file(const char *name);