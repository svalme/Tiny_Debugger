// breakpoint.c - Breakpoint implementation
#include "breakpoint.h"
#include "memory.h"

uint64_t get_main_address(const char *program) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "nm %s 2>/dev/null | grep ' T main$'", program);

    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;

    char line[256];
    uint64_t addr = 0;

    if (fgets(line, sizeof(line), fp))
        sscanf(line, "%" SCNx64, &addr);

    pclose(fp);
    return addr;
}

uint64_t set_breakpoint(pid_t pid, uint64_t addr) {
    uint64_t data = read_memory(pid, addr);
    uint64_t data_with_trap = (data & ~0xFF) | 0xCC;
    write_memory(pid, addr, data_with_trap);
    return data;
}

void remove_breakpoint(pid_t pid, uint64_t addr, uint64_t original_data) {
    write_memory(pid, addr, original_data);
}

void fix_rip_after_breakpoint(pid_t pid) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0)
        die("PTRACE_GETREGS");

    regs.rip -= 1;

    if (ptrace(PTRACE_SETREGS, pid, NULL, &regs) < 0)
        die("PTRACE_SETREGS");
}