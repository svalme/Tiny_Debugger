// debugger.c - Launch target, attach debugger, hand off to REPL
#include "types.h"
#include "breakpoint.h"
#include "commands.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: %s <program>\n", argv[0]);
        printf("Note: compile target with -no-pie flag\n");
        return 1;
    }

    uint64_t main_addr = get_main_address(argv[1]);
    if (main_addr == 0) {
        fprintf(stderr, "Error: could not find main symbol\n");
        fprintf(stderr, "Make sure program is compiled with -g and -no-pie\n");
        return 1;
    }

    printf("[found main at 0x%llx]\n", (unsigned long long)main_addr);

    pid_t child = fork();
    if (child < 0) die("fork");

    if (child == 0) {
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0)
            die("PTRACE_TRACEME");
        raise(SIGSTOP);
        execl(argv[1], argv[1], NULL);
        die("execl");
    }

    // Wait for child to stop on SIGSTOP
    int status;
    if (waitpid(child, &status, 0) < 0) die("waitpid");
    if (!WIFSTOPPED(status)) {
        fprintf(stderr, "Child did not stop\n");
        return 1;
    }

    printf("[debugger attached to pid %d]\n", child);

    // Let the program load into memory
    if (ptrace(PTRACE_CONT, child, NULL, NULL) < 0) die("PTRACE_CONT");
    if (waitpid(child, &status, 0) < 0) die("waitpid");

    // Set breakpoint at main and run to it
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

    debugger_loop(child);

    return 0;
}