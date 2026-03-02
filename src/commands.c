// commands.c - Debugger REPL: command parsing and dispatch
#include "commands.h"
#include "memory.h"
#include "breakpoint.h"
#include "backtrace.h"

static void trim_newline(char *s) {
    if (s) s[strcspn(s, "\r\n")] = 0;
}

static void print_help(void) {
    printf("Commands:\n");
    printf("  regs         - show registers\n");
    printf("  bt           - show backtrace (call stack)\n");
    printf("  locals       - show current stack frame\n");
    printf("  x <addr> [n] - examine memory (n qwords, default 8)\n");
    printf("  step         - execute one instruction\n");
    printf("  cont         - continue execution\n");
    printf("  quit         - kill program and exit\n");
}

static int wait_for_child(pid_t child) {
    int status;
    if (waitpid(child, &status, 0) < 0)
        die("waitpid");

    if (WIFEXITED(status)) {
        printf("[program exited with status %d]\n", WEXITSTATUS(status));
        return 0;
    } else if (WIFSIGNALED(status)) {
        printf("[program killed by signal %d]\n", WTERMSIG(status));
        return 0;
    }
    return 1; // still running
}

void debugger_loop(pid_t child) {
    char line[256];
    print_help();

    while (1) {
        printf("dbg> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            break;

        trim_newline(line);

        if (strlen(line) == 0)
            continue;

        if (strcmp(line, "regs") == 0) {
            cmd_regs(child);

        } else if (strcmp(line, "bt") == 0) {
            cmd_backtrace(child);

        } else if (strcmp(line, "locals") == 0) {
            cmd_locals(child);

        } else if (strncmp(line, "x ", 2) == 0) {
            unsigned long long addr = 0;
            int count = 8;
            int parsed = sscanf(line, "x %llx %d", &addr, &count);
            if (parsed >= 1)
                cmd_examine(child, (uint64_t)addr, count);
            else
                printf("usage: x <hex_address> [count]\n");

        } else if (strcmp(line, "step") == 0) {
            if (ptrace(PTRACE_SINGLESTEP, child, NULL, NULL) < 0)
                die("PTRACE_SINGLESTEP");
            if (!wait_for_child(child)) break;

        } else if (strcmp(line, "cont") == 0) {
            if (ptrace(PTRACE_CONT, child, NULL, NULL) < 0)
                die("PTRACE_CONT");

            int status;
            if (waitpid(child, &status, 0) < 0) die("waitpid");

            if (WIFEXITED(status)) {
                printf("[program exited with status %d]\n", WEXITSTATUS(status));
                break;
            } else if (WIFSIGNALED(status)) {
                printf("[program killed by signal %d]\n", WTERMSIG(status));
                break;
            } else if (WIFSTOPPED(status)) {
                int sig = WSTOPSIG(status);
                if (sig == SIGTRAP)
                    printf("[hit breakpoint or trap]\n");
                else
                    printf("[stopped by signal %d]\n", sig);
            }

        } else if (strcmp(line, "quit") == 0) {
            ptrace(PTRACE_KILL, child, NULL, NULL);
            printf("[program killed]\n");
            break;

        } else {
            printf("Unknown command: %s\n", line);
            print_help();
        }
    }
}