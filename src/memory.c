// memory.c - Memory reading/writing implementation
#include "memory.h"

/**
 * @brief Reads one 8-byte word from the tracee's address space.
 *
 * Uses PTRACE_PEEKDATA, which returns a single word at a time. Reads 
 * always return a full 8-byte word regardless of how many bytes you 
 * actually need.
 *
 * Calls die() on failure (e.g. invalid address, process not stopped).
 *
 * @param pid   The tracee process ID.
 * @param addr  The address to read from in the tracee's address space.
 * @return      The 8-byte word at 'addr'.
 */
uint64_t read_memory(pid_t pid, uint64_t addr) {
    errno = 0;
    uint64_t value = ptrace(PTRACE_PEEKDATA, pid, (void *)addr, NULL);
    if (errno != 0)
        die("PTRACE_PEEKDATA");
    return value;
}

/**
 * @brief Writes one 8-byte word into the tracee's address space.
 *
 * Uses PTRACE_POKEDATA to overwrite the word at 'addr' with 'value'.
 * Used to set and remove breakpoints by writing trap instructions (0xCC) 
 * and, subsequently, restoring original code.
 *
 * Calls die() on failure (e.g. writing to read-only memory, bad address).
 *
 * @param pid    The tracee process ID.
 * @param addr   The address to write to in the tracee's address space.
 * @param value  The 8-byte word to write.
 */
void write_memory(pid_t pid, uint64_t addr, uint64_t value) {
    if (ptrace(PTRACE_POKEDATA, pid, (void *)addr, (void *)value) < 0)
        die("PTRACE_POKEDATA");
}

/**
 * @brief Dumps 'count' consecutive 8-byte words from the tracee's memory,
 *        formatted as address/value pairs.
 *
 * Clamps 'count' to the range [1, 256] to prevent runaway output.
 * Stops early and prints a read error if a PTRACE_PEEKDATA call fails
 * (e.g. the range crosses an unmapped page).
 *
 * @param pid    The tracee process ID.
 * @param addr   The starting address to examine.
 * @param count  Number of 8-byte words to read. Clamped to [1, 256].
 */
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