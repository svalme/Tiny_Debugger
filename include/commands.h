// commands.h - Debugger REPL
#ifndef COMMANDS_H
#define COMMANDS_H

#include "types.h"
#include "dwarf.h"

void debugger_loop(pid_t child, dwarf_session_t *ds);

#endif // COMMANDS_H