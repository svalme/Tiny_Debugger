// breakpoint.h - Breakpoint management
#ifndef BREAKPOINT_H
#define BREAKPOINT_H

#include "types.h"

uint64_t get_main_address(const char *program);
uint64_t set_breakpoint(pid_t pid, uint64_t addr);
void remove_breakpoint(pid_t pid, uint64_t addr, uint64_t original_data);
void fix_rip_after_breakpoint(pid_t pid);
uint64_t get_rip(pid_t pid);
int step_over(pid_t pid);

#endif // BREAKPOINT_H