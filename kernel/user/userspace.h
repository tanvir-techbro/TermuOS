#pragma once
#include <stdint.h>

void userspace_init(void);
void jump_to_userspace(uint64_t entry, uint64_t user_stack);