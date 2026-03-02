// backtrace.h - Stack inspection
#ifndef BACKTRACE_H
#define BACKTRACE_H

#include "types.h"

void cmd_regs(pid_t pid);
void cmd_backtrace(pid_t pid);
void cmd_locals(pid_t pid);

#endif // BACKTRACE_H