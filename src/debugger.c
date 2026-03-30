// debugger.c - Launch target, attach debugger, hand off to REPL
#include "types.h"
#include "breakpoint.h"
#include "commands.h"
#include "dwarf.h"

/**
 * @brief Entry point. Forks the target process, attaches ptrace, runs it
 *        to main, then hands off to the interactive REPL.
 *
 * Sequence:
 *   1. Look up the address of "main" in the target binary via nm.
 *   2. Fork. The child calls PTRACE_TRACEME and raises SIGSTOP so the
 *      parent can attach before any code runs.
 *   3. Parent waits for the child's SIGSTOP, then issues PTRACE_CONT to
 *      let the dynamic linker finish loading the program into memory.
 *   4. Sets a software breakpoint (INT3) at "main".
 *   5. Issues PTRACE_CONT again; waits for the SIGTRAP that fires when
 *      the breakpoint is hit.
 *   6. Fixes up RIP (decrements by 1, since the CPU advanced past the
 *      INT3), removes the breakpoint, and enters the REPL.
 *
 * Requires the target to be compiled with -no-pie so that the symbol
 * address from nm matches the runtime virtual address.
 *
 * @param argc  Argument count. Must be >= 2.
 * @param argv  argv[1] is the path to the target executable.
 * @return      0 on normal exit, 1 on error.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: %s <program>\n", argv[0]);
        printf("Note: compile target with -no-pie\n");
        return 1;
    }
 
    uint64_t main_addr = get_main_address(argv[1]);
    if (main_addr == 0) {
        fprintf(stderr, "Error: could not find main symbol\n");
        fprintf(stderr, "Make sure program is compiled with -g and -no-pie\n");
        return 1;
    }
 
    printf("[found main at 0x%llx]\n", (unsigned long long)main_addr);
 
    // Open DWARF session before forking (reads the binary on disk, not the
    // tracee's memory, so the parent can hold it for the whole session).
    dwarf_session_t *ds = dwarf_open(argv[1]);
    if (!ds)
        printf("[warning: no DWARF info — backtraces will show raw addresses]\n");
 
    pid_t child = fork();
    if (child < 0) die("fork");
 
    if (child == 0) {
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0)
            die("PTRACE_TRACEME");
        raise(SIGSTOP);
        execl(argv[1], argv[1], NULL);
        die("execl");
    }
 
    int status;
    if (waitpid(child, &status, 0) < 0) die("waitpid");
    if (!WIFSTOPPED(status)) {
        fprintf(stderr, "Child did not stop\n");
        return 1;
    }
 
    printf("[debugger attached to pid %d]\n", child);
 
    if (ptrace(PTRACE_CONT, child, NULL, NULL) < 0) die("PTRACE_CONT");
    if (waitpid(child, &status, 0) < 0) die("waitpid");
 
    uint64_t original_data = set_breakpoint(child, main_addr);
    printf("[breakpoint set at main]\n");
 
    if (ptrace(PTRACE_CONT, child, NULL, NULL) < 0) die("PTRACE_CONT");
    if (waitpid(child, &status, 0) < 0) die("waitpid");
 
    if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
        printf("[hit breakpoint at main]\n");
        fix_rip_after_breakpoint(child);
        remove_breakpoint(child, main_addr, original_data);
        printf("[ready to debug]\n\n");
    } else {
        printf("[warning: did not hit expected breakpoint]\n");
    }
 
    debugger_loop(child, ds);
 
    dwarf_close(ds);
    return 0;
}