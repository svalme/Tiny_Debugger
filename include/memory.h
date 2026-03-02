// memory.h - Memory reading/writing operations
#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

uint64_t read_memory(pid_t pid, uint64_t addr);
void write_memory(pid_t pid, uint64_t addr, uint64_t value);
void cmd_examine(pid_t pid, uint64_t addr, int count);

#endif // MEMORY_H