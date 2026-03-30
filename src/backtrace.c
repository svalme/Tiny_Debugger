// backtrace.c - Stack inspection implementation
#include "backtrace.h"

/**
 * @brief Prints the current values of RIP, RSP, and RBP.
 *
 * Uses PTRACE_GETREGSET with NT_PRSTATUS to fill a user_regs_struct.
 * The registers instruction pointer (RIP), stack pointer (RSP), and 
 * base pointer (RBP) are printed.
 *
 * @param pid  The tracee process ID.
 */
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
 
// Format and print one frame line.
// With DWARF:  #N  funcname() at src.c:42  [0x...]
// Without:     #N  0x...
static void print_frame(int n, uint64_t addr, dwarf_session_t *ds) {
    if (ds) {
        dwarf_location_t loc = dwarf_lookup(ds, addr);
        if (loc.function || loc.srcfile) {
            const char *fn  = loc.function ? loc.function : "??";
            const char *src = loc.srcfile  ? loc.srcfile  : "??";
            // Basename only — the full path is noisy for a learning tool.
            const char *base = strrchr(src, '/');
            if (base) src = base + 1;
            if (loc.line > 0)
                printf("  #%d  %s() at %s:%d  [0x%016llx]\n",
                       n, fn, src, loc.line, (unsigned long long)addr);
            else
                printf("  #%d  %s() at %s  [0x%016llx]\n",
                       n, fn, src, (unsigned long long)addr);
            return;
        }
    }
    printf("  #%d  0x%016llx\n", n, (unsigned long long)addr);
}
 
/**
 * @brief Walks the frame pointer chain and prints a call stack.
 *
 * Starts at the current RIP (frame #0), then follows the saved-RBP linked
 * list: each stack frame stores the caller's RBP at [RBP+0] and the return
 * address at [RBP+8]. Walking these links produces the call chain.
 *
 * Stops when RBP is 0 (outermost frame), when a memory read fails (e.g.
 * unwinding past the bottom of the stack), when the return address is 0,
 * when the saved RBP doesn't advance (corrupt or missing frame pointers),
 * or after 50 frames as a safety cap.
 *
 * Note: requires the tracee to have been compiled with frame pointers
 * (-fno-omit-frame-pointer). 
 *
 * @param pid  The tracee process ID.
 * @param ds   An open DWARF session for the tracee's binary, or NULL to 
 *             skip DWARF lookups and just print raw addresses.
 */
void cmd_backtrace(pid_t pid, dwarf_session_t *ds) {
    struct user_regs_struct regs;
    struct iovec iov = { .iov_base = &regs, .iov_len = sizeof(regs) };
 
    if (ptrace(PTRACE_GETREGSET, pid, (void*)NT_PRSTATUS, &iov) < 0) {
        perror("PTRACE_GETREGSET");
        return;
    }
 
    printf("Backtrace:\n");
    print_frame(0, regs.rip, ds);
 
    uint64_t rbp = regs.rbp;
    int frame = 1;
 
    while (rbp != 0 && frame < 50) {
        errno = 0;
        uint64_t saved_rbp = ptrace(PTRACE_PEEKDATA, pid, (void *)rbp, NULL);
        if (errno != 0) break;
 
        uint64_t return_addr = ptrace(PTRACE_PEEKDATA, pid, (void *)(rbp + 8), NULL);
        if (errno != 0) break;
 
        if (return_addr == 0) break;
 
        print_frame(frame, return_addr, ds);
 
        if (saved_rbp <= rbp) break;
        rbp = saved_rbp;
        frame++;
    }
}

/**
 * @brief Dumps the raw contents of the current stack frame.
 *
 * Prints all 8-byte words in the range [RSP, RBP), i.e. the local variable
 * area of the current function. Values are shown as raw hex — there's no
 * DWARF parsing, so variable names and types are not available.
 *
 * Skips output and prints a diagnostic if RBP is 0 (no frame pointer),
 * RSP >= RBP (invalid frame), or the frame is larger than 64KB (likely
 * a corrupt or missing frame pointer).
 *
 * @param pid  The tracee process ID.
 */
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