// backtrace.c - Stack inspection implementation
#include "backtrace.h"

void cmd_regs(pid_t pid) {
    struct user_regs_struct regs;
    struct iovec iov = { .iov_base = &regs, .iov_len = sizeof(regs) };

    if (ptrace(PTRACE_GETREGSET, pid, (void*)NT_PRSTATUS, &iov) < 0) {
        perror("PTRACE_GETREGSET");
        return;
    }

    printf("RIP: 0x%016llx  RSP: 0x%016llx  RBP: 0x%016llx\n",
           (unsigned long long)regs.rip,
           (unsigned long long)regs.rsp,
           (unsigned long long)regs.rbp);
}

void cmd_backtrace(pid_t pid) {
    struct user_regs_struct regs;
    struct iovec iov = { .iov_base = &regs, .iov_len = sizeof(regs) };

    if (ptrace(PTRACE_GETREGSET, pid, (void*)NT_PRSTATUS, &iov) < 0) {
        perror("PTRACE_GETREGSET");
        return;
    }

    printf("Backtrace:\n");
    printf("  #0  0x%016llx\n", (unsigned long long)regs.rip);

    uint64_t rbp = regs.rbp;
    int frame = 1;

    while (rbp != 0 && frame < 50) {
        errno = 0;
        uint64_t saved_rbp = ptrace(PTRACE_PEEKDATA, pid, (void *)rbp, NULL);
        if (errno != 0) break;

        uint64_t return_addr = ptrace(PTRACE_PEEKDATA, pid, (void *)(rbp + 8), NULL);
        if (errno != 0) break;

        if (return_addr == 0) break;

        printf("  #%d  0x%016llx\n", frame, (unsigned long long)return_addr);

        if (saved_rbp <= rbp) break;
        rbp = saved_rbp;
        frame++;
    }
}

void cmd_locals(pid_t pid) {
    struct user_regs_struct regs;
    struct iovec iov = { .iov_base = &regs, .iov_len = sizeof(regs) };

    if (ptrace(PTRACE_GETREGSET, pid, (void*)NT_PRSTATUS, &iov) < 0) {
        perror("PTRACE_GETREGSET");
        return;
    }

    uint64_t rsp = regs.rsp;
    uint64_t rbp = regs.rbp;

    printf("Stack frame [RSP -> RBP):\n");
    printf("RSP: 0x%016llx\n", (unsigned long long)rsp);
    printf("RBP: 0x%016llx\n", (unsigned long long)rbp);

    if (rbp == 0) { printf("  [RBP is 0 - no frame pointer]\n"); return; }
    if (rsp >= rbp) { printf("  [RSP >= RBP - invalid frame]\n"); return; }

    uint64_t frame_size = rbp - rsp;
    if (frame_size > 0x10000) {
        printf("  [Frame too large: 0x%llx bytes]\n", (unsigned long long)frame_size);
        return;
    }

    printf("\n");

    for (uint64_t addr = rsp; addr < rbp; addr += 8) {
        errno = 0;
        uint64_t value = ptrace(PTRACE_PEEKDATA, pid, (void *)addr, NULL);
        if (errno != 0) {
            printf("  0x%016llx: <read error>\n", (unsigned long long)addr);
            break;
        }
        printf("  0x%016llx: 0x%016llx\n",
               (unsigned long long)addr,
               (unsigned long long)value);
    }
}