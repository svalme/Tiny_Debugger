// memory.c - Memory reading/writing implementation
#include "memory.h"

uint64_t read_memory(pid_t pid, uint64_t addr) {
    errno = 0;
    uint64_t value = ptrace(PTRACE_PEEKDATA, pid, (void *)addr, NULL);
    if (errno != 0)
        die("PTRACE_PEEKDATA");
    return value;
}

void write_memory(pid_t pid, uint64_t addr, uint64_t value) {
    if (ptrace(PTRACE_POKEDATA, pid, (void *)addr, (void *)value) < 0)
        die("PTRACE_POKEDATA");
}

void cmd_examine(pid_t pid, uint64_t addr, int count) {
    if (count <= 0) count = 8;
    if (count > 256) count = 256;

    printf("Memory at 0x%llx (%d qwords):\n", (unsigned long long)addr, count);

    for (int i = 0; i < count; i++) {
        uint64_t current_addr = addr + (i * 8);
        errno = 0;
        uint64_t value = ptrace(PTRACE_PEEKDATA, pid, (void *)current_addr, NULL);

        if (errno != 0) {
            printf("  0x%016llx: <read error>\n", (unsigned long long)current_addr);
            break;
        }

        printf("  0x%016llx: 0x%016llx\n",
               (unsigned long long)current_addr,
               (unsigned long long)value);
    }
}