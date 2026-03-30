// breakpoint.c - Breakpoint implementation
#include "breakpoint.h"
#include "memory.h"

/**
 * @brief Looks up the address of the 'main' symbol in a compiled binary
 *        by shelling out to "nm".
 *
 * Parses the output of "nm <program> | grep ' T main$'" to find the
 * address of the text-section symbol named "main". Requires the binary
 * to have been compiled with symbols (i.e. not stripped).
 *
 * @param program  Path to the executable to inspect.
 * @return         The virtual address of "main", or 0 if not found or
 *                 if "nm" fails.
 */
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

/**
 * @brief Sets a software breakpoint at the given address by patching in
 *        an INT3 instruction (0xCC).
 *
 * Reads the 8-byte word at 'addr', replaces the lowest byte with 0xCC,
 * and writes it back. When the tracee executes the INT3, the kernel
 * stops it with SIGTRAP.
 *
 * The caller must save the return value and pass it to remove_breakpoint()
 * later to restore the original instruction.
 *
 * @param pid   The tracee process ID.
 * @param addr  The address at which to insert the breakpoint.
 * @return      The original 8-byte word at 'addr', needed to undo the patch.
 */
uint64_t set_breakpoint(pid_t pid, uint64_t addr) {
    uint64_t data = read_memory(pid, addr);
    uint64_t data_with_trap = (data & ~0xFF) | 0xCC;
    write_memory(pid, addr, data_with_trap);
    return data;
}

/**
 * @brief Removes a software breakpoint by restoring the original instruction.
 *
 * Writes back the original word that was saved when the breakpoint was set,
 * effectively erasing the 0xCC patch.
 *
 * @param pid            The tracee process ID.
 * @param addr           The address of the breakpoint to remove.
 * @param original_data  The original 8-byte word returned by set_breakpoint().
 */
void remove_breakpoint(pid_t pid, uint64_t addr, uint64_t original_data) {
    write_memory(pid, addr, original_data);
}

/**
 * @brief Rewinds RIP by one byte after the tracee has hit an INT3 breakpoint.
 *
 * When the CPU executes an INT3 (0xCC), RIP advances past it to the next
 * byte. Before resuming execution at the original instruction, RIP must be
 * decremented by 1 so the tracee re-executes from the correct address.
 *
 * Must be called after the tracee stops with SIGTRAP due to a breakpoint,
 * and before remove_breakpoint() + PTRACE_CONT.
 *
 * @param pid  The tracee process ID.
 */
void fix_rip_after_breakpoint(pid_t pid) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0)
        die("PTRACE_GETREGS");

    regs.rip -= 1;

    if (ptrace(PTRACE_SETREGS, pid, NULL, &regs) < 0)
        die("PTRACE_SETREGS");
}

uint64_t get_rip(pid_t pid) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0)
        die("PTRACE_GETREGS");
    return regs.rip;
}
 
// Returns 1 if still running, 0 if exited
int step_over(pid_t pid) {
    uint64_t rip = get_rip(pid);
 
    // Read the byte at RIP to check for call instructions
    uint64_t word = read_memory(pid, rip);
    uint8_t opcode  = (word >>  0) & 0xFF;
    uint8_t modrm   = (word >>  8) & 0xFF;
 
    uint64_t bp_addr = 0;
 
    if (opcode == 0xE8) {
        // Direct call: E8 <rel32>  — always 5 bytes
        bp_addr = rip + 5;
    } else if (opcode == 0xFF && (((modrm >> 3) & 0x7) == 2)) {
        // Indirect call: FF /2 — most common form is 2 bytes (FF D0, FF D3, etc.)
        // ModRM field /2 means reg field == 2 (CALL r/m64)
        // This covers FF D0 (call rax), FF D3 (call rbx), FF 15 (call [rip+rel32], 6 bytes)
        // For simplicity treat mod=3 (register) as 2 bytes, others as 6
        uint8_t mod = (modrm >> 6) & 0x3;
        if (mod == 3)
            bp_addr = rip + 2;       // call <reg>
        else
            bp_addr = rip + 6;       // call [mem] with 32-bit displacement (PLT form)
    }
 
    if (bp_addr != 0) {
        // Set temp breakpoint at the instruction after the call, cont, then clean up
        uint64_t orig = set_breakpoint(pid, bp_addr);
 
        if (ptrace(PTRACE_CONT, pid, NULL, NULL) < 0)
            die("PTRACE_CONT");
 
        int status;
        if (waitpid(pid, &status, 0) < 0)
            die("waitpid");
 
        remove_breakpoint(pid, bp_addr, orig);
 
        if (WIFEXITED(status)) {
            printf("[program exited with status %d]\n", WEXITSTATUS(status));
            return 0;
        } else if (WIFSIGNALED(status)) {
            printf("[program killed by signal %d]\n", WTERMSIG(status));
            return 0;
        } else if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
            fix_rip_after_breakpoint(pid);
        }
        return 1;
    }
 
    // Not a call — plain single-step
    if (ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL) < 0)
        die("PTRACE_SINGLESTEP");
 
    int status;
    if (waitpid(pid, &status, 0) < 0)
        die("waitpid");
 
    if (WIFEXITED(status)) {
        printf("[program exited with status %d]\n", WEXITSTATUS(status));
        return 0;
    } else if (WIFSIGNALED(status)) {
        printf("[program killed by signal %d]\n", WTERMSIG(status));
        return 0;
    }
    return 1;
}